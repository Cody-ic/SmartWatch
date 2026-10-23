# 生产 C 功能回归

测试直接编译实际 UART、原型模型、睡眠模型、睡眠特征、血氧和 CH582M FIFO 代码；`stubs/` 只替代硬件接口。

在已配置 MSVC `cl` 和 Flutter SDK 的 Developer Command Prompt 中，从仓库根目录执行：

```bat
cd app\flutter_app
dart test\fixtures\export_restore.dart test\fixtures\dart_restore.bin
call ..\..\firmware\stm32\tests\host\run.cmd
flutter test --no-pub
```

`run.cmd` 默认使用仓库内的两个固件和 App，可选参数为 CH582M、App 工程绝对路径。产物写入 `%TEMP%\smartwatch-host-regression`。

覆盖原型传输失败保护、重试与事务隔离、血氧过期、完整睡眠窗口及 FIFO 原子写入。Dart 恢复数据交给 C 解析，C 备份数据交给 Flutter 测试解析。

主机测试不模拟真实中断、传感器和无线通信，设备功能验收见 [功能验证](../../../../README.md#功能验证)。

## LSM6DSOX 决策树与采集共存

在同一个 Developer Command Prompt 中，从仓库根目录执行：

```bat
call firmware\stm32\tests\host\run_lsm6dsox.cmd
```

该测试直接编译 `Core/Src/LSM6DSOX.c`；独立的 `lsm6dsox_stubs/` 替代 HAL IIC。模型模拟寄存器组、配置页地址及写入自增，以 ST 官方参考数据检查全部 425 个配置页字节的内容与顺序，并检查：

- 104 Hz、量程、FIFO 水位及 INT1 采集路由保留。
- 从 embedded bank 读取分类，未加载、未产生结果及通信失败返回未知。
- 配置中途失败后关闭配置页写入、禁用残缺模型并恢复 user bank。
- 配置页已接收字节但 HAL 报错时立即终止上传，避免自增地址被重试推进。
- 读取和寄存器组切换失败的重试、恢复，以及重新初始化后的失效处理。
- MLC 两轮寄存器组恢复均失败后，下次 wake-up 与 FIFO 读取重新选择 user bank。
- 独立 wake-up 检测的读后清除及其与 MLC、FIFO 的隔离。

产物位于 `%TEMP%\smartwatch-lsm6dsox-regression`。测试验证驱动控制流程；实际采样时序、片内分类执行、佩戴识别准确率仍需真机验证。
