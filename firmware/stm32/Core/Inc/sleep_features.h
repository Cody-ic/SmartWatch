/**
 * @file sleep_features.h
 * @brief 睡眠分期特征提取 — 旧版 108-tree LightGBM (56-dim/epoch × 5 windows = 280)
 *
 * [2026-07-12] 曾尝试迁移到 LGB 50t (126-dim × 3 epochs = 378), 后因传感器不匹配
 *   (缺 EDA + HRV 非线性计算过重) 回退。旧模型 56×5=280 继续使用。
 *   LGB 50t 文件 (sleep_lgb_mcu_50t.c) 已从构建中移除。
 *   ⚠ 若日后换模型: 需同步改 SF_N_WINDOWS + SF_N_FEATURES + compute_epoch() 特征集。
 */

#ifndef SLEEP_FEATURES_H
#define SLEEP_FEATURES_H

#include <stdint.h>

#define SF_EPOCH_S      30
#define SF_N_WINDOWS    5    /* 108-tree 旧模型: 5 epochs (±2 window) */
#define SF_N_FEATURES   56   /* 108-tree 旧模型: 56 dim/epoch (ACC 19 + TEMP 5 + BVP 6 + HR 6 + PPG_Rate 1 + Circadian 3 + HRV 16) */
#define SF_INPUT_DIM    (SF_N_FEATURES * SF_N_WINDOWS)  /* 280 */

/* One 250 ms batch: 26 raw ACC samples at 104 Hz, up to 25 PPG at 100 Hz.
 * Incomplete batches invalidate the epoch; stale samples are never reused. */
void sleep_features_feed(const int16_t *acc26, const uint32_t *ppg_ir,
                         uint8_t ppg_count, float hr, float temp_c);
int  sleep_features_ready(void);
void sleep_features_get(float *features);

#endif
