# 最终包第一人称逐帧峰值

1920x1080 paired final-scene renders, full effect lifetime at controlled30Hz simulation; duplicated hidden-FX control; threshold>=32 RGB levels. This is A diagnostic, not native-speed viewport capture. Native public MP4 is separate.

|效果|变化峰值%|对照噪声%|帧数|状态|
|---|---:|---:|---:|---|
|Arcane_muzzle|7.271|0.045|93|PASS|
|Arrow_charge|0.136|0.040|33|FAIL|
|Arrow_release|14.585|0.043|150|PASS|
|Bolt_charge|1.199|0.044|36|FAIL|
|Bolt_release|3.867|0.042|150|FAIL|
|CrimsonNight_charge|11.061|0.044|33|PASS|
|CrimsonNight_slash1|8.065|0.124|29|PASS|
|CrimsonNight_slash2|31.139|0.199|29|FAIL|
|CrimsonNight_slash3|8.037|0.262|29|PASS|
|CrimsonNight_wave|37.909|0.098|120|FAIL|
|Heal|12.621|0.042|242|PASS|
|Shield|3.596|0.044|242|PASS|
|Shock_release|11.996|0.052|150|PASS|
|Shock_warning|2.335|0.050|36|FAIL|
|StarTide_charge|11.899|0.057|33|PASS|
|StarTide_slash1|12.328|0.317|29|PASS|
|StarTide_slash2|23.270|0.590|29|FAIL|
|StarTide_slash3|10.590|1.007|29|FAIL|
|StarTide_wave|40.157|0.225|120|FAIL|
|Steamlock_muzzle|6.858|0.040|93|PASS|

11项数值PASS、9项FAIL。星潮第2/3斩对照噪声超过0.5%，仍FAIL。30Hz为实际最终包模拟采样，墙钟渲染低于30FPS，不能冒充30FPS正常主视口录制；独立原生录像另附。
