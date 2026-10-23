/**
 * @file task_ppg.c
 * @brief PPG 采集 + 心率计算任务
 */

#include "task_ppg.h"
#include "data_bus.h"
#include "app_config.h"
#include "max30102.h"
#include "hr_detector.h"
#include "FreeRTOS.h"
#include "task.h"

static hr_state_t hr_state;

void TaskPPG(void *argument)
{
    max30102_init();
    hr_detector_init(&hr_state);

    max30102_sample_t samples[8];

    for (;;) {
        /* 从 MAX30102 FIFO 读数据 */
        int n = max30102_read_fifo(samples, 8);

        for (int i = 0; i < n; i++) {
            int updated = hr_detector_update(&hr_state, samples[i].ir);

            if (updated) {
                hr_result_t result;
                result.bpm = hr_state.bpm;
                result.bpm_smoothed = hr_state.bpm_smoothed;
                result.is_valid = (hr_state.signal_quality > 30) ? 1 : 0;
                result.timestamp = samples[i].timestamp;

                taskENTER_CRITICAL();
                memcpy((void *)&g_state.last_hr, &result, sizeof(result));
                taskEXIT_CRITICAL();
                xQueueSend(xQueueHR, &result, 0);
            }
        }

        /* 100Hz 轮询 */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
