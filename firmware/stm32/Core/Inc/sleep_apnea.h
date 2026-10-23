/**
 * @file    sleep_apnea.h
 * @brief   睡眠呼吸暂停初筛 — ODI (Oxygen Desaturation Index) 算法
 *
 * 基于 SpO2 血氧监测, 在睡眠期间检测血氧下降事件。
 * 临床标准参考 AASM v3.0 / CMS Medicare ODI-3% 定义。
 *
 * ## 算法流程
 *
 *   [睡眠分期 → P(sleep)>0.5]
 *       ↓
 *   状态机: IDLE → BASELINE (120s EMA 基线建立) → MONITORING (持续检测)
 *       ↓
 *   SpO2 每秒喂入 → EMA 慢速基线 (τ≈500s)
 *       ↓
 *   当前 SpO2 < 基线 - 3% → DESAT 事件开始
 *       ↓
 *   SpO2 恢复到 ≥ 基线 - 3% → 事件结束 → 记录 (需 ≥10s 持续时间)
 *       ↓
 *   每 tick (1s) 重算 ODI = 事件数 / 监测小时数
 *
 * ## 严重程度分级 (ODI events/hour)
 *
 *   - 正常 (Normal):   ODI < 5
 *   - 轻度 (Mild):     5 ≤ ODI < 15
 *   - 中度 (Moderate): 15 ≤ ODI < 30
 *   - 重度 (Severe):   ODI ≥ 30
 *
 * ## 调用方式
 *
 *   1. sleep_apnea_init(&state) 初始化
 *   2. sleep_apnea_tick(&state, p_sleep, spo2, spo2_valid, now_s) 每秒调用
 *   3. sleep_apnea_get_odi/get_severity/get_events 读取结果
 *
 * ## 内存
 *
 *   ~250 bytes (状态 + 50 事件环缓)
 *
 * [2026-07-05] 新增, 配合 spo2.h v2 + sleep_staging_model.h
 */

#ifndef SLEEP_APNEA_H
#define SLEEP_APNEA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════
 * 可调阈值 (AASM / Medicare 标准)
 * ═══════════════════════════════════════════════════════ */

#ifndef APNEA_ODI_THRESHOLD
#define APNEA_ODI_THRESHOLD       3.0f   /* SpO2 下降阈值 (%) — ODI-3% 标准 */
#endif

#ifndef APNEA_MIN_EVENT_SEC
#define APNEA_MIN_EVENT_SEC       10     /* 最小事件持续时间 (秒) — AASM 低通气 ≥10s */
#endif

#ifndef APNEA_BASELINE_SEC
#define APNEA_BASELINE_SEC        120    /* 睡眠开始后基线建立时间 (秒) */
#endif

#ifndef APNEA_SLEEP_DEBOUNCE_S
#define APNEA_SLEEP_DEBOUNCE_S    90     /* [2026-07-05 fixed]: 90s=3epochs×30s, tick 为 1Hz 所以用秒 */
#endif

#ifndef APNEA_WAKE_DEBOUNCE_S
#define APNEA_WAKE_DEBOUNCE_S     60     /* 60s=2epochs 确认醒来 */
#endif

#ifndef APNEA_MAX_EVENTS
#define APNEA_MAX_EVENTS          50     /* 事件环缓容量 (~1 晚) */
#endif

/* EMA 基线时间常数 */
#define APNEA_BASELINE_ALPHA      0.01f  /* 基线建立期 α (τ≈100s) */
#define APNEA_MONITOR_ALPHA       0.002f /* 监测期 α (τ≈500s, 极慢适应) */


/* ═══════════════════════════════════════════════════════
 * 类型定义
 * ═══════════════════════════════════════════════════════ */

/** @brief 呼吸暂停检测状态机 */
typedef enum {
    APNEA_IDLE       = 0,  /* 未入睡, 不监测 */
    APNEA_BASELINE   = 1,  /* 入睡后建立 SpO2 基线 (前 2 分钟) */
    APNEA_MONITORING = 2,  /* 持续监测, 检测降氧事件 */
    APNEA_DESAT      = 3,  /* 正在发生降氧事件 */
} sleep_apnea_state_e;

/** @brief ODI 严重程度分级 */
typedef enum {
    ODI_NORMAL   = 0,  /* ODI < 5    — 正常 */
    ODI_MILD     = 1,  /* 5-15       — 轻度 */
    ODI_MODERATE = 2,  /* 15-30      — 中度 */
    ODI_SEVERE   = 3,  /* ≥30        — 重度 */
} sleep_apnea_severity_e;

/** @brief 呼吸暂停筛查状态 (全部字段, ~250 bytes) */
typedef struct {
    /* ---- 状态机 ---- */
    sleep_apnea_state_e state;

    /* ---- SpO2 基线 ---- */
    float   spo2_baseline;       /* EMA 慢速基线 (%) */
    float   spo2_current;        /* 最新有效 SpO2 (%) */
    uint8_t spo2_valid;          /* 是否有有效 SpO2 读数 */

    /* ---- 降氧事件追踪 ---- */
    uint8_t  in_desat;           /* 当前是否在事件中 */
    uint32_t desat_start_s;      /* 事件开始时刻 (秒) */
    float    desat_min_spo2;     /* 事件期间最低 SpO2 */

    /* ---- 累计统计 (每夜重置) ---- */
    uint16_t total_events;       /* 有效事件总数 */
    uint32_t total_sleep_s;      /* 累计睡眠时间 (秒) */
    uint32_t total_monitor_s;    /* 累计监测时间 (秒, 不含基线期) */
    float    lowest_spo2;        /* 整晚最低 SpO2 */
    float    sum_desat_dur_s;    /* 所有事件持续时间之和 (秒) */

    /* ---- 事件环缓 (最近 50 个) ---- */
    uint16_t event_durations[APNEA_MAX_EVENTS];  /* 每个事件持续秒数 */
    float    event_min_spo2[APNEA_MAX_EVENTS];   /* 每个事件最低 SpO2 */
    uint8_t  event_idx;                           /* 环缓写入位置 */

    /* ---- 睡眠去抖动 ---- */
    /* [2026-07-07 Fix]: uint8_t→uint16_t, 原 255s 溢出后环绕→0 在阈值90/60内巧合安全,
     *   但若阈值>255 则永远无法触发。C11 无符号溢出为 defined behavior 但语义依赖不应依赖。 */
    uint16_t sleep_epoch_cnt;    /* 连续 P(sleep)>0.5 的秒数 (1Hz tick) */
    uint16_t wake_epoch_cnt;     /* 连续 P(sleep)≤0.5 的秒数 */
    uint8_t is_asleep;           /* 去抖后的睡眠状态 (0=醒, 1=睡) */
    uint8_t night_just_ended;    /* 睡眠刚结束 (醒来上升沿), 消费者读取后清零 */

    /* ---- 输出 ---- */
    float                   odi;       /* 当前 ODI (events/hour) */
    sleep_apnea_severity_e  severity;  /* 严重程度 */

    /* ---- 时间基准 ---- */
    uint32_t last_tick_s;        /* 上次 tick 时刻 (秒) */
    uint32_t sleep_start_s;      /* 本次睡眠开始时刻 (秒) */
    uint32_t baseline_start_s;   /* 基线建立开始时刻 (秒) */
} sleep_apnea_state_t;


/* ═══════════════════════════════════════════════════════
 * 公共 API
 * ═══════════════════════════════════════════════════════ */

/** @brief 全局呼吸暂停筛查状态 (sleep_apnea.c 定义) */
extern sleep_apnea_state_t g_apnea;

/**
 * @brief 初始化呼吸暂停筛查状态
 * @param s  状态指针 (调用方分配)
 */
void sleep_apnea_init(sleep_apnea_state_t *s);

/**
 * @brief 每秒 tick — 主入口
 *
 * 由 app_tick() 1Hz 段调用, 传入睡眠概率和 SpO2 最新值。
 * 内部做: 睡眠去抖 → 状态机转移 → 基线更新 → 降氧检测 → ODI 重算。
 *
 * @param s            状态指针
 * @param p_sleep      睡眠概率 (0~1), 来自 sleep_model_predict()
 * @param spo2         当前 SpO2 (%), 来自 g_spo2_value (0=无效)
 * @param spo2_valid   SpO2 是否有效, 来自 g_spo2_valid
 * @param now_s        当前时间 (秒), HAL_GetTick()/1000
 */
void sleep_apnea_tick(sleep_apnea_state_t *s, float p_sleep, float spo2,
                      uint8_t spo2_valid, uint32_t now_s);

/* ---- 结果读取 ---- */

/** @brief 当前 ODI (events/hour), 监测不足 1min 返回 0 */
float sleep_apnea_get_odi(const sleep_apnea_state_t *s);

/** @brief 当前严重程度 */
sleep_apnea_severity_e sleep_apnea_get_severity(const sleep_apnea_state_t *s);

/** @brief 有效降氧事件总数 */
uint16_t sleep_apnea_get_events(const sleep_apnea_state_t *s);

/** @brief 整晚最低 SpO2, 无数据返回 100 */
float sleep_apnea_get_lowest_spo2(const sleep_apnea_state_t *s);

/** @brief 累计监测时间 (秒), 不含基线期 */
uint32_t sleep_apnea_get_monitor_time(const sleep_apnea_state_t *s);

/** @brief 当前是否处于睡眠状态 (去抖后) */
uint8_t sleep_apnea_is_asleep(const sleep_apnea_state_t *s);

/** @brief 当前状态机阶段名称 (调试用) */
const char *sleep_apnea_state_name(sleep_apnea_state_e st);

/** @brief 严重程度名称 (UI 用) */
const char *sleep_apnea_severity_name(sleep_apnea_severity_e sev);

/** @brief 最近一次降氧事件的最低 SpO2, 无事件返回 0 */
float sleep_apnea_get_last_event_spo2(const sleep_apnea_state_t *s);

/** @brief 最近一次降氧事件的持续时间 (秒), 无事件返回 0 */
uint16_t sleep_apnea_get_last_event_dur(const sleep_apnea_state_t *s);


#ifdef __cplusplus
}
#endif

#endif /* SLEEP_APNEA_H */
