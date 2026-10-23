/**
 * @file    task_kws.c
 * @brief   KWS 关键词唤醒任务 — 累积音频 → MFCC → 推理 → 防抖 → 触发安全告警
 *
 * [2026-07-04]: 集成。
 *   Mic_Task (EventGroup) → KWS_Task (累积 → MFCC → 推理) → safety_feed_shout
 *   推理间隔: ~200ms 滑动窗口
 *   防误触: 连续 2 帧同关键词才触发
 */

#include "task_kws.h"
#include "kws_mfcc.h"
#include "kws_model.h"
#include "spu0410.h"
#include "safety_detect.h"
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include <string.h>

/* 外部 Mic_Task 事件组 (main.c 定义) */
extern EventGroupHandle_t xMicEvents;
#define EVT_MIC_FRAME_READY  (1 << 0)

/* 音频环形缓冲: 2×16384 样本 (双倍于 KWS 窗口, 支持 50% 滑动) */
#define KWS_BUF_SAMPLES  32768
static int16_t kws_audio_buf[KWS_BUF_SAMPLES];
static uint32_t kws_buf_write = 0;       /* 写入位置 (样本数) */
static uint32_t kws_buf_total  = 0;      /* 已累积样本数 (不取模) */

/* 推理输出 */
#define KWS_DEBOUNCE_FRAMES  2    /* 连续命中帧数 */
static int kws_last_label = 3;     /* 上一帧标签, 初始 = unknown */
static int kws_hit_count  = 0;     /* 连续命中计数 */

/* MFCC 输出缓冲区 */
static float    mfcc_buf[KWS_NUM_MFCC][KWS_NUM_FRAMES];
static int8_t   kws_input[KWS_MODEL_IN_SIZE];   /* 量化输入 */
static float    kws_output[KWS_MODEL_OUT_SIZE]; /* softmax 输出 */

/* [2026-07-04 Fix]: 添加 __attribute__((unused)) 消除编译警告,
 *   kws_labels 保留用于调试时打印标签名。 */
static const char *kws_labels[4] __attribute__((unused)) = {"救命", "帮帮我", "silence", "unknown"};

void TaskKWS(void *argument)
{
    (void)argument;

    /* [2026-07-04]: 初始化 KWS 模型 (STEdgeAI 生成, ~22KB 激活缓冲) */
    if (kws_model_init() != 0) {
        /* 模型加载失败 — 静默退出, 不影响其他子系统 */
        vTaskDelete(NULL);
    }

    /* 等 Mic_Task 就绪 */
    vTaskDelay(pdMS_TO_TICKS(500));

    /* [2026-07-11]: 激活 KWS 消费通道(方案C双缓冲) — 此后 ISR 才填 ch_kws 并唤醒本任务。
     *   放在此处(而非 TaskKWS 入口)确保 Mic 侧 mic_init 已完成标定。 */
    mic_set_kws_active(1);

    for (;;) {
        /* 等待一帧 MIC 数据 (256 样本, ~19ms @ 13.5kHz) */
        EventBits_t bits = xEventGroupWaitBits(xMicEvents, EVT_MIC_FRAME_READY,
                                               pdTRUE, pdFALSE, pdMS_TO_TICKS(500));
        if (!(bits & EVT_MIC_FRAME_READY)) continue;

        /* 拷贝 Mic_Task 的帧到 KWS 环形缓冲 */
        int16_t mic_frame[MIC_BUFFER_SIZE];
        if (mic_get_frame(mic_frame) != 0) continue;  /* 帧数据获取失败 */

        for (int i = 0; i < MIC_BUFFER_SIZE; i++) {
            kws_audio_buf[kws_buf_write] = mic_frame[i];
            kws_buf_write = (kws_buf_write + 1) % KWS_BUF_SAMPLES;
        }
        kws_buf_total += MIC_BUFFER_SIZE;

        /* 每累积 ~200ms 音频运行一次推理 (200ms / (256/13500) ≈ 10.5 帧) */
        static int frame_count = 0;
        frame_count++;
        if (frame_count < 10) continue;  /* 约 190ms */
        frame_count = 0;

        /* 检查是否有足够样本 */
        if (kws_buf_total < KWS_AUDIO_SAMPLES) continue;

        /* [2026-07-04 Fix]: window_samples 改为 static — 原为 32KB 局部数组,
         *   但 KWSTask 栈仅 1024字=4KB (main.c:316)。32KB > 4KB → 8× 栈溢出。
         *   所有其他大缓冲区 (kws_audio_buf, mfcc_buf) 已为 static,
         *   此数组也改为 static 放入 BSS 段。
         *   交叉验证: FreeRTOS portmacro (stack=words×4) + GCC AAPCS + 直接计算 */
        static int16_t window_samples[KWS_AUDIO_SAMPLES];
        uint32_t start = (kws_buf_write + KWS_BUF_SAMPLES - KWS_AUDIO_SAMPLES) % KWS_BUF_SAMPLES;
        for (int i = 0; i < KWS_AUDIO_SAMPLES; i++)
            window_samples[i] = kws_audio_buf[(start + i) % KWS_BUF_SAMPLES];

        /* MFCC 特征提取 */
        kws_extract_mfcc(window_samples, mfcc_buf);

        /* [2026-07-04]: QLinear 量化, 参数来自 STEdgeAI 模型校准
         *   scale=0.039314173, zp=128 → q = clamp(round(z/scale + zp), 0, 255) */
        #define KWS_Q_SCALE  0.039314173f
        #define KWS_Q_ZP     128
        for (int f = 0; f < KWS_NUM_FRAMES; f++)
            for (int c = 0; c < KWS_NUM_MFCC; c++) {
                float v = mfcc_buf[c][f];
                int q = (int)(v / KWS_Q_SCALE + KWS_Q_ZP + 0.5f);
                /* [2026-07-04 Fix]: 拆分单行 if-if 为两行,
                 *   原写法 "if (q < 0) q = 0; if (q > 255) q = 255;"
                 *   触发 -Wmisleading-indentation 警告。 */
                if (q < 0) q = 0;
                if (q > 255) q = 255;
                /* [2026-07-14 NOTE]: kws_input 声明为 int8_t 但模型期望 uint8。
                 *   (int8_t)q 当 q∈[128,255] 时产生负值, 但 memcpy 逐字节拷贝
                 *   保留了 uint8 位模式, 模型收到正确字节。功能正确, 不改类型。 */
                /* [2026-08-27 Fix]: 输入布局转置(CRITICAL)。模型输入 shape={1,40,32,1}
                 *   + CHANNEL_LAST 期望 coeff-major(索引=coeff*32+frame); 原 `f*40+c`
                 *   是 frame-major, 与模型张量互为转置 → DS-CNN 首层卷积激活错乱,
                 *   关键词识别失效。交叉验证: kws_network_details.h:25(shape+CHANNEL_LAST)
                 *   + kws_network.c:523(AI_STRIDE 宽度维 stride=32) + librosa mfcc 输出 [40,32]。 */
                kws_input[c * KWS_NUM_FRAMES + f] = (int8_t)q;
            }

        /* 运行推理 */
        if (kws_model_run(kws_input, kws_output) != 0) continue;

        /* 找最大概率类别 */
        int best = 0;
        float best_p = kws_output[0];
        for (int i = 1; i < 4; i++)
            if (kws_output[i] > best_p) { best_p = kws_output[i]; best = i; }

        /* 防抖: 连续 N 帧同一非静音关键词才触发 */
        if (best < 2 && best_p > 0.7f) {  /* 救命(0) 或 帮帮我(1), 置信度 >70% */
            if (best == kws_last_label)
                kws_hit_count++;
            else { kws_hit_count = 1; kws_last_label = best; }

            if (kws_hit_count >= KWS_DEBOUNCE_FRAMES) {
                /* 触发安全告警 — 通过延迟标志 (Mic_Task 上下文安全) */
                safety_feed_shout(xTaskGetTickCount() * portTICK_PERIOD_MS, 1);
                kws_hit_count = 0;  /* 触发后清零, 等下一轮 */
            }
        } else {
            kws_hit_count = 0;
            if (best >= 2) kws_last_label = best;  /* silence/unknown 不累积 */
        }

        /* [2026-07-04 Fix]: 滑动窗口 — 原逻辑每轮推理 +2560 样本后 -8192 = 净 -5632,
         *   导致 3-4 轮积累才能再次触发推理 (预期每轮触发, 50% 重叠)。
         *   修复: 推理后保留半窗口样本 (KWS_AUDIO_SAMPLES/2), 下一次满窗时自然实现 50% 重叠。
         *   交叉验证: 源码算术跟踪 + 稳态收敛计算 (初始分析误判为永久停滞, 实际为 3-4× 降速) */
        if (kws_buf_total > KWS_AUDIO_SAMPLES)
            kws_buf_total = KWS_AUDIO_SAMPLES / 2;
    }
}
