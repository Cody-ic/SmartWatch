#ifndef LSM6DSOX_HOST_HAL_H
#define LSM6DSOX_HOST_HAL_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    void *Instance;
} I2C_HandleTypeDef;

typedef enum {
    HAL_OK = 0,
    HAL_ERROR = 1,
    HAL_BUSY = 2,
    HAL_TIMEOUT = 3
} HAL_StatusTypeDef;

#define I2C_MEMADD_SIZE_8BIT 1U

HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *handle,
    uint16_t device, uint16_t reg, uint16_t address_size,
    uint8_t *data, uint16_t length, uint32_t timeout);
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *handle,
    uint16_t device, uint16_t reg, uint16_t address_size,
    uint8_t *data, uint16_t length, uint32_t timeout);
void HAL_Delay(uint32_t milliseconds);
uint32_t HAL_GetTick(void);

#endif
