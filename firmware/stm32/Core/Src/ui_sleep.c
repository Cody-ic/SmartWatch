/**
 * @file ui_sleep.c
 * @brief Page — 综合睡眠 (睡眠分期 + 呼吸暂停初筛)
 *
 * 240x240 圆形屏: 睡眠状态 + ODI + 严重度 + 事件统计
 *
 * [2026-07-05]: 新增, 合并 sleep_staging + sleep_apnea
 */
#include "ui.h"
#include "sleep_apnea.h"
#include <stdio.h>

/* --- 外部全局 --- */
extern volatile float  g_spo2_value;
extern volatile uint8_t g_spo2_valid;
extern sleep_apnea_state_t g_apnea;        /* sleep_apnea.c 定义 */

/* --- 控件 --- */
static lv_obj_t *lbl_title;
static lv_obj_t *lbl_sleep_state;   /* "● Asleep" / "○ Awake" */
static lv_obj_t *lbl_sleep_dur;     /* "Duration: 2.3h" */
static lv_obj_t *lbl_odi;           /* ODI 大数字 */
static lv_obj_t *lbl_odi_unit;      /* "events/hr" + severity */
static lv_obj_t *lbl_stats_top;     /* Events | Lowest */
static lv_obj_t *lbl_stats_bot;     /* Avg dur | Monitored */
static lv_obj_t *lbl_spo2_info;     /* SpO2 + 基线 */

/* ============================================================
 * 创建页面
 * ============================================================ */
void ui_sleep_create(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_color(parent, lv_color_hex(0x060614), 0);
    lv_obj_set_style_bg_grad_dir(parent, LV_GRAD_DIR_VER, 0);

    /* --- y=14: 标题 --- */
    lbl_title = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0x78909C), 0);
    lv_label_set_text(lbl_title, "SLEEP");
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 14);

    /* --- y=36: 睡眠时长 — 主视觉, 大号白字 --- */
    lbl_sleep_dur = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_sleep_dur, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_sleep_dur, lv_color_white(), 0);
    lv_label_set_text(lbl_sleep_dur, "--");
    lv_obj_align(lbl_sleep_dur, LV_ALIGN_TOP_MID, 0, 36);

    /* --- y=68: 睡眠状态 --- */
    lbl_sleep_state = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_sleep_state, &lv_font_montserrat_14, 0);
    lv_label_set_text(lbl_sleep_state, "No Data");
    lv_obj_align(lbl_sleep_state, LV_ALIGN_TOP_MID, 0, 68);

    /* --- y≈96: ODI + 严重度 --- */
    lbl_odi = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_odi, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_odi, lv_color_white(), 0);
    lv_label_set_text(lbl_odi, "--");
    lv_obj_align(lbl_odi, LV_ALIGN_CENTER, -4, -14);

    /* --- ODI 单位 + 严重度 --- */
    lbl_odi_unit = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_odi_unit, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_odi_unit, lv_color_hex(0x78909C), 0);
    lv_label_set_text(lbl_odi_unit, "events/hr");
    lv_obj_align(lbl_odi_unit, LV_ALIGN_CENTER, 28, -6);

    /* --- 统计行1 --- */
    lbl_stats_top = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_stats_top, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_stats_top, lv_color_hex(0xB0BEC5), 0);
    lv_label_set_text(lbl_stats_top, "Events: --   Low: --%");
    lv_obj_align(lbl_stats_top, LV_ALIGN_CENTER, 0, 28);

    /* --- 统计行2 --- */
    lbl_stats_bot = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_stats_bot, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_stats_bot, lv_color_hex(0x78909C), 0);
    lv_label_set_text(lbl_stats_bot, "Avg: --s   Mon: --");
    lv_obj_align(lbl_stats_bot, LV_ALIGN_CENTER, 0, 50);

    /* --- y=215: SpO2 实时 + 基线 --- */
    lbl_spo2_info = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_spo2_info, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_spo2_info, lv_color_hex(0x546E7A), 0);
    lv_label_set_text(lbl_spo2_info, "SpO2: --   BL: --");
    lv_obj_align(lbl_spo2_info, LV_ALIGN_BOTTOM_MID, 0, -14);
}

/* ============================================================
 * 更新 — 由 app_tick 每秒调用
 * ============================================================ */
void ui_sleep_update(void)
{
    sleep_apnea_state_t *s = &g_apnea;
    char buf[48];
    uint8_t has_data = (s->is_asleep || s->total_events > 0 || s->total_monitor_s > 0);

    /* ---- 睡眠状态 --- */
    if (s->is_asleep) {
        lv_label_set_text(lbl_sleep_state, LV_SYMBOL_WIFI "  Asleep");
        lv_obj_set_style_text_color(lbl_sleep_state, lv_color_hex(0x66BB6A), 0);
    } else if (s->total_events > 0 || s->total_monitor_s > 0) {
        /* 有昨晚数据 */
        lv_label_set_text(lbl_sleep_state, LV_SYMBOL_CLOSE "  Awake");
        lv_obj_set_style_text_color(lbl_sleep_state, lv_color_hex(0x78909C), 0);
    } else {
        lv_label_set_text(lbl_sleep_state, "No Data");
        lv_obj_set_style_text_color(lbl_sleep_state, lv_color_hex(0x546E7A), 0);
    }

    /* ---- 睡眠时长 (主视觉) --- */
    if (s->total_sleep_s >= 60) {
        float hours = (float)s->total_sleep_s / 3600.0f;
        lv_snprintf(buf, sizeof(buf), "%.1fh", (double)hours);
        lv_label_set_text(lbl_sleep_dur, buf);
    } else if (s->is_asleep) {
        uint32_t sec = s->total_sleep_s;
        lv_snprintf(buf, sizeof(buf), "%lum", (unsigned long)(sec / 60));
        lv_label_set_text(lbl_sleep_dur, buf);
    } else if (s->total_events > 0 || s->total_monitor_s > 0) {
        /* 醒来后显示昨晚时长 */
        float hours = (float)s->total_sleep_s / 3600.0f;
        lv_snprintf(buf, sizeof(buf), "%.1fh", (double)hours);
        lv_label_set_text(lbl_sleep_dur, buf);
    } else {
        lv_label_set_text(lbl_sleep_dur, "--");
    }

    /* ---- ODI --- */
    if (!has_data) {
        lv_label_set_text(lbl_odi, "--");
        lv_obj_set_style_text_color(lbl_odi, lv_color_hex(0x455A64), 0);
        lv_label_set_text(lbl_odi_unit, "events/hr");
        lv_obj_set_style_text_color(lbl_odi_unit, lv_color_hex(0x546E7A), 0);
    } else if (s->total_monitor_s >= 60) {
        float odi = sleep_apnea_get_odi(s);
        lv_snprintf(buf, sizeof(buf), "%.1f", (double)odi);
        lv_label_set_text(lbl_odi, buf);

        /* ODI 颜色 */
        sleep_apnea_severity_e sev = sleep_apnea_get_severity(s);
        switch (sev) {
            case ODI_NORMAL:
                lv_obj_set_style_text_color(lbl_odi, lv_color_white(), 0);
                break;
            case ODI_MILD:
                lv_obj_set_style_text_color(lbl_odi, lv_color_hex(0xFFB300), 0);
                break;
            case ODI_MODERATE:
                lv_obj_set_style_text_color(lbl_odi, lv_color_hex(0xFF9800), 0);
                break;
            case ODI_SEVERE:
                lv_obj_set_style_text_color(lbl_odi, lv_color_hex(0xEF5350), 0);
                break;
            default:
                break;
        }

        /* 单位 + 严重度 */
        const char *sev_name = sleep_apnea_severity_name(sev);
        lv_snprintf(buf, sizeof(buf), "events/hr  %s", sev_name);
        lv_label_set_text(lbl_odi_unit, buf);

        switch (sev) {
            case ODI_NORMAL:
                lv_obj_set_style_text_color(lbl_odi_unit, lv_color_hex(0x66BB6A), 0);
                break;
            case ODI_MILD:
                lv_obj_set_style_text_color(lbl_odi_unit, lv_color_hex(0xFFB300), 0);
                break;
            case ODI_MODERATE:
                lv_obj_set_style_text_color(lbl_odi_unit, lv_color_hex(0xFF9800), 0);
                break;
            case ODI_SEVERE:
                lv_obj_set_style_text_color(lbl_odi_unit, lv_color_hex(0xEF5350), 0);
                break;
            default:
                break;
        }
    } else {
        /* 监测中但不足 1 分钟 */
        lv_label_set_text(lbl_odi, "--");
        lv_obj_set_style_text_color(lbl_odi, lv_color_hex(0x455A64), 0);
        lv_label_set_text(lbl_odi_unit, "baseline...");
        lv_obj_set_style_text_color(lbl_odi_unit, lv_color_hex(0x42A5F5), 0);
    }

    /* ---- 统计行1: Events | Lowest --- */
    {
        uint16_t events = sleep_apnea_get_events(s);
        float low = sleep_apnea_get_lowest_spo2(s);

        if (events > 0 || low < 100.0f) {
            lv_snprintf(buf, sizeof(buf), "Events: %u   Low: %.0f%%",
                       (unsigned)events, (double)low);
            lv_label_set_text(lbl_stats_top, buf);
            /* 最低 SpO2 < 85% 标红 */
            if (low < 85.0f)
                lv_obj_set_style_text_color(lbl_stats_top, lv_color_hex(0xEF5350), 0);
            else if (events > 0)
                lv_obj_set_style_text_color(lbl_stats_top, lv_color_hex(0xB0BEC5), 0);
        } else {
            lv_label_set_text(lbl_stats_top, "Events: 0   Low: --%");
            lv_obj_set_style_text_color(lbl_stats_top, lv_color_hex(0x78909C), 0);
        }
    }

    /* ---- 统计行2: Avg dur | Monitor --- */
    {
        uint16_t events = sleep_apnea_get_events(s);
        uint32_t mon_s  = sleep_apnea_get_monitor_time(s);

        char dur_str[16];
        if (events > 0 && s->sum_desat_dur_s > 0.0f) {
            float avg = s->sum_desat_dur_s / (float)events;
            lv_snprintf(dur_str, sizeof(dur_str), "Avg: %.0fs", (double)avg);
        } else {
            lv_snprintf(dur_str, sizeof(dur_str), "Avg: --s");
        }

        char mon_str[16];
        if (mon_s >= 3600) {
            float h = (float)mon_s / 3600.0f;
            lv_snprintf(mon_str, sizeof(mon_str), "Mon: %.1fh", (double)h);
        } else if (mon_s >= 60) {
            lv_snprintf(mon_str, sizeof(mon_str), "Mon: %lum", (unsigned long)(mon_s / 60));
        } else {
            lv_snprintf(mon_str, sizeof(mon_str), "Mon: --");
        }

        lv_snprintf(buf, sizeof(buf), "%s   %s", dur_str, mon_str);
        lv_label_set_text(lbl_stats_bot, buf);
    }

    /* ---- SpO2 + 基线 --- */
    {
        float spo2 = g_spo2_valid ? g_spo2_value : 0.0f;
        if (g_spo2_valid && spo2 > 0.0f) {
            lv_snprintf(buf, sizeof(buf), "SpO2: %.0f%%   BL: %.0f%%",
                       (double)spo2, (double)s->spo2_baseline);
        } else if (s->spo2_baseline > 50.0f) {
            lv_snprintf(buf, sizeof(buf), "SpO2: --   BL: %.0f%%",
                       (double)s->spo2_baseline);
        } else {
            lv_snprintf(buf, sizeof(buf), "SpO2: --   BL: --");
        }
        lv_label_set_text(lbl_spo2_info, buf);
    }
}
