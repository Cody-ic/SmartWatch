/**
 * @file    ui_clock.c
 * @brief   Page 0 - Clock (time + date)
 */
#include "ui.h"
#include <stdio.h>

static lv_obj_t *lbl_time, *lbl_colon, *lbl_date, *lbl_weather;
static lv_timer_t *blink_timer;

static void blink_cb(lv_timer_t *t)
{
    static uint8_t on = 1;
    on = !on;
    lv_obj_set_style_opa(lbl_colon, on ? LV_OPA_COVER : LV_OPA_20, 0);
}

void ui_clock_create(lv_obj_t *parent)
{
    /* Background: 深蓝紫渐变（LVGL 8.x 仅支持 VER/HOR，用垂直渐变近似径向效果） */
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x1a1a3e), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_color(parent, lv_color_hex(0x060614), 0);
    lv_obj_set_style_bg_grad_dir(parent, LV_GRAD_DIR_VER, 0);

    /* Time: HH : MM */
    lbl_time = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_time, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lbl_time, lv_color_white(), 0);
    lv_obj_align(lbl_time, LV_ALIGN_CENTER, 0, -16);
    lv_label_set_text(lbl_time, "00  00");

    /* Colon overlay - blinks every 500ms. Aligned to same Y as time text. */
    lbl_colon = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_colon, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lbl_colon, lv_color_hex(0x4fc3f7), 0);
    lv_label_set_text(lbl_colon, ":");
    lv_obj_align(lbl_colon, LV_ALIGN_CENTER, 0, -16);   /* same Y as lbl_time */

    /* Date */
    lbl_date = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_date, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_date, lv_color_hex(0x888888), 0);
    lv_obj_align(lbl_date, LV_ALIGN_CENTER, 0, 14);
    lv_label_set_text(lbl_date, "Jun 7  Sat");

    /* Weather footer */
    lbl_weather = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_weather, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_weather, lv_color_hex(0xff9800), 0);
    lv_obj_align(lbl_weather, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_label_set_text(lbl_weather, "26" "\xC2\xB0" "C");

    blink_timer = lv_timer_create(blink_cb, 500, NULL);
}

void ui_clock_update(uint8_t hour, uint8_t minute,
                     uint8_t day, uint8_t month, uint8_t weekday)
{
    /* 注意：时间文本里不放 ":"，冒号由独立的 lbl_colon 叠加上去实现闪烁。
     * 所以这里用双空格占位，冒号 label 正好落在空格位置上。 */
    char buf[32];  /* 足够容纳最长日期字符串 "September 31  Wednesday" (26+1=27字节) */
    snprintf(buf, sizeof(buf), "%02u  %02u", (unsigned)hour, (unsigned)minute);
    lv_label_set_text(lbl_time, buf);

    static const char *wkday[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    static const char *moname[] = {"","Jan","Feb","Mar","Apr","May","Jun",
                                   "Jul","Aug","Sep","Oct","Nov","Dec"};
    if (weekday > 6) weekday = 6;
    if (month > 12 || month < 1) month = 1;
    if (day < 1 || day > 31) day = 1;

    snprintf(buf, sizeof(buf), "%s %u  %s", moname[month], (unsigned)day, wkday[weekday]);
    lv_label_set_text(lbl_date, buf);
}

/**
 * @brief 更新天气/温度显示（供 BLE 任务或外部数据源调用）
 * @param temp_c  温度（摄氏度），-128 表示无效/不更新
 * @param icon    LVGL 符号或 NULL
 */
void ui_clock_update_weather(int8_t temp_c, const char *icon)
{
    if (temp_c == -128 && icon == NULL) return;

    if (temp_c != -128) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d" "\xC2\xB0" "C", (int)temp_c);
        lv_label_set_text(lbl_weather, buf);
    }
    /* icon 可用于未来扩展（如晴天/雨天符号） */
    (void)icon;
}
