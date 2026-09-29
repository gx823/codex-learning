# 旧 M4 测试入口

R5 最终 Shipping 的旧 M4 入口已记录 10 分钟超时。本轮静态核实：`HCM1TestRunner::BeginPlay` 的 Shipping 分支只接受 `HCM5VS3LocalQA()`，后者要求 `M5Test=m5_*` 及 M5 专用隔离存档。`M4Test=melee/r2_combat` 与 M4 前缀不满足守卫；NPC `ResetCombatForTest` 也被 `!UE_BUILD_SHIPPING` 排除。

因此本轮不重复已知不会启动的十分钟等待，也不在最终打包期间放宽 Shipping 测试安全守卫。两个原 M4 测试保持 NOT_RUN；R6 剑矩阵、跑跳重击专项及完整回归的空手检查分别实际执行，不冒称等价替代旧测试。此项是 A 测试覆盖缺口。
