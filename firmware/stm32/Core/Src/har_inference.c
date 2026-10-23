/**
 * @file    har_inference.c
 * @brief   HAR inference engine v13 — stai API, bridges sensor data to AI model + UI
 *
 * [2026-07-12 Update]: v6(legacy ai_platform.h)→v13(stai.h)。
 *   STEdgeAI v4.0.1 从 har_v13_seed42_int8.onnx 生成 har_network*.c/h。
 *   输入: uint8(1x6x512) QLinear(1.02747, 104), 输出: uint8(1x7) QLinear(0.09789, 113).
 *   预处理: ADC→物理单位→PAMAP2 z-score→QDQ 量化 (不变)。
 *   与 KWS 共用 stai 运行时 NetworkRuntime1201_CM33_GCC.a。
 */

#include "har_inference.h"
#include "har_model.h"
#include "har_embedding.h"      /* [2026-07-14]: 128-dim embedding → proto */
#include "proto_model.h"        /* [2026-07-14]: few-shot personalization */
#include "ble_uart.h"           /* [2026-07-14]: proto_session_feed */
#include "main.h"
#include "ui.h"
#include "safety_detect.h"
#include <string.h>
#include <math.h>

/* ============================================================
 * Configuration
 * ============================================================ */
#define WIN_SIZE         512
#define N_CHAN           6
#define SAMPLES_PER_CALL 25

/* ============================================================
 * Physical unit conversion (LSM6DSOX: ±8g, ±2000dps)
 * ============================================================ */
#ifndef ACC_SENS_PER_G
#define ACC_SENS_PER_G  4096.0f
#endif
#define GYR_SENS_DPS    0.070f

/* ============================================================
 * PAMAP2 global per-channel statistics — z-score normalization
 * Channel order: ax, ay, az, gx, gy, gz
 * Same dataset as v6, normalization unchanged.
 * ============================================================ */
static const float ch_mean[6] = {
    -4.843759f, 3.270267f, 3.801946f, -0.039940f, 0.042354f, 0.010867f
};
static const float ch_std[6] = {
    6.946883f, 6.932382f, 3.796365f, 1.211825f, 0.921595f, 1.633670f
};

/* ============================================================
 * Quantization: v13 QDQ (from STEdgeAI v4.0.1 analyze report)
 * Input  QLinear(1.027474523, 104, uint8)
 * Output QLinear(0.097888656, 113, uint8)
 * ============================================================ */
#define QINPUT_SCALE  1.027474523f
#define QINPUT_ZP     104
#define QOUTPUT_SCALE 0.097888656f
#define QOUTPUT_ZP    113

/* ============================================================
 * Buffers
 * ============================================================ */
static int16_t  ring_buf[WIN_SIZE * N_CHAN];
static uint16_t ring_head = 0;
static uint16_t ring_filled = 0;
static uint8_t  ai_input[WIN_SIZE * N_CHAN] __attribute__((aligned(16)));
static uint8_t  ai_output[HAR_NUM_CLASSES];
static volatile har_class_t last_best_class = HAR_STANDING;

/* [2026-07-14]: Proto 网络输出 */
static int   g_proto_class = -1;
static float g_proto_probs[7];
static int   g_proto_fresh = 0;  /* 本轮推理是否产出了新的 proto 结果 */

/* ============================================================
 * Preprocessing: ADC→physical→z-score→QDQ quantize
 * ============================================================ */
static void preprocess_window(const int16_t *raw, uint8_t *out)
{
    for (uint16_t i = 0; i < WIN_SIZE; i++) {
        for (uint8_t ch = 0; ch < N_CHAN; ch++) {
            int16_t r = raw[i * N_CHAN + ch];
            float phys;
            if (ch < 3) phys = (float)r / ACC_SENS_PER_G;
            else        phys = (float)r * GYR_SENS_DPS;
            float z = (phys - ch_mean[ch]) / ch_std[ch];
            float q = z / QINPUT_SCALE + QINPUT_ZP;
            if (q < 0.0f) q = 0.0f;
            if (q > 255.0f) q = 255.0f;
            out[i * N_CHAN + ch] = (uint8_t)(q + 0.5f);
        }
    }
}

/* ============================================================
 * Argmax on uint8 output (linear equivalent to float dequantized)
 * ============================================================ */
static har_class_t argmax7(const uint8_t *p)
{
    uint8_t best_v = 0; uint8_t best_i = 0;
    for (uint8_t i = 0; i < 7; i++) { if (p[i] > best_v) { best_v = p[i]; best_i = i; } }
    return (har_class_t)best_i;
}

/* ============================================================
 * Public API
 * ============================================================ */
void HAR_Init(void)
{
    if (har_model_init() != 0) {
        while (1); /* model load failed — fatal */
    }
    /* [2026-07-14]: 注册 embedding 回调 — node 164 START 捕获 float32[128] */
    har_embedding_init(har_model_get_network());

    /* [2026-07-14]: 从分类器权重推算通用原型 — static 移入 BSS, 避免栈上 3584B */
    {
        static float default_proto[PROTO_N_CLASSES][PROTO_DIM];
        har_model_extract_prototypes(default_proto);
        proto_model_set(default_proto, 1);  /* version=1: 权重推算的通用原型 */
    }

    memset(ring_buf, 0, sizeof(ring_buf));
    memset(ai_input, 128, sizeof(ai_input));
    ring_head = 0; ring_filled = 0;
    g_proto_class = -1;
    g_proto_fresh = 0;
    for (int i = 0; i < 7; i++) g_proto_probs[i] = -1.0f;
    safety_init();
}

int HAR_AddSamples(int16_t *acc_25, int16_t *gyr_25)
{
    if (!acc_25 || !gyr_25) return 0;
    for (uint16_t i = 0; i < SAMPLES_PER_CALL; i++) {
        ring_buf[ring_head * N_CHAN + 0] = acc_25[i * 3 + 0];
        ring_buf[ring_head * N_CHAN + 1] = acc_25[i * 3 + 1];
        ring_buf[ring_head * N_CHAN + 2] = acc_25[i * 3 + 2];
        ring_buf[ring_head * N_CHAN + 3] = gyr_25[i * 3 + 0];
        ring_buf[ring_head * N_CHAN + 4] = gyr_25[i * 3 + 1];
        ring_buf[ring_head * N_CHAN + 5] = gyr_25[i * 3 + 2];
        ring_head = (ring_head + 1) % WIN_SIZE;
        if (ring_filled < WIN_SIZE) ring_filled++;
    }
    if (ring_filled < WIN_SIZE) return 0;

    static int16_t linear[WIN_SIZE * N_CHAN];
    for (uint16_t i = 0; i < WIN_SIZE; i++) {
        uint16_t src = (ring_head + i) % WIN_SIZE;
        memcpy(&linear[i * N_CHAN], &ring_buf[src * N_CHAN], N_CHAN * sizeof(int16_t));
    }
    preprocess_window(linear, ai_input);
    har_embedding_reset();  /* [2026-07-14]: 清零 ready, 防陈旧 embedding */
    if (har_model_run(ai_input, ai_output) != 0) return 0;

    last_best_class = argmax7(ai_output);
    ui_har_update(last_best_class, ai_output);

    /* [2026-07-14]: Proto 推理 + Session feed */
    {
        float emb[HAR_EMBEDDING_DIM];
        g_proto_fresh = 0;
        if (har_embedding_get(emb) == 0) {
            if (proto_model_is_loaded()) {
                proto_model_predict_probs(emb, g_proto_probs);
                g_proto_class = proto_model_predict(emb);
                g_proto_fresh = 1;
            }
            proto_session_feed_if(emb, (int)last_best_class, HAR_GetLastConfidence());
        }
    }

    /* Bonus: feed safety with actual HAR class (bullying uses it) */
    {
        int16_t last_ax = acc_25[(SAMPLES_PER_CALL-1)*3+0], last_ay = acc_25[(SAMPLES_PER_CALL-1)*3+1], last_az = acc_25[(SAMPLES_PER_CALL-1)*3+2];
        int16_t last_gx = gyr_25[(SAMPLES_PER_CALL-1)*3+0], last_gy = gyr_25[(SAMPLES_PER_CALL-1)*3+1], last_gz = gyr_25[(SAMPLES_PER_CALL-1)*3+2];
        safety_feed_imu(HAL_GetTick(),
            (float)last_ax/ACC_SENS_PER_G, (float)last_ay/ACC_SENS_PER_G, (float)last_az/ACC_SENS_PER_G,
            (float)last_gx*0.070f, (float)last_gy*0.070f, (float)last_gz*0.070f,
            (int)last_best_class);
    }
    return 1;
}

har_class_t HAR_GetLastClass(void) { return last_best_class; }

float HAR_GetLastConfidence(void)
{
    float logits[7]; float max_l = -1e30f;
    for (int i = 0; i < 7; i++) { logits[i] = ((float)ai_output[i] - QOUTPUT_ZP) * QOUTPUT_SCALE; if (logits[i] > max_l) max_l = logits[i]; }
    float sum = 0.0f; for (int i = 0; i < 7; i++) sum += expf(logits[i] - max_l);
    return expf(logits[last_best_class] - max_l) / sum;
}

const char *HAR_GetClassName(har_class_t c) {
    static const char *names[7] = {"Lying","Sitting","Standing","Walking","Jogging","Downstairs","Cycling"};
    return ((int)c<0||c>=7) ? "Unknown" : names[c];
}

/* [2026-07-14]: Proto 网络输出 — 原型未加载或本轮未产出新结果时返回 -1 / -1.0f */
int HAR_GetProtoClass(void)
{
    return (proto_model_is_loaded() && g_proto_fresh) ? g_proto_class : -1;
}

void HAR_GetProtoProbs(float probs[7])
{
    if (proto_model_is_loaded() && g_proto_fresh)
        memcpy(probs, g_proto_probs, 7 * sizeof(float));
    else
        for (int i = 0; i < 7; i++) probs[i] = -1.0f;
}
