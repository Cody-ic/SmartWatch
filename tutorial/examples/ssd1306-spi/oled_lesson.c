/* Original lesson implementation. Controller command meanings: SSD1306 datasheet.
 * Assumptions: hspi1 ready, 8-bit/MSB-first/mode 0, software NSS, <=1 MHz;
 * PB0=CS, PB1=D/C, PB2=RES, all ordinary push-pull outputs initially high.
 * A ready-made 3.3 V-compatible 128x64 module must be strapped for four-wire SPI.
 * This uses blocking HAL calls from main, never from interrupt callbacks.
 */
#include "oled_lesson.h"

static uint8_t frame[8][128];

static HAL_StatusTypeDef write_bytes(uint8_t is_data,
                                    const uint8_t *data, uint16_t size)
{
    if (data == 0 || size == 0) {
        return HAL_ERROR;
    }
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1,
                      is_data ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
    HAL_StatusTypeDef status = HAL_SPI_Transmit(&hspi1, data, size, 100);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
    return status;
}

void OLED_LessonClear(void)
{
    for (uint16_t page = 0; page < 8; page++) {
        for (uint16_t x = 0; x < 128; x++) {
            frame[page][x] = 0;
        }
    }
}

void OLED_LessonPixel(uint16_t x, uint16_t y, uint8_t on)
{
    if (x >= 128 || y >= 64) {
        return;
    }
    uint8_t mask = (uint8_t)(1U << (y % 8U));
    if (on != 0) {
        frame[y / 8U][x] |= mask;
    } else {
        frame[y / 8U][x] &= (uint8_t)~mask;
    }
}

HAL_StatusTypeDef OLED_LessonFlush(void)
{
    for (uint8_t page = 0; page < 8; page++) {
        uint8_t address[] = {(uint8_t)(0xB0U | page), 0x00, 0x10};
        HAL_StatusTypeDef status = write_bytes(0, address, sizeof(address));
        if (status != HAL_OK) {
            return status;
        }
        status = write_bytes(1, frame[page], 128);
        if (status != HAL_OK) {
            return status;
        }
    }
    return HAL_OK;
}

HAL_StatusTypeDef OLED_LessonInit(void)
{
    /* Example timings, not universal panel timing requirements. */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2,
                      GPIO_PIN_SET);
    HAL_Delay(20);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_SET);
    HAL_Delay(100);

    const uint8_t setup[] = {
        0xAE,             /* Display off during setup. */
        0xD5, 0x80,       /* Display clock, independent of the SPI SCK. */
        0xA8, 0x3F,       /* 64 multiplexed rows. */
        0xD3, 0x00, 0x40, /* Offset zero, start line zero. */
        0x8D, 0x14,       /* Enable the internal charge pump. */
        0x20, 0x02,       /* Page addressing mode. */
        0xA1, 0xC8,       /* Segment and COM scan directions. */
        0xDA, 0x12,       /* COM pin configuration for this panel assumption. */
        0x81, 0x7F,       /* Contrast. */
        0xD9, 0xF1, 0xDB, 0x20,
        0xA4, 0xA6, 0x2E  /* RAM display, normal polarity, scrolling off. */
    };
    HAL_StatusTypeDef status = write_bytes(0, setup, sizeof(setup));
    if (status != HAL_OK) {
        return status;
    }
    OLED_LessonClear();
    status = OLED_LessonFlush();
    if (status != HAL_OK) {
        return status;
    }
    const uint8_t display_on = 0xAF;
    return write_bytes(0, &display_on, 1);
}

HAL_StatusTypeDef OLED_LessonBitmap(uint16_t x, uint16_t y,
                                  uint16_t width, uint16_t height,
                                  const uint8_t *data, uint16_t size)
{
    /* Page-major bytes; each byte is a vertical column of up to 8 pixels.
     * Bit 0 is at the top. Partial final pages are allowed; no UTF-8 prefixes.
     * Off-screen target pixels are clipped, rather than wrapping coordinates.
     */
    if (data == 0 || width == 0 || width > 128 || height == 0 || height > 64
        || x >= 128 || y >= 64) {
        return HAL_ERROR;
    }
    uint16_t required = (uint16_t)(width * ((height + 7U) / 8U));
    if (size != required) {
        return HAL_ERROR;
    }
    for (uint16_t dy = 0; dy < height; dy++) {
        for (uint16_t dx = 0; dx < width; dx++) {
            uint8_t byte = data[(dy / 8U) * width + dx];
            OLED_LessonPixel((uint16_t)(x + dx), (uint16_t)(y + dy),
                             (uint8_t)((byte >> (dy % 8U)) & 1U));
        }
    }
    return HAL_OK;
}
