# 本轮录像

真实 Editor -game，正常速度，原生游戏音效，公开 BGM 静音；不是独立包或真实 OS 键鼠证明。

文件：`D:\科研学习\codex学习\docs\HarborCity_M5_VS3\recordings\20260927_161607_545239E4\capture.mp4`

本段约 24.9 秒，MP4 约 13.3 MiB；同样的 1080p、30 FPS 未压缩 RGB 帧约 4.3 GiB。录制上限 30 FPS，实际采样约 20.4 FPS，没有补帧或变速；这不是无录制开销的性能测试。

录像继续放在 `docs/HarborCity_M5_VS3/recordings/`，不放入打包目录或 Git。JPG 临时帧已在 MP4 全音视频解码验证后清理，仅保留少量关键图。旧 PPM 目录未删除。

本轮录音修复位于 `HarborCity/Source/HarborCity/M3/HCM3Recording.cpp`：仅 QA 录制期间保存并设置失焦音量，结束恢复；正常游玩不改变系统音量。格式流程沿用 R1 的 JPG→H.264/AAC 验证与清理；`.gitignore` 已排除 PPM、BMP、录像及临时目录。
