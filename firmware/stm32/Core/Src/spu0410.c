#include "spu0410.h"
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include <math.h>
#include <string.h>

extern ADC_HandleTypeDef hadc1;
extern EventGroupHandle_t xMicEvents;
#define EVT_MIC_FRAME_READY (1 << 0)

/* DMA 循环缓冲区 */
static volatile uint16_t __attribute__((aligned(32))) mic_buf[MIC_BUFFER_SIZE];

/* [2026-07-11 重构]: Mic/KWS 争帧修复 — 单缓冲改「双缓冲·两独立 SPSC」(方案C)。
 *   每个消费者一套私有帧 + 独立 half/pending/ready, 完全解耦:
 *   - 无撕裂: 消费者读私有 frame; frame_ready=1 期间 ISR 只标 pending、不改该通道 frame;
 *   - 无丢帧: 各通道独立 ready, 一方消费不影响另一方;
 *   - 无耦合: 一个消费者卡死绝不 hold 另一个(保留旧代码在单点崩溃下的健壮性)。
 *   源码可证: FreeRTOS 临界区(BASEPRI≥configMAX_SYSCALL_INTERRUPT_PRIORITY=5, 屏蔽 GPDMA1_Ch0
 *   ISR 优先级6 + 禁任务切换, 见 port.c:1030/1790)保证"清 ready + 处理 pending"原子;
 *   frame_ready=1 作为"ISR 勿写此通道 frame"的门闩 → 读期间无写入。手改勿重生成。 */
typedef struct {
    volatile uint16_t frame[MIC_BUFFER_SIZE];       /* 私有完整帧(消费者读) */
    volatile uint8_t  half0_done, half1_done;       /* 前/后半已拷入 frame */
    volatile uint8_t  pending_half0, pending_half1;  /* frame_ready 期间新半区积压 */
    volatile uint8_t  frame_ready;                   /* 该通道帧就绪 */
} mic_ch_t;

static mic_ch_t ch_mic;                 /* Mic_Task(shout 检测)通道 */
static mic_ch_t ch_kws;                 /* KWS_Task(关键词)通道 */
static volatile uint8_t kws_active = 0; /* KWS 已启动并参与消费; 否则 ISR 不填 ch_kws */
static volatile uint8_t active = 0;     /* ADC DMA 运行中 */

static float dc_off = MIC_ADC_RESOLUTION / 2.0f;
/* NOTE: dc_off 由 Mic_Task 写(mic_get_energy/mic_is_shouting, 临界区), KWS_Task 只读(mic_get_frame)。
 *   4 字节对齐读原子, 读到略旧值对去 DC 偏移无实质影响。 */
static uint8_t shout_cnt = 0;

/* ---------------- 内部辅助 ---------------- */

static void ch_reset(mic_ch_t *ch) {
    ch->half0_done = 0; ch->half1_done = 0;
    ch->pending_half0 = 0; ch->pending_half1 = 0;
    ch->frame_ready = 0;
}

/* 从 mic_buf 拷半区到某通道的 frame */
static inline void copy_half_to(mic_ch_t *ch, uint8_t idx) {
    if (idx == 0)
        memcpy((void*)&ch->frame[0], (void*)&mic_buf[0],
               MIC_BUFFER_SIZE/2 * sizeof(uint16_t));
    else
        memcpy((void*)&ch->frame[MIC_BUFFER_SIZE/2], (void*)&mic_buf[MIC_BUFFER_SIZE/2],
               MIC_BUFFER_SIZE/2 * sizeof(uint16_t));
}

/* ISR/临界区内: 一个通道收到半区完成。返回 1 = 该通道刚拼出完整帧(需 set EventGroup) */
static uint8_t ch_on_half_locked(mic_ch_t *ch, uint8_t idx) {
    if (ch->frame_ready) {              /* 上一帧未消费 → 积压, 不覆盖 frame */
        if (idx == 0) ch->pending_half0 = 1; else ch->pending_half1 = 1;
        return 0;
    }
    copy_half_to(ch, idx);
    if (idx == 0) ch->half0_done = 1; else ch->half1_done = 1;
    if (ch->half0_done && ch->half1_done) {
        ch->frame_ready = 1;
        ch->half0_done = 0; ch->half1_done = 0;
        return 1;
    }
    return 0;
}

/* 消费者读完某通道后(临界区内): 清 ready → 处理 pending → 可能重新 ready。返回 1 = 重新 ready */
static uint8_t ch_release_locked(mic_ch_t *ch) {
    ch->frame_ready = 0;
    if (ch->pending_half0) { copy_half_to(ch, 0); ch->half0_done = 1; ch->pending_half0 = 0; }
    if (ch->pending_half1) { copy_half_to(ch, 1); ch->half1_done = 1; ch->pending_half1 = 0; }
    if (ch->half0_done && ch->half1_done) {
        ch->frame_ready = 1;
        ch->half0_done = 0; ch->half1_done = 0;
        return 1;
    }
    return 0;
}

/* ---------------- 中断回调 ---------------- */

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance != ADC1) return;
    UBaseType_t s = taskENTER_CRITICAL_FROM_ISR();
    uint8_t re = ch_on_half_locked(&ch_mic, 0);
    if (kws_active) re |= ch_on_half_locked(&ch_kws, 0);
    taskEXIT_CRITICAL_FROM_ISR(s);
    if (re) {
        BaseType_t woken = pdFALSE;
        xEventGroupSetBitsFromISR(xMicEvents, EVT_MIC_FRAME_READY, &woken);
        portYIELD_FROM_ISR(woken);
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance != ADC1) return;
    UBaseType_t s = taskENTER_CRITICAL_FROM_ISR();
    uint8_t re = ch_on_half_locked(&ch_mic, 1);
    if (kws_active) re |= ch_on_half_locked(&ch_kws, 1);
    taskEXIT_CRITICAL_FROM_ISR(s);
    if (re) {
        BaseType_t woken = pdFALSE;
        xEventGroupSetBitsFromISR(xMicEvents, EVT_MIC_FRAME_READY, &woken);
        portYIELD_FROM_ISR(woken);
    }
}

/* ---------------- 公开接口 ---------------- */

int mic_init(void)
{
    mic_start();

    /* 启动阶段 KWS 未激活(kws_active=0), 只有 ch_mic 被填充, 用它标定 dc_off */
    uint32_t tickstart = HAL_GetTick();
    while (!ch_mic.frame_ready) {
        if (HAL_GetTick() - tickstart > 100) {
            mic_stop();
            return -1;
        }
        taskYIELD();
    }

    float sum = 0.0f;
    for (int i = 0; i < MIC_BUFFER_SIZE; i++)
        sum += (float)ch_mic.frame[i];
    dc_off = sum / MIC_BUFFER_SIZE;
    shout_cnt = 0;

    taskENTER_CRITICAL();
    ch_release_locked(&ch_mic);   /* 消费掉初始标定帧 */
    taskEXIT_CRITICAL();
    return 0;
}

void mic_start(void)
{
    if (!active) {
        ch_reset(&ch_mic);
        ch_reset(&ch_kws);
        shout_cnt = 0;
        HAL_ADC_Start_DMA(&hadc1, (uint32_t*)mic_buf, MIC_BUFFER_SIZE);
        active = 1;
    }
}

void mic_stop(void)
{
    if (active) {
        HAL_ADC_Stop_DMA(&hadc1);
        active = 0;
        ch_reset(&ch_mic);
        ch_reset(&ch_kws);
    }
}

/* [2026-07-11]: KWS 启动后调用 → ISR 开始填 ch_kws 并纳入唤醒。
 *   未激活时不填 ch_kws(省 ISR 拷贝); 方案C本就无耦合, 此仅为效率与启动时序清晰。 */
void mic_set_kws_active(uint8_t on)
{
    taskENTER_CRITICAL();
    kws_active = on ? 1 : 0;
    if (!on) ch_reset(&ch_kws);
    taskEXIT_CRITICAL();
}

int mic_is_frame_ready(void)
{
    return ch_mic.frame_ready;
}

/* 兼容保留: 消费 Mic 通道(等价旧 mic_clear_frame_flag, 现仅作用于 ch_mic) */
void mic_clear_frame_flag(void)
{
    uint8_t re;
    taskENTER_CRITICAL();
    re = ch_release_locked(&ch_mic);
    taskEXIT_CRITICAL();
    if (re) xEventGroupSetBits(xMicEvents, EVT_MIC_FRAME_READY);
}

/* [2026-07-04]: 供 KWS_Task 获取音频帧 — 读 ch_kws 私有通道, 与 Mic 完全独立 */
int mic_get_frame(int16_t *buf)
{
    if (!buf || !active || !ch_kws.frame_ready) return -1;
    uint16_t dc = (uint16_t)(dc_off + 0.5f);
    /* frame_ready=1 期间 ISR 只标 pending、不写 ch_kws.frame → 读期间无写入, 无需临界区 */
    for (int i = 0; i < MIC_BUFFER_SIZE; i++)
        buf[i] = (int16_t)(ch_kws.frame[i] - dc);
    uint8_t re;
    taskENTER_CRITICAL();
    re = ch_release_locked(&ch_kws);
    taskEXIT_CRITICAL();
    if (re) xEventGroupSetBits(xMicEvents, EVT_MIC_FRAME_READY);
    return 0;
}

/* ---------------- 信号处理(读 ch_mic 私有通道; 均由 Mic_Task 单线程调用) ---------------- */

static uint32_t mic_calc_energy(float *out_dc)
{
    if (!active || !ch_mic.frame_ready) {
        *out_dc = dc_off;
        return 0;
    }
    int64_t sum_sq = 0;
    float dc_temp = dc_off;
    for (int i = 0; i < MIC_BUFFER_SIZE; i++) {
        float v = (float)ch_mic.frame[i] - dc_temp;
        sum_sq += (int64_t)(v * v);
        dc_temp = 0.99998f * dc_temp + 0.00002f * (float)ch_mic.frame[i];
    }
    *out_dc = dc_temp;
    return (uint32_t)(sum_sq / MIC_BUFFER_SIZE);
}

uint32_t mic_get_energy(void)
{
    float dc;
    uint32_t energy = mic_calc_energy(&dc);
    taskENTER_CRITICAL();
    dc_off = dc;
    taskEXIT_CRITICAL();
    return energy;
}

static float mic_calc_zcr(float dc_base)
{
    if (!active || !ch_mic.frame_ready)
        return 0.0f;
    int zc = 0;
    float v_prev = (float)ch_mic.frame[0] - dc_base;
    for (int i = 1; i < MIC_BUFFER_SIZE; i++) {
        float v_curr = (float)ch_mic.frame[i] - dc_base;
        if ((v_prev >= 0 && v_curr < 0) || (v_prev < 0 && v_curr >= 0)) {
            if (fabsf(v_curr - v_prev) > 2.0f)
                zc++;
        }
        v_prev = v_curr;
    }
    return (float)zc / (MIC_BUFFER_SIZE - 1);
}

float mic_get_zcr(void)
{
    return mic_calc_zcr(dc_off);
}

int mic_is_shouting(void)
{
    if (!ch_mic.frame_ready)
        return 0;

    /* 读 ch_mic.frame 算 energy/zcr — frame_ready=1 期间 ISR 不改 ch_mic.frame, 读安全 */
    float dc_base;
    uint32_t energy = mic_calc_energy(&dc_base);
    float zcr = mic_calc_zcr(dc_base);

    uint8_t re;
    taskENTER_CRITICAL();
    dc_off = dc_base;
    re = ch_release_locked(&ch_mic);   /* 消费 Mic 通道(替代旧 mic_clear_frame_flag) */
    taskEXIT_CRITICAL();
    if (re) xEventGroupSetBits(xMicEvents, EVT_MIC_FRAME_READY);

    if (energy > SHOUT_ENERGY_THRESH && zcr > SHOUT_ZCR_MIN && zcr < SHOUT_ZCR_MAX) {
        shout_cnt++;
        if (shout_cnt >= SHOUT_CONTINUOUS_CNT) {
            shout_cnt = SHOUT_CONTINUOUS_CNT;
            return 1;
        }
    } else {
        if (shout_cnt > 0)
            shout_cnt--;
    }
    return 0;
}
