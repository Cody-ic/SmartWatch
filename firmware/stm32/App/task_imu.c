/**
 * @file task_imu.c
 * @brief [DEPRECATED] IMU 采集任务 — 100Hz 轮询读 LSM6DSOX + 滑动窗口缓冲
 *
 * ⚠️  v3 (2026-07-03): 此任务已废弃，实际运行的 Sensor_Task 在 main.c 中使用
 *     xSemaphoreTake + LSM6DSOX_GetFifoBatch(26) 的 FIFO 批量读取模式，
 *     中断驱动、无 ODR 偏差。task_imu.c + task_har.c 为旧架构保留，待清理。
 *
 * v2: 正确实现环形缓冲 + 滑动窗口
 *
 * 环形缓冲策略:
 *   - buffer[channel][HAR_WINDOW_SIZE] 存储最新的 512 个样本
 *   - 写指针 write_idx 在 0..511 循环
 *   - 每 HAR_WINDOW_STRIDE (100) 个新样本后，通知 HAR 任务
 *   - HAR 任务读取时，从 write_idx 开始顺序读取 512 个样本
 *
 * 线程安全:
 *   - 写入方: 只有 IMU task（100Hz ISR 级别）
 *   - 读取方: 只有 HAR task（每秒一次）
 *   - 使用 memcpy 而非逐字节写入，避免撕裂
 *   - 加 mutex 保护窗口拷贝操作
 */

#define TASK_IMU_DEPRECATED  /* [2026-07-03]: 此任务未创建, 实际由 main.c Sensor_Task 替代 */

#include "task_imu.h"
#include "data_bus.h"
#include "app_config.h"
#include "lsm6dsox.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

#if 0  /* TASK_IMU_DEPRECATED — 旧架构, 实际由 main.c Sensor_Task 替代 */

/* ======================== 环形缓冲 ======================== */

/*
 * 双缓冲策略:
 *   ring_buf: IMU task 写入（100Hz）
 *   har_buf:  HAR task 读取（1Hz）
 *   通过 mutex 保护 har_buf 的拷贝
 *
 * ring_buf 结构: [ch0_s0, ch0_s1, ..., ch0_s511]
 *                [ch1_s0, ch1_s1, ..., ch1_s511]
 *                ...
 *                [ch5_s0, ch5_s1, ..., ch5_s511]
 */
static int16_t ring_buf[IMU_CHANNELS][HAR_WINDOW_SIZE];
static volatile int write_idx = 0;     /* 写指针 0..511 */
static volatile int samples_since_last = 0;

/* HAR 任务读取用的缓冲（独立的，不怕撕裂） */
static int16_t har_window[IMU_CHANNELS][HAR_WINDOW_SIZE];

/* Mutex 保护窗口拷贝 */
static SemaphoreHandle_t xWindowMutex = NULL;

/* ======================== 初始化 ======================== */

void task_imu_init(void)
{
    memset(ring_buf, 0, sizeof(ring_buf));
    memset(har_window, 0, sizeof(har_window));
    write_idx = 0;
    samples_since_last = 0;
    xWindowMutex = xSemaphoreCreateMutex();
}

/* ======================== 获取窗口数据 ======================== */

/**
 * @brief 获取最新的 HAR 窗口数据（线程安全）
 * @param dst 目标缓冲区 [IMU_CHANNELS][HAR_WINDOW_SIZE]
 * @return 0=成功, -1=mutex超时
 *
 * 调用方: HAR task
 */
int imu_get_window(int16_t dst[IMU_CHANNELS][HAR_WINDOW_SIZE])
{
    if (xWindowMutex && xSemaphoreTake(xWindowMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memcpy(dst, har_window, sizeof(har_window));
        xSemaphoreGive(xWindowMutex);
        return 0;
    }
    return -1;
}

/* ======================== 主任务 ======================== */

void TaskIMU(void *argument)
{
    task_imu_init();
    lsm6dsox_init();

    for (;;) {
        /* 等 IMU 数据就绪（100Hz） */
        if (lsm6dsox_data_ready()) {
            lsm6dsox_data_t data;
            if (lsm6dsox_read(&data) == 0) {
                int idx = write_idx;

                /* 加速度: g → 16-bit raw (±8g range, sensitivity = 0.244 mg/LSB) */
                ring_buf[0][idx] = (int16_t)(data.accel[0] / 0.000244f);
                ring_buf[1][idx] = (int16_t)(data.accel[1] / 0.000244f);
                ring_buf[2][idx] = (int16_t)(data.accel[2] / 0.000244f);

                /* 陀螺仪: dps → 16-bit raw (±2000dps, sensitivity = 70 mdps/LSB) */
                ring_buf[3][idx] = (int16_t)(data.gyro[0]  / 0.070f);
                ring_buf[4][idx] = (int16_t)(data.gyro[1]  / 0.070f);
                ring_buf[5][idx] = (int16_t)(data.gyro[2]  / 0.070f);

                /* 更新写指针 */
                write_idx = (idx + 1) % HAR_WINDOW_SIZE;
                samples_since_last++;

                /* 每 STRIDE 个样本通知 HAR 任务 */
                if (samples_since_last >= HAR_WINDOW_STRIDE) {
                    samples_since_last = 0;

                    /* 拷贝到 HAR 缓冲（mutex 保护） */
                    if (xWindowMutex &&
                        xSemaphoreTake(xWindowMutex, 0) == pdTRUE) {
                        /*
                         * 从 write_idx 开始，顺序读取 512 个样本
                         * 因为是环形缓冲，需要处理 wrap-around
                         */
                        int first_len = HAR_WINDOW_SIZE - write_idx;
                        for (int ch = 0; ch < IMU_CHANNELS; ch++) {
                            /* 旧数据（write_idx 到末尾） */
                            memcpy(&har_window[ch][0],
                                   &ring_buf[ch][write_idx],
                                   first_len * sizeof(int16_t));
                            /* 新数据（开头到 write_idx） */
                            memcpy(&har_window[ch][first_len],
                                   &ring_buf[ch][0],
                                   write_idx * sizeof(int16_t));
                        }
                        xSemaphoreGive(xWindowMutex);
                    }

                    /* 通知 HAR 任务有新数据 */
                    imu_sample_t sample;
                    sample.timestamp = data.timestamp;
                    memcpy(sample.accel, data.accel, sizeof(sample.accel));
                    memcpy(sample.gyro, data.gyro, sizeof(sample.gyro));
                    xQueueSend(xQueueIMU, &sample, 0);
                }
            }
        }

        /* 严格 10ms = 100Hz（旧架构：非中断驱动，存在~4% ODR 偏差） */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

#endif /* TASK_IMU_DEPRECATED */
