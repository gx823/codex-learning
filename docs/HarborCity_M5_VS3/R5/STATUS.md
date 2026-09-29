# M5-VS3 R5 · PARTIAL · 可试玩交付

最终 Shipping EXE（UE 5.8.2，编辑器关闭后实测）：

`E:\GameDev\Builds\HarborCity\M5_VS3\Candidate_20260929_165508_714_fce88b68\Archive\Windows\HarborCity\Binaries\Win64\HarborCity-Win64-Shipping.exe`

SHA-256：`2C6E92F039C7098F8661D9F912DD66BB9898AEB18675330B92761DBD73B26268`

A 中间 Shipping EXE（B/C/D 修正前）：

`E:\GameDev\Builds\HarborCity\M5_VS3\Candidate_20260929_144447_429_f177dbcb\Archive\Windows\HarborCity\Binaries\Win64\HarborCity-Win64-Shipping.exe`

SHA-256：`b901500f1b8200fdb2496d6686a54a914b75d384dbd06ee10b3a85535e027ea4`

2026-09-29，技术交付 PARTIAL；用户手感、美术验收 PENDING。游戏与编辑器均已退出，桌面操作结束。最终各批独立包测试开始时 AC=1、电量100%、Windows未锁屏。构建、Cook、打包及原生资源/索引加密检查 PASS；游戏整包仅留本地。

|范围|完成情况与未达标项|
|---|---|
|A 跑跳出招|移动/空中上半身分层与根运动规则已落地；跑剑、冲刺剑、FP剑各10/10命中，最低/平均320cm/s。原地与跑跳剑各10/10；剑跳跃高度最大误差0.00314%、跑跳距离0.01888%。规定横向1.2m跑拳0/10；改为近距离场景的跳拳各10/10单独记录，不覆盖原失败。|
|B 头发|已找到原作PhysBone、实量并完成三种方案；保持头发物理。30FPS走/跑/冲刺穿插0%/8%/70.37%，最深0/0.724/3.362cm。跑步、冲刺及1秒发梢偏移未达标。|
|C 特效|最终独立包实际冲击/治疗与主片、慢放已取得。FP完整生命周期配对诊断20项：11数值通过、9失败；30Hz模拟不是原生30FPS主视口验收。标准取景、完整8m剑气和其余效果清晰度仍有缺口。|
|D 魔导枪|恢复纹理/法线/粗糙度，R4与R5三处同机位实机对照及Blender渲染已交。屏幕象牙/金色取样FAIL；傍晚小尺寸枪体可靠取样NOT_MEASURED。|
|E 星弓|BLOCKED_DOWNLOAD；新已购包未取得，未恢复装备。旧方案共享第三人称指标的问题已审计。|
|第五节测试|完整回归只跑一次：171项/3旧FAIL；剑矩阵140/0；普通与蓄力光箭各10/10、专项37/0。旧M4入口在最终Shipping不可用，NOT_RUN；保留10分钟超时与编辑器旧结果。|
|性能|接电1080p High负载109.744FPS/p99 10.482ms；Epic73.815/16.174；均达目标。实际冲击命中6名NPC。|
|C-OS|坐标输入已发送，但截图停在武器库，关闭后跑跳证据无效；PARTIAL，人工实点PENDING。|
|第六节|主片236.907秒/29.97FPS，−20.0LUFS/−6.08dBTP；游戏内0.25倍慢放130.6秒。另有正常速度头发对比和FP诊断视频；缺新弓，部分机位不达要求。|
|第七节|本地新包、文档、源码diff及审阅白名单已准备；GitHub上传结果以本地github/docs_publication.json与publication_result.json为准。|

包内31系统/27音效均Cook；**最终一次完整回归只使用18/31系统、17/27音效**，零次均FAIL。禁止跨运行累计或用Cook覆盖代替实际使用。旧接送/倒地FAIL保留并移交M5-VS4，本轮未扩图。

[审阅页](review/index.html) · [11项问题与图像索引](ISSUE_MATRIX.md) · [人工复测](MANUAL_RETEST.md) · [发布地址（上传前为预定链接）](https://github.com/gx823/codex-learning/releases/tag/harborcity-m5-vs3-r5-20260929-fce88b68)

失败运行全部保留在appendix/FAILURE_LEDGER.json；不以新PASS覆盖旧失败。完整游戏连续30分钟自由探索专项NOT_RUN；不能将多次短测累加成一次稳定性测试。
