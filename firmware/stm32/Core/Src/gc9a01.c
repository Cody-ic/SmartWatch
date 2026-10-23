/**
 * @file    gc9a01.c
 * @brief   GC9A01 240×240 圆形 IPS LCD 驱动（STM32 HAL SPI）
 *
 * SPI 时序：Mode 0 (CPOL=0, CPHA=0)，MSB first
 * 最大 SPI 时钟 ≈ 30MHz（GC9A01 spec），建议 ≤ 40MHz
 *
 * 引脚分配（可在此文件顶部宏修改）：
 *   SPI2_SCK  = PB13 (AF5)
 *   SPI2_MOSI = PB15 (AF5)
 *   CS  = PC7  (GPIO Output, 默认高)  [2026-07-10] PA4→PC7
 *   DC  = PC6  (GPIO Output, 命令=低, 数据=高)  PA8→PC6
 *   RST = PC8  (GPIO Output, 低=复位)  PA9→PC8 (模块无背光)
 */
#include "gc9a01.h"

/* ========== 引脚映射（按你的硬件改这里）========== */
/* [2026-07-10 Fix]: PCB重做 显示控制脚 GPIOA→GPIOC (手改, 勿重生成 — .ioc 已发散)
 *   CS: PA4→PC7, DC: PA8→PC6, RST: PA9→PC8。四源核实(网表+.epro2+飞针+DS13737)。 */
#define GC9A01_CS_PORT   GPIOC
#define GC9A01_CS_PIN    GPIO_PIN_7

#define GC9A01_DC_PORT   GPIOC
#define GC9A01_DC_PIN    GPIO_PIN_6

/* [2026-07-03]: 模块无背光引脚, RST 为硬件复位
 *   背光由 VCC 直连 (常亮), GC9A01_SetBacklight() 改为空函数 */
#define GC9A01_RST_PORT  GPIOC
#define GC9A01_RST_PIN   GPIO_PIN_8

/* SPI 句柄——由 CubeMX 生成的 spi.c 中定义 */
extern SPI_HandleTypeDef hspi2;

/* SPI 超时（ms）：有限超时防止 SPI 卡死时永久挂起 */
#define DISPLAY_SPI_TIMEOUT 50

/* ========== 底层 SPI 收发 ========== */

static inline void CS_LOW(void)  { HAL_GPIO_WritePin(GC9A01_CS_PORT, GC9A01_CS_PIN, GPIO_PIN_RESET); }
static inline void CS_HIGH(void) { HAL_GPIO_WritePin(GC9A01_CS_PORT, GC9A01_CS_PIN, GPIO_PIN_SET); }
static inline void DC_LOW(void)  { HAL_GPIO_WritePin(GC9A01_DC_PORT, GC9A01_DC_PIN, GPIO_PIN_RESET); }
static inline void DC_HIGH(void) { HAL_GPIO_WritePin(GC9A01_DC_PORT, GC9A01_DC_PIN, GPIO_PIN_SET); }

static void spi_write(const uint8_t *data, uint16_t len)
{
    /* [2026-07-04 Fix]: 检查 HAL_SPI_Transmit 返回值 — 原丢弃返回值, SPI 总线故障时无感知。
     *   调用链: write_cmd/write_data → spi_write → HAL_SPI_Transmit。
     *   超时时长 DISPLAY_SPI_TIMEOUT 足以完成传输, 错误返回时由上层决定是否重试。 */
    if (HAL_SPI_Transmit(&hspi2, (uint8_t *)data, len, DISPLAY_SPI_TIMEOUT) != HAL_OK) {
        /* SPI 传输失败 — 硬件故障 (总线短路/显示模块未连接), 静默丢弃 */
    }
}

static void write_cmd(uint8_t cmd)
{
    CS_LOW();
    DC_LOW();
    spi_write(&cmd, 1);
    CS_HIGH();
}

static void write_data(uint8_t data)
{
    CS_LOW();
    DC_HIGH();
    spi_write(&data, 1);
    CS_HIGH();
}

static void write_data16(uint16_t data)
{
    uint8_t buf[2] = { (uint8_t)(data >> 8), (uint8_t)(data & 0xFF) };
    CS_LOW();
    DC_HIGH();
    spi_write(buf, 2);
    CS_HIGH();
}

/* ========== 初始化序列 ========== */

void GC9A01_Init(void)
{
    /* [2026-07-03]: 硬件复位 — PA9 拉低 50ms → 拉高 120ms
     *   来源: Arduino_GFX/TFT_eSPI 社区标准 (GC9A01 datasheet: RESX min 10us)
     *   120ms 等待内部 LDO + 振荡器稳定, 多家模组厂商推荐 */
    HAL_GPIO_WritePin(GC9A01_RST_PORT, GC9A01_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(50);
    HAL_GPIO_WritePin(GC9A01_RST_PORT, GC9A01_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(120);

    /* [2026-07-05 Fix]: 替换为 Adafruit/STM32_GC9A01/TFT_eSPI 标准序列
     *   MADCTL 0x00(RGB)→0x48(BGR+MX), 加 INVON+gamma+power+timing, 去合宙特化寄存器 */

    /* Inter Register Enable */
    write_cmd(0xEF);
    write_cmd(0xEB); write_data(0x14);
    write_cmd(0xFE);
    write_cmd(0xEF);
    write_cmd(0xEB); write_data(0x14);

    write_cmd(0x84); write_data(0x40);
    write_cmd(0x85); write_data(0xFF);
    write_cmd(0x86); write_data(0xFF);
    write_cmd(0x87); write_data(0xFF);
    write_cmd(0x88); write_data(0x0A);
    write_cmd(0x89); write_data(0x21);
    write_cmd(0x8A); write_data(0x00);
    write_cmd(0x8B); write_data(0x80);
    write_cmd(0x8C); write_data(0x01);
    write_cmd(0x8D); write_data(0x01);
    write_cmd(0x8E); write_data(0xFF);
    write_cmd(0x8F); write_data(0xFF);

    write_cmd(0xB6); write_data(0x00); write_data(0x00);

    /* MADCTL: MX=1 BGR=1 (三源一致 Adafruit/STM32/TFT_eSPI) */
    write_cmd(0x36); write_data(0x48);
    write_cmd(0x3A); write_data(0x05);

    write_cmd(0x90); write_data(0x08); write_data(0x08);
                     write_data(0x08); write_data(0x08);
    write_cmd(0xBD); write_data(0x06);
    write_cmd(0xBC); write_data(0x00);
    write_cmd(0xFF); write_data(0x60); write_data(0x01); write_data(0x04);
    write_cmd(0xC3); write_data(0x13);
    write_cmd(0xC4); write_data(0x13);
    write_cmd(0xC9); write_data(0x22);
    write_cmd(0xBE); write_data(0x11);
    write_cmd(0xE1); write_data(0x10); write_data(0x0E);
    write_cmd(0xDF); write_data(0x21); write_data(0x0C); write_data(0x02);

    write_cmd(0xF0); write_data(0x45); write_data(0x09); write_data(0x08);
                     write_data(0x08); write_data(0x26); write_data(0x2A);
    write_cmd(0xF1); write_data(0x43); write_data(0x70); write_data(0x72);
                     write_data(0x36); write_data(0x37); write_data(0x6F);
    write_cmd(0xF2); write_data(0x45); write_data(0x09); write_data(0x08);
                     write_data(0x08); write_data(0x26); write_data(0x2A);
    write_cmd(0xF3); write_data(0x43); write_data(0x70); write_data(0x72);
                     write_data(0x36); write_data(0x37); write_data(0x6F);

    write_cmd(0xED); write_data(0x1B); write_data(0x0B);
    write_cmd(0xAE); write_data(0x77);
    write_cmd(0xCD); write_data(0x63);
    write_cmd(0xE8); write_data(0x34);

    write_cmd(0x62); write_data(0x18); write_data(0x0D); write_data(0x71);
                     write_data(0xED); write_data(0x70); write_data(0x70);
                     write_data(0x18); write_data(0x0F); write_data(0x71);
                     write_data(0xEF); write_data(0x70); write_data(0x70);
    write_cmd(0x63); write_data(0x18); write_data(0x11); write_data(0x71);
                     write_data(0xF1); write_data(0x70); write_data(0x70);
                     write_data(0x18); write_data(0x13); write_data(0x71);
                     write_data(0xF3); write_data(0x70); write_data(0x70);
    write_cmd(0x64); write_data(0x28); write_data(0x29); write_data(0xF1);
                     write_data(0x01); write_data(0xF1); write_data(0x00);
                     write_data(0x07);
    write_cmd(0x66); write_data(0x3C); write_data(0x00); write_data(0xCD);
                     write_data(0x67); write_data(0x45); write_data(0x45);
                     write_data(0x10); write_data(0x00); write_data(0x00);
                     write_data(0x00);
    write_cmd(0x67); write_data(0x00); write_data(0x3C); write_data(0x00);
                     write_data(0x00); write_data(0x00); write_data(0x01);
                     write_data(0x54); write_data(0x10); write_data(0x32);
                     write_data(0x98);
    write_cmd(0x74); write_data(0x10); write_data(0x85); write_data(0x80);
                     write_data(0x00); write_data(0x00); write_data(0x4E);
                     write_data(0x00);
    write_cmd(0x98); write_data(0x3E); write_data(0x07);

    /* Default display area: 0~239 */
    write_cmd(0x2A);
    write_data16(0x0000);
    write_data16(0x00EF);   /* 239 */

    write_cmd(0x2B);
    write_data16(0x0000);
    write_data16(0x00EF);   /* 239 */

    /* TEON + INVON (三源一致, 原缺失 INVON 致对比度异常) */
    write_cmd(0x35);  /* TEON, no data */
    write_cmd(0x21);  /* INVON */

    /* Sleep Out */
    write_cmd(0x11);
    HAL_Delay(120);

    /* Display On */
    write_cmd(0x29);
    HAL_Delay(20);

}

/* ========== 背光 ========== */

void GC9A01_SetBacklight(uint8_t on)
{
    /* [2026-07-03]: 此模块无独立背光引脚, 背光由 VCC 直连 (常亮).
     *   保留此函数接口避免编译错误, 实际为空操作. */
    (void)on;
}

/* ========== 绘图 ========== */

void GC9A01_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    write_cmd(0x2A);
    write_data16(x0);
    write_data16(x1);

    write_cmd(0x2B);
    write_data16(y0);
    write_data16(y1);

    write_cmd(0x2C);  /* Memory Write */
}

void GC9A01_WritePixels(const uint8_t *data, uint32_t len)
{
    CS_LOW();
    DC_HIGH();
    HAL_SPI_Transmit(&hspi2, (uint8_t *)data, len, DISPLAY_SPI_TIMEOUT);
    CS_HIGH();
}

void GC9A01_FillScreen(gc9a01_color_t color)
{
    GC9A01_SetWindow(0, 0, GC9A01_WIDTH - 1, GC9A01_HEIGHT - 1);

    uint8_t hi = (uint8_t)(color >> 8);
    uint8_t lo = (uint8_t)(color & 0xFF);

    /* [2026-07-04 Fix]: buf 从栈局部变量改为 static — 480B 在 Sensor_Task
     *   4KB 栈上虽能容纳但余量紧张 (LVGL + HAR 推理 + BLE 帧均有栈开销)。
     *   所有对 FillScreen 的调用来自 Sensor_Task (单线程), static 安全。 */
    static uint8_t buf[240 * 2];
    for (int i = 0; i < 240; i++) {
        buf[i * 2]     = hi;
        buf[i * 2 + 1] = lo;
    }

    CS_LOW();
    DC_HIGH();
    for (int row = 0; row < GC9A01_HEIGHT; row++) {
        HAL_SPI_Transmit(&hspi2, buf, sizeof(buf), DISPLAY_SPI_TIMEOUT);
    }
    CS_HIGH();
}

void GC9A01_DrawBitmap(uint16_t x, uint16_t y,
                       uint16_t w, uint16_t h,
                       const uint8_t *data)
{
    GC9A01_SetWindow(x, y, x + w - 1, y + h - 1);
    GC9A01_WritePixels(data, (uint32_t)w * h * 2);
}
