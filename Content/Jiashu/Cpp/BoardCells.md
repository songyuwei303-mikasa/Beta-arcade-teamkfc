2026-10-21 流程更新：现已增加主菜单/对局/结算状态和三连胜利。BeginPlay 不再开始对局，改由 UI 调用 StartNewRound。格子必须配置 Row/Column；最新接入见 [MatchFlow.md](MatchFlow.md)，下文早期自动初始化说明已由它替代。

# 九宫格交互、小游戏池与纪录（F/G）

2026-10-21：本轮已实现并接入 L_Main。两名角色分别站进格子的 InteractionVolume 后按确认，复用已有 Host → Session → Entry → Presentation。成功完成后返回棋盘并比较该格纪录；没有玩家主动退出小游戏功能。对局总胜负与正式 UI 不在本次范围。

## 已接入的主地图

- L_Main 保留原有场景、GameMode、角色蓝图、相机和两个宿主配置；两个宿主实例的 Start Automatically 已关闭。
- 新增 BoardRound 管理器、BoardCell_1_1 到 BoardCell_3_3 九个原生格子。每格有附着的 BoardTile 占位薄方块，无阻挡碰撞；原地面承载角色。以后可移除占位方块并换成格子蓝图。
- `/Game/Jiashu/Board/DA_WalkToGoal`：现有小游戏地图；Metric=ElapsedSeconds、Direction=LowerWins、Tags=Movement。
- `/Game/Jiashu/Board/DA_DefaultPool`：目前只含上面的游戏。九格共享该池，因此本局九格相同，但纪录各自独立。
- 示例布局间距 310 cm，交互 Box 半范围 (140,140,120)，中心位于地板上方。可在 Details 修改碰撞范围和格子位置；占位方块随父格移动。

直接打开 L_Main Play（Number of Players=1）。未连手柄时 P1 用 WASD/Space，P2 用方向键/Enter。进入时确认键需松开后再操作小游戏；返回也保留输入中立保护。两人可以同时挑战同一格。

## 给蓝图作者的类

**ArcadeBoardCell**：创建蓝图子类，例如 BP_ArcadeBoardCell，自行添加静态模型、骨骼、灯光、Widget、Niagara。C++ 不创建美术网格，不假定它必须是地板或街机。保留 InteractionVolume；它只响应 Pawn overlap，不阻挡移动。

下面按当前源码说明这个类。蓝图里看到的名字可能去掉 `b` 前缀并加空格，例如 `bNewRecord` 显示为 **New Record**。它是布尔值，不是成绩数字。

### 蓝图可以读的字段

这些字段在 Details 和蓝图 Get 节点里可见。除 `Pool` 外，都不要在蓝图里写入。

| 字段 | 含义 | 何时变化 |
| --- | --- | --- |
| `InteractionVolume` | 交互盒。只与 Pawn 重叠，不挡移动。 | 构造时创建。范围可在编辑器改。 |
| `Pool` | 这一格使用的小游戏池资产。编辑器指定。 | 运行中分配逻辑不改它。 |
| `AssignedGame` | 本局抽到的游戏定义，用来显示名字、图标、标签。 | 每局 `Assign` 时写入。开局前可能为空。 |
| `OwnerSeat` | 当前主人。`-1` 无人，`0` 是 P1，`1` 是 P2。 | 新局重置为 `-1`。只有“严格更优的成功成绩”才会改成该席。 |
| `bHasRecord` / Has Record | 本局是否已有合法纪录。 | 与 `OwnerSeat` 一起重置、一起在占格或刷新纪录时变为 true。 |
| `BestValue` | 当前纪录的比较数字。Score 规则下是提交分；ElapsedSeconds 规则下是会话耗时。 | 新局写成 `0`。只有打破或建立纪录时改成这一轮的比较值。 |

比较用哪一个数字、越大越好还是越小越好，在开局时从定义复制进格子内部，蓝图读不到这两个副本。之后改 Data Asset 不会改已经开始的这一局。

`BestValue == 0` 不能表示“没有纪录”：新局的空值就是 0，而 0 分、负分也可以是真实成绩。

### 蓝图可以调用的函数

- `Contains Character(Character)`：角色胶囊是否与本格交互盒重叠。纯查询，不改状态。
- `Try Interact(Character)`：请求这一席开始挑战。角色按确认时，公共角色代码会自己找最近的重叠格子并调用它。蓝图也可以调，但远距离、错误席位、宿主不空闲、没有本局分配时会返回 false，不会绕过检查。返回 true 只表示加载请求已接受。

`Assign` 和 `Resolve` 不是蓝图节点。分配和结算由 `ArcadeBoardRound` 调用。

### 事件生命周期

一次正常流程：

1. 关卡 BeginPlay 后，管理器 `Start New Round`。它先对每格 `Assign`：写入 `AssignedGame`，并把 `OwnerSeat=-1`、`HasRecord=false`、`BestValue=0`。然后才调用 **On Round Assigned**。这个事件里读到的已经是新局的空纪录。它**不会**再发 On Owner Changed。
2. 角色在重叠区内按下确认，且 `Try Interact` 成功：先记下“这一席正在挑战这一格”，再请求宿主加载。加载请求接受后调用 **On Challenge Started(Seat)**。此时小游戏还在加载，正式计时还没开始。
3. 小游戏会话结束（成功、失败或取消，且这次结果属于这一席当前挑战）时，管理器调用结算。结算函数先改字段，再发事件。外框关闭动画还可能继续播；输入恢复不依赖这两个蓝图事件。
4. 加载失败、宿主丢失、还没有会话结果时，发 **On Challenge Error(Seat, Message)**，不改纪录。若会话已经先发出取消结果，走的是 On Challenge Resolved，不是这个错误事件。

**On Challenge Resolved(Result, bNewRecord)** 每次有结算结果都会执行，包括没破纪录、失败和取消。调用时字段已经改完。

- `Result` 始终是**这一轮**挑战的结果。`Result.Score` 是小游戏提交分，`Result.Elapsed Seconds` 是从会话 Start 起的耗时。失败或取消时这些数字仍是这一轮的，不是格子上的最高纪录。
- `bNewRecord` 为 true：这一轮建立或刷新了纪录。此时 `BestValue` 已经等于本轮用来比较的那个数（Score 或 ElapsedSeconds，由本局规则决定），`HasRecord` 为 true，`OwnerSeat` 已是这一轮的席位。
- `bNewRecord` 为 false：这一轮没有写入纪录。`BestValue` 仍是原来的最高纪录；若本局还从没占格，它仍是 `0`，`OwnerSeat` 仍是 `-1`。不要把 `Result` 里的分数当成 `BestValue`。

**On Owner Changed(Previous Seat, New Seat)** 只在主人席位数字真正变化时执行，而且排在 On Challenge Resolved **之前**。两种情况会变：

- 本局第一次合法成绩：`Previous Seat` 为 `-1`，`New Seat` 为完成者。
- 另一席严格更优、抢走格子：`Previous Seat` 为原主人，`New Seat` 为新主人。

同一人再次打破自己的纪录时，`OwnerSeat` 不变，所以**只**执行 On Challenge Resolved（`bNewRecord` 为 true），不执行 On Owner Changed。平局、更差、失败、取消也不执行 On Owner Changed。

因此，第一次成功占格时两个事件都会执行，顺序固定为：

1. 写入 `BestValue`、`HasRecord`、`OwnerSeat`
2. On Owner Changed
3. On Challenge Resolved（`bNewRecord` 为 true）

### 用 OwnerSeat = -1 判断有没有纪录

可以。按现在的实现，`OwnerSeat == -1` 和 `Has Record == false` 始终一起出现：只有建立或刷新纪录时才会把主人从 `-1` 改成 0 或 1，而且第一次只要是合法完成（Completed + Submitted，成绩是有限数）就会占格，不管分数多差，包括 0 和负数。本局之后不会自动变回无人。

不要把它理解成“只要打完一次就占格”。失败、取消、无穷大成绩、加载失败都不会占格，`OwnerSeat` 保持 `-1`。新的一局开始时会重新变成 `-1`，那是清局，不是丢纪录动画；清局只发 On Round Assigned。

更直接的节点仍是 **Has Record**。`OwnerSeat == -1` 在当前规则下和它等价，用来做“无人占领”的外观是合适的。

新局清空外观用 OnRoundAssigned；不会逐格额外发送 OwnerChanged(-1)。动画只负责显示，不需回调 C++ 才能更新归属，也不会暂停另一位玩家。用管理器 `Get Challenged Cell(Seat)` 查询该席当前挑战，避免用一个 bool 表示两席同时挑战。

原生角色已经消费 Confirm 的按下沿并挑选相交格子中最近的一个；等距按 Actor 路径稳定选择。成功开局后不会再转发同一次按下给 OnBoardButton。没有成功开始时继续交给原有事件；松开和 Exit 继续转发。若蓝图覆盖原始 ArcadeButton 接口，必须调用父类实现。没有额外读取物理键或创建 LocalPlayer。

`Try Interact(Character)` 也可由蓝图调用，但仍检查碰撞、正式 GameMode 的席位角色、输入所有权、池分配、宿主是否空闲，不允许从远处绕过交互条件。

## 编辑小游戏与池

Content Browser → Miscellaneous → Data Asset → 选择 **ArcadeMinigameDefinition**：

1. DisplayName：显示名称。
2. Map：普通独立小游戏地图，依旧要求唯一 ArcadeMinigameEntry 和本实例相机。
3. Metric：Score 使用 Finish 提交值；ElapsedSeconds 使用会话自 Start 开始的时间（不包含加载、准备、外框打开）。
4. Direction：HigherWins 或 LowerWins。
5. ChallengeTags：多选标签数组，可组合 HandSpeed / Calculation / PreciseTiming / Memory / Observation / Planning / Movement / Relaxed。

再创建 **ArcadeMinigamePool** Data Asset，直接编辑 Games 数组增删、拖入游戏。也可复制池资产，给不同格子指定不同的 Pool。**同一个池指同一资产引用**；两个内容相同但独立的池资产仍是两个分配组。重复项不会成为权重，同地图的多个定义只取首次出现，空引用被过滤；过滤后空池拒绝开新局。

一个地图代表一个游戏身份。如果将来同地图需要承载不同规则变体，应先扩展明确的变体标识和传参，不能靠重复定义资产骗过唯一性。

池修改在下一局生效。当前局的地图、指标、比较方向都已快照；不会因编辑共享定义改写已开始对局的规则。AssignedGame 保留资产引用供显示，因此不建议在运行中改它的名称／标签。

## 分配与新局

关卡放 **一个 ArcadeBoardRound**；Cells 留空时 BeginPlay 自动收集本关卡格子，或显式填写参与列表。读取已有两个宿主（按 Seat），没有时自动生成。重复管理器或重复席位宿主视为配置错误，查看 LastError；宿主的实例偏移和自定义画中画配置继续由宿主负责。

`Initialize On Begin Play` 默认开启，只分配本局游戏，不开小游戏。`Start New Round(Seed)` 用于后续正式对局流程：两席宿主均 Idle 且无挑战时才允许。Seed=-1 随机，固定种子可复现；RoundSeed 保存本次种子。不支持在挑战中途强制重开。

同池 K 款不同游戏、N 个格子：

- K >= N：打乱后无放回选 N 款。
- K < N：先放入全部 K 款，剩余位置均匀随机补齐，再打乱格子分配。

所有池先验证，失败保留上一局全部分配和纪录；成功才统一提交并发送 OnRoundAssigned、OnRoundStarted。管理器初次启动按 Actor 路径排序，同地图和同池顺序下固定种子可复现。

目前只做普通随机。未来疲劳/能力轮换可从 `UArcadeMinigamePool::Allocate` 的集中分配入口扩展，利用 ChallengeTags 与玩家历史，必须保留覆盖约束。不要在每次挑战结束后重抽当前格子，否则会破坏单局抢纪录的公平性。

## 纪录与并发

用户确认的规则：只有 Completed + Submitted 且有限成绩可以建立/刷新纪录；严格更优才能抢格；相等保留原主人；失败、取消、加载失败不改变纪录。第一次合法成绩即建立纪录，允许负数。

两名玩家同格时运行不同宿主/关卡实例。按 Session 完成通知顺序即时比较当前最佳值，不等外框动画结束；同帧仍由游戏线程依次处理，平分时先结算者保留归属。以宿主活动 SessionId 和席位保留的格子确认结果来源，第一次消费后清除保留，重复/迟到通知不能改另一次挑战。

纪录更新后外框仍可继续关闭；该席输入依旧由原公共会话恢复，但宿主 Closing/Unloading 时不能再次开局。格子移除后结果不会再写入已销毁对象；宿主销毁时清理输入并解除保留。新局不能跨越任何未卸载挑战。

## 验证

UE 5.8.3 Editor Development 完整构建通过。`Saved/Automation/BoardCells/index.json`：12 项编辑器测试全部 Success，含新增 PoolCoverage（100 个种子、1–12 款游戏、1–9 格）、CellRecords、RoundLifecycle，及现有输入、会话、双实例、相机回归。

`Saved/Automation/BoardCellRuntime/index.json`：`Arcade.Board.CellRuntime` 在真实 L_Main、真实样例蓝图、离屏 GPU 渲染下通过。覆盖两席在同格通过角色抽象确认输入进入、独立宿主/画中画、即时纪录、同格比较、不同格独立、输入恢复、卸载后重开、新局清空、旧 Run 拒绝、范围外拒绝、失败不占格。测试直接注入抽象确认并提交会话成绩，不能替代手动走到终点、真实键盘/手柄与窗口焦点验收。

尚需人工：九宫格镜头构图、视觉提示、占领动画、双人长时间游玩、焦点/设备中断；尚未做打包验证。当前占位方格不会自动变色，归属可在运行时 Details 读取，正式表现请接上述蓝图事件。新增软引用小游戏地图仍需纳入 Packaging 的地图列表。

补充回归：已有 Arcade.Minigame.PresentationRuntime 已改为测试内显式启动宿主，在新 L_Main 上通过；报告 Saved/Automation/BoardPresentationRegression。它检查双席 RT、GPU 像素、窗口位置、主相机与单席清理。共 12 项编辑器测试 + 2 项离屏真实地图测试通过。

