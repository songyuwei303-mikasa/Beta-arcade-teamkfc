# C++ 公共系统文档

新接手项目时，请按以下顺序阅读：

1. [ProjectDevelopmentGuide.md](ProjectDevelopmentGuide.md)：全项目开发约定、当前实现状态、**§7.0 下一阶段实施顺序**、公共小游戏架构、团队协作与测试基线。
2. [InputSystem.md](InputSystem.md)：现有输入系统的设备映射、蓝图接口和测试面板用法。
3. [BoardMovement.md](BoardMovement.md)：正式棋盘移动、角色换模／动画／调参、卡顿修复记录。
4. [MinigameSession.md](MinigameSession.md)：已实现的阶段 A 会话节点、蓝图事件、输入交接、清理责任、临时验收与团队开发里程碑。
5. [FirstMinigameTutorial.md](FirstMinigameTutorial.md)：从创建蓝图开始，制作“走到终点并确认”的计时样例；包含 C 测试 GameMode 配置和 UI 接线。

此目录用于存放公共系统的接入说明与设计侧参考文档。Unreal 实际编译的 `.h` / `.cpp` 源码位于工程的 `Source/BetaArcadeTeamkfc` 模块中，不能放在 Content 下参与编译。

当前已实现：输入系统、菜单导航、输入测试工作台、正式双席位连续移动（已人工阶段验收），以及小游戏阶段 A 与 C 测试 GameMode（构建及七项自动化通过，蓝图/UI 联调待做）。**下一优先：用户按教程完成 B，联调 C**，随后 D 动态实例与 E 画中画。用户已确认小游戏中途不能主动退出，Exit 不接 Cancel。后续功能尚未实现，不能当成现成节点。
