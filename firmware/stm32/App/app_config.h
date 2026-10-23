/**
 * @file app_config.h
 * @brief 全局配置参数 — 所有阈值和常量集中管理
 *
 * 修改阈值只改这个文件，不用动业务逻辑
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>

/* ======================== 硬件参数 ======================== */

#define IMU_SAMPLE_RATE_HZ      104     /* 匹配 CTRL1_XL=0x4C, DS13305 ODR 表 ODR_XL=0100→104Hz */
#define PPG_SAMPLE_RATE_HZ      100     /* MAX30102 采样率 */
#define MIC_SAMPLE_RATE_HZ      8000    /* 麦克风采样率 */

#define IMU_CHANNELS            6       /* accel_xyz + gyro_xyz */
#define IMU_ACCEL_RANGE_G       8       /* ±8g */
#define IMU_GYRO_RANGE_DPS      2000    /* ±2000 dps */

/* ======================== HAR 模型 ======================== */

#define HAR_WINDOW_SIZE         512     /* 5.12s @ 100Hz */
/* [2026-07-02]: HAR_WINDOW_STRIDE 为死代码, 实际 stride=25(SAMPLES_PER_CALL), 保留供参考
 * [2026-07-03 CORRECTION]: 上述"死代码"不准确 — task_imu.c:110 实际引用此宏控制通知频率,
 *                           内层批量 stride 由 har_inference.c SAMPLES_PER_CALL=25 控制, 两者用途不同。 */
#define HAR_WINDOW_STRIDE       100
#define HAR_NUM_CLASSES         7

/* ======================== 睡眠分期模型 ======================== */

#define SLEEP_FEATURES          280     /* 56维 × 5窗口(±2) */
#define SLEEP_EPOCH_S           30      /* 每 epoch 30 秒 (AASM 标准) */
#define SLEEP_FLASH_KB          145     /* sleep_staging_model.h */

/* 活动类别 ID */
typedef enum {
    ACT_LYING     = 0,
    ACT_SITTING   = 1,
    ACT_STANDING  = 2,
    ACT_WALKING   = 3,
    ACT_JOGGING   = 4,
    ACT_DOWNSTAIRS= 5,
    ACT_CYCLING   = 6,
    ACT_UNKNOWN   = 255
} activity_t;

/* 动态/静态分类（用于防霸凌和产品功能） */
#define IS_DYNAMIC_ACT(a)  ((a) == ACT_WALKING || (a) == ACT_JOGGING || \
                            (a) == ACT_DOWNSTAIRS || (a) == ACT_CYCLING)
#define IS_STATIC_ACT(a)   ((a) == ACT_LYING || (a) == ACT_SITTING || \
                            (a) == ACT_STANDING)

/* ======================== 心率检测 ======================== */

#define HR_BUFFER_SIZE         200     /* 2秒 PPG 缓冲 @100Hz */
#define HR_PEAK_MIN_INTERVAL   30     /* 最小峰间距 = 0.3s (~200bpm 上限) */
#define HR_PEAK_MAX_INTERVAL   150    /* 最大峰间距 = 1.5s (~40bpm 下限) */
#define HR_SMOOTH_WINDOW       5      /* 心率移动平均窗口 */
#define HR_INIT_RESTING_BPM    72     /* 默认静息心率 */
#define HR_ANOMALY_THRESHOLD   40     /* 静息HR + 此值 = 异常阈值 */

/* ======================== 防霸凌状态机 ======================== */

/* HR 异常持续多久才触发（10Hz 采样，300 = 30秒） */
#define SAFETY_HR_ELEVATED_TIMEOUT_MS   30000

/* 麦克风开启后检测窗口 */
#define SAFETY_MIC_WINDOW_MS            5000

/* 声学能量阈值（需标定） */
#define SAFETY_ACOUSTIC_ENERGY_THRESH   5000.0f

/* IMU 异常：剧烈运动检测 */
#define SAFETY_IMU_VARIANCE_THRESH      2.0f    /* 加速度方差阈值 (g^2) */

/* 三重条件时间窗口 */
#define SAFETY_EVENT_WINDOW_MS          30000   /* 30秒内三事件关联 */

/* ======================== 产品功能 ======================== */

/* 卡路里估算 — MET 值表 */
typedef struct {
    activity_t activity;
    float met_value;       /* 代谢当量 */
} met_entry_t;

/* 有效运动判断 */
#define EXERCISE_HR_ZONE_FRACTION   0.5f   /* HR 储备百分比阈值 */
#define EXERCISE_MIN_DURATION_S     10     /* 最短有效运动时间（秒） */
#define EXERCISE_SMOOTH_WINDOW_S    5      /* 滑动窗口平滑（秒） */

/* 运动效果评分权重 */
#define SCORE_HR_WEIGHT     0.6f
#define SCORE_STABILITY_WEIGHT  0.4f

/* ======================== BLE 协议 ======================== */

#define BLE_PACKET_HEADER      0xAA
#define BLE_MAX_PAYLOAD        128

/* 消息类型 */
typedef enum {
    BLE_MSG_HAR_RESULT     = 0x01,
    BLE_MSG_HR_DATA        = 0x02,
    BLE_MSG_SAFETY_ALERT   = 0x10,
    BLE_MSG_CALORIE        = 0x20,
    BLE_MSG_EXERCISE_SCORE = 0x21,
    BLE_MSG_STATUS         = 0x30,
} ble_msg_type_t;

/* ======================== FreeRTOS ======================== */

/* [2026-07-05] 以下为旧架构 task 栈定义, 仅供参考。当前活跃任务栈在 main.c 中直接指定:
 *   Sensor_Task: 1536w(6KB), Mic_Task: 512w(2KB), KWSTask: 1024w(4KB) */
#define TASK_IMU_STACK_SIZE     512
#define TASK_PPG_STACK_SIZE     512
#define TASK_HAR_STACK_SIZE     1536  /* [2026-07-05] 参考 Sensor_Task 实际分配更新 */
#define TASK_SAFETY_STACK_SIZE  512
#define TASK_MIC_STACK_SIZE     1024
#define TASK_PRODUCT_STACK_SIZE 512
#define TASK_BLE_STACK_SIZE     512

#define QUEUE_IMU_DATA_LEN      10
#define QUEUE_PPG_DATA_LEN      10
#define QUEUE_HAR_RESULT_LEN    5
#define QUEUE_HR_RESULT_LEN     5

#endif /* APP_CONFIG_H */
