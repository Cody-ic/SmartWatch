/**
 * @file    sleep_apnea.c
 * @brief   睡眠呼吸暂停初筛 — 算法实现
 *
 * 状态机驱动, 基于 SpO2 3% ODI 检测。
 * [2026-07-05] 新增
 */

#include "sleep_apnea.h"
#include <string.h>

/* 全局单例 — UI/BLE/app_tick 共享 */
sleep_apnea_state_t g_apnea;

/* ============================================================
 * 内部辅助
 * ============================================================ */

/** @brief 根据 ODI 值查表分级 */
static sleep_apnea_severity_e classify_odi(float odi)
{
    if (odi < 5.0f)   return ODI_NORMAL;
    if (odi < 15.0f)  return ODI_MILD;
    if (odi < 30.0f)  return ODI_MODERATE;
    return ODI_SEVERE;
}

/* ============================================================
 * 公共 API
 * ============================================================ */

void sleep_apnea_init(sleep_apnea_state_t *s)
{
    memset(s, 0, sizeof(*s));
    s->state       = APNEA_IDLE;
    s->severity    = ODI_NORMAL;
    s->spo2_baseline = 0.0f;
    s->lowest_spo2   = 100.0f;
}


void sleep_apnea_tick(sleep_apnea_state_t *s, float p_sleep, float spo2,
                      uint8_t spo2_valid, uint32_t now_s)
{
    /* 首次调用: 记录时间基准 */
    if (s->last_tick_s == 0) {
        s->last_tick_s = now_s;
        return;
    }
    s->last_tick_s = now_s;

    /* ============================================================
     * 1. 睡眠状态去抖
     * ============================================================ */
    if (p_sleep > 0.5f) {
        s->sleep_epoch_cnt++;
        s->wake_epoch_cnt = 0;
    } else {
        s->wake_epoch_cnt++;
        s->sleep_epoch_cnt = 0;
    }

    uint8_t was_asleep = s->is_asleep;

    if (s->sleep_epoch_cnt >= APNEA_SLEEP_DEBOUNCE_S) {
        s->is_asleep = 1;
    } else if (s->wake_epoch_cnt >= APNEA_WAKE_DEBOUNCE_S) {
        s->is_asleep = 0;
    }

    /* ============================================================
     * 2. 睡眠开始/结束 → 状态重置
     * ============================================================ */
    if (!was_asleep && s->is_asleep) {
        /* 入睡: 重置整晚统计, 进入基线建立期 */
        s->state          = APNEA_BASELINE;
        s->sleep_start_s  = now_s;
        s->baseline_start_s = now_s;
        s->total_events   = 0;
        s->total_sleep_s  = 0;
        s->total_monitor_s = 0;
        s->lowest_spo2    = 100.0f;
        s->sum_desat_dur_s = 0.0f;
        s->spo2_baseline  = 0.0f;
        s->in_desat       = 0;
        s->odi            = 0.0f;
        s->severity       = ODI_NORMAL;
        s->event_idx      = 0;

        /* 清空事件环缓 */
        for (uint8_t i = 0; i < APNEA_MAX_EVENTS; i++) {
            s->event_durations[i] = 0;
            s->event_min_spo2[i]  = 0.0f;
        }
    } else if (was_asleep && !s->is_asleep) {
        /* 醒来: 结束监测, 最终 ODI 已在上次 tick 计算 */
        s->state = APNEA_IDLE;
        s->night_just_ended = 1;  /* 上升沿, 供 app_tick 消费后清零 */
    }

    /* ============================================================
     * 3. 仅在睡眠期间继续
     * ============================================================ */
    if (!s->is_asleep) return;

    s->total_sleep_s = now_s - s->sleep_start_s;

    /* SpO2 无效时保持状态但不更新.
     * [2026-07-05]: spo2 <= 50 也视为无效 — spo2.h 将 <70% 截断为 0,
     *   此时 valid=1 但值不可用, 需防止误触发降氧事件 (基线-0=97>>3). */
    if (!spo2_valid || spo2 <= 50.0f) return;

    s->spo2_current = spo2;
    s->spo2_valid   = 1;
    if (spo2 < s->lowest_spo2) s->lowest_spo2 = spo2;

    /* ============================================================
     * 4. 状态机 — 核心检测逻辑
     * ============================================================ */
    switch (s->state) {

    case APNEA_BASELINE: {
        /* 基线建立期: 快速 EMA, τ≈100s */
        if (s->spo2_baseline < 50.0f) {
            s->spo2_baseline = spo2;  /* 首样本直接设基线 */
        } else {
            s->spo2_baseline = (1.0f - APNEA_BASELINE_ALPHA) * s->spo2_baseline
                             + APNEA_BASELINE_ALPHA * spo2;
        }

        /* 基线期结束 → 进入监测 */
        if (now_s - s->baseline_start_s >= APNEA_BASELINE_SEC) {
            s->state = APNEA_MONITORING;
            s->total_monitor_s = 0;
        }
        break;
    }

    case APNEA_MONITORING:
    case APNEA_DESAT: {
        /* 监测时间 (不含基线期) */
        s->total_monitor_s = now_s - s->baseline_start_s - APNEA_BASELINE_SEC;

        /* [2026-07-05 FIX]: 降氧检测必须在基线更新之前 — 否则 EMA 每次吸收
         *   当前值后 drop 缩小 α×Δ, 恰好 3% 的持续降氧永远达不到阈值.
         *   先检测 (用旧基线), 再更新基线. */
        float drop = s->spo2_baseline - spo2;

        /* 慢速基线更新, τ≈500s, 不会被降氧事件大幅度拉偏 */
        s->spo2_baseline = (1.0f - APNEA_MONITOR_ALPHA) * s->spo2_baseline
                         + APNEA_MONITOR_ALPHA * spo2;

        if (drop >= APNEA_ODI_THRESHOLD) {
            /* 进入/持续降氧事件 */
            if (!s->in_desat) {
                s->in_desat       = 1;
                s->desat_start_s  = now_s;
                s->desat_min_spo2 = spo2;
                s->state          = APNEA_DESAT;
            } else {
                /* 持续中: 追踪最低点 */
                if (spo2 < s->desat_min_spo2)
                    s->desat_min_spo2 = spo2;
            }
        } else if (s->in_desat) {
            /* 降氧事件结束 */
            uint32_t dur = now_s - s->desat_start_s;

            if (dur >= (uint32_t)APNEA_MIN_EVENT_SEC) {
                /* 有效事件: 记录到环缓 */
                s->total_events++;
                s->sum_desat_dur_s += (float)dur;

                s->event_durations[s->event_idx] = (uint16_t)dur;
                s->event_min_spo2[s->event_idx]  = s->desat_min_spo2;
                s->event_idx = (s->event_idx + 1) % APNEA_MAX_EVENTS;
            }
            /* else: 持续时间不足 10s, 丢弃 (可能是运动伪差或短暂屏气) */

            s->in_desat = 0;
            s->state    = APNEA_MONITORING;
        }

        /* 每分钟重算 ODI (监测 ≥60s 才有效) */
        if (s->total_monitor_s >= 60) {
            float hours = (float)s->total_monitor_s / 3600.0f;
            s->odi      = (float)s->total_events / hours;
            s->severity = classify_odi(s->odi);
        }
        break;
    }

    default:
        break;
    }
}


/* ---- 结果读取 ---- */

float sleep_apnea_get_odi(const sleep_apnea_state_t *s)
{
    return s->odi;
}

sleep_apnea_severity_e sleep_apnea_get_severity(const sleep_apnea_state_t *s)
{
    return s->severity;
}

uint16_t sleep_apnea_get_events(const sleep_apnea_state_t *s)
{
    return s->total_events;
}

float sleep_apnea_get_lowest_spo2(const sleep_apnea_state_t *s)
{
    return s->lowest_spo2;
}

uint32_t sleep_apnea_get_monitor_time(const sleep_apnea_state_t *s)
{
    return s->total_monitor_s;
}

uint8_t sleep_apnea_is_asleep(const sleep_apnea_state_t *s)
{
    return s->is_asleep;
}

float sleep_apnea_get_last_event_spo2(const sleep_apnea_state_t *s)
{
    if (s->total_events == 0) return 0.0f;

    /* event_idx 指向下一个写入位, 最新事件在 (event_idx - 1) % MAX */
    uint8_t last = (s->event_idx == 0)
                   ? (uint8_t)(APNEA_MAX_EVENTS - 1)
                   : (uint8_t)(s->event_idx - 1);
    return s->event_min_spo2[last];
}

uint16_t sleep_apnea_get_last_event_dur(const sleep_apnea_state_t *s)
{
    if (s->total_events == 0) return 0;

    uint8_t last = (s->event_idx == 0)
                   ? (uint8_t)(APNEA_MAX_EVENTS - 1)
                   : (uint8_t)(s->event_idx - 1);
    return s->event_durations[last];
}

const char *sleep_apnea_state_name(sleep_apnea_state_e st)
{
    switch (st) {
        case APNEA_IDLE:       return "Idle";
        case APNEA_BASELINE:   return "Baseline";
        case APNEA_MONITORING: return "Monitoring";
        case APNEA_DESAT:      return "Desat";
        default:               return "?";
    }
}

const char *sleep_apnea_severity_name(sleep_apnea_severity_e sev)
{
    switch (sev) {
        case ODI_NORMAL:   return "Normal";
        case ODI_MILD:     return "Mild";
        case ODI_MODERATE: return "Moderate";
        case ODI_SEVERE:   return "Severe";
        default:           return "?";
    }
}
