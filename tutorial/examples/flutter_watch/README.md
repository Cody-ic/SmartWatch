# 离线仪表盘实验

这一小程序只使用模拟电量，不连接蓝牙，不发送短信。

先把此目录复制到独立练习位置，运行 `flutter pub get`、`flutter test`。需要运行 Android 时，在练习副本内运行 `flutter create --platforms=android .`，再执行 `flutter run`。平台生成器可能重写示例测试，保留本目录的 `lib/main.dart` 和 `test/widget_test.dart` 后再运行。

正文从静态页面、按钮状态、异步数据逐步解释这个最终小程序。真实手表连接见支线三的蓝牙篇；CH582M 桥接实现见支线二；完整 App 对照仓库 `app/flutter_app`。
