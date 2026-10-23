/**
 * @file safety_detect.c
 * @brief 安全检测引擎实现 — 跌倒/霸凌/心率异常
 *
 * 状态机驱动，MCU 资源占用极小：
 *   - 1 个 IMU 滑动窗口（~50 样本 ≈ 500ms）
 *   - 几个 uint32 时间戳
 *   - 无 malloc，全部静态
 *
 * 调用流程：
 *   - main loop / sensor task:
 *       safety_feed_imu(...)    // 每条 IMU 样本
 *       safety_feed_hr(...)     // 每个 BPM 输出
 *   - 1Hz timer task:
 *       safety_tick_1hz()       // 状态推进 + 告警触发
 *
 * 触发告警后由 ui_alert_show() 推送到屏幕。
 *
 * © 2026 AI Watch Project
 */
#include "safety_detect.h"
#include "ui_alert.h"
#include "ble_uart.h"      /* [2026-07-03]: BLE 安全告警发送 */
#include "lvgl.h"          /* lv_snprintf */
#include "stm32u5xx_hal.h" /* HAL_GetTick */
#include <math.h>
#include <string.h>
#include "FreeRTOS.h"  /* [2026-08-27 Fix]: taskENTER_CRITICAL 序列化 bully 字段 RMW */
#include "task.h"

/* ---------- 内部状态 ---------- */
typedef struct {
    /* 跌倒检测状态机 */
    enum {
        FALL_IDLE = 0,
        FALL_IMPACT,      /* 已检测到冲击，等待静止 */
        FALL_STILL,       /* 静止期 */
        FALL_CONFIRMED,   /* 触发告警（持续未起身） */
    } fall_state;
    uint32_t fall_impact_ts;   /* 冲击时刻 (ms) */
    uint32_t fall_still_start; /* 静止开始时刻 */

    /* [2026-07-04 Fix]: 霸凌检测字段加 volatile — 原注释称"Cortex-M33 天然原子"
     *   但原子性 ≠ 可见性。这些字段由 Mic_Task/KWS_Task (safety_feed_shout) 写入,
     *   Sensor_Task 读取。无 volatile 时编译器可将读取缓存在寄存器中 (GCC -O1+)。
     *   交叉验证: C11 §5.1.2.3 (非 volatile = 编译器可优化消除访问)
     *   + GCC ARM -O0→-Os (非 volatile 跨函数可缓存) + FreeRTOS 推荐模式 */
    volatile uint32_t bully_high_accel_start;
    volatile int      bully_hr_baseline;
    volatile uint32_t bully_last_trigger_ts;

    /* 心率异常 */
    uint32_t hr_high_start;   /* 0 = 当前未超阈值 */
    uint32_t hr_low_start;

    /* 最近一次喂入的 HAR
     * [2026-08-27 Fix]: 加 volatile — last_har 由 Sensor_Task(safety_feed_imu:108) 写,
     *   被 Mic_Task/KWS(safety_feed_shout:258) 跨任务读。同结构体其余跨任务字段
     *   (bully/shout/deferred) 均已 volatile, 唯独漏了它。非 volatile 跨线程访问
     *   是 C11 UB, -O1+ 下编译器可缓存读取 → 门控读到陈旧 HAR 类别。 */
    volatile int last_har;
    int      last_hr;
    int      hr_valid;

    /* 声学检测（Mic_Task 写入，Sensor_Task 读取）
     * [2026-07-04 Fix]: 加 volatile — Mic/KWS 任务写入, Sensor_Task 读取,
     *   同 bully 字段的跨任务可见性问题。 */
    volatile uint32_t shout_ts;
    volatile uint8_t  shout_active;

    /* 延迟告警机制：Mic_Task 设置标志，Sensor_Task 消费并调用 LVGL
     * [2026-07-02]: deferred_type 加 volatile——防编译器重排跨任务写入顺序 */
    volatile uint8_t deferred_pending;
    volatile alert_type_t deferred_type;
    /* [2026-07-04]: 防抖计数器 — 连续 N 样本满足条件才触发, 抗单点噪声 */
    uint8_t still_entry_cnt;
    uint8_t rise_cnt;
} safety_state_t;

/* NOTE: safety_feed_shout() 由 Mic_Task 调用。
 * ui_alert_show() 直接调 LVGL API——LVGL 非线程安全。
 * 因此 safety_feed_shout() 只设置 deferred_pending 标志，
 * 实际 ui_alert_show() 由 safety_tick_1hz()（Sensor_Task 上下文）消费执行。
 *
 * safety_feed_imu()、safety_feed_hr()、safety_tick_1hz()
 * 都在 Sensor_Task（app_tick）中调用，不存在并发。 */
static safety_state_t st;

/* ---------- 工具：判断是否运动类 HAR ---------- */
static inline int is_exercise(int har) {
    /* walking(3), jogging(4), downstairs(5), cycling(6) — 算运动 */
    return (har >= 3 && har <= 6);
}

/* ---------- 公共 API ---------- */
void safety_init(void) {
    memset(&st, 0, sizeof(st));
}

void safety_feed_imu(uint32_t ts_ms,
                     float ax, float ay, float az,
                     float gx, float gy, float gz,
                     int har_class) {
    (void)gx; (void)gy; (void)gz;   /* 暂不用陀螺仪 */

    /* 合加速度 */
    float acc_mag = sqrtf(ax*ax + ay*ay + az*az);

    /* [2026-07-04 Fix]: 仅当 har_class 为有效值 (>=0) 时才更新 st.last_har。
     *   原逻辑 app_on_imu_batch 对 25 个样本传 har_class=-1 → 每次批量覆盖推理结果，
     *   导致霸凌/HR 异常检测的 is_exercise(st.last_har) 保护窗口仅 ~30ms (0.6%)。
     *   har_inference.c:268 每 5.12s 传入真实 HAR 类别, 本修复确保它能被保留。 */
    if (har_class >= 0)
        st.last_har = har_class;

    /* === 跌倒检测（两阶段） === */
    switch (st.fall_state) {
    case FALL_IDLE:
        /* 阶段 1：冲击检测 */
        st.still_entry_cnt = 0;
        st.rise_cnt = 0;
        if (acc_mag > SAFETY_FALL_ACCEL_G) {
            st.fall_state = FALL_IMPACT;
            st.fall_impact_ts = ts_ms;
        }
        break;

    case FALL_IMPACT:
        /* 阶段 2：等待 200ms 后看是否进入静止 */
        if (ts_ms - st.fall_impact_ts > 1500) {
            st.fall_state = FALL_IDLE;
        } else if (ts_ms - st.fall_impact_ts > 200) {
            /* [2026-07-04]: 防抖 — 连续 N 个样本低于阈值才确认静止, 抗单点噪声 */
            if (acc_mag < SAFETY_FALL_STILL_ACCEL_G) {
                st.still_entry_cnt++;
                if (st.still_entry_cnt >= SAFETY_FALL_DEBOUNCE_CNT) {
                    st.fall_state = FALL_STILL;
                    st.fall_still_start = ts_ms;
                }
            } else {
                st.still_entry_cnt = 0;
            }
        }
        break;

    case FALL_STILL:
        /* 阶段 3：持续静止 3s → 触发 */
        /* [2026-07-04]: 防抖 — 连续 N 个样本高于阈值才确认起身, 单次抖动不取消 */
        if (acc_mag > SAFETY_FALL_RISE_ACCEL_G) {
            st.rise_cnt++;
            if (st.rise_cnt >= SAFETY_FALL_DEBOUNCE_CNT) {
                st.fall_state = FALL_IDLE;
            }
        } else {
            st.rise_cnt = 0;
        }
        if (ts_ms - st.fall_still_start > (uint32_t)SAFETY_FALL_STILL_MS) {
            ui_alert_show(ALERT_FALL_DETECTED, NULL, NULL);
            /* [2026-07-14 Fix]: 跌倒也上报手机 — 原来只 ui_alert_show、从不发 BLE,
             *   手机端整条 fall→全屏告警→紧急短信 pipeline(dashboard_page.dart:109 /
             *   emergency_alert.dart)因此永不触发。imu=1/acoustic=0 → App 解析为 FALL
             *   (dashboard_page.dart:111 `acousticAlert ? bullying : fall`)。
             *   本函数在 Sensor_Task 上下文, 与其他 BLE 发送同线程, 直接发安全。 */
            ble_uart_send_safety(0, 0, 1);
            st.fall_state = FALL_CONFIRMED;
        }
        break;

    case FALL_CONFIRMED:
        /* 等告警解除后由 safety_tick_1hz 复位 */
        break;
    }

    /* === 霸凌检测 === */
    /* 剧烈动作判定（与运动区分）：
       - acc_mag 持续 > 阈值
       - 且 HAR 不是已知运动类（即 standing/lying/sitting 时有剧烈 IMU）
       - 加上心率突增
    */
    /* [2026-07-04 Fix]: 每样本 har_class=-1 (app_on_imu_batch 传入) 时使用
     *   st.last_har (由 har_inference.c:268 每 5.12s 更新, C4 修复保护不覆盖)。
     *   原逻辑直接用 har_class=-1 → is_exercise(-1) 恒 false → 运动中 acc>2g+HR↑
     *   误触霸凌告警。霸凌检测应与喊叫/关键词联动 (safety_feed_shout) 共同判定。 */
    int effective_har = (har_class >= 0) ? har_class : st.last_har;
    if (acc_mag > SAFETY_BULLY_ACCEL_G && !is_exercise(effective_har)) {
        if (st.bully_high_accel_start == 0) {
            st.bully_high_accel_start = ts_ms;
            st.bully_hr_baseline = st.last_hr;
        } else if (ts_ms - st.bully_high_accel_start >
                   (uint32_t)SAFETY_BULLY_DURATION_MS) {
            /* 持续 5s 剧烈动作 + HR 突增 > 30 */
            int hr_jump = (st.hr_valid && st.bully_hr_baseline > 0)
                          ? (st.last_hr - st.bully_hr_baseline) : 0;
            if (hr_jump > 30) {
                /* 冷却 30s 避免反复触发 */
                if (ts_ms - st.bully_last_trigger_ts > 30000) {
                    ui_alert_show(ALERT_BULLYING, NULL, NULL);
                    /* [2026-07-14 Fix]: 霸凌也上报手机 — imu=1/acoustic=1 → App 解析为
                     *   BULLYING (原此路径不发 BLE, 手机收不到)。 */
                    ble_uart_send_safety(0, 1, 1);
                    st.bully_last_trigger_ts = ts_ms;
                    st.bully_high_accel_start = 0;  /* 只在实际触发时重置 */
                }
                /* 冷却期内不重置累积器，保持追踪 */
            }
        }
    } else {
        /* 不再剧烈 → 重置（防累积） */
        st.bully_high_accel_start = 0;
    }
}

void safety_feed_hr(uint32_t ts_ms, int bpm, int valid) {
    /* 只在 valid 时更新 last_hr，避免无效值污染霸凌基线 */
    st.hr_valid = valid;

    if (!valid) {
        st.hr_high_start = 0;
        st.hr_low_start = 0;
        return;
    }

    /* 非运动状态时检查心率阈值 */
    if (is_exercise(st.last_har)) {
        /* 先清触发时间戳再更新 BPM，减少 race window */
        st.hr_high_start = 0;
        st.hr_low_start = 0;
        st.last_hr = bpm;
        return;
    }

    /* 高心率 */
    if (bpm > SAFETY_HR_HIGH_BPM) {
        if (st.hr_high_start == 0) st.hr_high_start = ts_ms;
        /* 持续时长由 safety_tick_1hz 检查 */
    } else {
        st.hr_high_start = 0;
    }

    /* 低心率 */
    if (bpm < SAFETY_HR_LOW_BPM) {
        if (st.hr_low_start == 0) st.hr_low_start = ts_ms;
    } else {
        st.hr_low_start = 0;
    }

    /* 最后更新 BPM，减少 race window */
    st.last_hr = bpm;

    /* 延迟补充霸凌 HR baseline（如果高加速期间第一次收到有效 HR） */
    if (st.bully_high_accel_start != 0 && st.bully_hr_baseline == 0) {
        st.bully_hr_baseline = bpm;
    }
}

void safety_feed_shout(uint32_t ts_ms, int is_shouting) {
    st.shout_ts     = ts_ms;
    st.shout_active = (is_shouting != 0) ? 1 : 0;

    /* 喊叫 + 霸凌检测联动：如果当前正在追踪剧烈动作且有喊叫，触发告警。
     * ★ LVGL 非线程安全——此处只设置 deferred 标志，实际 ui_alert_show()
     *   由 safety_tick_1hz() 在 Sensor_Task 上下文消费执行。
     * [2026-08-27 Fix]: bully_high_accel_start / bully_last_trigger_ts 被
     *   Sensor_Task(safety_feed_imu) 与 Mic_Task/KWS(此处) 三方多写者 RMW,
     *   volatile 只保证可见性、不保证复合读-改-写原子性 → 用临界区序列化
     *   这段 RMW, 防霸凌追踪窗口被误清零、冷却时间戳被覆盖(冷却失效)。
     *   safety_feed_imu 在 Sensor_Task(prio2 最高) 上下文, 不被 Mic/KWS 抢占,
     *   故无需临界区。 */
    if (is_shouting) {
        taskENTER_CRITICAL();
        if (st.bully_high_accel_start != 0 &&
            !is_exercise(st.last_har)) {
            /* 冷却 30s 避免反复触发 */
            if (ts_ms - st.bully_last_trigger_ts > 30000) {
                st.deferred_type = ALERT_BULLYING;
                st.deferred_pending = 1;
                st.bully_last_trigger_ts = ts_ms;
                st.bully_high_accel_start = 0;
            }
        }
        taskEXIT_CRITICAL();
    }
}

void safety_tick_1hz(void) {
    /* 统一用 HAL_GetTick() —— safety_feed_hr 的 ts_ms 也是调用方传的 HAL_GetTick() */
    uint32_t now = HAL_GetTick();

    /* === 消费 Mic_Task 的延迟告警（LVGL 线程安全：在 Sensor_Task 上下文执行） === */
    if (st.deferred_pending) {
        alert_type_t alert = st.deferred_type;
        st.deferred_pending = 0;
        ui_alert_show(alert, NULL, NULL);
        /* [2026-07-03]: BLE 安全告警 (霸凌/跌倒)
         * [2026-07-14 Fix]: acoustic 位区分 App 端 fall/bully — 原恒发 (0,0,1) 使
         *   霸凌被 App 误标成 FALL。deferred_type 目前仅 ALERT_BULLYING (line 252),
         *   用类型判定既纠正当前误标、又防未来复用 deferred 时再误标。 */
        ble_uart_send_safety(0, (alert == ALERT_BULLYING) ? 1 : 0, 1);
    }

    /* === 心率异常触发 === */
    if (st.hr_high_start && (now - st.hr_high_start) >
        (uint32_t)SAFETY_HR_DURATION_MS) {
        char buf[24];
        lv_snprintf(buf, sizeof(buf), "BPM: %d", st.last_hr);
        ui_alert_show(ALERT_HR_HIGH, "HR HIGH", buf);
        st.hr_high_start = 0;   /* 单次触发，等再次累积 */
        /* [2026-07-03]: BLE 心率异常告警 */
        ble_uart_send_safety(1, 0, 0);  /* hr_anomaly=1 */
    }
    if (st.hr_low_start && (now - st.hr_low_start) >
        (uint32_t)SAFETY_HR_DURATION_MS) {
        char buf[24];
        lv_snprintf(buf, sizeof(buf), "BPM: %d", st.last_hr);
        ui_alert_show(ALERT_HR_LOW, "HR LOW", buf);
        st.hr_low_start = 0;
        /* [2026-07-03]: BLE 心率异常告警 */
        ble_uart_send_safety(1, 0, 0);  /* hr_anomaly=1 */
    }

    /* === 跌倒告警解除后复位 === */
    if (st.fall_state == FALL_CONFIRMED && !ui_alert_is_active()) {
        st.fall_state = FALL_IDLE;
    }
}
