# 支线配套实验

把示例复制到独立练习目录运行。输出目录用于保存本次实验结果，避免混入手表工程。

## Dart 与页面

- `dart run dart_basics.dart`：语法、对象与回调。
- `dart run dart_async.dart`：一次结果与连续事件。
- `dart --enable-asserts run battery_frame.dart`：合法电量帧与坏校验。
- `../flutter_watch/`：完整离线页面及界面测试，按该目录 README 运行。

## Python、训练与量化

先在虚拟环境中按 `requirements.txt` 准备依赖。

```sh
python python_basics.py
python train_activity.py --demo --out practice_mlp --epochs 8 --export
python quantize_activity.py practice_mlp
python train_activity.py --demo --out practice_cnn --network cnn --epochs 8 --export
python prototype_demo.py
```

`--demo` 使用脚本生成的人工信号，仅用于检查训练、导出、量化和评估程序能够运行，不能代表真实动作识别效果。真实记录改用 `--csv path/to/recordings.csv`，列为 `person,session,split,time_ms,label,ax,ay,az,gx,gy,gz`，每个人仅属于一个 split（train/valid/test），输入须预先统一物理单位并采样为 100 Hz。完整窗口 512 点，步长 256，跨标签和缺失窗口跳过。标签从零连续编号，类别顺序须另行记录。

教学模型是新训练的小模型，不用于直接覆盖仓库原有网络。元数据、归一化参数与评估结果均随输出保存。QDQ 示例保留浮点输入边界，不能因内部使用八位整数就按 uint8 字节直接喂入。

## 普通文件升级模拟

`ota_files.py` 只操作明确指定的普通目录，不访问串口、设备或 Flash。每条命令都是一个单独进程，状态通过 `state.json` 保存。

```sh
python -c "from pathlib import Path; Path('new.bin').write_bytes(b'new-demo-app')"
python ota_files.py --root update_practice init
python ota_files.py --root update_practice begin new.bin
python ota_files.py --root update_practice chunk new.bin --offset 0 --length 4
python ota_files.py --root update_practice status
python ota_files.py --root update_practice chunk new.bin --offset 4 --length 8
python ota_files.py --root update_practice finish
python ota_files.py --root update_practice boot
python ota_files.py --root update_practice confirm
```

新文件为 12 字节。试运行后直接再次 `boot`，将回退到旧文件；先 `confirm` 再 `boot`，则保留新文件。使用新的练习目录重新开始，不要删除现有工程。模拟支持同内容重传、完整性检查和未确认回退，不实现真实启动跳转、Flash 掉电行为或固件签名验证。
