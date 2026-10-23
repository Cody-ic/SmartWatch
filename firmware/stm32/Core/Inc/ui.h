/**
 * @file    ui.h
 * @brief   AI Watch UI - LVGL 240x240 round display
 *
 * 5 pages (left/right swipe or buttons):
 *   1) Clock     - time + date
 *   2) HAR       - 7-class action recognition (project highlight)
 *   3) Heart     - BPM + ECG waveform
 *   4) Steps     - daily step ring
 *   5) Adaptive  - AI 原型网络自适应状态（在线学习/校准）
 *
 * Plus overlaid Toast 告警系统:
 *   - ALERT_FALL_DETECTED  (跌倒，红色全屏 + 5s SOS 倒计时)
 *   - ALERT_BULLYING       (霸凌，红色 + 3s 长按解除)
 *   - ALERT_HR_HIGH / LOW  (心率异常，黄色 Toast)
 *
 * Integration:
 *   1) Call ui_init() once after LVGL is up
 *   2) LVGL tick is handled by app_tick() (lv_tick_inc + lv_timer_handler)
 *   3) Call ui_<page>_update() when new data arrives
 *   4) Call ui_page_next/prev() from button EXTI
 *   5) Safety events via ui_alert_show() (see ui_alert.h)
 */
#ifndef UI_H
#define UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include <stdint.h>

/* ============================================================
 * HAR 7-class enum (MUST match har7class_v6_int8 output index)
 * ============================================================ */
typedef enum {
    HAR_LYING = 0,
    HAR_SITTING,
    HAR_STANDING,
    HAR_WALKING,
    HAR_JOGGING,
    HAR_DOWNSTAIRS,
    HAR_CYCLING,
    HAR_COUNT
} har_class_t;

/* HAR 7-class color palette (shared by ui_har.c, ui_adaptive.c) */
extern const lv_color_t ui_har_colors[HAR_COUNT];

/* ============================================================
 * Public API
 * ============================================================ */

/* Init UI - call once after lv_init() and display driver is ready */
void ui_init(void);

/* Page navigation (also handled by tileview swipe) */
void ui_page_next(void);
void ui_page_prev(void);
uint8_t ui_get_current_page(void);

/* ---- Clock page ---- */
/* hour: 0-23, minute: 0-59, day: 1-31, month: 1-12, weekday: 0=Sun */
void ui_clock_update(uint8_t hour, uint8_t minute,
                     uint8_t day, uint8_t month, uint8_t weekday);
/* temp_c: -128=不更新, icon: NULL=不更新（供 BLE/传感器调用） */
void ui_clock_update_weather(int8_t temp_c, const char *icon);

/* ---- HAR page ---- */
/* best: index of top class, probs[7]: 0-255 confidence per class */
void ui_har_update(har_class_t best, const uint8_t probs[HAR_COUNT]);

/* ---- Heart rate page ---- */
/* bpm: heart rate, ppg_sample: latest PPG sample for waveform (0-255) */
void ui_hr_update(uint8_t bpm, uint16_t ppg_sample);

/* ---- Steps page ---- */
/* steps: current count, goal: daily target (e.g. 10000) */
void ui_steps_update(uint32_t steps, uint32_t goal);

/* ---- Safety / Alert (toast overlay) ---- */
#include "ui_alert.h"

/* ---- Adaptive page (5th page) ---- */
#include "ui_adaptive.h"

/* ---- Safety detection engine ---- */
#include "safety_detect.h"

#ifdef __cplusplus
}
#endif

#endif /* UI_H */
