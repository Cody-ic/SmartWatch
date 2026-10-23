# LSM6DSOX 腕戴活动决策树

`lsm6dsox_wrist_mlc.h` 保存 ST 官方 Activity recognition for wrist 的寄存器配置，启动时由 `LSM6DSOX_MLC_Init()` 经 I2C 写入 LSM6DSOX 片内配置存储器。特征计算和决策树运行在传感器的 MLC 中，STM32 只读取分类结果；它与 STM32 上的 HAR 神经网络是两个独立模型。

## 来源与许可

- [官方模型目录与说明](https://github.com/STMicroelectronics/st-mems-machine-learning-core/tree/de192e99620b7b818cd7cca0f840893098dc4a93/examples/activity_recognition_for_wrist/lsm6dsox)
- [原始 JSON 配置](https://github.com/STMicroelectronics/st-mems-machine-learning-core/blob/de192e99620b7b818cd7cca0f840893098dc4a93/examples/activity_recognition_for_wrist/lsm6dsox/lsm6dsox_activity_recognition_for_wrist.json)
- [LSM6DSOX 数据手册](https://www.st.com/resource/en/datasheet/lsm6dsox.pdf)
- [AN5259：MLC 配置、输入采样率与结果读取](https://www.st.com/resource/en/application_note/an5259-lsm6dsox-machine-learning-core-stmicroelectronics.pdf)

上游提交固定为 `de192e99620b7b818cd7cca0f840893098dc4a93`。原始配置的 **461 次写入**按顺序转换为简单的寄存器/数值数组，保留所有重复写入，没有增加模型抽象层。BSD 3-Clause 完整许可保存在同目录 [LICENSE](LICENSE)，发布二进制时也须保留许可材料。

## 模型与输出

模型仅使用加速度，量程 ±8 g。MLC 以 26 Hz 计算 52 样本窗口，单个窗口 2 秒；使用七种统计特征、一棵约 70 节点的决策树和元分类器。元分类器可能使首次有效结果和类别切换等待多个窗口。

| embedded bank 的 MLC0_SRC (0x70) | 固件枚举 | 含义 |
| --- | --- | --- |
| 1 | `MLC_OTHER` | 静止或其他非走跑动作，不能认定为静止 |
| 4 | `MLC_WALKING` | 步行/快走 |
| 8 | `MLC_RUNNING` | 慢跑/跑步 |
| 其他值、未加载、读取失败 | `MLC_UNKNOWN` (255) | 无有效分类 |

`Sensor_Task` 每 2 秒读取一次，保存在可用调试器观察的 `g_mlc_activity`。走跑分类会恢复屏幕亮度；空闲暗屏仍根据独立 wake-up 检测器的超阈值事件计时。通信失败或分类无效时重新计时并恢复亮度。wake-up 使用 250 mg 阈值、slope 输入和读取清除的锁存事件；它既不是 MLC，也不是抬腕姿态识别。

## 与原有采集并行

上游配置为加速度 26 Hz、陀螺仪关闭，并将 MLC 输出路由到 INT1。此处由加载函数做两项适配：

1. 在写入时抑制 user bank `MD1_CFG=0x02` 和 embedded bank `MLC_INT1=0x01` 的中断路由。MLC 和 wake-up 都采用轮询，INT1 → PB4 保留给 FIFO 水位。
2. 加载后恢复加速度 **104 Hz / ±8 g**、陀螺仪 **104 Hz / ±2000 dps**。MLC 仍为 26 Hz。FIFO 的 104 Hz 双传感器批处理、52 words 水位和 continuous 模式继续使用，不以分类结果动态降频。

AN5259 允许传感器 ODR 不低于 MLC 处理率；104 Hz 到 26 Hz 的内部抽取不会额外低通滤波，实际频响与官方 26 Hz 采集条件有差异。当前采用的是 **ST 参考树**，尚未用本手表采样重新训练，也未完成本机佩戴准确率验证。

加载仅在初始化阶段进行，期间停止 IMU、清空 FIFO，再恢复采集。PPG 在模型上传完成后才初始化，避免在上传期间积压。运行中的 bank 切换与其他 IMU/I2C1 访问都保留在同一 Sensor 任务；不要在其他任务或中断中并发读取该传感器。每次启动或重新初始化传感器后均重新加载树配置。

任一写入失败都会中止加载，尝试关闭页写和 MLC 并恢复 user bank，启动流程进入错误处理，不带着部分树开始采集。`PAGE_VALUE` 会自动增加目标地址，因此页数据写入发生错误时不会直接重试，防止“芯片已收到字节、主控却报告错误”造成后续模型字节错位。总线故障时恢复操作本身也可能失败，因此必须先排除通信故障，再重新执行完整初始化。

## 验证

运行方法见 [生产 C 回归](../../../tests/host/README.md)。主机测试通过 bank 和配置页模拟验证配置字节、顺序、分类映射、失败恢复以及 FIFO/采样寄存器保留；它不能证明传感器实际分类准确率。

真机测试时观察 `g_mlc_activity`，分别保持安静、做非走跑手部动作、步行和跑步，每种状态保持多个窗口；记录分类延迟和误分类。同步检查 FIFO 中断周期、每批 26 对 IMU/25 点 PPG 的完整性和长期溢出情况，并核对空闲暗屏及通信异常时的亮度行为。未完成这些测试前，不能把参考树的输出当作经过本项目标定的活动结论。
