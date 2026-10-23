/**
 * @file proto_model.c
 * @brief Prototype storage + cosine distance inference for few-shot HAR
 *
 * [2026-07-12]: 实现。7-class × 128-dim, L2-normalized embedding → cosine similarity → argmax.
 *   原型通过 BLE proto restore (0x0F) 从手机下发, 或出厂预置通用原型。
 * [2026-07-14]: 防御性 L2 归一化 — proto_model_set() 对输入原型逐类归一化,
 *   predict/predict_probs() 对输入 embedding 归一化 (不依赖调用方遵守契约)。
 */
#include "proto_model.h"
#include <string.h>
#include <math.h>

static float g_prototypes[PROTO_N_CLASSES][PROTO_DIM];
static uint8_t g_proto_version = 0;
static uint8_t g_proto_loaded = 0;

void proto_model_init(void)
{
    memset(g_prototypes, 0, sizeof(g_prototypes));
    g_proto_version = 0;
    g_proto_loaded = 0;
}

int proto_model_is_loaded(void) { return g_proto_loaded; }

uint8_t proto_model_get_version(void) { return g_proto_version; }  /* [2026-07-14] */

/* ---- L2 归一化 (in-place) ---- */
static void l2_normalize(float *v)
{
    float norm = 0.0f;
    for (int i = 0; i < PROTO_DIM; i++) norm += v[i] * v[i];
    if (norm > 1e-8f) {
        norm = 1.0f / sqrtf(norm);
        for (int i = 0; i < PROTO_DIM; i++) v[i] *= norm;
    }
}

void proto_model_set(const float prototypes[PROTO_N_CLASSES][PROTO_DIM], uint8_t version)
{
    memcpy(g_prototypes, prototypes, sizeof(g_prototypes));
    for (int c = 0; c < PROTO_N_CLASSES; c++)
        l2_normalize(g_prototypes[c]);
    g_proto_version = version;
    g_proto_loaded = 1;
}

/* [2026-07-14]: 零拷贝版 — 直接从 BLE payload 字节 buffer 写入, 避免栈上 3584B 拷贝.
 *   写前验证数据: 如有 NaN/Inf → 拒绝, 保留旧原型不变 */
int proto_model_set_raw(const uint8_t data[PROTO_N_CLASSES * PROTO_DIM * 4], uint8_t version)
{
    /* NaN/Inf 验证 — `!(v > -1e10 && v < 1e10)` 同时捕获 NaN/Inf/极端值 */
    {
        int bad = 0;
        for (int i = 0; i < PROTO_N_CLASSES * PROTO_DIM; i++) {
            float v;
            /* Wire data starts after two bytes and need not be aligned. */
            memcpy(&v, &data[i * sizeof(float)], sizeof(v));
            if (!(v > -1e10f && v < 1e10f)) { bad = 1; break; }
        }
        if (bad) return 0;  /* 数据异常, 拒绝, 不更新原型 */
    }

    memcpy(g_prototypes, data, sizeof(g_prototypes));
    for (int c = 0; c < PROTO_N_CLASSES; c++)
        l2_normalize(g_prototypes[c]);
    g_proto_version = version;
    g_proto_loaded = 1;
    return 1;
}

void proto_model_get(float prototypes[PROTO_N_CLASSES][PROTO_DIM], uint8_t *version)
{
    memcpy(prototypes, g_prototypes, sizeof(g_prototypes));
    if (version) *version = g_proto_version;
}

/* [2026-07-14]: 零拷贝版 — 返回内部存储指针, 避免栈上 3584B 拷贝 */
const float (*proto_model_get_ptr(void))[PROTO_DIM]
{
    return (const float (*)[PROTO_DIM])g_prototypes;
}

/** cosine similarity between two L2-normalized vectors = dot product */
static float cosine_sim(const float *a, const float *b)
{
    float dot = 0.0f;
    for (int i = 0; i < PROTO_DIM; i++) dot += a[i] * b[i];
    return dot;
}

int proto_model_predict(const float *embedding)
{
    if (!g_proto_loaded) return 0;

    /* [2026-07-14]: 防御性归一化 — 拷贝后归一化, 不修改调用方数据 */
    float v[PROTO_DIM];
    memcpy(v, embedding, sizeof(v));
    l2_normalize(v);

    float best_sim = -2.0f; int best = 0;
    for (int c = 0; c < PROTO_N_CLASSES; c++) {
        float sim = cosine_sim(g_prototypes[c], v);
        if (sim > best_sim) { best_sim = sim; best = c; }
    }
    return best;
}

void proto_model_predict_probs(const float *embedding, float probs[PROTO_N_CLASSES])
{
    if (!g_proto_loaded) { for (int i=0; i<PROTO_N_CLASSES; i++) probs[i]=(i==0)?1.0f:0.0f; return; }

    /* [2026-07-14]: 防御性归一化 */
    float v[PROTO_DIM];
    memcpy(v, embedding, sizeof(v));
    l2_normalize(v);

    float sims[PROTO_N_CLASSES], sum = 0.0f;
    for (int c = 0; c < PROTO_N_CLASSES; c++) {
        sims[c] = expf(cosine_sim(g_prototypes[c], v) * 10.0f);  /* softmax temperature */
        sum += sims[c];
    }
    for (int c = 0; c < PROTO_N_CLASSES; c++) probs[c] = sims[c] / sum;
}
