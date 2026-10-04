# R2 素材

最终候选143049已引用下列迁入素材、最终地面和小地图。默认新王都，固定包测试的原生材质/来源检查通过；视觉质量另列USER_REVIEW。High60相对Epic不限功耗下降24.6486%，未达35%，不以小道具剔除的保存记录宣称功耗通过。公开内容不含包原件、任何派生资产、地图/几何数据或完整游戏。

2026-10-04 14:10：新增Finish_20261004私有地面材质/遮罩，使用已迁入Village地形草、碎石及石砖纹理。City/Castle原件新增1034个实例用于城外田地/树篱和港边货物；原25个远景岩壁实例位置/缩放修正。只改R2关卡引用、派生资产和实例，不覆盖三个购买包原件。地图/遮罩/田地配置/新小地图均留本地、不上传；公开仅源码方法、统计和游戏画面。

2026-10-04：全城旧Village参考房被后续布局覆盖，105515仅平移其11个实例及比例标记到城外，不改购买原件/材质/1.12缩放。原全城地图备份在本机 `E:\GameDev\Assets\HarborCity\M5_VS4\R2\capital_map_backups\20261004_105515_490_VillageReferenceRelocate\L_CapitalR2.umap`，SHA-256 `73faa4a12d1a3348a2173079b89467e1eaf5639b72e0b6bdc0e6841c1b98741d`。修改后关卡和备份均不上传。此前小道具剔除只改46组件/1751实例的距离，原件与建筑/植被不改；功耗收益尚未验收。

2026-10-03：空间淡化6个私有母材质中5个继承了“可视作不透明”旧标志，已由限定目录的原生作者接口清除，26个实例刷新；不改购买原件。五个遗留默认槽替换为已有黄铜/铁材质，车辆单独派生支持骨骼网格的M_R2VehicleIron。补完全城细节后的原生俯拍9cfa782a...已于181309重新导入私有小地图，裁切贴图SHA-256 c9de410590078afc981973c331154ad8e280baedcfcfed9e9a336047507d823b。上述派生资产均不公开。

全城本地增补（2026-10-02 22:22 保存，尚待画面验收）：31 组共 1589 个细节实例、187 个街灯实例，使用已授权 City/Castle/Village 模块；残留的灯塔、工坊、城外风车白模已替换。风车帆来自 Village 原件，当前为静态；未找到可用水车，不用基础形体伪造。22:30 另保存 32 个相机淡化派生材质、绑定 210 个建筑网格组件，沿镜头到主角连线局部淡化，不给整条街统一淡化；原材质不改，1 个透明组件保留实体相机碰撞。均属于不可公开的本地派生资产。此处的保存/数量记录不是视觉或运行 PASS。

v2 更新（2026-10-02 16:23）：139 栋 City 静态实例楼、六种墙色 MI、四种顶色，68 外观组合；新补侧后窗共计 2041 个窗组件，1142 个暖色夜光。Castle 原有墙/栏杆模块用于连续城堡、驳岸和桥栏，悬崖替代蓝色远山；Village 划艇系在运河岸。水改为经连线校验的 SingleLayerWater，派生父材质清空节点时逐个删除，避免 UE 批量删除遗留自定义输出。最后整批日志没有编译失败/缺失资产/默认材质回退。所有原包不改；新关卡、MI、几何导出和其他派生资产均仅留本机。以下保留 B0 与 v1 记录。

City Pack 与 Highlands Castle 个人档由用户确认，原文件和派生资产不公开。使用 UE 5.8.2 AssetTools.migrate_packages，无弹窗，保留 /Game/ 顶层路径。演示地图、BuiltData、build 和 demo 排除；不改原包曝光或天空。

| 包 | 已迁资产 | 字节 | 实际用途 |
|---|---:|---:|---|
| Fantastic_City_Pack | 1,071 | 450,128,489 | 广场、市场/酒馆、运河街区、工坊、石桥、室内 |
| Fantastic_Highlands_Castle | 380 | 438,407,312 | 城堡山、城墙、城门、灯塔、悬崖与远景 |

B0 首次进程 `editor/20261002_004739_325_B0_migrate`：所有 1,451 个资产加载和迁移文件验证完成，原件 SHA-256 清单一致，缺失引用 0；但 UE FindInBlueprintManager 搜索索引发生 handled ensure，命令行进程退出非零，整体 FAIL 保留。迁入结果须在 HarborCity 中独立验证，不以脚本内部 PASS 覆盖进程 FAIL。

实际 Asset Registry 显示 Castle 的 PS_FX_falling_leaves_castle、PS_FX_fog_castle、PS_FX_fog_small_castle、PS_FX_particles_castle、PS_FX_windtrails_castle 已为 NiagaraSystem，不是 Cascade，故无需再次转换。暂存工程提示未启用距离场，需在 HarborCity 验证落叶碰撞。City 的火焰/烟雾仍为 ParticleSystem，尚未放入样板区。

HarborCity 独立复查 `editor/20261002_010428_375_B0_catalog_retry` PASS，43 个 City 完整预制楼实际实例化，迁入包引用未缺失。先前目录检查因 `get_relative_scale3d` 不是 Python 接口而 FAIL，保留原结果；修复为反射属性读取后继续同一副本。

Village 找到原生划艇 `meshes/props/vehicles/SM_PROP_rowboat`。通过原有 static-only Advanced Copy 帮助器复制船、桨与四种门组件的依赖闭包，共 37 包、源资产 83,434,544 字节，放在新的私有 Village_15dcffc04d60 命名空间；没有修改原包，没有用基础形体拼船。

当前 R2 样板：City 建筑、石桥、招牌、路灯、摊位、公告板、花草与远山；Castle 塔段、塔顶、窗墙、悬崖、树；Village 划艇及 house_2 门口比例参考。City 建筑蓝图已转成静态实例，不保留蓝图 Tick。89 栋 City 楼、20 种材质/预制组合、4 种体形，3.125 cm 采样的原生屋顶覆盖率 41.195947% 通过数量门槛，美术仍 USER_REVIEW。另 1 栋 Village 不计入 City 屋顶覆盖率。材质实例化用途修复、屋顶着色及样板关卡均为本地派生资产，不改原件且不公开。公开内容仅文字源码差异、测量摘要和渲染后的 JPG/MP4，不含几何导出或资产文件。

Village 房屋查询初次 FAIL：所有房屋蓝图带灯或粒子，不能把整份蓝图标为 static-only 安全。随后按 SCS 静态组件白名单复制 house_2 的网格与材质，丢弃灯/粒子且不执行蓝图，`editor/20261002_014252_404_village_house_static` PASS，放入 Village_1cb59bf736c7。原始素材和私有几何导出均留本机。

门洞比例：主角现有 1.25 倍网格导入高度 171.204 cm；City 框内 238 cm，缩放 1.00；Castle 小门中线净高约 247 cm，缩放 0.97 后为 239.59 cm；Village 门框内缘约 214.2307 cm，缩放 1.12 后为 239.938 cm。门/主角比为 1.390、1.399、1.401；三张站稳后的编辑器 -game 实景对照来自 `m5_vs3_r3_b_20261002_122609`。远景植被等不使用建筑缩放基准。用户设置和原 B1 关卡未改。
# 2026-10-03 旧港町五槽修补

用户明确授权后，旧图只替换包裹、两处任务终端、灯开关、车辆底层网格的5个默认材质槽，使用现有M_Brass、M_Iron及已验证的私有SkeletalMesh铁材质。1714个Actor的位置、旋转、缩放、静态网格、碰撞配置和可见性数值未变，未改购买原件。原生保存记录`editor/20261003_200824_619_OldHarborFiveMaterialSlotsNumeric/author_result.json`。

原地图字节备份在`E:\GameDev\Assets\HarborCity\M5_VS4\R2\old_map_backups\20261003_200824_619_OldHarborFiveMaterialSlotsNumeric\L_HarborTown.umap`，3,824,410字节，SHA-256 `12a19cd779c7301d715a35f4d3e379f88208f5d9631e3bbc42d4973b649080bd`；修改后`4bc44116ac6923f1d48fd19f613bbc15eab0a36d9a75add5df2e2d5acb0458a8`。地图、备份和派生材质均不上传，旧候选保持不变。
