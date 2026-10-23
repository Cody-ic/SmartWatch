/**
 * @file    ui.c
 * @brief   AI Watch UI - main manager (tileview + page navigation)
 *
 * Uses LVGL 8.x tileview for swipe navigation between 4 pages.
 * Each page is implemented in its own ui_<name>.c module.
 */
#include "ui.h"
#include <stdio.h>

/* ============================================================
 * Shared with page modules
 * ============================================================ */
lv_obj_t *ui_tileview;     /* public so pages can add themselves */

/* Color palette per HAR class (shared by HAR page + indicator) */
const lv_color_t ui_har_colors[HAR_COUNT] = {
    [HAR_LYING]      = LV_COLOR_MAKE(0x9C, 0x27, 0xB0),
    [HAR_SITTING]    = LV_COLOR_MAKE(0x3F, 0x51, 0xB5),
    [HAR_STANDING]   = LV_COLOR_MAKE(0x21, 0x96, 0xF3),
    [HAR_WALKING]    = LV_COLOR_MAKE(0x4C, 0xAF, 0x50),
    [HAR_JOGGING]    = LV_COLOR_MAKE(0xFF, 0x98, 0x00),
    [HAR_DOWNSTAIRS] = LV_COLOR_MAKE(0x79, 0x55, 0x48),
    [HAR_CYCLING]    = LV_COLOR_MAKE(0x00, 0xBC, 0xD4),
};

/* ============================================================
 * Page module declarations (defined in ui_<name>.c)
 * ============================================================ */
void ui_clock_create(lv_obj_t *parent);
void ui_har_create(lv_obj_t *parent);
void ui_hr_create(lv_obj_t *parent);
void ui_steps_create(lv_obj_t *parent);
void ui_adaptive_create(lv_obj_t *parent);   /* 5th page */
void ui_spo2_create(lv_obj_t *parent);       /* [2026-07-05]: 6th page — SpO2 */
void ui_sleep_create(lv_obj_t *parent);      /* [2026-07-05]: 7th page — Sleep */

/* ============================================================
 * Page dots indicator (top of every page)
 * ============================================================ */
#define UI_N_PAGES 7  /* [2026-07-05]: 5→6→7, 加 SpO2 + Sleep 页面 */
static lv_obj_t *dots[UI_N_PAGES];     /* 5 个 dot 对象，全局共享，按需高亮 */
static lv_obj_t *tiles[UI_N_PAGES];    /* 存 tile 指针，翻页时直接用 */
static uint8_t cached_page = 0;        /* cached page index */

static void dots_update(uint8_t active)
{
    for (uint8_t i = 0; i < UI_N_PAGES; i++) {
        lv_obj_set_size(dots[i], i == active ? 16 : 5, 5);
        lv_obj_set_style_bg_color(dots[i],
            lv_color_hex(i == active ? 0x4fc3f7 : 0x444444), 0);
    }
}

static void dots_create(void)
{
    /* 在 tileview 顶层创建一个固定不随页面滚动的 dot 指示器 */
    lv_obj_t *cont = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(cont, 4, 0);

    for (uint8_t i = 0; i < UI_N_PAGES; i++) {
        lv_obj_t *d = lv_obj_create(cont);
        lv_obj_remove_style_all(d);
        lv_obj_set_size(d, i == 0 ? 16 : 5, 5);  /* page 0 active by default */
        lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(d, lv_color_hex(i == 0 ? 0x4fc3f7 : 0x444444), 0);
        dots[i] = d;
    }
}

/* Called when tileview scrolls - updates dots and cached page */
static void tileview_scroll_cb(lv_event_t *e)
{
    (void)e;
    lv_coord_t x = lv_obj_get_scroll_x(ui_tileview);
    uint8_t page = (uint8_t)(x / 240);
    if (page < UI_N_PAGES && page != cached_page) {
        cached_page = page;
        dots_update(cached_page);
    }
}

/* ============================================================
 * Public API
 * ============================================================ */
void ui_init(void)
{
    /* Create root tileview - 5 tiles horizontally */
    ui_tileview = lv_tileview_create(lv_scr_act());
    lv_obj_set_style_bg_color(ui_tileview, lv_color_hex(0x060614), 0);
    lv_obj_set_size(ui_tileview, 240, 240);
    lv_obj_clear_flag(ui_tileview, LV_OBJ_FLAG_SCROLL_ELASTIC);

    /* 全局 dot 指示器（固定在 tileview 上层，不随页面滚动） */
    dots_create();

    /* Page 0 - Clock */
    tiles[0] = lv_tileview_add_tile(ui_tileview, 0, 0, LV_DIR_RIGHT);
    ui_clock_create(tiles[0]);

    /* Page 1 - HAR */
    tiles[1] = lv_tileview_add_tile(ui_tileview, 1, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    ui_har_create(tiles[1]);

    /* Page 2 - Heart rate */
    tiles[2] = lv_tileview_add_tile(ui_tileview, 2, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    ui_hr_create(tiles[2]);

    /* Page 3 - Steps */
    tiles[3] = lv_tileview_add_tile(ui_tileview, 3, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    ui_steps_create(tiles[3]);

    /* Page 4 - SpO2 [2026-07-05] */
    tiles[4] = lv_tileview_add_tile(ui_tileview, 4, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    ui_spo2_create(tiles[4]);

    /* Page 5 - Sleep [2026-07-05]: 睡眠分期 + 呼吸暂停初筛 */
    tiles[5] = lv_tileview_add_tile(ui_tileview, 5, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    ui_sleep_create(tiles[5]);

    /* Page 6 - Adaptive (AI 原型网络自适应) [2026-07-05]: 移至末页 */
    tiles[6] = lv_tileview_add_tile(ui_tileview, 6, 0, LV_DIR_LEFT);
    ui_adaptive_create(tiles[6]);

    lv_obj_add_event_cb(ui_tileview, tileview_scroll_cb, LV_EVENT_SCROLL, NULL);

    /* 初始化告警 Toast 覆盖层（跌倒/霸凌/心率异常） */
    ui_alert_init();
}

void ui_page_next(void)
{
    /* [2026-07-04 Fix]: 移除 cached_page 预修改。原逻辑在 lv_obj_set_tile() 前
     *   写 cached_page, 导致 scroll 动画完成后 tileview_scroll_cb 中
     *   page != cached_page 恒为 false → dots_update() 永不触发 → 按键翻页后
     *   dot 指示器卡在上一个位置。现仅调 LVGL, cached_page+dots 由回调更新。 */
    uint8_t p = cached_page;
    if (p < UI_N_PAGES - 1) {
        lv_obj_set_tile(ui_tileview, tiles[p + 1], LV_ANIM_ON);
    }
}

void ui_page_prev(void)
{
    uint8_t p = cached_page;
    if (p > 0) {
        lv_obj_set_tile(ui_tileview, tiles[p - 1], LV_ANIM_ON);
    }
}

uint8_t ui_get_current_page(void)
{
    lv_coord_t x = lv_obj_get_scroll_x(ui_tileview);
    uint8_t page = (uint8_t)(x / 240);
    if (page >= UI_N_PAGES) page = UI_N_PAGES - 1;
    return page;
}
