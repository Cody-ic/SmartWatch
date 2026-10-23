/**
 * @file    kws_model.c
 * @brief   KWS DS-CNN 模型推理 — STEdgeAI v4.0 生成, INT8 QLinear
 *
 * [2026-07-04]: STEdgeAI 4.0 生成自 kws_v5_ds-cnn_int8_fixed.onnx。
 *   输入:  uint8[1][40][32][1] = 1280 B, QLinear(scale=0.0393, zp=128)
 *   输出:  uint8[1][4] = 4 B, QLinear(scale=0.0417, zp=106)
 *   权重:  14.5 KB, 激活: 22.1 KB, 总 RAM: 22.1 KB
 */

#include "kws_model.h"
#include "network.h"
#include "network_data.h"
#include <string.h>

/* STEdgeAI 网络句柄及缓冲区
 * [2026-07-05 FIX]: stai_network = uint8_t(1B), g_kws_network 原是单字节。
 *   stai_network_init 向 _stai_network_context(~48B) 写入 → 缓冲区溢出。
 *   改为 sizeof(_stai_network_context) 的数组, 删除冗余 g_kws_context。
 *   交叉验证: stai.h:246 类型定义 + network.h:155 ST 官方注释 "allocated as byte array" */
static stai_network g_kws_network[STAI_NETWORK_CONTEXT_SIZE] __attribute__((aligned(16)));
static uint8_t g_kws_activations[STAI_NETWORK_ACTIVATIONS_SIZE] __attribute__((aligned(16)));
static int g_kws_ready = 0;

int kws_model_init(void)
{
    stai_return_code ret = stai_network_init(g_kws_network);
    if (ret != STAI_SUCCESS) return -1;

    /* [2026-07-04 Fix]: 补全 stai_network_set_weights + set_activations,
     *   STAI v1.0 要求在 init 后显式设置权重和激活缓冲区,
     *   否则推理时网络缺少权重/激活指针, 输出全零或 HardFault。 */
    const stai_ptr weights[] = { (stai_ptr)g_network_weights_array };
    ret = stai_network_set_weights(g_kws_network, weights, 1);
    if (ret != STAI_SUCCESS) return -1;

    const stai_ptr activations[] = { g_kws_activations };
    ret = stai_network_set_activations(g_kws_network, activations, 1);
    if (ret != STAI_SUCCESS) return -1;

    g_kws_ready = 1;
    return 0;
}

int kws_model_run(const int8_t *mfcc_in, float probs_out[4])
{
    if (!g_kws_ready) return -1;
    if (!mfcc_in || !probs_out) return -1;

    /* 1. 拷贝量化后的 MFCC 特征到模型输入缓冲区 */
    stai_ptr inputs[1];
    stai_size n_inputs = 1;
    /* [2026-07-04 Fix]: stai_network_get_inputs 签名需要 3 个参数,
     *   第 3 个为 stai_size* n_inputs 输出参数, 缺失导致编译错误。 */
    stai_network_get_inputs(g_kws_network, inputs, &n_inputs);
    if (!inputs[0]) return -1;
    memcpy(inputs[0], mfcc_in, KWS_MODEL_IN_SIZE);  /* 1280 B */

    /* 2. 运行推理 */
    /* [2026-07-04 Fix]: stai_network_run 需要第 2 个参数 stai_run_mode,
     *   传入 STAI_MODE_SYNC 做同步推理, 缺失导致编译错误。 */
    stai_return_code ret = stai_network_run(g_kws_network, STAI_MODE_SYNC);
    if (ret != STAI_SUCCESS) return -2;

    /* 3. 获取输出并反量化 */
    stai_ptr outputs[1];
    stai_size n_outputs = 1;
    /* [2026-07-04 Fix]: stai_network_get_outputs 签名需要 3 个参数,
     *   第 3 个为 stai_size* n_outputs 输出参数, 缺失导致编译错误。 */
    stai_network_get_outputs(g_kws_network, outputs, &n_outputs);
    if (!outputs[0]) return -3;

    /* 输出 QLinear: scale=0.041678678, zp=106, uint8
     * 反量化: prob = (q - zp) * scale */
    uint8_t *q_out = (uint8_t *)outputs[0];
    for (int i = 0; i < 4; i++)
        probs_out[i] = ((float)q_out[i] - 106.0f) * 0.041678678f;

    return 0;
}
