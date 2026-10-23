# 核验摘要与文件追踪

- [software-baseline.json](software-baseline.json)：实际构建、测试数量与验证阶段。
- [artifact-hashes.json](artifact-hashes.json)：已构建软件产物的 SHA256；仅记录指纹，产物由本地源码工程生成。
- [hardware-manifest.json](hardware-manifest.json)：三份已审阅硬件输入的大小、SHA256 和当前发布状态。
- [c-regressions.txt](c-regressions.txt)：实际生产 C 主机测试的 PASS 摘要。
- [flutter-analyze.txt](flutter-analyze.txt)：静态检查结果摘要。
- [flutter-tests.txt](flutter-tests.txt)：18 项测试完成摘要。

这些摘要来自 2026-10-23 已完成的软件回归。未包含设备烧录、实物电气测试或实际短信发送结果。测试覆盖及验收步骤见 [testing.md](../docs/testing.md)。
