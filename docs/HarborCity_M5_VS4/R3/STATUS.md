# HarborCity M5-VS4 R3 · 清穿模第 7 版 · 限时交付

更新：2026-10-27T11:01:14.934801+08:00。**FAIL：必做镜头与导航未全部达标，新候选含花叶材质回归，不能替代已实玩认可的 f30d03e5。**

EXE：`E:\GameDev\Builds\HarborCity\M5_VS4\Candidate_20261027_010941_000_e01592e2\Archive\Windows\HarborCity\Binaries\Win64\HarborCity-Win64-Shipping.exe`

SHA-256：`0769e6ded161bc20282e06ff6ed9e61b9c72a77119fe36eef66394ae5ce0f319`

镜头审计：275,646 帧；胶囊内 **0**；无俯仰输入自动俯角超过 60° **0**；非植物不透明遮挡 **0**；隐藏时自动俯仰偏差超过 10° **0**。

人物隐藏 **16,531 帧**，累计 **201.95 秒**，最长连续 **20.29 秒**。镜头原生审计状态：PASS；人工画面复核另见交付记录。

还剩：玫瑰与部分树叶出现不透明方片/硬边（新增回归）；公会门洞 225° 的一只鞋越过画面下缘；旧近墙测试的树顶落脚条件失败。六处完整任务路线 3/6 通过，B155 入口及两处室内未接通。31 组导航候选 8/31 全采样接通，其余保留具体阻挡。摆放实际修改 15/40；原 154 件余 154、栏杆余 83、大件余 658（集合有重合，不能相加）。B027 部分视角曝光偏亮，夜间灯距仍有超过 12 m 的区间。

最终 Shipping 原 15 项：**14/15 PASS**。扩大碰撞：**92/92**；SACK74 的实际接触类型见碰撞记录，踩过低袋子不写成横向挡停。


本轮改动：保持玩家输入俯仰，近头部切换第一人称并隐藏完整人物；过渡约 0.25 秒。1 m 内近镜头花草处理已实现隐藏/恢复，但材质透明轮廓出现回归，本项 FAIL。B017/B010/B027 地面和入口修复；B155 的失败尝试保留原生证据。恢复 B027 两盏暖白主光，降低前侧白布补光；北桥灯改为夜间低亮度。新增 80 处有可见灯具且经原生位置检查的夜灯。15 件摆放保持原尺寸。

本轮只撤回了本轮新加、后来发现阻挡室内路线的 B155 装饰门框；未删除旧物件，未缩小物件，未合并实体对象。已有 3 对逻辑别名不是实体重复，未据此删物件。


| 任务点 | 完整实走与 HUD | HUD |
|---|---|---|
| v6_task_V2_B017 | PASS | 路线 165 m；直线 31 m |
| v6_task_V2_B010 | PASS | 路线 132 m；直线 93 m |
| v6_task_V2_B155 | FAIL | 路线 204 m；直线 160 m |
| v5_room_V2_B027_2 | PASS | 路线 127 m；直线 107 m |
| v5_room_V2_B155_1 | FAIL | 路线不可用；直线 167 m |
| v5_room_V2_B155_2 | FAIL | 路线不可用；直线 159 m |

B155：撤回新增装饰门框后，室内走线恢复，但室外台阶到房间的全局路线仍未接通。沿入口与门洞中线的连接都未通过连续实体地面/台阶检查，因此没有强行添加链接；记录为 BLOCKED，不用“路线可用”掩盖实走失败。


| 原 15 项 | 结果 |
|---|---|
| story | PASS |
| travel | PASS |
| oldtravel | PASS |
| ride | PASS |
| named | PASS |
| bow | PASS |
| a6 | PASS |
| follow | PASS |
| alleys | PASS |
| distance | PASS |
| high | PASS |
| safety | PASS |
| close | FAIL_OR_INCOMPLETE |
| physics | PASS |
| medium | PASS |

近墙测试的 8 条失败均关联原 MarketOriginalTree 的树冠落脚射线未命中及随后的站立测试无效。本轮树网格处理关闭了叶片分段碰撞，不只是忽略镜头通道，因此不能把这项写成测试误报。树顶巡查拼图使用飞行驻留，不能替代站立验收。11 处拼图 88 张人工复核中，87 张满足人物完整或正常前视，公会门洞 225° 仍有一只鞋越框。原生四项零违规不等同于所有画面通过。


花坛已改用出生广场真实玫瑰长椅点补拍，另检查了 4 个花坛共 32 张画面。近处隐藏/离开恢复可见，但玫瑰花片和部分树叶有明显方片边缘。源脚本可能在重新连接透明遮罩时丢失原输出通道；这是代码与画面的推断，未做最终材质图引脚读回。修复透明遮罩与树冠站立碰撞后仍需重新打包及回归，当前包未修复这两项。


性能条件：Intel(R) Core(TM) i7-14650HX；NVIDIA GeForce RTX 4060 Laptop GPU；1920×1080、100% 屏幕比例、VSync 关闭。High 60 帧上限，预热后每处连续转动约 20 秒，录像单独执行。


| High 位置 | 平均 FPS | P99 帧时 ms | >50 ms 帧 |
|---|---:|---:|---:|
| 广场 | 60.00 | 19.43 | 0 |
| 港口 | 60.00 | 19.35 | 0 |
| 公会室内 | 60.00 | 19.25 | 0 |
| 王座厅 | 60.00 | 19.29 | 0 |

Medium 30 帧上限原三处观察测试：PASS。稳态观察步骤约 30 FPS；PNG 截图步骤单独出现约 0.37–0.39 秒写图开销，未把它们隐去或算成正常游玩帧时。完整逐步骤数据见 PERFORMANCE_FINAL.json。


| 31 组导航 | 结果 | 明确挡点 / 原因 |
|---|---|---|
| 5 | BLOCKED · 0/3 | 实体走廊检查被以下物件阻挡：HC_VS4_R2_V2_ISM_0351、HC_R3_Clipping3_Barrier_bank_east_169、HC_R3_Clipping3_Barrier_bank_east_171、HC_R3_Clipping3_Barrier_bank_east_173；首个未通点 cm=[14272.4, 4971.8, 289] |
| 6 | BLOCKED · 0/3 | 实体走廊检查被以下物件阻挡：HC_VS4_R2_V2_ISM_0350、HC_VS4_R2_V2_ISM_0351、HC_R3_Clipping3_Barrier_bank_east_173；首个未通点 cm=[14369.6, 5815.1, 289] |
| 7 | BLOCKED · 0/3 | 实体走廊检查被以下物件阻挡：HC_R3_Clipping3_Barrier_bank_east_169、HC_R3_Clipping3_Barrier_bank_east_171、HC_R3_Clipping3_Barrier_bank_east_173；首个未通点 cm=[14327.6, 5462.4, 289] |
| 8 | BLOCKED · 0/1 | 实体走廊检查被以下物件阻挡：HC_VS4_R2_V2_ISM_0350、HC_VS4_R2_V2_ISM_0351、HC_R3_Clipping3_Barrier_bank_east_173；首个未通点 cm=[14349.7, 5925.1, 289] |
| 11 | BLOCKED · 0/2 | 实体走廊检查被以下物件阻挡：HC_VS4_R2_V2_ISM_0351、HC_R3_Clipping3_Barrier_bank_east_185；首个未通点 cm=[14365.5, 6807.0, 289] |
| 13 | PASS · 1/1 | 已接通 |
| 14 | BLOCKED · 0/1 | 实体走廊检查被以下物件阻挡：HC_VS4_R2_V2_ISM_0234、HC_VS4_R2_V2_ISM_0235；首个未通点 cm=[15618.3, 9564.4, 241.4] |
| 31 | PASS · 1/1 | 已接通 |
| 32 | BLOCKED · 0/3 | 高架桥面（Z 10.1 m）已有可站立 NavMesh，但与主线路网断开；本轮搜索没有找到可通过实体地面检查的短连接候选。；首个未通点 cm=[13871.4, 2396.9, 1010] |
| 33 | PASS · 2/2 | 已接通 |
| 34 | BLOCKED · 0/3 | 实体走廊检查被以下物件阻挡：HC_VS4_R2_V2_ISM_0350、HC_R3_Clipping3_Barrier_bank_west_166、HC_R3_Clipping3_Barrier_bank_west_168；首个未通点 cm=[12352.0, 4746.3, 289] |
| 38 | BLOCKED · 0/3 | 实体走廊检查被以下物件阻挡：HC_VS4_R2_V2_ISM_0350、HC_R3_Clipping3_Barrier_bank_west_184、HC_R3_Clipping3_Barrier_bank_west_186；首个未通点 cm=[12437.7, 6995.8, 289] |
| 88 | PASS · 1/1 | 已接通 |
| 90 | BLOCKED · 0/2 | 实体走廊检查被以下物件阻挡：HC_VS4_R2_V2_ISM_0351、HC_R3_Clipping3_Barrier_bank_west_084、HC_R3_Clipping3_Barrier_bank_west_088；首个未通点 cm=[8492.1, -7851.0, 289] |
| 111 | BLOCKED · 0/1 | 实体走廊检查被以下物件阻挡：R3_BRIDGE_WEST_VEHICLE_1_1_1_1、R3_BRIDGE_WEST_VEHICLE_SIDE_0_2_0_0、R3_BRIDGE_WEST_VEHICLE_SIDE_0_2_1_0、R3_BRIDGE_WEST_VEHICLE_SIDE_2_1_1_0；首个未通点 cm=[10126.6, 2430.6, 407.2] |
| 115 | BLOCKED · 0/2 | 实体走廊检查被以下物件阻挡：HC_R3_Next4_V2_B146、HC_R3_Next4_V2_B146、HC_R3_Next4_V2_B146、HC_R3_Next4_V2_B146；首个未通点 cm=[6234.1, 13214.0, 369.5] |
| 147 | PASS · 2/2 | 已接通 |
| 159 | PASS · 1/1 | 已接通 |
| 181 | PASS · 1/1 | 已接通 |
| 182 | PASS · 1/1 | 已接通 |
| 276 | BLOCKED · 0/2 | 城堡岩壁顶部两个孤立采样点（Z 31.0–33.3 m）；没有符合范围的短连接候选。；首个未通点 cm=[-9931.5, -10044.6, 3102.3] |
| 299 | BLOCKED · 0/2 | 城堡塔顶平台（Z 52.2 m）与主线路网断开；没有符合范围的短连接候选。；首个未通点 cm=[-6580.2, -16470.7, 5220.0] |
| 304 | BLOCKED · 0/1 | 实体走廊检查被以下物件阻挡：SM_PROP_painting_castle_3、SM_PROP_painting_castle_2、SM_PROP_curtain_castle_8；首个未通点 cm=[402.2, -16764.3, 3050.3] |
| 320 | BLOCKED · 0/1 | 城堡岩壁斜顶（Z 17.7 m，原生检测标为 roof）；没有符合范围的短连接候选。；首个未通点 cm=[4172.7, -15655.4, 1768.8] |
| 326 | BLOCKED · 0/1 | 实体走廊检查被以下物件阻挡：SM_ENV_cliff_castle_15；首个未通点 cm=[5327.4, -15094.1, 1291.0] |
| 331 | BLOCKED · 0/1 | 实体走廊检查被以下物件阻挡：Landscape_0.LandscapeHeightfieldCollisionComponent_13；首个未通点 cm=[7020.1, -13533.0, 940.3] |
| 336 | BLOCKED · 0/1 | 实体走廊检查被以下物件阻挡：Landscape_0.LandscapeHeightfieldCollisionComponent_13；首个未通点 cm=[7775.9, -13733.4, 871.8] |
| 345 | BLOCKED · 0/3 | 实体走廊检查被以下物件阻挡：HC_VS4_R2_V2_ISM_0350、HC_VS4_R2_V2_ISM_0352、HC_R3_ShortRail_SM_MOD_Railing_03_straight_01、HC_R3_Clipping2_RailContinuation_gap_17d989a6d7ebe7d96a；首个未通点 cm=[8638.9, -10813.7, 279.7] |
| 347 | BLOCKED · 0/2 | 实体走廊检查被以下物件阻挡：HC_VS4_R2_V2_ISM_0350、HC_VS4_R2_V2_ISM_0351、HC_R3_Clipping3_Barrier_bank_east_069、HC_R3_Clipping3_Barrier_bank_east_071；首个未通点 cm=[10701.2, -11246.6, 289] |
| 348 | BLOCKED · 0/3 | 实体走廊检查被以下物件阻挡：HC_VS4_R2_V2_ISM_0350、HC_VS4_R2_V2_ISM_0351、HC_R3_Clipping3_Barrier_bank_west_074、HC_R3_Clipping3_Barrier_bank_west_076；首个未通点 cm=[8478.9, -9208.9, 279.7] |
| 366 | BLOCKED · 0/1 | 实体走廊检查被以下物件阻挡：HC_R3_District_central_01_V2_B331、HC_R3_District_roof_south_art_finish_roof_south02_V2_B332；首个未通点 cm=[6820.1, -7861.9, 619.5] |

| 摆放余项集合 | 按区 | 按类 |
|---|---|---|
| remaining185 | {"interior": 52, "workshop": 8, "central": 3, "castle": 52, "gate": 5, "market": 9, "canal": 14, "port": 11} | {"furniture": 27, "building": 2, "shrub": 5, "planter": 9, "other": 25, "railing": 83, "car": 1, "stall": 1, "tree": 1} |
| bulk685 | {"castle": 197, "workshop": 166, "canal": 47, "market": 114, "central": 61, "interior": 43, "gate": 19, "port": 11} | {"planter": 141, "other": 128, "car": 105, "bench": 46, "lamp": 50, "tree": 178, "furniture": 8, "shrub": 2} |
| rails | {"gate": 4, "castle": 43, "market": 9, "canal": 14, "workshop": 3, "port": 10} | {"railing": 83} |

前 40 件均有同机位照片：15 件已调整，22 件未找到通过门口、支撑、碰撞检查的落点，3 件在真实人眼视角被遮挡而排除。未修改项目在拼图中明确标记。摆放工作计时上界约 101.5 分钟；剩余项保留。


| 夜间街段 | 仍超过 12 m 的区间数 |
|---|---|
| wall_loop | 44 |
| castle_approach | 5 |
| market_port | 10 |
| smith_gate | 13 |
| north_bridge | 11 |
| central_bridge | 5 |
| canal_walk | 13 |
| west_lane | 13 |
| market_lane | 12 |
| east_lane | 12 |
| tavern_street | 4 |
| harbor_cafe_lane | 4 |

灯距按道路折线投影复核，新增灯具另经可达地面、眼高和实体遮挡检查；完整区间起止米数在 STREET_LIGHT_SPACING_AFTER.json，未声称全域灯距达标。


六个水下站点按 2026-10-26 原文取消补拍，已从本轮活动巡查清单移除：canal_wall_loop_rev_068、port_wall_loop_fwd_004、port_wall_loop_fwd_005、port_wall_loop_rev_067、castle_wall_loop_fwd_063、castle_wall_loop_rev_009。原因：水下或 3 m 内无干燥可站点，用户明确不再补拍。


普通房间实走 20/20、误送回检查 40/40 通过。柜台 2/3/5 m 镜头距离各停留 60 秒，6 项误送回检查均为 0。贴墙飞行 272 帧未检测到羽毛穿墙；滑翔头发截图复核通过，贴墙近翼仍会占据较大前景，未声称所有飞行构图理想。


20 段房间逐间光照复核（最终 Shipping，正常游戏曝光）

检查依据为 20 段实走截图和 20 个补充观察机位。实走 20/20 通过，普通房间误送回检查 40/40 通过；B155 两个任务 HUD 失败。除 B027 外，本次未改室内灯光，表中的余项不写成已修。

| 房间/段 | 问题与观察 | 本轮修改 | 结论/余项 |
|---|---|---|---|
| B002_1 | 墙、地板和桌面清楚，暖色较强；本视角未见窗光方向。 | 保持已认可的公会灯光。 | 窗光方向待确认；桌面纹理可见。 |
| B002_2 | 公会走廊到柜台可读，末端偏暗；未见明确窗光斜射。 | 未改本房间。 | 保留；窗光方向待确认。 |
| B004_1 | 入口白门、白布和窗边很亮；桌面木纹可见。 | 未改本房间。 | 入口亮部仍需压低；未证实纯白面积。 |
| B004_2 | 窗端明、壁炉侧暗，亮暗差大。 | 未改本房间。 | 桌面可读；窗光方向不够明确。 |
| B008_1 | 木柱遮挡视线，桌面可读；顶灯和墙面补光主导。 | 未改本房间。 | 未确认光从窗进来。 |
| B008_2 | 窗端较亮，近桌与地板偏暗，木纹仍可辨。 | 未改本房间。 | 窗口来源不明确，阴影余项保留。 |
| B015_1 | 柜台近处高亮，木纹保留；顶灯主导。 | 未改本房间。 | 缺少明确窗光方向。 |
| B015_2 | 后窗亮，近侧桌面暗；石壁炉仍可辨。 | 未改本房间。 | 桌面暗区与窗光方向余项保留。 |
| B025_1 | 窗旁墙面有明显大亮斑，但缺少从窗延伸的光影；桌面有纹理。 | 未改本房间。 | 墙面亮斑偏强。 |
| B025_2 | 室内轮廓清楚，亮窗在后，桌面正常。 | 未改本房间。 | 可读；窗光方向待确认。 |
| B027_1 | 原黑暗改善；全室偏亮，白布仍可见褶皱。 | 启用 800/320 cd 暖白主光，前侧补光降至 65 cd。 | USER_REVIEW：整体曝光偏亮；白布取样 RGB>=250 的比例为 0。 |
| B027_2 | 墙、地板、家具和人物清楚，但整体金黄偏亮；窗光被主光淹没。 | 同上，两盏主光 4300 K，半径 1150/950 cm。 | USER_REVIEW：需降低整体亮度并建立窗光方向。 |
| B140_1 | 此机位朝窗口和墙边，不能代表整间桌面。 | 未改本房间。 | 该段窗光方向未确认；结合 _2 复核桌面。 |
| B140_2 | 窗旁家具和地板可读，木柜与桌面纹理正常。 | 未改本房间。 | 窗外来源不够明显。 |
| B141_1 | 白布保留褶皱与器皿接触阴影，人物与柜台正常。 | 未改本房间。 | 桌布可读；窗光方向待确认。 |
| B141_2 | 门边白墙/白门偏亮，桌布褶皱与木纹仍在。 | 未改本房间。 | 门侧曝光余项保留；不能把门光算成窗光。 |
| B155_1 | 柜台、墙和地板清楚，顶灯主导；无明确窗光。 | 未改房间灯光；只处理路线尝试。 | 照明可读，任务 HUD 仍失败。 |
| B155_2 | 石台阶纹理与人物可读，窗口侧明亮。 | 未改房间灯光。 | 本视角无桌面；任务 HUD 仍失败。 |
| B209_1 | 门侧角落可读，木地板纹理正常。 | 未改本房间。 | 窗光方向未直接入镜。 |
| B209_2 | 可见从左窗方向斜入地面的阴影，柜台木纹清楚；远端白门偏亮。 | 未改本房间。 | 窗光方向可辨；远端门亮度余项保留。 |



录像独立于性能测量：20 秒，1920×1080，H.264，30 FPS，600 帧；逐帧检查无冻结帧。只录 MP4，未保存未压缩帧。清理记录见 TEMP_FRAME_CLEANUP_FINAL.json；保留最终图片、MP4、少量关键截图及原生 JSON/CSV。


时间说明：暂停前实现计时上界约 480.43 分钟，已结束开发时间盒。用户本次“继续”后完成剩余验收、截图复核、报告和交付，当前累计有效时间上界为 530.18 分钟；没有把续做验收时间算作等待，也未继续修改生产代码/资产。


本轮原文顺序末尾：REQUEST_CLIPPING6_20261026.md；后写覆盖先写。已完成各保存批次的备份、自动检查、保存、重载和具名 NPC 存读档；失败或被磁盘保护阻断的尝试没有冒充通过。


本机审计目录：`D:\科研学习\codex学习\docs\HarborCity_M5_VS4\R3\appendix\clipping6_20261026`。关键 JSON：CAMERA_FINAL_AUDIT、NAVIGATION_FINAL_REVIEW、COLLISION_FINAL_REMAINING、PLACEMENT_FINAL_REMAINING、FOLIAGE_FINAL_REVIEW、RUNTIME_FINAL。


最终图片：

- [v6_task_V2_B017 · 实走终点任务栏](progress/clipping6_20261026_route_1_v6_task_V2_B017.jpg)
- [v6_task_V2_B010 · 实走终点任务栏](progress/clipping6_20261026_route_2_v6_task_V2_B010.jpg)
- [v6_task_V2_B155 · 实走未到达，保留失败画面](progress/clipping6_20261026_route_3_v6_task_V2_B155.jpg)
- [v5_room_V2_B027_2 · 实走终点任务栏](progress/clipping6_20261026_route_4_v5_room_V2_B027_2.jpg)
- [v5_room_V2_B155_1 · 实走终点任务栏](progress/clipping6_20261026_route_5_v5_room_V2_B155_1.jpg)
- [v5_room_V2_B155_2 · 实走终点任务栏](progress/clipping6_20261026_route_6_v5_room_V2_B155_2.jpg)
- [SACK74 · 无遮挡方向实际接触](progress/clipping6_20261026_SACK74.jpg)
- [摆放优先清单 1–4](progress/clipping6_20261026_placement_01_04.jpg)
- [摆放优先清单 5–8](progress/clipping6_20261026_placement_05_08.jpg)
- [摆放优先清单 9–12](progress/clipping6_20261026_placement_09_12.jpg)
- [摆放优先清单 13–16](progress/clipping6_20261026_placement_13_16.jpg)
- [摆放优先清单 17–20](progress/clipping6_20261026_placement_17_20.jpg)
- [摆放优先清单 21–24](progress/clipping6_20261026_placement_21_24.jpg)
- [摆放优先清单 25–28](progress/clipping6_20261026_placement_25_28.jpg)
- [摆放优先清单 29–32](progress/clipping6_20261026_placement_29_32.jpg)
- [摆放优先清单 33–36](progress/clipping6_20261026_placement_33_36.jpg)
- [摆放优先清单 37–40](progress/clipping6_20261026_placement_37_40.jpg)
- [公会门口左墙角 · 原地慢转 360°](progress/clipping6_20261026_orbit_01_guild_left.jpg)
- [公会门口右墙角 · 原地慢转 360°](progress/clipping6_20261026_orbit_02_guild_right.jpg)
- [公会门洞 · 原地慢转 360°](progress/clipping6_20261026_orbit_03_guild_door.jpg)
- [B004_2 · 原地慢转 360°](progress/clipping6_20261026_orbit_04_B004_2.jpg)
- [B027_2 · 原地慢转 360°](progress/clipping6_20261026_orbit_05_B027_2.jpg)
- [市场西巷一 · 原地慢转 360°](progress/clipping6_20261026_orbit_06_market_west_1.jpg)
- [市场西巷二 · 原地慢转 360°](progress/clipping6_20261026_orbit_07_market_west_2.jpg)
- [出生广场紫顶货摊 · 原地慢转 360°](progress/clipping6_20261026_orbit_08_purple_stall.jpg)
- [出生广场长椅花坛（花叶方片回归待修） · 原地慢转 360°](progress/clipping6_20261026_orbit_09_flower_bench.jpg)
- [北排屋脊（飞行驻留） · 原地慢转 360°](progress/clipping6_20261026_orbit_10_north_roof.jpg)
- [树顶（飞行驻留） · 原地慢转 360°](progress/clipping6_20261026_orbit_11_tree_top.jpg)
- [B027 · 下午和夜里主光调整](progress/clipping6_20261026_B027_day_night.jpg)
- [中央北桥 · 夜间桥下光照](progress/clipping6_20261026_north_bridge_night.jpg)
- [夜间树冠和草地 · 回归对比](progress/clipping6_20261026_night_foliage.jpg)
- [20 段房间实走 · 光照检查](progress/clipping6_20261026_rooms20_1.jpg)
- [20 段房间实走 · 光照检查](progress/clipping6_20261026_rooms20_2.jpg)
- [20 段房间实走 · 光照检查](progress/clipping6_20261026_rooms20_3.jpg)
- [长椅花坛 · FAIL，近镜头处理引入花叶方片回归](progress/clipping6_20261026_flower_before_after.jpg)

临时画面清理已验证：本次删除 791 张 PNG（2.16 GiB），这些已登记运行目录保留 10 张关键 PNG；另保留 35 张最终进度图、1 段 MP4、3 张视频关键图与联系表、4 张光照/花坛复核拼图。早期开发截图清理另有独立记录。没有将录像帧放入 Shipping 产物或 git。
