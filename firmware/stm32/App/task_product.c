/**
 * @file task_product.c
 * @brief 产品功能任务 — 卡路里估算 + 运动评分 + 有效运动时长
 *
 * 每秒更新一次，从全局状态读取 HAR + HR 数据
 */

#include "task_product.h"
#include "data_bus.h"
#include "app_config.h"
#include "FreeRTOS.h"
#include "task.h"
#include <math.h>
#include <string.h>

/* ======================== MET 表 ======================== */

static const met_entry_t met_table[] = {
    { ACT_LYING,      1.0f },
    { ACT_SITTING,    1.2f },
    { ACT_STANDING,   1.5f },
    { ACT_WALKING,    3.5f },
    { ACT_JOGGING,    7.0f },
    { ACT_DOWNSTAIRS, 3.8f },
    { ACT_CYCLING,    6.0f },
};

#define MET_TABLE_LEN  (sizeof(met_table) / sizeof(met_table[0]))

static float get_met(activity_t act)
{
    for (int i = 0; i < MET_TABLE_LEN; i++) {
        if (met_table[i].activity == act)
            return met_table[i].met_value;
    }
    return 1.5f;  /* 默认 */
}

/* ======================== 状态 ======================== */

static float calorie_accum = 0;         /* 累计卡路里 (kcal) */
static uint32_t exercise_time_accum = 0; /* 有效运动累计（秒） */
static float score_buffer[60];           /* 1分钟评分缓冲 */
static int score_idx = 0;

/* 用户参数（可个性化） */
#define USER_WEIGHT_KG      65.0f
#define USER_AGE            22
#define USER_MAX_HR         (220 - USER_AGE)  /* 198 */
#define USER_RESTING_HR     72.0f

void TaskProduct(void *argument)
{
    for (;;) {
        har_result_t har = g_state.last_har;
        hr_result_t  hr  = g_state.last_hr;

        /* ---- 卡路里 ---- */
        /* 公式: Calorie = MET × weight_kg × time_h
         * 每秒增量 = MET × weight × (1/3600) */
        float met = get_met(har.activity);
        float cal_per_sec = met * USER_WEIGHT_KG / 3600.0f;
        calorie_accum += cal_per_sec;

        /* ---- 有效运动时长 ---- */
        /* 条件：动态运动 + 心率超过储备的50% */
        int is_exercising = 0;
        if (IS_DYNAMIC_ACT(har.activity) && hr.is_valid) {
            float hr_reserve = USER_MAX_HR - USER_RESTING_HR;
            float hr_zone_threshold = USER_RESTING_HR + hr_reserve * EXERCISE_HR_ZONE_FRACTION;
            if (hr.bpm_smoothed > hr_zone_threshold) {
                is_exercising = 1;
                exercise_time_accum++;
            }
        }

        /* ---- 运动效果评分 ---- */
        float score = 0;
        if (IS_DYNAMIC_ACT(har.activity) && hr.is_valid) {
            /* HR 得分：心率在区间内的占比 */
            float hr_reserve_pct = (hr.bpm_smoothed - USER_RESTING_HR) /
                                   (USER_MAX_HR - USER_RESTING_HR);
            if (hr_reserve_pct < 0) hr_reserve_pct = 0;
            if (hr_reserve_pct > 1) hr_reserve_pct = 1;
            float hr_score = hr_reserve_pct * 100;

            /* 稳定性得分：动作置信度（越高越稳） */
            float stability_score = har.confidence * 100;

            score = SCORE_HR_WEIGHT * hr_score +
                    SCORE_STABILITY_WEIGHT * stability_score;
            if (score > 100) score = 100;
        }

        /* 更新全局 */
        g_state.product.calorie = calorie_accum;
        g_state.product.exercise_time_s = exercise_time_accum;
        g_state.product.exercise_score = score;
        g_state.product.current_act = har.activity;
        g_state.product.current_hr = hr.bpm_smoothed;

        /* 1Hz */
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
