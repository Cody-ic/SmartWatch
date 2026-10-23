/**
 * @file    battery.c
 * @brief   电池电压监测 — ADC1 注入通道单次转换 + PMOS 开关
 *
 * [2026-07-10 Fix]: PCB 重做, 电池从 ADC4(PB1)/使能 PB0 迁到 ADC1(PA2)/使能 PA4。
 *   - PA2 = ADC1_IN7 (注入通道, 与麦克风常规 DMA 共享 ADC1)
 *   - PA4 = BAT_ADC_EN: 低→Q3(SI2301 P沟道FET) 导通, 分压支路通电
 *   - 分压 R8/R9: 100k/100k = 0.5x (旧板 R15/R30: 1k/4k = 0.8x)
 *   - 注入转换用 HAL ADC_Injected API, 与常规组 DMA 不冲突 (HAL 源码验证:
 *     InjectedStart L491 只查注入 busy, 不碰常规组; InjectedConfigChannel L1991
 *     直接写 JSQR 寄存器, 可在 ADC 已启用时配置)。
 *   手改, 勿重生成 (.ioc 已发散)。
 *
 * [2026-07-03]: 原版。旧板电路: Q1 PMOS S→VBAT+/D→R15(1k)/G→BAT_ADC_EN,
 *   R21(10k) 栅极上拉, 分压 0.8x, PB1=ADC4_IN19。
 */

#include "battery.h"
/* [2026-07-10 Fix]: 原 #include "stm32u575xx.h" 仅 CMSIS 寄存器层(只有 ADC_TypeDef),
 *   缺 HAL ADC 注入 API 的类型/函数(ADC_HandleTypeDef / ADC_InjectionConfTypeDef /
 *   HAL_ADCEx_Injected*) → 编译 32 errors。改 main.h(→ stm32u5xx_hal.h)提供全量 HAL 声明,
 *   同时仍带 CMSIS device 头 → GPIOA->MODER / RCC->AHB2ENR1 直操不受影响。手改, 勿重生成。 */
#include "main.h"

/* ── 外部声明 ────────────────────────────────── */
extern ADC_HandleTypeDef hadc1;   /* adc.c: ADC1 句柄, 麦克风常规组 DMA 共用 */

/* ── 引脚 ─────────────────────────────────────── */
#define BAT_EN_PORT    GPIOA                    /* PA4: BAT_ADC_EN */
#define BAT_EN_PIN     (1U << 4)                /* 低=导通 Q3(SI2301 P沟道), 默认高阻 */

/* ── 分压 ─────────────────────────────────────── */
/* R8/R9 = 100k/100k → Vadc = Vbat × 0.5 → Vbat = Vadc × 2.0
 * 定点: mv = raw * 3300 / 4095 * 2 = raw * 6600 / 4095                         */
#define DIV_NUM        6600UL
#define DIV_DEN        4095UL

/* ── 状态 ─────────────────────────────────────── */
static int inj_ready = 0;        /* 注入通道已配置 */

/* ================================================================
 *  battery_init() — 配置 PA4 (BAT_ADC_EN) + ADC1_IN7 注入通道
 * ================================================================ */
void battery_init(void)
{
    /* 0. 确保 GPIOA 时钟使能 */
    RCC->AHB2ENR1 |= RCC_AHB2ENR1_GPIOAEN;
    __DSB();

    /* 1. PA4 = BAT_ADC_EN: 默认高阻输入 = Q3 关断 (R11 10k 上拉到 VBAT+) */
    GPIOA->MODER &= ~(3U << (4 * 2));     /* Input (hi-Z) */
    GPIOA->PUPDR &= ~(3U << (4 * 2));     /* No pull  */

    /* 2. PA2 = ADC1_IN7: 模拟模式 (已在 adc.c/gpio.c 或 battery_read 时不用,
     *    但在这里显式配一下确保不被别处改成其它模式) */
    GPIOA->MODER |= (3U << (2 * 2));      /* Analog */
    GPIOA->PUPDR &= ~(3U << (2 * 2));     /* No pull */

    /* 3. 配置 ADC1 注入通道 IN7 (HAL 接口) — 此时 ADC1 已 MX_ADC1_Init 过但未开始转换 */
    ADC_InjectionConfTypeDef injCfg = {0};
    injCfg.InjectedChannel      = ADC_CHANNEL_7;              /* ADC1_IN7 = PA2 */
    injCfg.InjectedRank         = ADC_INJECTED_RANK_1;
    injCfg.InjectedSamplingTime = ADC_SAMPLETIME_814CYCLES;   /* 长采样: 100k 分压源阻抗高 */
    injCfg.InjectedSingleDiff   = ADC_SINGLE_ENDED;
    injCfg.InjectedOffsetNumber = ADC_OFFSET_NONE;
    injCfg.InjectedOffset       = 0;
    injCfg.InjectedNbrOfConversion = 1;
    injCfg.ExternalTrigInjecConv    = ADC_INJECTED_SOFTWARE_START;
    injCfg.ExternalTrigInjecConvEdge = ADC_EXTERNALTRIGINJECCONV_EDGE_NONE;
    injCfg.AutoInjectedConv      = DISABLE;
    injCfg.InjectedDiscontinuousConvMode = DISABLE;
    injCfg.InjecOversamplingMode = DISABLE;

    if (HAL_ADCEx_InjectedConfigChannel(&hadc1, &injCfg) != HAL_OK) return;

    inj_ready = 1;
}

/* ================================================================
 *  battery_read_mv() — 导通分压 → 注入采样 → 关断分压, 返回 mV
 * ================================================================ */
uint16_t battery_read_mv(void)
{
    if (!inj_ready) return 0U;

    /* 1. 导通分压支路: PA4 输出低 → Q3 P沟道 Vgs=-VBAT → 导通 */
    GPIOA->MODER |= (1U << (4 * 2));          /* PA4: Output */
    BAT_EN_PORT->BRR = BAT_EN_PIN;            /* BRR 写 1 拉低 → FET ON */
    for (volatile int i = 0; i < 5000; i++);  /* ~0.5ms 稳定 (分压 100k, C_adc ~5pF, τ≈0.5µs, 取 1000× margin) */

    /* 2. 注入转换 (软件触发, 阻塞轮询) */
    uint32_t raw = 0U;  /* 先声明 (goto 前), C11 mixed-decl 安全 */
    if (HAL_ADCEx_InjectedStart(&hadc1) != HAL_OK) {  /* 幂等: ADC 已启用则只触发 JADSTART */
        goto restore;
    }
    if (HAL_ADCEx_InjectedPollForConversion(&hadc1, 5) != HAL_OK) {  /* 5ms 超时 */
        (void)HAL_ADCEx_InjectedStop(&hadc1);
        goto restore;
    }
    raw = HAL_ADCEx_InjectedGetValue(&hadc1, ADC_INJECTED_RANK_1);
    (void)HAL_ADCEx_InjectedStop(&hadc1);

    /* 3. 关断分压支路: PA4 回到高阻输入, R11 上拉 → Vgs=0 → FET OFF */
restore:
    GPIOA->MODER &= ~(3U << (4 * 2));         /* PA4: Input (hi-Z) */

    if (!raw) return 0U;  /* raw==0 表示步骤2失败 */

    /* Vbat_mV = raw * 6600 / 4095 (+0.5 四舍五入) */
    uint32_t mv = (raw * DIV_NUM + (DIV_DEN / 2U)) / DIV_DEN;
    return (uint16_t)mv;
}

/* ================================================================
 *  battery_read_pct() — LiPo 粗略: 4200mV=100%, 3700mV=0%
 * ================================================================ */
uint8_t battery_read_pct(void)
{
    uint16_t mv = battery_read_mv();
    if (mv >= 4200U) return 100U;
    if (mv <= 3700U) return 0U;
    return (uint8_t)((uint32_t)(mv - 3700U) * 100U / 500U);
}
