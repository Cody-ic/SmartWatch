/**
 * @file img_heart.c
 * @brief 16x14 心形位图 (1bpp Alpha), 用于心率页面替代缺失的 U+2665 字形
 *
 * [2026-07-03]: 创建此文件因 lv_font_montserrat_28 不包含 ♥ (U+2665),
 *               FontAwesome fa-heart (U+F004) 亦未编译进字体。
 *               长期方案: 用 lvgl 字体工具重新生成字体, 加入 U+2665 或 U+F004。
 */

#include "../lvgl/lvgl.h"

#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

#ifndef LV_ATTRIBUTE_IMG_HEART
#define LV_ATTRIBUTE_IMG_HEART
#endif

/* 16x14 像素, LV_IMG_CF_ALPHA_1BIT: 每字节 8 像素 MSB first, 每行 2 字节, 共 28 字节 */
const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_IMG_HEART uint8_t img_heart_map[] = {
    /* Row  0:  ...XX... ...XX...  */ 0x18, 0x18,
    /* Row  1:  ..XXXX.. ..XXXX..  */ 0x3c, 0x3c,
    /* Row  2:  .XXXXXX. .XXXXXX.  */ 0x7e, 0x7e,
    /* Row  3:  XXXXXXXXXXXXXXXXX  */ 0xff, 0xff,
    /* Row  4:  XXXXXXXXXXXXXXXXX  */ 0xff, 0xff,
    /* Row  5:  XXXXXXXXXXXXXXXXX  */ 0xff, 0xff,
    /* Row  6:  XXXXXXXXXXXXXXXX.  */ 0xff, 0xfe,
    /* Row  7:  XXXXXXXXXXXXXX..   */ 0xff, 0xfc,
    /* Row  8:  .XXXXXXXXXXXX...   */ 0x7f, 0xf8,
    /* Row  9:  ..XXXXXXXXXX....   */ 0x3f, 0xf0,
    /* Row 10:  ...XXXXXXXX.....   */ 0x1f, 0xe0,
    /* Row 11:  ....XXXXXX......   */ 0x0f, 0xc0,
    /* Row 12:  .....XXXX.......   */ 0x07, 0x80,
    /* Row 13:  ......XX........   */ 0x03, 0x00,
};

const lv_img_dsc_t img_heart = {
    .header.always_zero = 0,
    .header.w = 16,
    .header.h = 14,
    .data_size = 28,
    .header.cf = LV_IMG_CF_ALPHA_1BIT,
    .data = img_heart_map,
};
