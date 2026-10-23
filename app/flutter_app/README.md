# SmartWatch 手机端

在本目录运行：

```sh
flutter pub get
flutter analyze
flutter test
flutter run
```

`lib/ble_service.dart` 管理连接与收发，`ble_protocol.dart` 定义消息，`proto_transfer.dart` 处理可靠原型传输，页面负责展示和操作。Android 原生短信桥接位于 `android/app/src/main/`。

当前主要验证 Android；首次使用需处理蓝牙权限，短信由用户配置联系人并授权。备份拉取已有界面，文件导入/导出界面尚未完成。测试不会向真实号码发送短信。

参阅 [App 功能](../../docs/app.md)、[构建步骤](../../docs/build.md) 和 [功能验证](../../docs/testing.md)。
