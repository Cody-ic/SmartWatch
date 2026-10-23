/**
 * @file task_imu.h
 * @brief IMU 采集任务接口
 */

#ifndef TASK_IMU_H
#define TASK_IMU_H

#include <stdint.h>
#include "app_config.h"

/**
 * @brief 初始化 IMU 任务（环形缓冲 + mutex）
 */
void task_imu_init(void);

/**
 * @brief 获取最新的 HAR 窗口数据（线程安全）
 * @param dst 目标缓冲区 [IMU_CHANNELS][HAR_WINDOW_SIZE]
 * @return 0=成功, -1=mutex超时
 */
int imu_get_window(int16_t dst[IMU_CHANNELS][HAR_WINDOW_SIZE]);

/**
 * @brief IMU 采集主任务
 */
void TaskIMU(void *argument);

#endif /* TASK_IMU_H */
