/**
 * @file    power.h
 * @brief   开关机电源管理 — CTL 自锁 + KEY3 长按关机
 *
 * [2026-07-03]: 新增。
 *   电路: PMOS + NPN 三极管 + KEY3 按键
 *   上电: KEY3 按下 → PMOS 导通 → 3.3V 上电 → MCU 启动
 *   自锁: MCU 启动后 PA10(CTL) 拉高 → NPN 导通 → PMOS 栅极拉低 → 维持导通
 *   关机: KEY3 长按 3s → PA10(CTL) 拉低 → NPN 截止 → PMOS 关断 → 断电
 *
 *   引脚: PA10 = CTL (推挽输出), PC13 = KEY3 (输入上拉, 同 BTN0_POWER)
 *
 * [2026-07-07]: 计划换 XC6194A。方案A (SHDN悬空) 软件零改动, 方案B (加SHDN) 需少量修改.
 *   详见 power.c 顶部注释。
 */

#ifndef POWER_H
#define POWER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 初始化电源控制引脚
 * - PA10: CTL, 推挽输出, 初始低电平 (未自锁)
 * - 调用 power_self_lock() 拉高 CTL 完成自锁
 * 应在 HAL_Init() 后、main() 早期调用
 */
void power_init(void);

/**
 * 自锁电源: 拉高 CTL, NPN 导通, PMOS 维持导通
 * 应在 SystemClock_Config() 之后立即调用
 */
void power_self_lock(void);

/**
 * 关机: 拉低 CTL, NPN 截止, PMOS 关断, 系统断电
 */
void power_shutdown(void);

/**
 * 检测 KEY3 是否按下 (PC13 低电平 = 按下)
 * @return 1=按下, 0=未按
 */
int power_key_pressed(void);

#ifdef __cplusplus
}
#endif

#endif /* POWER_H */
