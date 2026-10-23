/**
 * @file module_flags.h
 * @brief 功能模块按需开关 — 位掩码控制, 未开启的模块跳过处理
 *
 * 加电默认全开。用户通过 BLE 指令按需关闭, 标志持久化到 TAMP 备份寄存器。
 * 每模块 1 bit, 16 bits 可扩展。
 *
 * 使用:
 *   if (MODULE_IS_ON(HAR)) { HAR_AddSamples(...); }
 *
 * BLE 指令: 0x80/0x02 — [0x02][flags:uint16 LE], 回复 ACK 同格式
 */

#ifndef MODULE_FLAGS_H
#define MODULE_FLAGS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════ 模块位定义 ═══════ */
#define MODULE_HAR          (1u << 0)   /* 运动识别 */
#define MODULE_HR           (1u << 1)   /* 心率监测 */
#define MODULE_SPO2         (1u << 2)   /* 血氧估算 */
#define MODULE_SAFETY       (1u << 3)   /* 安全检测 (跌倒/霸凌/HR异常) */
#define MODULE_KWS          (1u << 4)   /* 关键词唤醒 */
#define MODULE_SLEEP        (1u << 5)   /* 睡眠分期 */
#define MODULE_BLE          (1u << 6)   /* BLE 数据上报 */
#define MODULE_STEPS        (1u << 7)   /* 步数统计 */
#define MODULE_APNEA        (1u << 8)   /* [2026-07-05] 睡眠呼吸暂停初筛 */

#define MODULE_ALL          0x01FFu     /* 全部开启 (bits 0-8) */
#define MODULE_DEFAULT      (MODULE_ALL)  /* [2026-07-05] 默认全开 */

/* ═══════ TAMP 备份 + 运行时标志 ═══════ */
/* [2026-07-06 Fix]: 原 static 定义在头文件中 — 每个翻译单元独立副本,
 *   module_flags_set() 在 ble_uart.c 的修改对 app.c 的 module_is_on() 不可见。
 *   改为 extern 声明, 单一定义在 app.c。 */
/* BKP0R=0xCAFE(RTC已配), BKP1R=模块持久化标志 */
#include "stm32u575xx.h"

extern volatile uint16_t g_module_flags;

/* [2026-07-11] ⚠ 当前无调用者: app.c 未在任何子系统入口用它门控 → 模块按需开关不生效,
 *   所有模块无条件常开。用户决定不接通(设备仅2键, 非必要)。若启用需在 app_on_imu_batch/
 *   app_on_hr_ready/app_tick 各入口加 if(module_is_on(MODULE_xxx))。见 CLAUDE.md / memory driver-audit-20260710。 */
static inline int module_is_on(uint16_t mask)
{
    return (g_module_flags & mask) != 0;
}

static inline uint16_t module_flags_get(void)
{
    return g_module_flags;
}

static inline void module_flags_init(void)
{
    if (TAMP->BKP0R == 0xCAFE) {
        g_module_flags = (uint16_t)TAMP->BKP1R;
        if (g_module_flags == 0 || g_module_flags == 0xFFFF)
            g_module_flags = MODULE_DEFAULT;
    } else {
        g_module_flags = MODULE_DEFAULT;
    }
    TAMP->BKP1R = g_module_flags;
}

static inline void module_flags_set(uint16_t flags)
{
    g_module_flags = flags;
    TAMP->BKP1R = flags;
}

#ifdef __cplusplus
}
#endif

#endif /* MODULE_FLAGS_H */
