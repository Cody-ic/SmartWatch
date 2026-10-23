/**
 * @file exercise_scoring.h
 * @brief 运动评分 & 卡路里估算 — 自包含 C 头文件
 *
 * v2.1: [2026-07-04] CC 三源交叉验证修正:
 *   ❌→✅  坐 MET:  1.3 → 1.0  (Compendium 2024: 07021 sitting quietly)
 *   ❌→✅  站 MET:  1.8 → 1.3  (Compendium 2024: 07040 standing quietly)
 *   ⚠️→✅  跑 MET:  8.0 → 7.5  (Compendium 2024: 12020 jogging general)
 *   ⚠️→✅  最大心率: 220-age → 208-0.7×age  (Tanaka 2001, JACC)
 *   ⚠️→✅  HRR阈值:  60% → 40%  (ACSM moderate intensity lower bound)
 *   ⚠️→✅  走 MET 注释: 17190→midpoint of 3.0-3.8
 *   来源: WHO 2020 → ACSM → Compendium 2024 三源独立交叉验证
 *
 * 参考:
 *   S1: 2024 Adult Compendium of Physical Activities (pacompendium.com)
 *   S2: ACSM's Guidelines for Exercise Testing and Prescription
 *   S3: WHO 2020 Guidelines on Physical Activity and Sedentary Behaviour
 *   S4: Tanaka H et al. JACC 2001;37(1):153-156 (HRmax formula)
 *
 * 无外部依赖。直接 include 即可使用。
 */

#ifndef EXERCISE_SCORING_H
#define EXERCISE_SCORING_H

#include <math.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════ 活动类别 (与 HAR 7 分类对齐) ═══════ */
typedef enum {
    EX_LYING      = 0,
    EX_SITTING    = 1,
    EX_STANDING   = 2,
    EX_WALKING    = 3,
    EX_JOGGING    = 4,
    EX_DOWNSTAIRS = 5,
    EX_CYCLING    = 6,
    EX_UNKNOWN    = 7,
} ex_activity_t;

/* ═══════ MET 表 — Compendium of Physical Activities 2024 ═══════ */
static const float EX_MET_TABLE[] = {
    1.0f,   /* LYING        — 07011: lying quietly */
    1.0f,   /* SITTING      — 07021: sitting quietly, general */
    1.3f,   /* STANDING     — 07040: standing quietly */
    3.5f,   /* WALKING      — midpoint of 3.0 (17170: 2.5mph) – 3.8 (17190: 2.8-3.4mph) */
    7.5f,   /* JOGGING      — 12020: jogging, general */
    3.5f,   /* DOWNSTAIRS   — 17070: descending stairs */
    6.0f,   /* CYCLING      — moderate cycling (5.8-7.0 midpoint) */
    1.3f,   /* UNKNOWN      — 占位: 站立水平 */
};

#define EX_MET_TABLE_LEN 8

/* ═══════ 运动判定阈值 (ACSM 标准) ═══════ */
#define EX_HR_RESERVE_FRACTION  0.4f    /* HRR 40% = ACSM moderate intensity lower bound */
#define EX_MIN_VALID_HR         40.0f
#define EX_MAX_VALID_HR         220.0f

/* ═══════ 评分权重 ═══════ */
#define EX_SCORE_HR_WEIGHT        0.6f
#define EX_SCORE_STABILITY_WEIGHT 0.4f

/* ═══════ 状态结构 ═══════ */
typedef struct {
    /* 用户参数 */
    float   weight_kg;
    uint8_t age;
    float   max_hr;             /* Tanaka: 208 - 0.7*age */
    float   resting_hr;         /* 72bpm 初始, 建议用 hr_detector 自适应值覆盖 */

    /* 累计指标 (GROSS 热量, 含 BMR) */
    float   calorie_kcal;
    uint32_t exercise_seconds;
    float   hr_accum;
    uint32_t mvpa_seconds;
    float   total_score_accum;
    uint32_t score_count;

    /* 实时值 */
    float   realtime_score;
    float   session_score;
    float   current_met;
    uint8_t is_mvpa;
    uint8_t is_exercising;

    uint32_t last_update_ms;
} exercise_state_t;

/* ═══════ API ═══════ */

static inline void exercise_init(exercise_state_t *s, float weight_kg, uint8_t age)
{
    s->weight_kg        = weight_kg;
    s->age              = age;
    s->max_hr           = 208.0f - 0.7f * (float)age;  /* Tanaka 2001 */
    s->resting_hr       = 72.0f;
    s->calorie_kcal     = 0.0f;
    s->exercise_seconds = 0;
    s->hr_accum         = 0.0f;
    s->mvpa_seconds     = 0;
    s->total_score_accum = 0.0f;
    s->score_count      = 0;
    s->realtime_score   = 0.0f;
    s->session_score    = 0.0f;
    s->current_met      = 1.0f;
    s->is_mvpa          = 0;
    s->is_exercising    = 0;
    s->last_update_ms   = 0;
}

static inline float exercise_get_met(int activity)
{
    if (activity < 0 || activity >= EX_MET_TABLE_LEN)
        return EX_MET_TABLE[EX_UNKNOWN];
    return EX_MET_TABLE[activity];
}

static inline int exercise_is_dynamic(int activity)
{
    return (activity >= EX_WALKING && activity <= EX_CYCLING);
}

static inline int exercise_is_mvpa_f(float met)
{
    return (met >= 3.0f);  /* WHO 2020: moderate = 3-6 MET */
}

/**
 * @brief 每秒调用一次，更新所有运动指标
 * @param s          运动状态
 * @param act        HAR 活动类别 (0-6), -1 = 无新推理结果
 * @param hr_bpm     当前心率 (bpm)
 * @param hr_valid   心率是否有效 (1=有效)
 * @param confidence HAR 分类置信度 (0-1, softmax 后)
 * @param ts_ms      当前时间戳 (ms), 用于 900ms 防抖
 */
static inline void exercise_tick(exercise_state_t *s,
                                  int act, float hr_bpm, int hr_valid,
                                  float confidence, uint32_t ts_ms)
{
    if (ts_ms - s->last_update_ms < 900) return;
    s->last_update_ms = ts_ms;

    float met = exercise_get_met(act);
    s->current_met = met;

    /* 卡路里: GROSS 热量 (含 BMR) = MET × kg × (1/3600) per second */
    s->calorie_kcal += met * s->weight_kg / 3600.0f;

    /* 有效运动: 动态类 + HR > resting + 40% HRR (ACSM moderate intensity lower bound) */
    int is_dynamic = exercise_is_dynamic(act);
    int exercising = 0;
    if (is_dynamic && hr_valid &&
        hr_bpm >= EX_MIN_VALID_HR && hr_bpm <= EX_MAX_VALID_HR) {
        float hr_reserve = s->max_hr - s->resting_hr;
        float threshold  = s->resting_hr + hr_reserve * EX_HR_RESERVE_FRACTION;
        if (hr_bpm > threshold) {
            exercising = 1;
            s->exercise_seconds++;
            s->hr_accum += hr_bpm;
        }
    }
    s->is_exercising = (uint8_t)exercising;

    /* MVPA: MET ≥ 3.0 (WHO 2020 moderate + vigorous) */
    s->is_mvpa = (uint8_t)exercise_is_mvpa_f(met);

    /* 运动效果评分: HR 区间 60% + 动作稳定性 40% */
    float score = 0.0f;
    if (exercising && confidence > 0.0f) {
        float hr_reserve = s->max_hr - s->resting_hr;
        float hr_pct = (hr_bpm - s->resting_hr) / hr_reserve;
        if (hr_pct < 0.0f) hr_pct = 0.0f;
        if (hr_pct > 1.0f) hr_pct = 1.0f;
        float hr_score = hr_pct * 100.0f;
        float stability_score = confidence * 100.0f;
        score = EX_SCORE_HR_WEIGHT * hr_score +
                EX_SCORE_STABILITY_WEIGHT * stability_score;
        if (score > 100.0f) score = 100.0f;

        s->total_score_accum += score;
        s->score_count++;
    }
    s->realtime_score = score;

    if (s->score_count > 0)
        s->session_score = s->total_score_accum / (float)s->score_count;

    if (s->is_mvpa)
        s->mvpa_seconds++;
}

/**
 * @brief 更新静息心率 (从 hr_detector 自适应值同步)
 *
 * 调用时机: hr_detector 检测到安静状态并更新 resting_hr 后,
 *           在 Sensor_Task 上下文中调此函数同步。
 */
static inline void exercise_set_resting_hr(exercise_state_t *s, float resting_hr)
{
    if (resting_hr >= 40.0f && resting_hr <= 100.0f)
        s->resting_hr = resting_hr;
}

static inline float exercise_avg_hr(const exercise_state_t *s)
{
    if (s->exercise_seconds == 0) return 0.0f;
    return s->hr_accum / (float)s->exercise_seconds;
}

static inline float exercise_minutes(const exercise_state_t *s)
{
    return (float)s->exercise_seconds / 60.0f;
}

static inline float exercise_mvpa_minutes(const exercise_state_t *s)
{
    return (float)s->mvpa_seconds / 60.0f;
}

static inline void exercise_reset_session(exercise_state_t *s)
{
    s->calorie_kcal      = 0.0f;
    s->exercise_seconds  = 0;
    s->hr_accum          = 0.0f;
    s->mvpa_seconds      = 0;
    s->total_score_accum = 0.0f;
    s->score_count       = 0;
    s->session_score     = 0.0f;
}

#ifdef __cplusplus
}
#endif

#endif /* EXERCISE_SCORING_H */
