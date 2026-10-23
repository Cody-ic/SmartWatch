/**
 * @file ui_alert.c
 * @brief Toast 告警 — 药丸形顶部条，适配 GC9A01 圆形屏
 *
 * 圆屏适配（240×240 圆，圆心 120,120, r=120）：
 *   - 药丸 180×32px，居中 x=30, y=12
 *   - y=12 处圆屏可用宽 ≈ 105px，但药丸圆角 r=16 与圆弧形状接近，
 *     两侧微小裁切视觉上不可见
 *   - 文字区域限制在中心 ~140px，确保核心内容在安全区内
 *
 * 两阶段设计：
 *   - 跌倒：红 #E53935 倒计时 3s → 绿 #4CAF50 SOS SENT 3s
 *   - 霸凌：蓝灰 #455A64 倒计时 3s（隐蔽）→ 绿 SOS SENT 3s
 *   - 心率异常：琥珀 #FFB300 3s 自动消失
 *
 * © 2026 AI Watch Project
 */
#include "ui_alert.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

#include "lvgl.h"

/* ============================================================
 * 药丸参数 — 圆屏适配
 * ============================================================
 *
 * 圆屏 y=12 处可用宽 ≈ 105px
 * 圆屏 y=28（药丸中心）处可用宽 ≈ 155px
 * → 药丸 180px 宽时，两侧各有 ~12px 在圆弧外
 *   但圆角 16px ≈ 圆弧曲率，视觉上无缝
 *
 * 文字安全区：中心 140px（x=50~190），绝对不会被切
 */
#define PILL_W       180
#define PILL_H       32
#define PILL_R       16          /* 圆角 = 高度/2 = 药丸形 */
#define PILL_X       30          /* (240 - 180) / 2 */
#define PILL_Y       12

/* 文字布局（药丸内部） */
#define ICON_PAD     10          /* 图标距药丸左边缘 */
#define TEXT_X       28          /* 文字起始 x（图标右边） */

/* ---------- 内部状态 ---------- */
static lv_obj_t *overlay   = NULL;
static lv_obj_t *bar       = NULL;
static lv_obj_t *bar_icon  = NULL;
static lv_obj_t *bar_text  = NULL;

static alert_type_t cur_type   = ALERT_NONE;
static int          countdown  = 0;
static lv_timer_t  *tick_timer = NULL;
static int          phase_sent = 0;   /* 0=倒计时, 1=已发送 */

/* ---------- 默认文案 ---------- */
static const char *default_icon(alert_type_t t) {
    switch (t) {
        case ALERT_FALL_DETECTED: return LV_SYMBOL_DOWN;
        case ALERT_BULLYING:      return LV_SYMBOL_WARNING;
        case ALERT_HR_HIGH:       return LV_SYMBOL_UP;
        case ALERT_HR_LOW:        return LV_SYMBOL_DOWN;
        case ALERT_LONG_STILL:    return LV_SYMBOL_WARNING;
        case ALERT_SLEEP_APNEA:  return LV_SYMBOL_CHARGE;  /* 充电=睡眠隐喻 */
        default: return LV_SYMBOL_BELL;
    }
}

static const char *default_text(alert_type_t t) {
    switch (t) {
        case ALERT_FALL_DETECTED: return "FALL";
        case ALERT_BULLYING:      return "HOLD";
        case ALERT_HR_HIGH:       return "HR HIGH";
        case ALERT_HR_LOW:        return "HR LOW";
        case ALERT_LONG_STILL:    return "STILL";
        case ALERT_SLEEP_APNEA:   return "APNEA";
        default: return "ALERT";
    }
}

/* ---------- 药丸显示 ---------- */
static void pill_show(lv_color_t bg, const char *icon, const char *text)
{
    if (!overlay) return;

    lv_obj_set_style_bg_color(bar, bg, 0);

    lv_label_set_text(bar_icon, icon);
    lv_obj_set_style_text_color(bar_icon, lv_color_white(), 0);

    lv_label_set_text(bar_text, text);
    lv_obj_set_style_text_color(bar_text, lv_color_white(), 0);

    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(overlay);
}

/* ---------- 倒计时 / 阶段切换 ---------- */
static void tick_cb(lv_timer_t *t)
{
    (void)t;

    if (phase_sent) {
        /* 已发送阶段 — 3s 后关闭 */
        countdown--;
        if (countdown <= 0) {
            if (tick_timer) { lv_timer_del(tick_timer); tick_timer = NULL; }
            ui_alert_dismiss();
        }
        return;
    }

    /* 倒计时阶段 */
    countdown--;
    if (countdown <= 0) {
        /* 倒计时结束 → 进入"已发送"阶段 */
        phase_sent = 1;
        countdown = 3;   /* SOS SENT 显示 3 秒 */
        pill_show(lv_color_hex(0x4CAF50), LV_SYMBOL_OK, "SOS SENT");
        return;
    }

    /* 更新倒计时数字 */
    char buf[24];
    if (cur_type == ALERT_BULLYING) {
        lv_snprintf(buf, sizeof(buf), "HOLD  %d", countdown);
    } else {
        lv_snprintf(buf, sizeof(buf), "FALL  %d", countdown);
    }
    lv_label_set_text(bar_text, buf);
}

/* ---------- 初始化 ---------- */
void ui_alert_init(void)
{
    /* 覆盖层挂在 lv_layer_top()，不受屏幕切换影响 */

    /* 覆盖层：药丸大小，居中 */
    overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, PILL_W, PILL_H);
    lv_obj_set_pos(overlay, PILL_X, PILL_Y);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_TRANSP, 0);

    /* 药丸面板 */
    bar = lv_obj_create(overlay);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, PILL_W, PILL_H);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bar, PILL_R, 0);    /* 药丸圆角 */

    /* 图标 */
    bar_icon = lv_label_create(bar);
    lv_obj_set_style_text_font(bar_icon, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(bar_icon, lv_color_white(), 0);
    lv_obj_align(bar_icon, LV_ALIGN_LEFT_MID, ICON_PAD, 0);

    /* 主文案 */
    bar_text = lv_label_create(bar);
    lv_obj_set_style_text_font(bar_text, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(bar_text, lv_color_white(), 0);
    lv_obj_align(bar_text, LV_ALIGN_LEFT_MID, TEXT_X, 0);
}

/* ---------- 公共 API ---------- */
void ui_alert_show(alert_type_t type,
                   const char *title,
                   const char *detail)
{
    if (!overlay) ui_alert_init();

    cur_type   = type;
    phase_sent = 0;

    if (tick_timer) { lv_timer_del(tick_timer); tick_timer = NULL; }

    switch (type) {

    case ALERT_FALL_DETECTED:
        countdown = 3;
        {
            char buf[24];
            lv_snprintf(buf, sizeof(buf), "FALL  %d", countdown);
            pill_show(lv_color_hex(0xE53935), LV_SYMBOL_DOWN, buf);
        }
        tick_timer = lv_timer_create(tick_cb, 1000, NULL);
        lv_timer_set_repeat_count(tick_timer, -1);
        break;

    case ALERT_BULLYING:
        countdown = 3;
        {
            char buf[24];
            lv_snprintf(buf, sizeof(buf), "HOLD  %d", countdown);
            pill_show(lv_color_hex(0x455A64), LV_SYMBOL_WARNING, buf);
        }
        tick_timer = lv_timer_create(tick_cb, 1000, NULL);
        lv_timer_set_repeat_count(tick_timer, -1);
        break;

    case ALERT_HR_HIGH:
    case ALERT_HR_LOW:
    case ALERT_LONG_STILL:
    case ALERT_SLEEP_APNEA:  /* [2026-07-05]: 醒来提醒, 琥珀色 3s */
        {
            char buf[48];
            const char *base = title ? title : default_text(type);
            if (detail && detail[0]) {
                lv_snprintf(buf, sizeof(buf), "%s %s", base, detail);
            } else {
                lv_snprintf(buf, sizeof(buf), "%s", base);
            }
            pill_show(lv_color_hex(0xFFB300),
                      default_icon(type), buf);
        }
        /* 3s 后自动关闭 */
        phase_sent = 1;
        countdown  = 3;
        tick_timer = lv_timer_create(tick_cb, 1000, NULL);
        break;

    default:
        ui_alert_dismiss();  /* 未知类型 → 清理遗留 overlay */
        return;
    }
}

void ui_alert_dismiss(void)
{
    if (!overlay) return;
    if (tick_timer) { lv_timer_del(tick_timer); tick_timer = NULL; }
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    cur_type   = ALERT_NONE;
    phase_sent = 0;
    countdown  = 0;
}

int ui_alert_is_active(void)
{
    return (cur_type != ALERT_NONE) && overlay &&
           !lv_obj_has_flag(overlay, LV_OBJ_FLAG_HIDDEN);
}

alert_type_t ui_alert_current(void)
{
    return cur_type;
}
