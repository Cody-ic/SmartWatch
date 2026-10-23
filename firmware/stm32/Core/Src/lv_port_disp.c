/**
 * @file    lv_port_disp.c
 * @brief   LVGL 8.x 显示驱动接口——对接 GC9A01
 *
 * LVGL 通过 flush_cb 向屏幕输出像素，本文件负责：
 * 1. 创建 draw buffer（LV_MEM_SIZE 中分配）
 * 2. 注册 display driver
 * 3. flush_cb 调用 GC9A01_DrawBitmap
 */
#include "lvgl.h"
#include "gc9a01.h"

/* ========== 显示缓冲区 ==========
 * 240×240 @ 16bpp = 115200 bytes 全帧
 * 为省 RAM 用 10 行缓冲：240×10×2 = 4800 bytes
 * 如果 RAM 充足（768KB SRAM），可以加大到 40 行甚至双缓冲
 */
#define BUF_ROWS  10
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[GC9A01_WIDTH * BUF_ROWS];

/* 如果 RAM 充足，可以启用双缓冲：
 * static lv_color_t buf2[GC9A01_WIDTH * BUF_ROWS];
 */

static lv_disp_drv_t disp_drv;

/* ========== flush 回调 ========== */
static void disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    (void)drv;

    uint16_t w = area->x2 - area->x1 + 1;
    uint16_t h = area->y2 - area->y1 + 1;

    GC9A01_DrawBitmap(area->x1, area->y1, w, h, (const uint8_t *)color_p);

    /* 告诉 LVGL 本次 flush 完成 */
    lv_disp_flush_ready(drv);
}

/* ========== 初始化 ========== */
void lv_port_disp_init(void)
{
    /* 先初始化 GC9A01 硬件 */
    GC9A01_Init();
    GC9A01_FillScreen(COLOR_BLACK);

    /* 初始化 draw buffer */
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, GC9A01_WIDTH * BUF_ROWS);
    /* 双缓冲版：lv_disp_draw_buf_init(&draw_buf, buf1, buf2, GC9A01_WIDTH * BUF_ROWS); */

    /* 注册 display driver */
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = GC9A01_WIDTH;
    disp_drv.ver_res  = GC9A01_HEIGHT;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
}
