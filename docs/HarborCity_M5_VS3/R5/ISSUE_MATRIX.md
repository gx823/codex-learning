# R5：11条问题与A—E对应证据

整体PARTIAL。所有图片位于review/final；用户美术与手感验收PENDING。严格同机位不足的对比明确注明。

|第三节|任务|结论/缺口|图片文件名|
|---|---|---|---|
|1 跑跳不能出招|A|移动/空中分层已实测；跑剑和跳剑各组10/10。横向1.2m跑拳0/10；C-OS动作未验收。R4同机位跑跳对比NOT_RUN。|01_mobile_R5.jpg|
|2 跑步手臂穿发|B|三方案已用完，走通过，跑/冲刺未达标；左侧为A中间包、右侧最终包，同机位正常速度。|02_hair_compare.jpg、02_hair_worst_proxy_frames.jpg|
|3 重击横移/出画|A/C|动画源根位移220.534cm；R4无目标时绕过夹紧。R5站定只取前向限幅，移动忽略根运动。旧录像具体目标状态UNKNOWN；双人10/10完整入画NOT_MEASURED。|03_wave.jpg|
|4 冲击光柱|C|独立包原生录像确有预告/下落柱/落地爆发；左侧目标不始终完整入画，慢放命中0保留FAIL。伤害仍在释放，视觉落地约晚0.3秒。|04_shock_triptych.jpg|
|5 治疗没证据|C|真实施放，法阵/绿十字出现；FP专项先受伤至70，峰值12.621%。数字清晰度仍需用户看，R4同机位对照NOT_RUN。|05_heal.jpg|
|6 FP过弱|C|最终包生命周期诊断11/20数值通过、9FAIL；控制噪声及低于/超过界限均保留。诊断30Hz不能冒充主视口原生30FPS。|06_fp_peaks.jpg|
|7 其余特效未验证|C|完整回归仅一次：18/31系统、17/27音效；未用上均FAIL。炉火、Dash及全部效果标准机位清晰度尚未完整验证。|07_effects.jpg|
|8 白塑料枪|D|恢复细节及原法线/粗糙度，三处R4/R5机位相同；主体偏灰、金属偏暗，屏幕色未达标。|08_gun_three_views.jpg、08_gun_blender_compare.jpg|
|9 发梢/蓝箭头|B|风/外力为0，1秒发梢持枪8.41/ADS14.63/施法26.05cm均FAIL。Arrow召唤来源已替换，所查最终待机画面未见旧蓝箭头；全状态无残留未完整证明。|09_hair_idle.jpg|
|10 验证交付|五—七|完整回归171/3旧FAIL、矩阵140/0、光箭37/0；接电性能达标，4段MP4已交。C-OS PARTIAL，最终包M4旧入口NOT_RUN。|10_os_armory.jpg；PERFORMANCE.md、RECORDING.md|
|11 星弓失败相同|E|旧代码路线确有差别，但TP指标共享构造，不能当三种独立FP量测。新依赖BLOCKED_DOWNLOAD，未重试旧弓。|新弓FP/TP图NOT_RUN；appendix/R4_BOW_ATTEMPT_AUDIT.json|

数值明细：A_FINAL_ACCEPTANCE、A_FINAL_CONTACT、A_FINAL_DISTANCE_AUDIT、B_FINAL_QUANT、FP_FINAL_PEAKS（均在appendix）。旧矩阵94FAIL：反复失焦已确认，触发失焦的外因UNKNOWN；未将其标成失效记录后丢弃。
