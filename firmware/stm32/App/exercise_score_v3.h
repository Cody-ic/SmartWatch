/**
 * exercise_score_v3.c - 运动评分 V3 (MET 主导 + HR 微调)
 * 
 * 改进：从纯 TRIMP (ρ=0.20) 提升到 MET+HR 混合 (ρ=0.92)
 * 核心思路：HAR 分类 → MET 查表 (70%) + HR 相对偏差 (30%)
 *
 * 依赖：HAR 分类结果、HR 传感器读数
 * 输出：0-100 分
 */

#ifndef EXERCISE_SCORE_V3_H
#define EXERCISE_SCORE_V3_H

#include <stdint.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── HAR 活动类别（与固件 HAR 模型一致）── */
typedef enum {
    ACT_LYING = 0,
    ACT_SITTING = 1,
    ACT_STANDING = 2,
    ACT_WALKING = 3,
    ACT_JOGGING = 4,
    ACT_DOWNSTAIRS = 5,
    ACT_CYCLING = 6,
    ACT_OTHER = 7,
} activity_t;

/* ── MET 查找表 ── */
static const float MET_TABLE[] = {
    1.0f,   /* LYING */
    1.3f,   /* SITTING */
    1.8f,   /* STANDING */
    3.5f,   /* WALKING */
    7.0f,   /* JOGGING */
    3.0f,   /* DOWNSTAIRS */
    6.8f,   /* CYCLING */
    1.0f,   /* OTHER */
};

/* ── 典型 HR（用于相对偏差计算）── */
static const float TYPICAL_HR[] = {
    75.0f,  /* LYING */
    80.0f,  /* SITTING */
    88.0f,  /* STANDING */
    113.0f, /* WALKING */
    157.0f, /* JOGGING */
    131.0f, /* DOWNSTAIRS */
    125.0f, /* CYCLING */
    80.0f,  /* OTHER */
};

/* ── MVPA 判定 ── */
static inline int is_mvpa(activity_t act) {
    int idx = (int)act;
    if (idx < 0 || idx > ACT_OTHER) return 0;
    return act >= ACT_WALKING && act <= ACT_CYCLING;
}

/**
 * 计算运动评分 V3
 * 
 * @param act   HAR 分类结果
 * @param hr    当前心率（bpm），<0 表示无效
 * @param alpha MET 权重（0.7 = 70% MET + 30% HR），范围 [0, 1]
 * 
 * @return 0-100 分
 */
static inline float exercise_score_v3(activity_t act, float hr, float alpha) {
    /* 范围保护：防止越界数组访问 */
    int idx = (int)act;
    if (idx < 0 || idx > ACT_OTHER) idx = ACT_OTHER;

    /* 基础分：MET 线性映射 */
    float met = MET_TABLE[idx];
    float base = (met - 1.0f) / 6.0f * 100.0f;
    if (base < 0.0f) base = 0.0f;
    if (base > 100.0f) base = 100.0f;
    
    /* HR 偏差因子 */
    float hr_factor = 1.0f;
    if (hr > 0.0f) {
        float typical = TYPICAL_HR[idx];
        float ratio = hr / typical;
        /* 限幅 [0.8, 1.2] */
        if (ratio < 0.8f) ratio = 0.8f;
        if (ratio > 1.2f) ratio = 1.2f;
        hr_factor = ratio;
    }
    
    /* 混合评分 */
    float score = base * (alpha + (1.0f - alpha) * hr_factor);
    
    if (score < 0.0f) score = 0.0f;
    if (score > 100.0f) score = 100.0f;
    return score;
}

/**
 * 计算每分钟运动评分（用于历史记录）
 * 
 * @param acts  60 个 epoch 的活动分类（1s/epoch）
 * @param hrs   60 个 epoch 的心率（可含 NaN 用 -1 表示）
 * @param n     epoch 数（通常 60）
 * @param alpha MET 权重
 * 
 * @return 平均运动评分 0-100
 */
static inline float exercise_score_minute(const activity_t *acts,
                                          const float *hrs,
                                          int n, float alpha) {
    float sum = 0.0f;
    int valid = 0;
    for (int i = 0; i < n; i++) {
        if (is_mvpa(acts[i])) {
            sum += exercise_score_v3(acts[i], hrs[i], alpha);
            valid++;
        }
    }
    return valid > 0 ? sum / valid : 0.0f;
}

#ifdef __cplusplus
}
#endif

#endif /* EXERCISE_SCORE_V3_H */
