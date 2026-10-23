#ifndef __LSM6DSOX_H
#define __LSM6DSOX_H

#include "stm32u5xx_hal.h"
/* [2026-07-11 Fix]: FIFO_WATERMARK 26→52。
 *   ★核心理由 = IMU-PPG 对齐(双 FIFO 硬同步): LSM6 INT1(watermark)是唯一时基, MAX30102 中断全禁,
 *   每次 INT1 在 Sensor_Task 同一轮读两个 FIFO。设计链:
 *     IMU 104Hz → 26 配对/250ms → DownSampleFrom26To25 → 100Hz 25 样本 ┐
 *     PPG 100Hz →                                         → 100Hz 25 样本 ┴ 同一 250ms 窗口 → 喂 AI
 *   要 INT1 在"IMU 满 26 配对(=250ms=PPG 25 样本窗口)"时触发, watermark 必须 = 26×2 = 52 word
 *   (单位 = FIFO word = 1 传感器样本 = TAG 1B + data 6B; FIFO_CTRL4=0x06 只批 acc+gyr, 每周期 2 word)。
 *   原值 26 word = 125ms, 与 250ms 对齐窗口不匹配 → INT1 过早, 要么 underrun 丢弃、要么靠读取延迟
 *   凑到 250ms(对齐点漂移、不稳)。改 52 使 INT1 周期精确锁定 250ms 对齐窗口, 每轮读到的 IMU/PPG
 *   是同一时段(对齐更准); 顺带消除原 26 "samples 是否=26 依赖读取延迟"的脆弱性。
 *   证据: DS13305 WTM/DIFF_FIFO 单位=word + INT1 触发 DIFF_FIFO≥WTM; main.c GetFifoBatch(...,26)+
 *   MAX30102 GetFifoBatch(...,25)+acBuf[26*3]+DownSample26→25 锁定 26 配对/25 PPG 的对齐意图。
 *   52 word << FIFO 深度 512 word(Linux st_lsm6dsx `.max_size=512`; DS13305 标称"9KB"是字节容量,
 *   换算须 ÷ 7B/word ≈ 1285, 但 DIFF_FIFO 10-bit 上限 1023, 驱动取 512); MAX30102 FIFO 深度 32 样本 > 25, 余量约 70ms 延迟裕度。
 *   ★watermark 52 假设 IMU 恒 104Hz(52 word=26 配对=250ms)。[2026-07-11] 动态 ODR 降频已从 main.c 移除
 *   (原静止→26Hz 会致 52 word=1s INT1、PPG@50Hz 溢出 FIFO32、sleep/HAR 采样率错 2-4× → 已砍降频保恒定
 *   104/100Hz, 见 main.c 及 memory driver-verify-20260711)。⚠ 故 watermark 52 与"恒 104Hz"是**配套**的:
 *   若日后重启 LSM6DSOX_SetODR 降频, 必须同步调 watermark(26Hz 需 ≤26 word 防 PPG 溢出)+ sleep_features
 *   /HAR 采样率适配, 否则复活低功耗采样率冲突。
 *   ⚠ 对齐另一脆弱点(原设计既有, 非本次引入): 两颗独立晶振 ppm 偏差 + GetFifoBatch 读固定量而非读空,
 *   长期有微漂移; 若日后要严格对齐需时间戳配对。手改勿重生成。 */
#define FIFO_WATERMARK 52

/* ============================================================
 * MLC (Machine Learning Core) — 硬件决策树, 片内运行, 不唤醒 MCU
 *
 * 三级低功耗架构:
 *   L1: MLC 静止/运动粗筛 → 静止时不触发中断, MCU 继续睡
 *   L2: MCU tickless WFI idle (FreeRTOS 自动管理)
 *   L3: 检测到运动 → 唤醒 MCU → 全速 AI 推理
 *
 * MLC 输出: 0=静止, 1=运动, 由 MD1_CFG bit5 路由到 INT1
 * 静止时 MCU 通过 tickless idle 进入 SLEEP 模式, 电流 < 5mA
 * ============================================================ */

/* MLC 输出枚举 */
typedef enum {
    MLC_STATIONARY = 0,
    MLC_MOTION     = 1
} mlc_result_t;

uint8_t LSM6DSOX_Init(I2C_HandleTypeDef *handle);

/**
 * 配置 MLC 静止/运动检测决策树, 路由输出到 INT1
 * 须在 LSM6DSOX_Init() 之后调用
 * 返回 0=成功, 非零=错误码
 */
uint8_t LSM6DSOX_MLC_Init(void);

/**
 * 读取 MLC 决策树输出 (MLC0_SRC 寄存器, 地址 0x70)
 * 返回 MLC 分类结果: 0=静止, 1=运动
 */
uint8_t LSM6DSOX_MLC_GetResult(void);

/**
 * [2026-07-04]: 动态 ODR 切换 — 静止降采样省电, 运动恢复全速。
 *   hi_rate=1 → 104Hz (全速), hi_rate=0 → 26Hz (低功耗).
 *   同步更新 CTRL1_XL/CTRL2_G/FIFO_CTRL3。
 *   返回 0=成功, 非零=错误码。
 */
uint8_t LSM6DSOX_SetODR(uint8_t hi_rate);

uint8_t LSM6DSOX_GetFifoBatch(int16_t *a_buf, int16_t *g_buf, int16_t *t_buf, uint8_t max_samples);

/* [2026-07-05]: 硬件 Free Fall 检测 — LSM6DSOX 片内 FREE_FALL 嵌入式功能
 *   冲击检测: 合加速度 < FF_THS (自由落体/失重)
 *   持续窗口: FF_DUR ODR 周期
 *   路由: INT1 (与 FIFO watermark 共享 PB4, OR 逻辑) [2026-07-08]: PCB 重做 PA0→PB4
 *   功耗: µA 级, 主 MCU 可沉睡
 *   参考: DS13305 §13.3 FREE_FALL, ST lsm6dsox_free_fall.c 示例
 */
uint8_t LSM6DSOX_Fall_Init(void);
uint8_t LSM6DSOX_IsFreeFall(void);

//only be used to debug
#if TEST
void LSM6DSOX_GetSingleData(int16_t *ax, int16_t *ay, int16_t *az, int16_t *gx, int16_t *gy, int16_t *gz, int16_t *temp);

void LSM6DSOX_ReadBegin_DMA(void);

uint8_t LSM6DSOX_IsDataReady(void);

void LSM6DSOX_ClearDataReadyFlag(void);
#endif

#endif

