# R3 省 token 批次记录

旧记录原样保留于 appendix/archive_20261011/TEST_REPORT.md。PASS 仅指注明的证据层，不代表用户美术验收。

| 日期/批次 | 做了什么 | 结果/剩余问题 | 证据/图 |
|---|---|---|---|
| 2026-10-11 恢复 | 原文保存；三份文档按字节归档；无残留UE/活跃锁；旧暂停标记归档 | PASS；B155重载/实走 NOT_RUN | appendix/archive_20261011/archive_receipt.json |
| 2026-10-11 B155恢复 | 新编辑器重载两地板、原门及两代理导航查询；零保存 | PASS（持久化）；实走 NOT_RUN | editor/20261011_144611_213_I_b155_two_floors_fresh_economy01/author_result.json |
| 2026-10-11 色调启动01 | 色调源码编译通过；新王都首次-game运行 | BLOCKED_INITIAL_FOCUS；未进测试，进程正常退出、无崩溃/材质回退；不算视觉PASS | runs/20261011_145322_740_r3palette/exit.json |
| 2026-10-11 色调近景01 | 两图午后/夜间/公会8图；原生56项检查通过，曝光未变 | 新王都持久运行PASS；旧港町室内取景FAIL（窗帘遮挡），肤色不可判；公会D.2待测 | appendix/economy_palette/after_both_maps_montage.jpg |
| 2026-10-11 公会色调白天诊断 | High真实进出/环看，MP4全帧统计，临时1071帧已清理 | FAIL：29.04FPS<29.5，录制时编译4帧/5个检查失败；室内156/410帧亮度不达标，旧FAIL保留 | appendix/economy_palette/guild_day.mp4；day_audit/compact_summary.json |
| 2026-10-11 亮度工具修正01 | 拼图发现实走录像仍有HUD；首版统计未排除HUD，已恢复既有1080p固定HUD遮罩 | 首版156/410数值作废但原报告保留；不重录、不改场景；将按同一MP4重算 | appendix/economy_palette/day_audit（旧）；day_audit02（重算） |
| 2026-10-11 色调批实机检查 | 两图近景；公会High日夜实走/全帧重算；曝光冻结 | 近景原生检查PASS；D.2日153/410、夜158/411 FAIL；29.04/28.96FPS为诊断；Epic及预览包冒烟NOT_RUN | appendix/economy_palette/；progress/2026-10-11/PALETTE_* |
| 2026-10-11 色调预览包 | 构建/Cook/Archive全部结束，独立Shipping产物SHA核对一致 | PASS（打包）；首次启动工具因路径斜杠比较误报FAIL，已规范化路径，未重打包；冒烟进行中 | builds/20261011_151728_918_36359823_package/package.json；runs/20261011_153939_571_m5_story/exit.json |
| 2026-10-11 色调预览冒烟 | 独立Shipping主线四章/保存读取、星弓20m、两图往返 | PASS：主线120、星弓50、往返12；未跑完整回归，D.2 FAIL继续保留 | appendix/economy_palette/preview_smoke_summary.json；progress/2026-10-11/PALETTE_* |
| 2026-10-11 发布/地面启动 | 仅进度JPG与STATUS已本地提交；首次推送连接重置；地面启动前枚举参数被拒绝并改为真实Save选项 | 推送重试中；首次启动零UE、零保存；地面正式批已启动 | appendix/economy_palette/；publication提交 5f29d28248b6 |

| 10-11 城堡固定空地草花段 | APPLIED_SAVED_PENDING_RUNTIME：82实例/6组，save1，无最终材质错误；下一步fresh与实走 | editor/20261011_155023_434_H_ground_cover_economy_apply01 |
| 10-11 色调进度推送 | FAIL：本地提交5f29d28已完成，GitHub连接重置/不可达，未上传确认 | appendix/economy_palette |
| 10-11 草花fresh | 数值PASS；拍照生成临时相机后工具BLOCKED_DIRTY；关闭不保存，284文件哈希一致，实走NOT_RUN | editor/20261011_155504_408_H_ground_cover_economy_fresh01/discard_recovery.json |
| 10-11 固定草地规划 | 两次候选检查未写入：部分点本已是草地或落在保护缓冲；排除空补丁，10处有效地面进入apply | private ground_paint_economy/request01.json |
| 10-11 城堡10处草地过渡 | APPLIED_SAVED_PENDING_RUNTIME：只改地面遮罩纹理；铺装通道/范围外像素不变；截图API触发非致命viewport ensure，保留记录 | editor/20261011_160511_737_H_ground_paint_economy_apply01 |
| 10-11 城堡草地fresh/实走 | fresh纹理哈希/设置PASS、284文件无写；两条5m往返16检查 PASS；平台大面积浅色视觉缺口仍在 | appendix/economy_palette/garden_paint_comparison_montage.jpg；runs/20261011_160953_752_r3courtwalk |
| 10-11 色调公开进度 | PASS：GitHub远端提交5f29d28核对一致，仅8JPG+STATUS | appendix/economy_palette/publication.json |
| 10-11 前庭草地修正 | 仅遮罩纹理保存；SELF_CHECK前庭绿地明显增加；全训练场矩形/道路保持；新-game 8检查PASS、两图、无最终材质回退 | editor/20261011_161940_623_H_ground_paint_economy_repair02；runs\20261011_162225_465_r3groundsurface |
| 10-11 地面预览包/三组冒烟 | PASS：新独立包；主线/存读档120、星弓50、地图往返12检查通过；完整回归NOT_RUN | appendix/economy_palette/ground_preview_smoke_summary.json |
| 10-11 地面公开进度 | PASS：远端da62538核对一致，仅6JPG+STATUS | appendix/economy_palette/ground_publication.json |
| 10-11 小楼派生库首写 | FAIL：CameraActor Python getter不存在；第一栋编译/数值通过但0保存。Sky两次不能激活；按预授权普通taskkill，磁盘/备份284文件一致 | editor/20261011_164710_812_E_small_building_library_apply01/discard_recovery.json |
| 10-11 小楼派生库重试/fresh | 10Blueprint/737构件编译、实例化、fresh数值PASS；新增10文件、原284不变。SELF_CHECK房屋可见；预览机位超天幕警告，外形IoU/城区放置NOT_RUN | editor/20261011_165138_217_E_small_building_library_apply02；editor/20261011_165750_245_E_small_building_library_fresh01；appendix/economy_palette/small_library_*_montage.jpg |

| 港口批入口01 | FAIL，启动器漏传私有请求参数；0保存，进程正常退出，磁盘原样。已补参数 | editor/20261011_172801_064_E_port_batch_apply01 |
| 港口批检查02 | FAIL，工具错读原生顶点字段；0写入/保存，普通终止后294文件未变，锁归档 | editor/20261011_172922_512_E_port_batch_apply02 |
| 港口批03 | BLOCKED，回调中__file__失效；原生数据已导出，0写入/保存，294文件未变，三次启动已用完 | editor/20261011_173239_707_E_port_batch_repair03 |
| 城门楼排检查01 | FAIL_FIT，19栋提案中11栋有待处理接触，整排筛选后不足工具自设10栋门槛；0写入/保存，294文件未变 | editor/20261011_174449_286_E_gate_rows_apply01 |
| 城门段写入02 | FAIL_TOOL，4F重合构件被唯一匹配误报；几何筛选6栋通过，实例化首栋后0保存；修工具改多重集合一一配对，不改容差、不重做已存预制 | editor/20261011_174938_079_E_gate_rows_segment_apply02 |
| 城门新进程01 | BLOCKED_FOREGROUND，游戏已载入但未取得前台，2检查PASS/1初始焦点FAIL；未执行核对及路线，不代表建筑失败 | runs/20261011_180811_499_r3courtwalk |
| 城门3排保存03 | PASS_SCOPED：8栋→6栋、24道具，撤588实例，导航重建，保存1次，仅地图改动；13提案后置，地面原为铺装 | editor/20261011_180023_611_E_gate_rows_segment_repair03；appendix/economy_palette/gate_rows_before_after.jpg |
| 城门新进程02 | FAIL：16检查PASS/1返程终点FAIL（原目标偏离实测导航点）；导出工具遗漏原版子Actor的12构件，378构件/24道具已匹配；294磁盘文件不变 | runs/20261011_181421_654_r3courtwalk |
| 城门新进程03 | PASS：16 Actor、390构件、24道具数值/材质匹配；2路线往返17检查0FAIL；294文件不变。启动器退窗误报原回执保留，另附复核（仅暖机2帧未聚焦，实走场景均0） | runs/20261011_182150_035_r3courtwalk；wrapper_reassessment.json |
| 启动器退窗修正 | PASS：26焦点/初始化、39退窗检查；真实回执重放及2拒绝边界通过。只改暖机退窗判定，未重跑游戏 | tools/test_hc_vs4_r3_run_focus.ps1；tools/test_hc_vs4_r3_completed_exit.ps1 |
| 中央段保存01 | PASS_SCOPED：4原版转换、16道具，导航重建、地图保存1次；多样化进展为0，20提案后置；附加立面漏留随后单独补回 | editor/20261011_183023_237_E_central_rows_apply01 |
| 原版立面补回 | PASS：6组50窗户/藤蔓实例，源组件保留，地图保存1次；新编辑器按请求原始位姿/材质核对PASS，294文件不变；NoCollision不改导航。拼图屋顶遮挡，艺术USER_REVIEW | editor/20261011_184436_147_E_original_facade_restore_apply01；editor/20261011_184858_873_E_original_facade_fresh01；appendix/economy_palette/original_facade_restored.jpg |
| 中央新编辑器重载02 | PASS：4原版本体/16道具与所触及源组件数值及材质核对，294文件未改；多样化进展仍0，实际中央路线NOT_RUN | editor/20261011_190504_100_E_central_fresh02；appendix/economy_palette/central_originals_fresh.jpg |
| 工坊区两个完整段 | PASS_SCOPED：4旧楼→2派生楼、8道具，撤292实例，导航重建，地图保存1次；新编辑器重载通过，294文件未改；31提案后置（新接触深度待确认） | editor/20261011_190734_926_E_workshop_rows_apply01；editor/20261011_191405_617_E_workshop_fresh02；appendix/economy_palette/workshop_rows_before_after.jpg |
| 工坊/中央/立面新进程01 | 数值核对PASS：25 Actor、387建筑构件、74道具/立面；实走15 PASS/2 FAIL：B575往返通过，B491目标沿用楼体Z导致短地面射线未命中。原失败保留 | runs/20261011_191615_743_r3courtwalk |
| 工坊实走02 | FAIL_TOOL：7 PASS/1起点落地FAIL；全点Z=0不适用于起点短射线。未进入实走，资产数值核对仍25 Actor/387构件/74实例PASS；下次只改B491行走目标的Z模式 | runs/20261011_192150_926_r3courtwalk |
| 工坊实走03 | BLOCKED B491：15 PASS/2 FAIL；室外长射线找到地面Z605.018，但导航投影失败。两次工具修正后后置，不再重试；B575往返PASS；25 Actor/387构件/74实例核对PASS | runs/20261011_192501_012_r3courtwalk |
| 市场区完整段 | PASS_SCOPED：13旧楼→7栋（6派生+1原版）、32道具/附加立面，撤951实例、导航重建、地图保存1次；fresh核对通过，40提案后置。两条原生导航往返17 PASS/0 FAIL；19 Actor/360构件/32实例匹配。裸土过渡随后修正 | editor/20261011_192806_518_E_market_rows_apply01；editor/20261011_193509_724_E_market_fresh02；runs/20261011_193806_325_r3courtwalk；appendix/economy_palette/market_rows_before_after.jpg |
| 市场地面修正03 | PASS_SCOPED：13旧楼地块周边9684像素转草地，仅遮罩资产保存1次；道路R、393冻结锚点8m范围、范围外与高度不改，地图字节未改。拼图仍有裸土保留，不称清零；纹理fresh并入运河首开 | editor/20261011_194407_988_E_market_ground_repair03；appendix/economy_palette/market_ground_before_after.jpg |
| 运河重载02 | FAIL_TOOL：保存计划路径传错，0保存/0脏资产；预授权普通关闭、不保存，前后294文件哈希一致，锁已归档。启动器新增路径存在性前置检查 | editor/20261011_195354_850_E_canal_fresh02 |
| 运河完整段及核验03 | PASS_SCOPED：24旧楼→13栋（11派生/2原版）、68实例，撤1865实例、导航重建、地图保存1次；fresh核对通过，26提案后置；2往返路线17 PASS/0 FAIL。市场遮罩新编辑器核对PASS | editor/20261011_194743_103_E_canal_rows_apply01；editor/20261011_195648_198_E_canal_fresh_repair03；runs/20261011_200146_390_r3courtwalk；appendix/economy_palette/canal_rows_before_after.jpg |
| 城堡拱门修正03 | BLOCKED本批：三旧楼地候选仍与另一墙饰有新接触；四角高度差各0.2662cm，仅落地通过。0写入/0保存/0脏资产，预授权关闭后294文件不变，锁归档；不再开本区编辑器 | editor/20261011_202339_432_E_castle_arch_repair03 |

| 城堡换楼段 | PASS 保存11→6派生+24道具，fresh核对；17实走PASS，运行快照16 Actor/549构件/24实例一致；28候选延后，未达全区目标 | editor/20261011_200805_860_E_castle_rows_arch_apply01；editor/20261011_201220_562_E_castle_fresh02；runs/20261011_202854_814_r3courtwalk；appendix/economy_palette/castle_rows_before_after.jpg |
| 室内批：原八室路线 | PASS 144/144；B155实际E开关门、6秒/70cm进出与F禁飞14项PASS，未改路线或门槛 | runs/20261011_203540_412_r3interioraccess8 |
| 室内批：B030木工坊底层 | PASS 20/20，原E/6秒70cm进出/F禁飞路线；上层无原楼梯仍NOT_RUN | runs/20261011_204028_154_r3interioraccessb030 |
| 室内批王座01 | FAIL 未写入：既定支撑面146/7057实际被地毯6585命中；室内6306简单/复杂碰撞高差12.13cm；0保存、dirty[]、294文件关闭前后未变，锁已归档 | editor/20261011_204221_916_I_remaining_throne_apply01 |
| 室内王座修补02 | FAIL 工具把静态组件当实例读取，写入前0保存；已修调用参数，294文件未变并归档锁 | editor/20261011_204708_034_I_remaining_throne_repair02 |
| 室内王座修补03 | BLOCKED 第三次仍未匹配原路径支撑面；两地毯临时碰撞改动已放弃，0保存/294文件关闭前后未变；本批不再诊断 | editor/20261011_204933_369_I_remaining_throne_repair03；appendix/economy_palette/throne_entry_before.jpg |
| 王座运行测试工具 | 初次FAIL仅旧整文件比较；按本次函数范围修工具后Python12/12、PowerShell24/24 PASS；新增专用测试分支尚未实走 | 私有runtime_glue01/economy_tests01.log、economy_tests02.log；economy_backup01 |
| 换楼预览主线启动01 | FAIL 启动模式误配m5_vs3_r3_b而非m5_story，等待焦点超时；不计为主线冒烟，已关闭自有测试进程 | runs/20261011_210529_445_m5_story |
| 星弓等待辅助工具 | FAIL 焦点日志等待用于不输出该标记的r2bow，超时；游戏自身已COMPLETE 50PASS，未重跑、未改结果 | runs/20261011_211116_046_r2bow |
| 换楼施工预览包与三组冒烟 | PASS 独立Shipping打包/主线120/星弓50/地图往返12；完整回归NOT_RUN；换楼目标及室内缺口仍未完成 | builds/20261011_205503_721_5656006b_package；appendix/economy_palette/district_preview_smoke_summary.json |
| 施工图发布01 | FAIL 首次加入截图遇到图片忽略规则，现按明确7文件白名单加入；远程首次DNS无法解析，未发布；无网络设置更改 | appendix/economy_palette/district_publish_receipt.json |
| 施工图发布02 | BLOCKED 本地提交304d65c只含6张JPG+STATUS；第二次GitHub443连接失败；未改网络设置，不声明远端成功 | appendix/economy_palette/district_publish_receipt.json |
| 城堡北坡18块地面 | PASS 保存与独立fresh：只改T_V2_GroundPaint，166092像素、道路R/393冻结缓冲/训练场不变；地图地形导航字节不变；拼图局部改善，裸土未全清，最新包不含此段 | editor/20261011_211633_098_H_castle_hillside18_apply01；editor/20261011_211908_994_H_castle_hillside18_fresh02；appendix/economy_palette/castle_hillside18_before_after.jpg |
| City烟雾批01 | FAIL 两个EnablePlugins参数只读取第一组，转换器未加载；0保存，临时副本放弃，294文件未变；已合并到单个启用列表 | editor/20261011_212708_859_J_city_smoke_apply01 |
| City烟囱烟雾72处 | PASS 原烟雾经官方转换器转换，1发射器最终编译ready；保存72/571=12.61%；fresh72绑定/数值姿态/无碰撞无导航影响通过，拼图可见烟柱。72条SetAutoActivate构造后警告为重复设值，默认自动激活保留，未重写已存内容；运河相关实走17/17 PASS，原楼与道具快照仍匹配 | runs/20261011_213854_828_r3courtwalk；editor/20261011_213209_427_J_city_smoke_repair02；editor/20261011_213442_513_J_city_smoke_fresh03；appendix/economy_palette/city_smoke72_before_after.jpg |

| E 地标实走初试 | FAIL_TOOL | runs/20261011_222110_458_r3courtwalk：5 PASS/1 FAIL；测试器要求两条路线，工具写成一条往返；尚未行走。保持原坐标和距离，拆为去程/回程重跑；场景不重写。 |
| E 地标部分保存 | PASS_SCOPED / REMAINDER_DEFERRED | editor/20261011_221414_240_E_landmarks_apply01：城堡观测塔1栋156实例，四角≤15cm、保守渲染间距、导航通过，1保存；7方案无合格空地未写。fresh221745零写入；runs/20261011_222549_300_r3courtwalk 13 PASS，34组156实例核对PASS。图 appendix/economy_palette/landmark_observatory_before_after.jpg；原尺寸 progress/2026-10-11/LANDMARK_01_HillObservatory_EDITOR.jpg。美术/C3/全城目标未通过。 |
| G 村落植被首试 | FAIL_ZERO_SAVE | editor/20261011_223223_858_G_outside_understorey_apply01：合格点少于自设40个目标，0保存，296文件哈希一致后关闭并归档锁。修订扩大到五屋南北缘；批量下限用用户规定10，落地/坡度/岸线/保护范围标准保持不变。 |
| G 村落植被修订 | FAIL_TOOL_ZERO_SAVE / DEFERRED | editor/20261011_223719_584_G_outside_understorey_repair02：552候选中11合格，541命中HCM2InstancedProp被Landscape-only筛选排除（未判定实际地面不合格）。改前图缺少fov失败，只有临时相机脏内存；0保存、296文件一致后关闭归档；本批暂缓，未新增植被。 |
| J 原版特效清点 | FAIL_TOOL_ZERO_SAVE | editor/20261011_224115_284_J_original_effects_apply01：3种原版效果/9处实例；仅按实例计批量误停，0保存、296文件一致后关闭归档。整组实际新增9 Niagara演员+6派生资产=15对象；位置/最终编译/导航检查不变。 |
| J 原版特效转换修订 | FAIL_ZERO_SAVE | editor/20261011_224603_893_J_original_effects_repair02：3 Niagara系统最终编译ready，9实例内存放置后原Cascade关闭状态未保留；0保存，6派生资产和地图脏内存全部丢弃，296文件哈希一致。改用原生可见性方法，并准备独立-game验证实际停用/启用。 |
| J 原版效果第三次 | SAVED / POST_SAVE_TOOL_SCOPE_FAIL | editor/20261011_225654_239_J_original_effects_repair03：9实例原Cascade全部visible=false/hidden=true/auto=false，3系统编译ready，1保存。文件检查额外发现私有Village MI_FX_smoke更新；二进制新增bOverride_UsageFlags/bHasStaticPermutationResource名称，无删除字符串。保留FAIL，302文件保存后哈希复核并关闭；不重做已保存改动。saved_plan_recovered.json记录6新资产、地图、私有材质及原备份；独立-game/视觉待检。 |
| J 保存后补充审计 | MATERIAL_REVIEW_PENDING | 文件时间审计另发现Content/Fantastic_City_Pack/materials/MI_FX_fire_city.uasset被引擎保存，不能只报告Capital变化。原始Vault/Staging未动；已保留旧Staging/当前材质对照。新增UE5.8/EditorOnlyData等序列化字段，旧源为UE4.26；未声称完整参数等价。详见private fx_economy/225654/material_recovery/comparison.json。 |
| J 独立-game首试 | FAIL_FOCUS_PREREQUISITE | runs/20261011_231326_319_r3courtwalk：2 PASS/1 FAIL；窗口30秒前台焦点超时，未发送测试输入、未运行效果检查、未拍后图。相同路线重跑，立即在FOCUS_READY后激活；不改资产或标准。 |
| J 独立-game重载/原版效果 | PASS_RUNTIME / VISUAL_NOT_ACCEPTED | runs/20261011_231801_783_r3courtwalk：18 PASS，R3_EFFECTS_FRESH 9/9，原Cascade停用、Niagara运行/编译/变换/无碰撞导航均通过。拼图 original_effects_before_fresh.jpg；普通火焰机位过暗，烟雾上部片状灰边需处理，未给视觉PASS。仅补拍一次炉火与烟雾局部。 |
| J 炉火补拍与材质回读 | PASS_EFFECT_BINDINGS / WALK_FAIL / SELF_CHECK | runs/20261011_232601_425_r3courtwalk：18 PASS，炉火近景可见；original_effects_close_review02.jpg为本批第2且最后拼图。runs/20261011_233631_572_r3courtwalk：17 PASS/1 FAIL；9效果原/新材质集合完全一致，无新增默认材质；运河B120返回终点129.63cm>70cm，之前两次18 PASS不覆盖本次FAIL。未改夹具坐标/6秒/70cm，不重复跑到绿。302 Capital文件及City火焰材质哈希未被-game改变。烟雾片状轮廓、两材质参数级等价均保留USER_REVIEW/NOT_RUN。 |

| 2026-10-12 施工预览234500 | PASS打包/归档/资产核对；仅施工包 | builds/20261011_234500_727_dd12aea3_package/package.json |
| 2026-10-12 预览主线退出 | FAIL_TOOL：旧循环时间早于刚落盘完成回执；原生120PASS、0失焦，未重复实走。调用点改为读完回执后Get-Date；39退出校验PASS | runs/20261012_000249_018_m5_story；私有preview_exit_clock_repair01备份 |
| 2026-10-12 预览冒烟 | 主线120/星弓20m 50/两图往返12原生PASS；主线启动器误判单列；完整回归NOT_RUN | appendix/economy_palette/late_preview_smoke_summary.json；progress/2026-10-11/FX_ORIGINAL_*、LANDMARK_01_* |
| 2026-10-12 公开进度 | PASS：GitHub连接器发布并读回e82b6c48；16 JPG+STATUS共17路径，远端树与本地1070e67完全一致；原git网络失败保留 | progress/2026-10-11；本地95da47e与远端不同提交、相同树，未重置 |
| 2026-10-12 Castle既有特效 | PASS运行70/70（雾38/粒子19/落叶7/风线6），位置/启用/可见/编译/材质无回退，相关实走18PASS；0地图写入 | runs/20261012_001647_181_r3courtwalk；runs/20261012_002106_386_r3courtwalk |
| 2026-10-12 Castle特效取景 | USER_REVIEW：6图中3图被墙/树冠遮挡，不能据此声明视觉PASS；保留原图与单张1400px拼图 | progress/2026-10-12/FX_DETAIL_CASTLE_*；appendix/economy_palette/castle_effects_runtime_review01.jpg |
| 2026-10-12 风车批apply01 | FAIL前置几何：原叶片包围盒不满足预设X轴；0保存，302文件字节一致，已关闭自有编辑器并归档锁；准备独立-game读取实际网格尺寸 | editor/20261012_003607_493_GJ_windmills_apply01 |
| 2026-10-12 风车repair02 | DEFERRED/0保存：Y轴与真实叶片旋转几何通过（半径875.505cm、离地40cm）；36候选中32无地形支撑/3接触/1道路冻结，未找到整段合格用地。302文件一致，13016关闭/锁归档，原风车仍静态；新代码未应用 | editor/20261012_005044_057_GJ_windmills_repair02；private:windmills_economy；源几何诊断runs/20261012_004343_898_r3courtwalk17PASS |
| 2026-10-12 水面apply01 | FAIL_TOOL/0保存：把材质传入仅接收StaticMesh的助手；302文件一致，自有17828已关闭/锁归档。修复参数类型及派生网格对象路径，使用已核对引擎FinishLoadingBeforeScreenshot等待最终编译 | editor/20261012_011145_750_J_water_sections_apply01 |
| 2026-10-12 水面repair02 | APPLIED_SAVED_PENDING_RUNTIME：海面+12段运河，337三角形；UV最大误差5.674e-8、切边8.799e-11cm；保存1次，仅地图+13私有派生网格，D.5/美术NOT_RUN | editor/20261012_011524_207_J_water_sections_repair02；private:water_sections_economy同名saved_plan.json及backup |
| 2026-10-12 水面fresh03 | FAIL：重载字段不等，尚未定位具体字段；0保存，315文件保留不变，自有42716已关闭/锁归档；本批3次编辑器用完，独立-game读取实际差异，不重做已存场景 | editor/20261012_011925_589_J_water_sections_fresh03 |

| 2026-10-12 水面真实加载差异/回退 | FAIL碰撞12/13（0→3），网格/有效材质父对象/数值变换13一致；原因已核对引擎：SetCollisionProfileName才关闭bUseDefaultCollision。已回退整段并核对原302文件，工具改用NoCollision profile但未重跑，本批3次用完 | private:water_sections_economy/runtime_compare01.json；rollback_20261012_013405/rollback.json保留失败地图+13网格 |

| 2026-10-12 公开Castle图 | 首次git add被既有ignore拦截、未提交；精确7路径白名单强制暂存修复；素材未上传 | publication_castle_runtime01.json |
| 2026-10-12 发布城堡运行图 | PASS：6 JPG+STATUS远端7路径与本地tree一致；首次断点写入超5KB被保护断言拦截，压缩JSON后保存 | commit 0ed1171；publication_castle_runtime01.json |
| 2026-10-12 L.2–L.5核对/C3准备 | L2/L3开发回归305/305已于10-09通过，31/31系统、27/27音效；最终包未测。C-OS无分开鼠标按抬接口，NOT_RUN。C3静态清单29楼/2010组件/18缺失副本，编译及语法PASS；两次补丁因重复目标被原子拒绝后修复，未部分写入 | runs/20261009_085006_989_r3fullfixture06；private:camera_economy/build02.log |
| 2026-10-12 C3换楼淡化apply01 | FAIL_TOOL：误用CollisionResponse，真实API为CollisionResponseType；0保存关闭，前后302文件一致，锁归档；修工具后再试一次 | editor/20261012_015642_983_C3_camera_binding_apply01 |
| 2026-10-12 C3换楼淡化repair02 | APPLIED：29楼/2010组件，21新私有材质，最终编译通过；几何/原绑定/玩家碰撞/导航字段未变，保存1次；fresh及运行未测 | editor/20261012_020116_989_C3_camera_binding_repair02；private:camera_economy/ |
| 2026-10-12 C3 fresh03 | PASS_SCOPED：2010组件重载字段一致，21私有材质存在，0保存；本批3次编辑器已用完 | editor/*C3_camera_binding_fresh03；appendix/economy_palette/camera_houses29_review01.jpg |
| 2026-10-12 C3门区实走01 | 16 PASS/1 FAIL：Row148返回74.201cm>70cm，原FAIL保留；首次工具将未遮挡时保留原材质误判为2010失败，已修为注册就绪/遮挡时换材质判定，未重做已保存资产 | runs/20261012_020835_290_r3courtwalk |
| 2026-10-12 C3窗户通道/读回02 | 480个无碰撞窗件未进Camera查询，运行时仅给此两类加Camera QueryOnly，其余通道全Ignore；2010/2010绑定读回PASS、480通道核对PASS、原生8/8、未覆盖查询0。启动器正常退出仍WAITING_HOST_WINDOW，FAIL_TOOL保留并BLOCKED，不重复修退出工具；完整C6未验 | runs/20261012_022028_914_r3courtwalk；private:camera_economy/runtime_binding_only02_comparison.json |
| 2026-10-12 L5真实鼠标操作 | NOT_RUN：桌面API无独立按下/抬起；人工步骤：B打开武器库→选星弓→鼠标点击关闭B→在原测试位置面向原靶，按住左键至满弓提示后松开，连续记录10次，不补发凑命中；这是人工待办，不是已测证据 | 无原生OS输入录像 |
| 2026-10-12 C3相关星弓20m | PASS：50/50，TP与FP各10/10命中；启动器正常结束，无录制、未跑完整回归 | runs/20261012_022438_755_r2bow |
| 2026-10-12 C3相关旧港町A6 | PASS：189/189、正常退出；本批第二张4格拼图已看，弓姿态/构图SELF_CHECK；仍不等于C6或美术验收 | runs/20261012_022808_071_vs4bowfinal；appendix/economy_palette/camera_a6_review02.jpg |
| 2026-10-12 C3补丁写入工具01 | FAIL_TOOL：同一补丁重复声明CameraManager目标，被原子拒绝；合并为同一目标后成功，无部分写入 | Source/HarborCity/M4R2/HCM4R2PlayerCameraManager.cpp |
| 2026-10-12 C3补丁写入工具02 | FAIL_TOOL：同一补丁重复声明Geometry头文件目标，被原子拒绝；合并目标后成功，无部分写入 | Source/HarborCity/M5VS4/HCM5VS4R3Geometry.h |
| 2026-10-12 C3地图往返/批次收束 | PASS：3图报告12/12、正常退出，切回硬/软不可查询0；323资产字节与保存计划一致，无UE/锁。旧7规则、Row148实走FAIL及退出工具FAIL保留；2张拼图已看，完整C6/新包未测 | runs/20261012_023505_970_vs4previewtravel；appendix/economy_palette/camera_batch_summary.json |
| 2026-10-12 C3进度发布 | PASS：3原尺寸JPG+STATUS共4路径，API远端提交/树与本地白名单核对一致；仅进度文件，无素材/派生/源码上传 | commit 5e0e4e9；appendix/economy_palette/publication_camera_batch01.json |

- 2026-10-12 港口 second_apply01：FAIL，局部落地/几何6栋通过；淡化材质未等编译完成即判失败，0次保存。已丢弃未保存改动，323文件哈希不变，锁已归档。
- 2026-10-12 港口 second_repair02：BLOCKED，几何6栋/导航重建通过；已有淡化实例16/16未就绪且无编译错误，180秒等待后FAIL。0保存，已丢弃；323文件哈希不变；同问题2次后不再启动港口。
- 2026-10-12 中央 second_apply01：FAIL_FIT，0修改0保存；树/街具/邻排接触使原9排方案未通过。按整排减少栋数后保留两排重试，旧失败不覆盖。
- 2026-10-12 中央 second_repair02：2新楼替换7旧楼、10797地面像素、导航已保存；11淡化材质最终编译通过。保存后范围工具FAIL保留；独立核对仅地图/地面/3个已有淡化父材质变化，重载与实走待验。
- 2026-10-12 中央 fresh03：PASS（2楼/129组件与shared实例重载；淡化标签/变体存在）；Row012拼图可见，Row015机位被邻楼屋顶遮挡USER_REVIEW。walk01：BLOCKED_INITIAL_FOCUS，2PASS/1中断FAIL，保留；改用外部UI激活后重试。
- 2026-10-12 中央walk02：PASS 17/17，外部UI激活、正常退出；fresh运行快照2楼129组件PASS，运行材质无回退。普查8例重复同位组件误报已修，486栋/53已识别几何签名；仍1栋未知和未归属模块，未做最终全城普查结论。
- 2026-10-12 工坊 second_apply01：已保存12楼替换21旧楼、地面17883像素和导航/淡化。保存后文件范围FAIL是把3张同名地面纹理都要求变化；现只核对选定City纹理，实际变化在白名单内。旧FAIL保留，重载待验。
- 2026-10-12 工坊 fresh02：12楼/942组件及淡化重载PASS；walk01夹具3条路线超过现有2条限制，5PASS/1夹具FAIL，未实走；修正为一条新街+原B491路线后再测。改后拼图发现旧烟囱烟需要随换楼重绑，合并工坊repair03处理，不冒称整区完成。

- 2026-10-12 工坊：142310存12楼/替21旧；144508独立游戏组件942核对PASS，新街往返PASS，B491导航/到达FAIL（本轮第1次实际复测）。143753余排落位FAIL、0保存/323文件不变；145042共墙工具修正后存2楼/替5旧，3处烟雾移至实有烟囱；实走进行中。新拼图WORKSHOP_SECOND_ROWS_EDITOR：Row130遮挡未作美术验收、Row153可见。
- 2026-10-12 工坊145042修整两排：独立游戏145400实走17/17 PASS；Row153编辑器对照已选，Row130遮挡图仅留私有证据。
- 2026-10-12 城门145752：落位FAIL、0保存、323文件哈希不变；150421整排缩减修整存2栋替6旧，地面/淡化/导航已存，fresh/实走未跑。工坊修整独立游戏2楼109组件核对PASS。
- 2026-10-12 测试工具：私有路线夹具由固定2条改为1–16条，以合并区内与遗留路线；坐标有限性、导航完整性、落地与70cm到达断言不变。150844 Editor构建PASS_COMPILE_ONLY。城门151103 fresh PASS（2楼及前段烟雾3处核对）；151433运行焦点超时FAIL/未输入，保留；改为阻塞等窗口后立即激活。B491新Landscape探点原目标XY地面605.018cm，导航投射仍无结果，非地面Z为0一处夹具问题即可解决。
- 2026-10-12 城门151749三路线22/22，本批2楼102组件game核对PASS；Row148本次通过但未确认旧返回漂移根因，保留FLAKY。发布图GATE_SECOND_ROWS_EDITOR.jpg为编辑器画面；工坊修整fresh3烟雾位置也已随城门加载核对。港口152124为实际父材质编译修复验证后的第3次author尝试，旧2FAIL均保留。
- 2026-10-12 港口152124：原有淡化父材质实际编译修复后，6新楼替12旧楼/地面/导航/镜头绑定保存成功，fresh及实走待做；保留此前2FAIL。城门progress本地f383804已提交，GitHub443连接超时未推送；未修改网络设置。
- 2026-10-12 港口fresh152526六楼555组件/镜头标签PASS；152825焦点超时FAIL、无测试输入保留；153217启动时阻塞等窗口即激活后17/17，555组件独立game核对PASS。港口拼图含城墙遮挡的Row094及尚未重做的运河黑水，相关收尾仍待做。
- 2026-10-12 原版19：153511落位FAIL/0保存、323文件不变；修正旋转/合并楼排共墙识别并按授权清理挡路装饰，155140保存B227原版L_Ext07，其他18仍未应用。新增2cm检查只使用包围盒分离距离作为严格上界，不能据包围盒重叠断言超标；此前FAIL不改写。B227待fresh/实走。
- 2026-10-12 B227 fresh160142 / -game160606：1楼109组件核对PASS，12/12实走PASS；原版缺额仍18。7条旧淡化规则的独立只读干跑：7个共用ISM均仍存在（窗、路缘、城堡顶/旗、烟囱），均原NoCollision/CameraIgnore。下一市场批修标签及专用相机查询；原行走碰撞仍忽略，结束游戏恢复原设置；C6全量未验。

- 2026-10-12 市场：162756_apply02保存19楼/替56；163657_fresh03核对1437组件和6烟雾；163931_game实走17/17、1437组件PASS。旧7条淡化unresolved=0，hard/soft不可查询=0，exact_restore=true；全量C6 NOT_RUN。拼图MARKET_SECOND_ROWS_EDITOR.jpg，裸土仍可见/USER_REVIEW。
- 工具：连续清理已授权挡路小道具；单条附加实走不再要求凑两条；旋转原版楼的临街轴偏移修正待下一批验证，既有FAIL保留。

- 2026-10-12 运河：164256_apply01保存18楼/替42；165120_fresh02重载PASS；165348_game核对1194组件、三路线22/22 PASS。B120去程22.210cm/返回21.962cm，旧129.63cm FAIL保留，FLAKY。4烟雾随新烟囱移动；11候选未解。CANAL_SECOND_ROWS_EDITOR.jpg/USER_REVIEW。

- 2026-10-12 城堡：165718_apply01保存6楼/替15；170336_fresh02核对373组件、2烟雾及普查；170750_game焦点FAIL/无输入；171038_game373组件PASS，实走15/17，B391去671.767cm/回247.950cm FAIL。CASTLE_SECOND_ROWS_EDITOR.jpg/USER_REVIEW。已识别城内393，原4x4=117/393、8x8=55/393；18原版缺额，分母UNKNOWN，不能标全城PASS。
- 2026-10-12 地标首轮172356_dry01：7候选均FAIL_FIT（阳台/墙饰、蜡烛、残留窗、港口地板等）；0保存，授权丢弃自有PID53076；327文件字节不变，锁归档。
- 2026-10-12 地标174139_apply03：落位3通过后，Blueprint组件计数FAIL_TOOL；0保存，授权丢弃自有PID36824；327文件字节不变，锁归档。

- 2026-10-12 地标174936_toolfix04保存CouncilTerrace/CraftsHall/GateWatchHall共3栋；175834_game从磁盘重载3蓝图与地图，533组件/三路线22项PASS；旧dry01全7失败和apply03组件计数FAIL_TOOL均保留。总4/8；LANDMARKS_FIRST_THREE_EDITOR.jpg/USER_REVIEW。

- 2026-10-12 地标第二段181713保存；-game182537重载211组件PASS，实走17/17 PASS；观测塔旧组件已补淡化绑定，非全量C6验收。拼图LANDMARKS_ARCHIVE_CANAL_EDITOR.jpg，USER_REVIEW。

- 2026-10-12 地标末段182853几何FAIL/0save；缩小市场厅并在港口原地块内沿街重排，183749 dry两栋PASS。184048因新增截图辅助函数循环导入FAIL_NO_RECEIPT，341文件核对未变；移入main修复，允许加开toolfix04，旧FAIL保留。

- 2026-10-12 地标末段184321保存2栋；-game184832重载256组件PASS、17/17实走PASS。8栋/七区完成，改后拼图LANDMARKS_MARKET_PORT_EDITOR.jpg；新地标艺术评审USER_REVIEW，非全量C6。

- 2026-10-12 林下185057材质不支持ISM，FAIL/0save，343文件未变；185412普通StaticMesh回退后保存288处/35组件；185935重载PASS。拼图OUTSIDE_UNDERSTOREY_EDITOR.jpg，USER_REVIEW。新7地标纳入普查后城内382，分母仍UNKNOWN。风车185935落位FAIL/0save，修正外圈地表整体包围盒误拦后190218重试。

- 2026-10-12 风车185935及190218两次FAIL/0save：121处render-contact拒绝、11道路/冻结拒绝，无新风车保存；各343文件未变。依两次失败规则BLOCKED，原静态风车不计转动PASS。

- 2026-10-12 田边/海岸190526保存27组件：3稻草人、2路标、6礁石；190901重载数值/碰撞/材质PASS。拼图OUTSIDE_FIELDS_COAST_EDITOR.jpg，USER_REVIEW；草木NoCollision，实际路线待第7项预览冒烟；风车BLOCKED保留。

- 2026-10-12 王座厅191239指定地板FAIL；191837楼梯简单碰撞坡面比渲染台阶高9.26cm，被旧2cm支持检查拦；192733地面通过、两Agent八路径完整，投影22–26cm被额外XY2cm门槛拦。三次均0save，343文件未变。按新请求接受有效厅内地面，原版地毯实例NoCollision；仅INSIDE simple允许20cm坡面误差，render仍2cm；导航保持总70cm、实际6s/70cm及原目标不变。允许额外tool修复启动一次04，尚未PASS。

- 2026-10-12 王座厅额外tool修复04：地面/导航门槛已通过，门口碰撞体查询使用的compose拒绝非均匀组件根变换，FAIL/0save；343文件未变并丢弃本次未存改动。本批达到3+1启动上限，BLOCKED；未假称已接通王座厅。

- 2026-10-12 Bar01 B008 saved193902/fresh194501 PASS；194918实际开门/入室15.84cm PASS，室内关门FAIL（终点距门约450cm，超过240cm）；旧FAIL保留，新增实际走回门边待测。
- 2026-10-12 Guild195205首试FAIL/0save：坐标筛选无候选；343文件不变，归档锁，修筛选后195444重试中。

- 2026-10-12 Guild195444保存1实体暖灯+1本书/1点光，195827重载PASS；两静帧亮度全达标（仅编辑器，非D.2巡览）；图GUILD_WARM_CABINETS_EDITOR.jpg。地窖门162.04cm/旧洞157.93cm未接，18间未达成；空墙补全未完成。

- 2026-10-12 Bar01 200043实测22/22 PASS：保留完整入室目标，追加真实步行回门后E关门/禁飞/出门通过；没有放宽240cm/6s70cm。旧194918FAIL保留。

- 2026-10-12 Water200337两段保存/200848重载PASS：13水面NoCollision、材质、UV、高度、面积一致，原Pond不变；WATER_13_SECTIONS_EDITOR.jpg；实际游戏冒烟并入第7项。

- 2026-10-12 Hill201150地面207019像素草地/塔脚铺装保存；201439保存44植被岩树（6树/3岩），201752新编辑器重载PASS，地形高度未改；HILLTOP_GROUND/PLANTING_EDITOR.jpg。

- 2026-10-12 Preview202033打包PASS；203319独立包主线冒烟119/120：四章/存读档PASS，单次射击计数0→2 FAIL（目标受伤断言通过）；未改包，待一次复测，旧FAIL保留。

- 2026-10-12 Preview203558星弓50/50 PASS；203750地图往返12/12 PASS；主线203900复测120/120 PASS，同包未改，203319单次射击FAIL原因未证实，标FLAKY不清除旧FAIL。

- 2026-10-12 第7项：独立包204110拍23景/相关实走17/17；204636旧港町30/30、4近景脸无遮挡；205012补3机位12/12、205426平台路12/12。王座厅改机位仍97.8%近黑→可见性FAIL/BLOCKED，Bar01偏暗；27JPG+拼图在progress/2026-10-12，未修饰照明掩盖失败。

| 2026-10-12 ART_FINISH 1 | 城内裸土2132072像素补草，1次保存+新编辑器重载 | PASS（编辑器持久化；高度/道路未改；艺术USER_REVIEW） | editor/20261012_220111_012_H_art_finish_city_apply01；220408 fresh；progress/2026-10-12/ART_FINISH_01_CITY_GROUND_EDITOR.jpg |
| 2026-10-12 ART_FINISH2 original18 apply01 | FAIL_TOOL: obsolete B272 instance rebind, 0 saves/clean; closed without save, all356 files unchanged | prior failures retained | editor/20261012_220701_958_E_art_finish_original18_apply01 |
| 2026-10-12 ART_FINISH2 original18 apply02 | FAIL: old parcels retain neighbour/wall contacts; stopped redundant offline alternatives, 0 saves/all356 bytes unchanged | tree contacts were tool false positives; revised vacant parcels next | editor/20261012_221127_966_E_art_finish_original18_apply02 |

| ART_FINISH2 originals | 新增2栋16x8_Ext09，保存/重载；首测BLOCKED_INITIAL_FOCUS（未实走），复测17项0FAIL，192组件运行时对比PASS | 原版仍缺16；艺术USER_REVIEW | runs/20261012_223529_529_r3courtwalk；progress/2026-10-12/ART_FINISH_02_ORIGINALS_EDITOR.jpg |

| ART_FINISH2 central | 1一对一换楼，保存/重载；实走3项/1FAIL | FAIL_RUNTIME；艺术USER_REVIEW | progress\2026-10-12\ART_FINISH_02_CENTRAL_EDITOR.jpg |

| ART_FINISH2 central focus repeat | 12项0FAIL，组件对比PASS；前次焦点FAIL保留 | PASS_SCOPED | runs\20261012_225322_662_r3courtwalk |

| ART_FINISH2 market | 1一对一换楼，保存/重载；实走0项/0FAIL | SAVED_RELOADED_PENDING_WALK；艺术USER_REVIEW | progress\2026-10-12\ART_FINISH_02_MARKET_EDITOR.jpg |

| ART_FINISH2 canal | 1一对一换楼，保存/重载；实走0项/0FAIL | SAVED_RELOADED_PENDING_WALK；艺术USER_REVIEW | progress\2026-10-12\ART_FINISH_02_CANAL_EDITOR.jpg |

| ART_FINISH2 workshop | 1一对一换楼，保存/重载；实走0项/0FAIL | SAVED_RELOADED_PENDING_WALK；艺术USER_REVIEW | progress\2026-10-12\ART_FINISH_02_WORKSHOP_EDITOR.jpg |

| ART_FINISH2 城堡首次 | 离线替代方案超时；0保存、资产逐字节未变；随后结束的检查结果仅用于下一次重新验证 | FAIL_TOOL_TIMEOUT | editor/20261012_233216_700_E_art_finish_castle_apply01 |
| ART_FINISH2 港口首次 | 3候选实际网格检查未通过；0保存、358文件未变 | FAIL_FIT | editor/20261012_230941_377_E_art_finish_port_apply01 |

| ART_FINISH2 castle_retry | 1一对一换楼，保存/重载；实走0项/0FAIL | SAVED_RELOADED_PENDING_WALK；艺术USER_REVIEW | progress\2026-10-12\ART_FINISH_02_CASTLE_RETRY_EDITOR.jpg |

| ART_FINISH2 gate | 全部候选未通过实际网格摆放检查；0保存，358文件逐字节未变 | BLOCKED_FIT | editor\20261012_235149_815_E_art_finish_gate_apply01 |

| ART_FINISH2 original_current | 6一对一换楼，保存/重载；实走0项/0FAIL | SAVED_RELOADED_PENDING_WALK；艺术USER_REVIEW | progress\2026-10-12\ART_FINISH_02_ORIGINAL_CURRENT_EDITOR.jpg |

| ART_FINISH2 空地复试工具 | 批次标签复用被保护规则阻止；0保存，368资产字节未变；改为独立新标签 | FAIL_TOOL，非物件失败 | editor/20261013_002123_078_E_art_finish_original_flat_retry_apply01 |

| ART_FINISH2 original_flat_retry | 6一对一换楼，保存/重载；实走0项/0FAIL | SAVED_RELOADED_PENDING_WALK；艺术USER_REVIEW | progress\2026-10-12\ART_FINISH_02_ORIGINAL_FLAT_RETRY_EDITOR.jpg |

| ART_FINISH2 roof_north | 27一对一换楼，保存/重载；实走0项/0FAIL | SAVED_RELOADED_PENDING_WALK；艺术USER_REVIEW | progress\2026-10-12\ART_FINISH_02_ROOF_NORTH_EDITOR.jpg |

| ART_FINISH2 roof_middle | 24一对一换楼，保存/重载；实走0项/0FAIL | SAVED_RELOADED_PENDING_WALK；艺术USER_REVIEW | progress\2026-10-12\ART_FINISH_02_ROOF_MIDDLE_EDITOR.jpg |

| ART_FINISH2 roof_south | 17一对一换楼，保存/重载；实走0项/0FAIL | SAVED_RELOADED_PENDING_WALK；艺术USER_REVIEW | progress\2026-10-12\ART_FINISH_02_ROOF_SOUTH_EDITOR.jpg |

| ART_FINISH2 original_footprints | 全部候选未通过实际网格摆放检查；0保存，368文件逐字节未变 | BLOCKED_FIT | editor\20261013_005807_725_E_art_finish_original_footprints_apply01 |

| ART_FINISH complete_BP 工具失败01 | 6栋网格检查通过；完整BP连带子对象销毁触发冻结检查，0保存，368文件逐字节未变，丢弃自身未保存修改；补充完整BP从属对象范围后重试 | FAIL_TOOL，旧记录保留 | editor/20261013_010145_191_E_art_finish_complete_bp_apply01 |

| ART_FINISH2 complete_bp | 6一对一换楼，保存/重载；实走0项/0FAIL | SAVED_RELOADED_PENDING_WALK；艺术USER_REVIEW | progress\2026-10-12\ART_FINISH_02_COMPLETE_BP_EDITOR.jpg |

| ART_FINISH2 完整BP / 缺项 | 第二次工具修正后6栋完整预制派生替换保存/重载；24个授权根及从属对象范围已核实。4x4=43/397=10.831%，8x8=41/397=10.327%；剩余L05x1、Arch16x8_01x1、Arch16x8_02x1分别楼体扩展件、坡面、围栏阻塞；26种各≥3未满足 | 配额数值PASS；实走NOT_RUN；3原版补位BLOCKED | editor/20261013_011057_406_E_art_finish_complete_bp_fresh01；原FAIL保留 |

| ART_FINISH2 original_clutter | 1一对一换楼，保存/重载；实走0项/0FAIL | SAVED_RELOADED_PENDING_WALK；艺术USER_REVIEW | progress\2026-10-12\ART_FINISH_02_ORIGINAL_CLUTTER_EDITOR.jpg |

| ART_FINISH facades apply | 实例20；保存1 | APPLIED_SAVED_PENDING_RUNTIME | editor\20261013_012036_609_ART_facades_apply01 |

| ART_FINISH facades fresh | 实例20；保存0 | DRY_RUN_READY | editor\20261013_012511_602_ART_facades_fresh01 |

- 2026-10-13 USER_PAUSED：用户要求暂停；runs/20261013_012951_045_r3courtwalk/exit.json = USER_ABORTED；实走未完成，无新增PASS。UE进程已退出；第3—7项NOT_RUN，断点已保存。

| ART_FINISH step2 runtime 20261013_100238 | 91actors/6547components readback PASS;87checks,4FAIL;14routes PASS;B391671.864cm,B491499.022cm/no-nav BLOCKED | resumed USER_ABORTED run; old FAIL retained |

| ART_FINISH facades_finish apply | 实例13；保存1 | APPLIED_SAVED_PENDING_RUNTIME | editor\20261013_101028_718_ART_facades_finish_apply01 |

| ART_FINISH facades_finish fresh | 实例13；保存0 | DRY_RUN_READY | editor\20261013_101508_296_ART_facades_finish_fresh01 |

| ART_FINISH streets apply | 实例315；保存1 | APPLIED_SAVED_PENDING_RUNTIME | editor\20261013_101957_318_ART_streets_apply01 |

| ART_FINISH streets fresh | 实例315；保存0 | DRY_RUN_READY | editor\20261013_102251_286_ART_streets_fresh01 |

| ART_FINISH street_fix apply | 实例28；保存1 | APPLIED_SAVED_PENDING_RUNTIME | editor\20261013_103342_372_ART_street_fix_apply01 |

| ART_FINISH street_fix fresh | 实例28；保存0 | DRY_RUN_READY | editor\20261013_103707_999_ART_street_fix_fresh01 |

| ART_FINISH step3 | 313 retained props/plants;30 corrected placements retired,28 added;save/fresh numeric PASS;12native paths preserved,2 old gaps | Market2stalls/no rows and castle garden1 BLOCKED after2 attempts; art USER_REVIEW | progress/2026-10-12/ART_FINISH_STREETS_FINAL_EDITOR_1/2.jpg |

| ART_FINISH interior baseline/apply | baseline2fixedPASS/15FAIL/1outwardview-invalid;132Movable, many hidden;94new fixture lights/61old disabled,60wall objects/16rooms;Cellar leaf removed from display/collision;Throne+Cellar native paths COMPLETE | SAVED, fresh/runtime/D2 pending | editor/20261013_105135_044_D_art_interiors_apply01 |

| Interior fresh01 | FAIL_TOOL:3 old switched lights restore visibility but keep saved intensity0;94new lights and numeric fields match;0save/clean disk | Own failed reader discarded; verifier now records zero-intensity visibility as nonblocking, old FAIL retained |

| ART_FINISH Throne+Cellar actual walk | PASS22/22;4Throne legs final16.311cm,3Cellar legs final14.573cm;native paths complete;floor samples 84 nonwalkable=0 | runs/20261013_110300_785_r3courtwalk |
| Interior relight first images | FAIL:overbright,17/18 fixed views fail;GuildPASS. Correcting only owned light intensity/shadows, no exposure edits | editor/20261013_105802_263_D_art_interiors_fresh02 |

- Art lighting tune: 110702 apply FAIL_TOOL (0 saves/no map hash change); corrected owner/component.modify; 111247 apply1save +111524 fresh numeric readback PASS. Intermediate111033 read-only cancelled unchanged,111203 stale-lock launcher0start retained. Final fixed D2 10/18PASS; THRONE/B008/B015/B027/B140/B155/B209/B214 FAIL (B214 view validity limitation). 48active lights<=3/room,60wall pictures; complete wall coverage NOT_MET.
- Guild112054 tour30/34native checksPASS;4FAIL compilation reappeared1frame. Full-frame brightness pending; no source receipt promoted.
- Guild full-tour D.2 BLOCKED: 51/247 indoor frames FAIL among746 encoded frames. Diagnostic35.918s/20.770fps;115.5ms initial audio gap and2.116s missing tail retained as FAIL. Source30/34 checks,1compilation-busy frame. Encoder diagnostic-only tolerance repaired once, acceptance not relaxed;746temporaryJPG removed after verifiedMP4. Evidence appendix/art_finish_20261012/guild_d2; tuning editor collages ART_FINISH_INTERIORS_TUNED_EDITOR_1/2.jpg.
- Step5:113229apply1save/113445freshnumericPASS;2complete mills,3understoreyinstancesremoved;nativecollision excludesLandscape/sky. 113637runtime15/15PASS (2rotors angle change over2s/fixedhub;out23.503cm/back26.026cm). Editor ART_FINISH_WINDMILLS_EDITOR_1.jpg;5sMP4 planned in preview.

| ART_FINISH outside apply | 实例4214；保存1 | APPLIED_SAVED_PENDING_RUNTIME | editor\20261013_114028_241_ART_outside_apply01 |

| ART_FINISH outside fresh | 实例4214；保存0 | DRY_RUN_READY | editor\20261013_114230_446_ART_outside_fresh01 |
- Step6:114028apply1save/114230freshPASS,4214objects;N267/W256/S197mixed woodland objects incl295trees,3223crops at1.25mrows,258fences/13hay. FIELD_9 sparse10crops/1fence NOT_MET, others planted;ground/contacts/freeze rejects preserved. Navigation rebuilt,noheightchange;runtime smoke in new preview pending. ART_FINISH_OUTSIDE_EDITOR_1.jpg. Windmill firstGitpush failed network, queued for next requestedprogress push.
- Preview package20261013_114539_417_d672f37c PASS:Editor/Game compile,Cook/Pak/Archive,cookedmap+archive verification. Independent runtime NOT_RUN at build completion; no final release.
- New preview story115508+115823 both25/26PASS, same Harbor checkpoint save validationFAIL (M3_ANing), laterchapters/F5F9NOT_RUN. Two real attempts:BLOCKED, no third retry/no slot validation bypass; old storyFLAKY preserved. Exact invalid NPC state cause unproven.
- Newpreview bow11591250/50PASS includingTP/FP20m. Travel120103capital-start never changedmap;nativeCOMPLETE3prerequisitechecks,owned diagnostic stopped andFAIL_PROCESS retained.120657oldstart reachedbothmaps butPreview memory snapshotFAIL(8/9aggregate),launcherWAITING_HOST_WINDOW. RoundtripBLOCKED aftertwo attempts, no acceptance.
- Previewinteriors120821:18screenshots complete,11/11harnesschecksPASS;2collages visually reviewed. CELLARnearblack VISUAL_FAIL despitewalkPASS;B214openforge/no closed interior view;wallcoverage/furniture remain incomplete. Images ART_PREVIEW_20261013_INTERIORS_COLLAGE_1/2.jpg;no art PASS inferred.
- Previewexteriors121217:15screenshots,11/11harnesschecksPASS;2collages reviewed. Finaltotal33=overview1+streets7+rooms17+throne+cellar+outside4+mills2. Throneactualnave/throne visible (stronghighlights),eastwallforestfield visible,southfield/woodland visible;Castle actualstreet. Artlimits retained,notfinalacceptance.
- Windmillvideo121541 native11/11PASS, bothrotors36.9628deg/fixedhub.150actualJPGframes encoded4.967824s VFR/1080p/H264+AAC,full decode+PTSchecksPASS;150tempJPG+WAVremoved,8keyframes retained;6-frame montage reviewed,2rotors visiblymove. Shipping compiles logging out, automatic game.log source binding NOT_RUN; exact run/fixture/native telemetry kept separately. No source receipt promoted,video is local preview,not final media acceptance.
- Endofround:published e9602d7 exact38files(37JPG+STATUS),earlier editorprogress preserved;MP4localonly. Finalcheckpoint<=5KiB,STATUS15lines,UEprocesses0/lockabsent,video tempframes0. StopafterStep7,listedBLOCKED remain;finalacceptance/delivery NOT_RUN.

| 2026-10-13 S1 diagnostic | FAIL reproduced (25 PASS/1 FAIL): M3_ANing capsule blocked by street batch 101957, basket HCM2InstancedProp_581 instance 3; validation unchanged. | runs/20261013_125319_196_m5_story/game.log |
| 2026-10-13 S1 scene correction | SAVED_PENDING_RUNTIME: removed only proven basket; navigation rebuilt; NPC/region transforms unchanged; backup verified. | editor/20261013_125631_379_S1_final_regression_apply01/author_result.json |
| 2026-10-13 S1 editor-game story | PASS 120/120 including four chapters and F5/F9; old FAIL retained. | runs/20261013_130018_656_m5_story |
| 2026-10-13 S1 travel focus | WAITING_HOST: initial SetForegroundWindow refused; no verified focused run; interrupted check retained, excluded from functional attempt count. | runs/20261013_130318_624_vs4previewtravel |
| 2026-10-13 S1 editor-game travel | PASS 12/12: Capital→OldTown→Capital, real in-memory save captured and restored. Prior host interruption retained. | runs/20261013_130552_089_vs4previewtravel |
| 2026-10-13 short NPC save helper | TOOL_FAIL: 45 PASS/2 count FAIL; actual save/load and actor validity PASS. R2_TrainingDummy incorrectly counted; changed iteration to production Experience.NPCs; pending build/retest. Not an art attempt. | runs/20261013_131224_906_r3namedsave |
| 2026-10-13 S2 attempt1 | SAVED_PENDING_RUNTIME:12 geometry/room-confirmed vegetation removals,23 furniture/letter props,23 light operations and5 fire emitters; NPC/region transforms unchanged. Native save smoke and19-view D2 pending. | editor/20261013_132935_413_S2_final_interiors_apply01 |
| 2026-10-13 S2 immediate named NPC save | PASS45/45 using production save roster (19 NPCs), actual save/readback and task validation. Earlier tool count error and host timeouts retained. | runs/20261013_133504_096_r3namedsave |

2026-10-13 S2固定机位：runs/20261013_134111_965_r3courtwalk，19张，D.2 12PASS/7FAIL；拼图审阅1/2，地窖过亮、四空屋仍缺家具。前次0截图为夹具scope不匹配，已修，不计美术尝试。

2026-10-13 S2 attempt2：editor/20261013_135500_518_S2_final_interiors_apply02保存，四空屋29家具全部落地；21灯调节+2可见灯具补光。具名存读档runs/20261013_135649_787_r3namedsave 45/45PASS；照片runs/20261013_135820_165_r3courtwalk 19张，D2 16/19PASS，B141/B155/B214过曝FAIL保留。床/地图原始道具未找到，家具完整度USER_REVIEW；两次结束转街面。

2026-10-13 S3：首替换140512原子拒绝3地块渲染接触，0保存；141436子蓝图组件数工具误判0保存，丢弃并修工具；141650保存Arch01一栋，L05/Arch02结构接触BLOCKED。142021具名45/45。街面142235异步载入导航锁工具FAIL，0保存丢弃；142553等待原生解锁后保存市场1/港口3饰物，8摊位均不合适。142758具名45/45；142939三照片及夹具11/11。第3项两次结束，附step3_after.jpg，未宣称大墙全部处理。

2026-10-13 S4新鲜导出：editor/20261013_143331_006_S4_final_full_geometry01 PASS_DIAGNOSTIC_ONLY，文件未改；13321actors/69932静态槽/751网格/88特殊组件（85导出，3不可见Volume排除）。两种SportsCar资产为Nanite fallback，保留限制。全类别初轮标量审计保留partial；为加速换编译三角SAT，600随机/共面/世界偏移样本与完整原三角判定一致，最大深度差2.83e-8cm，原/加速各60单测PASS。首个错误对照只调用裸SAT造成19个共面假阳性，已改为完整原判定，记录在triangle_kernel_parity.json。阈值2cm不变；B2_render_initial_fast仍RUNNING，无验收PASS。

| 2026-10-13 第4项全图审计与两批植被修复 | 初扫509015对，原始>2cm记录108053（含同建筑组合，不能当未豁免计数）；第一批后105349。保存338处藤蔓移除+158株树灌木移位，393冻结点不变；每批具名NPC/任务存读档45/45 PASS。B.2未清零，最终几何复核/B.6待完成 | appendix/final_20261013/B2_render_initial_fast.json；B2_render_after_foliage02.json；editor/20261013_160523_775_S4_foliage_overlap_apply02；runs/20261013_160752_058_r3namedsave |

| 2026-10-13 B6中央/市场首轮 | 中央155/157，两个同点反向采样因地形高度低于导航面未拍，计划补拍；市场199/199、原生404/404，启动器FAIL_COMPLETED_EXIT_TIMEOUT但进程0退出；港区首轮WAITING_HOST_WINDOW、0图，不计真实失败。市场两张拼图已看：部分暗室、黑色遮挡仍在，B6未通过 | runs/20261013_161858_276_r3courtwalk；runs/20261013_163155_662_r3courtwalk；runs/20261013_165233_760_r3courtwalk；appendix/final_20261013/B6/market |
- 2026-10-13 B6港区两次WAITING_HOST_WINDOW：原生initial focus timeout，OS前台正确但Slate视口未聚焦，0有效采样；并非地图FAIL。171333轮通过实际点击视口启动，仍在运行。完成退出宽限仅15→90秒、已知隐藏窗口5→30秒；39项退出竞态/20项切图夹具测试PASS。
- 2026-10-13 B6港区171333：native362/362、退出0；港176/176、中央补2/2。港区亮度3告警、中央累计8告警。各看2拼图：中央0581/0582树旁黑块、0611近墙；港1092/0929黑块、B142亮部问题。B141标为室内的点实际落在桥上，室内覆盖不能记通过，待用真实室内高度重采。N严格巡游夹具编译PASS，仅测试准备。
- 2026-10-13 B6运河172700：356/356、退出0；175/175截图，14亮度告警，已看2拼图。0313/0314大面积全黑，0283/0284近墙黑块，1022树叶几乎遮满画面；这些不能用导航通过抵消，列入C6。B141普查已标不完整建筑，室内完成与覆盖BLOCKED，旧固定D2数字只代表截图。
- 2026-10-13 工坊174021：WAITING_HOST_WINDOW/exit0，原生initial focus timeout（OS前台1/Slate0）。点击发生在黑色加载画面而非加载后的视口；0采样、不计真实尝试。
- 2026-10-13 工坊重跑174437：native364项/6FAIL、正常退出0；工坊138/138（4亮度告警），城堡第二段29/31（1146/1147无导航），改站位室内9/13（B010/B017/B027/B142的观察目标点不可站立，原有有效图保留）。工坊2拼图已审：1057/1058近物黑块；B209、B214在室内标识机位显示外部/消失墙体，留待实际镜头路线诊断。
2026-10-13 B6 castle: 175849_810 native 384 checks/22 FAIL (ground/nav setup); combined castle coverage207/231. Two collages reviewed: multiple fully black near-object views and foliage, B004 near obstruction; CELLAR/THRONE survey points visually coincide, so cellar completeness not proven. E/B6/castle; gate135 running181252_570.
2026-10-13 Step4 B6 COMPLETE_WITH_GAPS: gate181252_570 276/276,135/135photos,5metric flags; total1187/1211,7 districts x2 collages viewed. Gate dark rooms995/996,nearwall1137/1174,B155 bright. B2 remains BLOCKED_NOT_ZERO. Camera geometry reader accepts this exact native inventory hash;11unit tests and valid/mismatched-native-binding checks PASS. Step5 next.
2026-10-13 C6 batch1 182458_354: native57checks12FAIL; normal engine exit0 but launcher WAITING_HOST_WINDOW due4 initial warmup unfocused frames (no midrun focus loss). 188s diagnostic12.470FPS, native audio1.379s short; encoded failed diagnostic with original shorter audio/no padding,2344JPG cleanup,9 exact failure keys retained. Native eyes/hero/stuck PASS; near/middle/jump FAIL;2426 camera geometry OPEN_SOLID unresolved. B008 wrong legacy-shop route corrected; B015 turn moved to already observed native feet; QA endpoint reapproach/analog slowdown compiled,6s70cm unchanged. Old evidence retained. Colorprobe max1/255 errorPASS;12encoder testsPASS; C6 others pending.
2026-10-13 C6 batch2 184516_370: completed83checks/15FAIL,zero unfocused scenes;288.67s3767frames, audio0.291s short. Partial nav B209/B146/B022;B016 target155cm,B140 look-target hits furniture106cm high. Geometry4039samples:4029open-solid unresolved,10FAIL at B146 segment2,all reviewed no-collision lantern mesh HCM2InstancedProp_31. Saved-map unchanged. Production central-depth query repair1 compiledPASS;camera-only25cm lantern bound guard building,full-mesh offline check remains separate. Step3+retests pending.
2026-10-13 C6 batch2 diagnostic MP4 complete13.0499FPS,3767temporary JPG removed;1x12 collage reviewed. Explicit video-memory-over-budget engine warning visible, near-wall blacks and bright interior walls. Native hero/eyesPASS butnear/middle/jump/stuckFAIL; pixelsFAIL. Middle-depth query compiled185404; reviewed no-collision lantern25cm camera clearance compiled190016 exit0,22source checksPASS. No map mutation. Next third batch uses both production repairs; first2 retests still required.

- 2026-10-13 C6第三批190549：102断言/12FAIL，王座厅和地窖抵达通过；18.5147FPS，角色/近物/中间遮挡/跳变FAIL，眼高PASS；静态几何1490帧clear/5636开曲面未决，0命中不等于全量PASS。证据appendix/final_20261013/C6_batch03。
- 2026-10-13 C6第二次局部修复193256：12派生材质仅扩大选中物件局部孔径，最终编译12/12；地图/物体坐标未变；保存后193739具名NPC任务存读档45/45。导航只读448点完成，17室环视候选改为当前地面和完整Nav路径；新坐标及旧FAIL均保留，未宣称全房覆盖。193719 Editor构建PASS，旧源码断言计数测试补入已存在的第5类动态路线后8/8。
- 2026-10-13 C6复测1 194002：59断言/2FAIL（B015返回3/4段），旧12FAIL已保留；仅预热3帧未聚焦、所有实际路线0，正常退出被运行器标WAITING_HOST_WINDOW，保留原回执不记验收通过。复测2启动194754未取得前台自动结束，WAITING_HOST、不计尝试，待重跑。
- 2026-10-13 C6复测2 195136：83断言/9FAIL，全程失焦0、正常退出；B016返回和B209整条入口导航仍未通过，原FAIL保留。最终普通镜头全路线验收不能PASS。第三批195841开始。
- 2026-10-13 C6复测3 195841：102断言/2FAIL（B017段2、B010段4），全部场景失焦0，正常退出；王座厅/地窖抵达通过，不代表全帧镜头通过。三批进入离线几何/亮度/黑块检查和MP4清理。
- 2026-10-13 第5项结束BLOCKED：appendix/final_20261013/C6_FINAL_SUMMARY.json；三份诊断MP4及各1张12格拼图审毕，全部JPG临时帧验证后清理。旧城堡NavRegion分类缺口不算D2通过；本轮只补测量端实际范围+顶面命中，未改相机，201942编译PASS。
- 2026-10-13 第6项原记录核对：L_EXISTING_EVIDENCE.json保留305/305、同次31/31+27/27、C1原有效10/10；不重复刷命中。L1专项16尚未跑；C-OS因无分开按抬接口NOT_RUN并附人工步骤。最终包各项尚未测试。

- 2026-10-13 Candidate3c2ceee9 Shipping构建PASS，SHA67a90b9a8e5c09fedea092cb1b3a1f41f0a696cd1a16d4a1591850845fcae6bf；N两次22/0，五视点FPS/p99/显存通过，Epic启动设备峰值7639MiB保留：appendix/final_20261013/N_CANDIDATE01.json。
- 同候选主线205011_563：120/0PASS。完整回归204602_845：125/8FAIL，支线NPC距离272.55/269.48cm未进入240cm交互范围，接送未接取，最终存档检查中止后续180项；首次改夹具为碰撞扫掠+实际Move有界接近，240cm/65cm/存档校验未放宽；新候选打包中。

- Candidate6cca83f6构建PASS SHA fedc433f8bc4c270f94e3a4bbc9ca114a4cbb8542702bb47dbfdd11bb427d1ef；完整210551_148：125/5FAIL，NPC接近3/3，实际交付/上车失败，末F5正确拒绝非步行状态，后续31/27未运行。证据FULL_CANDIDATE02.json。
- 同候选211355_150独立TP接送诊断21/1：咖啡/上车/接送/F5F9通过；倒车中真实速度-272至-1591cm/s，3秒末反弹导致最终符号FAIL。第二次夹具修复：实际NPC提示/对话归属、普通TP并恢复原FP、持续负速度0.25秒后结束(3秒上限及原断言不变)；生产交互240cm、接送65cm、NPC坐标、存档校验未改。FP组合失败保留；Candidate03构建中。

- Candidatefc13ebe7构建PASS SHA564e51061ef9c4c094646e7baa9e72fe6bd1de4c2fca1b0c9d5390e22e0d75c2；完整212739_049：309项307PASS/2FAIL，同次系统31/31+音效27/27全部通过，支线三NPC/E归属、咖啡/接送/主线最终存档通过。仅Spell1/2预期15/35MP实测0，施法前HP25；证据FULL_CANDIDATE03.json。
- 攻击技能费用夹具首次修正：临时战斗目标160→600cm并按已有机制还原原NPC；8秒有界正常就绪，不重置HP/MP不重复施法；冲击波朝普通地面瞄准；新增实际CastKind读回，15/35MP、冷却和其他生产值不改。Candidate04构建中。

- 2026-10-13 Full04：313项/5FAIL，系统31/31音效27/27同次PASS；法术就绪/选择PASS但扣蓝FAIL，未改拳击/魔导枪断言出现FLAKY。见 appendix/final_20261013/FULL_CANDIDATE04.json。法术夹具修正2/2加入普通Attack明确零值和0.25秒释放交付间隔及实际输入读回；无额外施法、无重置、原费用断言保留，结果待最终包。

- 2026-10-13 最终候选563d98c7 / SHA51881bf1：Full05 317/317PASS，同次系统31/31、音效27/27，原305项名称/重数全覆盖；Attack实际按下/释放和两种攻击技能原15/35MP断言均PASS。修正仅测试输入释放交付，无施法重试/状态重置/断言放宽。Full01–04原FAIL及拳击/魔导枪FLAKY保留。证据 appendix/final_20261013/FULL_CANDIDATE05.json；runs/20261013_221938_241_r3finalfull05。

- 2026-10-13 最终Shipping十专项：story 120/0FAIL, travel_capital 12/0FAIL, travel_old 18/0FAIL, old_save_import 15/0FAIL, capital_r2_import 23/0FAIL, ride 20/0FAIL, c1 116/4FAIL, bow20m 50/0FAIL, old_a6 189/0FAIL, old_sword 140/0FAIL。C1仅本次一组，TP腰射3有效0命中/12无效，TP冲刺重击0有效/15无效，4FAIL，不重跑取好结果；其余专项PASS。证据 appendix/final_20261013/functional_batch_20261013_222841_679/ledger.json。
- 2026-10-13 画面设置首启 FAIL_TOOL_NO_GAME：graphics_20261013_225129_964要求实际打包清单不存在的cook_log字段；未启动游戏、用户配置未改。改用完整回归同一候选目录/EXE哈希/Cook日志哈希绑定，原清单不改。旧静态源码检查1FAIL（仍期待不存在字段）保留，改成实际provenance字段后19/19PASS；仅重跑画面设置。

- 2026-10-13 画面设置：graphics_20261013_225837_348 WAITING_HOST_WINDOW，工具在启动1秒尚无窗口时误判；7条局部原生检查不能当全测PASS。等待初始化再做窗口检测后19工具检查PASS；graphics_20261013_230304_941 PASS_M_FUNCTIONAL_ONLY，实际新王都、正常退出、用户真实配置哈希不变；非C-OS，不含已取消的35%功耗门槛。

- 2026-10-13T23:13:32.2441196+08:00 PAUSED_BY_USER：收到“先暂停”，设置STOP_DESKTOP_CHAIN并停止本轮媒体批次及其后处理进程。c6_01 runs/20261013_230351_925_r3courtwalk游戏已结束，后处理/验收未完成；保留原始压缩截图和部分产物，未续跑、未发布。
- 2026-10-14T13:02:46.5851244+08:00 c6_01续完：原后处理未完目录保留；实际Shipping路线原生断言0FAIL，逐帧近物/中部遮挡及黑块FAIL、14.74FPS<29.5；12格拼图已审，局部亮斑/门框遮挡仍见；临时帧已编码清理2975张。shipping_summary.json记录明细，几何覆盖未决。
- 2026-10-14T13:12:03.7770849+08:00 续跑工具：未完pixels目录先保留再分析；新增Shipping逐段汇总与76个D2房间/昼夜/画质单元覆盖表，缺测不写PASS；py_compile通过；c6_01真实数据验证及全部待测76NOT_RUN核对完成。
- 2026-10-14T13:17:30.6676704+08:00 Shipping C6_02：83条/8FAIL均B209路径/到达；13.40FPS，眼高/跳变PASS，可见率/近物/中部/黑块FAIL，几何覆盖未决。1张12格拼图已审（152.55s大黑块、局部亮斑/遮挡）；3775临时帧已编码清理。media_batch_20261014_130247_622/c6_02。
- 2026-10-14T13:29:10.4263409+08:00 汇总修正：C6_01导出几何已有1帧FAIL，不能只记覆盖未决；现标FAIL_EXPORTED_GEOMETRY并保留完整覆盖未决。原汇总已备份，原始帧统计未改、未重跑。
- 2026-10-14T13:34:59.9220503+08:00 逐帧分析提速（仅后处理）：3线程、至多3帧排队；公式/阈值/输出顺序不变。旧39项+新4项均PASS，10张真实1080p帧全部指标严格相等，0.655→0.315s；原工具备份和METRIC_WORKERS_PARITY.json保留，已完成视频不重算。
- 2026-10-14T13:39:08.4448945+08:00 Shipping C6_03：102条/1FAIL（B010出口，采样射线/实际导航地面高度不一致，单独夹具复核待跑）；19.67FPS，眼高PASS，其余可见率/近物/中部/黑块/跳变FAIL、几何覆盖未决。1张12格拼图已审，6567临时帧已清理；地窖整圈均亮0.64–9.49FAIL，王座厅大量unknown室内上下文，不能记D2PASS。
- 2026-10-14T13:45:38.5273884+08:00 D2 Epic白天01：10房短路线56/0；亮度4PASS（B016/B025/B140/B146）、6FAIL（B002/B015/B141/B008/B004/B209），全部室内上下文覆盖有效。11.73FPS；1张12格拼图已审，暗角/亮台面仍见；1899临时帧清理。详SHIPPING_CAMERA_D2_PARTIAL.json及批次同名目录。
- 2026-10-14T13:52:38.3414759+08:00 D2 Epic白天02：9房全部亮度FAIL；本组短路线室内上下文均有效（不同于C6王座深处unknown）。16.98FPS，2302临时帧清理，1张12格拼图已审。Epic白天19房合计4PASS/15FAIL，入口黑区/室内高光未解决；非完整房间遍历通过。
- 2026-10-14T13:59:46.1755313+08:00 D2 Epic夜01：10房3PASS（B016/B025/B140）、7FAIL，室内上下文均有效；11.64FPS，1910临时帧清理，1张12格拼图已审，暗部/亮台面未解决。
- 2026-10-14T14:07:24.3473203+08:00 D2 Epic夜02：9房全FAIL，上下文均有效；17.34FPS，2361临时帧清理，1张12格拼图已审。Epic夜合计3PASS/16FAIL；昼夜数据各自保留。后续新增High三批C6黑块复查（REQUEST第50行），只测不改场景。编码4线程的12项测试PASS，色彩/时间轴/CRF不改。
- 2026-10-14T14:18:51.1469656+08:00 High白天01：10房3PASS（B016/B025/B140）、7FAIL，14.58FPS；2232临时帧清理，1张拼图已审。汇总曾因GetOverall=-1误记缺测；本机UE Scalability.cpp:1083把High87%与要求100%判custom，现以同次11项保存/启动参数及运行时GI/反射2、比例100绑定High；原-1/旧汇总保留，4项工具测试PASS，无重跑。
- 2026-10-14T14:21:56.6238728+08:00 D2 High白天02：9房全FAIL，High100%及室内上下文绑定有效；18.67FPS，2531临时帧清理，1张拼图已审。High白天合计3PASS/16FAIL。
- 2026-10-14T14:28:10.0052413+08:00 D2 High夜01：10房4PASS（B016/B025/B140/B146）、6FAIL，上下文及High100%绑定有效；14.44FPS，2252临时帧清理，1张拼图已审。
- 2026-10-14T14:35:18.4128554+08:00 D2 High夜02：9房全FAIL，18.16FPS，2499临时帧清理，拼图已审。最终76组合14PASS62FAIL；Epic昼4/19夜3/19、High昼3/19夜4/19；四组合全PASS仅B016/B025/B140。SHIPPING_D2_FINAL.json。全部8段各审1张拼图，D2验收FAIL。

- 2026-10-14 B010夹具复核：原外侧目标落地射线误取678.902cm，实际Nav570cm；仅夹具XY移约55cm/Z改到570，70cm到达阈值不变，地图/NPC/任务坐标未动。Shipping143520路线0FAIL；近物、D2、14.13FPS仍FAIL，几何未决。已审1拼图，570临时JPG清理；media_batch_20261014_143518_720。原失败保留。

- 2026-10-14 最终High C6批1：路线0FAIL；眼高/可见率PASS，近中遮挡/黑块/跳变FAIL，几何6140帧未决；17.27FPS。已审1拼图，3358临时JPG清理；media_batch_20261014_144805_998/c6_high_01。

- 2026-10-14 最终High C6批2：B209四段导航/到达共8FAIL；眼高/可见率PASS，近中遮挡/黑块/跳变FAIL，几何8333帧未决；17.20FPS。已审1拼图，4666临时JPG清理；media_batch_20261014_144805_998/c6_high_02。

- 2026-10-14 最终High C6批3：路线0FAIL（含修正夹具的B010）；眼高/跳变PASS，可见率/近中遮挡/黑块FAIL；20.77FPS。几何10817帧未决/2869已导出静态几何CLEAR。已审1拼图，7048临时JPG清理；media_batch_20261014_144805_998/c6_high_03。
- 2026-10-14 宿主工具：35份旧遥测JSON启用NTFS压缩，逐文件SHA不变，释放约2.56GiB；录像不受影响。剪辑4秒探针：旧默认时间基丢17帧、滤镜拼接多27ms均被阻断；改为逐段编码+明确时长后60帧、段内时间戳与切点完全匹配，音频−17.12LUFS/−1.45dBTP；末尾取帧工具错误已修。探针不交付。

- 2026-10-14 最终N首次（接电、不录像）：High五视点76.62–97.15FPS/p9911.96–14.98ms、Epic49.86–56.00FPS/p9920.55–23.35ms均PASS；设备显存峰值7135/7549MiB，Epic启动8秒处超7GiB，FAIL保留；正式Epic视点4506–5909MiB。旧港町High/Epic各16PASS、峰值2952/3839MiB。N_FINAL_FIRST_20261014.json。隔离启动INI此前缺11画质组，已补齐目标档位和100%，复测验证，未改用户配置或包。

- 2026-10-14 N启动配置复核：High峰值7135→5283MiB；Epic7549→7551MiB，超限仍FAIL；两档五视点均22/0。N_STARTUP_BOUND_20261014.json。当前进行隔离瞬时堆优化试验（128→64MiB，回收16→2周期），属于诊断覆盖，不可代替最终包结果。

- 2026-10-14 瞬时堆优化诊断PASS：Epic22/0，49.95–55.38FPS、p9921.03–23.93ms，启动峰值7551→6698MiB；五视点最高5896MiB。仅DefaultEngine.ini采纳最小堆64MiB/回收2周期，原文件已备份；地图、资产、源码未变，准备候选06。诊断覆盖不当作最终包验收；N_HEAP_PROBE_20261014.json/TRANSIENT_HEAP_ADOPTION_20261014.json。

- 2026-10-14 候选06 6a38b425 Shipping打包PASS：只采纳两项瞬时堆配置，地图/内容未变；EXE SHA仍51881bf1…6205，归档清单SHA变为B805F7B3…990E8。源码/内容构建期间稳定、Cook与归档核对PASS；新包重新绑定验收，不能继承05为最终PASS。builds/20261014_155711_753_6a38b425_package/package.json。

- 2026-10-14 Candidate06完整回归：317项/1FAIL（Actual punch damages target），系统31/31、音效27/27；FULL_CANDIDATE06.json保留。发现R3拳击夹具每次重新摆位调用PrepareR2SwordTestStand重置血量；修复后续仅移动玩家，距离115/110/110cm与Health<100断言不变，新增逐拳原生事件记录，尚未编译/运行，不能认定已修好。

- 2026-10-14 Candidate06专项结束：主线120/0、地图往返12/0+18/0、旧档带入15/0+R2→R3 23/0、接送20/0、星弓20m 50/0、旧港町A6 189/0及剑矩阵140/0；C1仍116项4FAIL；画面设置PASS_FUNCTIONAL_ONLY。证据functional_batch_20261014_161832_117与graphics_20261014_164055_269。
- 2026-10-14 Candidate07 fc840e5a打包PASS：Editor/Shipping编译、Cook/Archive、地图审计与文件核对通过；EXE SHA256 422a063a3da6b73a049461b82882796663c99e2dca2a25f5953fd34ad81686cf；builds/20261014_164156_066_fc840e5a_package/package.json。运行验收尚未完成。

- 2026-10-14 Full07：319项/2FAIL（R3 gun sequence deals fresh target damage、Magic gun uses existing firearm damage），系统31/31、音效27/27；旧317项名称/重数全覆盖，新增2次NPC状态保留检查。拳击逐拳HP100/100/75，第三拳真实PunchDamage25；前两拳未命中，不能认定重置夹具是06失败唯一原因。魔导枪两次输入均被接受、正常散布射线未命中目标，保留FAIL及既有FLAKY历史，不重跑刷PASS。FULL_CANDIDATE07、FULL07_CHECK_COVERAGE、PUNCH07_OBSERVATIONS_SUMMARY。
- 2026-10-14 07专项首次启动WAITING_DISK，游戏未启动（functional_batch_20261014_170117_677）；核对05/06完整Archive逐文件摘要后删除重复Cooked/Staged临时目录约3.31GiB，独立包、源码、日志和旧FAIL均保留（PACKAGE_WORK_CLEANUP_20261014.json）；随后按同计划恢复专项，完整回归不重复。

- 2026-10-14 Candidate07专项结束：主线120/0、往返12/0+18/0、旧档15/0+23/0、接送20/0、星弓20m 50/0、旧港A6 189/0、剑140/0；C1 116项4FAIL（Hipfire有效3命中1/无效12，HeavySprint有效0/无效15）。画面设置PASS_M_FUNCTIONAL_ONLY，用户配置未改；FUNCTIONAL_CANDIDATE07.json、graphics_20261014_172824_592。

- 2026-10-14 07最终N：接电High/Epic五视点各22/0，High76.61–97.28FPS、p9912.07–15.07ms、显存峰值4943MiB；Epic50.17–56.28FPS、p9920.59–23.78ms，五视点达标但启动峰值7557MiB，FAIL。旧港High/Epic各16/0。包内DefaultEngine.ini已验证含64MiB/2周期，不能因配置存在就声称优化生效；N_CANDIDATE07_20261014.json。

- 2026-10-14 第二次瞬时堆试验32MiB/1周期：诊断覆盖下Epic22/0、五视点达标，但启动7682MiB超限；N_HEAP32_PROBE07_20261014.json。未采纳32/1，项目与07归档不变；两次堆调整后N显存记BLOCKED，不重复刷通过，继续07媒体与探索。

- 2026-10-14 07最终C6批1：路线0FAIL；逐片眼高/跳变PASS，可见率/近中遮挡/黑块FAIL；几何1个确认失败、4151帧未决；15.32FPS。已审1拼图，3022临时JPG删除，原生大JSON压缩前后SHA一致。media_batch_20261014_204915_421/c6_01；全局跳变含夹具转换，逐片口径见shipping_summary。

- 2026-10-14 07最终C6批2：B209四段导航/到达共8FAIL；眼高/可见率PASS，近中遮挡/黑块/跳变FAIL；几何5424帧未决；14.30FPS。已审1拼图，4051临时JPG清理，压缩保持SHA。media_batch_20261014_204915_421/c6_02。

- 2026-10-14 07最终C6批3：路线0FAIL（含修正B010落点）；眼高PASS，可见率/近中遮挡/黑块/跳变FAIL；几何7731未决、1906已导出静态几何CLEAR；20.15FPS。已审1拼图，6791临时JPG清理；media_batch_20261014_204915_421/c6_03。

- 2026-10-14 07 D2极高白天组1：{'FAIL': 6, 'PASS': 4}；原生0FAIL，12.03FPS，几何2390帧未决；已审1拼图，1942临时JPG清理。media_batch_20261014_204915_421/d2_epic_day_01；76格增量汇总SHIPPING_D2_CANDIDATE07.json。

- 2026-10-14 07 D2极高白天组2：{'FAIL': 9}，白天19间合计{'FAIL': 15, 'PASS': 4}；原生0FAIL，16.20FPS；几何2782未决/271静态CLEAR；已审1拼图，2208临时JPG清理。media_batch_20261014_204915_421/d2_epic_day_02。

- 2026-10-14 07 D2极高夜间组1：3PASS/7FAIL（B016/B025/B140通过）；原生0FAIL，12.39FPS；几何2476未决；已审1拼图，1961临时JPG清理。media_batch_20261014_204915_421/d2_epic_night_01。

- 2026-10-14 07 D2极高夜间组2：9FAIL，夜间19间合计3PASS/16FAIL；原生0FAIL，16.53FPS；几何2977未决/244静态CLEAR；已审1拼图，2273临时JPG清理。media_batch_20261014_204915_421/d2_epic_night_02。

- 2026-10-14 07 D2 High白天组1：3PASS/7FAIL（B016/B025/B140通过）；原生0FAIL，13.86FPS；几何2确认FAIL/3449未决；已审1拼图，2149临时JPG清理。media_batch_20261014_204915_421/d2_high_day_01。

- 2026-10-14 Candidate07 d2_high_day_02: native 0 FAIL; D2 {"FAIL": 9};18.44FPS FAIL; geometry4450 unresolved/337 clear;2512 temporary JPGs removed; one collage reviewed. Evidence: appendix/final_20261013/media_batch_20261014_204915_421/d2_high_day_02/shipping_summary.json.

- 2026-10-14 Candidate07 d2_high_night_01: native 0 FAIL; D2 {"FAIL": 7, "PASS": 3};13.54FPS FAIL; geometry2FAIL/3473 unresolved;2138 temporary JPGs removed; one collage reviewed. Evidence: appendix/final_20261013/media_batch_20261014_204915_421/d2_high_night_01/shipping_summary.json.

- 2026-10-14 Candidate07 d2_high_night_02: native 0 FAIL; D2 {"FAIL": 9};19.10FPS FAIL; geometry4255 unresolved/322 clear;2581 temporary JPGs removed; one collage reviewed. Evidence: appendix/final_20261013/media_batch_20261014_204915_421/d2_high_night_02/shipping_summary.json.

- 2026-10-14 Candidate07 c6_high_01: {"native_fail":0,"fps":17.685,"checks":{"C6.1_render_geometry":"NOT_RUN_FULL_RUNTIME_SURFACE_COVERAGE","C6.2_eye_height":"PASS","C6.3_character_visible":"PASS","C6.4_near":"FAIL","C6.4_middle":"FAIL","C6.6_camera_jump":"PASS","capture_fps":"FAIL","C6.5_black_component":"FAIL"},"geometry":{"NOT_RUN_OPEN_SOLID":6131},"temporary_frames_removed":3366};one collage reviewed. Evidence: appendix/final_20261013/media_batch_20261014_204915_421/c6_high_01/shipping_summary.json.
- 2026-10-14 07旧港町同名六NPC/多法术压力段：High104.161FPS/p99 11.222ms，Epic69.326FPS/p99 16.970ms；相对R2 109.43/74.25下降4.815%/6.632%，每档16/0。比较不用整段启动平均；早期错误口径草稿保留并标INVALID_SCOPE。证据appendix/final_20261013/OLD_PERFORMANCE_COMPARISON07.json。

- 2026-10-14 Candidate07 c6_high_02: {"native_fail":8,"fps":16.245,"checks":{"C6.1_render_geometry":"NOT_RUN_FULL_RUNTIME_SURFACE_COVERAGE","C6.2_eye_height":"PASS","C6.3_character_visible":"FAIL","C6.4_near":"FAIL","C6.4_middle":"FAIL","C6.6_camera_jump":"PASS","capture_fps":"FAIL","C6.5_black_component":"FAIL"},"geometry":{"NOT_RUN_OPEN_SOLID":7821},"temporary_frames_removed":4342};one collage reviewed. Evidence: appendix/final_20261013/media_batch_20261014_204915_421/c6_high_02/shipping_summary.json.

- 2026-10-14 Candidate07 c6_high_03 WAITING_HOST/BLOCKED_FOCUS；运行20261014_223950_537_r3courtwalk因前台失焦中断，不算失败次数；前13组保留，仅本组待重跑。

- 2026-10-14 逐栋落地筛查：398栋，实际底座外轮廓点266栋≤15cm、51栋超限候选、80栋支撑未决、1栋换楼归属未决；非全图/连续边界/最终动态表面验收。两次AABB工具结果保留，第三次改用真实网格顶点。地图哈希绑定检查及全部逐栋记录见appendix/BUILDING_GROUND_AUDIT.json。

- 2026-10-14 被中断c6_high_03仅保留诊断MP4：305.582s/21.346FPS，BLOCKED_FOCUS；音频尾部不完整，显式short-audio诊断，acceptance_eligible=false；验证后删除6523张临时JPG。首次严格转码音频校验拒绝记录保留。

- 2026-10-14 c6_high_03恢复启动BLOCKED_INITIAL_FOCUS：20261014_230500_352_r3courtwalk；WAITING_HOST，不计失败次数，未重复前13组。已询问主机可用时间，转离线制作。
- 2026-10-14 final07 media14 complete: resumed c6_high_03 run231446_518 native102/0,331.383s/21.5098FPS FAIL; pixels/near/visible/middle FAIL, geometry11135unresolved/2895clear. One12tile collage reviewed;7128temporaryJPG removed. Two focus interruptions retained. Full14aggregate SHIPPING_D2_CANDIDATE07_COMPLETE14.json: D2 13PASS/63FAIL. Normal task/navigation HUD confirmed; earlier debug-overlay suspicion withdrawn.
- 2026-10-14 final07 soak first run233211_498:19checks4FAIL before clock (port→CASTLE_0166 and CASTLE_0166→throne approach partial native paths). 30min NOT_RUN. Door does not affect navmesh. Next fixture uses observed throne outdoor approach as castle district point; all9 required zones and actual3interior points retained, thresholds/map unchanged. Not a PASS claim.
- 2026-10-14 final07 soak adjusted fixture233625_595:17checks2FAIL, port→observed throne outdoor approach still partial. DoorLeaf navigation=false; not a closed-door preflight artefact. Full30min clock never started; distance/stuck/hitch/memory metrics NOT_RUN. Required zones not dropped; SOAK_CANDIDATE07.json BLOCKED. Continue delivery with this limitation.
- 2026-10-14 final07 photos five jobs complete, ledger functional_batch_20261014_233801_376: Epic fixed115/0,19interiors11/0,8landmarks11/0,catalog26 11/0,High fixed115/0; actual L_CapitalR2. These are capture fixture checks, visual review pending. Candidate07 archive87files hash-verified; removed only1.775GB duplicate Cooked/Staged working copies, archive/source/evidence intact.
- 2026-10-15 video batch235759_753: main69/4FAIL (guild70.595cm>70; canal partial path/zero walk),21.11FPS. Flight1 13.75FPS,2 29.99FPS,3 29.07FPS. Flight4 run001929_626 ended WAITING_HOST_WINDOW; native8/0 and all12scene unfocused_frames=0, initial focus verified, process exit0. No user interruption proven; retain as incomplete launcher/capture evidence and retry4 once with canal supplement.

- 2026-10-15 离线收尾：flight4恢复002354_065有效；canal补拍002612_676 native11/0、9.24FPS/像素FAIL。首轮flight4仅诊断23.971s/30.036FPS，保留WAITING_HOST_WINDOW，非验收源；错误--failed-diagnostic用法首次被工具拒绝，二次按实际capture状态编码并清帧。
- 2026-10-15 编辑计划工具：主片Chapter1原尾段比最后帧晚23.531ms、飞行17.5秒越过实际采集区间，严格绑定拒绝；分别显式选实际14秒和每段17.3秒后生成，保留失败日志，不补帧、不变速。四母版时长336.131/215.200/69.276/63.100秒、FPS20.061/13.611/24.092/16.672，全FAIL；音频均PASS。
- 2026-10-15 最终照片147张、R2固定对比8组、作者同类对比8组已检查限定拼图；JPG均≤500KB。目录裁切/侧墙稀疏/远景生硬保留FAIL，B214朝街与B141壳不完整分别记录。旧影片逐问题同姿态对比NOT_RUN。

- 2026-10-15 汇总工具修正：原主片B4误把0.0791/0.0221/0.0291秒低速都算失败，按原>0.5s门槛撤销该误报；C6.1原聚合漏识别FAIL_EXPORTED_GEOMETRY，主片4帧/镜头片1帧据实改FAIL；98%可见率门槛与环境/主角覆盖分别计算。原native_audit.json不改，native_audit_corrected.json摘要绑定原文件；不重录。

- 2026-10-15 文档汇总首次因旧室内视觉回执没有public_sha256字段停止，原回执备份，补绑定同一已审视频摘要后成功；无媒体改动或重审。
- 2026-10-15 清理10-11历史临时帧：6组共4523张JPG，编码并完整解码核验后清除，各保留8张关键图；原FAIL源仅作本地诊断，不纳入最终或公开媒体。无PPM/BMP/TGA。清理回执final_20261013/legacy_cleanup_20261015/receipt.json。
