# 第一人称全生命周期峰值

最终 Shipping，1920×1080，20 项全部重测：**19 PASS / 1 FAIL**。对照噪声均低于 0.5%。

方法沿用 R5：真实场景成对渲染，RGB 最大通道差值至少 32；隐藏特效后再拍一次对照；受控 30 Hz 仿真覆盖完整生命周期。R6 每次成对捕获重置渲染历史，避免把 Lumen 历史噪声当作特效。此诊断不是主视口原生 30 FPS 录像。

|效果|峰值 %|对照噪声 %|帧数|结果|
|---|---:|---:|---:|---|
|Arcane_muzzle|7.443|0.154|93|PASS|
|Arrow_charge|28.602|0.144|33|FAIL|
|Arrow_release|14.813|0.170|150|PASS|
|Bolt_charge|5.172|0.144|36|PASS|
|Bolt_release|4.929|0.158|150|PASS|
|CrimsonNight_charge|11.326|0.171|33|PASS|
|CrimsonNight_slash1|9.758|0.184|29|PASS|
|CrimsonNight_slash2|24.690|0.249|29|PASS|
|CrimsonNight_slash3|8.135|0.203|29|PASS|
|CrimsonNight_wave|20.638|0.152|120|PASS|
|Heal|11.917|0.158|242|PASS|
|Shield|4.111|0.161|242|PASS|
|Shock_release|13.979|0.119|150|PASS|
|Shock_warning|3.797|0.107|36|PASS|
|StarTide_charge|11.880|0.249|33|PASS|
|StarTide_slash1|12.373|0.380|29|PASS|
|StarTide_slash2|23.190|0.398|29|PASS|
|StarTide_slash3|10.590|0.428|29|PASS|
|StarTide_wave|20.140|0.342|120|PASS|
|Steamlock_muzzle|6.813|0.114|93|PASS|

光箭蓄力从太小调整成偏大（28.6%），没有达到 3%--25%。两把剑的剑气约 20%，绯夜第二斩 24.69%，魔力弹蓄力/发射和冲击预告均达到各自阈值。所有原始失败保留，不以效果平均值掩盖峰值。

完整数值、运行路径和动作前提见 `appendix/FP_FINAL_PEAKS.json`；诊断 MP4 与清理记录在 `review/final/R6_FP_EFFECT_LIFETIMES.json`。画面观感 USER_REVIEW。
