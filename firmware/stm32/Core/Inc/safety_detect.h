/**
 * @file safety_detect.h
 * @brief 安全检测引擎 — 跌倒/霸凌/心率异常
 *
 * 与 UI 完全解耦：本模块只决定"何时该告警"，
 * 通过回调函数通知 UI（ui_alert_show）。
 *
 * 检测算法（轻量，MCU 可负担）：
 *
 *  跌倒 (Fall):
 *    - 加速度合量 > 3g (冲击) → 0g (失重静止) 检测
 *    - 静止持续 3s 内未起身 → 触发 ALERT_FALL_DETECTED
 *
 *  霸凌 (Bullying):
 *    - 持续高频剧烈动作 (>2g 波动) > 5s
 *    - + 心率突增 (>30 BPM 上升)
 *    - + 非运动类 HAR → 触发 ALERT_BULLYING
 *
 *  心率异常 (HR Anomaly):
 *    - 非运动状态：HR > 120 (HR_HIGH) 或 < 40 (HR_LOW)
 *    - 持续 > 10 秒 → 触发告警
 *
 * © 2026 AI Watch Project
 */
#ifndef SAFETY_DETECT_H
#define SAFETY_DETECT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "ui_alert.h"   /* alert_type_t */

/* ---------- 配置阈值（可在编译时覆盖） ---------- */
#ifndef SAFETY_FALL_ACCEL_G
#define SAFETY_FALL_ACCEL_G       3.0f   /* 冲击加速度阈值 */
#endif
#ifndef SAFETY_FALL_STILL_MS
#define SAFETY_FALL_STILL_MS      3000   /* 冲击后静止时长 */
#endif
#ifndef SAFETY_FALL_STILL_ACCEL_G
#define SAFETY_FALL_STILL_ACCEL_G  1.3f  /* [2026-07-04] 静止判定阈值, 原硬编码 */
#endif
#ifndef SAFETY_FALL_RISE_ACCEL_G
#define SAFETY_FALL_RISE_ACCEL_G   2.0f  /* [2026-07-04] 起身取消阈值, 原硬编码 */
#endif
#ifndef SAFETY_FALL_DEBOUNCE_CNT
#define SAFETY_FALL_DEBOUNCE_CNT   8     /* [2026-07-04] 连续样本防抖 (~80ms@100Hz), 考虑250ms批次间隙 */
#endif
#ifndef SAFETY_BULLY_ACCEL_G
#define SAFETY_BULLY_ACCEL_G      2.0f   /* 剧烈动作阈值 */
#endif
#ifndef SAFETY_BULLY_DURATION_MS
#define SAFETY_BULLY_DURATION_MS  5000   /* 持续时长 */
#endif
#ifndef SAFETY_HR_HIGH_BPM
#define SAFETY_HR_HIGH_BPM        120    /* 非运动高心率 */
#endif
#ifndef SAFETY_HR_LOW_BPM
#define SAFETY_HR_LOW_BPM         40     /* 低心率 */
#endif
#ifndef SAFETY_HR_DURATION_MS
#define SAFETY_HR_DURATION_MS     10000  /* 异常心率持续时长 */
#endif

/* ---------- 公共 API ---------- */

/**
 * 初始化（清空内部状态）
 */
void safety_init(void);

/**
 * 喂入 IMU 样本（来自 LSM6DSOX FIFO）
 *
 * @param ts_ms      时间戳（ms）
 * @param ax/ay/az   加速度原始值（已转换为 g）
 * @param gx/gy/gz   陀螺仪（dps，可暂不使用）
 * @param har_class  当前 HAR 分类（用于霸凌/运动区分）
 */
void safety_feed_imu(uint32_t ts_ms,
                     float ax, float ay, float az,
                     float gx, float gy, float gz,
                     int har_class);

/**
 * 喂入心率样本（来自 MAX30102 算法）
 *
 * @param ts_ms   时间戳（ms）
 * @param bpm     当前心率
 * @param valid   信号是否有效
 */
void safety_feed_hr(uint32_t ts_ms, int bpm, int valid);

/**
 * 喂入声学检测结果（来自麦克风喊叫检测）
 *
 * @param ts_ms       时间戳（ms）
 * @param is_shouting 是否检测到喊叫
 */
void safety_feed_shout(uint32_t ts_ms, int is_shouting);

/**
 * 状态机 tick（每秒调用一次）
 * 内部检测时长窗口、状态转换、触发告警
 */
void safety_tick_1hz(void);

#ifdef __cplusplus
}
#endif
#endif /* SAFETY_DETECT_H */
