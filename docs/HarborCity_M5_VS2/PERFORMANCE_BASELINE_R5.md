# R5 50m 样板性能基线

两次实际采样及进程退出均通过，**性能目标验收仍为 USER_REVIEW**。对象为当前 R5 50m 午后街角，含 Q/R/J/T/U/V/W/X 八名独立 NPC、保留安全衣层的主角和一辆车；来自已核验 Live R5 私有地图。运行层为 UnrealEditor.exe `-game`，不是 Windows 独立包，也不是 250m/全城。

| 条件/结果 | High | Epic |
|---|---:|---:|
| 预热（秒） | 30.007 | 30.003 |
| 连续测量（秒） | 65.007 | 65.011 |
| 帧样本 | 6846 | 4626 |
| 平均 FPS | 105.31 | 71.16 |
| p99 帧时间（ms） | 12.057 | 19.112 |
| 最长单帧（ms） | 47.347 | 49.364 |
| 进程 LOCAL 显存采样最大值（GiB） | 2.981 | 3.911 |
| 显存样本（约1Hz） | 65 | 65 |

硬件实际读回：Intel(R) Core(TM) i7-14650HX（24 逻辑线程）；NVIDIA GeForce RTX 4060 Laptop GPU，D3D12，驱动 592.82；物理 RAM 15.71 GiB。

画质实际读回 High 的十项 sg 质量均为 2、Epic 均为 3；1920×1080、100% 渲染比例、VSync=0、t.MaxFPS=0，动态分辨率/固定帧率/平滑帧率均关闭，所查帧生成/超分插件 CVar 未注册。两次预热与测量分离；正式窗口没有暂停重试，测量期 ShaderJobs 与资源 StreamingWanted 最大值均为 0。没有请求截图或录屏，原生记录器未检测到首帧，启动器也没有发现新增 capture.json。

统计复算与原始数据一致：平均 FPS=N/累计帧间隔；p99 为升序第 ceil(0.99N) 项，不滤除长帧。采样取真实视口 OnEndDraw 的相邻游戏帧时间，保留正常世界/HUD运行及小量采样开销，不是 GPU 时间戳或显示器呈现延迟。相机沿已建道路运行，未模拟玩家输入。

显存使用 DXGI QueryVideoMemoryInfo、实际 RHI 适配器 LUID、当前进程 LOCAL 用量的约 1Hz 最大采样，**不是瞬时峰值，也不是整卡总占用**。更长运行、飞行高处、黄昏/夜景、战斗群体、后续材质候选与最终独立包仍需各自测量，不能由此推定。

两次使用相同计划与历史模块 SHA `47F6354D1917E95DAE37409A50F0FBD75D83D2E317B108CABB44F04CA5A6979D`；后续编译不改写这份基线。

[High 原始结果](D:/科研学习/codex学习/docs/HarborCity_M5_VS2/editor_runtime/20260925_104419_902_776b5c16_corner_performance_r5_High_Afternoon/CornerPerformance_20260925_104439_7788541E/corner_performance.json) · [启动/退出记录](D:/科研学习/codex学习/docs/HarborCity_M5_VS2/editor_runtime/20260925_104419_902_776b5c16_corner_performance_r5_High_Afternoon/launch.json)
[Epic 原始结果](D:/科研学习/codex学习/docs/HarborCity_M5_VS2/editor_runtime/20260925_105104_895_e3bab909_corner_performance_r5_Epic_Afternoon/CornerPerformance_20260925_105126_6A949EEE/corner_performance.json) · [启动/退出记录](D:/科研学习/codex学习/docs/HarborCity_M5_VS2/editor_runtime/20260925_105104_895_e3bab909_corner_performance_r5_Epic_Afternoon/launch.json)

[精简复算数据与完整来源 SHA](D:/科研学习/codex学习/docs/HarborCity_M5_VS2/PERFORMANCE_BASELINE_R5.json)
