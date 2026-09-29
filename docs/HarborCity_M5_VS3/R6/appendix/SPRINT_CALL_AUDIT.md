# 冲刺调用审计

路径均相对 HarborCity/Source/HarborCity。行号以 SOURCE_CHANGES.patch 为准。

|文件 / 函数|R5 触发条件|R6|
|---|---|---|
|M4/HCM4CombatComponent.cpp / StartPunch|地面移动出拳 SetSprinting(false)，0.8 倍跑速|移除清除，冲刺限速 0.85 倍冲刺速度|
|M5VS3/HCM5VS3Abilities.cpp / PressSword|地面按下蓄力即清除|移除，长按期间独立限速 200|
|同上 / StartSwing|地面挥剑清除|移除，命中窗口结束恢复|
|同上 / CastSpell|任何施法清除|移除，施法期间限速 400|
|M5VS3/HCM5VS3Bow.cpp / PressBow|星弓与魔导枪左键共用入口，按下即清除|移除；星弓拉弓 400，魔导枪快速腰射 650，长按 200|
|M1/HCM1PlayerController.cpp / SprintPressed|施法或挥剑时 BlocksRunJump 提前返回|移除该拒绝，允许动作期间改变冲刺输入|
|M4/HCM4CombatComponent.cpp / FirePistol|本身不清除，无独立降速|保持射击规则，移除共用 PressBow 的副作用|
|同上 / SetAimHeld|不清除，无独立限速|统一速度计算中 ADS 限速 400|
|同上 / RequestReload|清除 ADS，不清冲刺|统一速度计算中装弹限速 400|
|同上 / RequestToggleWeapon|清除 ADS，ResetTransient 恢复倍率|仍保留冲刺输入|
|M1/HCM1Character.cpp / SetCombatMovementScale|按当前冲刺标志乘倍率|非冲刺沿用；冲刺由动作状态限速|
|M1/HCM1PlayerController.cpp / SprintReleased|冲刺键释放|保留|
|同上 / ClearGameplayInput|失焦、暂停、对话等清空输入|保留安全释放|
|M1/HCM1Character.cpp / SetSeated|上下车|保留模式释放|
|M5VS2/HCM5VS2FlightComponent.cpp / 模式切换|地面与飞行转换|保留，飞行有独立 BoostHeld|

旧测试导演 HCM5VS2HeroExerciseDirector、HCM5VS2HairReviewDirector 也设置冲刺，只控制测试场景；不属于玩家攻击路径，未修改。
