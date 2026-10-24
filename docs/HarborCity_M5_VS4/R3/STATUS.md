EXE（本轮最终唯一 Shipping 候选）：E:\GameDev\Builds\HarborCity\M5_VS4\Candidate_20261025_040814_431_d703375a\Archive\Windows\HarborCity\Binaries\Win64\HarborCity-Win64-Shipping.exe

2026-10-24 清穿模第5版：DELIVERED_WITH_REMAINING。更新时间：2026-10-25T05:25:34.989920+08:00。

最终 Shipping 误触发验收：52 组各静站至少10秒、45 个不同站位，并实际往返；误送回 0 次。新增齐膝碰撞 914 件。原始导航候选 416 组，其中 68 组全部原始种子已连通；未修复候选 348 组，逐组记录保留。

最终第三人称镜头审计 187872 帧：胶囊内 0 帧、近头且人物仍显示 0 帧。整个人物同时隐藏/恢复，植物忽略镜头；原树顶保留360度回归。515个巡查站点已复查，2个树冠站纠正，另23个被挡/水下站点保留待修。

剩余重点：扩大碰撞实走 8 条失败断言（具体对象见附录），无法建立有效接近路线者为 NOT_RUN；部分房间完整任务路径、导航候选、墙脚灰色尖石、半段回墙接缝、运河悬根树、185件/91栏杆及685大件尚未全部视觉验收。没有把未通过项改记为通过。


最终15项原生验收：

| 项目 | 结果 | 检查数 | 失败数 |
|---|---|---:|---:|
| story | PASS | 122 | 0 |
| travel | PASS | 13 | 0 |
| oldtravel | PASS | 19 | 0 |
| ride | PASS | 21 | 0 |
| named | PASS | 46 | 0 |
| bow | PASS | 51 | 0 |
| a6 | PASS | 190 | 0 |
| follow | PASS | 21 | 0 |
| alleys | PASS | 73 | 0 |
| distance | PASS | 17 | 0 |
| high | PASS | 22 | 0 |
| safety | PASS | 70 | 0 |
| close | PASS | 95 | 0 |
| physics | FAIL_OR_INCOMPLETE | 534 | 8 |
| medium | PASS | 45 | 0 |

真受困专项包括水下3次、P菜单3次、城堡/运河脱困、室内门口内侧恢复及地板下恢复；正常台面/闲置/对话不作为无路回收理由。Medium 原生测试比较游戏 QualityPreset=1、11组均1、30帧上限。

按区保存均有存前备份、自动检查、保存、重载和具名NPC存读档；393冻结点保持。预览包和最终包独立，旧包未覆盖。墙接头已用重叠闭合并统一顶线，24/24射线通过；无效第二次下沉已撤回，灰色墙脚未宣称解决。平板车已沿路缘方向重新摆放，最终照片供复核。

按区/类剩余清单（自动检查通过不等于人工视觉通过）：

- canal:CLEAR_AUTOMATED：1 件。
- canal:REMAINING：18 件。
- castle:CLEAR_AUTOMATED：4 件。
- castle:REMAINING：53 件。
- central:REMAINING：3 件。
- gate:CLEAR_AUTOMATED：2 件。
- gate:REMAINING：5 件。
- interior:CLEAR_AUTOMATED：12 件。
- interior:REMAINING：52 件。
- market:CLEAR_AUTOMATED：2 件。
- market:REMAINING：9 件。
- port:CLEAR_AUTOMATED：1 件。
- port:REMAINING：11 件。
- workshop:CLEAR_AUTOMATED：4 件。
- workshop:REMAINING：8 件。
- 91段栏杆中自动检查无问题 3 段，尚未完成全量视觉验收。
- 大件优先对象 /Game/HarborCity/M5VS4/Capital/R2_eae16c3bde68/L_CapitalR2.L_CapitalR2:PersistentLevel.HCM2InstancedProp_73.Instances#instance=47（market）：REMAINING_NPC_PRIORITY。
- 大件优先对象 /Game/HarborCity/M5VS4/Capital/R2_eae16c3bde68/L_CapitalR2.L_CapitalR2:PersistentLevel.HCM2InstancedProp_73.Instances#instance=48（market）：REMAINING_NPC_PRIORITY。
- 大件优先对象 /Game/HarborCity/M5VS4/Capital/R2_eae16c3bde68/L_CapitalR2.L_CapitalR2:PersistentLevel.HCM2InstancedProp_73.Instances#instance=49（market）：REMAINING_NPC_PRIORITY。
- 大件优先对象 /Game/HarborCity/M5VS4/Capital/R2_eae16c3bde68/L_CapitalR2.L_CapitalR2:PersistentLevel.HCM2InstancedProp_73.Instances#instance=50（market）：REMAINING_NPC_PRIORITY。
- 大件优先对象 /Game/HarborCity/M5VS4/Capital/R2_eae16c3bde68/L_CapitalR2.L_CapitalR2:PersistentLevel.HCM2InstancedProp_387.Instances#instance=133（workshop）：SAVED_MOVED_NPC_CLEAR。
- 大件优先对象 /Game/HarborCity/M5VS4/Capital/R2_eae16c3bde68/L_CapitalR2.L_CapitalR2:PersistentLevel.HCM2InstancedProp_387.Instances#instance=145（market）：SAVED_MOVED_NPC_CLEAR。
- 大件优先对象 /Game/HarborCity/M5VS4/Capital/R2_eae16c3bde68/L_CapitalR2.L_CapitalR2:PersistentLevel.HCM2InstancedProp_387.Instances#instance=163（port）：SAVED_MOVED_NPC_CLEAR。
- 大件优先对象 /Game/HarborCity/M5VS4/Capital/R2_eae16c3bde68/L_CapitalR2.L_CapitalR2:PersistentLevel.HCM2InstancedProp_388.Instances#instance=129（workshop）：SAVED_MOVED_NPC_CLEAR。
- 大件优先对象 /Game/HarborCity/M5VS4/Capital/R2_eae16c3bde68/L_CapitalR2.L_CapitalR2:PersistentLevel.HCM2InstancedProp_388.Instances#instance=141（market）：SAVED_MOVED_NPC_CLEAR。
- 大件优先对象 /Game/HarborCity/M5VS4/Capital/R2_eae16c3bde68/L_CapitalR2.L_CapitalR2:PersistentLevel.HCM2InstancedProp_388.Instances#instance=150（central）：SAVED_MOVED_NPC_CLEAR。
- 大件优先对象 /Game/HarborCity/M5VS4/Capital/R2_eae16c3bde68/L_CapitalR2.L_CapitalR2:PersistentLevel.HCM2InstancedProp_388.Instances#instance=159（port）：SAVED_MOVED_NPC_CLEAR。
- 大件优先对象 /Game/HarborCity/M5VS4/Capital/R2_eae16c3bde68/L_CapitalR2.L_CapitalR2:PersistentLevel.HCM2InstancedProp_389.Instances#instance=153（port）：SAVED_MOVED_NPC_CLEAR。
- 673个落地/高处大件：分区视觉抽查未完成；完整逐件清单保存在 bulk685_final_audit.json。
- 地面巡查9组和港口缺站：最终补拍结果单列；①⑥⑲同机位和③推车专门对比图尚未完成。

详细证据：appendix/clipping4_20261024/RECOVERY_FINAL_REVIEW.json、CAMERA_FINAL_AUDIT.json、runtime_final_consolidated.json、COLLISION_FINAL_REMAINING.json、NAVIGATION_FINAL_REVIEW.json、VISUAL_ACCEPTANCE_FINAL.json。

原文顺序：REQUEST_CLIPPING4_20261024.md → REQUEST_CLIPPING4_CAMERA_20261024.md（最后一条优先）。不删资产、不缩至90%以下；暂停P、Esc停止；只保留最终JPG和少量关键PNG，不提交录像帧。发布仅限指定progress图片及STATUS。

最终补充核对：

- 九类实际阻挡样本：{'BARREL': 6, 'BENCH': 6, 'CART': 6, 'CRATE': 4, 'HAY': 6, 'PLANTER': 5, 'SACK': 5, 'STALL': 36, 'TABLE': 4}；每类至少3件成立。共 78 件实际阻挡通过，4件失败（TABLE37、TABLE41、CRATE67、STALL69），6 件接近路线无效为 NOT_RUN。
- 地面补拍30张、216条原生检查通过，港口 port_wall_loop_fwd_006 已补；中央北桥正反两组画面仍过暗，保留视觉待修。最终拼图在 appendix/clipping4_20261024/ground_retakes_final_contact.jpg。
- 性能环境：i7-14650HX、RTX 4060 Laptop GPU、1920×1080，编辑器关闭、前台独立运行。High四处共8段采样条件有效且目标通过，平均帧率范围 59.99–60.28；详细帧时间与显存见原生采样。Medium原生45项通过、30帧上限。
- 七区39件摆放完成保存/重载/具名NPC验证；原185件中26件自动检查无问题、159件仍有问题，不代表26件已全部视觉验收。部分室内机位较俯视或局部过亮，平板车贴边间距仍供复核。

交付图（正常场景光照；墙、平板车为保存地图的GameView，其余为最终Shipping）：

- [HAY：实际移动后被对应碰撞阻挡](progress/clipping4_20261024_collision_hay.jpg)
- [BARREL：实际移动后被对应碰撞阻挡](progress/clipping4_20261024_collision_barrel.jpg)
- [CRATE：实际移动后被对应碰撞阻挡](progress/clipping4_20261024_collision_crate.jpg)
- [SACK：实际移动后被对应碰撞阻挡](progress/clipping4_20261024_collision_sack.jpg)
- [PLANTER：实际移动后被对应碰撞阻挡](progress/clipping4_20261024_collision_planter.jpg)
- [STALL：实际移动后被对应碰撞阻挡](progress/clipping4_20261024_collision_stall.jpg)
- [CART：实际移动后被对应碰撞阻挡](progress/clipping4_20261024_collision_cart.jpg)
- [BENCH：实际移动后被对应碰撞阻挡](progress/clipping4_20261024_collision_bench.jpg)
- [TABLE：实际移动后被对应碰撞阻挡](progress/clipping4_20261024_collision_table.jpg)
- [公会两凳间柜台：静站10秒，未送回](progress/clipping4_20261024_guild_counter_10s.jpg)
- [真实落至地板下后恢复到本室门内侧](progress/clipping4_20261024_recovery_inner_door.jpg)
- [市场同路段3米视距；地面站位](progress/clipping4_20261024_market_fwd.jpg)
- [市场同路段3米视距；地面站位](progress/clipping4_20261024_market_rev.jpg)
- [20段室内实走的10秒停留画面；每格原生测试日志单独对应](progress/clipping4_20261024_rooms20.jpg)
- [⑪正面接缝闭合、墙顶连续；灰色墙脚尖石仍待修。保存地图正常光照GameView。](progress/clipping4_20261024_wall_joint.jpg)
- [反馈平板车沿路缘方向移动后的正常光照GameView；贴边间距仍供用户复核。](progress/clipping4_20261024_flat_cart.jpg)

临时帧清理：删除 2303 张测试临时帧，另清理 13 张工作联系图；保留16张交付JPG、地面最终拼图和8张原生关键PNG。最终Candidate录像帧检查为0；所有数值和失败日志保留。桌面测试进程已结束。

进度图片与本STATUS发布至 [progress分支](https://github.com/gx823/codex-learning/tree/codex/harborcity-m5-vs3-r2-gate-20260927/docs/HarborCity_M5_VS4/R3/progress)，本轮不推送源码或资产。
