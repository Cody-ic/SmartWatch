/**
 * @file    app.c
 * @brief   AI Watch 应用整合层实现
 *
 * 不含任何演示/模拟逻辑——全部由真实传感器数据驱动。
 *
 * © 2026 AI Watch Project
 */
#include "app.h"
#include "ui.h"
#include "har_inference.h"
#include "safety_detect.h"
#include "lv_port_disp.h"
#include "lvgl.h"
#include "main.h"
#include "ble_uart.h"           /* [2026-07-03]: BLE 数据发送 */
#include "rtc_time.h"           /* [2026-07-03]: RTC 计时替代 HAL_GetTick */
#include "battery.h"            /* [2026-07-03]: 电池电压监测 */
#include "power.h"              /* [2026-07-03]: 开关机, 长按关机 */
#include "exercise_scoring.h"   /* [2026-07-04]: 运动评分 & 卡路里估算 */
#include "hr_detector.h"        /* [2026-07-04]: resting_hr guard 同步 */
#include "spo2.h"               /* [2026-07-05]: SpO2 血氧估算 */
#include "ui_spo2.h"            /* [2026-07-05]: SpO2 页面 */
#include "sleep_apnea.h"        /* [2026-07-05]: 睡眠呼吸暂停初筛 */
#include "ui_sleep.h"           /* [2026-07-05]: 综合睡眠页面 */
#include "ui_alert.h"           /* [2026-07-05]: 醒来告警 */
#include "module_flags.h"        /* [2026-07-06]: 功能模块按需开关 */
#include "proto_model.h"         /* [2026-07-12]: 原型网络 */
#include <stdio.h>
#include <string.h>

/* ============================================================
 * 配置
 * ============================================================ */

/* 步频估算（每次 HAR 推理 ≈ 5.12 秒窗口） */
#define STEPS_PER_WALK_INFER   6    /* ~110 步/min × 5.12s ≈ 9，保守取 6 */
#define STEPS_PER_JOG_INFER    9    /* ~150 步/min × 5.12s ≈ 12，保守取 9 */
#define STEPS_PER_DOWN_INFER   5
#define STEPS_GOAL_DEFAULT     10000

/* 时钟刷新间隔（ms）——用 HAL_GetTick 推算时间 */
#define CLOCK_UPDATE_MS        1000

/* ============================================================
 * 内部状态
 * ============================================================ */

/* 步数 */
static volatile uint32_t step_count = 0;

/* 时间基准 — [2026-07-03] 改为从 RTC 读取, 不再依赖 HAL_GetTick */
static uint32_t last_clock_tick = 0;  /* 上次刷新时钟的 HAL_GetTick, 避免每秒多次读 RTC */

/* 1Hz 定时辅助 */
static uint32_t last_1hz_tick = 0;
static uint32_t last_lv_tick = 0;  /* 上次 lv_tick_inc 的 HAL_GetTick */

/* 运动评分状态 — [2026-07-04]: MET 热量 + 运动时长 + 实时评分 + MVPA */
static exercise_state_t ex_state;

/* [2026-07-05]: SpO2 血氧最新值 (main.c Sensor_Task 更新, app_tick 读取) */
volatile float g_spo2_value = 0.0f;     /* [2026-07-05] 非 static — main.c/ui_spo2.c extern 引用 */
volatile uint8_t g_spo2_valid = 0;
volatile uint32_t g_spo2_last_valid_ms = 0;  /* [2026-07-06] SpO2 超时清零 */
/* [2026-07-06]: 功能模块标志 — module_flags.h extern, ble_uart 0x80/0x02 修改 */
volatile uint16_t g_module_flags = MODULE_DEFAULT;
/* [2026-07-05]: 睡眠概率 (main.c sleep_model_predict 输出, sleep_apnea/UI 读取) */
volatile float g_sleep_prob = 0.0f;
static int   ex_last_class = -1;       /* 最新 HAR 类别, -1=未推理 */
static float ex_last_conf  = 0.0f;     /* 最新 HAR 置信度 */
static int   ex_last_hr    = 0;        /* 最新心率值 */
static int   ex_last_hr_ok = 0;        /* 心率是否有效 */

/* 按键缓存 flag。[2026-07-11 注释订正]: app_on_button 原注称"从 EXTI 调用",
 * 但按键 PC0/PC1 在 gpio.c 配为 GPIO_INPUT 轮询(非 EXTI), 全项目无 caller —
 * 实际按键走 app_tick() 轮询路径, app_on_button 为预留未接入。 */
static volatile uint8_t pending_button = 0xFF;

/* [2026-07-03]: 长按取消告警 — 按键按下时刻 (HAL_GetTick) */
static uint32_t button_press_tick = 0;
#define BTN_LONG_PRESS_MS  2000   /* 长按阈值 2s */

/* [2026-07-08]: PCB 重做, 按键从 PB4/PB5→PC0/PC1 */
#define BTN1_GPIO_PORT  GPIOC
#define BTN1_GPIO_PIN   GPIO_PIN_0   /* PC0: SW4 (上翻/取消告警) */
#define BTN2_GPIO_PORT  GPIOC
#define BTN2_GPIO_PIN   GPIO_PIN_1   /* PC1: SW6 经 D3 二极管 (下翻) */
#define BTN0_GPIO_PORT  GPIOC
#define BTN0_GPIO_PIN   GPIO_PIN_13  /* PC13: BTN0_POWER (电源/唤醒), VBAT域 */

/* ============================================================
 * 内部函数
 * ============================================================ */

/* [2026-07-03]: 读取按键 GPIO 电平, 返回 1=仍按住（硬件未接时恒返回 0） */
static int btn_is_held(uint8_t btn_id);

/* [2026-07-03]: 从 RTC 读取时间, 每秒刷新一次 */
static void update_clock_from_rtc(void)
{
    uint32_t now = HAL_GetTick();
    if (now - last_clock_tick < 1000) return;  /* 未满 1 秒, 或每秒刷新 */
    last_clock_tick = now;

    uint8_t h, m, s;
    uint16_t y;
    uint8_t mon, d;

    rtc_get_hms(&h, &m, &s);
    rtc_get_ymd(&y, &mon, &d);

    /* 计算星期几 (Zeller 简化算法, 1900 年偏移) */
    uint8_t wd;
    {
        uint16_t y0 = (mon <= 2) ? y - 1 : y;
        uint8_t  m0 = (mon <= 2) ? mon + 12 : mon;
        wd = (uint8_t)((d + 2*m0 + 3*(m0+1)/5 + y0 + y0/4 - y0/100 + y0/400 + 1) % 7);
    }

    ui_clock_update(h, m, (int)d, (int)mon, (int)wd);
}

/* 步数累加：HAR 推理结果为运动类时按窗口步频估算 */
static void update_steps(har_class_t cls)
{
    uint16_t inc = 0;
    switch (cls) {
        case HAR_WALKING:    inc = STEPS_PER_WALK_INFER; break;
        case HAR_JOGGING:    inc = STEPS_PER_JOG_INFER;  break;
        case HAR_DOWNSTAIRS: inc = STEPS_PER_DOWN_INFER; break;
        default: break;   /* 静态类不计步 */
    }
    if (inc == 0) return;

    step_count += inc;
    ui_steps_update(step_count, STEPS_GOAL_DEFAULT);
}

/* ============================================================
 * 公共 API
 * ============================================================ */

void app_init(void)
{
    /* 0. LVGL 内核 + 显示驱动（必须在 ui_init 之前） */
    lv_init();
    lv_port_disp_init();

    /* 1. UI（含告警覆盖层、tileview 5 页） */
    ui_init();

    /* 2. HAR 推理引擎（内部会调 safety_init()） */
    HAR_Init();

    /* 2.5. 运动评分 — [2026-07-04]: 默认 65kg/22岁, 后续可通过 BLE 配置 */
    exercise_init(&ex_state, 65.0f, 22);

    /* 2.6. 睡眠呼吸暂停初筛 — [2026-07-05] */
    sleep_apnea_init(&g_apnea);
    /* 2.7. 功能模块按需开关 — [2026-07-06] 从 TAMP 备份寄存器恢复, 默认全开 */
    module_flags_init();
    /* 2.8. 原型网络 — [2026-07-12] 初始化原型存储(空,等手机 BLE 下发) */
    proto_model_init();
    ex_last_class = -1;
    ex_last_conf  = 0.0f;
    ex_last_hr    = 0;
    ex_last_hr_ok = 0;

    /* 3. 状态清零 */
    step_count = 0;
    last_1hz_tick = HAL_GetTick();
    last_lv_tick  = HAL_GetTick();
    last_clock_tick = 0;

    /* [2026-07-03]: RTC 未配置时设为默认值 2026-01-01 00:00:00 */
    if (!rtc_is_configured()) {
        rtc_set_unix(rtc_calc_unix(2026, 1, 1, 0, 0, 0));
    }

    /* 4. 首次 UI 刷屏：步数=0、从 RTC 读时间 */
    ui_steps_update(0, STEPS_GOAL_DEFAULT);
    {
        uint8_t h, m, s;
        uint16_t y;
        uint8_t mon, d;
        rtc_get_hms(&h, &m, &s);
        rtc_get_ymd(&y, &mon, &d);
        ui_clock_update(h, m, (int)d, (int)mon, 0);
    }
}

void app_on_imu_batch(int16_t *acc25, int16_t *gyr25)
{
    /* [2026-07-03 FIX]: 对所有 25 个样本逐条调用安全检测(原仅推理完成后取最后1个样本,
     *   跌倒检测需 100Hz 数据, 5.2s 一次完全不够)。HAR class 传 -1 未知, 霸凌检测
     *   仍由 har_inference.c 推理完成后用实际 class 做二次判定。
     * [2026-07-11 Fix]: ts_ms 精度 — 原 25 样本共用一个 HAL_GetTick() 时间戳,
     *   safety_feed_imu 用 ts_ms 做时间差(STILL 3s/霸凌 5s/IMPACT 200ms 判定) →
     *   被量化为 250ms 批粒度(短窗口 IMPACT 等 200ms 实际可偏至 ~250ms, +25%)。
     *   @100Hz 每样本间隔 10ms → 基准 ts + i*10 递推至样本级精度。 */
    uint32_t ts_base = HAL_GetTick();
    for (int i = 0; i < 25; i++) {
        float ax = (float)acc25[i * 3 + 0] / 4096.0f;   /* ±8g: 0.244mg/LSB → 4096 LSB/g */
        float ay = (float)acc25[i * 3 + 1] / 4096.0f;
        float az = (float)acc25[i * 3 + 2] / 4096.0f;
        float gx = (float)gyr25[i * 3 + 0] * 0.070f;    /* ±2000dps: 70mdps/LSB */
        float gy = (float)gyr25[i * 3 + 1] * 0.070f;
        float gz = (float)gyr25[i * 3 + 2] * 0.070f;
        safety_feed_imu(ts_base + (uint32_t)(i * 10), ax, ay, az, gx, gy, gz, -1);
    }

    /* 推理 + UI 更新（原有逻辑不变） */
    int inferred = HAR_AddSamples(acc25, gyr25);

    if (inferred) {
        /* 推理完成 → 步数累加 */
        har_class_t cls = HAR_GetLastClass();
        update_steps(cls);

        /* [2026-07-03]: BLE 发送 HAR 结果 + 步数 (原发送路径静默) */
        float conf = HAR_GetLastConfidence();
        ble_uart_send_har((int)cls, conf);

        /* [2026-07-14]: BLE 发送 Proto 推理结果 */
        if (proto_model_is_loaded()) {
            int pc = HAR_GetProtoClass();
            if (pc >= 0) {
                float pprobs[7];
                HAR_GetProtoProbs(pprobs);
                ble_uart_send_proto_result(pc, pprobs[pc]);
            }
        }

        /* [2026-07-04]: 缓存 HAR 结果供运动评分 1Hz tick 使用 */
        ex_last_class = (int)cls;
        ex_last_conf  = conf;

        /* [2026-07-04]: 同步 HAR 类别 → hr_detector resting_hr guard
         *   仅静态类 (≤2) 允许 resting_hr EMA 更新, 对标 ACSM 临床标准 */
        hr_detector_set_activity((int)cls);

        uint32_t steps_copy = step_count;  /* volatile → 快照, 防竞争 */
        float dist_km = (float)steps_copy * 0.00072f;   /* 0.72m/step */
        float kcal   = (float)steps_copy * 0.043f;
        ble_uart_send_steps(steps_copy, STEPS_GOAL_DEFAULT, dist_km, kcal);
    }
}

void app_on_hr_ready(int bpm, uint16_t ppg_sample, int valid, float resting_hr)
{
    /* 刷新心率页 */
    uint8_t bpm_u8 = (bpm < 0 || bpm > 255) ? 0 : (uint8_t)bpm;
    ui_hr_update(bpm_u8, ppg_sample);

    /* 喂安全引擎（心率异常检测） */
    safety_feed_hr(HAL_GetTick(), bpm, valid);

    /* [2026-07-03]: BLE 发送心率数据 (原发送路径静默) */
    ble_uart_send_hr((float)bpm, (float)bpm, valid);

    /* [2026-07-04]: 缓存心率供运动评分 1Hz tick 使用 */
    ex_last_hr    = bpm;
    ex_last_hr_ok = valid;

    /* [2026-07-04]: 同步 hr_detector 自适应静息心率 → 运动评分模型
     *   hr_detector 安静状态下自动追踪 resting_hr (滑动窗口),
     *   替换 exercise_scoring 的硬编码 72bpm → Karvonen 阈值更准确 */
    exercise_set_resting_hr(&ex_state, resting_hr);
}

void app_tick(void)
{
    /* 喂独立看门狗（4s 超时，每次主循环 ~30ms 喂一次） */
    IWDG_Refresh();

    /* LVGL tick：跟踪真实耗时，避免动画变慢 */
    uint32_t now_lv = HAL_GetTick();
    lv_tick_inc(now_lv - last_lv_tick);
    last_lv_tick = now_lv;

    /* LVGL 主循环（处理动画、刷新、事件） */
    lv_timer_handler();

    /* [2026-07-03]: 按键轮询 (30ms间隔, 替代ISR方式, 无需EXTI配置)
     *   [2026-07-11 订正]: 引脚随 PCB 重做已迁移 —
     *   PC0→Btn1(Prev/Cancel), PC1→Btn2(Next/Cancel), PC13→Btn0(Power,悬空恒高) */
    if (pending_button == 0xFF) {
        if (HAL_GPIO_ReadPin(BTN1_GPIO_PORT, BTN1_GPIO_PIN) == GPIO_PIN_RESET)
            pending_button = 1;
        else if (HAL_GPIO_ReadPin(BTN2_GPIO_PORT, BTN2_GPIO_PIN) == GPIO_PIN_RESET)
            pending_button = 2;
        else if (HAL_GPIO_ReadPin(BTN0_GPIO_PORT, BTN0_GPIO_PIN) == GPIO_PIN_RESET)
            pending_button = 0;
        if (pending_button != 0xFF)
            button_press_tick = HAL_GetTick();  /* 记录按下时刻 */
    }

    /* 按键延迟处理（主循环轮询 → 长按检测） */
    /* [2026-07-03]: Btn0(PC13) 长按 3s → 关机; 霸凌警报中 Btn1/2 长按 → 取消 */
    uint8_t btn = pending_button;
    if (btn != 0xFF) {
        /* 电源键长按关机 (独立于告警状态) */
        if (btn == 0 && btn_is_held(btn)) {
            uint32_t elapsed = HAL_GetTick() - button_press_tick;
            if (elapsed >= 3000) {  /* 3s 长按 */
                /* [2026-07-10]: power_shutdown() 停用 — 新板长按关机由 XC6194A(SW6)硬件完成, MCU 不处理。 */
#if 0
                power_shutdown();
#endif
            }
        }
        if (ui_alert_is_active() && ui_alert_current() == ALERT_BULLYING) {
            /* [2026-07-07]: 霸凌告警 — 短按即取消 (原为长按2s). 开关机/复位由 XC6194A 硬件处理 */
            if (!btn_is_held(btn)) {
                pending_button = 0xFF;
                ui_alert_dismiss();
            }
            /* else: 按住中, 等松手再触发 */
        } else {
            /* 非告警状态: 正常翻页
             * [2026-07-06 Fix]: 原代码对 btn=0 也执行 pending_button=0xFF,
             *   导致长按计时器每 tick 被重置, 3s 关机永远无法触发。
             *   修复: 电源键按住中不重置, 松手重置 (btn_is_held 返回0)。 */
            if (btn == 0) {
                if (!btn_is_held(btn)) pending_button = 0xFF;  /* 松手: 取消 */
                /* else: 还在按住但 <3s, 保留 pending_button 继续等 */
            } else {
                pending_button = 0xFF;
                switch (btn) {
                    case 1: ui_page_prev(); break;
                    case 2: ui_page_next(); break;
                    default: break;
                }
            }
        }
    }

    /* 1Hz 定时任务 */
    uint32_t now = HAL_GetTick();
    if (now - last_1hz_tick >= 1000) {
        last_1hz_tick = now;

        /* 时钟推进 — [2026-07-03] 从 RTC 读取 */
        update_clock_from_rtc();

        /* 安全引擎 1Hz tick（心率异常触发 + 跌倒状态复位） */
        safety_tick_1hz();

        /* [2026-07-03]: 电池电压监测 — 每秒读一次 */
        {
            uint16_t bat_mv = battery_read_mv();
            uint8_t  bat_pct = battery_read_pct();
            ble_uart_send_battery(bat_mv, bat_pct);
        }

        /* [2026-07-14]: Proto 状态 — 每 30s 推送给手机 (个性化进度)
         *   用 proto_model_get_version() 轻量获取版本, 避免栈上 3584B 原型拷贝 */
        {
            static uint8_t proto_status_tick = 0;
            if (++proto_status_tick >= 30 && proto_model_is_loaded()) {
                proto_status_tick = 0;
                ble_uart_send_proto_status(proto_model_get_version(),
                    proto_session_get_counts());
            }
        }

        /* [2026-07-05]: SpO2 页面刷新 — 与 HR 联动, 每秒更新 */
        ui_spo2_update((uint8_t)(ex_last_hr > 0 ? ex_last_hr : 0));
        ble_uart_send_spo2(g_spo2_valid ? g_spo2_value : 0.0f, g_spo2_valid);

        /* [2026-07-05]: 睡眠呼吸暂停初筛 — 1Hz tick */
        {
            float spo2_val = g_spo2_valid ? g_spo2_value : 0.0f;
            sleep_apnea_tick(&g_apnea, g_sleep_prob, spo2_val, g_spo2_valid,
                             HAL_GetTick() / 1000);
            ui_sleep_update();

            /* 醒来且 ODI ≥ 轻度 → 告警提醒用户 */
            if (g_apnea.night_just_ended) {
                g_apnea.night_just_ended = 0;  /* 消费上升沿 */
                if (sleep_apnea_get_severity(&g_apnea) >= ODI_MILD
                    && sleep_apnea_get_events(&g_apnea) > 0) {
                    char buf[48];
                    lv_snprintf(buf, sizeof(buf), "ODI:%.1f Ev:%u Low:%.0f%%",
                               (double)sleep_apnea_get_odi(&g_apnea),
                               (unsigned)sleep_apnea_get_events(&g_apnea),
                               (double)sleep_apnea_get_lowest_spo2(&g_apnea));
                    ui_alert_show(ALERT_SLEEP_APNEA, "Sleep Apnea", buf);
                }
                /* 醒来时发送最终报告到手机 */
                {
                    uint16_t events = sleep_apnea_get_events(&g_apnea);
                    uint16_t avg_dur = (events > 0 && g_apnea.sum_desat_dur_s > 0.0f)
                        ? (uint16_t)(g_apnea.sum_desat_dur_s / (float)events) : 0;
                    uint16_t mon_min = (uint16_t)(sleep_apnea_get_monitor_time(&g_apnea) / 60);
                    ble_uart_send_sleep_apnea(
                        (uint8_t)sleep_apnea_get_severity(&g_apnea),
                        sleep_apnea_get_odi(&g_apnea), events,
                        (uint8_t)sleep_apnea_get_lowest_spo2(&g_apnea),
                        avg_dur, mon_min, 0);
                }
            }

            /* 睡眠中每 5 分钟发送一次中间报告 */
            if (g_apnea.is_asleep && g_apnea.total_monitor_s >= 60) {
                static uint32_t last_apnea_ble_s = 0;
                uint32_t now_s = HAL_GetTick() / 1000;
                if (now_s - last_apnea_ble_s >= 300) {
                    last_apnea_ble_s = now_s;
                    uint16_t events = sleep_apnea_get_events(&g_apnea);
                    uint16_t avg_dur = (events > 0 && g_apnea.sum_desat_dur_s > 0.0f)
                        ? (uint16_t)(g_apnea.sum_desat_dur_s / (float)events) : 0;
                    uint16_t mon_min = (uint16_t)(sleep_apnea_get_monitor_time(&g_apnea) / 60);
                    ble_uart_send_sleep_apnea(
                        (uint8_t)sleep_apnea_get_severity(&g_apnea),
                        sleep_apnea_get_odi(&g_apnea), events,
                        (uint8_t)sleep_apnea_get_lowest_spo2(&g_apnea),
                        avg_dur, mon_min, 1);
                }
            }
        }

        /* [2026-07-04]: 运动评分 — MET 热量 + 有效运动时长 + 实时评分 + MVPA */
        exercise_tick(&ex_state, ex_last_class, (float)ex_last_hr,
                       ex_last_hr_ok, ex_last_conf, now);
        ble_uart_send_status(ex_state.calorie_kcal,
                              ex_state.session_score,
                              ex_state.exercise_seconds,
                              ex_last_class,
                              (float)ex_last_hr);
    }
}

void app_set_time(uint8_t hour, uint8_t minute)
{
    /* [2026-07-03]: 改为写入 RTC — 保留当前日期, 仅覆盖时分秒 */
    uint16_t y;
    uint8_t  mon, d;
    rtc_get_ymd(&y, &mon, &d);
    uint32_t unix = rtc_calc_unix(y, mon, d, hour, minute, 0);
    rtc_set_unix(unix);

    ui_clock_update(hour, minute, (int)d, (int)mon, 0);
}

/* [2026-07-03]: 读取按键 GPIO 电平, 返回 1=仍按住（硬件未接时恒返回 0） */
static int btn_is_held(uint8_t btn_id)
{
    GPIO_TypeDef *port = NULL;
    uint16_t pin = 0;
    if (btn_id == 0) { port = BTN0_GPIO_PORT; pin = BTN0_GPIO_PIN; }
    if (btn_id == 1) { port = BTN1_GPIO_PORT; pin = BTN1_GPIO_PIN; }
    if (btn_id == 2) { port = BTN2_GPIO_PORT; pin = BTN2_GPIO_PIN; }
    if (port == NULL || pin == 0) return 0;  /* 硬件未接, 回退为短按取消 */
    return (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_RESET) ? 1 : 0;
}

void app_on_button(uint8_t button_id)
{
    /* [2026-07-11 注释订正]: 此函数为 EXTI 中断预留入口, 但当前按键(PC0/PC1)配为
     *   GPIO_INPUT 轮询, 未接 EXTI, 故本函数无 caller(死代码保留)。实际按键处理在
     *   app_tick() 轮询路径。若日后改用 EXTI: STM32U5 回调名是 HAL_GPIO_EXTI_Rising_Callback。 */
    pending_button = button_id;
    button_press_tick = HAL_GetTick();  /* [2026-07-03]: 记录按下时刻供长按检测 */
}

/* [2026-07-04]: 手机 BLE 下发用户参数 → 更新运动评分模型
 *   体重影响卡路里: kcal/s = MET × kg / 3600
 *   年龄影响最大心率: HRmax = 208 - 0.7×age (Tanaka 2001)
 *   范围: weight 20-250kg, age 5-120 岁 (WHO/ACSM 合理边界) */
void app_set_user_profile(float weight_kg, uint8_t age)
{
    if (weight_kg < 20.0f || weight_kg > 250.0f) return;
    if (age < 5 || age > 120) return;

    ex_state.weight_kg = weight_kg;
    ex_state.age       = age;
    ex_state.max_hr    = 208.0f - 0.7f * (float)age;  /* Tanaka 2001: HRmax 随年龄更新 */
}
