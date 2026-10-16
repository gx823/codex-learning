# R3 修补轮 · 1仍有Epic启动/录像FAIL，按4小时回退进入2 · 2026-10-16
修补预览EXE：E:/GameDev/Builds/HarborCity/M5_VS4/Candidate_20261016_122639_225_polishshipping/Archive/Windows/HarborCity/Binaries/Win64/HarborCity-Win64-Shipping.exe
第1项未完成：最新包Epic启动再次GPU崩溃；ddagrab20秒/30帧近似重复13.38%（≤5%）；8前必须修。
High/60 四处平均 16.69/16.67/16.67/16.67 ms；p99 19.63/19.49/19.59/19.37 ms；>50ms均0。
Epic 四处平均 17.64/16.17/16.39/18.20 ms；p99 22.44/19.91/20.83/22.82 ms；>50ms均0。
以上为有效样本数字：快甩180°四处均无>50ms；Epic虽有两次PASS，最新编译包启动仍FAIL，未验收。
广场基线64.36ms/p99746.57ms；五版二分首次退化d672f37c；只关局部投影即可恢复约16.67ms。
128个私有Nanite副本覆盖57,840实例，36处局部投影调整；Lumen/VSM/100%渲染比例保留。
DXGI基线LOCAL峰值8.117GiB/预算7.020GiB；修补后High约4GiB；共享段有约0.26GiB占用，不据此冒称分页已实测。
具名NPC/任务实际存读档45/45；主线四章/F5F9、地图往返、星弓、接送、镜头输入均PASS。
Epic启动崩溃及录像FAIL全部保留；硬件光栅/启动配置尚未形成稳定修复；Insights按用户选择NOT_RUN。
图：progress/2026-10-16/polish01_turning_video_pending.jpg；录像与完整数字留本地appendix/polish_20261015。
接着做2镜头距离，再3城堡灯、4穿模、5魔导车、6道路/探索、7空屋；8最终包与性能/录像复验。
恢复：WORK_CHECKPOINT.json；私有saved_plan及保存前备份保留；素材原件与用户设置未改。
