/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "gpio.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/** Configure pins
*/
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  /* [2026-07-10 Fix]: 显示 CS/DC/RST 在 GPIOC(PC7/6/8). 手改勿重生成 */
  HAL_GPIO_WritePin(GPIOC, GC9A01_CS_Pin|GC9A01_DC_Pin|GC9A01_RST_Pin, GPIO_PIN_RESET);
  /* [2026-07-10]: PWR_CTL(PA10) WritePin 停用(#if 0) — 新板 XC6194A 硬件管电源, PA10 改作 BLE USART1_RX。 */
#if 0
  HAL_GPIO_WritePin(PWR_CTL_GPIO_Port, PWR_CTL_Pin, GPIO_PIN_RESET);
#endif

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BAT_ADC_EN_GPIO_Port, BAT_ADC_EN_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : BTN0_POWER_Pin */
  GPIO_InitStruct.Pin = BTN0_POWER_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  /* [2026-07-04 Fix]: NOPULL→PULLUP。与 PB4/PB5 同类问题:
   *   .ioc 配置 PULLUP, CubeMX 错误生成 NOPULL。PCB 设计依赖片内上拉。 */
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(BTN0_POWER_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LSM6DSOX_INT1_Pin */
  GPIO_InitStruct.Pin = LSM6DSOX_INT1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(LSM6DSOX_INT1_GPIO_Port, &GPIO_InitStruct);

  /* [2026-07-10 Fix]: 显示 CS/DC/RST → GPIOC(PC7/6/8), 从 PWR_CTL 组拆出. 手改勿重生成 */
  GPIO_InitStruct.Pin = GC9A01_CS_Pin|GC9A01_DC_Pin|GC9A01_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* [2026-07-10]: PWR_CTL(PA10) GPIO 初始化停用(#if 0) — 见 power.c。PA10 留给 BLE USART1_RX。 */
#if 0
  /*Configure GPIO pin : PWR_CTL_Pin (PA10) */
  GPIO_InitStruct.Pin = PWR_CTL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(PWR_CTL_GPIO_Port, &GPIO_InitStruct);
#endif

  /* [2026-07-10 Fix]: BAT_ADC_EN PB0→PA4 (PCB重做, Q3栅极). 手改勿重生成 */
  GPIO_InitStruct.Pin = BAT_ADC_EN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BAT_ADC_EN_GPIO_Port, &GPIO_InitStruct);

  /* [2026-07-08]: 按键从 PB4/PB5→PC0/PC1 (PCB 重做). PC0=SW4, PC1=SW6(经D3) */
  GPIO_InitStruct.Pin = BTN1_PREV_Pin|BTN2_NEXT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(LSM6DSOX_INT1_EXTI_IRQn, 6, 0);  /* [2026-07-08]: PA0→PB4 */
  HAL_NVIC_EnableIRQ(LSM6DSOX_INT1_EXTI_IRQn);

}

/* USER CODE BEGIN 2 */
/* USER CODE END 2 */
