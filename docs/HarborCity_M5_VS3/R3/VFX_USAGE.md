# 购买包实际用途与录像索引

新独立包最终展示运行：**31/31 系统、27/27 音效**实际生成/播放。0次项目：{'systems': [], 'sounds': []}。武器库预览、临时工程演示不计入覆盖。

包内没有NS_BasicAttack；空手与NPC反击用小尺寸Gunshot_Hit配SFX_BasicAttack：短促、集中在胸口接触处，比大范围爆炸更适合拳击。两个Omni分别用于空中光箭命中和剑气终点。

| 系统（NiagaraSystem） | 场景 | 运行生命周期上限s | 录像约秒 | 次数 |
|---|---|---|---:|---:|
| NS_Arrow_Cast | 魔导枪蓄力光箭（E.7降级） | .5 | 58.68 | 2 |
| NS_Arrow_Hit | 魔导枪蓄力光箭（E.7降级） | 2.5 | 58.75 | 2 |
| NS_Arrow_Projectile | 魔导枪蓄力光箭（E.7降级） | 按弹体存活，最长约1.2 | 58.70 | 2 |
| NS_Bomb_Explosion | 2号冲击：预告/上方弹体/落地爆发 | 2 | 80.56 | 1 |
| NS_Bomb_Projectile | 2号冲击：预告/上方弹体/落地爆发 | 2 | 80.47 | 1 |
| NS_Bomb_Spawn | 2号冲击：预告/上方弹体/落地爆发 | 选目标期间，最多12 | 78.63 | 1 |
| NS_Buff_Cast | 召唤、4号护盾/持续；装备预览不计数 | 1–2 | 108.52 | 12 |
| NS_Buff_Loop | 召唤、4号护盾/持续；装备预览不计数 | 护盾6/蓄力按住至30保护上限 | 109.14 | 11 |
| NS_Dash | 吸附冲步超过0.8m或飞行Shift加速 | 1 | 143.49 | 1 |
| NS_Debuff_Cast | 成年NPC转敌对/敌对期间淡循环 | 2.5 | 88.22 | 2 |
| NS_Debuff_Loop | 成年NPC转敌对/敌对期间淡循环 | 8，敌对时续播 | 88.88 | 2 |
| NS_Explosion_Fire_Ground | 重击与冲击地面环 | 2.5 | 13.45 | 3 |
| NS_Explosion_Fire_Omni | 蓄力光箭胸部空中命中 | 2.5 | 58.81 | 2 |
| NS_Explosion_Ice_Ground | 1号魔力弹命中 | 2.5 | 73.41 | 2 |
| NS_Explosion_Ice_Omni | 剑气约8m尽头碎光 | 2.5 | 13.85 | 3 |
| NS_Fire | 绯夜蓄力；傍晚港口两火盆和咖啡馆炉火 | 蓄力按住；环境60保护上限/自然结束重播 | 172.57 | 40 |
| NS_Fireball_Cast | 绯夜蓄力、两剑剑气核心和命中/1号发射 | 蓄力期间/施法最多12 | 71.35 | 19 |
| NS_Fireball_Hit | 绯夜蓄力、两剑剑气核心和命中/1号发射 | 2.5 | 13.65 | 6 |
| NS_Fireball_Projectile | 绯夜蓄力、两剑剑气核心和命中/1号发射 | 剑气.7/魔弹按弹体 | 13.55 | 4 |
| NS_Gunshot_Cast | 魔导枪与紧凑近战命中 | 2.5 | 37.43 | 3 |
| NS_Gunshot_Hit | 魔导枪与紧凑近战命中 | 2.5 | 3.45 | 17 |
| NS_Gunshot_Projectile | 魔导枪与紧凑近战命中 | 按实际弹体 | 37.43 | 3 |
| NS_Heal_Cast | 3号实际受伤后的治疗 | 1 | 92.95 | 2 |
| NS_Heal_Loop | 3号实际受伤后的治疗 | 3 | 93.62 | 1 |
| NS_Lightning_Cast | 蒸汽铳装备/装弹/枪口/弹道/命中 | .8 | 42.61 | 3 |
| NS_Lightning_Hit | 蒸汽铳装备/装弹/枪口/弹道/命中 | 2.5 | 43.95 | 5 |
| NS_Lightning_Muzzle | 蒸汽铳装备/装弹/枪口/弹道/命中 | .35 | 43.87 | 5 |
| NS_Lightning_Range | 蒸汽铳装备/装弹/枪口/弹道/命中 | .18或按弹体 | 43.91 | 10 |
| NS_Pickup_Cast | 可交互任务点/咖啡领取 | 2.5 | 180.38 | 1 |
| NS_Pickup_Loop | 可交互任务点/咖啡领取 | 8，靠近时续播 | 177.38 | 2 |
| NS_Slash | 动画通知启动的三段月牙/重击剑气 | .3/剑气1.5 | 3.45 | 12 |

这里的时长是运行代码的终止上限/续播规则，不是伪造固定原包片长；Niagara多个发射器各自结束时间不同，可能提前自然消失。原包逐项发射器/参数盘点见FX_INVENTORY。录像时点由实际步骤时钟定位并抽帧，具体可见性仍与生成计数分开。

| 音效（SoundWave） | 原音时长s | 实际播放次数 | 场景 |
|---|---:|---:|---|
| SFX_ArrowShot_Cast | 1.030 | 2 | 对应ArrowShot特效场景 |
| SFX_ArrowShot_Hit | 0.817 | 2 | 对应ArrowShot特效场景 |
| SFX_BasicAttack | 0.972 | 28 | 拳脚与近战 |
| SFX_Bomb_Bounce | 0.993 | 1 | 对应Bomb特效场景 |
| SFX_Bomb_Cast | 2.181 | 1 | 对应Bomb特效场景 |
| SFX_Bomb_Explosion | 2.020 | 1 | 对应Bomb特效场景 |
| SFX_Bomb_Launch | 1.418 | 1 | 对应Bomb特效场景 |
| SFX_Bomb_Loop | 2.762 | 1 | 对应Bomb特效场景 |
| SFX_Buff_Cast | 2.154 | 13 | 对应Buff特效场景 |
| SFX_Buff_Loop | 2.957 | 1 | 对应Buff特效场景 |
| SFX_Dash | 0.749 | 1 | 对应Dash特效场景 |
| SFX_Debuff_Cast | 2.392 | 2 | 对应Debuff特效场景 |
| SFX_Debuff_Loop | 2.227 | 2 | 对应Debuff特效场景 |
| SFX_Explosion | 1.748 | 4 | 对应Explosion特效场景 |
| SFX_ExplosionIce | 1.824 | 5 | 对应ExplosionIce特效场景 |
| SFX_FireBall_Cast | 2.236 | 12 | 对应FireBall特效场景 |
| SFX_FireBall_Hit | 1.830 | 6 | 对应FireBall特效场景 |
| SFX_FireBall_Projectile | 2.461 | 4 | 对应FireBall特效场景 |
| SFX_GunShot | 1.020 | 3 | 对应GunShot特效场景 |
| SFX_GunShot_Impact | 0.920 | 3 | 对应GunShot特效场景 |
| SFX_Heal_Cast | 3.279 | 2 | 对应Heal特效场景 |
| SFX_Heal_Loop | 2.957 | 1 | 对应Heal特效场景 |
| SFX_Lightning_Cast | 2.247 | 3 | 对应Lightning特效场景 |
| SFX_Lightning_Hit | 1.641 | 5 | 对应Lightning特效场景 |
| SFX_Lightning_Launch | 1.372 | 5 | 对应Lightning特效场景 |
| SFX_PickUp | 1.507 | 1 | 对应PickUp特效场景 |
| SFX_PickUp_Loop | 1.837 | 2 | 对应PickUp特效场景 |

预算：每个R3复制发射器FixedCount128；最多40个托管系统，按发射器数×128预留合计20,480粒子位，最多160个购买包发射器/渲染器；本片预留峰值14080、系统峰值8、预算拒绝0次。不是GPU实际活粒子测量。法术弹体最多24、剑气4、声音16。购买包接入路径新增动态点光源0，三处火源用自发光。半透明每像素叠层尚未GPU过绘测量，NOT_RUN；不能把160个渲染器写成每像素层数。

原包目录保持 /Game/Vefects/Anime_Stylized_VFX/；副本 /Game/HarborCity/M5VS3/R3/FX/。31系统和27音效强引用预加载，颜色/曲线、尺寸和位置只改副本/实例。所有购买资源和派生资产被Git忽略并加密入包。

NS_Postgate有对应者已由购买包替换；保留自制BladeRibbon连续剑刃拖尾（包中Slash是月牙、没有等价持续刀刃采样带），以及玩法读数/地面残留/护盾受击形体。护盾形体承担6秒边界与受击提示，购买Buff承担展开/循环；这些不计购买包覆盖。

视觉仍PARTIAL/USER_REVIEW：2号光柱下降演出、每种三层/1–3帧强闪、所有第一人称帧≤四分之一遮挡未做逐像素穷举；首次所有技能无卡顿不能仅凭强引用加载宣称PASS。

## 最终逐帧抽检保留项

敌对提示有遮挡、光箭弹体短暂；12–13秒剑蓄力符文与脚下圈偏弱；13.75–14.15秒蓝白剑气收尾出现黑烟；场景火焰间歇。31/31触发不等于31项视觉通过。三层结构、1–3帧强闪、所有第一人称效果四分之一遮挡限值和实际逐像素半透明层数未逐项量化，保持PARTIAL/NOT_RUN。未使用系统/音效均为0；包内没有NS_BasicAttack，见空手替代映射。
