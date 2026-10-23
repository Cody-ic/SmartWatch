#pragma once
#include <stdint.h>
typedef struct { void *Instance; } UART_HandleTypeDef;
typedef int HAL_StatusTypeDef;
#define USART1 ((void*)1)
#define HAL_OK 0
uint32_t HAL_GetTick(void);
int HAL_UART_Receive_IT(UART_HandleTypeDef*, uint8_t*, uint16_t);
int HAL_UART_Transmit(UART_HandleTypeDef*, uint8_t*, uint16_t, uint32_t);
static inline uint32_t __get_PRIMASK(void) { return 0; }
static inline void __disable_irq(void) {}
static inline void __set_PRIMASK(uint32_t value) { (void)value; }
