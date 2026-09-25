# HeroFill Renderer 崩溃：命令对象恢复与剩余根因缺口

2026-09-25；**根因 OPEN，旧两次 FAIL 保留**。仅离线读取既有 dump、当前本机二进制与源码；没有启动 UE、Insights、编译、修改引擎/生产代码，也没有再次录 trace。

## 本次新增的可靠证据

用当前 Renderer DLL 的实际导出函数及反汇编确认栈布局，从两份旧 minidump 恢复 `SetShaderBindings` 的上一层 `SetOnCommandList`。不是按猜测的 C++ 对齐读取，也没有给未知 RVA 编造函数名。

| 已恢复字段 | cf3c4779 | 3a1b5cba |
|---|---|---|
| 实际调用分支 / 布局序号 | `SF_Vertex` / 0 | `SF_Vertex` / 0 |
| 命令地址 | `0x189ad0e01b0` | `0x1911205a360` |
| 布局数 / frequency bits / 绑定长度 | 2 / `0x9`（VS+PS）/ 72 B | 相同 |
| DebugData.VertexShader | `0x18a53b48b30` | `0x191bf72cb30` |
| 损坏的 ParameterMapInfo | `0x18a53b48b88` | `0x191bf72cb88` |
| 两字段差值 | `0x58` | `0x58` |
| VS/PS 指向的同一 ShaderMap | `0x18a70f85500` | `0x1909222ac80` |
| VS 与 PS shader 地址差 | `0x6e0` | `0x6e0` |
| 命令中复制的材质名字 | 长度含零 20；字符串页未收录 | 相同 |

`SetOnCommandList` 的原生返回地址均是 Renderer RVA `b4ad31`。前面的 `test dil,dil` 确认这是顶点着色器分支；保存的 `rdi/rsi` 均为 0，构成独立交叉验证。`SetDebugData` 原生写入偏移另证实 shader/map/name 字段位置。

上一报告已证实，损坏 metadata 所在区域与 Canvas 字体顶点的 9 条、540 个有效字节精确匹配。这次又确认：**命令里两个独立字段仍然一致指向同一 shader 及其内部 metadata，而被指向的 shader 区域已经含有字体顶点数据**。因此，“仅 metadata 引用偶然改成任意 Canvas 地址”的解释更弱；优先调查 shader-content 存储过期后重用，或对该存储的成片覆盖。但没有分配/释放事件，仍不能把 UAF 写成已确认。

命令头并非同样被字体覆盖；两次都呈正常 2 槽、72 B 结构。`DebugData.PrimitiveSceneProxyIfNotUsingStateBuckets` 为零不能证明 primitive 已销毁：引擎会为共享 state bucket 主动清除此字段。没有收录某内存页也不能证明对象被 free。

## 生命周期审查中容易误判的两点

1. **`TShaderRef` 不是持有所有权的智能指针。** `Shader.h:1192–1193` 是 shader 与 map 两个裸指针；`IsValid()`（1043）只检查非空。命令 DebugData 保存同样的引用，不能保护其存储。另一个层级 `FBoundShaderStateInput` / minimal PSO 保留 RHI shader resource，也不等价于保留 CPU `FShader::ParameterMapInfo`。因此加非空检查、只持有 RHI shader 都不是有效修法。
2. **不能直接指控编译发布漏了缓存失效。** 本机 `ShaderCompiler.cpp:2398–2408` 发布后确实调用 `PropagateMaterialChangesToPrimitives`，2438–2440 对受影响组件 `MarkRenderStateDirty`；`RendererScene.cpp:7171–7173` 明确依赖准确的 UsedMaterials，7206–7220 串行处理并延后重建命令。批量发布还有 RDG wait，ShaderMap 销毁也延后帧同步。项目与 VRM4U 受查代码未发现自定义主角 mesh proxy 漏报 GetUsedMaterials 的具体实现。单独 `SetGameThreadShaderMap` 缺少本地 wait 仍只是候选，不能跳过上述实际保护宣布引擎 bug。

## 8adce7f2 的 trace 已完成，不能重复当“下一步”

`D:/GameDev/Diagnostics/HarborCity/M5_VS2/20260925_075416_168_8adce7f2/renderer_memory.utrace` 实际 **971,301,855 B**。对应 launch exit 0，原生报告 PASS、4 PNG、无 Esc，未复现两次崩溃。本次只读文件大小与 32 B 文件头，没有声称分析过全部 allocation 事件。

该进程正常完成，不能用旧崩溃的绝对地址去查询这个进程的 heap，也不能据它关闭旧缺陷。本次**不建议重复同样的启动 trace**。旧报告中“建议首次采 trace”的步骤已经完成，历史文字保留但不是当前待办。

## 最小修法必须先满足哪项证据

目前没有一个能诚实称作“针对已证实根因”的生产补丁。继续加等待、全局 flush、隐藏 Preparing 字体、关异步或关闭补光，只能改变分配/提交时序。

| 后续若取得的直接证据 | 最小修复边界 | 修复验证条件 |
|---|---|---|
| 命令缓存跨 ShaderMap generation 存活，旧 content 已释放 | 精确失效并重建该 material/primitive 的旧命令；或在实际命令拥有者处保持对应 CPU ShaderMap 至最后消费完成 | 同一 owner 的旧命令在旧 content 释放前退休，VS/PS metadata 均来自当前 generation；正常图与材质就绪仍通过 |
| 不是长久缓存，是正在运行的旧异步提交任务尚未消费完 | 在实际 content 释放/替换的拥有者边界等待该任务，或把该 map 的持有期延至任务完成 | 只证明该边界的任务序列和释放顺序，不用全局每帧同步掩盖 |
| content allocation 一直存活，出现越界写 | 修复被观测到的 writer 长度/索引或引用写入处 | 真实写地址落在合法目标内；不能用上述 retain/等待代替 |

尚缺：崩溃进程内上述 ShaderMap 对象及 content 范围、命令所属 pass/primitive/material 名字、命令建立/失效时刻、该 content 的最后 alloc/free/writer。现有两份约 2 MB 的普通 dump 无法补回这些页或历史；缺 PDB 不是唯一缺口。

若后续必要的正常功能回归自然再次触发，可在**那次既定运行**采用本机支持的 `-fullcrashdump` 保存缺失对象页（`GenericPlatformCrashContext.cpp:477–488`，`WindowsPlatformCrashContext.cpp:169–172`）。这不是新增重跑请求；全 dump 仍不提供 free 历史，不能单独确证 UAF。没有在本轮增加参数或执行该方案。

## 可复核产物

- `HERO_FILL_DRAWCOMMAND_FORENSICS_20260925.json`：两 dump 的实际对象字段、raw binding 头、缺页标记、原 dump SHA。
- `HERO_FILL_DRAWCOMMAND_NATIVE_OFFSETS_20260925.txt`：4 段有界原生反汇编，复核手工展开一层栈所用偏移。
- `extract_hero_fill_drawcommand_20260925.py`：仅读两个原 dump，每次内存范围读取上限 512 B；校验 DLL/dump SHA，拒绝覆盖旧输出。离线提取实际 PASS，生产修复与崩溃复现均未执行。
