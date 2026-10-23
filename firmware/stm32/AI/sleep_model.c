/**
 * @file sleep_model.c
 * @brief 睡眠分期推理引擎 — LightGBM 108-tree binary classifier
 *
 * 调用 sleep_staging_model.h 内 static inline 推理函数
 * 模型: DREAMT 100人训练, 消融验证跨数据集泛化 (Macro F1=0.697)
 */
#include "sleep_model.h"
#include "sleep_staging_model.h"  /* static inline sleep_predict / sleep_predict_class */

void sleep_model_predict(const float *features, float *probs)
{
    sleep_predict(features, probs);
}

int sleep_model_predict_class(const float *features)
{
    return sleep_predict_class(features);
}
