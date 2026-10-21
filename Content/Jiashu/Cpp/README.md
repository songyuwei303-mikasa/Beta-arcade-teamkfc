# C++ 公共系统文档

E 已完成完整构建、9 项编辑器回归及 2 项真实渲染测试，均零测试警告。默认外框可以直接在 L_Main 测试；自定义 Widget/动画的创建步骤和样例补局部灯的说明见 [MinigamePresentation.md](MinigamePresentation.md)。

新接手项目时，请按以下顺序阅读：

1. [ProjectDevelopmentGuide.md](ProjectDevelopmentGuide.md)：全项目开发约定、当前实现状态、**§7.0 下一阶段实施顺序**、公共小游戏架构、团队协作与测试基线。
2. [InputSystem.md](InputSystem.md)：现有输入系统的设备映射、蓝图接口和测试面板用法。
3. [BoardMovement.md](BoardMovement.md)：正式棋盘移动、角色换模／动画／调参、卡顿修复记录。
4. [MinigameSession.md](MinigameSession.md)：已实现的阶段 A 会话节点、蓝图事件、输入交接、清理责任、临时验收与团队开发里程碑。
5. [FirstMinigameTutorial.md](FirstMinigameTutorial.md)：从创建蓝图开始，制作“走到终点并确认”的计时样例；包含 C 测试 GameMode 配置和 UI 接线。
6. [DynamicMinigameInstances.md](DynamicMinigameInstances.md)：D 动态实例宿主、L_Main 编辑器配置、双实例验证和当前显示边界。
7. [MinigamePresentation.md](MinigamePresentation.md)：E 画中画、角色内相机、Widget 外框设计、嵌套 UI 与开关动画。
8. [BoardCells.md](BoardCells.md)：F/G 格子字段、事件顺序、纪录比较。做格子蓝图时先读其中的 ArcadeBoardCell 说明。

此目录用于存放公共系统的接入说明与设计侧参考文档。Unreal 实际编译的 `.h` / `.cpp` 源码位于工程的 `Source/BetaArcadeTeamkfc` 模块中，不能放在 Content 下参与编译。

当前已实现：输入、双人移动、小游戏 A–E，以及 F/G 格子交互与单局纪录，见 BoardCells.md。正式总胜负 UI、选设备和最终外框美术尚未实现。小游戏中途不能主动退出，Exit 不接 Cancel。

2026-10-21：新增 [九宫格交互、小游戏池与纪录](BoardCells.md)。L_Main 已接入，正式流程不再使用 Start Automatically。

2026-10-21：[主菜单、对局与胜利结算接口](MatchFlow.md)。默认停留主菜单，需显式 StartNewRound；Row/Column 三连获胜。
