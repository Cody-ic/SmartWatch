# SmartWatch HTML 教程

当前快照为 **v0.1.0 预览版**（2026-10-25）。[从 GitHub Release 下载离线包](https://github.com/Cody-ic/SmartWatch/releases/tag/tutorial-v0.1.0)，完整解压后打开 `tutorial/index.html`。本版本范围和验证记录见 [发布说明](RELEASE_NOTES.md)。

默认掌握 C，按“知识 → 独立实验 → 项目实践 → 验收”学习。入口为 [index.html](index.html)，主线分为第一章“裸机开发”和第二章“FreeRTOS”，共 29 篇（含配套与拓展）；支线一拆成 7 篇，其他支线共 4 篇实践，全站共 40 篇课程。

可双击入口离线阅读；推荐从仓库根目录启动本地静态服务：

```sh
python -m http.server 8000
```

访问 `http://localhost:8000/tutorial/`。普通 GitHub 文件页面显示 HTML 源码，不会运行交互；需要本地打开或使用静态站点托管。

`main/` 保存基础、裸机、无线和 FreeRTOS 章节；`branches/` 保存 LVGL、App、ST.AI、proto 与 OTA；`assets/` 保存本地样式、交互脚本和原创 SVG。页面无需构建或下载 CDN，未运行脚本时正文仍可阅读。

图示和动画均为教学示意。代码块区分 HAL 片段、纯算法、命令参考和伪代码，需按各章条件使用。AI 参数需要真机数据，后训练约定采用 proto；完整固件 OTA 当前仅为设计教学。教程不会执行 Generate Code、烧录或擦除。

引用见每章末及 [资料索引](references.html)。学习进度仅保存在当前浏览器，可在章节末标记或撤销；浏览器拒绝本地存储时不影响阅读。

## 主线学习顺序

第一章依次学习 GPIO、串口、中断、SPI、IIC、ADC、无线通信和 DMA；时钟、TIM 与看门狗等放在嵌入式拓展，最后用裸机逻辑综合收束。无线通信下拉包含基础、手写协议和项目实现。

## 第二章：FreeRTOS

前置从 [普通函数、函数指针与多任务](main/freertos/00-foundations.html) 开始，再依次学习 [创建任务](main/14-freertos.html)、[调度与等待](main/freertos/02-scheduling.html)、[中断与信号量](main/freertos/03-semaphore.html)、[队列](main/15-sync.html)、[共享资源](main/freertos/05-resources.html)、[声音帧的多个使用者](main/freertos/06-audio.html)、[栈与内存](main/freertos/07-memory.html)，最后 [接回当前手表](main/freertos/08-project.html)。九篇均为目录中的同级入口。

示例按所在位置逐步加入；函数指针例子可在电脑运行，任务实验采用独立 STM32 工程。项目对照以 main.c 当前实际创建的三个任务为准。

## 支线一：显示驱动与 LVGL

完成 SPI 后可按需进入 [七篇学习路线](branches/lvgl.html)：GC9A01 屏幕驱动、LVGL 显示移植、控件、布局与样式、事件与页面、数据与生命周期、刷新优化与排错。主线由 SPI 直接进入 I²C。

## IIC 下拉实战

点击目录中的“I²C / 总线与传感器驱动”展开基础篇与四篇实战：[LSM6DSOX](main/i2c/01-lsm6dsox.html)、[MAX30102](main/i2c/02-max30102.html)、[联合采集与重采样](main/i2c/03-sampling.html)、[MLC 与 AI 训练数据](main/i2c/04-mlc.html)。第三篇推导 104→100 Hz 的线性插值；第四篇说明辅助标注、分歧复核及当前固件边界。
