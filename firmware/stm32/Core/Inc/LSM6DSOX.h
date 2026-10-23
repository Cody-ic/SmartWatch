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

/* ST 官方腕戴活动决策树的 MLC0_SRC 原始输出。
 * OTHER 包含静止以及其他非走跑动作，不能作为静止判据。
 * MLC 以 26 Hz 处理 52 样本窗口；原始 IMU/FIFO 仍保持 104 Hz。
 * 结果由 Sensor_Task 轮询，INT1 仅保留 FIFO 水位中断。 */
typedef enum {
    MLC_OTHER   = 0x01,
    MLC_WALKING = 0x04,
    MLC_RUNNING = 0x08,
    MLC_UNKNOWN = 0xFF
} mlc_result_t;

uint8_t LSM6DSOX_Init(I2C_HandleTypeDef *handle);

/**
 * 向片内配置存储器加载 ST 官方腕戴活动决策树。
 * 须在 LSM6DSOX_Init() 成功之后、任务开始采集之前调用。
 * 加载期间会停止 IMU 并清空 FIFO，不能在采集中调用。
 * 返回 0=成功，非零=加载或通信失败；失败后须重新初始化。
 */
uint8_t LSM6DSOX_MLC_Init(void);

/**
 * 读取 embedded bank 的 MLC0_SRC (0x70)，随后恢复 user bank。
 * 返回上述分类值；未加载、尚无有效分类或通信失败返回 MLC_UNKNOWN。
 * 单个窗口为 2 秒，元分类器还会影响首次输出/切换的等待时间。
 * 运行时与其他 LSM6DSOX 访问一样，仅由 Sensor_Task 调用。
 */
uint8_t LSM6DSOX_MLC_GetResult(void);

/* 独立的 wake-up 运动检测器，用于屏幕空闲计时，不属于 MLC。
 * Init 在任务采集前调用，返回 0=成功、1=失败，不路由到 INT1。
 * GetResult 返回 0=自上次读取以来无超阈值事件、1=有事件、0xFF=通信失败。
 * 无事件不等于严格静止，也不能识别抬腕姿态。 */
uint8_t LSM6DSOX_WakeUp_Init(void);
uint8_t LSM6DSOX_WakeUp_GetResult(void);

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

