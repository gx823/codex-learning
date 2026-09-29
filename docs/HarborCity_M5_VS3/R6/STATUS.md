# M5-VS3 R6 / PARTIAL

中间 Shipping EXE（A 先行包，保留）：
`E:\GameDev\Builds\HarborCity\M5_VS3\Candidate_20260929_202113_410_b188b92d\Archive\Windows\HarborCity\Binaries\Win64\HarborCity-Win64-Shipping.exe`

最终可试玩 Shipping EXE（UE 5.8.2，Cook、归档与构建输入稳定性 PASS）：
`E:\GameDev\Builds\HarborCity\M5_VS3\Candidate_20260929_214747_862_494a0410\Archive\Windows\HarborCity\Binaries\Win64\HarborCity-Win64-Shipping.exe`

最终 EXE SHA256：`90295598EBB7FDD0E458AB95E76C82986D271511C71329164FBC29237FA3FFAC`。最终包 2026-09-29 22:01 完成；编辑器关闭后独立运行。原生容器加密及单一目标无密钥提取拒绝验证 PASS（不是绝对防提取保证）。购买/派生素材只在本地及加密游戏中，完整游戏不上传。

## 结论

- A：Shift 冲刺意图不再被剑、拳、施法或共用枪/弓入口清除。最终包双视角各十次：轻剑/拳最低 552.5、腰射 650、ADS/施法/装弹 400、长按蓄力 200 cm/s。最终动作结束后最多约 0.134 s 恢复，但 TP 拳命中后的窗口恢复最差 0.733 s，仍 FAIL。
- B：Ventyra 原生五段动作、Exouch FP 和 Mixamo TP 已接入并可装备；TP 8/10、FP 10/10，FP 遮挡 10.6041%。第一人称左手握姿仍不自然，不能把技术接入当成美术验收。
- C：20 项 FP 生命周期峰值 19 PASS；光箭蓄力 28.6% 超限。象牙色改善但仍未达标。咖啡馆仍有墙体遮挡，局部淡化未完成。鼠标关闭 C-OS 因原生插件连接失败未验证。
- D：唯一完整回归 235 项，仅 3 个原有接送 FAIL；同一次实际调用 31/31 系统、27/27 音效。70 cm 跑拳 10/10；冲刺穿发仍约 72%--74%、3.4 cm。High 111.487 FPS/p99 10.396 ms，Epic 74.444/16.243，均接电且与录像分开。
- 30 分钟探索：FAIL：wall_seconds=1800.003 goals=0 hitches_gt100ms=0 frames=219486；进程退出码 0，AC 全程 True；实际到达 0 个路线目标，采样路径约 0.0 m。角色没有实际移动，因此只证明原地稳定运行，不满足自由探索。 私有内存增长 24.57 MiB，峰值 4692.84 MiB；工作集增长 23.11 MiB。

冲刺最初长会话耗尽弹药的失败、星弓 TP miss、光箭普通 9/10、旧目录数量断言、历史倒地/接送与旧 M4 入口不可用均保留。精确数据和证据等级见 TEST_REPORT，不宣称 A/B 或整轮已全面验收。

## 交付

审阅页 `review/index.html` 只放结论和图；技术细节在附录，完整自有源码及工具差异 `appendix/SOURCE_CHANGES.patch`。主片原生采集 182.245 s / 29.987 FPS，真实游戏 0.25 倍慢放、咖啡馆、双视角星弓短片及特效诊断分别交付；最终编码和音频核验见 RECORDING。

文档进入 gx823/codex-learning，审阅 ZIP 和 MP4 放 Release。公开白名单排除购买原件/解压件/派生资产、Selestia 原始数据、私人音乐、独立音效、完整游戏及存档。发布回执见本地 `github/docs_publication.json`、`publication_result.json`。

用户对冲刺、星弓、镜头、特效和魔导枪的验收全部 **PENDING**。未扩图、未升级引擎、未触碰旧 MAD 或其他项目。请求追加回归授权未得到新答复时，保持当前包，不擅自执行第二次完整回归。
