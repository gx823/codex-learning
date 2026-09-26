# 录像格式与清理

当前录制器先保存压缩 JPG；每段结束后立即生成 H.264/AAC MP4、完整解码校验，再删除临时帧和录制 WAV。保留视频、少量关键图及元数据。它不是直接流式写 MP4，录制期间仍会暂占压缩帧空间；失败时保留证据并停止后续录制。

输出目录：`D:\科研学习\codex学习\docs\HarborCity_M5_VS3\recordings`。不再写到游戏包的 Saved/M*Recording。录制启动脚本、原生录制器以及打包脚本均检查 D/E 至少各有 30 GiB；不满足就停止。

本轮示例约 2 分 22 秒：MP4 69.5 MiB；原 JPG 中间帧约 2.35 GiB，已清理。同样帧数的 1080p RGB PPM 理论约 23.7 GiB，MP4 约为其 0.29%。实际大小随运动与画面复杂度变化。

四段本轮录制均已编码并清理临时帧。最新一段原生音轨静音，保留 FAIL；没有配入替代音效。VS3 目录没有发现 PPM，因此没有删除任何 PPM。此前压缩 JPG 历史证据保留，清单见附录；未动旧候选、素材和存档。

修改文件：

- `HarborCity/Source/HarborCity/M3/HCM3Recording.cpp`：删除 PPM 写出分支，统一 JPG85、约 30 Hz、文档目录与空间保护。
- `tools/run_m5_vs3.ps1`、`tools/run_m5_vs3_r1.ps1`：录制前查空间，结束后自动转码。
- `tools/complete_m5_vs3_capture.ps1`：绑定本段录像并执行编码校验。
- `tools/encode_m5_vs3_r1_final.py`：保留原始时间戳与音轨，H.264/AAC、完整解码、关键帧保留与安全清理。
- `tools/cleanup_m5_vs3_recording_frames.ps1`：已有合格 MP4 的定点清理；不处理 PPM。
- `tools/package_m5_vs3.ps1`：D/E 打包门槛至少各 30 GiB。
- `.gitignore`：忽略 PPM、BMP、录制临时目录与本项目图片/MP4；这轮 Git 仅提交文本与数据。

另修 `tools/build_m5_vs3.ps1`：原 Python 路径消失时复用已安装的 D 盘 Python，未安装新工具。详细路径、空间和清理量见 `appendix/RECORDING_STORAGE_RESULTS.json`、`RECORDING_CHANGED_FILES.json`。
