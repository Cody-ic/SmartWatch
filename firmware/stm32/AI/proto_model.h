/**
 * @file proto_model.h
 * @brief Prototypical Network — 128-dim prototype storage + cosine distance inference
 *
 * [2026-07-12]: 部署交付包 proto_har_v3 (Pretrain_Cosine_Aug, 7-class, 128-dim).
 *   BLE proto backup/restore 传输原型向量, 本模块存储并执行 on-device 推理。
 *   原型: 7 × 128 float = 3584B (BSS).
 *
 * ## L2-归一化契约
 *   - proto_model_set() 传入的原型必须已 L2-归一化 (‖p‖=1)。
 *   - proto_model_predict() / proto_model_predict_probs() 传入的 embedding
 *     也必须已 L2-归一化。cosine_sim 直接做内积, 不做归一化。
 *   - 手机端 (Flutter APP) 下发原型前需自行归一化; MCU 端调用方
 *     (如 HAR embedding extractor) 也需先归一化再传入。
 */

#ifndef PROTO_MODEL_H
#define PROTO_MODEL_H

#include <stdint.h>

#define PROTO_N_CLASSES  7
#define PROTO_DIM        128

#ifdef __cplusplus
extern "C" {
#endif

void proto_model_init(void);
int  proto_model_is_loaded(void);
uint8_t proto_model_get_version(void);  /* [2026-07-14]: 轻量, 不拷贝 3584B 原型 */

/** 设置原型 (从 BLE proto restore, 手机下发).
 *  @param prototypes 7×128 float, 必须已 L2-归一化 (‖p‖=1) */
void proto_model_set(const float prototypes[PROTO_N_CLASSES][PROTO_DIM], uint8_t version);
/** [2026-07-14]: 零拷贝版 — 直接从 BLE payload 字节写入, 免栈上 3584B 拷贝 */
/* Returns 1 on commit, 0 for invalid data (previous model is preserved). */
int proto_model_set_raw(const uint8_t data[PROTO_N_CLASSES * PROTO_DIM * 4], uint8_t version);
void proto_model_get(float prototypes[PROTO_N_CLASSES][PROTO_DIM], uint8_t *version);
/** [2026-07-14]: 零拷贝版 — 返回内部存储指针, 免栈上 3584B 拷贝 */
const float (*proto_model_get_ptr(void))[PROTO_DIM];

/** cosine-distance argmax: 返回 (0..6) 最近原型的类别 ID.
 *  @param embedding 128-dim float, 必须已 L2-归一化 (‖v‖=1) */
int  proto_model_predict(const float *embedding);
/** softmax 概率输出 (temperature=10).
 *  @param embedding 128-dim float, 必须已 L2-归一化 (‖v‖=1) */
void proto_model_predict_probs(const float *embedding, float probs[PROTO_N_CLASSES]);

#ifdef __cplusplus
}
#endif

#endif
