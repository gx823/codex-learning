# 1080p 性能

High/Epic 测量绑定到录制政策更新前的 Shipping 候选；之后只有录制器源码改变，游戏资产清单一致。最终 EXE 未冒称重新测过性能。未录屏、未编码，关闭帧率上限，100% 渲染比例，保留 Lumen、NPC 和植被。

| 画质 / 片段 | 平均 FPS | p99 ms |
|---|---:|---:|
| HIGH / South approach walk | 101.0 | 11.68 |
| HIGH / Market approach walk | 101.7 | 11.55 |
| HIGH / High aerial hover | 100.8 | 11.36 |
| HIGH / Town aerial traversal | 102.6 | 11.19 |
| HIGH / Multiple spells and six NPC AI performance | 99.0 | 11.71 |
| EPIC / South approach walk | 64.5 | 17.98 |
| EPIC / Market approach walk | 66.5 | 17.73 |
| EPIC / High aerial hover | 70.3 | 16.32 |
| EPIC / Town aerial traversal | 71.1 | 16.44 |
| EPIC / Multiple spells and six NPC AI performance | 66.2 | 17.31 |

所采片段满足本次帧率目标，非全地图逐区保证。显存为整张显卡每 3 秒采样，High 峰值约 4.09 GiB、Epic 约 4.93 GiB。硬件与内存、特效参数见 appendix/PERFORMANCE.json。全局 Niagara 粒子硬上限与半透明重叠层数还未完成计量。
