# Beta Arcade：项目开发约定与交接

**最新进展（2026-10-20，C 接入）：** `ArcadeMinigameTestGameMode` 已加入，独立地图可以自动准备／开始同一会话，并由测试环境创建入口配置的 UI。完整构建和七项自动化通过；B 的蓝图/UI 人工联调待用户完成，详见 [FirstMinigameTutorial.md](FirstMinigameTutorial.md)。当前剩余公共阶段为 D/E，不能把普通视口 UI 当成已验证的画中画。用户已明确：小游戏中途不可主动退出，玩法忽略 Exit；菜单可用 Exit 返回上级。Cancel 保留给异常清理，不作为玩法退出按钮。

更新日期：2026-10-20。最初整理时的代码基线：`6953205 InputSystem`；双席位输入与移动已由用户阶段验收。Sony DirectInput 按需 `EnumDevices` 保持不变（禁止恢复每秒枚举）。**阶段 A 与 C 独立测试壳已实现，完整构建与七项自动化通过；下一步由用户制作 B 并做蓝图/UI 联调**。节点与职责见 [MinigameSession.md](MinigameSession.md)，顺序见 §7.0；后续动态加载、画中画、归属尚未实现。

本文件供新对话、主程序和小游戏制作者交接使用。**必须区分“已实现”与“设计方向／待实现”；后文拟议接口不能被当作引擎里已经存在的节点。** 本文记录已有讨论，不代表现在授权实现所有后续功能。新任务仍以用户当次请求为范围。

## 1. 新接手者先读这些

1. 阅读本文，再阅读 [InputSystem.md](InputSystem.md)。后者是现有输入系统的具体接入说明。棋盘移动见 [BoardMovement.md](BoardMovement.md)。
2. 检查当前工作区、工程配置和源码。本文是交接快照；发现与代码不符时，明确指出差异，不要凭文档覆盖已有修改。
3. 双席位连续移动已阶段完成（复用现有输入，无第二 LocalPlayer／Possess）。小游戏阶段 A 已加入；下一步由用户做极简入口蓝图，公共层补薄测试 GameMode，复用同一会话；不要另造一套生命周期。
4. 正式玩法不能继承测试 Controller 的自动创建测试面板行为，否则面板会把两名玩家的输入接收对象重新绑定到自己。
5. 保留输入测试入口，确保新玩法开发后仍有独立的设备回归测试方式。运行中改 Seat／设备匹配的正式 UI 留到后续 UI 阶段。

项目核心约束：本地同机双人、无联网功能；三个抽象输入是二维方向、确认、退出；鼠标只用于共享 UI；两人可以同时独立游玩 3D 小游戏；小游戏以游戏内画中画展示，不创建 Windows 窗口。

## 2. 游戏与团队背景

玩家在九宫格地图上移动，在所在格子按确认进入该格子的小游戏。小游戏是单人游戏，但两位玩家的挑战可以同时进行。结束后依据成绩决定格子归属；另一位玩家可以挑战同一格，按该小游戏规则比较高分或短耗时。

以下规则尚不能擅自补全：平局、失败／退出是否保留旧纪录、对局总体胜利条件、双方同时结算同一格的处理细节。用户已确认类似胡闹厨房的自由连续移动与固定共享镜头。

团队由用户担任主程序及部分设计，另两名设计专业组员可以用蓝图制作小游戏。分工方向：

- C++ 公共系统：设备与席位管理、输入路由、小游戏会话与场景加载、公共 UI 容器、成绩提交与棋盘归属等跨小游戏机制。
- 蓝图小游戏：具体规则、场景对象、角色行为、镜头构图、局部 UI 与反馈。
- 设计侧应能在编辑器中调整时间、目标分数、速度、资源、颜色等配置，不必修改 C++。
- 公共系统提供简单、可读的蓝图入口和默认可运行实现，避免让组员了解底层设备或流式加载细节。

资料来源说明：玩法和协作方案来自本对话的用户说明及讨论；已读取命名规范。GDD 文件位置已知，但本次整理未读取其正文，不能声称本文完整反映 GDD。需要具体规则时再核对 `D:\Modules\Bata Arcade\GDD Beta Arcade Team KFC.docx`；这是本机参考路径，其他组员不应依赖它存在。

## 3. 当前工程与实现清单

工程：`BetaArcadeTeamkfc.uproject`，EngineAssociation 为 5.8，本机编译验证使用 UE 5.8.3。运行模块为 `BetaArcadeTeamkfc`。

路径均以工程根目录为基准：

- `Source/BetaArcadeTeamkfc/Public`：蓝图可见接口与公共类声明。
- `Source/BetaArcadeTeamkfc/Private`：实现、设备后端和自动化测试。
- `Content/Jiashu/Cpp`：技术文档。**此处不是 C++ 编译目录，.h/.cpp 应放在 Source 模块中。**
- `Content/Jiashu/Levels/L_Main.umap`：当前已存在的地图资源，后续正式开发先检查其实际内容与 World Settings。
- `Content/Jiashu/Levels/L_TestingLevel.umap`：独立输入测试地图。

2026-10-20 通过引擎重新只读加载核实：L_Main Override 为 `/Game/Jiashu/GameModeOverride/BP_ArcadeBoardGameMode`，有地面 StaticMeshActor、两个 PlayerStart、环境及 CameraActor；L_TestingLevel Override 为 ArcadeInputDemoGameMode，有 StaticMeshActor 和一个 PlayerStart。用户已完成移动实测。本次阶段 A 不修改任何地图或蓝图资产。

当前 `Config/DefaultEngine.ini` 的项目默认 GameMode 为 `/Script/BetaArcadeTeamkfc.ArcadeBoardGameMode`，GameDefaultMap 为 `/Game/Jiashu/Levels/L_Main`，分屏关闭。编辑器个人启动设置和各地图 Override 仍可能不同；测试地图应显式选择 ArcadeInputDemoGameMode。

### 已实现的源码入口

- `UArcadeInputSubsystem`：GameInstance Subsystem，两个逻辑席位、来源选择、弱引用接收对象、取消／中立保护、光标闲置控制。
- `UArcadeInputReceiver` / `IArcadeInputReceiver`：Actor、组件、Widget 可以实现的统一输入接口。
- `FArcadeSonyInput`：Windows 原生 Sony DirectInput 后端，按设备身份保留槽位；按需枚举（启动／断连／AutoAssign），已连接设备逐帧读取。禁止恢复为游戏线程每秒 EnumDevices。
- `ArcadeGamepadMath.h`：摇杆死区、十字键优先、POV 方向转换。
- `UArcadeMenuWidget`：显式按钮列表的双席位导航；鼠标和确认键执行相同的按钮 OnClicked。
- `AArcadeInputDemoGameMode`、`AArcadeInputDemoController`、`UArcadeInputDemoWidget`：临时输入工作台，不是正式游戏框架。
- `AArcadeBoardGameMode`、`AArcadeBoardController`、`AArcadeBoardCharacter`：正式双席位连续移动基础版；一个共享视角、按 Seat0/Seat1 PlayerStart 生成角色，无第二个 LocalPlayer 或 Possess，Controller 不创建测试 UI。地图与角色美术／AnimBP／移动调参见 BoardMovement.md；占位 Cube 可被蓝图隐藏，使用 Character 自带 Mesh。
- `AArcadeMinigameEntry`、`UArcadeMinigameSession`、`UArcadeMinigameSubsystem`：阶段 A。World 范围内两席独立会话；Run 实现 ArcadeInputReceiver 并向入口蓝图转发，准备／就绪／运行／结束分离，结果通知和输入恢复；不加载关卡或改变镜头。
- `ArcadeInputTests.cpp`：三个输入基础测试、角色移动/路由生命周期测试、地图只读核对，以及小游戏会话生命周期测试，共六项。

### 尚未实现

极简蓝图示例小游戏（B）、动态关卡实例加载（D）、画中画（E）、格子交互／归属、对局胜负、正式输入选择与断连 UI、成绩比较业务规则。阶段 A/C 已构建／自动化验收，仍需 B 做实际蓝图/UI 联调。

早期曾讨论 Enhanced Input，但**当前实现没有创建 IA/IMC 资源，实际是 Slate 键盘预处理 + XInput + Sony DirectInput**。不要把设想误写成现状。若以后换设备后端，保持玩法侧接口和已确认的行为。

## 4. 全项目架构原则

1. **输入来源、玩家席位、棋盘角色、小游戏操作对象分别管理。** 席位不随进入小游戏或设备更换而改变。
2. **依赖公共接口，不依赖具体小游戏类。** 棋盘不应硬编码某个投篮或赛车蓝图；小游戏不应 Cast 到棋盘来修改格子。
3. **配置与逻辑分离。** 时间、分数比较规则、资源引用等应通过可编辑配置传入；具体配置资产类型尚待实现阶段选择。
4. **每次挑战单独保存运行状态。** 分数、计时、相机、渲染目标、UI、动态对象均归属于本次实例／会话，不能因使用同一蓝图类而共享可变状态。
5. **初始化与正式开始分开。** BeginPlay 不等于挑战开始，加载、转场和输入就绪之后才启动正式计时。
6. **退出与异常路径也有明确生命周期。** 取消、断连、加载失败、重复结束、过期回调和清理必须有定义。
7. **默认交付可以试玩的小步版本。** 把底层能力做成完整的小流程，再扩展内容；不能用“编译通过”代替玩法验证。
8. **本地双人不引入网络架构。** 不因出现“多人”二字而添加服务器、RPC、Replication 或网络 PIE。

## 5. 现有输入系统的稳定约定

### 5.1 席位与设备

逻辑席位固定使用 `Seat=0` 表示 P1，`Seat=1` 表示 P2。**这不是 UE LocalPlayer 索引或 PlayerController 索引的承诺。** 当前一个 GameInstance 内维护两席，PIE Number of Players 保持 1，不启用自动分屏。

键盘的两套按键是同一物理键盘上的逻辑来源，不是识别两块键盘。鼠标不占席位，只点击 UI。

默认绑定：

- Keyboard1：WASD → 二维方向；Space → 确认；Escape → 退出。
- Keyboard2：方向键 → 二维方向；Enter 或 NumPad0 → 确认；Delete → 退出。
- Xbox／XInput：左摇杆或十字键 → 方向；A 或 X → 确认；B → 退出。
- PlayStation：左摇杆或十字键 → 方向；叉或方块 → 确认；圆圈 → 退出。

方向 X 向右为正、Y 向上为正，向量长度不超过 1。十字键非零时优先，松开后恢复摇杆；不是把两个来源简单相加。摇杆默认径向死区 0.2，键盘和十字键斜向归一化。

自动分配当前排序为：已连接 XInput 槽位 → 已连接 Sony 槽位 → Keyboard1 → Keyboard2，前两项分配给两席。运行中新设备不抢占已分配席位；显式调用 `AutoAssignSeats` 或未来选择界面调用 `AssignSeat` 才改变分配。

物理断连取消输入并保留来源。XInput 槽位不是永久设备身份；Sony 的 USB／蓝牙切换也可能改变设备身份，需要重新分配。Sony 后端当前在本 GameInstance 中最多保留四个设备身份，不应把它描述成无限设备池。

### 5.2 玩法侧接口

通过 `Get Game Instance Subsystem` 获取 `ArcadeInputSubsystem`。目标对象实现 `ArcadeInputReceiver`，再调用 `SetInputReceiver(Seat, Receiver)`。

- `ArcadeDirection(int32 Seat, FVector2D Direction)`：值改变时发送，释放会发送零向量；**不是每帧调用**。持续运动应缓存输入，在运动更新中使用。
- `ArcadeButton(int32 Seat, EArcadeInputAction Action, bool bPressed)`：Action 为 Confirm 或 Exit；按下和松开都可收到。多个确认键按住状态合并，只有全部松开才变为释放。
- `ArcadeInputCancelled(int32 Seat)`：失焦、断连、切换等取消路径。接收者必须清零方向、停止蓄力和相应持续行为；取消不是“正常松开后发射”。

`SetInputReceiver` 会先取消旧目标，要求该席位所有方向和按钮回到中立之后才继续投递。防止打开小游戏的确认同时触发游戏操作。接收对象为弱引用；销毁后不会保持其存活。空 Receiver 停止事件投递。

每席当前只有一个接收对象，没有内置接收栈。弹窗打开／关闭时，由调用方保存并恢复目标。不要同时由 UI 和角色各自读取物理按键。

`GetSeatState` 是路由层保存的席位状态，不应被当作未经处理的硬件实时采样流；在失焦或等待中立时，它与物理按住状态可能不同。连接／分配通知通过 `OnSeatChanged`。同一设备不能由 `AssignSeat` 分给两人；交换设备先解除占用。

### 5.3 UI 与焦点

鼠标事件继续交给原生 UMG，鼠标不发送玩法输入。默认静止 3 秒隐藏光标，移动、点击、滚轮恢复显示。正式 Controller 需要承接测试 Controller 当前设置的 `GameAndUI`、不锁鼠标、捕获时不自动隐藏光标等行为。

`UArcadeMenuWidget` 使用 `SetNavigationButtons` 提供的有序按钮列表，支持双席位独立选择和按住重复；它不是任意 UI 的自动空间导航系统。文本框、滑条、复杂网格导航需要另行实现。

测试面板收到确认后点击选中按钮属于正确的输入消费，不是 UMG 抢走了键盘2的确认。面板的“仅输入测试”开关只是诊断选项。正式玩法切换接收对象后，不应让后台菜单继续响应。

输入只在本游戏视口拥有焦点时投递。失焦取消；PIE 中 Shift+F1 可释放焦点。不要删除这些保护来掩盖移动卡住或重复点击问题。

### 5.4 平台支持边界

Windows XInput + 原生 Sony DirectInput 已实现。Sony 当前识别 DualShock 4（05C4/09CC，含 0BA0 适配器）、DualSense（0CE6）、DualSense Edge（0DF2），通过 Windows 标准 HID 布局映射 USB／蓝牙。

用户已经实测原有输入符合预期，并确认蓝牙 PS4 DualShock 4 能正常接收。PS5、其它连接组合和第三方型号尚不能写成已实机验证。PS3、Switch 和未知第三方协议没有保证支持。

使用虚拟 Xbox 映射软件且原始 Sony 设备未隐藏时，可能重复枚举同一手柄。增强报告模式或非标准驱动也可能需要适配。设备兼容性问题应优先修复来源后端，不把硬件差异泄漏到小游戏蓝图。

## 6. 正式 Level、GameMode 与移动接入约束

以下保留接入约束；用户已确认自由连续移动与固定共享镜头，并已完成双人移动实测。配置说明见 [BoardMovement.md](BoardMovement.md)：

1. 检查 `L_Main` 的现有内容和 GameMode Override，保留用户搭建的场景。决定正式 GameMode／Controller 的类与资产，并保留单独的输入测试入口。
2. 创建或定位两名棋盘角色，明确席位与角色的映射。不要假设 `Get Player Character 0/1` 已代表它们。
3. 每名角色或其输入适配组件实现现有接口；由正式启动流程将 Seat 0/1 分别绑定到对应目标。
4. 将二维输入映射到棋盘平面或相机相对方向；移动速度、加速度等暴露给蓝图。角色收到取消事件必须停止运动。
5. 用户已确认自由连续移动与固定共享镜头；当前边界使用场景碰撞体，没有格点吸附。
6. 当前 Demo GameMode 的 DefaultPawnClass 为 nullptr，Demo Controller 会自动创建并绑定输入面板。正式 GameMode 应自己明确生成和管理角色，不照搬 Demo 的启动副作用。
7. 如果选择 Character/Pawn，需要明确 Controller／Possess 方案。不能让一个 PlayerController 同时 Possess 两个 Pawn；也不能为了套用默认输入就未经评估创建第二个 LocalPlayer。可以设计角色输入适配层或独立控制器，确保两席继续走现有接口。
8. 公共输入 Subsystem 与 GameInstance 同寿命。场景切换／接收对象销毁时清理或重新绑定，避免依赖上一个 Level 的弱引用。
9. 初次接入先保持设备分配策略，未来“选择输入”页面再扩展。不要在普通 Tick 中反复 AutoAssignSeats。

首个验收目标：同一游戏窗口中，两位玩家独立移动；一人操作菜单时不控制另一人；取消／失焦不会留下持续移动；鼠标只操作 UI。

## 7. 公共小游戏方案：阶段 A 已实现，后续阶段待做

### 7.0 下一阶段实施顺序（先做“门”，再做“屋子”）

用户已确认：双席位移动阶段完成；运行中改设备／Seat 匹配留到正式 UI。小游戏相关系统互相依赖，**禁止**一上来做完整小游戏 GameMode、画中画或格子归属。正式流程中子关卡**不会**启动自己的 GameMode；测试 GameMode 只是调用同一套生命周期的薄壳。

| 阶段 | 内容 | 验收要点 | 本阶段不做 |
| --- | --- | --- | --- |
| **A（已实现，待蓝图联调）** | C++：入口基类 + 双席会话 + 输入交接。实际节点和清理职责见 MinigameSession.md。结果仅通知，不决定归属。 | 构建、自动化已通过；手动在现有场景用测试按钮启动，验证入口事件与输入返回。 | 画中画、关卡实例、格子归属、选设备 UI、复杂示例玩法 |
| **B** | 极简蓝图示例小游戏（独立小地图 + 入口子类；例如按住确认计时或走到目标）。只用现有 `ArcadeInputReceiver`。 | 一人完整打完并提交结果。 | 精美术、双套玩法代码 |
| **C（源码已加入，待 B 联调）** | 小游戏**测试** GameMode：打开小游戏地图 Play 时创建测试会话、绑输入、调用与正式相同的入口生命周期；画面可先全屏／简单显示。 | 设计侧不进棋盘也能单独调小游戏。 | 与正式两套生命周期 |
| **D** | 动态关卡实例加载／卸载、偏移、本实例内找入口；同图双实例。 | 两人同时各跑一份，不串引用、不串输入。 | 未定案的比分业务硬编码 |
| **E** | SceneCapture → 每会话 RT → UMG 容器；独立测试与正式同一拍摄路径。 | 主屏保持棋盘共享相机；小游戏不抢主 View Target。 | — |
| **F → G → H** | 棋盘格子开门 → 成绩／归属（规则先与用户确认）→ 正式 UI（含 Seat／设备）。 | 从棋盘进入再返回；归属符合已确认规则。 | 断连全局暂停等未定案方案 |

实施评估：顺序总体合理，但 B 与 C 应紧密联调，避免示例长期依赖临时启动逻辑。当前依赖：输入 ✅ → 棋盘移动 ✅ → **A 源码／自动化 ✅** → **B 极简蓝图＋C 测试壳联调** → D 同图双实例 → E 统一拍摄 → F／G／H。

用户现在即可制作独立普通小地图的极简入口蓝图，使用已加入的 C 测试 GameMode 直接 Play；组员可在 B/C 人工联调通过后开始玩法原型。稳定团队模板还须 D 和 E 两个公共阶段验证，再分发经过双实例／画中画验证的样例。不必等待格子归属、完整菜单或最终美术才开始小游戏玩法。

**阶段 A 的范围约束（已落实；后续代理继续遵守公共边界）：**

1. 先读本文 §4、§5、§7.0–7.4 与 InputSystem.md、BoardMovement.md「将某一席位暂交给菜单」。
2. 阶段 A/C 已按用户任务推进；下一步先完成 B 人工联调，不自动连做 D–H。
3. 复用 `UArcadeInputSubsystem`／`IArcadeInputReceiver`／`AArcadeBoardGameMode::RestoreBoardInput`／`GetSeatCharacter`；不引入第二 LocalPlayer、不默认 Possess 双角色、不改回每秒 `EnumDevices`。
4. 当前由 Run 实现 `ArcadeInputReceiver`，再转发入口事件；入口蓝图通过本实例引用控制操作对象。棋盘角色保留，不默认 Possess，不让具体玩法管理物理设备。
5. 类名／节点名选定后写入专项文档（建议新建 `MinigameSession.md`），并更新本文「已实现／尚未实现」；拟议旧名在落地前不可当作已存在。
6. 修改公共 C++ 后完整编译（关编辑器或 `Build.bat`），跑受影响的 `Arcade.` 自动化，并说明哪些仅编译通过、哪些需人工试玩。
7. 平局、失败是否占格、断连是否整局暂停等**业务规则未定案**，阶段 A 只提交结果给公共回调／日志，不写死棋盘归属。

### 7.1 独立地图制作，动态实例化运行

每个小游戏由组员在独立普通小地图中制作。正式游戏使用动态关卡实例加载，例如底层 `ULevelStreamingDynamic`／`Load Level Instance (by Object Reference)`，将地图实例放入分配的场景区域。

两人同时挑战同一个小游戏时，加载两份实例，分别拥有 Actor 状态、分数、计时、相机、渲染目标和 UI。资源可以共享，运行状态不能共享。

这仍是**同一个 World 中的多个关卡实例**，并非多个完全隔离的游戏世界。它解决协作、复用、加载和卸载管理，不会自动隔离 GameMode、全局时间、灯光、音频或对象查询。

组员不必把自己的场景复制粘贴进棋盘地图，也不应为了开始小游戏调用 OpenLevel 替换整个游戏。初版倾向普通非 World Partition 小地图；不是将玩法打包成面向静态网格优化的 Packed Level Blueprint。

### 7.2 GameMode 分工

计划提供正式游戏 GameMode 与公共小游戏测试 GameMode。后者用于直接打开小游戏地图按 Play 时创建测试上下文、接入输入和展示界面。

小游戏地图作为子关卡流入正式游戏时，不会启动自己的 GameMode，也不会接管主世界。具体计分、目标和局部规则放在小游戏入口蓝图中，不放在小游戏自定义 GameMode 里。

测试与正式运行由不同启动环境调用**同一套小游戏生命周期**，不能为两个环境分别写两份玩法。

### 7.3 小游戏入口与生命周期

已提供 `AArcadeMinigameEntry` C++ 基类，组员创建蓝图子类作为地图中的唯一入口。阶段 A 中操作对象由入口蓝图自行通过实例引用管理；公共相机／UI 配置留到后续阶段。

已实现生命周期：PrepareSession → OnPrepare → MarkReady → Start／OnStarted → Running → Finish 或 Cancel → OnStopped／OnCleanup → Ended。阶段 A 的蓝图节点以 MinigameSession.md 为准；**Pause、Resume、Finish Minigame 等旧拟议名称不是现有节点**。

准备阶段完成所需初始化，再显式 Start；阶段 D/E 将接入加载和显示准备。本阶段 Start 开始计时并要求输入再次回到中立，不自动等待硬件中立才计时。每次挑战使用独立 Run 和 SessionId，结束后拒绝重复／迟到提交；加载失败尚待 D 实现。

小游戏只提交完成状态、分数、耗时等必要结果；公共系统统一比较成绩并更新棋盘。小游戏不直接占领格子，也不查询另一位玩家的小游戏。平局、失败、取消和同时结束的业务规则先确认再实现。

### 7.4 角色与输入

倾向让棋盘角色留在原格并显示“正在挑战”，小游戏使用自己的 Pawn、角色或操作 Actor，而不是把同一个棋盘角色传送过去承担所有玩法。投篮、赛车、准星控制不需要同一种角色结构。

组员通过传入的上下文和明确引用获取本次玩家／操作对象，不写死 PlayerController 0，不使用 Auto Receive Input 去抢全局玩家输入。需要 Possess 的小游戏由公共适配功能安排，不让每个小游戏自行管理玩家席位。

### 7.5 画中画与镜头

计划用公共相机模板承接组员配置的位置、旋转、FOV 和跟随行为；通过 SceneCapture2D → 每会话独立 Render Target → 对应 UMG 容器展示。Render Target 只负责画面，输入继续由席位路由投递。

组员指定／切换小游戏相机，不调用 Set View Target 去接管共享主屏幕。CameraComponent 的设置不会自动全部同步给 Scene Capture；框架要明确支持和同步的参数。

独立测试也使用相同的拍摄与 UI 路径，只放大容器展示，避免测试时普通相机正常而正式画中画曝光／效果不同。16:9 曾作为固定画面比例的建议，**尚未最终确定**；确认比例后保持构图，不随意拉伸。

小游戏 UI 由框架加入对应玩家容器，不自行 Add to Viewport 覆盖整个屏幕。组员可制作自己的 Widget Blueprint，公共框架提供时间、结果等共通显示能力。

两个小游戏加棋盘会增加渲染负担，需要测量分辨率、更新频率和目标帧率；目前没有已经验证的性能保证。

### 7.6 同一 World 的隔离与清理约定

- 使用本实例出生点或相对 Transform，不写死生成到世界原点。动态加载偏移后仍应正常运行。
- 使用实例内明确引用；避免 Get All Actors of Class 后取第一个，因为可能拿到另一个挑战的对象。
- 动态生成对象、计时器、事件绑定、声音和效果应登记到本会话；结束统一清理。不能假设卸载关卡会清除所有后续生成的对象。
- 不擅自全局暂停、全局时间缩放、切换主地图或修改全局可变状态。
- Directional Light、Sky Light、无限范围后处理和全局音效不会天然只影响自己的实例。初版倾向由公共环境管理全局效果，小游戏使用局部效果。
- 把场景移到远处不自动解决音频监听问题。声音播放与监听方案须由公共层安排。
- 暂停不等于只停止一个计时变量。若需要局部暂停，物理、Tick、动画与生成任务等也要有清晰策略。

设备断连后“暂停整场对局、提示重新认领并倒计时恢复”曾被提出作为公平性方案，但**尚未定案或实现**。现有输入系统只负责取消和通知，不会自动暂停游戏。

## 8. 团队协作与交付约定

以下是沿用本项目低耦合目标的协作方式，具体文件夹名可在分工时确定：

- 每个小游戏有独立目录、地图、入口蓝图、UI 和资源；每次任务明确负责人和修改范围。
- 公共 C++ 模块／模板／输入约定由主程序维护。组员发现公共接口不足时反馈，不在自己的小游戏里复制另一套设备或棋盘系统。
- `.umap`、`.uasset` 是二进制资源，避免两人同时编辑同一地图／蓝图。多个独立小游戏地图不会因为加载到同一 World 就必须合并成一个资产。
- Git 合并不覆盖别人未提交的资源；需要迁移时带上依赖并检查引用。不复制 Saved、Intermediate、DerivedDataCache 或本机 Binaries 代替源码交付。
- 后续应提供小游戏模板和接入示例，再让组员据此开发。不能承诺任意已经完成的单机原型都能零改动接入。
- 每个小游戏交付至少包括：入口和地图、玩法／操作说明、可调整参数、结果指标及优劣方向、独立测试方式、双实例测试结果、已知限制。
- 接口变化说明新增／删除的节点、需要的迁移步骤和默认值，并同步维护本文与专项文档。

目标工作流：复制模板 → 独立地图搭建 → 连接抽象输入 → 设置本实例相机／操作对象 → 提交结果 → 独立 Play → 双实例集成测试。

## 9. 命名与可维护性

已提供命名规范中的前缀：Blueprint `BP`，Blueprint Interface `BPI`，Actor Component `AC`，Widget Blueprint `WBP`，Material `M`，Material Instance `MI`，Structure `F`，Enum `E`，Data Table `DT`。资产沿用项目常见下划线形式，例如 `BP_...`、`WBP_...`；C++ 类保持 Unreal 的 A/U/F/E/I 类型前缀和现有风格。

原始参考为 `D:\Modules\Bata Arcade\NamingConventions.txt`。它没有定义所有未来资产类型的前缀；未规定的部分先保持工程一致，不声称已有团队标准。

公共蓝图接口需要清晰 Category、参数含义、默认值和取消语义。将复杂逻辑留在公共层，将调参和表现选择暴露给设计侧。不要把文档中的示例类名误当作已存在的资源。

## 10. 必要测试与证据

### 已有验证基线

之前已在 UE 5.8.3 上编译通过以下测试：

- `Arcade.Input.KeyboardMappings`
- `Arcade.Input.SeatIsolationAndReconnect`
- `Arcade.Input.DPadAndStick`

历史测试报告在 `Saved/Automation/Input`，日志在 `Saved/Logs/InputAutomation.log`。实际设备识别日志在 `Saved/Logs/InputSonySmoke.log`，截图在 `Saved/Screenshots/WindowsEditor/ScreenShot00002.png`。Saved 可能被清理或不进入 Git，缺失不等于测试从未执行，也不等于当前版本已通过；代码修改后应重新产生相应证据。

用户已确认输入初版、PS4 蓝牙输入和双人移动阶段测试通过，不扩大为所有硬件均已验证。10-19 移动接入的五项自动化报告见 `Saved/Automation/Board`。10-20 阶段 A 完整构建通过，`Arcade.` 六项全部 Success、零测试警告；报告见 `Saved/Automation/Minigame`，日志见 `Saved/Logs/MinigameAutomation.log`。新增会话测试覆盖输入交接、两席隔离、中立保护、重复／迟到回调、入口销毁、资源清理与 World 退出。小游戏蓝图联调及显示路径仍待 B/C/D/E 验收。

### 修改公共 C++ 后

- 使用匹配的 UE 5.8 工具链编译，修复实际编译错误。结构／反射接口变化应在保存并关闭编辑器后完整构建，不依赖热重载证明正确。
- 运行受影响的自动化测试；修改输入或正式输入接入时运行 `Arcade.Input` 回归集。
- 核实测试具体 Result，不能仅凭进程退出码 0 或“队列执行完毕”认定全部成功。
- 测试断连、取消、焦点和对象切换等真实边界，而不只验证和实现完全相同的算式。
- 交付说明区分：编译通过、自动化通过、实际运行、人工／硬件测试。没有执行的检查明确标记。

本机参考命令（其他组员替换引擎和工程绝对路径）：

```powershell
& 'E:\UE\UE_5.8\Engine\Build\BatchFiles\Build.bat' BetaArcadeTeamkfcEditor Win64 Development '-Project=F:\UnrealProject\Beta-arcade-teamkfc\BetaArcadeTeamkfc.uproject' -WaitMutex -NoHotReloadFromIDE
```

可在编辑器 Session Frontend / Automation 搜索 `Arcade.Input`，或用 UnrealEditor-Cmd 的 `-ExecCmds="Automation RunTests Arcade.Input"`、`-TestExit="Automation Test Queue Empty"` 运行。无界面／NullRHI 测试不证明实际 UI、焦点或硬件操作正确。

### 正式移动接入的最低人工验收

- 无手柄、一个手柄、两个手柄；两套键盘同时操作；Xbox 与 PS 混合。没有相应硬件时记录未测。
- 两人同向、反向和斜向移动互不干扰；斜向不额外加速，摇杆死区正常。
- 十字键覆盖摇杆、松开恢复；确认键重叠按住不会提前释放。
- 切换到菜单后只导航菜单，返回后角色不保留旧方向；进入操作的确认不贯穿到新对象。
- Alt+Tab、PIE 失焦、拔插设备、销毁／更换接收对象后不持续移动或错误发射。
- 鼠标可点击 UI、闲置隐藏及恢复，不移动角色；退出键不误关闭编辑器测试。

### 公共小游戏框架实施后的最低验收

- 同一小游戏加载两份，同时运行和计分，不交叉引用或串输入。
- 一人进入／退出／完成，另一人继续；加载失败和取消不影响另一实例。
- 同一格两人结算、重复提交、退出后迟到回调按确定规则处理。
- 多次进入／退出不残留 Actor、计时器、事件绑定、声音、Widget 或渲染目标。
- 原点独立测试和偏移后的实例均可运行；独立测试与画中画构图／比例一致。
- 棋盘＋两个真实小游戏同时运行时验证帧率和资源占用，并检查灯光、音频和后处理串扰。
