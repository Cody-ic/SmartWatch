# CH582M 蓝牙协处理器

负责 BLE 外设服务与 UART 双向桥接。CH582M 不解析 `0xAA` 应用帧，STM32 和 App 应能处理拆包、连续帧与不完整数据。

## 源码与接口

| 位置 | 职责 |
| --- | --- |
| `src/Main.c` | 初始化与主循环 |
| `src/peripheral.c` | BLE 外设、连接与数据调度 |
| `src/ble_uart_service/` | GATT 服务与收发回调 |
| `src/app_drv_fifo/` | 环形缓冲、整块写入 |
| `HAL/`、`StdPeriphDriver/` | 芯片外设接口 |
| `LIB/`、`Ld/`、`Startup/` | BLE 库、链接脚本与启动代码 |

主循环为 `TMOS_SystemProcess()` 加 `app_uart_process()`。UART1 使用 115200、8N1，PA8 为 RX、PA9 为 TX。与主控的接线见 [硬件 README](../../hardware/README.md)。

| GATT 项目 | 配置 |
| --- | --- |
| 广播名 | SmartWatch，名称在 Scan Response |
| 服务 | FFE0 |
| 手机写入 | FFE2，Write / Write Without Response |
| 手机接收 | FFE3，Notify |

完整 UUID、应用帧及原型传输见 [协议规范](../stm32/PROTOCOL.md)。

## 构建

使用 MounRiver Studio 2 导入本目录，首次 Build 生成 `obj/`。将配套 RISC-V Embedded GCC 和 `others/Build_Tools/Make/bin/` 加入 PATH，在本目录执行：

```sh
make -C obj -j8 all
# 强制重新编译：
make -C obj -B -j8 main-build
```

配置为 RV32IMAC / ILP32、GNU99，产物为 `obj/CH582M.elf` 和 HEX。最近构建 FLASH 143864 字节，RAM 22704 字节。`.cproject` 是配置来源，`obj/` 是生成目录。

## 缓冲与维护约束

UART→BLE RX FIFO 为 4096 字节，BLE→UART TX FIFO 为 512 字节，空间不足时整块拒绝写入，避免半包进入队列。`BLE_BUFF_MAX_LEN=251`，App 按实际 MTU 串行分块写入。

生产配置禁用 `DEBUG`。UART1 是协议链路，调试文本会污染数据；冲突配置会触发编译错误。协议变更需同步主控和 App。

## 测试与排障

FIFO 回归位于 [主机测试目录](../stm32/tests/host/README.md)。无线空口、低 MTU、连续备份和多次重连仍需真机验证。

若 make 的递归 `all` 失败，检查是否使用 MounRiver 配套 `Build_Tools/Make/bin/make.exe`。下载使用兼容的 WCH-LinkE 两线接口；`WLink Open Error` 先排查下载器识别与连接，再检查目标访问。连接后无数据时核对 FFE2/FFE3、Notify 订阅、串口交叉连接和队列拒绝情况。
