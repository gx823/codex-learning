# 独立包性能

实际覆盖的步行、高空、六人施法与剑气、武器库片段达到本轮性能目标。1920×1080，RTX 4060 Laptop GPU，High／Epic，垂直同步和帧率上限关闭，渲染比例 100%；采样与录像分开，没有关闭 Lumen 或删除 NPC、植被。

| 画质 | 实际片段 | 平均 FPS | p99 ms |
|---|---|---:|---:|
| High | South approach walk | 118.58 | 10.16 |
| High | Market approach walk | 117.58 | 10.29 |
| High | High aerial hover | 117.99 | 10.28 |
| High | Town aerial traversal | 120.02 | 9.85 |
| High | Multiple spells and six NPC AI performance | 112.28 | 10.55 |
| High | Armory real preview render performance | 96.03 | 11.93 |
| Epic | South approach walk | 78.59 | 16.78 |
| Epic | Market approach walk | 76.92 | 16.85 |
| Epic | High aerial hover | 79.51 | 15.46 |
| Epic | Town aerial traversal | 78.36 | 15.56 |
| Epic | Multiple spells and six NPC AI performance | 76.37 | 16.13 |
| Epic | Armory real preview render performance | 69.56 | 17.36 |

High 显卡整卡显存采样峰值 5.13 GiB; Epic 显卡整卡显存采样峰值 6.03 GiB。显存约每三秒采样，不能当成捕获了所有瞬时峰值。

第一份 High 与音频处理重叠，保留为无效条件记录；仅重测 High。新购特效包与星弓未接入，因此没有证明该负载下的性能。详细条件和原始采样见 appendix/PERFORMANCE.json。
