#ifndef __MPU6050_H
#define __MPU6050_H

#include "stm32u5xx_hal.h"
extern I2C_HandleTypeDef hi2c1;

uint8_t MPU6050_Init(I2C_HandleTypeDef* handle);

void MPU6050_readbegin(void);

uint8_t MPU6050_isReady(void);

void MPU6050_getData(int16_t* ax, int16_t* ay, int16_t* az, int16_t* temp, int16_t* gx, int16_t* gy, int16_t* gz);

#endif
