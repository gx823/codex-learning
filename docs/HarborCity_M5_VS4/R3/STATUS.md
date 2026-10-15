# R3 镜头/输入修补 · 2026-10-15 · 未全通过，候选可试玩
EXE：E:/GameDev/Builds/HarborCity/M5_VS4/Candidate_20261015_132233_171_53c25695/Archive/Windows/HarborCity/Binaries/Win64/HarborCity-Win64-Shipping.exe（Shipping，53c25695）。
跟手PASS：改前授权旧代码测量副本f3c0526d最大误差High179.858°/Epic179.795°；改后两档0°，起转1帧。原版fc840e5a未改；1帧响应不代表毫秒延迟达标。
High帧时间FAIL（以下均广场/港口/公会，ms）：平均181.02/21.13/102.29→94.40/22.68/173.97；p99 643.65/127.56/387.11→607.13/224.73/792.20，仍有>50ms长帧。
High游戏线程：平均12.78/8.28/11.55→6.12/4.03/8.04；p99 32.69/13.21/24.78→16.59/9.34/42.44。
Epic帧时间：平均157.96/22.94/171.43→157.55/26.45/223.58；p99 910.04/134.45/679.08→890.68/179.73/972.73，不能声称流畅。
Epic游戏线程：平均12.24/9.23/13.87→8.58/5.38/7.67；p99 44.11/13.44/31.04→25.96/10.30/27.96。
测量：i7-14650HX/16GB/RTX4060Laptop8GB，UE5.8.2；独立包1080p100%、High60/Epic不限帧、VSync关，三处各20秒真实时钟；不录像/不深度读回。
C6未清零：两档原生镜头路线各70项PASS；渲染几何Cafe软窗帘各1帧距面约8cm FAIL，开放/WPO/Nanite覆盖BLOCKED；沿用软物只淡化规则。
两档主线四章/F5F9、双向地图往返、接送、星弓20m及A6均PASS；P菜单0.5–2倍/反转Y/写盘PASS，2560×1440/150%/1707×960焦点60/30/10/60 PASS。
本地对比片：D:/科研学习/codex学习/docs/HarborCity_M5_VS4/R3/recordings/HarborCity_R3_Camera_BeforeAfter_20261015_53c25695.mp4（30秒/30fps/无音轨，原版→新包）；视频不进git。
进度图：progress/camera_fix_20261015_menu.jpg；progress/camera_fix_20261015_compare.jpg。
地图/资产6482项哈希一致；用户原设置只读校验一致；桌面操作结束，UE/录屏已退出。
旧FAIL/FLAKY保留TEST_REPORT；详见appendix/camera_fix_20261015/CAMERA_FIX_EVIDENCE.md；长帧与C6剩余问题未解决，不预填整体PASS。
