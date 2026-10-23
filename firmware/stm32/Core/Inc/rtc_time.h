/**
 * @file    rtc_time.h
 * @brief   RTC 时间管理 — LSI 驱动 + BLE 授时校正
 *
 * [2026-07-03]: 替代原有的 HAL_GetTick() uptime 计时, 提供持久化时间。
 *   RTC 时钟源: LSI (~32kHz, 无需外部晶振), 关机断电后时间丢失,
 *   每次 BLE 连接手机时通过 0x91 授时包自动校正。
 *
 * 使用:
 *   1. main.c 中调 rtc_init()
 *   2. app.c 中调 rtc_get_hms() / rtc_get_ymd() 获取当前时间
 *   3. ble_uart.c 收到 0x91 时调 rtc_set_unix() 校正
 */

#ifndef RTC_TIME_H
#define RTC_TIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 公共 API
 * ============================================================ */

/**
 * 初始化 RTC — LSI 时钟, 异步 127 + 同步 255 → ~1Hz
 * 应在 HAL_Init() + SystemClock_Config() 后调用。
 * LSI 已在 IWDG 初始化时使能, 这里只配置 RTC 外设。
 */
void rtc_init(void);

/**
 * 读取当前 Unix 时间戳 (秒, 1970-01-01 00:00:00 UTC)
 */
uint32_t rtc_get_unix(void);

/**
 * 写入 Unix 时间戳 (BLE 授时校正)
 */
void rtc_set_unix(uint32_t unix);

/**
 * 从 RTC 获取当前时间 (时/分/秒, 24h 制)
 */
void rtc_get_hms(uint8_t *hour, uint8_t *minute, uint8_t *second);

/**
 * 从 RTC 获取当前日期 (年/月/日, 公历)
 */
void rtc_get_ymd(uint16_t *year, uint8_t *month, uint8_t *day);

/**
 * 日历 → Unix 时间戳 (软件转换, 不依赖 HAL 日历)
 * @param year   年 (1970-2105)
 * @param month  月 (1-12)
 * @param day    日 (1-31)
 * @param hour   时 (0-23)
 * @param minute 分 (0-59)
 * @param second 秒 (0-59)
 * @return Unix 时间戳
 */
uint32_t rtc_calc_unix(uint16_t year, uint8_t month, uint8_t day,
                       uint8_t hour, uint8_t minute, uint8_t second);

/**
 * 检查 RTC 是否已被配置 (已同步过时间)
 * @return 0=未配置(上电首次), 1=已配置
 */
int rtc_is_configured(void);

#ifdef __cplusplus
}
#endif

#endif /* RTC_TIME_H */
