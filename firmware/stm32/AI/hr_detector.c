/**
 * @file hr_detector.c
 * @brief PPG 心率检测实现
 *
 * 自适应阈值峰值检测 + 移动平均平滑
 * 不需要 FFT，纯时域算法，MCU 友好
 */

#include "hr_detector.h"
#include "app_config.h"
#include <math.h>
#include <string.h>

/* PPG 信号缓冲 */
#define PPG_BUF_LEN  200  /* 2s @ 100Hz */
static int32_t ppg_buf[PPG_BUF_LEN];
static int ppg_idx = 0;
static int ppg_filled = 0;

/* 峰值检测状态 */
static float last_peak_val = 0;
static float threshold = 0;
static int last_peak_idx = -HR_PEAK_MAX_INTERVAL - 1;
static int rising = 0;
static int32_t prev_val = 0;

/* 心率历史（移动平均） */
#define HR_HISTORY_LEN  HR_SMOOTH_WINDOW
static float hr_history[HR_HISTORY_LEN];
static int hr_hist_idx = 0;
static int hr_hist_count = 0;

/* 静息心率自适应
 * [2026-07-04]: 2 重 guard — 对标 ACSM + Fitbit + Apple Watch 三方共识:
 *   G1: HAR 静态类 (躺/坐/站) — 等效加速度计 no-motion 检测
 *   G2: bpm < resting × 1.3 — 排除运动后恢复期 (运动后 5-30min HR 仍偏高)
 *   EMA α=0.001 本身极慢, 不需额外时间窗口
 *   交叉验证: ACSM 临床安静标准 + Fitbit 专利 WO2015189304A1 + Apple HealthKit RHR */
#define RESTING_UPDATE_ALPHA   0.001f
#define RESTING_GUARD_BPM_CAP  1.3f    /* bpm 不高于当前估算 1.3× */
static int resting_update_counter = 0;
static uint8_t hr_activity_class = 0;   /* 默认静态, 由 app 层每 5.12s 更新 */

/* 信号质量衰减：连续无峰值 > 3s 则降级
 * [2026-07-02 FIX]: 原 signal_quality 永不衰减，传感器脱落后仍报告 80 */
#define SIGNAL_DECAY_TIMEOUT  300  /* 3s @ 100Hz */
static int no_peak_counter = 0;

void hr_detector_init(hr_state_t *state, float sr_hz)
{
    memset(ppg_buf, 0, sizeof(ppg_buf));
    memset(hr_history, 0, sizeof(hr_history));
    ppg_idx = 0;
    ppg_filled = 0;
    last_peak_val = 0;
    threshold = 0;
    last_peak_idx = -HR_PEAK_MAX_INTERVAL - 1;
    rising = 0;
    prev_val = 0;
    hr_hist_idx = 0;
    hr_hist_count = 0;

    state->bpm = 0;
    state->bpm_smoothed = 0;
    state->resting_hr = HR_INIT_RESTING_BPM;
    state->signal_quality = 0;
    state->is_anomaly = 0;
    state->sr_hz = sr_hz;  /* [2026-07-04]: 替换硬编码 100Hz */
}

void hr_detector_set_sample_rate(hr_state_t *state, float sr_hz)
{
    state->sr_hz = sr_hz;
}

int hr_detector_update(hr_state_t *state, int32_t ir_value)
{
    /* 直接存储原始 IR 值（后续可实现滑动窗口移动平均滤波） */
    ppg_buf[ppg_idx] = ir_value;
    ppg_idx = (ppg_idx + 1) % PPG_BUF_LEN;
    if (!ppg_filled && ppg_idx == 0) ppg_filled = 1;

    /* 自适应阈值：用最近缓冲的统计量 */
    if (ppg_filled) {
        float max_v = -1e9f, min_v = 1e9f;
        for (int i = 0; i < PPG_BUF_LEN; i++) {
            if (ppg_buf[i] > max_v) max_v = ppg_buf[i];
            if (ppg_buf[i] < min_v) min_v = ppg_buf[i];
        }
        threshold = min_v + (max_v - min_v) * 0.5f;  /* 中间 50% */
    }

    /* 峰值检测 */
    int new_hr = 0;
    int32_t cur = ir_value;

    if (cur > prev_val) {
        rising = 1;
    } else if (rising && cur < prev_val) {
        /* 下降沿 → 检测到一个峰值 */
        rising = 0;

        if (prev_val > threshold) {
            int cur_peak_pos = ppg_idx - 2;
            if (cur_peak_pos < 0) cur_peak_pos += PPG_BUF_LEN;
            int gap = cur_peak_pos - last_peak_idx;
            if (gap < 0) gap += PPG_BUF_LEN;

            if (gap >= HR_PEAK_MIN_INTERVAL && gap <= HR_PEAK_MAX_INTERVAL) {
                /* 计算 BPM */
                /* [2026-07-04]: 用 state->sr_hz 替换硬编码 100Hz, 支持 50/100Hz 动态切换 */
                float instant_bpm = (state->sr_hz * 60.0f) / gap;
                if (instant_bpm >= HR_MIN_BPM && instant_bpm <= HR_MAX_BPM) {
                    state->bpm = instant_bpm;

                    /* 移动平均 */
                    hr_history[hr_hist_idx] = instant_bpm;
                    hr_hist_idx = (hr_hist_idx + 1) % HR_HISTORY_LEN;
                    if (hr_hist_count < HR_HISTORY_LEN) hr_hist_count++;

                    float sum = 0;
                    for (int i = 0; i < hr_hist_count; i++) sum += hr_history[i];
                    state->bpm_smoothed = sum / hr_hist_count;
                    state->signal_quality = (hr_hist_count >= 3) ? 80 : 30;
                    no_peak_counter = 0;   /* 找到有效峰值，复位衰减计时 */

                    new_hr = 1;
                }
            }
            last_peak_idx = ppg_idx - 2;  /* 峰值是前一个样本(prev_val)，不是当前样本 */
            if (last_peak_idx < 0) last_peak_idx += PPG_BUF_LEN;
            last_peak_val = (float)prev_val;
        }
    }
    prev_val = cur;

    /* [2026-07-04]: 静息心率自适应 — 2 重 guard
     *   仅当用户处于静态类且 HR 未异常偏高时更新, 防止运动污染 resting_hr。
     *   EMA α=0.001 极慢, 无需额外时间窗口。
     *   交叉验证: ACSM + Fitbit WO2015189304A1 + Apple HealthKit (见 CLAUDE.md) */
    resting_update_counter++;
    if (resting_update_counter >= (int)(state->sr_hz * 30.0f)) {  /* 每 30 秒 */
        resting_update_counter = 0;
        if (state->bpm_smoothed > 0
            && hr_activity_class <= 2                         /* G1: 静态类 */
            && state->bpm_smoothed < state->resting_hr
                                     * RESTING_GUARD_BPM_CAP) { /* G2: 排除恢复期 */
            state->resting_hr += RESTING_UPDATE_ALPHA *
                                 (state->bpm_smoothed - state->resting_hr);
        }
    }

    /* 异常判断 */
    state->is_anomaly = hr_detector_check_anomaly(state);

    /* 信号质量衰减：连续无峰值超时则重置历史
     * [2026-07-02 FIX]: 原 signal_quality 永久=80，传感器脱落后不降 */
    no_peak_counter++;
    /* [2026-07-04]: 由采样率动态计算, 替代硬编码 300(@100Hz) */
    if (no_peak_counter > (int)(state->sr_hz * 3.0f)) {
        /* [2026-07-14 Fix]: 原只将 hr_hist_count 清零, 但 hr_hist_idx 未复位。
         *   移动平均 sum 恒从 history[0] 开始求和, 依赖 idx 从 0 顺序填充。
         *   idx 不动 → 信号恢复后新峰值写到旧位置 (如 idx=3), 但 sum 只读 [0..count),
         *   前 HR_HISTORY_LEN 个 bpm_smoothed 全为陈旧值 (脱腕前的旧 BPM)。
         *   同步复位 idx=0 使写入与"从 0 求和"重新对齐, hr_detector_init 本就是
         *   idx=0/count=0 成对, 此处是 decay 通路遗漏该成对性。
         *   修复: 加 hr_hist_idx = 0。触发场景: 脱腕/强运动伪影 >3s 后恢复佩戴。 */
        no_peak_counter = 0;
        hr_hist_count  = 0;
        hr_hist_idx    = 0;
        state->signal_quality = 30;
    }

    return new_hr;
}

/* [2026-07-04]: 从 app 层同步 HAR 活动类别 → resting_hr guard */
void hr_detector_set_activity(int har_class)
{
    if (har_class >= 0 && har_class <= 6)
        hr_activity_class = (uint8_t)har_class;
    /* har_class = -1 (未知) 或超范围 → 保留旧值, 避免误复位 guard */
}

int hr_detector_check_anomaly(const hr_state_t *state)
{
    if (state->bpm_smoothed <= 0) return 0;
    float threshold_bpm = state->resting_hr + HR_ANOMALY_THRESHOLD;
    return (state->bpm_smoothed > threshold_bpm) ? 1 : 0;
}
