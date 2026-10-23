/**
 * @file    battery.h
 * @brief   电池电压监测 — ADC4 单次转换 + PMOS 开关防关机耗电
 *
 * [2026-07-03]: 新增电池监测。
 *   电路: PMOS(Q1) + R21(10k上拉) + R15(1k) + R30(4k) 分压
 *   引脚: [2026-07-10] PA4 = BAT_ADC_EN (低=导通), PA2 = ADC1_IN7 (注入通道)。旧板: PB0/PB1(ADC4)
 *   分压比: Vadc/Vbat = 4/(1+4) = 0.8 → Vbat = Vadc × 1.25
 *   LiPo: 3.7V(放电终止) ~ 4.2V(满电), 分压后 2.96V~3.36V (VREF=3.3V 略超满电,
 *   实际 4.2V 时 ADC 饱和在 4095, 不影响电量估算)
 */

#ifndef BATTERY_H
#define BATTERY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 初始化 ADC4 + BAT_ADC_EN 控制引脚
 * 应在 HAL_Init() + SystemClock_Config() 后调用
 */
void battery_init(void);

/**
 * 读取电池电压 (mV)
 * 内部自动: 拉低 EN → 延时 1ms → ADC 采样 → 拉高 EN (关断分压支路, 省电)
 * 返回 0-5000 mV
 */
uint16_t battery_read_mv(void);

/**
 * 电池电量百分比 (LiPo 粗略: 4200mV=100%, 3700mV=0%)
 * 返回 0-100 %
 */
uint8_t battery_read_pct(void);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_H */
