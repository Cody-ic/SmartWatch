/**
 * @file hr_detector.h
 * @brief PPG 心率检测 — 峰值检测 + 移动平均
 */

#ifndef HR_DETECTOR_H
#define HR_DETECTOR_H

#include <stdint.h>

#define HR_MAX_BPM      220
#define HR_MIN_BPM      30

typedef struct {
    float bpm;              /* 当前瞬时心率 */
    float bpm_smoothed;     /* 平滑后心率 */
    float resting_hr;       /* 静息心率（自适应更新） */
    uint8_t signal_quality; /* 0-100 信号质量 */
    uint8_t is_anomaly;     /* 心率异常标志 */
    float sr_hz;            /* [2026-07-04]: 当前采样率, 替换硬编码 100Hz */
} hr_state_t;

/**
 * @brief 初始化心率检测器
 * @param sr_hz  采样率 (Hz), 如 100.0f 或 50.0f
 */
void hr_detector_init(hr_state_t *state, float sr_hz);

/**
 * @brief [2026-07-04]: 运行时切换采样率 (MLC 省电时降频)
 * @param sr_hz  新采样率 (Hz)
 */
void hr_detector_set_sample_rate(hr_state_t *state, float sr_hz);

/**
 * @brief 输入一个 PPG 样本，更新心率
 * @param ir_value PPG IR 通道值
 * @return 0=未计算出, 1=有新心率值
 */
int hr_detector_update(hr_state_t *state, int32_t ir_value);

/**
 * @brief 判断心率是否异常
 * @return 1=异常（超过静息+阈值）, 0=正常
 */
int hr_detector_check_anomaly(const hr_state_t *state);

/**
 * @brief [2026-07-04]: 同步当前活动类别 → resting_hr 更新 guard
 *
 * 调用时机: HAR 推理完成后 (app_on_imu_batch), 传入最新 HAR class.
 * har_class: 0=躺 1=坐 2=站 3=走 4=跑 5=下楼 6=骑行, -1=未知
 *
 * 仅当用户处于静态类 (≤2) 时, resting_hr EMA 才允许更新,
 * 对标 ACSM 临床标准 + Fitbit 加速度计静止检测 + Apple Watch 静止期要求。
 */
void hr_detector_set_activity(int har_class);

#endif /* HR_DETECTOR_H */
