/**
 * @file    ble_uart.h
 * @brief   BLE UART 驱动 — STM32 LPUART1 ↔ CH582M 透传模块
 *
 * CH582M 烧录 WCH 官方 BLE_UART 透传固件后，行为等价于 HM-10 透传模块：
 *   STM32 LPUART1 TX → CH582 UART RX → BLE Notify → 手机
 *   手机 BLE Write → CH582 UART TX → STM32 LPUART1 RX
 *
 * 数据包格式: [0xAA][type(1B)][len(1B)][payload(N)][checksum(1B)]
 *   checksum = ~sum(header..payload) + 1  (二进制补码)
 *
 * 使用方式:
 *   1. main.c 初始化:  ble_uart_init();
 *   2. Sensor_Task 中定时发送:  ble_uart_send_har/ble_uart_send_hr/...
 *   3. LPUART1 中断自动接收，ble_uart_process() 在主循环中轮询处理
 */

#ifndef BLE_UART_H
#define BLE_UART_H

#include "stm32u5xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 数据包定义
 * ============================================================ */

#define BLE_PACKET_HEADER  0xAA
#define BLE_RX_BUF_SIZE    1024   /* 1023 usable bytes; restore uses stop-and-wait ACKs */
#define BLE_TX_BUF_SIZE    128    /* 发送缓冲区 */

typedef enum {
    BLE_MSG_HAR_RESULT   = 0x01,   /* HAR 活动识别结果 */
    BLE_MSG_HR_DATA      = 0x02,   /* 心率数据 */
    BLE_MSG_SAFETY_ALERT = 0x03,   /* 安全告警（跌倒/霸凌/心率异常） */
    BLE_MSG_STATUS       = 0x04,   /* 综合状态（卡路里/评分/运动时间） */
    BLE_MSG_STEPS        = 0x05,   /* 步数数据 */
    BLE_MSG_CMD_RESP     = 0x80,   /* 命令响应 */
    BLE_MSG_TIME_SYNC    = 0x91,   /* [2026-07-03]: 手机授时 (4B Unix BE) */
    BLE_MSG_BATTERY      = 0x06,   /* [2026-07-03]: 电池电压 (2B mV LE + 1B pct) */
    BLE_MSG_SESSION_CTRL = 0x0C,   /* [2026-07-05]: 运动 session 控制 (手机→手表) */
    BLE_MSG_PROTO_RESULT = 0x0A,   /* [2026-07-14]: Proto 推理结果 (手表→手机) */
    BLE_MSG_PROTO_STATUS = 0x0D,   /* [2026-07-05]: 原型状态查询 (手表→手机) */
    BLE_MSG_PROTO_BACKUP = 0x0E,   /* [2026-07-05]: 原型备份 (手表→手机) */
    BLE_MSG_PROTO_RESTORE= 0x0F,   /* [2026-07-05]: 原型恢复 (手机→手表) */
    BLE_MSG_PROTO_ACK    = 0x10,   /* transfer ID u32 LE, fragment index, status */
    BLE_MSG_SLEEP_APNEA  = 0x07,   /* [2026-07-05]: 睡眠呼吸暂停报告 (手表→手机) */
    BLE_MSG_SLEEP_STAGE  = 0x08,   /* [2026-07-05]: 睡眠分期 P(sleep) (手表→手机) */
    BLE_MSG_SPO2_DATA    = 0x09,   /* [2026-07-05]: 实时 SpO2 数据 (手表→手机) */
} ble_msg_type_t;

/* [2026-07-04]: 0x80 命令子类型 — 手机 → 手表 */
#define BLE_CMD_SET_PROFILE  0x01   /* 用户参数: weight_kg(4B float LE) + age(1B) */

/* [2026-07-05]: 0x0C session 控制子命令 */
#define BLE_SESSION_START  0x01
#define BLE_SESSION_END    0x02
#define BLE_SESSION_ABORT  0x03

/* [2026-07-05]: 0x0E/0x0F 原型数据子命令 */
#define BLE_PROTO_FULL           0x01   /* 全部 7 类原型 */
#define BLE_PROTO_FACTORY        0x02   /* 恢复出厂通用原型 */
#define BLE_PROTO_REQUEST_BACKUP 0x03   /* [2026-07-14]: 手机请求手表发送 0x0E backup */
#define BLE_PROTO_BEGIN          0xF0   /* outside fragment indices 0..29: ID u32 LE + CRC32 u32 LE */
#define BLE_PROTO_N_CLASSES 7
#define BLE_PROTO_DIM       128

/* [2026-07-14]: Session 追踪 — 单个活动类 embedding 累加, 结束时更新原型 */
void proto_session_start(uint8_t class_id);
/** 门控 feed — HAR 类别匹配目标类 + 置信度 > 阈值才累加 */
void proto_session_feed_if(const float embedding[BLE_PROTO_DIM], int har_class, float har_conf);
void proto_session_feed(const float embedding[BLE_PROTO_DIM]);
void proto_session_end(void);
void proto_session_abort(void);
int  proto_session_is_active(void);
const uint16_t *proto_session_get_counts(void);

/* ============================================================
 * 公共 API
 * ============================================================ */

/**
 * 初始化 BLE UART（LPUART1 已由 CubeMX 配置）
 * 启动空闲中断接收，使能环形缓冲
 */
void ble_uart_init(void);

/**
 * 在 FreeRTOS 任务或主循环中定时调用，处理接收到的数据包
 * 内部根据消息类型分发
 */
void ble_uart_process(void);

/**
 * 检查 BLE 是否已连接（CH582 上电广播中不算连接）
 * @return 1=已连接，0=断开/广播中
 */
int  ble_uart_is_connected(void);

/* ---- 发送接口（直接在 Sensor_Task 中调用，非阻塞） ---- */

int  ble_uart_send(ble_msg_type_t type, const uint8_t *payload, uint8_t len);

/* ---- 便捷封装 ---- */

int  ble_uart_send_har(int activity, float confidence);
int  ble_uart_send_hr(float bpm_smoothed, float bpm_raw, int valid);
int  ble_uart_send_safety(uint8_t hr_anomaly, uint8_t acoustic_alert,
                           uint8_t imu_anomaly);
int  ble_uart_send_steps(uint32_t steps, uint32_t goal, float distance_km, float kcal);
int  ble_uart_send_status(float calorie, float score, uint32_t exercise_time_s,
                           int current_act, float current_hr);
int  ble_uart_send_battery(uint16_t mv, uint8_t pct);  /* [2026-07-03] */
int  ble_uart_send_user_config(float weight_kg, uint8_t age);  /* [2026-07-04]: ACK 用户参数 */
int  ble_uart_send_proto_result(int proto_class, float confidence);  /* [2026-07-14] */
int  ble_uart_send_proto_status(uint8_t version, const uint16_t counts[BLE_PROTO_N_CLASSES]);  /* [2026-07-05] */
int  ble_uart_send_proto_backup(uint8_t version, const float prototypes[BLE_PROTO_N_CLASSES][BLE_PROTO_DIM], uint32_t transfer_id);
int  ble_uart_send_sleep_apnea(uint8_t severity, float odi, uint16_t total_events,
                                 uint8_t lowest_spo2, uint16_t avg_dur_s,
                                 uint16_t monitor_min, uint8_t is_asleep);  /* [2026-07-05] */
int  ble_uart_send_sleep_stage(float p_sleep, uint8_t is_asleep);  /* [2026-07-05] */
int  ble_uart_send_spo2(float spo2, uint8_t valid);  /* [2026-07-05] */

#ifdef __cplusplus
}
#endif

#endif /* BLE_UART_H */
