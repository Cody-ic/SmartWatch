/**
 * @file task_ble.c
 * @brief [DEPRECATED] BLE 通信任务 — 向手机 APP 发送数据 + 接收指令
 *
 * ⚠️  v3 (2026-07-03): 此任务已废弃，实际 BLE 发送应由 main.c Sensor_Task 直接调用
 *     ble_uart_send_*()。task_ble.c 的 ble_send() 实现与 ble_uart.c 重复且从未被 xTaskCreate。
 *     保留供参考，待清理。参见 [[cubemx-code-trust]] 修改原则。
 *
 * 当前：UART 模拟 BLE（实际接 HM-10 或类似模块）
 * TODO: 换成真正的 BLE 协议栈
 */

#define TASK_BLE_DEPRECATED  /* [2026-07-03]: 未创建, 实际由 ble_uart.c + Sensor_Task 替代 */

#if 0  /* TASK_BLE_DEPRECATED */

#include "task_ble.h"
#include "data_bus.h"
#include "app_config.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

extern UART_HandleTypeDef huart1;  /* BLE UART */

/* 简单数据包格式: [0xAA][type][len][payload...][checksum] */
static uint8_t tx_buf[64];

static uint8_t checksum(const uint8_t *data, int len)
{
    uint8_t sum = 0;
    for (int i = 0; i < len; i++) sum += data[i];
    return (uint8_t)(~sum + 1);
}

static void ble_send(ble_msg_type_t type, const uint8_t *payload, uint8_t len)
{
    if (len > sizeof(tx_buf) - 4) return;  /* 防溢出：header(2)+len(1)+payload+checksum(1) */
    tx_buf[0] = BLE_PACKET_HEADER;
    tx_buf[1] = (uint8_t)type;
    tx_buf[2] = len;
    memcpy(&tx_buf[3], payload, len);
    tx_buf[3 + len] = checksum(tx_buf, 3 + len);

    HAL_UART_Transmit(&huart1, tx_buf, 4 + len, 100);
}

static void send_har_data(void)
{
    har_result_t har = g_state.last_har;
    uint8_t payload[6];
    payload[0] = (uint8_t)har.activity;
    memcpy(&payload[1], &har.confidence, 4);
    payload[5] = (uint8_t)(har.imu_variance * 10);  /* 精度 0.1 */
    ble_send(BLE_MSG_HAR_RESULT, payload, 6);
}

static void send_hr_data(void)
{
    hr_result_t hr = g_state.last_hr;
    uint8_t payload[5];
    uint16_t bpm = (uint16_t)(hr.bpm_smoothed * 10);  /* 0.1 精度 */
    memcpy(&payload[0], &bpm, 2);
    payload[2] = hr.is_valid;
    uint16_t bpm_raw = (uint16_t)(hr.bpm * 10);
    memcpy(&payload[3], &bpm_raw, 2);
    ble_send(BLE_MSG_HR_DATA, payload, 5);
}

static void send_safety_alert(void)
{
    safety_event_t evt = g_state.safety;
    uint8_t payload[8];
    payload[0] = evt.hr_anomaly;
    payload[1] = evt.acoustic_alert;
    payload[2] = evt.imu_anomaly;
    memcpy(&payload[3], &evt.alert_timestamp, 4);
    payload[7] = 0;  /* reserved */
    ble_send(BLE_MSG_SAFETY_ALERT, payload, 8);
}

static void send_product_data(void)
{
    product_data_t pd = g_state.product;
    uint8_t payload[17];
    memcpy(&payload[0],  &pd.calorie, 4);
    memcpy(&payload[4],  &pd.exercise_score, 4);
    memcpy(&payload[8],  &pd.exercise_time_s, 4);
    payload[12] = (uint8_t)pd.current_act;
    memcpy(&payload[13], &pd.current_hr, 4);  /* float = 4 bytes */
    ble_send(BLE_MSG_STATUS, payload, 17);
}

void TaskBLE(void *argument)
{
    uint32_t tick = 0;

    for (;;) {
        /* 检查安全事件 */
        EventBits_t bits = xEventGroupGetBits(xSafetyEventGroup);

        if (bits & EVT_SAFETY_ALERT) {
            send_safety_alert();
            xEventGroupClearBits(xSafetyEventGroup, EVT_SAFETY_ALERT);
        }

        /* 定期发送数据（每秒一次） */
        tick++;
        if (tick >= 10) {  /* 100ms × 10 = 1s */
            tick = 0;
            send_har_data();
            send_hr_data();
            send_product_data();
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
#endif /* TASK_BLE_DEPRECATED — [2026-07-04 FIX]: 原 #endif 位置错误, 三行代码漏在外面 */
