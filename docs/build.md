# 构建环境与开发步骤

## 1. 环境基线

| 工程 | 已使用的环境 | 构建产物 |
| --- | --- | --- |
| STM32 | STM32CubeIDE 2.1.0，GNU Arm 14.3.rel1，工程 C11 配置 | `Debug/SmartWatch.elf`、map、反汇编列表 |
| CH582M | MounRiver Studio 2，RISC-V Embedded GCC，RV32IMAC / ILP32，GNU99 | `obj/CH582M.elf`、`.hex`、`.lst` |
| Flutter | 本地 Flutter SDK、Dart、Android SDK、Java 17 工程配置 | `build/app/outputs/flutter-apk/app-debug.apk` |
| C 主机回归 | MSVC Build Tools，`cl /std:c11` | 测试可执行文件与双向协议 fixture |

三份源码工程分别存在于本地目录，详见仓库首页。先在相应 IDE 导入完整工程；厂商 HAL、运行时和生成网络文件需随工程具备。文档仓库本身不包含可独立构建全部固件的源码树。

## 2. STM32 编译

推荐先用 CubeIDE 导入 `D:\STM32Dev\SmartWatch` 并执行 Build。独立 PowerShell 可使用本机实际工具链路径：

```powershell
$plugins = 'D:\STM32Dev\STM32CubeIDE_2.1.0\STM32CubeIDE\plugins'
$gcc = Join-Path $plugins 'com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740\tools\bin'
$make = Join-Path $plugins 'com.st.stm32cube.ide.mcu.externaltools.make.win32_2.2.100.202601091506\tools\bin\make.exe'
$env:PATH = "$gcc;$env:PATH"
Set-Location D:\STM32Dev\SmartWatch\Debug
& $make -j16 all
```

工具链随安装版本变化，迁移开发机时调整目录。需要全量构建时，先在 `Debug` 中执行 `& $make clean`，再执行 `all`。检查命令退出码与链接完成信息，不仅检查 ELF 文件是否存在。

当前链接脚本为 `STM32U575RITx_FLASH.ld`，FLASH=2048 KiB、主 RAM=768 KiB；网表器件却为 RGT6 / 1024 KiB。最终硬件型号确认后再统一，不能把当前产物仍小于 1 MiB 当作已解决选型不一致。

### 生成代码维护约束

`.ioc` 与若干手工迁移后的初始化存在发散。不要直接使用 CubeMX Generate Code 覆盖。引脚改动同时核对 `main.h`、`gpio.c`、`spi.c`、`i2c.c`、`usart.c`、中断处理与网表。

AI 源文件排除列表保存在 `.cproject`。若出现重复网络符号，先检查是否错误纳入旧 `network*.c`；不要通过删除当前模型包装来消除链接错误。

## 3. CH582M 编译

用 MounRiver 导入 `D:\CH32Dev\CH582M` 后首次 Build，生成/更新 `obj` 构建文件。现有本机命令为：

```powershell
$mrs = 'D:\MounRiveStudio\MounRiver_Studio2\resources\app\resources\win32'
$gcc = Join-Path $mrs 'components\WCH\Toolchain\RISC-V Embedded GCC\bin'
$make = Join-Path $mrs 'others\Build_Tools\Make\bin\make.exe'
$env:PATH = "$gcc;$env:PATH"
Set-Location D:\CH32Dev\CH582M\obj
& $make -j8 all
# 需要强制重新编译时：
# & $make -B -j8 main-build
```

应使用 `Build_Tools/Make/bin/make.exe`；同一安装中另一份 make 曾出现递归 `all` 失败。当前配置保留 `BLE_BUFF_MAX_LEN=251`，禁用 `DEBUG`。`DEBUG=1` 会选择 UART1，生产代码对这一冲突配置设置了编译错误。

项目配置 `.cproject` 是持续维护的来源；`obj/*/subdir.mk` 是现有构建实例，IDE 再生成后也要复核有效参数。

## 4. Flutter 与 APK

```powershell
Set-Location D:\SmartWatchApp\flutter_app
& D:\Flutter\flutter\bin\flutter.bat pub get
& D:\Flutter\flutter\bin\flutter.bat analyze --no-pub
& D:\Flutter\flutter\bin\flutter.bat test --no-pub
& D:\Flutter\flutter\bin\flutter.bat build apk --debug --no-pub
```

`pub get` 成功后再使用 `--no-pub`。应用包名为 `com.example.smartwatch_app`，源码版本为 `1.4.0+101`。当前交付的是 Debug APK；Android 工程的 release 配置仍引用 debug signing，正式发行前需要单独配置应用标识、签名和版本管理。

`android/local.properties` 保存本机 SDK 路径，迁移开发机时由工具重新生成。不要将本机 SDK 路径视为其他开发者必须照抄的配置。

## 5. 主机 C 回归

在已配置 MSVC 的 Developer Command Prompt 中：

```bat
cd /d D:\SmartWatchApp\flutter_app
D:\Flutter\flutter\bin\dart.bat test\fixtures\export_restore.dart test\fixtures\dart_restore.bin
D:\STM32Dev\SmartWatch\tests\host\run.cmd D:\CH32Dev\CH582M D:\SmartWatchApp\flutter_app
D:\Flutter\flutter\bin\flutter.bat test --no-pub
```

本机 MSVC 环境也可由 `D:\tools\BuildTools\VC\Auxiliary\Build\vcvars64.bat` 初始化。主机产物写入 `%TEMP%\smartwatch-host-regression`。先生成 Dart 恢复 fixture，再让 C 测试生成备份 fixture，最后运行 Flutter 测试，避免跨语言文件顺序错误。

## 6. 烧录前准备

当前硬件存在 [整改项](pcb-review.md)，用户暂未安排烧录。整改并完成连接确认后，STM32 使用 ST-LINK SWD，CH582M 使用兼容的 WCH-LinkE 两线下载。CN1 缺少 NRST/VTREF，先解决参考电压和供电。

每次烧录应记录设备标识、芯片实际型号、产物 SHA256、写入与验证结果，再复位运行。不要执行 Readout Unprotect、全片擦除或修改选项字节来盲目解决连接问题；应先明确这些操作对现有数据的影响。

安装 Android 包时先用 `adb devices -l` 核对唯一目标并授权 USB 调试，再用 `adb -s <serial> install -r <apk>` 安装。原型传输 v2 需要两个固件和 App 配套更新，避免在不同版本混用时调试数据损坏。
