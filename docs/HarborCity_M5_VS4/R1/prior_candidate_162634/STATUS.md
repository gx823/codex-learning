# M5-VS4 R1 / PARTIAL

最终 Shipping 已构建并独立实测；录像和公开审阅包正在收尾。星弓 A6 FAIL，新王都仅 B1 白模，用户观感验收 PENDING。不是 A/B 全部完成版。

最终 EXE：`E:\GameDev\Builds\HarborCity\M5_VS4\Candidate_20261001_162634_899_a993e2fd\Archive\Windows\HarborCity\Binaries\Win64\HarborCity-Win64-Shipping.exe`

中间包 1：NOT_RUN（A 未完成验收）。中间包 2：NOT_RUN（B3 未完成）。此前 `Candidate_20261001_155949_568_e006d0b1` 是诊断候选，不能冒充中间包 1。

## 结论

- A：28 + 28 条 Exouch 同步轨道、原箭、作者相机整体镜像已导入；Ventyra 真比例、弓弦和弓臂动作、空弦/取消/装填状态已接入。运行状态使用 Exouch 20 段、Ventyra 4 段、Mixamo 10 段，实际录像时间点见 BOW_ANIM_MAP.md。
- A6 FAIL：名义 20 m 满弓 FP 1/10、TP 0/10。距离夹具未稳定瞄在预定靶面；FP 实际约 40 m 轴角 1.18026 / 1.182146 度，中央手像素 3572，仍不达标。交接、几何接触和 13.372% 占比通过，不等于整项通过。
- B：500 x 450 m Landscape、104 临时体块、7 地标体量、3 桥位、高差 32.2 m。正式建筑 0；City Pack / Highlands Castle 均已由用户确认个人档，但尚未加入 VFXStaging，各迁移 0 资产/0 字节；B2-B5 未完成。没有使用未购买的 Seaside Town，港口仍无正式船只替代。
- 默认仍是旧港町；P 菜单进入新王都预览。正式切图回调往返 12 项通过；新图主线、接送、NPC 导航、室内、车路、小地图未迁移，不能当作完整可玩王都。
- C1 PARTIAL / WAITING_HOST：01-03 COMPLETE，04 全部采样存在但完成标记缺失，05 失焦中断 FAIL，06-10 NOT_RUN；不宣称十会话通过。C2 PASS：最终包光箭蓄力峰值 17.87534%、释放 14.62114%。魔导枪普通/蓄力 ADS 均 10/10，冻结倍率通过。
- 唯一完整回归已执行 1 次：235 项、3 个旧接送 FAIL，同次实际使用 31/31 系统、27/27 音效。R5 移动 328/0；剑矩阵 140/1，失败为旧“4 条目”断言，实际 5；全部失败保留。
- 旧图六 NPC + 魔法压力段：High 109.548 FPS / p99 10.891 ms；Epic 73.997 / 15.882。接电、解锁、1080p、100% 比例、采样无录像。不是 B5 王都性能；新城 30 分钟 3 km 探索 NOT_RUN。
- C-OS BLOCKED，真实桌面键鼠工具未暴露。全局墙柱淡化和 B4 可见率验收未完成，旧咖啡馆片段只作缺口展示。

## 证据与边界

TEST_REPORT.md 为最终结果；RECORDING.md 为媒体规格；review/index.html 只放结论、图和录像入口。全部代码/工具差异在 appendix/SOURCE_CHANGES.patch，资产和加密密钥不公开。构建及原生加密边界 PASS，源代码/配置/资产在完整回归前冻结。

17:03 原“王都”截图实际载入旧图，FAIL_MAP_MISMATCH，原记录保留。17:19 仅修正隔离捕获配置，确认同一最终包中的真实白模；不替换失败运行或重跑完整回归。四段飞行使用各自独立会话的正常体力，不能当连续探索。编辑器五次星弓预检、导入/编译/画面失败另存 BOW_APPROACHES.md 和 FAILURE_LEDGER.json。

ASSETS.md 按最新个人档确认和实际结果更新；包内构建时通知逐字节保存在 appendix/ASSETS_AT_BUILD.md，不修改已经验证的归档。公开白名单排除完整游戏、Selestia 源件、全部购买原件/派生资产、私人 BGM、独立付费音效、存档、密钥及逐帧序列。

## 待用户

1. 添加已购 City Pack / Highlands Castle 到 VFXStaging；两个个人档许可已确认，无须重复确认。
2. 整体瞄准上限 6 度已获用户允许；当前冻结 EXE 仍为 4 度，待修包/回归授权落实，不标成已应用。
3. 是否授权继续修星弓和测试夹具、重打包，并追加一次完整回归；本轮唯一一次已执行，未静默追加。
4. 按 MANUAL_RETEST.md 试玩星弓、B 关闭按钮及 P 预览往返。艺术验收仍 PENDING。
