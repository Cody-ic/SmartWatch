/**
 * @file    rtc_time.c
 * @brief   RTC 时间管理 — LSI 驱动 + BLE 授时校正 (直接寄存器, 不依赖 HAL RTC)
 *
 * [2026-07-03]: 替代 HAL_GetTick() uptime 计时。
 *   RTC 时钟: LSI (~32kHz), 异步预分频 127 + 同步预分频 255 → ~1Hz
 *   使用 CMSIS 寄存器定义 (stm32u585xx.h), 与 MX_IWDG_Init 同风格。
 *   依据: STM32U5 RM0456 §RTC, §RCC_BDCR.
 */

#include "rtc_time.h"
/* [2026-07-04 Fix]: stm32u585xx.h→stm32u575xx.h, 芯片已换型为 STM32U575RITx */
#include "stm32u575xx.h"

/* ---- 内部状态 ---- */
static int rtc_ready = 0;

/* ---- 工具: 闰年判断 ---- */
static int is_leap(uint16_t year)
{
    return (year % 4 == 0) && (year % 100 != 0 || year % 400 == 0);
}

/* ---- Unix → 日历 (前向声明, rtc_set_unix 需要) ---- */
static void unix_to_calendar(uint32_t unix,
                             uint16_t *year, uint8_t *month, uint8_t *day,
                             uint8_t *hour, uint8_t *minute, uint8_t *second);

/* ---- 解锁 RTC 写保护 ---- */
static void rtc_unlock(void)
{
    /* STM32U5: RTC WPR 地址 0x24, 写 0xCA→0x53 解锁, 写 0xFF 锁定 */
    RTC->WPR = 0xCA;
    RTC->WPR = 0x53;
}

static void rtc_lock(void)
{
    RTC->WPR = 0xFF;
}

/* ---- 等待 RTC 寄存器同步 (RSF=1) ---- */
static void rtc_wait_sync(void)
{
    RTC->ICSR &= ~RTC_ICSR_RSF;
    while (!(RTC->ICSR & RTC_ICSR_RSF));
}

/* ---- 防翻转竞争: 读两次一致 ---- */
static uint32_t rtc_read_tr(void)
{
    uint32_t tr;
    do { rtc_wait_sync(); tr = RTC->TR; } while (tr != RTC->TR);
    return tr;
}

static uint32_t rtc_read_dr(void)
{
    uint32_t dr;
    do { rtc_wait_sync(); dr = RTC->DR; } while (dr != RTC->DR);
    return dr;
}

/* ---- BCD → 二进制 ---- */
static uint8_t bcd2bin(uint32_t bcd, uint32_t mask, uint32_t shift)
{
    uint32_t val = (bcd & mask) >> shift;
    return (uint8_t)((val & 0xF) + ((val >> 4) & 0xF) * 10);
}

/* ============================================================
 * 公共 API
 * ============================================================ */

void rtc_init(void)
{
    /* 1. 备份域访问使能 (RM0456 §PWR: PWR_DBPR.DBP=1 才能写 RCC_BDCR + TAMP_BKPxR) */
    PWR->DBPR |= PWR_DBPR_DBP;
    while (!(PWR->DBPR & PWR_DBPR_DBP));  /* 等待生效 */

    /* 2. 使能 LSI (RM0456 §RCC_BDCR: LSION/LSIRDY 在 BDCR 而非 CSR) */
    RCC->BDCR |= RCC_BDCR_LSION;
    uint32_t to = 100000;
    while (!(RCC->BDCR & RCC_BDCR_LSIRDY) && --to);
    if (!to) return;

    /* 2. 选 LSI 为 RTC 时钟 + 使能 RTC */
    RCC->BDCR = (RCC->BDCR & ~RCC_BDCR_RTCSEL_Msk)
              | (0x2UL << RCC_BDCR_RTCSEL_Pos);  /* RTCSEL=10 → LSI */
    RCC->BDCR |= RCC_BDCR_RTCEN;

    /* 3. 解锁 */
    rtc_unlock();

    /* 4. 进初始化 */
    RTC->ICSR |= RTC_ICSR_INIT;
    to = 100000;
    while (!(RTC->ICSR & RTC_ICSR_INITF) && --to);
    if (!to) { rtc_lock(); return; }

    /* 5. 预分频: ASYNC=127, SYNC=255 → 128*256=32768 → ~1Hz */
    RTC->PRER = ((uint32_t)127 << 16) | (uint32_t)255;

    /* 6. 二进制模式 (BCDU=000) + 退出初始化 */
    RTC->ICSR &= ~RTC_ICSR_BCDU_Msk;  /* BCDU=000 → 纯 BCD, 无偏移 */
    RTC->ICSR &= ~RTC_ICSR_INIT;
    to = 100000;
    while ((RTC->ICSR & RTC_ICSR_INITF) && --to);

    rtc_lock();
    rtc_ready = 1;
}

uint32_t rtc_get_unix(void)
{
    if (!rtc_ready) return 0;

    uint32_t tr = rtc_read_tr();
    uint32_t dr = rtc_read_dr();

    uint8_t h = bcd2bin(tr, RTC_TR_HT_Msk | RTC_TR_HU_Msk, RTC_TR_HU_Pos);
    uint8_t m = bcd2bin(tr, RTC_TR_MNT_Msk | RTC_TR_MNU_Msk, RTC_TR_MNU_Pos);
    uint8_t s = bcd2bin(tr, RTC_TR_ST_Msk  | RTC_TR_SU_Msk,  RTC_TR_SU_Pos);
    uint8_t mo= bcd2bin(dr, RTC_DR_MT_Msk | RTC_DR_MU_Msk, RTC_DR_MU_Pos);
    uint8_t d = bcd2bin(dr, RTC_DR_DT_Msk | RTC_DR_DU_Msk, RTC_DR_DU_Pos);
    uint16_t y= 2000 + bcd2bin(dr, RTC_DR_YT_Msk | RTC_DR_YU_Msk, RTC_DR_YU_Pos);

    return rtc_calc_unix(y, mo, d, h, m, s);
}

void rtc_set_unix(uint32_t unix)
{
    if (!rtc_ready) return;

    uint16_t y; uint8_t mo, d, h, m, s;
    unix_to_calendar(unix, &y, &mo, &d, &h, &m, &s);

    /* 编 BCD */
    uint32_t tr = 0;
    tr |= (uint32_t)((h / 10) << RTC_TR_HT_Pos)  & RTC_TR_HT_Msk;
    tr |= (uint32_t)((h % 10) << RTC_TR_HU_Pos)  & RTC_TR_HU_Msk;
    tr |= (uint32_t)((m / 10) << RTC_TR_MNT_Pos) & RTC_TR_MNT_Msk;
    tr |= (uint32_t)((m % 10) << RTC_TR_MNU_Pos) & RTC_TR_MNU_Msk;
    tr |= (uint32_t)((s / 10) << RTC_TR_ST_Pos)  & RTC_TR_ST_Msk;
    tr |= (uint32_t)((s % 10) << RTC_TR_SU_Pos)  & RTC_TR_SU_Msk;

    uint32_t dr = 0;
    uint8_t y2 = (uint8_t)(y - 2000);
    dr |= (uint32_t)((y2 / 10) << RTC_DR_YT_Pos) & RTC_DR_YT_Msk;
    dr |= (uint32_t)((y2 % 10) << RTC_DR_YU_Pos) & RTC_DR_YU_Msk;
    dr |= (uint32_t)((mo / 10) << RTC_DR_MT_Pos) & RTC_DR_MT_Msk;
    dr |= (uint32_t)((mo % 10) << RTC_DR_MU_Pos) & RTC_DR_MU_Msk;
    dr |= (uint32_t)((d  / 10) << RTC_DR_DT_Pos) & RTC_DR_DT_Msk;
    dr |= (uint32_t)((d  % 10) << RTC_DR_DU_Pos) & RTC_DR_DU_Msk;
    dr |= (uint32_t)1 << RTC_DR_WDU_Pos;  /* weekday=1 (Monday), 占位 */

    rtc_unlock();
    RTC->ICSR |= RTC_ICSR_INIT;
    while (!(RTC->ICSR & RTC_ICSR_INITF));
    RTC->TR = tr;
    RTC->DR = dr;
    RTC->ICSR &= ~RTC_ICSR_INIT;
    while ((RTC->ICSR & RTC_ICSR_INITF));

    /* 标记: TAMP 备份寄存器 0 = 0xCAFE */
    TAMP->BKP0R = 0xCAFE;
    rtc_lock();
}

void rtc_get_hms(uint8_t *hour, uint8_t *minute, uint8_t *second)
{
    if (!rtc_ready) { *hour = 0; *minute = 0; *second = 0; return; }
    uint32_t tr = rtc_read_tr();
    *hour   = bcd2bin(tr, RTC_TR_HT_Msk | RTC_TR_HU_Msk, RTC_TR_HU_Pos);
    *minute = bcd2bin(tr, RTC_TR_MNT_Msk | RTC_TR_MNU_Msk, RTC_TR_MNU_Pos);
    *second = bcd2bin(tr, RTC_TR_ST_Msk  | RTC_TR_SU_Msk,  RTC_TR_SU_Pos);
}

void rtc_get_ymd(uint16_t *year, uint8_t *month, uint8_t *day)
{
    if (!rtc_ready) { *year = 2026; *month = 1; *day = 1; return; }
    uint32_t dr = rtc_read_dr();
    *year  = 2000 + bcd2bin(dr, RTC_DR_YT_Msk | RTC_DR_YU_Msk, RTC_DR_YU_Pos);
    *month = bcd2bin(dr, RTC_DR_MT_Msk | RTC_DR_MU_Msk, RTC_DR_MU_Pos);
    *day   = bcd2bin(dr, RTC_DR_DT_Msk | RTC_DR_DU_Msk, RTC_DR_DU_Pos);
}

int rtc_is_configured(void)
{
    if (!rtc_ready) return 0;
    return (TAMP->BKP0R == 0xCAFE) ? 1 : 0;
}

/* ---- Unix → 日历 (公历, 1970-2105) ---- */
static void unix_to_calendar(uint32_t unix,
                             uint16_t *year, uint8_t *month, uint8_t *day,
                             uint8_t *hour, uint8_t *minute, uint8_t *second)
{
    static const uint16_t md_accum[] = {0,31,59,90,120,151,181,212,243,273,304,334,365};
    *second = (uint8_t)(unix % 60);
    *minute = (uint8_t)((unix / 60) % 60);
    *hour   = (uint8_t)((unix / 3600) % 24);
    uint32_t days = unix / 86400;

    uint16_t y = 1970;
    while (1) {
        uint16_t diy = is_leap(y) ? 366 : 365;
        if (days < diy) break;
        days -= diy; y++;
    }
    *year = y;
    uint8_t m = 1;
    while (m <= 12) {
        uint16_t md = md_accum[m] - md_accum[m-1];
        if (m == 2 && is_leap(y)) md = 29;
        if (days < md) break;
        days -= md; m++;
    }
    *month = m;
    *day   = (uint8_t)(days + 1);
}

/* ---- 日历 → Unix ---- */
uint32_t rtc_calc_unix(uint16_t year, uint8_t month, uint8_t day,
                       uint8_t hour, uint8_t minute, uint8_t second)
{
    static const uint16_t md_accum[] = {0,31,59,90,120,151,181,212,243,273,304,334,365};
    if (year < 1970 || year > 2105 || month < 1 || month > 12 || day < 1 || day > 31)
        return 0;

    uint32_t days = (uint32_t)(year - 1970) * 365;
    for (uint16_t y = 1970; y < year; y++)
        if (is_leap(y)) days++;
    days += md_accum[month - 1];
    if (month > 2 && is_leap(year)) days++;
    days += (uint32_t)(day - 1);

    return days * 86400 + (uint32_t)hour * 3600 + (uint32_t)minute * 60 + second;
}
