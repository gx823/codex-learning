# R3 最终测试报告（2026-10-15）

总体：FAIL / 部分BLOCKED或NOT_RUN。独立包与媒体已交付准备完成，不等于验收通过。所有此前批次、写入失败、测试失败及中断原样保留在 [测试历史](appendix/TEST_REPORT_HISTORY_20261015.md)，没有以新运行覆盖旧FAIL。

| 批次 | 实际结果 | 证据 |
|---|---|---|
| 第1项存档回归 | M3_ANing篮筐阻挡修复，点位/校验不改；编辑器主线120/0、往返12/0 | DIAGNOSIS.md |
| 第2/3项补完 | 各2/2次；每段保存后具名NPC及任务存读45/45；家具/楼型等缺口保留 | ASSETS.md、LAYOUT.md |
| 第4/5项审计 | B2未清零，B6缺24机位；C6两次修复后仍失败 | appendix/ACCEPTANCE_FINAL.md |
| 完整05 | 317项/0FAIL，记录保留；不能替代07 | appendix/TEST_REPORT_HISTORY_20261015.md |
| 完整06 | 317项/1FAIL，拳击夹具原因；修正后才跑07 | 同上 |
| 完整07 | 319项/2枪FAIL；同次系统31/31、音效27/27；枪历史FLAKY | appendix/FINAL_EVIDENCE_R3.json |
| 07 story | 120项/0FAIL；PASS | appendix/FINAL_EVIDENCE_R3.json |
| 07 travel_capital | 12项/0FAIL；PASS | appendix/FINAL_EVIDENCE_R3.json |
| 07 travel_old | 18项/0FAIL；PASS | appendix/FINAL_EVIDENCE_R3.json |
| 07 old_save_import | 15项/0FAIL；PASS | appendix/FINAL_EVIDENCE_R3.json |
| 07 capital_r2_import | 23项/0FAIL；PASS | appendix/FINAL_EVIDENCE_R3.json |
| 07 ride | 20项/0FAIL；PASS | appendix/FINAL_EVIDENCE_R3.json |
| 07 c1 | 116项/4FAIL；FAIL_OR_INCOMPLETE | appendix/FINAL_EVIDENCE_R3.json |
| 07 bow20m | 50项/0FAIL；PASS | appendix/FINAL_EVIDENCE_R3.json |
| 07 old_a6 | 189项/0FAIL；PASS | appendix/FINAL_EVIDENCE_R3.json |
| 07 old_sword | 140项/0FAIL；PASS | appendix/FINAL_EVIDENCE_R3.json |
| 07菜单画面 | PASS，仅功能；High60用户配置不变 | graphics_20261014_172824_592 |
| 07 N | 五点High/Epic FPS/p99通过；Epic整次启动7557MiB FAIL；旧港压力下降4.815%/6.632% | appendix/ACCEPTANCE_FINAL.md |
| 07 D2/C6 14组 | D2 76单元13PASS/63FAIL；C6仍失败/未决；最后一组两次前台中断原记录保留 | appendix/FINAL_EVIDENCE_R3.json |
| 07 30分钟探索两次 | 19/4与17/2，均在计时前城堡导航partial；30分钟NOT_RUN | appendix/FINAL_EVIDENCE_R3.json |
| 07照片5组 | 固定Epic/High各115/0、室内/地标/目录各11/0；147照片。艺术不随夹具PASS | final/photos07_20261015/manifest.json |
| 07主片源 | 69/4FAIL保留；原运河partial/走距0，另有效点补拍11/0；不是清零 | appendix/MEDIA_QA_R3.md |
| 07飞行4首轮 | WAITING_HOST_WINDOW，native8/0且失焦0；只作诊断，恢复片另存 | appendix/TEST_REPORT_HISTORY_20261015.md |
| 07四交付片 | 时长/音频/解码/原帧PTS通过；FPS全FAIL；主片/室内/镜头像素FAIL，飞行像素无失败 | appendix/MEDIA_QA_R3.md |
| 汇总工具修正 | 撤销低于0.5s的B4误报；主片/镜头几何漏计改FAIL；旧回执保留 | appendix/MEDIA_QA_DATA_R3.json |
| 未执行 | C-OS真实拉弓10次；30分钟探索；旧片逐问题精确配对；全片人工听审 | appendix/ACCEPTANCE_FINAL.md |

美术与星弓观感仍PENDING。中间包/最终包、媒体路径和SHA见[交付清单](DELIVERY.md)。发布完成与否只以实际GitHub回执为准。

2026-10-15 发布核验：审阅内容提交ff74711c47f8ecce25925e70eeddf3e9c7343d51，Release共6份附件远端大小与SHA-256全部一致；仅R2发布分支、无force、无main。首轮GitHub目录树POST422原回执保留，同一请求重试201且摘要完全一致后复用对象继续；原因未确证。验收状态不变。ZIP为上传前快照，最新发布记录见appendix/PUBLICATION.json。
