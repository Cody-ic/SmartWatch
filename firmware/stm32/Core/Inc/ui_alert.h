/**
 * @file ui_alert.h
 * @brief Toast 告警系统 — 全部使用顶部条（不打断用户操作）
 *
 * 设计：
 *   - 所有告警都是 240×44 顶部条，不全屏覆盖
 *   - 跌倒：红色条 + 3s 倒计时 → 绿色 "SOS SENT" 3s
 *   - 霸凌：隐蔽蓝灰条 + 3s 倒计时（长按取消）→ 绿色 "SOS SENT" 3s
 *   - 心率异常：琥珀色条 + 3s 自动消失
 *
 * UI 层（LVGL），与 safety_detect 引擎解耦。
 * 引擎调用 ui_alert_show() 触发；UI 自动定时消失或用户按键取消。
 *
 * © 2026 AI Watch Project
 */
#ifndef UI_ALERT_H
#define UI_ALERT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ui.h"     /* ui_get_screen() */

/* ---------- 告警类型 ---------- */
typedef enum {
    ALERT_NONE = 0,
    ALERT_FALL_DETECTED,     /* 跌倒 — 红色顶部条 + 3s 倒计时 → SOS SENT */
    ALERT_BULLYING,          /* 霸凌 — 隐蔽蓝灰顶部条 + 3s 倒计时 → SOS SENT */
    ALERT_HR_HIGH,           /* 心率过高 — 琥珀色顶部条 3s */
    ALERT_HR_LOW,            /* 心率过低 — 琥珀色顶部条 3s */
    ALERT_LONG_STILL,        /* 长时间静止 — 琥珀色顶部条 3s */
    ALERT_SLEEP_APNEA,       /* [2026-07-05] 睡眠呼吸暂停 — 醒来时 ODI≥5 提醒 */
} alert_type_t;

/* 级别（保留给外部逻辑用，当前 UI 统一用顶部条） */
typedef enum {
    ALERT_LEVEL_INFO = 0,
    ALERT_LEVEL_WARNING,      /* 心率异常等 — 自动消失 */
    ALERT_LEVEL_CRITICAL,     /* 跌倒/霸凌 — 倒计时 → SOS SENT */
} alert_level_t;

/* ---------- 公共 API ---------- */

/**
 * 初始化告警系统（创建隐藏的容器）
 * 在 ui_init() 后调用一次。
 */
void ui_alert_init(void);

/**
 * 弹出告警（顶部条，不全屏覆盖）。
 *  - 跌倒/霸凌：倒计时 3s → 绿色 "SOS SENT" 3s → 自动消失
 *  - 心率异常：  琥珀色条 3s → 自动消失
 *
 * @param type      告警类型（决定颜色/图标/默认文案）
 * @param title     标题文案（可为 NULL，用类型默认）
 * @param detail    副文案（如 "BPM: 135"；可为 NULL）
 */
void ui_alert_show(alert_type_t type,
                   const char *title,
                   const char *detail);

/**
 * 立即关闭告警（用户按键或外部解除）
 */
void ui_alert_dismiss(void);

/**
 * 是否有 CRITICAL 告警正在显示（用于阻塞翻页等其他逻辑）
 */
int ui_alert_is_active(void);

/**
 * 获取当前告警类型
 */
alert_type_t ui_alert_current(void);

#ifdef __cplusplus
}
#endif
#endif /* UI_ALERT_H */
