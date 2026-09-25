# 港町性能抽测

1920×1080、100% 渲染比例、VSync 关闭、无帧率上限，无录像/截图。独立包，分段真实输入路线；不是全区连续路线验收。

|画质/区间|平均 FPS|p99 ms|
|---|---:|---:|
|High / South approach walk|92.02|12.29|
|High / Market approach walk|90.08|12.92|
|High / High aerial hover|97.66|11.81|
|High / Town aerial traversal|104.16|11.47|
|Epic / South approach walk|60.06|19.57|
|Epic / Market approach walk|61.48|19.41|
|Epic / High aerial hover|67.65|17.59|
|Epic / Town aerial traversal|70.02|17.29|

High 整卡显存采样峰值：3.94 GiB（含其他程序，5 秒采样）。

Epic 整卡显存采样峰值：4.81 GiB（含其他程序，5 秒采样）。

硬件：NVIDIA GeForce RTX 4060 Laptop GPU。高 GPU 利用率仅支持 GPU 负载较重的判断，不能代替 GPU/CPU 专项剖析。未关闭 Lumen 或删除 NPC/植被。样本满足阈值也不等于全港町性能放行，仍需覆盖夜间灯群、两处室内与所有区域的连续驾驶路线。
