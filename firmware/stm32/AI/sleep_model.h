/**
 * @file sleep_model.h
 * @brief 睡眠分期 (Wake/Sleep 二分类) LightGBM 推理接口
 *
 * 模型: 108 trees, 63 leaves/tree, 280 features
 * Flash: ~145KB (sleep_staging_model.h)
 * 数据: DREAMT 100人训练, AAUWSS 13人独立验证 (Macro F1=0.697)
 *
 * 特征: 56维/epoch × 5窗口(±2) = 280维
 *   ACC(19) + TEMP(5) + BVP(6) + HR(6) + PPG_Rate(1) + Circadian(3) + HRV(16)
 *   ⚠ 特征提取模块待实现, 当前仅提供推理接口
 */

#ifndef SLEEP_MODEL_H
#define SLEEP_MODEL_H

#include <stdint.h>

#define SLEEP_N_FEATURES  280   /* 输入特征维度 */
#define SLEEP_N_OUTPUTS   2     /* [P(wake), P(sleep)] */
#define SLEEP_FLASH_KB    145   /* 模型 Flash 占用 */

/**
 * @brief 运行睡眠分期推理
 * @param features [280] float32 特征向量, 顺序与 MCU 特征提取一致
 * @param probs    [2] float32 输出概率 [P(wake), P(sleep)]
 */
void sleep_model_predict(const float *features, float *probs);

/**
 * @brief 便捷接口 — 直接返回 0=Wake 或 1=Sleep
 * @param features [280] float32
 * @return 0=Wake, 1=Sleep
 */
int sleep_model_predict_class(const float *features);

#endif /* SLEEP_MODEL_H */
