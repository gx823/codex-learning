# M5-VS4 R1 Repair 1 / PARTIAL / USER_REVIEW

2026-10-01：星弓收尾、新候选独立测试、唯一追加完整回归、性能与五段录像均已完成。桌面操作结束，不再启动游戏。技术专项通过，完整回归仍有三个旧接送 FAIL；用户观感验收 PENDING。扩图 B0、B2-B5 留 R2，旧港町默认、P 菜单白模预览不变。

## 新候选

EXE：`E:\GameDev\Builds\HarborCity\M5_VS4\Candidate_20261001_214751_102_bc437d0e\Archive\Windows\HarborCity\Binaries\Win64\HarborCity-Win64-Shipping.exe`

SHA-256：`84B51E766AD007454A207359818D7BCCFB14ACEDE42EC4449EA994F73C21C2CE`

UE 5.8.2 Shipping，Compile/Cook/Stage/Pak/Archive、双地图审计、389 个源码输入绑定及五类私有资源加密边界 PASS。编辑器正常退出后从该 EXE 独立实测；未修改冻结候选。

## 最终结果

| 项目 | 本候选结果 |
|---|---|
| A6 | PASS，189/0；FP/TP 轴角最大 0.202876/0.127374 度，最大整体修正 5.537234 度，平移 0 cm |
| 静止靶 | FP 横/纵最大 0.006324/0.002478 cm，TP 0.003538/0.006559 cm；每视角十发，靶面位移全为 0；NPC 各 10/10 |
| 交接与遮挡 | FP/ADS 0 px，TP 0 cm；准星 192 x 108 px 方框手/袖子/弦均 0 px；竖带 2,194 px 仅参考；弓手占比 13.461% |
| 96 Hz 原作对照 | PASS，九段均采到，最大手掌差 0.167858 cm；修正和平移关闭，未放宽 3 cm 门槛 |
| 魔导枪 / C2 | PASS，ADS 普通十发、蓄力十发均命中；蓄力/释放峰值 17.8699%/14.5952% |
| C1 + R6 十会话 | PASS，整组 10/10、320/0、无中断或拼接；拳 FP/TP 最慢恢复 0.05000/0.05001 s；仅速度与恢复门槛，动态命中观察另列 |
| 剑矩阵 / P 预览往返 | PASS，140/0、12/0；真实 OS 键鼠仍 BLOCKED |
| 唯一追加完整回归 | COMPLETE，235 项 / 3 FAIL；同次实际触发 31/31 系统、27/27 音效；额度已用完，不再追加 |
| 旧港町压力性能 | High 108.837 FPS / p99 11.013 ms；Epic 73.337 / 16.445，各一次、1080p、接电、无录像 |
| 录像 | 新 EXE 五段 H.264/AAC，解码/FPS/音频均 PASS；作者对照为满弓、放箭、上箭的 1.295 s 短片 |

完整回归三个 FAIL：合法路边上客车门落点、真实乘客上车、正常停车下客完成。它们没有因 31/31、27/27 覆盖完整而被隐藏。手指蒙皮穿插与原作大幅横握的主观观感仍为 USER_REVIEW，不以几何接触断言代替。

## 素材与交付

City Pack、Highlands Castle 均由用户确认个人档并添加到 VFXStaging。23:11 仅一次只读盘点：`Fantastic_City_Pack` 为 1,081 文件 / 3,401,966,253 字节；`Fantastic_Highlands_Castle` 为 385 / 558,817,181。已落盘，不等同于启动器完成回执或工程加载验证；迁移 0 资产 / 0 字节，留 R2。未操作启动器、未打开 VFXStaging、未改 L_CapitalPreview、未升级引擎。

验收细项见 TEST_REPORT.md，录像规格见 RECORDING.md，审阅入口为 review_repair1/index.html。旧候选结果在 prior_candidate_162634/ 原样保留，本轮执行时间线在 appendix/REPAIR1_EXECUTION_HISTORY.md，全部源码/工具差异在 appendix/SOURCE_CHANGES.patch。

公开文件只含报告、源码差异和渲染画面。购买原件、96 Hz 派生动作、重定向结果、拆出的箭、私有音频、游戏归档、存档、密钥与逐帧序列均不上传。ZIP 与五段视频发布到本候选专属 Release；实际发布回执在本地 github/。

## 待用户

按 MANUAL_RETEST.md 试玩第一/第三人称握弓、放箭与上箭观感、B 关闭按钮、ADS 首帧及 P 预览返回后的旧存档状态。C-OS 因真实桌面输入工具不可用，不能由引擎测试代替。暂不调整原作握法；王都迁移与扩图下一轮再做。
