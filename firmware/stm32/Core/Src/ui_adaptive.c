/**
 * @file ui_adaptive.c
 * @brief 第5页：AI 端侧自适应学习（中文界面，圆屏适配版）
 *
 * 圆屏安全区计算（240×240 圆，圆心 120,120, r=120）：
 *
 *   y偏移   可用宽度     用途
 *   ─────────────────────────────
 *   y=18    ~140px      标题 + 状态点
 *   y=38    ~178px      大字号状态
 *   y=58    ~200px      置信度 / 裕度
 *   y=78    ~222px      7类网格（2列）
 *   y=148   ~222px      更新次数
 *   y=168   ~210px      校准按钮
 *
 * 7类 2列纵向布局（HTML 演示验证过）：
 *   列1(躺/站/跑/骑行)  列2(坐/走/下楼)
 *
 * © 2026 AI Watch Project
 */
#include "ui_adaptive.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

#include "lvgl.h"

/* ============================================================
 * 中文字体（14px 黑体，lv_font_conv 生成）
 * ============================================================ */
extern const lv_font_t lv_font_cn_14;
#define CN_FONT  &lv_font_cn_14

/* ---------- 状态字符串（中文 UTF-8） ---------- */
static const char *state_str(adaptive_state_t s) {
    switch (s) {
        case ADAPT_OFF:      return "\xE5\x85\xB3\xE9\x97\xAD";             /* 关闭 */
        case ADAPT_COLLECT:  return "\xE9\x87\x87\xE9\x9B\x86\xE4\xB8\xAD"; /* 采集中 */
        case ADAPT_READY:    return "\xE5\xB0\xB1\xE7\xBB\xAA";             /* 就绪 */
        case ADAPT_UPDATING: return "\xE6\x9B\xB4\xE6\x96\xB0\xE4\xB8\xAD"; /* 更新中 */
        case ADAPT_UPDATED:  return "\xE5\xAE\x8C\xE6\x88\x90";             /* 完成 */
        default: return "?";
    }
}

static lv_color_t state_color(adaptive_state_t s) {
    switch (s) {
        case ADAPT_OFF:      return lv_color_hex(0x9E9E9E); /* 灰 */
        case ADAPT_COLLECT:  return lv_color_hex(0x2196F3); /* 蓝 */
        case ADAPT_READY:    return lv_color_hex(0xFFB300); /* 黄 */
        case ADAPT_UPDATING: return lv_color_hex(0xE53935); /* 红 */
        case ADAPT_UPDATED:  return lv_color_hex(0x4CAF50); /* 绿 */
        default:             return lv_color_hex(0x9E9E9E);
    }
}

/* HAR 类别名（中文） */
static const char *class_cn[7] = {
    "\xE8\xBA\xBA",                          /* 躺 */
    "\xE5\x9D\x90",                          /* 坐 */
    "\xE7\xAB\x99",                          /* 站 */
    "\xE8\xB5\xB0",                          /* 走 */
    "\xE8\xB7\x91",                          /* 跑 */
    "\xE4\xB8\x8B\xE6\xA5\xBC",              /* 下楼 */
    "\xE9\xAA\x91\xE8\xA1\x8C",              /* 骑行 */
};

/* ============================================================
 * 布局参数 — 圆屏安全区适配
 * ============================================================ */

/* 7类 2列网格：
 *   每项：类名(24px) + 进度条(50px) + 数量(14px) ≈ 88px
 *   两列 + 间距：88*2 + 14 = 190px
 *   居中：x起点 = (240-190)/2 = 25
 *   列2 x = 25 + 88 + 14 = 127
 *
 *   行高 18px，4 行（最后一行只有1项）
 *   y起点 = 78
 */
#define GRID_COL1_X     25
#define GRID_COL2_X     127
#define GRID_ROW_Y0     78
#define GRID_ROW_DY     18      /* 行间距 */
#define GRID_BAR_W      50
#define GRID_BAR_H      4
#define GRID_NAME_W     24

/* 列/行分配：
 *   row0: [0 躺] [1 坐]
 *   row1: [2 站] [3 走]
 *   row2: [4 跑] [5 下楼]
 *   row3: [6 骑行]
 */
static const int8_t grid_col[7] = {0, 1, 0, 1, 0, 1, 0};  /* 0=col1, 1=col2 */
static const int8_t grid_row[7] = {0, 0, 1, 1, 2, 2, 3};

/* ---------- 局部控件 ---------- */
static lv_obj_t *lbl_title      = NULL;
static lv_obj_t *lbl_state_dot  = NULL;
static lv_obj_t *lbl_state      = NULL;
static lv_obj_t *lbl_conf       = NULL;
static lv_obj_t *lbl_margin     = NULL;

static lv_obj_t *bar_bg[7];
static lv_obj_t *bar_fill[7];
static lv_obj_t *bar_label[7];
static lv_obj_t *bar_count[7];

static lv_obj_t *lbl_updates    = NULL;
static lv_obj_t *btn_calibrate  = NULL;
static lv_obj_t *lbl_btn        = NULL;

/* ---------- 校准按钮回调 ---------- */
__attribute__((weak)) void ui_adaptive_on_calibrate(void) {}  /* 弱定义，用户可覆写 */

static void calibrate_btn_cb(lv_event_t *e) {
    (void)e;
    ui_adaptive_on_calibrate();
}

/* ============================================================
 * 创建第5页
 * ============================================================ */
void ui_adaptive_create(lv_obj_t *tile) {
    /* 背景：蓝紫径向渐变 */
    lv_obj_set_style_bg_color(tile, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_color(tile, lv_color_hex(0x060614), 0);
    lv_obj_set_style_bg_grad_dir(tile, LV_GRAD_DIR_VER, 0);

    /* ---- y=18: 标题 + 状态灯 ---- */
    lbl_title = lv_label_create(tile);
    lv_obj_set_style_text_font(lbl_title, CN_FONT, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xCFD8DC), 0);
    lv_label_set_text(lbl_title,
        "\xE8\x87\xAA\xE9\x80\x82\xE5\xBA\x94\xE5\xAD\xA6\xE4\xB9\xA0"); /* 自适应学习 */
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, -10, 14);  /* 略偏左给状态灯留位 */

    lbl_state_dot = lv_obj_create(tile);
    lv_obj_remove_style_all(lbl_state_dot);
    lv_obj_set_size(lbl_state_dot, 7, 7);
    lv_obj_set_style_radius(lbl_state_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(lbl_state_dot, lv_color_hex(0x9E9E9E), 0);
    lv_obj_set_style_bg_opa(lbl_state_dot, LV_OPA_COVER, 0);
    lv_obj_align(lbl_state_dot, LV_ALIGN_TOP_MID, 38, 18); /* 标题右侧 */

    /* ---- y=38: 大字号状态 ---- */
    lbl_state = lv_label_create(tile);
    lv_obj_set_style_text_font(lbl_state, CN_FONT, 0);
    lv_obj_set_style_text_color(lbl_state, lv_color_hex(0x9E9E9E), 0);
    lv_label_set_text(lbl_state, "\xE5\x85\xB3\xE9\x97\xAD"); /* 关闭 */
    lv_obj_align(lbl_state, LV_ALIGN_TOP_MID, 0, 36);

    /* ---- y=58: 置信度 / 裕度 ---- */
    lbl_conf = lv_label_create(tile);
    lv_obj_set_style_text_font(lbl_conf, CN_FONT, 0);
    lv_obj_set_style_text_color(lbl_conf, lv_color_hex(0xB0BEC5), 0);
    lv_label_set_text(lbl_conf, "\xE7\xBD\xAE\xE4\xBF\xA1\xE5\xBA\xA6: --"); /* 置信度: -- */
    lv_obj_align(lbl_conf, LV_ALIGN_TOP_LEFT, 28, 58);

    lbl_margin = lv_label_create(tile);
    lv_obj_set_style_text_font(lbl_margin, CN_FONT, 0);
    lv_obj_set_style_text_color(lbl_margin, lv_color_hex(0xB0BEC5), 0);
    lv_label_set_text(lbl_margin, "\xE8\xA3\x95\xE5\xBA\xA6: --"); /* 裕度: -- */
    lv_obj_align(lbl_margin, LV_ALIGN_TOP_RIGHT, -28, 58);

    /* ---- y=78~: 7类样本进度条（2列纵向）---- */
    for (int i = 0; i < 7; i++) {
        int8_t cx = (grid_col[i] == 0) ? GRID_COL1_X : GRID_COL2_X;
        int16_t cy = GRID_ROW_Y0 + grid_row[i] * GRID_ROW_DY;

        /* 类名 */
        bar_label[i] = lv_label_create(tile);
        lv_obj_set_style_text_font(bar_label[i], CN_FONT, 0);
        lv_obj_set_style_text_color(bar_label[i], lv_color_hex(0x90A4AE), 0);
        lv_label_set_text(bar_label[i], class_cn[i]);
        lv_obj_align(bar_label[i], LV_ALIGN_TOP_LEFT, cx, cy);

        /* 进度条背景 */
        bar_bg[i] = lv_obj_create(tile);
        lv_obj_remove_style_all(bar_bg[i]);
        lv_obj_set_size(bar_bg[i], GRID_BAR_W, GRID_BAR_H);
        lv_obj_set_style_radius(bar_bg[i], 2, 0);
        lv_obj_set_style_bg_color(bar_bg[i], lv_color_hex(0x2a2a3e), 0);
        lv_obj_set_style_bg_opa(bar_bg[i], LV_OPA_COVER, 0);
        lv_obj_align(bar_bg[i], LV_ALIGN_TOP_LEFT, cx + GRID_NAME_W + 2, cy + 5);

        /* 进度条填充 */
        bar_fill[i] = lv_obj_create(bar_bg[i]);
        lv_obj_remove_style_all(bar_fill[i]);
        lv_obj_set_size(bar_fill[i], 0, GRID_BAR_H);
        lv_obj_set_style_radius(bar_fill[i], 2, 0);
        lv_obj_set_style_bg_color(bar_fill[i], ui_har_colors[i], 0);
        lv_obj_set_style_bg_opa(bar_fill[i], LV_OPA_COVER, 0);
        lv_obj_align(bar_fill[i], LV_ALIGN_LEFT_MID, 0, 0);

        /* 样本数 */
        bar_count[i] = lv_label_create(tile);
        lv_obj_set_style_text_font(bar_count[i], &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(bar_count[i], lv_color_hex(0x78909C), 0);
        lv_label_set_text(bar_count[i], "0");
        lv_obj_align(bar_count[i], LV_ALIGN_TOP_LEFT,
                     cx + GRID_NAME_W + 2 + GRID_BAR_W + 4, cy);
    }

    /* ---- y=152: 更新次数 ---- */
    lbl_updates = lv_label_create(tile);
    lv_obj_set_style_text_font(lbl_updates, CN_FONT, 0);
    lv_obj_set_style_text_color(lbl_updates, lv_color_hex(0xB0BEC5), 0);
    lv_label_set_text(lbl_updates,
        "\xE6\x9B\xB4\xE6\x96\xB0: 0\xE6\xAC\xA1"); /* 更新: 0次 */
    lv_obj_align(lbl_updates, LV_ALIGN_TOP_MID, 0, 152);

    /* ---- y=172: 校准按钮（100×22，圆角药丸）---- */
    btn_calibrate = lv_btn_create(tile);
    lv_obj_set_size(btn_calibrate, 100, 22);
    lv_obj_align(btn_calibrate, LV_ALIGN_TOP_MID, 0, 172);
    lv_obj_set_style_bg_color(btn_calibrate, lv_color_hex(0x2196F3), 0);
    lv_obj_set_style_radius(btn_calibrate, 11, 0);  /* 药丸圆角 */
    lv_obj_add_event_cb(btn_calibrate, calibrate_btn_cb, LV_EVENT_CLICKED, NULL);

    lbl_btn = lv_label_create(btn_calibrate);
    lv_obj_set_style_text_font(lbl_btn, CN_FONT, 0);
    lv_obj_set_style_text_color(lbl_btn, lv_color_white(), 0);
    lv_label_set_text(lbl_btn,
        "\xE4\xB8\xBB\xE5\x8A\xA8\xE6\xA0\xA1\xE5\x87\x86"); /* 主动校准 */
    lv_obj_center(lbl_btn);
}

/* ============================================================
 * 更新第5页数据
 * ============================================================ */
void ui_adaptive_update(adaptive_state_t state,
                        float confidence,
                        float margin,
                        const int *support_counts,
                        int updates,
                        int last_update_min) {
    if (!lbl_state) return;

    /* 状态文本 + 颜色 + 状态灯 */
    lv_label_set_text(lbl_state, state_str(state));
    lv_obj_set_style_text_color(lbl_state, state_color(state), 0);
    lv_obj_set_style_bg_color(lbl_state_dot, state_color(state), 0);

    /* 置信度 */
    if (confidence >= 0 && confidence <= 1.0f) {
        char buf[24];
        lv_snprintf(buf, sizeof(buf),
            "\xE7\xBD\xAE\xE4\xBF\xA1\xE5\xBA\xA6: %d%%", (int)(confidence * 100));
        lv_label_set_text(lbl_conf, buf);
    } else {
        lv_label_set_text(lbl_conf, "\xE7\xBD\xAE\xE4\xBF\xA1\xE5\xBA\xA6: --");
    }

    /* 裕度 */
    if (margin >= 0 && margin <= 1.0f) {
        char buf[24];
        /* "裕度: 0.35" — lv_snprintf 不支持 %f，用整数运算 */
        int margin_int = (int)(margin * 100);
        lv_snprintf(buf, sizeof(buf),
            "\xE8\xA3\x95\xE5\xBA\xA6: %d.%02d",
            margin_int / 100, margin_int % 100);
        lv_label_set_text(lbl_margin, buf);
    } else {
        lv_label_set_text(lbl_margin, "\xE8\xA3\x95\xE5\xBA\xA6: --");
    }

    /* 7类样本进度条 */
    if (support_counts) {
        for (int i = 0; i < 7; i++) {
            int cnt = support_counts[i];
            char buf[12];
            lv_snprintf(buf, sizeof(buf), "%d", cnt);
            lv_label_set_text(bar_count[i], buf);

            int pct = (cnt < 0) ? 0 : (cnt > 10) ? 10 : cnt;
            uint16_t w = (uint16_t)(GRID_BAR_W * pct / 10);
            lv_obj_set_width(bar_fill[i], w);
        }
    }

    /* 更新次数 */
    {
        char buf[48];
        if (updates > 0 && last_update_min >= 0) {
            /* "更新: 3次 (12m前)" */
            lv_snprintf(buf, sizeof(buf),
                "\xE6\x9B\xB4\xE6\x96\xB0: %d\xE6\xAC\xA1 (%dm\xE5\x89\x8D)",
                updates, last_update_min);
        } else if (updates > 0) {
            /* "更新: 3次" */
            lv_snprintf(buf, sizeof(buf),
                "\xE6\x9B\xB4\xE6\x96\xB0: %d\xE6\xAC\xA1", updates);
        } else {
            lv_snprintf(buf, sizeof(buf),
                "\xE6\x9B\xB4\xE6\x96\xB0: 0\xE6\xAC\xA1");
        }
        lv_label_set_text(lbl_updates, buf);
    }
}
