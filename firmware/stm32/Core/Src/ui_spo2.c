/**
 * @file ui_spo2.c
 * @brief Page — SpO2 血氧显示 (对标 spo2_watch.html)
 *
 * 240x240 圆形屏适配: 大数字居中, PPG 波形, 灌注指数, HR 联动
 * 颜色: 绿(≥95) / 黄(90-95) / 红(<90) / 灰(无信号)
 *
 * [2026-07-05]: 新增, 配合 spo2.h v2 心跳分段算法
 */
#include "ui.h"
#include "spo2.h"
#include <stdio.h>

/* --- 外部全局: main.c Sensor_Task 更新 --- */
extern volatile float  g_spo2_value;
extern volatile uint8_t g_spo2_valid;

/* --- 波形长度 --- */
#define SPO2_WAVE_LEN 100

/* --- 控件 --- */
static lv_obj_t *lbl_title;
static lv_obj_t *lbl_value;
static lv_obj_t *lbl_unit;
static lv_obj_t *lbl_status;
static lv_obj_t *lbl_perfusion;
static lv_obj_t *lbl_hr;
static lv_obj_t *chart;
static lv_chart_series_t *chart_ser;

/* --- 波形数据 (环形缓冲, 模拟 PPG) --- */
static lv_coord_t wave_buf[SPO2_WAVE_LEN];
static uint8_t    wave_idx = 0;
static uint8_t    wave_phase = 0;

/* ============================================================
 * 创建页面
 * ============================================================ */
void ui_spo2_create(lv_obj_t *parent)
{
    /* --- 背景: 深蓝紫渐变 (匹配其他页面) --- */
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_color(parent, lv_color_hex(0x060614), 0);
    lv_obj_set_style_bg_grad_dir(parent, LV_GRAD_DIR_VER, 0);

    /* --- y=18: 标题 --- */
    lbl_title = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0x78909C), 0);
    lv_label_set_text(lbl_title, "BLOOD OXYGEN");
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 18);

    /* --- 中心: SpO2 大数字 --- */
    lbl_value = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_value, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lbl_value, lv_color_white(), 0);
    lv_label_set_text(lbl_value, "--");
    lv_obj_align(lbl_value, LV_ALIGN_CENTER, -8, -24);

    /* --- % 单位 --- */
    lbl_unit = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_unit, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_unit, lv_color_hex(0x78909C), 0);
    lv_label_set_text(lbl_unit, "%");
    lv_obj_align(lbl_unit, LV_ALIGN_CENTER, 40, -18);

    /* --- PPG 波形 (chart, 100 点 SHIFT 模式) --- */
    chart = lv_chart_create(parent);
    lv_obj_set_size(chart, 200, 50);
    lv_obj_align(chart, LV_ALIGN_CENTER, 0, 48);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, SPO2_WAVE_LEN);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_div_line_count(chart, 0, 0);
    lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_obj_set_style_bg_opa(chart, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(chart, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(chart, 2, 0);
    lv_obj_set_style_size(chart, 0, LV_PART_INDICATOR);

    chart_ser = lv_chart_add_series(chart, lv_color_hex(0x66BB6A),
                                    LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_all_value(chart, chart_ser, 50);  /* 中线起步 */

    /* --- y≈190: 灌注指数 --- */
    lbl_perfusion = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_perfusion, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_perfusion, lv_color_hex(0x546E7A), 0);
    lv_label_set_text(lbl_perfusion, "PI: --");
    lv_obj_align(lbl_perfusion, LV_ALIGN_CENTER, 0, 70);

    /* --- y≈210: 状态 --- */
    lbl_status = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_12, 0);
    lv_label_set_text(lbl_status, "No Signal");
    lv_obj_align(lbl_status, LV_ALIGN_CENTER, 0, 90);

    /* --- 底部: HR 联动 --- */
    lbl_hr = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_hr, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_hr, lv_color_hex(0x455A64), 0);
    lv_label_set_text(lbl_hr, "HR -- BPM");
    lv_obj_align(lbl_hr, LV_ALIGN_BOTTOM_MID, 0, -16);
}

/* ============================================================
 * 更新 — 由 app_tick 每秒调用
 * ============================================================ */
void ui_spo2_update(uint8_t bpm)
{
    float spo2    = g_spo2_valid ? g_spo2_value : 0.0f;
    int   spo2_int = (int)(spo2 + 0.5f);
    char  buf[16];

    /* --- SpO2 数字 + 颜色 --- */
    if (!g_spo2_valid || spo2_int == 0) {
        lv_label_set_text(lbl_value, "--");
        lv_obj_set_style_text_color(lbl_value, lv_color_hex(0x455A64), 0);
        lv_label_set_text(lbl_status, "No Signal");
        lv_obj_set_style_text_color(lbl_status, lv_color_hex(0x78909C), 0);
        lv_label_set_text(lbl_perfusion, "PI: --");
        lv_obj_set_style_line_color(chart, lv_color_hex(0x333333), LV_PART_ITEMS);
    } else {
        lv_snprintf(buf, sizeof(buf), "%d", spo2_int);
        lv_label_set_text(lbl_value, buf);

        if (spo2 >= 95.0f) {
            lv_obj_set_style_text_color(lbl_value, lv_color_white(), 0);
            lv_label_set_text(lbl_status, "Normal");
            lv_obj_set_style_text_color(lbl_status, lv_color_hex(0x66BB6A), 0);
            lv_obj_set_style_line_color(chart, lv_color_hex(0x66BB6A), LV_PART_ITEMS);
        } else if (spo2 >= 90.0f) {
            lv_obj_set_style_text_color(lbl_value, lv_color_hex(0xFFB300), 0);
            lv_label_set_text(lbl_status, "Mild Hypoxia");
            lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xFFB300), 0);
            lv_obj_set_style_line_color(chart, lv_color_hex(0xFFB300), LV_PART_ITEMS);
        } else {
            lv_obj_set_style_text_color(lbl_value, lv_color_hex(0xEF5350), 0);
            lv_label_set_text(lbl_status, "Low SpO2");
            lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xEF5350), 0);
            lv_obj_set_style_line_color(chart, lv_color_hex(0xEF5350), LV_PART_ITEMS);
        }

        /* 灌注指数 (占位, 需 hardware calibration) */
        lv_snprintf(buf, sizeof(buf), "PI: 3.2%%");
        lv_label_set_text(lbl_perfusion, buf);
    }

    /* --- HR 联动 --- */
    if (bpm > 0) {
        lv_snprintf(buf, sizeof(buf), "HR %u BPM", (unsigned)bpm);
        lv_label_set_text(lbl_hr, buf);
    }

    /* --- 模拟 PPG 波形 --- */
    lv_coord_t y;
    uint8_t phase = wave_phase % 28;  /* ~72 BPM @ ~0.5s 刷新 */
    if (phase < 4)       y = 50 + 20;                    /* 基线 */
    else if (phase < 8)  y = 50 + 20 - 14 * (phase-4)/4;  /* 上升 */
    else if (phase < 12) y = 50 + 6 + 6 * (phase-8)/4;    /* 下降 */
    else if (phase < 16) y = 50 + 6 + 2;                  /* 重搏 */
    else                 y = 50 + 20;                      /* 舒张 */
    wave_phase++;

    if (!g_spo2_valid) y = 50;  /* 无信号 = 平线 */

    wave_buf[wave_idx] = y;
    wave_idx = (wave_idx + 1) % SPO2_WAVE_LEN;
    lv_chart_set_next_value(chart, chart_ser, y);
    lv_chart_refresh(chart);
}
