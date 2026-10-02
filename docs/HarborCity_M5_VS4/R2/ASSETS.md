# R2 素材

v2 更新（2026-10-02 16:23）：139 栋 City 静态实例楼、六种墙色 MI、四种顶色，68 外观组合；新补侧后窗共计 2041 个窗组件，1142 个暖色夜光。Castle 原有墙/栏杆模块用于连续城堡、驳岸和桥栏，悬崖替代蓝色远山；Village 划艇系在运河岸。水改为经连线校验的 SingleLayerWater，派生父材质清空节点时逐个删除，避免 UE 批量删除遗留自定义输出。最后整批日志没有编译失败/缺失资产/默认材质回退。所有原包不改；新关卡、MI、几何导出和其他派生资产均仅留本机。以下保留 B0 与 v1 记录。

City Pack 与 Highlands Castle 个人档由用户确认，原文件和派生资产不公开。使用 UE 5.8.2 AssetTools.migrate_packages，无弹窗，保留 /Game/ 顶层路径。演示地图、BuiltData、build 和 demo 排除；不改原包曝光或天空。

| 包 | 已迁资产 | 字节 | 用途计划 |
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
