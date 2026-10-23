/**
 * @file    gc9a01.h
 * @brief   GC9A01 1.28" 240×240 圆形 IPS LCD 驱动（4线 SPI）
 *
 * 硬件连接（标准 7 脚 SPI 模块）：[2026-07-10] PCB 重做, 引脚更新为新板
 *   SPI2_SCK (PB13) → GC9A01 SCL (时钟)
 *   SPI2_MOSI (PB15) → GC9A01 SDA (数据)
 *   PC7 (GPIO Output) → GC9A01 CS  (片选，低有效)
 *   PC6 (GPIO Output) → GC9A01 DC  (数据/命令，高=数据, 低=命令)
 *   PC8 (GPIO Output) → GC9A01 RST (硬件复位，低有效)
 *   GND → GND, VCC → 3.3V (背光由 VCC 直连, 常亮)
 *
 * 注：模块无背光引脚, RST 为硬件复位。旧板为 SPI1/PA4/PA8/PA9。
 */
#ifndef GC9A01_H
#define GC9A01_H

#include "stm32u5xx_hal.h"
#include <stdint.h>

/* 画布尺寸 */
#define GC9A01_WIDTH   240
#define GC9A01_HEIGHT  240

/* 颜色格式：RGB565 */
typedef uint16_t gc9a01_color_t;

/* 常用颜色 */
#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F

/**
 * @brief  初始化 GC9A01 显示屏
 * @note   调用前必须先初始化 SPI2（HAL_SPI_Init）
 *         以及 DC/CS/BL 对应的 GPIO
 */
void GC9A01_Init(void);

/**
 * @brief  设置背光开关
 * @param  on  1=开 0=关
 */
void GC9A01_SetBacklight(uint8_t on);

/**
 * @brief  全屏填充一种颜色
 */
void GC9A01_FillScreen(gc9a01_color_t color);

/**
 * @brief  向指定矩形区域写入像素数据
 * @param  x,y,w,h  矩形区域
 * @param  data     RGB565 像素数组，长度 = w*h*2 字节
 * @note   LVGL 显示驱动会调这个函数
 */
void GC9A01_DrawBitmap(uint16_t x, uint16_t y,
                       uint16_t w, uint16_t h,
                       const uint8_t *data);

/**
 * @brief  设置绘窗（用于后续连续写入像素）
 */
void GC9A01_SetWindow(uint16_t x0, uint16_t y0,
                      uint16_t x1, uint16_t y1);

/**
 * @brief  发送 RGB565 像素流（需先调 SetWindow）
 * @param  data   像素数据
 * @param  len    字节数（= 像素数 × 2）
 */
void GC9A01_WritePixels(const uint8_t *data, uint32_t len);

#endif /* GC9A01_H */
