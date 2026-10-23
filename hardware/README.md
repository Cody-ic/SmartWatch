# 硬件资料发布状态

已检查用户提供的 2026-07-09 网表、Gerber 与嘉立创工程，文件指纹见 [hardware-manifest.json](../verification/hardware-manifest.json)，详细结论见 [PCB 核验报告](../docs/pcb-review.md)。

当前发现 XC6194 TEST 脚未接地、STM6522 复位输入/输出连接不符合用途、电芯与充电参数未匹配等问题。因此当前原始设计文件没有作为核验通过的制板包上传。

本目录不是生产交付包。待完成原生 EDA 整改、ERC/DRC、铺铜和重新导出后，可增加明确版本的源工程、网表、Gerber、BOM 与更新后的核验结论。
