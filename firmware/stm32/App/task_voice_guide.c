/**
 * @file task_voice_guide.c
 * @brief 语音实时指导实现
 *
 * v1: 逻辑框架 + 条件触发
 * 硬件播放部分待接入 DAC/PWM
 *
 * 测试: 待写 PC 端模拟测试
 */

#include "task_voice_guide.h"
#include "data_bus.h"
#include "app_config.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

/* ======================== 音频播放（待硬件实现） ======================== */

/*
 * 预录制音频存储方案:
 * - 每个 .wav 片段 1-3 秒，8kHz 8-bit PCM
 * - 存储在外部 Flash（如有）或内部 Flash 空余区域
 * - 播放: DMA + DAC 或 PWM + 低通滤波
 *
 * 预估资源占用:
 * - 8kHz 8-bit × 3s = 24KB per clip
 * - 8 个片段 = 192KB (超出内部 Flash 余量)
 * - 方案: 压缩为 ADPCM (4:1) → 48KB，或用外部 SPI Flash
 */

/* 占位: 实际播放函数 */
static void play_audio(voice_guide_type_t type)
{
    /* TODO: DAC DMA 播放对应音频片段 */
    (void)type;
}

/* ======================== 状态 ======================== */

/* 每种指导的最后触发时间（初始化为 0，第一次无条件触发） */
static uint32_t last_trigger_time[VOICE_COUNT] = {0};
static uint32_t last_global_trigger = 0;

/* 久坐计时器 */
static uint32_t sitting_start = 0;
static uint8_t was_sitting = 0;

/* 运动目标 */
#define DAILY_EXERCISE_GOAL_S  1800  /* 30 分钟 */
static uint8_t goal_announced = 0;

/* ======================== 辅助函数 ======================== */

static uint32_t now_ms(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

/**
 * @brief 检查是否可以触发指导（间隔控制）
 */
static int can_trigger(voice_guide_type_t type, uint32_t now)
{
    /* 全局间隔 */
    if ((now - last_global_trigger) < (uint32_t)VOICE_GLOBAL_MIN_INTERVAL_S * 1000)
        return 0;
    /* 同类型间隔 */
    if ((now - last_trigger_time[type]) < (uint32_t)VOICE_MIN_INTERVAL_S * 1000)
        return 0;
    return 1;
}

/**
 * @brief 触发语音指导
 */
static void do_trigger(voice_guide_type_t type, uint32_t now)
{
    if (!can_trigger(type, now)) return;

    play_audio(type);
    last_trigger_time[type] = now;
    last_global_trigger = now;
}

/**
 * @brief 计算 Karvonen 心率储备百分比
 * @param hr_bpm 当前心率
 * @param resting_hr 静息心率
 * @param max_hr 最大心率
 * @return 0.0-1.0 的储备百分比
 */
static float hr_reserve_pct(float hr_bpm, float resting_hr, float max_hr)
{
    float reserve = max_hr - resting_hr;
    if (reserve <= 0) return 0;
    float pct = (hr_bpm - resting_hr) / reserve;
    if (pct < 0) pct = 0;
    if (pct > 1) pct = 1;
    return pct;
}

/* ======================== 初始化 ======================== */

void voice_guide_init(void)
{
    memset(last_trigger_time, 0, sizeof(last_trigger_time));
    last_global_trigger = 0;
    sitting_start = 0;
    was_sitting = 0;
    goal_announced = 0;
}

/* ======================== 主任务 ======================== */

void TaskVoiceGuide(void *argument)
{
    voice_guide_init();

    for (;;) {
        uint32_t now = now_ms();
        har_result_t har = g_state.last_har;
        hr_result_t hr = g_state.last_hr;

        /* ---- 1. 心率区间指导 ---- */
        if (hr.is_valid && IS_DYNAMIC_ACT(har.activity)) {
            float hr_max = 220.0f - 22.0f;  /* TODO: 用户年龄参数 */
            float hr_rest = 72.0f;           /* TODO: 自适应静息心率 */
            float pct = hr_reserve_pct(hr.bpm_smoothed, hr_rest, hr_max);

            /* 心率过高 → 危险 */
            if (pct > HR_ZONE_DANGER_HI) {
                do_trigger(VOICE_HR_TOO_HIGH, now);
            }
            /* 在有氧区间但偏低 → 加速 */
            else if (pct >= HR_ZONE_WARMUP_LO && pct < HR_ZONE_AEROBIC_LO) {
                do_trigger(VOICE_SPEED_UP, now);
            }
            /* 在有氧区间且良好 → 鼓励 */
            else if (pct >= HR_ZONE_AEROBIC_LO && pct < HR_ZONE_AEROBIC_HI) {
                do_trigger(VOICE_KEEP_GOING, now);
            }
        }

        /* ---- 2. 久坐提醒 ---- */
        if (har.activity == ACT_SITTING || har.activity == ACT_LYING) {
            if (!was_sitting) {
                sitting_start = now;
                was_sitting = 1;
            } else if ((now - sitting_start) > (uint32_t)SITTING_REMINDER_S * 1000) {
                do_trigger(VOICE_SITTING_TOO_LONG, now);
                sitting_start = now;  /* 重置，30 分钟后再提醒 */
            }
        } else {
            was_sitting = 0;
            sitting_start = 0;
        }

        /* ---- 3. 运动目标达成 ---- */
        if (!goal_announced &&
            g_state.product.exercise_time_s >= DAILY_EXERCISE_GOAL_S) {
            do_trigger(VOICE_GOAL_REACHED, now);
            goal_announced = 1;
        }

        /* 1Hz 循环 */
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ======================== 外部触发接口 ======================== */

void voice_trigger(voice_guide_type_t type)
{
    uint32_t now = now_ms();
    do_trigger(type, now);
}
