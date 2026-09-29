# Selestia 原作物理数值（只读提取）

FOUND。原始 YAML 仅保存在本机 assets 下，不属于公开交付。JSON 为所需的数值、曲线采样与骨骼映射。

半径、高度、偏移保留 Unity 局部单位；不得未经祖先缩放、模型尺寸、坐标轴核验直接套入 UE。Spring 字段按原名保留，未假定与所有 PhysBone 版本的 Momentum 完全相同。

## SELESTIA_lilToon.prefab

PhysBone 50 条；Collider 14 个。

|碰撞骨骼|形状编号|半径|高度|偏移 xyz|祖先累计缩放|
|---|---:|---:|---:|---|---|
|SELESTIA_lilToon/Armature/Hips/Spine/Hair_back_C2|1|0.18|0.49|{'x': 0, 'y': -0.09, 'z': 0}|[1.0, 0.99999994, 1.0]|
|SELESTIA_lilToon/Armature/Hips/UpperLeg_R|1|0.08|0.42|{'x': 0.005, 'y': 0.13, 'z': 0.01}|[1.0, 1.0, 1.0008097]|
|SELESTIA_lilToon/Armature/Hips/Spine/Chest/Shoulder_L/UpperArm_L/LowerArm_L|1|0.035|0.22|{'x': 0, 'y': 0.08, 'z': 0}|[0.9999999, 0.999999740000012, 0.9999998800000036]|
|SELESTIA_lilToon/Armature/Hips/Spine/Chest/Shoulder_R/UpperArm_R/NadeNade_UpperArm_R|0|0.07|0.19|{'x': 0, 'y': 0.05, 'z': 0}|[0.9999999, 0.999999740000012, 0.99999994]|
|SELESTIA_lilToon/Armature/Hips/Spine/Chest/Hair_back_C1|1|0.12|0.4|{'x': 0, 'y': 0.035, 'z': 0.02}|[1.0, 0.99999994, 1.0]|
|SELESTIA_lilToon/Armature/Hips/Spine/Chest/Shoulder_L|0|0.04|2|{'x': -0.005, 'y': 0.04, 'z': -0.01}|[0.9999999, 0.999999740000012, 0.99999994]|
|SELESTIA_lilToon/Armature/Hips/Spine/Chest/Hair_side_C|1|0.1|0.28|{'x': 0, 'y': 0.04, 'z': 0}|[1.0, 0.99999994, 1.0]|
|SELESTIA_lilToon/Armature/Hips/Spine/Chest/Shoulder_R|0|0.04|2|{'x': 0.005, 'y': 0.04, 'z': -0.01}|[0.9999999, 0.999999740000012, 0.99999994]|
|SELESTIA_lilToon/Armature/Hips/Spine/Chest/Shoulder_R/UpperArm_R|1|0.035|0.2|{'x': 0, 'y': 0.1, 'z': 0}|[0.9999999, 0.999999740000012, 0.99999994]|
|SELESTIA_lilToon/Armature/Hips/Spine/Chest/Shoulder_L/UpperArm_L/NadeNade_UpperArm_L|0|0.07|0.19|{'x': 0, 'y': 0.05, 'z': 0}|[0.9999999, 0.999999740000012, 0.99999994]|
|SELESTIA_lilToon/Armature/Hips/UpperLeg_L|1|0.08|0.42|{'x': -0.005, 'y': 0.13, 'z': 0.01}|[1.0, 1.0, 1.0008097]|
|SELESTIA_lilToon/Armature/Hips/Spine/Chest/Shoulder_L/UpperArm_L|1|0.035|0.2|{'x': 0, 'y': 0.1, 'z': 0}|[0.9999999, 0.999999740000012, 0.99999994]|
|SELESTIA_lilToon/Armature/Hips/Spine/Chest/Shoulder_R/UpperArm_R/LowerArm_R|1|0.035|0.22|{'x': 0, 'y': 0.08, 'z': 0}|[0.9999999, 0.999999740000012, 0.9999998800000036]|
|SELESTIA_lilToon/Armature/Hips/Spine/Chest/Neck/Head/NadeNade_Head|0|0.11|2|{'x': 0, 'y': 0.18, 'z': 0.03}|[1.0, 0.9999998400000061, 0.9999998800000036]|

|头发根骨骼|Pull|Spring|Stiffness|Gravity|Immobile|限角 X/Z|半径|
|---|---:|---:|---:|---:|---:|---|---:|
|SELESTIA_lilToon/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_side2_L|0.06|0.974|0.441|0.17|0.543|43/45|0.03|
|SELESTIA_lilToon/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_side2_R|0.06|0.974|0.441|0.17|0.543|43/45|0.03|
|SELESTIA_lilToon/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_front|0.159|0.189|0.349|0.17|0.543|30/45|0.02|
|SELESTIA_lilToon/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_back_long1_L|0.052|0.905|0.266|0.17|0.182|60/45|0.055|
|SELESTIA_lilToon/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_ahoge|0.307|0.54|0.433|0|0.6|10/45|0.015|
|SELESTIA_lilToon/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_back_long3_L|0.052|0.905|0.266|0.17|0.182|60/45|0.055|
|SELESTIA_lilToon/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_back_long3_R|0.052|0.905|0.266|0.17|0.182|60/45|0.055|
|SELESTIA_lilToon/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_back_short1_R|0.072|0.988|0.669|0.05|0.543|45/45|0.02|
|SELESTIA_lilToon/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_back_long2_R|0.052|0.905|0.266|0.17|0.182|60/45|0.055|
|SELESTIA_lilToon/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_tail_R|0.081|0.785|0.142|0.05|0.247|45/45|0.025|
|SELESTIA_lilToon/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_back_short4|0.072|0.988|0.669|0.05|0.543|45/45|0.02|
|SELESTIA_lilToon/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_tail_L|0.081|0.785|0.142|0.05|0.247|45/45|0.025|
|SELESTIA_lilToon/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_side1_R|0.159|0.189|0.349|0.17|0.543|20/45|0.025|
|SELESTIA_lilToon/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_back_long2_L|0.052|0.905|0.266|0.17|0.182|60/45|0.055|
|SELESTIA_lilToon/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_back_short2_L|0.072|0.988|0.669|0.05|0.543|45/45|0.02|
|SELESTIA_lilToon/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_back_short2_R|0.072|0.988|0.669|0.05|0.543|45/45|0.02|
|SELESTIA_lilToon/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_ribbon_1|0.046|0.868|0.08|0|0.2|60/70|0.02|
|SELESTIA_lilToon/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_ribbon_2|0.046|0.868|0.08|0|0.2|60/70|0.02|
|SELESTIA_lilToon/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_back_long1_R|0.052|0.905|0.266|0.17|0.182|60/45|0.055|
|SELESTIA_lilToon/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_side1_L|0.159|0.189|0.349|0.17|0.543|20/45|0.025|
|SELESTIA_lilToon/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_back_short3_R|0.072|0.988|0.669|0.05|0.543|45/45|0.02|
|SELESTIA_lilToon/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_back_short1_L|0.072|0.988|0.669|0.05|0.543|45/45|0.02|
|SELESTIA_lilToon/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_back_short3_L|0.072|0.988|0.669|0.05|0.543|45/45|0.02|
## SELESTIA_UTS.prefab

PhysBone 50 条；Collider 14 个。

|碰撞骨骼|形状编号|半径|高度|偏移 xyz|祖先累计缩放|
|---|---:|---:|---:|---|---|
|SELESTIA_UTS/Armature/Hips/Spine/Chest/Shoulder_R|0|0.04|2|{'x': 0.005, 'y': 0.04, 'z': -0.01}|[0.9999999, 0.999999740000012, 0.99999994]|
|SELESTIA_UTS/Armature/Hips/Spine/Chest/Shoulder_R/UpperArm_R|1|0.035|0.2|{'x': 0, 'y': 0.1, 'z': 0}|[0.9999999, 0.999999740000012, 0.99999994]|
|SELESTIA_UTS/Armature/Hips/Spine/Chest/Shoulder_L/UpperArm_L/NadeNade_UpperArm_L|0|0.07|0.19|{'x': 0, 'y': 0.05, 'z': 0}|[0.9999999, 0.999999740000012, 0.99999994]|
|SELESTIA_UTS/Armature/Hips/UpperLeg_L|1|0.08|0.42|{'x': -0.005, 'y': 0.13, 'z': 0.01}|[1.0, 1.0, 1.0008097]|
|SELESTIA_UTS/Armature/Hips/Spine/Chest/Shoulder_L/UpperArm_L|1|0.035|0.2|{'x': 0, 'y': 0.1, 'z': 0}|[0.9999999, 0.999999740000012, 0.99999994]|
|SELESTIA_UTS/Armature/Hips/Spine/Chest/Shoulder_R/UpperArm_R/LowerArm_R|1|0.035|0.22|{'x': 0, 'y': 0.08, 'z': 0}|[0.9999999, 0.999999740000012, 0.9999998800000036]|
|SELESTIA_UTS/Armature/Hips/Spine/Chest/Neck/Head/NadeNade_Head|0|0.11|2|{'x': 0, 'y': 0.18, 'z': 0.03}|[1.0, 0.9999998400000061, 0.9999998800000036]|
|SELESTIA_UTS/Armature/Hips/UpperLeg_R|1|0.08|0.42|{'x': 0.005, 'y': 0.13, 'z': 0.01}|[1.0, 1.0, 1.0008097]|
|SELESTIA_UTS/Armature/Hips/Spine/Chest/Shoulder_L/UpperArm_L/LowerArm_L|1|0.035|0.22|{'x': 0, 'y': 0.08, 'z': 0}|[0.9999999, 0.999999740000012, 0.9999998800000036]|
|SELESTIA_UTS/Armature/Hips/Spine/Chest/Hair_back_C1|1|0.12|0.4|{'x': 0, 'y': 0.035, 'z': 0.02}|[1.0, 0.99999994, 1.0]|
|SELESTIA_UTS/Armature/Hips/Spine/Chest/Shoulder_R/UpperArm_R/NadeNade_UpperArm_R|0|0.07|0.19|{'x': 0, 'y': 0.05, 'z': 0}|[0.9999999, 0.999999740000012, 0.99999994]|
|SELESTIA_UTS/Armature/Hips/Spine/Chest/Shoulder_L|0|0.04|2|{'x': -0.005, 'y': 0.04, 'z': -0.01}|[0.9999999, 0.999999740000012, 0.99999994]|
|SELESTIA_UTS/Armature/Hips/Spine/Chest/Hair_side_C|1|0.1|0.28|{'x': 0, 'y': 0.04, 'z': 0}|[1.0, 0.99999994, 1.0]|
|SELESTIA_UTS/Armature/Hips/Spine/Hair_back_C2|1|0.18|0.49|{'x': 0, 'y': -0.09, 'z': 0}|[1.0, 0.99999994, 1.0]|

|头发根骨骼|Pull|Spring|Stiffness|Gravity|Immobile|限角 X/Z|半径|
|---|---:|---:|---:|---:|---:|---|---:|
|SELESTIA_UTS/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_back_long1_R|0.052|0.905|0.266|0.17|0.182|60/45|0.055|
|SELESTIA_UTS/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_side1_L|0.159|0.189|0.349|0.17|0.543|20/45|0.025|
|SELESTIA_UTS/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_back_short3_R|0.072|0.988|0.669|0.05|0.543|45/45|0.02|
|SELESTIA_UTS/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_back_short1_L|0.072|0.988|0.669|0.05|0.543|45/45|0.02|
|SELESTIA_UTS/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_back_short3_L|0.072|0.988|0.669|0.05|0.543|45/45|0.02|
|SELESTIA_UTS/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_back_long2_L|0.052|0.905|0.266|0.17|0.182|60/45|0.055|
|SELESTIA_UTS/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_back_short2_L|0.072|0.988|0.669|0.05|0.543|45/45|0.02|
|SELESTIA_UTS/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_back_short2_R|0.072|0.988|0.669|0.05|0.543|45/45|0.02|
|SELESTIA_UTS/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_ribbon_2|0.046|0.868|0.08|0|0.2|60/70|0.02|
|SELESTIA_UTS/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_ribbon_1|0.046|0.868|0.08|0|0.2|60/70|0.02|
|SELESTIA_UTS/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_back_long2_R|0.052|0.905|0.266|0.17|0.182|60/45|0.055|
|SELESTIA_UTS/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_tail_R|0.081|0.785|0.142|0.05|0.247|45/45|0.025|
|SELESTIA_UTS/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_back_short4|0.072|0.988|0.669|0.05|0.543|45/45|0.02|
|SELESTIA_UTS/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_tail_L|0.081|0.785|0.142|0.05|0.247|45/45|0.025|
|SELESTIA_UTS/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_side1_R|0.159|0.189|0.349|0.17|0.543|20/45|0.025|
|SELESTIA_UTS/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_side2_R|0.06|0.974|0.441|0.17|0.543|43/45|0.03|
|SELESTIA_UTS/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_side2_L|0.06|0.974|0.441|0.17|0.543|43/45|0.03|
|SELESTIA_UTS/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_front|0.159|0.189|0.349|0.17|0.543|30/45|0.02|
|SELESTIA_UTS/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_back_long1_L|0.052|0.905|0.266|0.17|0.182|60/45|0.055|
|SELESTIA_UTS/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_back_long3_L|0.052|0.905|0.266|0.17|0.182|60/45|0.055|
|SELESTIA_UTS/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_ahoge|0.307|0.54|0.433|0|0.6|10/45|0.015|
|SELESTIA_UTS/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_back_long3_R|0.052|0.905|0.266|0.17|0.182|60/45|0.055|
|SELESTIA_UTS/Armature/Hips/Spine/Chest/Neck/Head/Selestia_hair_root/Hair_back_short1_R|0.072|0.988|0.669|0.05|0.543|45/45|0.02|

## Kawaii 对应关系（本轮建议，非官方换算）

|原参数|Kawaii 参考项|限制|
|---|---|---|
|Pull / Stiffness|Stiffness 与 Rest 骨架回复|综合拟合，不能直接相等|
|Spring / Momentum|Damping|用实际摆动衰减时间拟合，不按 1−Spring 直接替代|
|Gravity / Falloff|Gravity 与链深度分布|先核验方向与单位|
|Immobile|WorldDamping / LocationDamping|控制角色运动惯性，非相同求解模型|
|Limit X/Z / rotation|LimitAngle 与骨骼参考朝向|Kawaii 单角锥不能精确表达原二维限制|
|Radius / Collider|PhysicsSettings.Radius / CapsuleLimits|按骨骼空间和实际比例转换；运行时逐帧验证|

本表只确认原作数据存在，尚未表示 UE 映射或防穿验收通过。
