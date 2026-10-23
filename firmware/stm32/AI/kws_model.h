/**
 * @file    kws_model.h
 * @brief   KWS DS-CNN 模型推理接口 — STEdgeAI v4.0 生成, INT8 QLinear
 *
 * [2026-07-04]: STEdgeAI 4.0 生成自 kws_v5_ds-cnn_int8_fixed.onnx。
 *   输入:  uint8[40][32] = 1280 B, QLinear(scale=0.0393, zp=128)
 *   输出:  float[4] softmax — 救命 / 帮帮我 / silence / unknown
 *   权重:  14.5 KB, 激活: 22.1 KB, ~991K MACCs
 */

#ifndef KWS_MODEL_H
#define KWS_MODEL_H

#include <stdint.h>

/* 模型输入/输出尺寸 */
#define KWS_MODEL_IN_SIZE   1280   /* uint8[40][32] flat */
#define KWS_MODEL_OUT_SIZE  4      /* 4 类: 救命, 帮帮我, silence, unknown */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 KWS 模型（创建 STEdgeAI 网络，加载权重）
 * @return 0 成功, -1 失败
 */
int kws_model_init(void);

/**
 * @brief 运行 KWS 推理
 * @param mfcc_in   [1280] int8 量化后的 MFCC 特征 (flat: frame × coeff)
 * @param probs_out [4] float softmax 概率输出
 * @return 0 成功, <0 失败
 */
int kws_model_run(const int8_t *mfcc_in, float probs_out[4]);

#ifdef __cplusplus
}
#endif

#endif /* KWS_MODEL_H */
