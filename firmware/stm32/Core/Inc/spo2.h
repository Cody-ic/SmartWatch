/**
 * @file spo2.h
 * @brief SpO2 血氧估算 v2 — Maxim MAXREFDES117 心跳分段法
 *
 * 算法: 谷值检测(IR AC) → 逐心跳分割 → DC(区间max) + AC(峰-插值基线)
 *       → R = (AC_red/DC_red) / (AC_ir/DC_ir) → 中位数R → SpO2
 *
 * v2 升级 (2026-07-05 消融实验: 噪声场景精度 +6.4%):
 *   - 逐心跳 DC(插值基线) 替代固定 EMA DC
 *   - 中位数 R 替代单次 R
 *   - 保留 EMA DC 作为辅助/冷启动
 *
 * 调用方式: spo2_feed() 每样本喂入双通道, spo2_get() 读取。
 *   无外部依赖。直接 include 即可使用。
 *
 * 内存: ~3.3KB (ring[800] + a few arrays)
 */

#ifndef SPO2_H
#define SPO2_H

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════ 校准系数 — Maxim 官方二次曲线 ═══════ */
#ifndef SPO2_A
#define SPO2_A  -45.060f
#endif
#ifndef SPO2_B
#define SPO2_B   30.354f
#endif
#ifndef SPO2_C
#define SPO2_C   94.845f
#endif

/* ═══════ 心跳分段参数 ═══════ */
#define SPO2_RING_SIZE    400    /* ~4s @ 100Hz, 至少 2 个完整心跳 */
#define SPO2_MAX_RATIOS   5      /* 中位数 R 的最大采样数 */
#define SPO2_DC_ALPHA     0.005f /* EMA α: fc≈0.08Hz */

/* 谷检测: 最小间隔 15 样本 (~25 样本/心跳 @ 60BPM/100Hz) */
#define SPO2_VALLEY_GAP   15
/* 有效心跳间隔: 25~200 样本 (30~240 BPM) */
#define SPO2_BEAT_MIN     25
#define SPO2_BEAT_MAX     200


typedef struct {
    /* AC 环形缓冲 */
    float  red_ac_ring[SPO2_RING_SIZE];
    float  ir_ac_ring[SPO2_RING_SIZE];
    uint16_t ring_idx;
    uint16_t ring_count;

    /* 谷值位置 (环形缓冲内) */
    uint16_t valleys[10];
    uint8_t  valley_count;

    /* R 比值采样 → 中位数 */
    float  ratios[SPO2_MAX_RATIOS];
    uint8_t ratio_count;

    /* EMA DC (辅助 + 冷启动种子) */
    float  red_dc;
    float  ir_dc;
    uint8_t dc_seeded;

    /* 输出 */
    float   spo2;
    uint8_t valid;

    uint32_t last_compute_ms;
    uint32_t last_valid_ms;   /* [2026-07-07]: valid 超时复位用 */
} spo2_state_t;


/**
 * @brief 初始化 SpO2 状态
 */
static inline void spo2_init(spo2_state_t *s)
{
    memset(s, 0, sizeof(*s));
    s->red_dc = 0.0f;
    s->ir_dc  = 0.0f;
}


/**
 * @brief 喂入一对 PPG 样本 (红光 + 红外原始值)
 */
/* Call even when the sensor produces no samples. */
static inline void spo2_expire(spo2_state_t *s, uint32_t now)
{
    if (s->valid && (uint32_t)(now - s->last_valid_ms) > 5000) {
        s->valid = 0;
        s->ring_count = 0;
        s->dc_seeded = 0;
    }
}

static inline void spo2_feed(spo2_state_t *s, uint32_t red, uint32_t ir,
                              uint32_t now)
{
    spo2_expire(s, now);
    float r = (float)red;
    float i = (float)ir;

    /* 1. EMA DC: 首样本种子, 避免冷启动误算 */
    if (!s->dc_seeded) {
        s->red_dc = r;
        s->ir_dc  = i;
        s->dc_seeded = 1;
    } else {
        s->red_dc = (1.0f - SPO2_DC_ALPHA) * s->red_dc + SPO2_DC_ALPHA * r;
        s->ir_dc  = (1.0f - SPO2_DC_ALPHA) * s->ir_dc  + SPO2_DC_ALPHA * i;
    }

    /* 2. 存储 AC 分量到环形缓冲 */
    uint16_t idx = s->ring_idx;
    s->red_ac_ring[idx] = r - s->red_dc;
    s->ir_ac_ring[idx]  = i - s->ir_dc;
    s->ring_idx = (idx + 1) % SPO2_RING_SIZE;
    if (s->ring_count < SPO2_RING_SIZE)
        s->ring_count++;

    /* 3. 每 2s 触发一次心跳分析 */
    if (s->ring_count < SPO2_RING_SIZE) return;
    if ((uint32_t)(now - s->last_compute_ms) < 2000) return;
    s->last_compute_ms = now;

    /* 4. 谷检测: 找 IR AC 信号的局部最小值 (舒张期) */
    uint16_t n = s->ring_count;
    uint16_t start = (s->ring_idx + SPO2_RING_SIZE - n) % SPO2_RING_SIZE;

    /* 计算振幅用于阈值 */
    float ir_min = 1e9f, ir_max = -1e9f;
    for (uint16_t j = 0; j < n; j++) {
        float v = s->ir_ac_ring[(start + j) % SPO2_RING_SIZE];
        if (v < ir_min) ir_min = v;
        if (v > ir_max) ir_max = v;
    }
    float ir_amp = ir_max - ir_min;
    if (ir_amp < 50.0f) return;  /* 信号太弱 */

    float valley_thresh = -ir_amp * 0.3f;

    s->valley_count = 0;
    uint16_t last_valley = 0;
    for (uint16_t j = 5; j < n - 5; j++) {
        uint16_t pos = (start + j) % SPO2_RING_SIZE;
        uint16_t p1  = (start + j - 1) % SPO2_RING_SIZE;
        uint16_t p2  = (start + j - 2) % SPO2_RING_SIZE;
        uint16_t n1  = (start + j + 1) % SPO2_RING_SIZE;
        uint16_t n2  = (start + j + 2) % SPO2_RING_SIZE;

        float v  = s->ir_ac_ring[pos];
        if (v < s->ir_ac_ring[p1] && v < s->ir_ac_ring[p2] &&
            v < s->ir_ac_ring[n1] && v < s->ir_ac_ring[n2] &&
            v < valley_thresh) {

            /* 非极大值抑制: 同一谷保留最深的 */
            if (s->valley_count == 0 || j - last_valley >= SPO2_VALLEY_GAP) {
                s->valleys[s->valley_count++] = pos;
                last_valley = j;
                if (s->valley_count >= 10) break;
            } else {
                uint16_t prev = s->valleys[s->valley_count - 1];
                if (v < s->ir_ac_ring[prev])
                    s->valleys[s->valley_count - 1] = pos;
            }
        }
    }

    if (s->valley_count < 3) return;  /* 需要至少 3 个谷 */

    /* 5. 逐心跳计算 R */
    s->ratio_count = 0;
    for (uint8_t k = 0; k < s->valley_count - 1; k++) {
        uint16_t va = s->valleys[k];
        uint16_t vb = s->valleys[k + 1];

        /* 心跳间隔检查 */
        int32_t gap = (int32_t)vb - (int32_t)va;
        if (gap < 0) gap += SPO2_RING_SIZE;
        if (gap < SPO2_BEAT_MIN || gap > SPO2_BEAT_MAX) continue;

        /* DC: 区间内 IR 最大值 */
        float ir_dc_beat = -1e9f, red_dc_beat = -1e9f;
        int32_t ir_max_pos = 0, red_max_pos = 0;
        int32_t pos_in_ring = 0;
        for (int32_t d = 0; d < gap; d++) {
            pos_in_ring = (va + d) % SPO2_RING_SIZE;
            float ir_v = s->ir_ac_ring[pos_in_ring];
            float red_v = s->red_ac_ring[pos_in_ring];
            if (ir_v > ir_dc_beat)  { ir_dc_beat = ir_v;  ir_max_pos = d; }
            if (red_v > red_dc_beat) { red_dc_beat = red_v; red_max_pos = d; }
        }
        /* DC_beat = EMA_DC + AC_peak (还原到原始信号域) */
        ir_dc_beat  += s->ir_dc;
        red_dc_beat += s->red_dc;

        /* AC: 峰值 - 插值基线 */
        float ir_bl  = s->ir_ac_ring[va]  + (s->ir_ac_ring[vb]  - s->ir_ac_ring[va])  * ir_max_pos  / gap;
        float red_bl = s->red_ac_ring[va] + (s->red_ac_ring[vb] - s->red_ac_ring[va]) * red_max_pos / gap;
        float ir_ac_beat  = s->ir_ac_ring[(va + ir_max_pos)  % SPO2_RING_SIZE] - ir_bl;
        float red_ac_beat = s->red_ac_ring[(va + red_max_pos) % SPO2_RING_SIZE] - red_bl;

        if (ir_ac_beat < 50.0f || red_ac_beat < 20.0f) continue;
        if (ir_dc_beat < 1000.0f || red_dc_beat < 1000.0f) continue;

        float r_ratio = (red_ac_beat / red_dc_beat) / (ir_ac_beat / ir_dc_beat);
        if (r_ratio > 0.3f && r_ratio < 2.0f) {
            s->ratios[s->ratio_count++] = r_ratio;
            if (s->ratio_count >= SPO2_MAX_RATIOS) break;
        }
    }

    if (s->ratio_count < 2) return;

    /* 6. 中位数 R → SpO2 */
    /* 冒泡排序 (5 个元素, O(25), 足够快) */
    for (uint8_t i = 0; i < s->ratio_count - 1; i++)
        for (uint8_t j = i + 1; j < s->ratio_count; j++)
            if (s->ratios[i] > s->ratios[j]) {
                float tmp = s->ratios[i];
                s->ratios[i] = s->ratios[j];
                s->ratios[j] = tmp;
            }
    float r_med = s->ratios[s->ratio_count / 2];

    float s_val;
    if (r_med < 1.0f) {
        s_val = SPO2_A * r_med * r_med + SPO2_B * r_med + SPO2_C;
    } else {
        s_val = 110.0f - 25.0f * r_med;
    }
    if (s_val < 70.0f)  s_val = 0.0f;
    if (s_val > 100.0f) s_val = 100.0f;

    s->spo2  = s_val;
    s->valid = 1;
    s->last_valid_ms = now;  /* [2026-07-07]: 记录有效时刻, 供超时检查 */
}


static inline float spo2_get(const spo2_state_t *s)
{
    return s->valid ? s->spo2 : 0.0f;
}

static inline int spo2_is_valid(const spo2_state_t *s)
{
    return s->valid;
}

#ifdef __cplusplus
}
#endif

#endif /* SPO2_H */
