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

主机测试不模拟真实中断、传感器和无线通信，设备功能验收见 [功能验证](../../../../docs/testing.md)。
