# STM32 主控固件

在 STM32CubeIDE 中导入本目录的现有工程，执行 Build。`Core/Src/main.c` 负责初始化与 RTOS 任务，`app.c` 负责应用调度，`ble_uart.c` 负责与蓝牙协处理器通信。模型位于 `AI/`，厂商依赖位于 `Drivers/`、`Middlewares/`。

当前使用 `STM32U575RITx_FLASH.ld`，保留现有配置；PCB 标注型号差异不列为当前阻塞项。用户确认实板供电正常，既往烧录异常的原因尚未定位。`App/` 为保留的旧架构，当前未参与编译；不要直接重新生成 CubeMX 代码覆盖手工配置。

参阅 [构建步骤](../../docs/build.md)、[系统架构](../../docs/architecture.md) 和 [功能验证](../../docs/testing.md)。
