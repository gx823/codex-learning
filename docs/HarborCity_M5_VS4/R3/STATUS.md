# R3 修补轮 · 2镜头距离完成，进入3城堡照明 · 2026-10-16
修补预览EXE：E:/GameDev/Builds/HarborCity/M5_VS4/Candidate_20261016_130303_836_polishshipping/Archive/Windows/HarborCity/Binaries/Win64/HarborCity-Win64-Shipping.exe
第1项仍未完成：Epic偶发启动GPU崩溃；ddagrab20秒/30帧近似重复13.38%（要求≤5%）；8前回修。
High/60 四处平均16.69/16.67/16.67/16.67ms；p99 19.63/19.49/19.59/19.37ms；>50ms均0，快甩无>50ms。
Epic有效样本平均17.64/16.17/16.39/18.20ms；p99 22.44/19.91/20.83/22.82ms；启动不稳定仍FAIL。
性能数字来自Candidate_20261016_115554_067；最终包将重测，不能据旧包数字宣称新包通过。
第2项：室外默认3m、室内约2.2m；P菜单镜头距离2–5m，与灵敏度/反转Y一起保存并重启读回通过。
广场/小巷/室内同机位对比通过；FOV未改，星弓肩位参数未改；人物明显放大，室内画面下缘裁到小腿。
镜头输入/穿墙路线、星弓20m/A6、主线四章/F5F9、地图双向往返实际PASS；退出窗口守卫异常旧记录保留。
第2项未改地图；上一地图批具名NPC/任务点实际存读档45/45通过。
图：progress/2026-10-16/polish02_camera_distance.jpg；第1项待修图polish01_turning_video_pending.jpg。
第1项已定位局部灯VSM及显存压力；128私有Nanite副本/57,840实例与36处投影调整，Lumen/VSM/100%比例保留。
接着3城堡灯、4穿模、5魔导车、6道路/探索、7空屋；8前回修Epic/录像，再做最终包与完整回归。
恢复见WORK_CHECKPOINT.json；证据STEP1_RECEIPT/STEP2_RECEIPT；只发布JPG与STATUS，私有资产和用户设置保留。
