/**
 * @file    ui_hr.c
 * @brief   Page 2 - Heart rate (BPM + ECG-style waveform)
 *
 * The waveform is a scrolling line buffer (100 samples).
 * Feed new PPG samples via ui_hr_update().
 */
#include "ui.h"
#include <stdio.h>
#include <string.h>

/* [2026-07-03]: 用内嵌位图替代 U+2665, 因 lv_font_montserrat_28 不含该字形 */
extern const lv_img_dsc_t img_heart;

#define HR_WAVE_LEN  100  /* samples in waveform */

static lv_obj_t *lbl_bpm, *lbl_unit, *lbl_status;
static lv_obj_t *heart_icon;
static lv_obj_t *chart;

/* Waveform data */

/* Heartbeat pulse animation (scale 1.0 -> 1.12 -> 1.0) */
static lv_anim_t pulse_anim;

static void pulse_anim_cb(void *var, int32_t v)
{
    lv_obj_t *lbl = (lv_obj_t *)var;
    lv_obj_set_style_transform_zoom(lbl, v, 0);
}

void ui_hr_create(lv_obj_t *parent)
{
    /* Dark red radial background */
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x2e0a0a), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_color(parent, lv_color_hex(0x0a0a0a), 0);
    lv_obj_set_style_bg_grad_dir(parent, LV_GRAD_DIR_VER, 0);

    /* Title */
    lv_obj_t *title = lv_label_create(parent);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x666666), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);
    lv_label_set_text(title, "HEART RATE");

    /* Heart icon (pulses) — [2026-07-03]: 改用内嵌位图, U+2665 字体缺失 */
    heart_icon = lv_img_create(parent);
    lv_img_set_src(heart_icon, &img_heart);
    lv_obj_set_style_img_recolor(heart_icon, lv_color_hex(0xff1744), 0);
    lv_obj_set_style_img_recolor_opa(heart_icon, LV_OPA_COVER, 0);
    lv_obj_align(heart_icon, LV_ALIGN_CENTER, 0, -32);
    lv_obj_set_style_transform_zoom(heart_icon, 256, 0);   /* 1.0 */
    /* Pivot 设在对象中心（auto-size 后动态计算，避免偏移） */
    lv_obj_update_layout(heart_icon);
    lv_obj_set_style_transform_pivot_x(heart_icon, lv_obj_get_width(heart_icon) / 2, 0);
    lv_obj_set_style_transform_pivot_y(heart_icon, lv_obj_get_height(heart_icon) / 2, 0);

    /* Pulse animation: 256 (=1.0x) -> 287 (=1.12x) -> 256 */
    lv_anim_init(&pulse_anim);
    lv_anim_set_var(&pulse_anim, heart_icon);
    lv_anim_set_values(&pulse_anim, 256, 287);
    lv_anim_set_time(&pulse_anim, 200);
    lv_anim_set_playback_time(&pulse_anim, 200);
    lv_anim_set_repeat_count(&pulse_anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&pulse_anim, pulse_anim_cb);
    lv_anim_start(&pulse_anim);

    /* BPM number */
    lbl_bpm = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_bpm, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lbl_bpm, lv_color_white(), 0);
    lv_obj_align(lbl_bpm, LV_ALIGN_CENTER, -10, 4);
    lv_label_set_text(lbl_bpm, "72");

    /* BPM unit */
    lbl_unit = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_unit, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_unit, lv_color_hex(0xff5252), 0);
    lv_obj_align(lbl_unit, LV_ALIGN_CENTER, 36, 16);
    lv_label_set_text(lbl_unit, "BPM");

    /* Status text */
    lbl_status = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_status, lv_color_hex(0x888888), 0);
    lv_obj_align(lbl_status, LV_ALIGN_CENTER, 0, 32);
    lv_label_set_text(lbl_status, "Normal");

    /* ECG waveform chart */
    chart = lv_chart_create(parent);
    lv_obj_set_size(chart, 200, 50);
    lv_obj_align(chart, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, HR_WAVE_LEN);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 255);
    lv_chart_set_div_line_count(chart, 0, 0);
    lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);

    /* Hide chart background, just show the line */
    lv_obj_set_style_bg_opa(chart, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(chart, LV_OPA_TRANSP, 0);
    lv_obj_set_style_line_color(chart, lv_color_hex(0xff1744), LV_PART_ITEMS);
    lv_obj_set_style_line_width(chart, 1, LV_PART_ITEMS);
    lv_obj_set_style_size(chart, 0, LV_PART_INDICATOR);  /* hide points */
    lv_obj_set_style_pad_all(chart, 2, 0);

    /* Single series */
    lv_chart_series_t *ser = lv_chart_add_series(chart,
                                                  lv_color_hex(0xff1744),
                                                  LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_all_value(chart, ser, 128);  /* mid-line baseline */

    /* Stash series pointer for update fn */
    lv_obj_set_user_data(chart, ser);

}

void ui_hr_update(uint8_t bpm, uint16_t ppg_sample)
{
    /* BPM display */
    char buf[8];
    snprintf(buf, sizeof(buf), "%u", (unsigned)bpm);
    lv_label_set_text(lbl_bpm, buf);

    /* Status text based on BPM */
    const char *status = "Normal";
    if (bpm < 50)       status = "Low";
    else if (bpm > 100) status = "High";
    lv_label_set_text(lbl_status, status);

    /* Append sample to chart (shifts automatically) */
    lv_chart_series_t *ser = (lv_chart_series_t *)lv_obj_get_user_data(chart);
    if (ser) {
        /* [2026-07-04 Fix]: PPG 18-bit (MAX30102, &0x3FFFF) → main.c 取低16-bit
         *   → 此处 0-65535。原 `if>255 clamp` 将手指信号(>50000)直接削为 255→
         *   波形恒为平线无生理信息。>>8 映射 65535→255, 无需额外 clamp。
         *   交叉验证: MAX30102 DS(18-bit 0x3FFFF) + SparkFun(返回raw不缩放)
         *   + LVGL chart 量程 0-255。 */
        lv_chart_set_next_value(chart, ser, (lv_coord_t)(ppg_sample >> 8));
        lv_chart_refresh(chart);
    }
}
