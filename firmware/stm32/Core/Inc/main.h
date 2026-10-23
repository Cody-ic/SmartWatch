/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32u5xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
void IWDG_Refresh(void);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define BTN0_POWER_Pin GPIO_PIN_13  /* [2026-07-07]: 换 XC6194A 后 PC13→SW, 方案A/B MCU 均不再读取 */
#define BTN0_POWER_GPIO_Port GPIOC
#define LSM6DSOX_INT1_Pin GPIO_PIN_4   /* [2026-07-08]: PCB 重做 PA0→PB4 */
#define LSM6DSOX_INT1_GPIO_Port GPIOB
#define LSM6DSOX_INT1_EXTI_IRQn EXTI4_IRQn
#define GC9A01_CS_Pin GPIO_PIN_7   /* [2026-07-10 Fix]: PCB重做 PA4→PC7 (手改, 勿重生成 — .ioc 已发散) */
#define GC9A01_CS_GPIO_Port GPIOC
#define BAT_ADC_EN_Pin GPIO_PIN_4   /* [2026-07-10 Fix]: PCB重做 PB0→PA4 (Q3栅极, P沟道FET, 低=导通分压支路)。手改勿重生成 */
#define BAT_ADC_EN_GPIO_Port GPIOA
#define GC9A01_DC_Pin GPIO_PIN_6   /* [2026-07-10 Fix]: PA8→PC6 */
#define GC9A01_DC_GPIO_Port GPIOC
#define GC9A01_RST_Pin GPIO_PIN_8   /* [2026-07-10 Fix]: PA9→PC8 */
#define GC9A01_RST_GPIO_Port GPIOC
/* [2026-07-10]: PWR_CTL(PA10) 停用 — 新板 XC6194A 硬件管电源(见 power.c #if 0);
 *   PA10 将在 Task#3 改作 BLE USART1_RX。用 #if 0 停用保留, 手改勿重生成。 */
#if 0
#define PWR_CTL_Pin GPIO_PIN_10
#define PWR_CTL_GPIO_Port GPIOA
#endif
#define BTN1_PREV_Pin GPIO_PIN_0   /* [2026-07-08]: PCB 重做 PB4→PC0 (SW4) */
#define BTN1_PREV_GPIO_Port GPIOC
#define BTN2_NEXT_Pin GPIO_PIN_1   /* [2026-07-08]: PCB 重做 PB5→PC1 (SW6 经 D3) */
#define BTN2_NEXT_GPIO_Port GPIOC

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
