/**
 * @file    ui_har.c
 * @brief   Page 1 - HAR action recognition (project highlight)
 *
 * Shows the current best-class label, big icon, confidence %,
 * and a 7-bar probability histogram.
 *
 * Uses English labels by default (no Chinese font needed).
 * To enable Chinese: provide a CN font and change HAR_SHORT_NAMES.
 */
#include "ui.h"
#include <stdio.h>
#include <math.h>  /* [2026-07-04]: expf for softmax */

extern const lv_color_t ui_har_colors[HAR_COUNT];

/* Short English labels (no font dependency) */
static const char *HAR_SHORT_NAMES[HAR_COUNT] = {
    "LYING", "SITTING", "STAND", "WALK", "JOG", "DOWN", "CYCLE"
};

/* Long display name (shown in big text) */
static const char *HAR_DISPLAY_NAMES[HAR_COUNT] = {
    "Lying", "Sitting", "Standing", "Walking", "Jogging", "Downstairs", "Cycling"
};

/* LVGL built-in symbol per class (best-effort mapping) */
static const char *HAR_SYMBOLS[HAR_COUNT] = {
    LV_SYMBOL_POWER,    /* lying   - power/rest */
    LV_SYMBOL_HOME,     /* sitting - home/static */
    LV_SYMBOL_UP,       /* standing */
    LV_SYMBOL_PLAY,     /* walking - moving */
    LV_SYMBOL_CHARGE,   /* jogging - energy/lightning */
    LV_SYMBOL_DOWN,     /* downstairs */
    LV_SYMBOL_REFRESH,  /* cycling - loop */
};

static lv_obj_t *lbl_title;
static lv_obj_t *lbl_icon, *lbl_result, *lbl_conf;
static lv_obj_t *bars[HAR_COUNT];        /* bar fill rectangles */
static lv_obj_t *bar_labels[HAR_COUNT];

void ui_har_create(lv_obj_t *parent)
{
    /* Dark blue radial background */
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_color(parent, lv_color_hex(0x060606), 0);
    lv_obj_set_style_bg_grad_dir(parent, LV_GRAD_DIR_VER, 0);

    /* Title */
    lbl_title = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0x666666), 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 24);
    lv_label_set_text(lbl_title, "AI ACTION");

    /* Big icon */
    lbl_icon = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_icon, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lbl_icon, lv_color_hex(0x4CAF50), 0);
    lv_obj_align(lbl_icon, LV_ALIGN_CENTER, 0, -30);
    lv_label_set_text(lbl_icon, LV_SYMBOL_PLAY);

    /* Result name */
    lbl_result = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_result, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_result, lv_color_white(), 0);
    lv_obj_align(lbl_result, LV_ALIGN_CENTER, 0, 4);
    lv_label_set_text(lbl_result, "Walking");

    /* Confidence % */
    lbl_conf = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_conf, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_conf, lv_color_hex(0x4fc3f7), 0);
    lv_obj_align(lbl_conf, LV_ALIGN_CENTER, 0, 28);
    lv_label_set_text(lbl_conf, "92%");

    /* ---- 7-bar histogram at bottom ---- */
    lv_obj_t *bar_cont = lv_obj_create(parent);
    lv_obj_remove_style_all(bar_cont);
    lv_obj_set_size(bar_cont, 200, 40);
    lv_obj_align(bar_cont, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_obj_set_flex_flow(bar_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar_cont, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);

    for (uint8_t i = 0; i < HAR_COUNT; i++) {
        lv_obj_t *col = lv_obj_create(bar_cont);
        lv_obj_remove_style_all(col);
        lv_obj_set_size(col, 22, 40);
        lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(col, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        /* Bar background (track) */
        lv_obj_t *track = lv_obj_create(col);
        lv_obj_remove_style_all(track);
        lv_obj_set_size(track, 16, 28);
        lv_obj_set_style_radius(track, 2, 0);
        lv_obj_set_style_bg_color(track, lv_color_hex(0x1a1a1a), 0);
        lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(track, lv_color_hex(0x333333), 0);
        lv_obj_set_style_border_width(track, 1, 0);
        lv_obj_set_style_pad_all(track, 0, 0);

        /* Bar fill */
        lv_obj_t *fill = lv_obj_create(track);
        lv_obj_remove_style_all(fill);
        lv_obj_set_size(fill, lv_pct(100), 0);
        lv_obj_align(fill, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_radius(fill, 2, 0);
        lv_obj_set_style_bg_color(fill, ui_har_colors[i], 0);
        lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, 0);
        bars[i] = fill;

        /* Label under bar */
        lv_obj_t *tag = lv_label_create(col);
        lv_obj_set_style_text_font(tag, &lv_font_montserrat_8, 0);
        lv_obj_set_style_text_color(tag, lv_color_hex(0x888888), 0);
        lv_label_set_text(tag, HAR_SHORT_NAMES[i]);
        bar_labels[i] = tag;
    }
}

void ui_har_update(har_class_t best, const uint8_t probs[HAR_COUNT])
{
    if (best >= HAR_COUNT) best = HAR_WALKING;

    /* [2026-07-04 Fix]: QDQ 反量化 + softmax → 真实概率。
     *   原 probs[i]*100/255 将量化值 (zp=132, scale=0.0888) 当线性概率——
     *   例如 zp=132→"51.7%", 但实际 logit=0 (中性), softmax 取决于各类分布。
     *   交叉验证: ST EdgeAI report + HAR har_model 参数 + Jacob 2018 QDQ
     *   软最大数值稳定: 减 max_logit 防指数上溢。
     *   同步修复: har_inference.c HAR_GetLastConfidence() 同源修改。 */
    float logits[7], max_logit = -1e30f, sum_exp = 0.0f;
    for (int i = 0; i < HAR_COUNT; i++) {
        logits[i] = ((float)probs[i] - 132.0f) * 0.088777043f;
        if (logits[i] > max_logit) max_logit = logits[i];
    }
    for (int i = 0; i < HAR_COUNT; i++)
        sum_exp += expf(logits[i] - max_logit);
    float confidence = expf(logits[best] - max_logit) / sum_exp;

    /* Update big display */
    lv_label_set_text(lbl_icon, HAR_SYMBOLS[best]);
    lv_obj_set_style_text_color(lbl_icon, ui_har_colors[best], 0);
    lv_label_set_text(lbl_result, HAR_DISPLAY_NAMES[best]);

    /* Confidence of the top class */
    char buf[8];
    unsigned conf_pct = (unsigned)(confidence * 100.0f + 0.5f);
    snprintf(buf, sizeof(buf), "%u%%", conf_pct > 100 ? 100 : conf_pct);
    lv_label_set_text(lbl_conf, buf);

    /* Update bar heights (max = 100% = 28px) */
    for (uint8_t i = 0; i < HAR_COUNT; i++) {
        float prob = expf(logits[i] - max_logit) / sum_exp;
        uint16_t h = (uint16_t)(28.0f * prob + 0.5f);
        if (i == best && h < 4) h = 4;
        lv_obj_set_height(bars[i], h);
        lv_obj_align(bars[i], LV_ALIGN_BOTTOM_MID, 0, 0);
    }
}
