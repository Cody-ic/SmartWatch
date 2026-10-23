#ifndef __MAX30102_H
#define __MAX30102_H

#include "stm32u5xx_hal.h"

uint8_t MAX30102_Init(I2C_HandleTypeDef *handle);

uint8_t MAX30102_GetFifoBatch(uint32_t *red_buf, uint32_t *ir_buf, uint8_t max_samples);

/**
 * [2026-07-04]: 动态采样率切换 — MLC 静止时降采样省电。
 *   hi_rate=1 → 100Hz (0x27), hi_rate=0 → 50Hz (0x07).
 *   写 SpO2_CONFIG (0x0A), 保留 4096nA/411μs 配置不变。
 *   返回 0=成功.
 */
uint8_t MAX30102_SetSampleRate(uint8_t hi_rate);

#if TEST2
void MAX30102_GetSingleSample(uint32_t *red, uint32_t *ir);
#endif

#endif






