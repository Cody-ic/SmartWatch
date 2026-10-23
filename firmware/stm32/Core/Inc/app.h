/**
 * @file    app.h
 * @brief   AI Watch 应用整合层
 *
 * 把 UI / HAR 推理 / 安全检测 / 步数 / 时钟串成一条完整的数据流水线。
 * main.c 只需要调 4 个函数即可跑起来：
 *
 *   app_init()                         — 初始化
 *   app_on_imu_batch(acc25, gyr25)    — IMU FIFO 批次到达
 *   app_on_hr_ready(bpm, sample, ok)  — 心率算法输出就绪
 *   app_tick()                         — main loop 每次迭代（驱动 LVGL）
 *
 * 对应的嵌入式数据流（非 HTML 演示）：
 *
 *   [LSM6DSOX FIFO 中断 260ms]
 *       ↓ 26 样本 → 降采样 25
 *   app_on_imu_batch()
 *       ↓
 *   HAR_AddSamples()  → 累积到 512 样本 → AI 推理 → ui_har_update()
 *       ↓ 推理完成后
 *   safety_feed_imu() → 跌倒/霸凌检测 → ui_alert_show()
 *       ↓ HAR 结果
 *   step counter      → walking/jogging 时步数累加 → ui_steps_update()
 *
 *   [MAX30102 + 心率算法]
 *       ↓ BPM 输出
 *   app_on_hr_ready()
 *       ↓
 *   ui_hr_update() + safety_feed_hr() → 心率异常告警
 *
 *   [main loop 每次]
 *   app_tick()
 *       ↓
 *   lv_timer_handler() + 1Hz 定时（safety_tick_1hz + 时钟刷新）
 *
 * © 2026 AI Watch Project
 */
#ifndef APP_H
#define APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * 初始化全部子系统：UI、HAR 推理引擎、安全检测、步数计数器、时钟基准。
 * 在 main() 的外设初始化（I2C/IMU/PPG）完成后调用一次。
 */
void app_init(void);

/**
 * IMU FIFO 批次到达时调用（25 个样本 × 6 通道，已经在 main.c 里降采样好）。
 * 内部自动累积、推理、更新 UI、喂安全引擎。
 */
void app_on_imu_batch(int16_t *acc25, int16_t *gyr25);

/**
 * 心率算法输出就绪时调用。
 * bpm:        心率值（次/分钟）
 * ppg_sample: 最新 PPG 波形采样（0-255，用于画 ECG 曲线）
 * valid:      1=本次 BPM 有效，0=算法未收敛
 * resting_hr: hr_detector 自适应静息心率, 传入运动评分模型修正 Karvonen 公式
 *             [2026-07-04]: 替换硬编码 72bpm
 */
void app_on_hr_ready(int bpm, uint16_t ppg_sample, int valid, float resting_hr);

/**
 * main loop 每次迭代调用。
 * 内部做：lv_tick_inc + lv_timer_handler + 1Hz 定时任务。
 */
void app_tick(void);

/**
 * 设置当前时间（由用户/RTC 在启动时注入）。
 * hour:   0-23
 * minute: 0-59
 */
void app_set_time(uint8_t hour, uint8_t minute);

/**
 * 按键事件回调。CubeMX 配好按键 GPIO 后，
 * 在 HAL_GPIO_EXTI_Callback() 里调这个函数即可。
 *
 * button_id:
 *   0 = 电源键（短按暂无定义，长按待扩展）
 *   1 = 上翻/确认键 → ui_page_prev()
 *   2 = 下翻/返回键 → ui_page_next()
 */
void app_on_button(uint8_t button_id);

/**
 * @brief [2026-07-04]: 手机 BLE 下发用户参数 (体重/年龄)
 * @param weight_kg  体重 (kg), 范围 20-250
 * @param age        年龄 (岁), 范围 5-120
 */
void app_set_user_profile(float weight_kg, uint8_t age);

#ifdef __cplusplus
}
#endif

#endif /* APP_H */
