# Repository Guidelines

## 项目结构

`firmware/stm32/` 包含主控固件，应用入口为 `Core/Src/main.c` 和 `app.c`，`AI/` 保存模型，`Drivers/` 与 `Middlewares/` 保存依赖。`firmware/ch582m/` 包含 BLE/UART 桥接固件，入口为 `src/Main.c`。`app/flutter_app/` 包含手机端，`lib/` 是业务代码，`test/` 是测试。设计和构建说明写在各模块的 `README.md`，协议规范位于 `firmware/stm32/PROTOCOL.md`，硬件资料位于 `hardware/`。

## 构建与测试

STM32CubeIDE 导入 `firmware/stm32/`，MounRiver 导入 `firmware/ch582m/` 后执行 Build。首次生成构建目录后可分别运行 `make -C firmware/stm32/Debug -j8 all`、`make -C firmware/ch582m/obj -j8 all`。需把对应工具链加入 PATH。

在 `app/flutter_app/` 执行 `flutter pub get`、`flutter analyze`、`flutter test` 和 `flutter build apk --debug`。生产 C 回归使用 `firmware/stm32/tests/host/run.cmd`，需 MSVC Developer Command Prompt；双向协议测试的执行顺序见 `firmware/stm32/tests/host/README.md`。

## 编码约定

C 沿用相邻文件缩进，模块采用 `snake_case.c/.h`，宏采用大写；LVGL 调用集中在 Sensor 任务。Dart 使用 `dart format`，遵守 `analysis_options.yaml` 的 Flutter lint。避免批量格式化生成代码或第三方依赖。

## 功能验证

新增 C 回归放入 `tests/host/`，Flutter 测试以 `_test.dart` 结尾。协议变动须同步三端并验证双向收发、重试和失败保护。目前没有覆盖率门槛；功能结论需区分自动逻辑测试与真机实测，并更新根目录 README 的功能验证表。不要直接通过 CubeMX 覆盖手工外设配置。

## 提交与拉取请求

使用简短模块主题，例如 `fix: 修复血氧过期显示`、`docs: 更新连接步骤`。PR 描述目的、影响模块、测试结果及相关问题；UI 改动附截图，协议改动说明兼容性。不要提交构建缓存、临时备份、签名密钥或本机 SDK 路径，保留第三方版权声明。

## 文档与 AI 维护约定

根 README 说明完整项目和功能进度，各模块 README 说明对应构建、使用和排障；长协议、PCB 检查等专项内容才拆成独立文件。使用项目自身的客观叙述，避免记录对话、执行过程或对话参与者的来源描述。PCB 工程以可能存在问题、尚未完整验证的设计资料公开。AI 参数需要真机数据验证；后续后训练约定采用 proto，流程与能力边界见 `firmware/stm32/AI/README.md`。
