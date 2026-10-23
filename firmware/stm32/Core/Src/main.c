/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os2.h"
#include "adc.h"
#include "gpdma.h"
#include "i2c.h"
#include "icache.h"
#include "usart.h"
#include "spi.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include "rtc_time.h"  /* [2026-07-03]: RTC 计时替代 HAL_GetTick */
#include "battery.h"   /* [2026-07-03]: 电池电压监测 */
#include "power.h"     /* [2026-07-03]: 开关机电源管理 */
#include "task.h"
#include "event_groups.h"
#include "semphr.h"
#include "LSM6DSOX.h"
#include "MAX30102.h"
#include "spu0410.h"
#include "app.h"
#include "hr_detector.h"
#include "safety_detect.h"
#include "ble_uart.h"           /* BLE UART 驱动 (CH582M 透传模块) */
#include "task_kws.h"           /* [2026-07-04]: KWS 关键词唤醒 */
#include "spo2.h"               /* [2026-07-05]: SpO2 血氧估算 */
#include "sleep_features.h"     /* [2026-07-05]: 睡眠分期特征提取 */
#include "sleep_model.h"        /* [2026-07-05]: 睡眠分期推理 */
_Static_assert(SF_INPUT_DIM == SLEEP_N_FEATURES, "Sleep feature/model dimensions differ");
/* USER CODE END Includes */

/* [2026-07-05]: SpO2 全局变量 — Sensor_Task 更新, app_tick/UI/BLE 读取 */
extern volatile float  g_spo2_value;
extern volatile uint8_t g_spo2_valid;
extern volatile uint32_t g_spo2_last_valid_ms;  /* [2026-07-06] SpO2 超时清零 */
/* [2026-07-05]: 睡眠概率 — sleep_model_predict 输出, sleep_apnea/UI 读取 */
extern volatile float  g_sleep_prob;

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
// FreeRTOS 同步对象
EventGroupHandle_t xMicEvents = NULL;
SemaphoreHandle_t xSensorSemaphore = NULL;

// 传感器数据缓冲区
int16_t acBuf[26 * 3];
int16_t gyBuf[26 * 3];
int16_t tempBuf;
int16_t MovingAverage_ac[25 * 3];
int16_t MovingAverage_gy[25 * 3];
uint32_t PPG_red_buf[25];
uint32_t PPG_ir_buf[25];

/* [2026-07-07 Fix]: spo2_state_t (3280B, 含 800 float 环缓) 原是 Sensor_Task 局部变量,
 *   占任务栈空间的 53% (3280/6144)。最深调用链 Sensor_Task(4480) → sleep_features_get(1176)
 *   → compute_epoch(232) → extract_ibi(872) = 6760B > 6144B, 溢出 632B。
 *   移到 BSS (文件级 static), Sensor_Task 栈使用从 4480B 降至 ~1200B, 最深链 ~3400B, 裕度 43%。
 *   交叉验证: GCC -fstack-usage .su 文件 + 手动 struct 核算 + ARM AAPCS 调用约定。
 *   spo2_init() 在 Sensor_Task 入口处仍然调用 (BSS 零初始化之上显式重置)。 */
static spo2_state_t spo2;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void SystemPower_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */
void Sensor_Task(void *argument);
void Mic_Task(void *argument);
void IWDG_Init(void);   /* [2026-08-26 Fix]: IWDG 初始化+启动, 定义见 USER CODE BEGIN 4 */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
 * @brief  外部中断回调：LSM6DSOX FIFO 触发后释放信号量
 */
/* [2026-07-05 FIX] STM32U5 HAL 使用 Rising_Callback 而非旧系列 EXTI_Callback.
 * STM32U5 HAL 驱动 stm32u5xx_hal_gpio.c:786 调用 HAL_GPIO_EXTI_Rising_Callback,
 * 原 HAL_GPIO_EXTI_Callback 是死代码 — IMU/HAR/心率处理从未执行。 */
void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_4)  /* [2026-07-08]: PCB 重做 PA0→PB4 */
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(xSensorSemaphore, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/**
 * @brief  从 26 个样本线性插值到 25 个（与原有逻辑完全一致）
 */
static void DownSampleFrom26To25(int16_t *acc_in, int16_t *gyr_in, int16_t *acc_out, int16_t *gyr_out)
{
    for (uint8_t i = 0; i < 25; i++)
    {
        float pos = i * 26.0f / 25.0f;
        float weight = pos - (float)i;

        for (uint8_t j = 0; j < 3; j++)
        {
            int16_t v0 = acc_in[i * 3 + j];
            int16_t v1 = acc_in[(i + 1) * 3 + j];
            acc_out[i * 3 + j] = (int16_t)(v0 * (1.0f - weight) + v1 * weight);
        }
        for (uint8_t j = 0; j < 3; j++)
        {
            int16_t v0 = gyr_in[i * 3 + j];
            int16_t v1 = gyr_in[(i + 1) * 3 + j];
            gyr_out[i * 3 + j] = (int16_t)(v0 * (1.0f - weight) + v1 * weight);
        }
    }
}

/**
 * @brief  传感器任务：等待 FIFO 中断，读取并处理数据
 */
void Sensor_Task(void *argument)
{
    // 心率检测器状态
    hr_state_t hr_state;
    hr_detector_init(&hr_state, 100.0f);  /* [2026-07-04]: 传入采样率 */

    /* [2026-07-05]: SpO2 血氧估算 — 复用 PPG 双通道数据, 每 2s 更新.
     *   [2026-07-07]: spo2 已从栈移至文件级 BSS, 此处仅做显式初始化。 */
    spo2_init(&spo2);

    for (;;)
    {
        // 30ms 超时等待 FIFO 中断，确保 LVGL 持续刷新
        if (xSemaphoreTake(xSensorSemaphore, pdMS_TO_TICKS(30)) == pdTRUE)
        {
            uint8_t samples = LSM6DSOX_GetFifoBatch(acBuf, gyBuf, &tempBuf, 26);
            if (samples == 26)
            {
                /* [2026-07-04 Fix]: 检查 MAX30102 实际读取样本数。
                 *   原代码忽略返回值、无条件循环 25 次——当 FIFO 空 (I2C 故障/上溢)
                 *   时 PPG_ir_buf 保留旧数据, hr_detector 处理过期值→虚假 BPM。
                 *   交叉验证: MAX30102 DS p13 (FIFO 溢出丢数据) + MAX30102.c:147
                 *   (return read_actul, 可返回 0) + Arduino check() (仅处理实际样本数) */
                uint8_t ppg_samples = MAX30102_GetFifoBatch(PPG_red_buf, PPG_ir_buf, 25);
                DownSampleFrom26To25(acBuf, gyBuf, MovingAverage_ac, MovingAverage_gy);

                /* [2026-07-05]: 睡眠分期 — 每 250ms 喂 IMU+PPG 数据到特征提取环缓 */
                {
                    float hr_now = (hr_state.signal_quality > 50) ? hr_state.bpm_smoothed : 72.0f;
                    float temp_c = (float)tempBuf / 256.0f + 25.0f;  /* LSM6DSOX internal temp */
                    sleep_features_feed(acBuf, PPG_ir_buf, ppg_samples, hr_now, temp_c);
                }

                // === IMU → HAR 推理 + UI + 安全检测 ===
                app_on_imu_batch(MovingAverage_ac, MovingAverage_gy);

                // === PPG → 心率检测 + SpO2 ===
                for (int i = 0; i < ppg_samples; i++)
                {
                    /* [2026-07-05]: SpO2 喂入 — 与心率复用同一组 PPG 样本 */
                    spo2_feed(&spo2, PPG_red_buf[i], PPG_ir_buf[i], HAL_GetTick());

                    if (hr_detector_update(&hr_state, (int32_t)PPG_ir_buf[i]))
                    {
                        app_on_hr_ready(
                            (int)hr_state.bpm_smoothed,
                            (uint16_t)(PPG_ir_buf[i] & 0xFFFF),
                            hr_state.signal_quality > 50,
                            hr_state.resting_hr);  /* [2026-07-04]: 同步静息心率 */
                    }
                }


            }
            else
            {
                /* FIFO 下溢/部分批次：跳过 PPG 读取和处理，防止不完整数据进入 HAR/HR */
                static uint32_t fifo_underrun_cnt = 0;
                fifo_underrun_cnt++;
                (void)fifo_underrun_cnt; /* 调试器可见的遥测计数器 */
            }
        }

        /* Expire independently of IMU/PPG interrupts; retain the measurement time. */
        spo2_expire(&spo2, HAL_GetTick());
        g_spo2_valid = spo2_is_valid(&spo2) ? 1 : 0;
        g_spo2_value = spo2_get(&spo2);
        g_spo2_last_valid_ms = spo2.last_valid_ms;

        // 无论有无传感器数据，持续驱动 LVGL + BLE 数据收发
        app_tick();
        ble_uart_process();

        /* The deployed model is binary: [P(wake), P(sleep)]. */
        if (sleep_features_ready()) {
            float feats[SLEEP_N_FEATURES], probs[SLEEP_N_OUTPUTS];
            sleep_features_get(feats);
            sleep_model_predict(feats, probs);
            g_sleep_prob = probs[1];
            ble_uart_send_sleep_stage(g_sleep_prob, g_sleep_prob > 0.5f);
        }

        /* [2026-07-04]: MLC 静止暗屏 + 抬腕亮屏 (省电)
         * [2026-07-11 Fix]: 移除 MLC 动态 ODR 降频 (原静止 10s → IMU 26Hz / PPG 50Hz)。
         *   根因: 降频改采样率, 但 sleep_features(硬编码 ACC_FS=104/PPG_FS=100)与 HAR 均按固定
         *   采样率处理、无自适应; 而降频触发条件(静止)恰是睡眠主场景 → 睡眠时 sleep_features 收到
         *   采样率错 2-4× 的数据、睡眠分期不可用(PPG 50Hz 直接毁 HRV, IBI 无法重采样救回)。
         *   ODR 降频省的传感器功耗有限, 主力省电在 tickless WFI + 暗屏, 故砍掉降频、IMU/PPG 恒定
         *   104/100Hz 保 AI 数据正确。LSM6DSOX_SetODR / MAX30102_SetSampleRate /
         *   hr_detector_set_sample_rate 随之无调用者(保留为备用 API)。暗屏/亮屏保留。
         *   见 memory driver-verify-20260711。手改勿重生成。 */
        {
            static uint8_t  mlc_tick = 0;
            static uint32_t stationary_since = 0;
            static uint8_t  display_dimmed = 0;   /* 屏幕是否已变暗 */
            static lv_obj_t *dim_overlay = NULL;  /* 暗屏覆盖层 */
            #define MLC_CHECK_INTERVAL   2000   /* 每 2s 检查一次 MLC */
            #define DISPLAY_DIM_TIMEOUT 60000   /* 静止 1min 后暗屏 */

            if (mlc_tick++ >= (MLC_CHECK_INTERVAL / 30)) {
                mlc_tick = 0;
                uint8_t mlc = LSM6DSOX_MLC_GetResult();
                uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

                if (mlc == MLC_STATIONARY) {
                    if (stationary_since == 0)
                        stationary_since = now;

                    /* 静止 60s → 暗屏省电 (GC9A01 无背光引脚, 软件覆盖层模拟) */
                    if (!display_dimmed && (now - stationary_since > DISPLAY_DIM_TIMEOUT)) {
                        display_dimmed = 1;
                        dim_overlay = lv_obj_create(lv_scr_act());
                        lv_obj_set_size(dim_overlay, 240, 240);
                        lv_obj_set_style_bg_color(dim_overlay, lv_color_black(), 0);
                        lv_obj_set_style_bg_opa(dim_overlay, LV_OPA_80, 0);
                        lv_obj_clear_flag(dim_overlay, LV_OBJ_FLAG_CLICKABLE);
                    }
                } else {
                    /* 运动检测 → 抬腕亮屏 */
                    if (display_dimmed) {
                        display_dimmed = 0;
                        if (dim_overlay) { lv_obj_del(dim_overlay); dim_overlay = NULL; }
                    }
                    stationary_since = 0;
                }
            }
        }
    }
}

/**
 * @brief  麦克风任务：等待 ADC 帧就绪，检测喊叫声
 */
void Mic_Task(void *argument)
{
    for (;;)
    {
        EventBits_t bits = xEventGroupWaitBits(
            xMicEvents,
            0x01,               // EVT_MIC_FRAME_READY
            pdTRUE,             // 读取后自动清除标志位
            pdFALSE,
            portMAX_DELAY);

        if (bits & 0x01)
        {
            int shouting = mic_is_shouting();
            /* 喂入安全引擎：喊叫 + 霸凌检测联动 */
            safety_feed_shout(xTaskGetTickCount() * portTICK_PERIOD_MS, shouting);
        }
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* [2026-07-10]: power_init() 停用 — 新板 XC6194A 硬件管电源, MCU 无需初始化 PA10。
   *   power.c 已整体 #if 0。PA10 将在 BLE(USART1) 迁移中改作 RX。用 #if 0 停用保留。 */
#if 0
  power_init();
#endif
  /* USER CODE END Init */

  /* Configure the System Power */
  SystemPower_Config();

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* [2026-07-10]: power_self_lock() 停用 — 新板无 PMOS 自锁回路, XC6194A 硬件维持供电。 */
#if 0
  power_self_lock();
#endif
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_GPDMA1_Init();
  MX_I2C1_Init();
  MX_ICACHE_Init();
  MX_ADC1_Init();
  MX_USART1_UART_Init();  /* [2026-07-10 Fix]: BLE LPUART1→USART1 (PA9/PA10), 手改勿重生成 */
  MX_SPI2_Init();  /* [2026-07-10 Fix]: 显示 SPI1→SPI2 (PB13/PB15), 手改勿重生成 */
  /* USER CODE BEGIN 2 */

    /* [2026-08-26 Fix]: 补 rtc_init() — 此前无任何调用者, rtc_ready 恒 0 → RTC 时间恒默认值、
     *   BLE 0x91 授时(rtc_set_unix)直接 early-return、app.c:177 的 rtc_is_configured() 恒 0。
     *   必须在 app_init() 读 rtc_is_configured() 之前调用; rtc_time.h:30 注明须在
     *   HAL_Init + SystemClock_Config 之后(此处已满足)。 */
    rtc_init();

    // 1. 创建 FreeRTOS 同步对象（须在调度器启动前完成）
    xMicEvents = xEventGroupCreate();
    if (xMicEvents == NULL) Error_Handler();

    xSensorSemaphore = xSemaphoreCreateBinary();
    if (xSensorSemaphore == NULL) Error_Handler();

    // 2. 初始化传感器（I2C1 已就绪）
    if (MAX30102_Init(&hi2c1) != 0) Error_Handler();
    if (LSM6DSOX_Init(&hi2c1) != 0) Error_Handler();
    if (LSM6DSOX_MLC_Init() != 0) Error_Handler();   /* MLC 硬件静止/运动检测 */

    // 3. 初始化电池监测（须在 mic_init 之前: ADC1 注入通道 IN7 的采样时间/PCSEL
    //    在 ADC 运行时硬件跳过, 需 ADC 已 init 但未启动 DMA 时配置）
    /* [2026-07-10 Fix]: battery_init 提到 mic_init 前, 否则注入通道 SMPR 被 ADC 运行态忽略,
     *   100k 高阻分压 → 采样不足 → 读数偏低。HAL 源码确认: PCSEL |= 叠加安全, SMPR 需 ADSTART=0。 */
    battery_init();

    // 4. 初始化麦克风（内部自动校准直流偏置，需 ADC DMA 正常）
    if (mic_init() != 0) Error_Handler();

    // 4. 初始化应用层（LVGL + UI + HAR引擎 + 安全检测）
    app_init();

    // 4.5. 初始化 BLE UART（CH582M 透传模块，LPUART1）
    ble_uart_init();

    // 5. 创建任务
    BaseType_t task_ret;
    /* [2026-07-05] 栈从 1024w(4KB) → 1536w(6KB): LVGL lv_timer_handler + HAR推理
     * + M33 FPU 自动压栈(68B) + 安全检测 + BLE 发送 需要更多栈空间。
     * 4KB 在高负载时已非常紧张, 6KB 留出安全余量。 */
    task_ret = xTaskCreate(Sensor_Task, "SensorTask", 1536, NULL, 2, NULL);
    if (task_ret != pdPASS) Error_Handler();

    task_ret = xTaskCreate(Mic_Task, "MicTask", 512, NULL, 1, NULL);
    if (task_ret != pdPASS) Error_Handler();

    /* [2026-07-04]: KWS 关键词唤醒 — 共用 Mic 数据源, 优先级同 Mic_Task */
    task_ret = xTaskCreate(TaskKWS, "KWSTask", 1024, NULL, 1, NULL);
    if (task_ret != pdPASS) Error_Handler();

    /* [2026-08-26 Fix]: 补 IWDG 启动 — 此前只有 IWDG_Refresh() 喂狗(app.c:275)而无任何
     *   IWDG 初始化(PR/RLR/KR=0xCCCC 全缺), 看门狗从未启动 → 4s 保护不生效。
     *   须在所有耗时初始化(传感器/app_init/ble_uart)完成之后、调度器启动之前:
     *   启动后 Sensor_Task(prio 2) 首循环 30ms 内即喂狗, 窗口 << 4s, 无复位风险。 */
    IWDG_Init();

    // 6. 启动 FreeRTOS 调度器（此后 main() 不再返回）
    vTaskStartScheduler();

    // 调度器若返回，则出错
    Error_Handler();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_0;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLMBOOST = RCC_PLLMBOOST_DIV4;
  RCC_OscInitStruct.PLL.PLLM = 3;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 1;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLLVCIRANGE_1;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Power Configuration
  * @retval None
  */
static void SystemPower_Config(void)
{

  /*
   * PWR Privilege Configuration
   */
  HAL_PWR_ConfigAttributes(PWR_ALL, PWR_NSEC_PRIV);
/* USER CODE BEGIN PWR */
/* USER CODE END PWR */
}

/* USER CODE BEGIN 4 */

/**
 * @brief  FreeRTOS 空闲钩子——无任务就绪时进入睡眠省电
 *
 * configUSE_TICKLESS_IDLE=1 配合此钩子:
 *   Sensor_Task 等信号量 (30ms)、Mic_Task 等事件组期间，
 *   CPU 通过 WFI 进入 SLEEP 模式，外设时钟保持运行。
 *   任何中断 (IMU INT1, ADC DMA, LPUART, SysTick) 自动唤醒。
 *   预估功耗降低 30-50%。
 */
void vApplicationIdleHook(void)
{
    HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
}

/**
  * @brief  初始化并启动独立看门狗（IWDG）— 直接寄存器访问, 不依赖 HAL
  * @note   LSI ≈ 32kHz, 预分频 /256 (PR=0x06) → 125Hz, 重装载 500 → 4s 超时
  * [2026-08-26 Fix]: 补回 — 此前 IWDG 只有 IWDG_Refresh() 喂狗、无初始化, 看门狗从未启动。
  *   原实现见 .backup/2026-07-04_before_MX/main.c:447-465。
  *   交叉验证: ST HAL IWDG (stm32u5xx_hal_iwdg.c) 写 PR+RLR 后统一轮询 PVU|RVU,
  *   确保配置寄存器更新完毕再启动, 防以错误预分频运行。
  */
void IWDG_Init(void)
{
    IWDG->KR = 0x5555;                              /* 解锁 */
    IWDG->PR = 0x06;                                /* /256 → 125Hz */
    IWDG->RLR = 500;                                /* 500 ticks → 4s */
    while (IWDG->SR & (IWDG_SR_PVU | IWDG_SR_RVU)); /* 等 PVU+RVU 更新完成 */
    IWDG->KR = 0xCCCC;                              /* 启动 (此后不可停) */
    IWDG->KR = 0xAAAA;                              /* 首次喂狗 */
}

/**
  * @brief  喂独立看门狗 — 直接寄存器访问
  */
void IWDG_Refresh(void)
{
    IWDG->KR = 0xAAAA;
}

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  /* 等待 ~1 秒让调试器有机会连接，然后触发系统复位 */
  for (volatile uint32_t i = 0; i < 10000000; i++) { __NOP(); }
  NVIC_SystemReset();
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
