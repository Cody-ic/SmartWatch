/**
 * @file    ui_steps.c
 * @brief   Page 3 - Step counter with progress ring
 *
 * Shows daily steps, goal ring, distance (km), kcal.
 */
#include "ui.h"
#include <stdio.h>

/* Steps ring radius (for arc display) */

static lv_obj_t *arc_progress;
static lv_obj_t *lbl_steps_num, *lbl_goal;
static lv_obj_t *lbl_km, *lbl_kcal;

void ui_steps_create(lv_obj_t *parent)
{
    /* Dark green radial background */
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x0a2e0a), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_color(parent, lv_color_hex(0x060606), 0);
    lv_obj_set_style_bg_grad_dir(parent, LV_GRAD_DIR_VER, 0);

    /* Title */
    lv_obj_t *title = lv_label_create(parent);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x666666), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);
    lv_label_set_text(title, "STEPS TODAY");

    /* ---- Progress arc ---- */
    arc_progress = lv_arc_create(parent);
    lv_obj_set_size(arc_progress, 120, 120);
    lv_obj_align(arc_progress, LV_ALIGN_CENTER, 0, -8);

    /* Arc config */
    lv_arc_set_rotation(arc_progress, 270);  /* start at 12 o'clock */
    lv_arc_set_bg_angles(arc_progress, 0, 360);
    lv_arc_set_value(arc_progress, 72);      /* 0-100 percent */
    lv_arc_set_range(arc_progress, 0, 100);

    /* Bg arc (track) */
    lv_obj_set_style_arc_color(arc_progress, lv_color_hex(0x1b3a1f), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_progress, 8, LV_PART_MAIN);

    /* Fg arc (progress) */
    lv_obj_set_style_arc_color(arc_progress, lv_color_hex(0x4CAF50), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_progress, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc_progress, true, LV_PART_INDICATOR);

    /* Hide knob */
    lv_obj_remove_style(arc_progress, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc_progress, LV_OBJ_FLAG_CLICKABLE);

    /* Steps number (centered in arc) */
    lbl_steps_num = lv_label_create(arc_progress);
    lv_obj_set_style_text_font(lbl_steps_num, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_steps_num, lv_color_white(), 0);
    lv_obj_align(lbl_steps_num, LV_ALIGN_CENTER, 0, -6);
    lv_label_set_text(lbl_steps_num, "7234");

    /* Goal below number */
    lbl_goal = lv_label_create(arc_progress);
    lv_obj_set_style_text_font(lbl_goal, &lv_font_montserrat_8, 0);
    lv_obj_set_style_text_color(lbl_goal, lv_color_hex(0x666666), 0);
    lv_obj_align(lbl_goal, LV_ALIGN_CENTER, 0, 16);
    lv_label_set_text(lbl_goal, "/ 10000");

    /* ---- Distance + kcal row ---- */
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, 200, 32);
    lv_obj_align(row, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);

    /* Distance */
    lv_obj_t *col_km = lv_obj_create(row);
    lv_obj_remove_style_all(col_km);
    lv_obj_set_size(col_km, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(col_km, 0, 0);
    lv_obj_set_flex_flow(col_km, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col_km, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lbl_km = lv_label_create(col_km);
    lv_obj_set_style_text_font(lbl_km, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_km, lv_color_hex(0x4CAF50), 0);
    lv_label_set_text(lbl_km, "5.2");

    lv_obj_t *lbl_km_u = lv_label_create(col_km);
    lv_obj_set_style_text_font(lbl_km_u, &lv_font_montserrat_8, 0);
    lv_obj_set_style_text_color(lbl_km_u, lv_color_hex(0x666666), 0);
    lv_label_set_text(lbl_km_u, "km");

    /* Separator */
    lv_obj_t *sep = lv_label_create(row);
    lv_obj_set_style_text_font(sep, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(sep, lv_color_hex(0x333333), 0);
    lv_label_set_text(sep, "|");

    /* Kcal */
    lv_obj_t *col_kcal = lv_obj_create(row);
    lv_obj_remove_style_all(col_kcal);
    lv_obj_set_size(col_kcal, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(col_kcal, 0, 0);
    lv_obj_set_flex_flow(col_kcal, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col_kcal, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lbl_kcal = lv_label_create(col_kcal);
    lv_obj_set_style_text_font(lbl_kcal, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_kcal, lv_color_hex(0xff9800), 0);
    lv_label_set_text(lbl_kcal, "312");

    lv_obj_t *lbl_kcal_u = lv_label_create(col_kcal);
    lv_obj_set_style_text_font(lbl_kcal_u, &lv_font_montserrat_8, 0);
    lv_obj_set_style_text_color(lbl_kcal_u, lv_color_hex(0x666666), 0);
    lv_label_set_text(lbl_kcal_u, "kcal");
}

void ui_steps_update(uint32_t steps, uint32_t goal)
{
    char buf[16];

    /* Number */
    snprintf(buf, sizeof(buf), "%u", (unsigned)steps);
    lv_label_set_text(lbl_steps_num, buf);

    /* Goal */
    snprintf(buf, sizeof(buf), "/ %u", (unsigned)goal);
    lv_label_set_text(lbl_goal, buf);

    /* Arc progress 0-100% */
    uint8_t pct;
    if (goal == 0 || steps >= goal) {
        pct = 100;
    } else {
        pct = (uint8_t)((steps * 100) / goal);
    }
    lv_arc_set_value(arc_progress, pct);

    /* Derived: distance (~0.72m per step) and kcal (~0.043 per step) */
    /* 用整数运算避免 %.1f（newlib-nano 默认不支持 float printf） */
    uint32_t km_10 = (uint32_t)(steps * 72) / 10000;   /* km × 10 (0.72m/step → /10000) */
    uint32_t kcal = (uint32_t)(steps * 43) / 1000;
    snprintf(buf, sizeof(buf), "%u.%u", (unsigned)(km_10 / 10), (unsigned)(km_10 % 10));
    lv_label_set_text(lbl_km, buf);
    snprintf(buf, sizeof(buf), "%u", (unsigned)kcal);
    lv_label_set_text(lbl_kcal, buf);
}
