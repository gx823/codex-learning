# 独立包性能

1920×1080，High/Epic 各一次；RTX 4060 Laptop GPU、i7-14650HX。帧率上限与VSync关闭，渲染比例100%。与录像、Blender及音视频处理分开；未关闭Lumen，未删除NPC或植被。负载采用6名明显成年NPC、连续魔法、剑气、普通/蓄力光箭与冲击；星弓已按E.7降级。

| 画质 | 片段 | FPS | p99 ms |
|---|---|---:|---:|
| High | South approach walk | 118.01 | 9.87 |
| High | Market approach walk | 116.93 | 10.02 |
| High | High aerial hover | 117.35 | 9.87 |
| High | Town aerial traversal | 119.62 | 9.99 |
| High | Armory real preview render performance | 91.41 | 12.78 |
| High | 6 NPC + spells/wave + fallback arrows + AoE | 107.57 | 10.93 |
| Epic | South approach walk | 73.86 | 16.25 |
| Epic | Market approach walk | 76.05 | 16.05 |
| Epic | High aerial hover | 80.30 | 14.24 |
| Epic | Town aerial traversal | 81.44 | 14.90 |
| Epic | Armory real preview render performance | 64.58 | 18.79 |
| Epic | 6 NPC + spells/wave + fallback arrows + AoE | 72.44 | 16.22 |

High 整卡显存采样峰值 3.89 GiB。

Epic 整卡显存采样峰值 4.76 GiB。

采样并非所有瞬时显存峰值。总体目标与逐片段门槛以附录原始值判断；场景摆位为A夹具，施法与射击为B Action，NPC反应维持原逻辑。整轮功能测试和录像帧率不替代该性能采样。

High负载检查保留FAIL：末尾受伤NPC计数为1，未满足至少2人的条件。源码在后续光箭步骤逐人重置生命，最终读数也不能可靠追溯先前AoE是否命中多人。六NPC数量与帧率样本存在，但不能据此声称完整规定负载验收通过；没有修改断言或重跑本轮性能。
