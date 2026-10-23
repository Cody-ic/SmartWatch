/**
 * @file data_bus.h
 * @brief 任务间数据传递 — FreeRTOS Queue / EventGroup 定义
 */

#ifndef DATA_BUS_H
#define DATA_BUS_H

#include "FreeRTOS.h"
#include "queue.h"
#include "event_groups.h"
#include "app_config.h"

/* ======================== 数据结构 ======================== */

/* IMU 一个采样点 (6 通道) */
typedef struct {
    int16_t accel[3];    /* x, y, z (raw ADC) */
    int16_t gyro[3];     /* x, y, z (raw ADC) */
    uint32_t timestamp;  /* ms */
} imu_sample_t;

/* PPG 一个采样点 */
typedef struct {
    int32_t red;
    int32_t ir;
    uint32_t timestamp;
} ppg_sample_t;

/* HAR 推理结果 */
typedef struct {
    activity_t activity;
    float confidence;
    float imu_variance;   /* 窗口内加速度方差（防霸凌用） */
    uint32_t timestamp;
} har_result_t;

/* 心率结果 */
typedef struct {
    float bpm;
    float bpm_smoothed;    /* 移动平均后 */
    uint8_t is_valid;      /* 信号质量标志 */
    uint32_t timestamp;
} hr_result_t;

/* 声学检测结果 */
typedef struct {
    float energy;          /* 短时能量 */
    float zcr;             /* 过零率 */
    uint8_t is_shouting;   /* 是否在喊叫 */
    uint32_t timestamp;
} acoustic_result_t;

/* 安全事件（防霸凌） */
typedef struct {
    uint8_t hr_anomaly;        /* 心率异常 */
    uint8_t acoustic_alert;    /* 声学异常 */
    uint8_t imu_anomaly;       /* 运动异常 */
    uint32_t alert_timestamp;  /* 报警时间 */
} safety_event_t;

/* 产品数据 */
typedef struct {
    float calorie;             /* 累计卡路里 (kcal) */
    float exercise_score;      /* 运动效果评分 0-100 */
    uint32_t exercise_time_s;  /* 有效运动时间（秒） */
    activity_t current_act;
    float current_hr;
} product_data_t;

/* ======================== 全局队列句柄 ======================== */

extern QueueHandle_t xQueueIMU;       /* imu_sample_t: IMU→HAR */
extern QueueHandle_t xQueuePPG;       /* ppg_sample_t: PPG→HR */
extern QueueHandle_t xQueueHAR;       /* har_result_t: HAR→Safety/Product */
extern QueueHandle_t xQueueHR;        /* hr_result_t:  HR→Safety/Product */

/* ======================== 事件组（防霸凌触发） ======================== */

#define EVT_HR_ANOMALY       (1 << 0)   /* 心率异常触发 */
#define EVT_MIC_START        (1 << 1)   /* 开启麦克风 */
#define EVT_MIC_STOP         (1 << 2)   /* 关闭麦克风 */
#define EVT_SAFETY_ALERT     (1 << 3)   /* 触发报警 */
#define EVT_SAFETY_CLEAR     (1 << 4)   /* 解除报警 */

extern EventGroupHandle_t xSafetyEventGroup;

/* ======================== 全局状态（线程安全读取） ========================
 *
 * 并发说明:
 *   - 写入端使用 taskENTER_CRITICAL() / taskEXIT_CRITICAL() 保护
 *   - 读取端（如 task_ble.c）未使用临界区，依赖 Cortex-M33 对 32-bit
 *     对齐成员的单拷贝原子性（single-copy atomic）。
 *   - har_result_t / hr_result_t / product_data_t 包含多个 float，
 *     理论上存在读取时被写入端 memcpy 打断导致"撕裂读"的风险。
 *   - 实际影响：1Hz 更新率下撕裂读概率极低，且下游 app 会忽略单帧异常值。
 *   - 若追求严格正确性，建议改为读写双缓冲（copy-on-write）+ 序列锁。
 */

typedef struct {
    /* 最新值（被各 task 更新） */
    volatile har_result_t  last_har;
    volatile hr_result_t   last_hr;
    volatile product_data_t product;
    volatile safety_event_t safety;

    /* 系统状态 */
    volatile uint8_t mic_active;     /* 麦克风是否在工作 */
    volatile uint8_t ble_connected;  /* BLE 是否连接 */
} system_state_t;

extern system_state_t g_state;

/* 初始化 */
void data_bus_init(void);

#endif /* DATA_BUS_H */
