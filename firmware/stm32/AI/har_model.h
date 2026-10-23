/**
 * @file    har_model.h
 * @brief   HAR v13 INT8 推理接口 — STEdgeAI v4.0.1 generate, stai API
 *
 * [2026-07-12 Update]: v6(legacy)→v13(stai)。STEdgeAI v4.0.1 从 har_v13_seed42_int8.onnx 生成。
 *   API 从 legacy ai_platform.h → stai.h。与 KWS 同运行时。
 *   输入:  uint8[6][512] = 3KB, QLinear(scale=1.02747, zp=104)
 *   输出:  uint8[7], QLinear(scale=0.09789, zp=113)
 *   权重:  94.68 KB, 激活: 23.88 KB, 11.1M MACCs, 8-fold LOSO F1=0.860
 *
 * 生成: 2026-07-12, ST Edge AI Core v4.0.1-20581, target=stm32u5
 */

#ifndef HAR_MODEL_H
#define HAR_MODEL_H

#include <stdint.h>

/* 尺寸常量 */
#define HAR_MODEL_IN_SIZE   (6 * 512)   /* uint8 flat = 3072 */
#define HAR_MODEL_OUT_SIZE   7            /* uint8[7] logits */
#define HAR_NUM_CHANNELS     6
#define HAR_WINDOW_SIZE      512
#define HAR_NUM_CLASSES      7

#ifdef __cplusplus
extern "C" {
#endif

int  har_model_init(void);
int  har_model_run(const uint8_t *q_input, uint8_t q_output[HAR_MODEL_OUT_SIZE]);
void har_model_get_info(uint32_t *flash_used, uint32_t *ram_used);

/* [2026-07-14]: 暴露网络句柄供 har_embedding 注册 stai 回调 */
#include "stai.h"
stai_network *har_model_get_network(void);

/* [2026-07-14]: 从分类器权重推算 7 类通用原型 (7×128 float32, L2 归一化) */
void har_model_extract_prototypes(float prototypes[7][128]);

#ifdef __cplusplus
}
#endif

#endif /* HAR_MODEL_H */
