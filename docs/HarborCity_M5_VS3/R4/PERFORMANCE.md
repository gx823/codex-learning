# 独立包性能

1920×1080；以下取各画质最近一轮实测，与录像分开。先前未接电的 High/Epic 失败保留在 FINAL_TESTS 与 POWER_INTERRUPTION，不能当作同条件性能对照。实际设置和硬件见附录。

|画质|片段|平均 FPS|p99 ms|
|---|---|---:|---:|
|High|South approach walk|45.22|25.13|
|High|Market approach walk|44.12|25.87|
|High|High aerial hover|45.83|24.11|
|High|Town aerial traversal|44.72|24.38|
|High|Armory real preview render performance|32.43|35.22|
|High|6 NPC + magic/sword wave/light arrow/shock|35.25|32.82|

High 整卡显存采样峰值 2.90 GiB。

|画质|片段|平均 FPS|p99 ms|
|---|---|---:|---:|
|Epic|South approach walk|23.67|45.13|
|Epic|Market approach walk|24.54|44.42|
|Epic|High aerial hover|26.75|39.20|
|Epic|Town aerial traversal|26.44|40.81|
|Epic|Armory real preview render performance|22.25|48.59|
|Epic|6 NPC + magic/sword wave/light arrow/shock|22.56|49.51|

Epic 整卡显存采样峰值 3.68 GiB。
