/* Standalone teaching driver: SSD1306, 128x64, four-wire SPI, internal charge pump.
 * Not part of the SmartWatch firmware build. See the SPI lesson for wiring.
 */
#ifndef OLED_LESSON_H
#define OLED_LESSON_H

#include "spi.h"

HAL_StatusTypeDef OLED_LessonInit(void);
void OLED_LessonClear(void);
void OLED_LessonPixel(uint16_t x, uint16_t y, uint8_t on);
HAL_StatusTypeDef OLED_LessonFlush(void);
HAL_StatusTypeDef OLED_LessonBitmap(uint16_t x, uint16_t y,
                                  uint16_t width, uint16_t height,
                                  const uint8_t *data, uint16_t size);

#endif
