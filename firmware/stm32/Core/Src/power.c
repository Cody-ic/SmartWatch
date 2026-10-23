/**
 * @file    power.c
 * @brief   开关机电源管理 — CTL 自锁 + KEY3 检测
 *
 * [2026-07-03]: 新增。
 *   引脚: PA10 = CTL (输出, 高=自锁), PC13 = KEY3 (输入, 低=按下)
 *   原理: 上电时 MCU 通过 PA10 拉高驱动 NPN 三极管, 维持 PMOS 导通.
 *        长按 KEY3 3s 后拉低 PA10, 切断 PMOS, 系统断电.
 *
 * [2026-07-07]: 计划换 XC6194A 替代 PMOS+NPN 离散自锁。引脚变化:
 *
 *   方案A (最小改动, 推荐): SHDN 悬空, 只接 VIN/VOUT/SW/GND.
 *       XC6194A 完全替代 PMOS+NPN 电路, SW 脚接管按键.
 *       软件零改动 — power_init/self_lock/shutdown 全部无害(PA10 悬空无人接收).
 *       PC13 读不到按键了, 但长按断电由 XC6194 硬件处理, 不再需要 MCU 检测.
 *
 *   方案B (加 SHDN 控制): PA10 → SHDN, PC13 → SW.
 *       power_shutdown() 保留, power_self_lock() 废弃, power_init() 需改初始电平.
 *       详见下文详细说明.
 */

#include "power.h"
/* [2026-07-04 Fix]: stm32u585xx.h→stm32u575xx.h, 芯片已换型为 STM32U575RITx */
#include "stm32u575xx.h"

/* [2026-07-10]: 新板改用 XC6194A 负载开关, 开关机/长按关机全部由硬件完成
 *   (按键 SW6 → XC6194A SW 脚; SHDN=PC2 高有效, 经 R1 下拉到 GND 默认为低=常开), MCU 不参与电源管理。
 *   本文件是旧板 PA10 驱动 NPN + PMOS 自锁离散电路的残留, 整体废弃。
 *   参照 MPU6050 约定用 #if 0 排除。手改, 勿重生成 (.ioc 已发散)。 */
#if 0

#define CTL_PORT   GPIOA
#define CTL_PIN    (1U << 10)   /* PA10: CTL 控制 */

#define KEY_PORT   GPIOC
#define KEY_PIN    (1U << 13)   /* PC13: KEY3, 同 BTN0_POWER */

void power_init(void)
{
    /* PA10: CTL — 推挽输出, 初始低 (未自锁, 防止误触发) */
    RCC->AHB2ENR1 |= RCC_AHB2ENR1_GPIOAEN;
    __DSB();
    GPIOA->MODER &= ~(3U << (10 * 2));
    GPIOA->MODER |=  (1U << (10 * 2));  /* 通用输出 */
    GPIOA->OTYPER &= ~(1U << 10);       /* 推挽 */
    GPIOA->PUPDR &= ~(3U << (10 * 2));  /* 无上下拉 */
    GPIOA->BSRR = CTL_PIN << 16;        /* BRR = 低电平 */

    /* PC13: KEY3 — 输入上拉 (已在 gpio.c 中由 CubeMX 配置为 BTN0_POWER)
     *   这里确认配置, 不做重复初始化 */
}

void power_self_lock(void)
{
    CTL_PORT->BSRR = CTL_PIN;           /* BS0 = 高电平, NPN 导通 → PMOS 自锁 */
}

void power_shutdown(void)
{
    CTL_PORT->BSRR = CTL_PIN << 16;     /* BRR = 低电平, NPN 截止 → PMOS 关断 */
    /* 此后 MCU 断电, 不会返回 */
    while (1) { __WFI(); }
}

int power_key_pressed(void)
{
    return (KEY_PORT->IDR & KEY_PIN) ? 0 : 1;  /* 低电平 = 按下 */
}

#endif /* 0 — [2026-07-10] power.c 废弃: 新板 XC6194A 硬件管理电源 */
