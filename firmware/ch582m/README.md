# CH582M 蓝牙协处理器固件

使用 MounRiver Studio 导入本目录，执行 Build，生成 `obj/CH582M.elf` 和 HEX。`src/Main.c` 为入口，`src/peripheral.c` 管理 BLE 外设，`src/ble_uart_service/` 提供 GATT 服务，`src/app_drv_fifo/` 实现收发缓冲。

本固件在 UART1 与 BLE 间桥接字节流。生产配置禁用 UART1 DEBUG 输出，避免日志混入手表协议；STM32 与 App 的协议版本需配套。

参阅 [构建步骤](../../docs/build.md)、[通信协议](../../docs/protocol.md) 和 [功能验证](../../docs/testing.md)。
