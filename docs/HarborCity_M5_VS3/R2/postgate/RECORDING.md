# 录像与清理

最终文件：`M5_VS3_R2_POSTGATE.mp4`，来自本次新 Shipping EXE。正常速度、原生游戏音效，公开录制时私人 BGM 静音；没有用后配音效或动画冒充实机。

实际约 191.47 秒／29.82 FPS／79.67 MiB。帧率未达到严格 30 FPS，来自原生逐帧捕获开销；没有插帧或变速掩盖。音轨做两遍测量的响度标准化，最终 −19.69 LUFS、−1.99 dBTP，完整视频解码通过。首次标准化峰值不合格的版本单独保留，未列入发布。

录制仍沿用压缩 JPG 临时帧→立即合成 H.264/AAC→解码确认→清理临时 JPG／WAV。此次没有生成 PPM／BMP。该段若保存未压缩 1080p RGB PPM，仅像素约 33.08 GiB；当前 MP4 约小 425 倍，不能把这个估算用于所有片段。

录像源保存在 `D:\科研学习\codex学习\docs\HarborCity_M5_VS3\recordings\<唯一目录>\capture.mp4`，交付副本在本 postgate 目录。没有把录像帧放入 Build、Staged、Archive 或 git。每次录制／打包前仍检查 D、E 各至少 30 GiB。

本轮改动：HCM3Recording.cpp 为受控武器库允许暂停画面采集，保留实际焦点和停止检查；encode_m5_vs3_r1_final.py 对中断片标 PARTIAL 后转码清理；run_m5_vs3_r2.ps1 记录活动进程和转码检查点。全局 .gitignore 已排除 *.ppm、*.bmp 和 recordings 临时目录。

早期编辑器录像因武器库暂停而截断，原失败保留。当前新包录像正常结束，但缺星弓、购买包全覆盖、四至六分钟内容和游戏内 0.25 倍慢放；这些是 NOT_RUN／PARTIAL，不从普通视频减速伪造。
