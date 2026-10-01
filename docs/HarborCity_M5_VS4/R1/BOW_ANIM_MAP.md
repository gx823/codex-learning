# 星弓动作映射

状态：已在最终独立包录制，实际状态采样时间见文末。动作有采样记录不等于接触、蒙皮穿插和原作位置对照通过。

## Exouch 同步轨道

Repair 1 每一行均对应 `/Game/HarborCity/M5VS4/Bow/FP96/A_FP_*` 的 96 Hz 手臂与 `Native/BowTracks/Animations/A_Bow_*`，同一采样时间、方向权重和 0.08 s 组件空间状态混合。原始 24 Hz 动作保留，用于关闭对准修正的运行时对照；新包最大手掌差 0.167858 cm。整个源场景已左右镜像，无运行时二次镜像。所有原件、96 Hz 动作、重定向结果及拆出的箭保持私有。

| 原动作 | 游戏状态 | 当前接入 |
|---|---|---|
| Equip | 有箭装备，最大 2 倍速 | 最终录像已采样，见文末 |
| Equip.Empty | 上次空弦时装备 | 最终录像已采样，见文末 |
| Unequip | 切走，最大 2 倍速 | 最终录像已采样，见文末 |
| Idle | 有箭待机 | 最终录像已采样，见文末 |
| EmptyIdle | 空弦待机 | 最终录像已采样，见文末 |
| Walk.Fw | 前行，与邻方向按角度混合 | 最终录像已采样，见文末 |
| Walk.Bw | 后行，与邻方向按角度混合 | 最终录像已采样，见文末 |
| Walk.L | 左行，与邻方向按角度混合 | 最终录像已采样，见文末 |
| Walk.R | 右行，与邻方向按角度混合 | 最终录像已采样，见文末 |
| Walk.Sprint_Fw | 前冲刺 | 最终录像已采样，见文末 |
| Walk.Sprint_Bw | 后冲刺 | 最终录像已采样，见文末 |
| Walk.Sprint_L | 左冲刺 | 最终录像已采样，见文末 |
| Walk.Sprint_R | 右冲刺 | 最终录像已采样，见文末 |
| Draw | 0.8 s 拉满 | 最终录像已采样，见文末 |
| Draw.Idle | 满弓持续循环，不停在末帧 | 最终录像已采样，见文末 |
| Draw.Cancel | R、菜单、切换取消，不发射 | 最终录像已采样，见文末 |
| Release | 普通放箭，0.13 s 后混出 | 最终录像已采样，见文末 |
| ReloadFromRelease | 放箭后上箭，2 倍速 | 最终录像已采样，见文末 |
| EmptyFromRelease | 冲刺放箭后保持空弦 | 最终录像已采样，见文末 |
| ReloadFromEmpty | 松开冲刺或空弦按左键，上好箭再拉弓 | 最终录像已采样，见文末 |
| Crouch.Start | 不用，游戏没有蹲姿 | 已导出导入 |
| Crouch.Idle | 不用，游戏没有蹲姿 | 已导出导入 |
| Crouch.End | 不用，游戏没有蹲姿 | 已导出导入 |
| Crouch.Walk.Fw | 不用，游戏没有蹲姿 | 已导出导入 |
| Crouch.Walk.Bw | 不用，游戏没有蹲姿 | 已导出导入 |
| Crouch.Walk.L | 不用，游戏没有蹲姿 | 已导出导入 |
| Crouch.Walk.R | 不用，游戏没有蹲姿 | 已导出导入 |
| Default | 绑定姿势，不播放 | 两个相同样本，规避零时长导入限制 |

## Ventyra

| 动作 | 用途 |
|---|---|
| Bow_Idle | 待机弓身 |
| Bow_Pull | 依据 Exouch Root 到 String 距离同步弓臂拉开程度 |
| Bow_Hold_Loop | 满弓弓身循环 |
| Bow_Release | 放箭时原有回弹与弦震 |
| Bow_Full_Action | 暂不播放；完整组合动作与分段状态机重复，保留原资产 |

## Mixamo Pro Longbow

| 原文件 | 派生动作 / TP 用途 |
|---|---|
| standing idle 01 | A_BowIdle / 待机 |
| standing equip bow | A_BowEquip / 装备 |
| standing disarm bow | A_BowUnequip / 收起 |
| standing draw arrow | A_BowReload / 上箭 |
| standing aim overdraw | A_BowAim / 拉弓、保持 |
| standing aim recoil | A_BowRelease / 放箭 |
| standing aim walk forward | A_BowAimWalk_Fw / 瞄准前行 |
| standing aim walk back | A_BowAimWalk_Bw / 瞄准后行 |
| standing aim walk left | A_BowAimWalk_L / 瞄准左行 |
| standing aim walk right | A_BowAimWalk_R / 瞄准右行 |

TP 使用 UpperBody 槽，腿保持 GAS 步态。原件 40 段中的其余 30 段不属于本轮指定动作范围。

## 校准边界

Repair 1 新 EXE 原作对照 PASS，最大手掌差 0.167858 cm；蒙皮穿插仍待画面验收。对准上限为获准的 6 度，约 0.1 s 平滑，实际最大 5.537234 度；整体平移 0 cm。旧候选保持 4 度，历史结果未改。95 度 FP 投影在 ADS 时保持不变；世界 FOV 与 1.6 倍 ADS 沿用 R6。手臂、弓、箭同步整体旋转，不做固定目标运行时双骨 IK，不另改原作横握角。

## 最终包录像状态时间点

时间相对已去掉 6.5 s 预热的正常速度视频。来自本次最终包逐帧状态采样，不代替手指穿插或原作九动作位置对照。

### VS4_BOW_FP_30S.mp4

| 原动作（导入名） | 首次实际播放区间 / s |
|---|---|
| Idle | 0.00 - 0.03 |
| Unequip | 0.03 - 0.54 |
| Equip | 1.34 - 1.87 |
| Walk_Fw | 3.74 - 4.40 |
| Walk_Bw | 4.43 - 4.99 |
| Walk_L | 4.99 - 5.60 |
| Walk_R | 5.63 - 6.15 |
| Walk_Sprint_R | 6.15 - 6.20 |
| Walk_Sprint_Fw | 6.20 - 6.84 |
| Walk_Sprint_Bw | 6.86 - 7.43 |
| Walk_Sprint_L | 7.43 - 8.06 |
| Draw | 9.28 - 10.08 |
| Draw_Idle | 10.08 - 10.89 |
| Draw_Cancel | 10.89 - 11.34 |
| Release | 12.47 - 12.60 |
| ReloadFromRelease | 12.60 - 13.01 |
| EmptyFromRelease | 17.30 - 17.44 |
| EmptyIdle | 17.44 - 18.71 |
| Equip_Empty | 20.04 - 20.54 |
| ReloadFromEmpty | 21.70 - 22.14 |

### VS4_BOW_TP_30S.mp4

| 原动作（导入名） | 首次实际播放区间 / s |
|---|---|
| Idle | 0.00 - 0.03 |
| Unequip | 0.03 - 0.53 |
| Equip | 1.34 - 1.87 |
| Walk_Fw | 3.71 - 4.38 |
| Walk_Bw | 4.40 - 4.97 |
| Walk_L | 4.97 - 5.60 |
| Walk_R | 5.63 - 6.14 |
| Walk_Sprint_R | 6.14 - 6.20 |
| Walk_Sprint_Fw | 6.20 - 6.84 |
| Walk_Sprint_Bw | 6.86 - 7.42 |
| Walk_Sprint_L | 7.42 - 8.06 |
| Draw | 9.27 - 10.08 |
| Draw_Idle | 10.08 - 10.87 |
| Draw_Cancel | 10.87 - 11.33 |
| Release | 12.47 - 12.61 |
| ReloadFromRelease | 12.61 - 13.04 |
| EmptyFromRelease | 17.28 - 17.42 |
| EmptyIdle | 17.42 - 18.71 |
| Equip_Empty | 20.04 - 20.53 |
| ReloadFromEmpty | 21.73 - 22.17 |

