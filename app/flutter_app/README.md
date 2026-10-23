# SmartWatch 手机端

Flutter 编写的 BLE 配套 App，提供 Dashboard、Session 和 Profile，Android 原生桥接用于短信请求。

## 构建与测试

配置 Flutter / Dart、Android SDK 和 Java 17，在本目录运行：

```sh
flutter pub get
flutter analyze --no-pub
flutter test --no-pub
flutter run
flutter build apk --debug --no-pub
```

APK 位于 `build/app/outputs/flutter-apk/app-debug.apk`。包名为 `com.example.smartwatch_app`，版本为 `1.4.0+101`。release 仍使用 debug signing，正式发布需配置独立签名和应用标识。`android/local.properties` 由本机工具生成，不提交 SDK 路径。Windows 若提示 symlink 支持，启用开发者模式后重试。

## 代码组织

| 文件 | 职责 |
| --- | --- |
| `lib/ble_protocol.dart` | 消息字段、载荷和备份重组 |
| `lib/proto_transfer.dart` | 逐片确认、超时重试、最终提交 |
| `lib/ble_service.dart` | 扫描、连接、订阅、MTU 分块与串行写入 |
| `lib/dashboard_page.dart` | 仪表盘、会话与个人设置 |
| `lib/emergency_contacts.dart` | 联系人保存与旧数据迁移 |
| `lib/emergency_alert.dart` | 倒计时、取消和告警 |
| `lib/emergency_service.dart`、`android/app/src/main/` | Android 短信桥接 |
| `test/` | 协议、互通、联系人与界面功能测试 |

## 运行与连接

Android 已完成构建和自动化测试；iOS/Windows 尚未完成功能验收，短信桥接为 Android 原生实现。

打开系统蓝牙，授予系统要求的扫描/连接权限。App 扫描 FFE0 服务并识别 SmartWatch 名称，选择设备后订阅 FFE3 通知，使用 FFE2 写入。连接时会发送默认体重 65 kg、年龄 22，并发送当前 Unix 时间。

在 Profile 中修改体重和年龄后可点击 Send to Watch。当前连接流程会再次发送默认配置，因此重连后应重新确认参数；没有实现完整的持久化个人档案恢复流程。

App 内的连接显示来自 Flutter BLE 状态；STM32 `ble_uart_is_connected()` 通过最近 UART 收包超时推断，两者不等价。连接问题要同时检查 GATT 与应用收发。

## Dashboard

主要展示心率、睡眠监测、SpO2、活动、步数、运动评分、告警和最近日志。电量为固件粗估百分比。

- 实时 SpO2 只使用有效的 0x09 数据。收到 invalid、断连或 5 秒没有新有效数据时清除旧显示。
- 睡眠报告使用最新累计 `monitorMin`，不会把 5、10、15 分钟累计成 30 分钟。
- 睡眠报告的最低血氧是历史统计，不作为实时血氧回退值。
- 心率与其他健康指标是当前算法输出，设备佩戴质量、实测误差和模型表现尚未完成验收。

## Session 个性化

连接后可选择 Walking、Jogging、Downstairs、Cycling，开始、结束或中止记录。手表仅接受符合目标活动及置信度要求的 embedding，并在结束时检查最少有效样本数。

界面上的“Recording”表示客户端发起了记录状态，不表示每一帧都通过固件门控；结束也不保证原型一定更新。结合原型版本和各类计数核对实际结果。当前原型驻留手表 RAM，重启后需要恢复或重新采集。

## 原型备份与恢复

Profile 的 Proto Backup / Pull 会向手表请求备份，接收期间禁用重复点击；成功后显示版本、类别数和维度，超时或校验失败会提示失败。

恢复发送逻辑位于 `SmartWatchBLE.sendProtoRestoreFull()`，它等到手表确认完整模型提交才报告成功。当前界面尚未提供完整的文件导入、导出和跨设备备份管理入口，因此这些功能仍是后续产品流程工作。

完整交互见 [原型传输 v2](../../firmware/stm32/PROTOCOL.md)。维护恢复界面时应接入现有服务 API，并显示最终提交结果，不能用“最后一个 BLE write 已返回”替代整包成功。

## 紧急联系人与告警

首次启动的联系人列表为空。更新旧版本时，程序一次性移除精确匹配的两条演示联系人，并在本地保留迁移前的序列化内容供恢复；自行配置的其他联系人保留。

在 Profile 添加姓名、关系和电话号码，并处理 Android SEND_SMS 权限。触发对应安全告警后显示 15 秒倒计时，可点击 I'M OK 取消。倒计时结束时重新读取联系人；没有联系人或没有权限时明确显示未发送原因。

短信正文说明“手表佩戴者可能跌倒/遇险”，不使用收件人的名字作为遇险人。多个接收人逐个请求发送。当前 Android 桥接返回值表示 SmsManager 调用被接受；没有接入运营商送达确认回调。界面已提示这是请求提交，真正送达需单独核实。

测试中的短信调用全部经过模拟 MethodChannel，没有对真实号码发信。设备验收时配置测试接收人，并分别核对取消、权限拒绝、无 SIM、发送失败和实际送达结果。

## 数据与维护位置

联系人保存在 SharedPreferences；BLE 数据主要存在当前页面与服务的内存状态中。当前没有完善的云同步、长期健康数据库或账号系统。

协议字段变化先修改 `ble_protocol.dart`，可靠传输修改 `proto_transfer.dart`，BLE 状态修改 `ble_service.dart`；页面只处理展示和交互操作。测试通过可注入 BLE 服务检查可见 UI 行为，避免把真实设备当作所有逻辑测试的前提。

## 后训练与真机数据

后续后训练按项目约定采用 proto。Session 采集真实活动对应的 embedding 并更新类别原型；模型参数和默认门限需要结合真机数据验证，流程见 [AI README](../../firmware/stm32/AI/README.md)。当前“Recording”状态不等于已有足够有效样本，也不代表训练效果已经验证。

## 常见问题与验证

- **扫描不到：** 检查系统蓝牙、扫描/连接权限及 FFE0 广播；名称位于 Scan Response。
- **连接后无数据：** 检查 FFE3 订阅、FFE2 写入和两固件版本；查看授时、配置、坏帧与解析日志。缺失特征时的 WARNING 也需要处理。
- **血氧消失：** invalid、断连或超时会清除旧值；不能用睡眠历史最低血氧替代实时数据。
- **备份超时：** 检查事务完整性、分片与 MTU，恢复需收到最终提交确认，详见 [协议](../../firmware/stm32/PROTOCOL.md)。
- **短信未收到：** 核对联系人、权限、SIM 和接收手机；提交成功与实际送达分别记录。

18 项 App 测试已通过，短信测试使用模拟通道。与生产 C 的双向互通测试运行顺序见 [主机测试](../../firmware/stm32/tests/host/README.md)。手机与手表的无线联调、实际短信送达和佩戴效果仍需设备验收。
