/**
 * @file    har_embedding.h
 * @brief   从 HAR v13 中间层提取 128 维 L2 归一化 embedding
 *
 * [2026-07-14] 新增。通过 stai 事件回调在 node 164 (classifier_2 Dense 128→64)
 *   START 时捕获 GAP 转换后的 float32[128] 特征向量，L2 归一化后供 proto_model 使用。
 *
 * 用法:
 *   1. har_embedding_init(network) — 注册回调 (在 har_model_init 之后调用)
 *   2. 每次 har_network_run() 后调用 har_embedding_get(out) 读取最新 embedding
 *
 * 回调触发时序: node 158 conversion STOP → node 164 START → node 164 STOP
 *   我们从 node 164 START 捕获 (buffers[0] = activations[0]+0, float32[128])
 */

#ifndef HAR_EMBEDDING_H
#define HAR_EMBEDDING_H

#include "stai.h"
#include <stdint.h>

#define HAR_EMBEDDING_DIM  128

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 注册 stai 回调，开始捕获 embedding
 * @param network 已初始化的 HAR 网络句柄 (har_model 内部 g_har_network)
 */
void har_embedding_init(stai_network *network);

/**
 * @brief 每轮推理前调用, 清零 ready 标志, 防止消费陈旧 embedding
 */
void har_embedding_reset(void);

/**
 * @brief 获取最近一次推理的 128 维 L2 归一化 embedding
 * @param out [128] float32 输出缓冲区
 * @return 0 成功, -1 回调未触发 (本轮推理 node 164 未到达)
 */
int har_embedding_get(float out[HAR_EMBEDDING_DIM]);

#ifdef __cplusplus
}
#endif

#endif /* HAR_EMBEDDING_H */
