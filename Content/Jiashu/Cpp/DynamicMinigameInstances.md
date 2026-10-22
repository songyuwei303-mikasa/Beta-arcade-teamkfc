2026-10-21 正式接入更新：F/G 已完成，L_Main 使用格子确认进入，详见 [BoardCells.md](BoardCells.md)。下面的自动启动配置仅为历史 D 集成步骤。

# 动态小游戏实例（阶段 D）

后续状态：用户已完成人工双席 D 验收。现在 E 已接入宿主，自动创建画中画，默认无需新蓝图；本文原“UI 不显示/没有画中画”为 D 阶段历史边界。最新说明见 [MinigamePresentation.md](MinigamePresentation.md)。新增宿主 Closing 状态等待外框退出动画，再卸载；Session 结束时立即恢复棋盘输入。

用户已完成 B/C 独立小游戏实测。本文描述 D 的加载与卸载。画中画已由 E 接上，见文首；格子触发和归属仍未做。下文“不会出现画中画”是 D 刚接入、E 尚未接上时的观察说明。

## 当前能做什么

`ArcadeMinigameInstanceHost` 是放在 L_Main 的公共宿主 Actor。每个宿主一次管理一份实例，可反复使用；两个席位各放一个。它复用已有 Session/Entry，不运行子地图的测试 GameMode，不创建第二个 LocalPlayer，不改变主相机。

流程：

1. `Start Minigame` 接受请求；马上接管并清空该席棋盘输入，另一席不受影响。
2. `LoadLevelInstanceBySoftObjectPtr` 异步加载地图，使用独立的运行时 Level 和位置偏移。
3. 等待可见、Actor 已 BeginPlay，仅遍历这份 Level 的 Actors，要求恰好一个 Entry。
4. 创建原有 Session，执行 OnPrepare；按 Entry 的 `MinigameWidgetClass` 创建独立 Widget。
5. Entry MarkReady 后自动 Start，调用原有 OnStarted。GetMinigameWidget 可用，但 D 不 AddToViewport。
6. Finish / 系统异常取消后，Session 通知公共 OnSessionEnded 并恢复原输入；宿主下一 Tick 清除 Widget、申请卸载，广播 OnInstanceEnded。
7. 真正完成流式移除后，宿主回到 Idle，允许再进入。加载中、准备中、运行中、卸载中均拒绝重复进入。

加载或准备各有 TimeoutSeconds（默认 30 秒真实时间）。无效地图、入口数量错误、输入已被别的对象接管、超时等会清理并报告 LastError / OnLoadFailed。已有菜单接收权不会被抢走。没有玩家主动退出接口，Exit 不触发卸载。停止 PIE 或销毁宿主会清理所属实例。

`State` 是宿主状态，`ActiveRun.State` 是会话状态，二者不同。Idle 时 ActiveRun、ActiveEntry、StreamingLevel 会清空；需要保留成绩时保存结果结构，而不是反复读取宿主当前 Run。

## 你在编辑器里需要做的事

先保存并关闭 UE，再完成 C++ 完整构建。重新打开后：

### 1. 检查现有小游戏地图

当前样例位于 `/Game/Jiashu/MiniGames/TestMiniGame_WalkToGoal/L_WalkToGoalMiniGame`，无需重做。

- 保留唯一的 BP_WTGEntry、角色、起点、终点、相机及本地对象引用。
- 独立地图仍使用 ArcadeMinigameTestGameMode。被正式地图加载时，这个 Override 不会产生另一个 GameMode。
- 将只为检查通信而写在小游戏 Level Blueprint 中的 BeginPlay → Bind OnSessionEnded 调试链移除或断开。它订阅的是整个 World 的广播；双实例下每份地图都订阅，会产生重复打印，甚至读到另一个席位的结果。玩法使用 Entry.OnStopped 的本轮 Result；全局观察器放在 L_Main 中，只绑定一次。
- 玩法不要使用 GetAllActorsOfClass 找“第一个角色/目标”，继续使用 Entry 上的实例引用。地图复制时引擎会重映射这些本地图引用。
- 起点／终点使用引用对象的 GetActorLocation，而不是写死原点世界坐标，否则偏移后会传回原场景。
- 保持角色 Auto Possess Player / Auto Possess AI / Auto Receive Input 关闭。不要在 BeginPlay 调用 SetViewTarget、AddToViewport、OpenLevel 或开始正式玩法。
- 此版针对普通非 World Partition 的自包含小地图，不支持依赖额外流式子关卡的小游戏。

### 2. 在 L_Main 放置公共宿主

1. Content Browser 新建 Blueprint Class → All Classes 搜索 `ArcadeMinigameInstanceHost`，命名 `BP_MinigameInstanceHost`。它只是公共配置蓝图，不用写玩法事件。
2. 打开 L_Main，保持现有 Board GameMode、PlayerStart、角色类和主相机配置。
3. 将宿主蓝图拖入两次，命名 `MinigameHost_P1`、`MinigameHost_P2`。
4. P1 的 Details → Arcade / Instance：
   - Seat = 0。
   - Minigame Map = L_WalkToGoalMiniGame。
   - Instance Location = (100000, 0, 0)。
   - Start Automatically = 勾选，仅用于这次集成测试。
   - Timeout Seconds = 30。
5. P2 同样配置，Seat = 1，Instance Location = (100000, 20000, 0)，地图仍选同一个。
6. PIE Number of Players = 1，使用 Play，不用 Simulate。

为什么放得远：把两个小场景与棋盘、彼此的碰撞体分开。单位为厘米，上述值只是这个小样例的测试间距，不是任意小游戏的尺寸保证。宿主 Actor 自己的位置不决定实例位置；本版使用显式的 Instance Location，只有平移，没有旋转或缩放。

Start Automatically 在首个 Tick 执行，确保 Board GameMode 已建立输入接收对象。它不会自动重复开局。后续正式格子或按钮只需调用该宿主 `Start Minigame`；返回 true 仅代表请求接受，不能立即读取 ActiveRun，应等待实例进入 Running。

### 3. 当前应该观察什么

- 主窗口仍显示棋盘，**不会出现画中画，也不会显示小游戏计时 Widget**。这是阶段 D 的明确边界，不是地图未加载。
- 开启双宿主自动启动后，两名棋盘角色停下，操作路由到各自小游戏。
- Output Log 搜索 `LogArcadeInstance`：应分别出现 Seat 0/1 loading、prepared entry；入口路径应属于不同实例。
- PIE 中按 F8 退出角色控制，以编辑器方式查看 World Outliner：应有两份小游戏对象。选择各份角色／终点，按 F 聚焦，确认它们位于不同位置。
- 在 PIE 的 Outliner 选中运行中的宿主，Details 查看 State=Running、ActiveRun 非空。检查两个 Run 的 Seat 和 SessionId。
- F8 检查期间属于失焦／输入中断，不要期待角色继续移动；返回游戏视口后松开全部按键再操作。
- 结束时应出现原有 LogArcadeMinigame 结果日志，接着 instance unloaded；对应棋盘角色恢复可操作，另一份实例仍继续。
- 同一地图两份不会共享 Entry、角色或 Session。完成后宿主回到 Idle，可以再次调用 Start Minigame 创建新实例。

若希望在 L_Main 打印结果：Level Blueprint 只绑定一次 ArcadeMinigameSubsystem.OnSessionEnded，Break Result 打印 Seat、SessionId、Score。加载尚未产生 Session 就失败时不会伪造一条成绩；用宿主 OnLoadFailed 或 LastError 查看原因。

本阶段没有方便地“看着远处小游戏游玩”的正式显示路径。无需为了测试改变主相机或把两个场景塞进棋盘。代码自动化验证加载/卸载链；完整可视双人试玩将在 E 的画中画接入后完成。

## 资源隔离注意事项

- 同 World 不等于两个独立宇宙。DirectionalLight、SkyLight、全局后处理、声音和 World 级操作仍可能互相影响。
- Basic 地图自带的全局光照／天空 Actor 不应在正式双实例中重复启用。保留备份后，集成版小游戏应依赖公共环境，必要局部灯使用有限影响半径。画面风格和音频隔离将在 E/后续显示设计中验证。
- 动态 SpawnActor 的 Owner 应设置为本实例 Entry，让对象生成到它所在 Level；检查并使用 RegisterSpawnedActor。不要让临时对象默认留在 L_Main，误以为卸载子地图会自动销毁。
- UI 对象由宿主创建、Entry 更新、宿主释放。UI Construct 不应依赖它已经 AddToViewport。现有计时样例的 OnStarted → GetMinigameWidget 仍可用。
- 打包时在 Project Settings → Packaging → List of Maps to Include in a Packaged Build 中包含 L_Main 与小游戏地图；本次编辑器测试不等于打包验证。

## 下一阶段

E、格子入口、三连胜利和最简菜单都已实现。正式对局不要勾选 Start Automatically。

这次没有修改任何 .umap 或 .uasset；上述宿主摆放和小游戏调试 Level Blueprint 清理由用户在编辑器完成。

## 当前验证状态

2026-10-20：用户关闭编辑器后，UE 5.8 Editor Development 完整构建成功；整个 `Arcade.` 回归集 **8 项成功、0 项失败、0 项测试警告**。报告在 `Saved/Automation/DynamicInstances/index.json`，日志在 `Saved/Logs/DynamicInstances.log`。此前 DLL 占用导致的 LNK1104 已解决。代码与文档 diff 空白检查通过。

新增 `Arcade.Minigame.DynamicInstances` 已读取真实样例地图两次，验证独立 Level、位置偏移、Entry 本地图引用重映射；随后仅在测试内存中用原生 Entry 替换玩法 Entry，验证重复请求拒绝、双席并发、单边结束、输入恢复、卸载重开、加载中销毁、旧结果拒绝和后续接收者不被抢占。未修改地图资产，也不将此测试等同于蓝图/UI 实际试玩。仍需用户按上述步骤做 L_Main 编辑器联调；NullRHI 自动化不证明画面或 UMG 可视效果。

