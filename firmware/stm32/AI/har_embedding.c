/**
 * @file    har_embedding.c
 * @brief   从 HAR v13 中间层提取 128 维 L2 归一化 embedding 的实现
 *
 * [2026-07-14] 新增。注册 stai 事件回调, 在 node 164 (classifier_2 Dense 128→64)
 *   START 时从 activations[0]+0 拷贝 float32[128], 立即 L2 归一化。
 *
 * 设计要点:
 *   - 激活池共享, 数据仅在回调内有效 → 必须在回调中拷贝, 不能事后读
 *   - node 164 START 的 buffers[0] = activations[0]+0 = float32[128]
 *     与 node 158 (conversion STOP) 是同一块内存, 取 164 是因为 node_id 唯一无歧义
 *   - L2 归一化在回调内完成, 128 维 ≈256 FLOPs, M33 FPU 单周期乘加, 开销可忽略
 *   - M33 FPU 支持非规格化数 DAZ/FTZ
 */

#include "har_embedding.h"
#include "har_network.h"
#include "stai_events.h"
#include <string.h>
#include <math.h>

/* ---- 捕获的 embedding ---- */
static float g_embedding[HAR_EMBEDDING_DIM];
static volatile int g_embedding_ready = 0;

/* ---- L2 归一化 (in-place) ---- */
static void l2_normalize_ip(float *v, int n)
{
    float norm = 0.0f;
    for (int i = 0; i < n; i++) norm += v[i] * v[i];
    if (norm > 1e-8f) {
        norm = 1.0f / sqrtf(norm);
        for (int i = 0; i < n; i++) v[i] *= norm;
    }
}

/* ---- stai 事件回调 ---- */
static void embedding_callback(
    void *cb_cookie,
    const stai_event_type event_type,
    const void *event_payload)
{
    (void)cb_cookie;

    /* 只监听 NODE_START: node 164 = classifier_2 (Dense 128→64) 的输入 */
    if (event_type != STAI_EVENT_NODE_START) return;

    const stai_event_node_start_stop *evt =
        (const stai_event_node_start_stop *)event_payload;

    if (evt->node_id != 164) return;

    /* buffers.data[0] → activations[0]+0 → float32[128]
     * 交叉验证: har_network.c:2203:
     *   _STAI_NETWORK_EVENT_NODE_START_CB(164, 1,
     *     {(stai_ptr)(float*)(net_ctx->_activations[0] + 0)});
     * 此即 GAP→conversion 后 128 维 float32 特征向量。 */
    const float *src = (const float *)(evt->buffers.data[0]);
    memcpy(g_embedding, src, HAR_EMBEDDING_DIM * sizeof(float));

    /* [2026-07-14]: NaN/Inf/异常值防护 — `!(v>1e10 && v<1e10)` 同时捕获 NaN
     *   (任何比较返回 false)、Inf (>1e10) 和极端值, 拒绝后不污染 proto/累加器 */
    int bad = 0;
    for (int i = 0; i < HAR_EMBEDDING_DIM; i++) {
        float v = g_embedding[i];
        if (!(v > -1e10f && v < 1e10f)) { bad = 1; break; }
    }
    if (bad) { g_embedding_ready = 0; return; }

    /* 立即 L2 归一化 — proto_model 的 cosine_sim 依赖输入归一化 */
    l2_normalize_ip(g_embedding, HAR_EMBEDDING_DIM);
    g_embedding_ready = 1;
}

/* ---- 公共 API ---- */

void har_embedding_init(stai_network *network)
{
    g_embedding_ready = 0;
    memset(g_embedding, 0, sizeof(g_embedding));

    har_network_set_callback(network, embedding_callback, NULL);
}

/* [2026-07-14]: 每轮推理前清零 — 若回调未触发 (推理失败/node 164 未到达),
 *   har_embedding_get 返回 -1, 消费端据此跳过 proto/session, 不喂陈旧数据 */
void har_embedding_reset(void)
{
    g_embedding_ready = 0;
}

int har_embedding_get(float out[HAR_EMBEDDING_DIM])
{
    if (!g_embedding_ready) return -1;

    memcpy(out, g_embedding, HAR_EMBEDDING_DIM * sizeof(float));
    return 0;
}
