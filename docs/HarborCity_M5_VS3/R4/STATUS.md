# M5-VS3 R4 · PARTIAL · 可试玩候选

A：特效改动已入包，真实运行合计31/31系统、27/27音效有生成；完整演示未完成。B：枪体已变亮，金色/刻纹PARTIAL。C：低位持枪与物理保持，自然度USER_REVIEW。D：ADS和剑矩阵已通过，鼠标锁屏阻断NOT_RUN。E：三种星弓方案均未过，保留魔导枪蓄力光箭，星弓留VS4。

新EXE：`E:\GameDev\Builds\HarborCity\M5_VS3\Candidate_20260928_225009_194_f9a7d9e2\Archive\Windows\HarborCity\Binaries\Win64\HarborCity-Win64-Shipping.exe`

|问题|结果|对比图（review/final/）|
|---|---|---|
|1 蓄力|符文渐亮、脚下圈、闪光和刃火已入包；观感待审。|01_charge_compare.jpg|
|2 剑气|月牙和淡化收尾已入包；全程同机位慢放待补。|02_wave_compare.jpg|
|3 第一人称魔力弹|指定时刻3.5252%，低于12.5%；逐帧峰值未测。|03_fp_bolt_compare.jpg|
|4 命中残留|旧投影命中特效已停用，角色/武器不接收贴花；此图为编辑器诊断。|04_ribbons_compare.jpg|
|5 冲击|隐藏炸弹网格，新增下降光柱；此图为编辑器诊断，最终包慢放未录。|05_shock_compare.jpg|
|6 治疗|治疗十字、敌对、枪口、冲刺和拾取增强已入包；此图为编辑器诊断。|06_heal_compare.jpg|
|7 火盆|双立式火盆与石灶已入包；此图为编辑器诊断，独立包视觉未补。|07_hearths_compare.jpg|
|8 魔导枪|主体变亮，六棱晶核/厚环带已入包；淡金与刻纹PARTIAL。|08_gun_compare.jpg|
|9 头发|低位持枪，头发物理保留；正常速度对照已交付，自然度待审。|09_hair_compare.jpg|
|10 测试与交付|ADS20/20、剑矩阵181/0；源码差异已附。最终完整回归与完整录像NOT_RUN。|fp_contact.jpg|

FP：20个指定时刻样本均达门槛；魔力弹3.5252%，新护盾2.9477%，最大样本为治疗9.7949%。旧护盾27.9091% FAIL保留；19项来自前一R4包，仅护盾在最终包复测。不能替代逐帧峰值验证。

最终完整回归0次，NOT_RUN。High/Epic旧电池条件失败保留，接电复测NOT_RUN。主片、慢放未完成，现有录像以PARTIAL_CAPTURE明确标识。锁屏解除前停止桌面输入；所需人工步骤见MANUAL_RETEST。用户艺术验收PENDING，不扩图、不进入VS4。
