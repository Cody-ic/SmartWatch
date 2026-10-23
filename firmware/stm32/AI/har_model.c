/**
 * @file    har_model.c
 * @brief   HAR v13 INT8 — STEdgeAI v4.0.1 stai API
 *
 * [2026-07-12 Update]: v6→v13。har_network_* 函数名加前缀防符号冲突;类型(stai_network等)共用stai.h。
 */

#include "har_model.h"
#include "har_network.h"
#include "har_network_data.h"
#include <string.h>
#include <math.h>

static stai_network g_har_network[HAR_NETWORK_CONTEXT_SIZE] __attribute__((aligned(16)));
static uint8_t g_har_activations[HAR_NETWORK_ACTIVATIONS_SIZE] __attribute__((aligned(16)));
static int g_har_ready = 0;

int har_model_init(void)
{
    stai_return_code ret = har_network_init(g_har_network);
    if (ret != STAI_SUCCESS) return -1;

    const stai_ptr weights[] = { (stai_ptr)g_har_network_weights_array };
    ret = har_network_set_weights(g_har_network, weights, 1);
    if (ret != STAI_SUCCESS) return -1;

    const stai_ptr activations[] = { g_har_activations };
    ret = har_network_set_activations(g_har_network, activations, 1);
    if (ret != STAI_SUCCESS) return -1;

    g_har_ready = 1;
    return 0;
}

int har_model_run(const uint8_t *q_input, uint8_t q_output[HAR_MODEL_OUT_SIZE])
{
    if (!g_har_ready) return -1;
    if (!q_input || !q_output) return -1;

    stai_ptr inputs[1];
    stai_size n_inputs = 1;
    har_network_get_inputs(g_har_network, inputs, &n_inputs);
    if (!inputs[0]) return -1;
    memcpy(inputs[0], q_input, HAR_MODEL_IN_SIZE);

    stai_return_code ret = har_network_run(g_har_network, STAI_MODE_SYNC);
    if (ret != STAI_SUCCESS) return -2;

    stai_ptr outputs[1];
    stai_size n_outputs = 1;
    har_network_get_outputs(g_har_network, outputs, &n_outputs);
    if (!outputs[0]) return -3;
    memcpy(q_output, outputs[0], HAR_MODEL_OUT_SIZE);

    return 0;
}

void har_model_get_info(uint32_t *flash_used, uint32_t *ram_used)
{
    if (flash_used) *flash_used = 96956;
    if (ram_used)   *ram_used   = 24448;
}

/* [2026-07-14]: 暴露网络句柄供 har_embedding 注册 stai 回调 */
stai_network *har_model_get_network(void)
{
    return g_har_network;
}

/* [2026-07-14]: 从 HAR v13 分类器权重推算通用原型
 *
 * 原理: classifier_2 (128→64) + classifier_4 (64→7) 等效于
 *   logit[c] = W2[c] @ ReLU(W1 @ x + b1) + b2[c].
 * 忽略 ReLU (GAP 后特征以正值为主), 128 维空间内类 c 的"理想方向"为:
 *   proto[c] = W1^T @ W2[c]^T   (128×64 @ 64×1 = 128×1)
 * 即 P = W2 @ W1  (7×64 @ 64×128 = 7×128).
 * L2 归一化后作为 cosine 距离原型。
 *
 * 权重布局 (从 har_network.c:2193-2218 提取, STEdgeAI v4.0.1 生成):
 *   W1[64][128] @ offset 62112 (float32, row-major)
 *   b1[64]      @ offset 94880
 *   W2[7][64]   @ offset 95136 (float32, row-major)
 *   b2[7]       @ offset 96928
 *   总大小 96956 B
 *
 * 计算量: 7×64×128 = 57,344 FMAC + 7×128 L2-norm, M33 FPU 单周期, ~0.6ms @160MHz
 */
#define W1_OFFSET  62112   /* float32[64][128] */
#define W2_OFFSET  95136   /* float32[7][64]   */

void har_model_extract_prototypes(float prototypes[7][128])
{
    const float *weights = (const float *)g_har_network_weights_array;
    const float *W1 = weights + W1_OFFSET / 4;  /* 64×128, row-major: W1[k][j] @ offset k*128+j */
    const float *W2 = weights + W2_OFFSET / 4;  /* 7×64,   row-major: W2[c][k] @ offset c*64+k  */

    /* P = W2 @ W1: proto[c][j] = sum_k W2[c][k] * W1[k][j] */
    for (int c = 0; c < 7; c++) {
        for (int j = 0; j < 128; j++) {
            float dot = 0.0f;
            for (int k = 0; k < 64; k++) {
                dot += W2[c * 64 + k] * W1[k * 128 + j];
            }
            prototypes[c][j] = dot;
        }
    }

    /* L2 归一化每类原型 */
    for (int c = 0; c < 7; c++) {
        float norm = 0.0f;
        for (int j = 0; j < 128; j++) norm += prototypes[c][j] * prototypes[c][j];
        if (norm > 1e-8f) {
            norm = 1.0f / sqrtf(norm);
            for (int j = 0; j < 128; j++) prototypes[c][j] *= norm;
        }
    }
}
