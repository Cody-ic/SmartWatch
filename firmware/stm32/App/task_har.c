/**
 * @file task_har.c
 * @brief [DEPRECATED] HAR 推理任务 — 每秒一次，从 IMU 环形缓冲推理活动类别
 *
 * ⚠️  v3 (2026-07-03): 此任务已废弃，实际 HAR 推理在 Sensor_Task(main.c)
 *     → app_on_imu_batch() → HAR_AddSamples() 中完成，不依赖 task_imu/task_har。
 *     task_imu.c + task_har.c 为旧架构保留，待清理。
 *
 * v2: 使用 imu_get_window() 获取线程安全的窗口数据
 */

#define TASK_HAR_DEPRECATED  /* [2026-07-03]: 此任务未创建, 实际由 main.c Sensor_Task 替代 */

#include "task_har.h"
#include "data_bus.h"
#include "app_config.h"
#include "har_model.h"
#include "FreeRTOS.h"
#include "task.h"
#include <math.h>
#include <string.h>

#if 0  /* TASK_HAR_DEPRECATED — 旧架构, 实际由 main.c Sensor_Task → HAR_AddSamples() 替代 */

/* IMU 窗口原始数据（从 TaskIMU 获取） */
static int16_t imu_window_raw[IMU_CHANNELS][HAR_WINDOW_SIZE];

void TaskHAR(void *argument)
{
    har_model_init();

    float input[IMU_CHANNELS][HAR_WINDOW_SIZE];
    float output[HAR_NUM_CLASSES];
    har_result_t result;

    for (;;) {
        /* 等 IMU 数据通知（每秒一次） */
        imu_sample_t sample;
        if (xQueueReceive(xQueueIMU, &sample, portMAX_DELAY) != pdTRUE)
            continue;

        /* 1. 获取最新的 IMU 窗口数据（线程安全） */
        if (imu_get_window(imu_window_raw) != 0) {
            continue;  /* mutex 超时，跳过本次 */
        }

        /* 2. 归一化 */
        har_normalize(imu_window_raw, input);

        /* 3. 推理 */
        uint8_t pred = har_model_predict(input, output);

        /* 4. 计算置信度（softmax 最大值） */
        float max_prob = 0;
        for (int i = 0; i < HAR_NUM_CLASSES; i++) {
            if (output[i] > max_prob) max_prob = output[i];
        }

        /* 5. 计算加速度方差（防霸凌用） */
        float var = 0;
        float mean_a = 0;
        for (int i = 0; i < HAR_WINDOW_SIZE; i++) {
            float a = input[0][i]*input[0][i] +
                      input[1][i]*input[1][i] +
                      input[2][i]*input[2][i];
            mean_a += a;
        }
        mean_a /= HAR_WINDOW_SIZE;
        for (int i = 0; i < HAR_WINDOW_SIZE; i++) {
            float a = input[0][i]*input[0][i] +
                      input[1][i]*input[1][i] +
                      input[2][i]*input[2][i];
            float d = a - mean_a;
            var += d * d;
        }
        var /= HAR_WINDOW_SIZE;

        /* 6. 填充结果 */
        result.activity = (activity_t)pred;
        result.confidence = max_prob;
        result.imu_variance = var;
        result.timestamp = sample.timestamp;

        /* 更新全局状态（critical section 防止 torn read） */
        taskENTER_CRITICAL();
        memcpy((void *)&g_state.last_har, &result, sizeof(result));
        taskEXIT_CRITICAL();

        /* 发给下游任务 */
        xQueueSend(xQueueHAR, &result, 0);
    }
}

#endif /* TASK_HAR_DEPRECATED */
