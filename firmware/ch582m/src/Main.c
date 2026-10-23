/**
 * @file    Main.c
 * @brief   BLE UART 透传固件 — CH582M 端 (智能手表 BLE 协处理器)
 *
 * 接收 STM32U575RGT6(LPUART1) 数据 → BLE Notify 发手机
 * 接收手机 BLE Write → UART1 发 STM32U575RGT6
 *
 * BLE 名称: "SmartWatch"（在 peripheral.c Scan Response 中配置）
 * [2026-07-11 Fix]: 硬件连接从 UART2(PA6/PA7) 迁至 UART1(PA8/PA9)
 *   网表真值: CH582M PA8(RXD1) ← STM32U575 PA9(LPUART1_TX)
 *             CH582M PA9(TXD1) → STM32U575 PA10(LPUART1_RX)
 *
 * 备份: Main.c.bak (原始 UART 回环例程)
 */

#include "CH58x_common.h"
#include "HAL.h"
#include "gattprofile.h"
#include "peripheral.h"

/* TMOS 内存池 */
__attribute__((aligned(4))) uint32_t MEM_BUF[BLE_MEMHEAP_SIZE / 4];

#if (BLE_MAC == FALSE)
const uint8_t MacAddr[6] = {0};
#endif

extern void app_uart_process(void);
/* [2026-07-05 FIX]: 原缺失 app_uart_init extern声明, 编译器报 implicit declaration,
 *   函数签名 void app_uart_init(void) 定义在 peripheral.c:66, 需声明后调用 */
extern void app_uart_init(void);

/*********************************************************************
 * @brief  TMOS 系统主循环
 */
__HIGH_CODE
void Main_Circulation(void)
{
    while (1) {
        TMOS_SystemProcess();
        app_uart_process();
    }
}

/*********************************************************************
 * @brief  主函数
 */
int main(void)
{
    SetSysClock(CLK_SOURCE_PLL_60MHz);

    CH58X_BLEInit();
    HAL_Init();
    GAPRole_PeripheralInit();
    Peripheral_Init();
    /* [2026-07-03 FIX]: 原缺失 app_uart_init() 调用, UART1 未初始化,
     *   GPIO/中断/DMA 均未配置, BLE透传不通。Peripheral_Init 后立即初始化UART1。 */
    app_uart_init();

    Main_Circulation();
}
