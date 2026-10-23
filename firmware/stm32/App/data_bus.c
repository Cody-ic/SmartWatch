/**
 * @file data_bus.c
 * @brief 数据总线初始化
 *
 * ⚠️ DEAD CODE: 此文件及 App/ 目录下其他 task_*.c 均未被编译到当前固件中。
 * 活跃代码路径在 Core/Src/ 下使用 har_inference.c + safety_detect.c。
 * 若将来恢复此架构，需同步更新 makefile 和 main.c 中的 xTaskCreate。
 */

#include "data_bus.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

/* 全局实例 */
QueueHandle_t xQueueIMU      = NULL;
QueueHandle_t xQueuePPG      = NULL;
QueueHandle_t xQueueHAR      = NULL;
QueueHandle_t xQueueHR       = NULL;
EventGroupHandle_t xSafetyEventGroup = NULL;

system_state_t g_state = {0};

void data_bus_init(void)
{
    xQueueIMU = xQueueCreate(QUEUE_IMU_DATA_LEN, sizeof(imu_sample_t));
    xQueuePPG = xQueueCreate(QUEUE_PPG_DATA_LEN, sizeof(ppg_sample_t));
    xQueueHAR = xQueueCreate(QUEUE_HAR_RESULT_LEN, sizeof(har_result_t));
    xQueueHR  = xQueueCreate(QUEUE_HR_RESULT_LEN, sizeof(hr_result_t));
    xSafetyEventGroup = xEventGroupCreate();

    configASSERT(xQueueIMU);
    configASSERT(xQueuePPG);
    configASSERT(xQueueHAR);
    configASSERT(xQueueHR);
    configASSERT(xSafetyEventGroup);

    memset((void *)&g_state, 0, sizeof(g_state));
}
