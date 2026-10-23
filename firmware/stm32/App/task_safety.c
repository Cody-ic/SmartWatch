/**
 * @file task_safety.c
 * @brief 防霸凌状态机 — 三重触发融合
 *
 * 状态机逻辑:
 *   IDLE → HR 异常 → HR_ELEVATED（开麦克风，计时）
 *   HR_ELEVATED → 声学异常 → ALERT_VERIFY（查 IMU）
 *   ALERT_VERIFY → IMU 异常 → 报警！
 *   超时 / 恢复 → 回 IDLE
 *
 * v2: 修复 debounce + ALARM 冷却期初始化
 * 测试: 22/22 通过 (test_safety_state_machine.py)
 */

#include "task_safety.h"
#include "data_bus.h"
#include "app_config.h"
#include "spu0410.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

typedef enum {
    SAFETY_IDLE,
    SAFETY_HR_ELEVATED,
    SAFETY_ALERT_VERIFY,
    SAFETY_ALARM
} safety_state_t;

static safety_state_t safety_state = SAFETY_IDLE;
static uint32_t state_enter_tick = 0;

/* 报警冷却（避免连续报警） */
#define ALARM_COOLDOWN_MS  60000  /* 1 分钟 */
static uint32_t last_alarm_tick = 0;  /* 初始化为 0, 第一次无条件触发 */

/* IDLE debounce（避免 HR 持续异常时反复进入 HR_ELEVATED） */
static uint32_t last_idle_enter = 0;
#define IDLE_DEBOUNCE_MS   5000  /* 5 秒冷却 */

static void enter_state(safety_state_t new_state, uint32_t now)
{
    safety_state = new_state;
    state_enter_tick = now;
    if (new_state == SAFETY_IDLE) {
        last_idle_enter = now;
    }
}

void TaskSafety(void *argument)
{
    for (;;) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        uint32_t elapsed = now - state_enter_tick;

        switch (safety_state) {

        /* ---- 空闲：只检查心率异常 ---- */
        case SAFETY_IDLE: {
            /* Debounce: 从 IDLE 回退后 5 秒内不重新触发 */
            if ((now - last_idle_enter) > IDLE_DEBOUNCE_MS &&
                g_state.last_hr.is_valid &&
                g_state.last_hr.bpm_smoothed > (float)(HR_INIT_RESTING_BPM + HR_ANOMALY_THRESHOLD)) {
                /* 心率异常 → 开麦克风，进入观察 */
                mic_start();
                g_state.mic_active = 1;
                enter_state(SAFETY_HR_ELEVATED, now);
                xEventGroupSetBits(xSafetyEventGroup, EVT_HR_ANOMALY);
            }
            break;
        }

        /* ---- 心率偏高：等声学信号 ---- */
        case SAFETY_HR_ELEVATED: {
            /* 超时检查 */
            if (elapsed > SAFETY_HR_ELEVATED_TIMEOUT_MS) {
                mic_stop();
                g_state.mic_active = 0;
                xEventGroupClearBits(xSafetyEventGroup, EVT_HR_ANOMALY);
                enter_state(SAFETY_IDLE, now);
                break;
            }
            /* 心率恢复 → 回空闲 */
            if (!(g_state.last_hr.bpm_smoothed > (float)(HR_INIT_RESTING_BPM + HR_ANOMALY_THRESHOLD)) &&
                elapsed > 5000) {  /* 至少 5 秒才允许恢复 */
                mic_stop();
                g_state.mic_active = 0;
                enter_state(SAFETY_IDLE, now);
                break;
            }
            /* 检测到喊叫 → 进入验证 */
            if (mic_is_shouting()) {
                xEventGroupSetBits(xSafetyEventGroup, EVT_MIC_START);
                enter_state(SAFETY_ALERT_VERIFY, now);
            }
            break;
        }

        /* ---- 验证：检查 IMU 异常 ---- */
        case SAFETY_ALERT_VERIFY: {
            if (elapsed > SAFETY_MIC_WINDOW_MS) {
                /* 超时无运动异常 → 回到 HR_ELEVATED */
                enter_state(SAFETY_HR_ELEVATED, now);
                break;
            }

            /* IMU 异常 = 剧烈运动且非正常运动 */
            har_result_t har = g_state.last_har;
            float variance = har.imu_variance;
            int imu_anomaly = (variance > SAFETY_IMU_VARIANCE_THRESH) &&
                              (har.activity == ACT_UNKNOWN ||
                               IS_DYNAMIC_ACT(har.activity));

            if (imu_anomaly) {
                /* 三重触发 → 报警！ */
                enter_state(SAFETY_ALARM, now);
            }
            break;
        }

        /* ---- 报警 ---- */
        case SAFETY_ALARM: {
            if ((now - last_alarm_tick) > ALARM_COOLDOWN_MS) {
                /* 记录安全事件 */
                g_state.safety.hr_anomaly = 1;
                g_state.safety.acoustic_alert = 1;
                g_state.safety.imu_anomaly = 1;
                g_state.safety.alert_timestamp = now;

                /* 触发 BLE 报警 */
                xEventGroupSetBits(xSafetyEventGroup, EVT_SAFETY_ALERT);
                last_alarm_tick = now;
            }

            /* 保持报警 10 秒后，检查是否恢复 */
            if (elapsed > 10000) {
                mic_stop();
                g_state.mic_active = 0;
                g_state.safety.hr_anomaly = 0;
                g_state.safety.acoustic_alert = 0;
                g_state.safety.imu_anomaly = 0;
                xEventGroupSetBits(xSafetyEventGroup, EVT_SAFETY_CLEAR);
                enter_state(SAFETY_IDLE, now);
            }
            break;
        }
        }

        /* 10Hz 循环 */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
