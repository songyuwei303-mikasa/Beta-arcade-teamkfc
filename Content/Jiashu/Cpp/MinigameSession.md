2026-10-21 正式接入更新：F/G 已完成，L_Main 使用格子确认进入，详见 [BoardCells.md](BoardCells.md)。下面的自动启动配置仅为历史 D 集成步骤。

# 公共小游戏：阶段 A 接入

E 接入更新：相机与显示路径见 [MinigamePresentation.md](MinigamePresentation.md)。Entry.MarkReady 后宿主创建显示/UI并等待外框 FinishOpening，再 Session.Start；OnPrepare 中仍不保证 UI 存在。Finish 后 Session 的 OnStopped/OnCleanup/输入恢复顺序不变，宿主保留冻结画面和 UI 播放关闭动画，之后才卸载关卡。“游戏结束”与“窗口消失”已分离。

最新状态：B/C 已验收。D 见 [DynamicMinigameInstances.md](DynamicMinigameInstances.md)，E 见 [MinigamePresentation.md](MinigamePresentation.md)。下文若写“没有动态实例／没有画中画／B 待做”，那是 A 时期记录，不要当成现状。格子入口和成绩归属仍未做。样例教程见 [FirstMinigameTutorial.md](FirstMinigameTutorial.md)。

## 真实类与节点

- `ArcadeMinigameSubsystem`：World Subsystem，每席最多一个活动会话。蓝图使用 **Get World Subsystem → ArcadeMinigameSubsystem**。随 World 清理，不跨地图保留会话。
- `ArcadeMinigameEntry`：Actor 基类；每个小游戏实例放一个蓝图子类。可以在该蓝图中引用本实例的角色、目标、计时器等。BeginPlay 只做场景初始化，不开始计分。
- `ArcadeMinigameSession`：每次挑战新建的 UObject，实现现有 `ArcadeInputReceiver`。它是 Subsystem 当前实际接收对象，检查席位和状态后向入口转发事件。蓝图不另读键盘／手柄，也不自行切换输入到入口。

主流程：

1. `Prepare Session(Seat, Entry)`：返回 Run，失败返回 None。检查席位、入口、World、已有会话；入口必须已 BeginPlay。成功立即暂停该席原接收对象，保存弱引用，接入 Run；此时没有玩法输入。
2. 入口 `On Prepare(Run)`：准备本次状态。父类默认调用 `Run.Mark Ready()`。若覆盖该事件，准备完成后自己调用 Mark Ready；异步准备不要提前调用父类。准备时间不计入成绩耗时。
3. 启动方检查 Run 有效且 State=Ready，再调用 `Run.Start()`。重新要求输入回到中立，随后 State=Running，调用入口 `On Started(Run)`；此时开始计时。Start 不等同于等待所有物理按键已松开，按住者仍须松开才能操作。
4. 运行时入口收到 `On Direction(Run, Value)`、`On Button(Run, Action, Pressed)`。方向是变化事件；连续移动需缓存，或每帧读 Run.Direction。确认聚合状态可读 Run.ConfirmHeld。
5. 正常结束：`Run.Finish(Score, Succeeded)`；准备失败等系统中止：`Run.Cancel()`。Finish 仅在 Running 接受，拒绝非有限分数；Cancel 可用于 Preparing、Ready、Running，但用户已明确小游戏中途不能由玩家主动退出，不能把 Exit 接到 Cancel。
6. 系统进入 Ending，清空方向和确认；调用入口 On Input Cancelled → On Stopped → On Cleanup，清理登记的临时 Actor；归还输入，释放席位和入口，标记 Ended，然后广播 Subsystem 的 `On Session Ended(Result)`。

准备和开始分离：同步原型可在 Prepare Session 返回后紧接 Start；异步原型在 On Prepare 的完成回调中对**保存的 Run**调用 Mark Ready，再由测试启动方或入口明确调用 Start。后续 C/D/E 的启动方将负责等场景和显示准备好。

无论成功、失败还是取消，本阶段都只广播结果，不决定是否占格、谁胜利或旧记录是否保留。

## 结果和迟到回调

`FArcadeMinigameResult` 包含 SessionId、Seat、Outcome（Completed/Failed/Cancelled）、Reason（Submitted/Requested/EntryDestroyed/WorldEnded）、Score、ElapsedSeconds。ElapsedSeconds 是从 Start 起的 World 游戏时间，包含失焦期间仍在推进的游戏时间；本阶段没有断连暂停／局部暂停策略。

**异步任务、延迟和计时器必须保存发起时的 Run。** 完成时对那个 Run 调用 Finish，不能重新 GetSession(Seat) 或 Entry.GetSession 来提交旧成绩。结束后的 Run 方法返回 false；入口再次使用时会创建新 Run、新 SessionId，旧回调不能结束新局。

同席重复进入、两席共享同一入口、跨 World 入口都被拒绝。两人同时玩相同玩法，当前须使用两个独立入口 Actor；阶段 D 才会加载两个独立地图实例。

## 输入和退出语义

- 输入流为 ArcadeInputSubsystem → Run（实现 ArcadeInputReceiver）→ Entry 事件。Run 按 Seat 过滤；不会创建 LocalPlayer、Controller 或额外 Possess。
- 切入准备阶段立即取消棋盘角色输入；另一席继续游玩。Start 和返回时都保留输入中立保护，进入按钮不会贯穿到小游戏。
- **On Input Cancelled 只清理移动、蓄力等持续输入。** 失焦／断连／接收对象切换不会自动将会话判为失败或取消，也不会暂停另一人。不要把取消事件当成正常松开后发射。
- 已确认规则：小游戏中途不能主动退出。玩法忽略 Exit，不能调用 Run.Cancel；菜单可使用 Exit 返回上一级。失焦、设备取消与生命周期异常仍按各自语义处理。
- 正式棋盘结束时，在原接收对象仍是本会话的前提下归还输入；原目标是该席棋盘角色则走 `RestoreBoardInput`。没有棋盘 GameMode 的测试 World 会恢复原接收对象，原目标销毁则清空。
- 若中途菜单已接管输入，结束不会抢走菜单的接收权。菜单关闭时须检查会话是否还 Running；如果已结束，回到棋盘，不能恢复已结束的 Run。
- World 退出只解除输入，不恢复正在退出的棋盘角色。

## 清理责任

On Stopped 停止玩法；On Cleanup 清除自己的 TimerHandle、事件绑定、音效和其它持续行为，并将可复用的场景对象重置。这两个事件可能发生在准备未完成时，因此要允许部分初始化。

通过 `Run.Register Spawned Actor(Actor)` 登记**仅为本次挑战动态创建**的 Actor，结束时系统销毁它们。不登记地面、相机、关卡原有对象或其它会话的资源。接口要求 Actor 与入口在同一 Level，拒绝棋盘角色、入口和已被另一会话认领的对象。将来动态加载时，生成对象的 Owner/OverrideLevel 须属于入口实例，不能默认生成到持久关卡再假设卸载会清理。

入口被销毁会取消会话并清理登记对象。已经失效的入口不能再执行蓝图清理事件，因此入口／其组件自身的 EndPlay 也应负责计时器、外部绑定等清理。系统不能自动推断任意蓝图创建的所有资源。未登记的动态对象仍由小游戏负责。

## 本阶段手动验收（无需等待画中画）

阶段 A 不自动扫描／启动小游戏，不改变相机，也不会在 L_Main 生成任何测试 UI。最小验证方法：

1. 创建 **BP_MinigameSmoke**，父类 ArcadeMinigameEntry，放在现有测试场景的共享相机可见位置。
2. On Started → Print String 显示 Run.Seat。On Button 中：Confirm 且 Pressed → Run.Finish(42, true)；Exit 分支留空。先不要覆盖 On Prepare（父类会立即就绪）。
3. 临时创建一个鼠标按钮 Widget，增加 Entry 引用变量并 Expose on Spawn；由 Level Blueprint 创建 Widget 时传入场景中该入口，Add to Viewport。按钮 OnClicked → Get World Subsystem → Prepare Session(0, Entry) → IsValid → Start。此按钮只是测试触发，不重绑任何玩法键。
4. 等棋盘角色生成后点击按钮，松开方向／确认，再按 P1 的确认结束。观察进入时 P1 停止、P2 仍能移动；结束后 P1 松开再按可移动。绑定 On Session Ended 查看结果，Output Log 也记录结果。
5. 第二次进入使用同一个 Entry；两人并发测试则放两个 Entry，分别 Prepare Seat 0/1。不要用同一 Entry 同时服务两人。
6. 测试中销毁入口、切换接收者、Alt+Tab、重复 Finish 和结束后迟到调用。临时 Widget 验收后移除，不加入正式 GameMode。

以上保留为在已有 World 中验收阶段 A 的临时办法。现在新建独立样例请直接使用 C 的 ArcadeMinigameTestGameMode，见 FirstMinigameTutorial.md，不必创建临时启动按钮。

## 独立测试 GameMode（C）

地图 Override 选择 `ArcadeMinigameTestGameMode`；持久关卡中放置唯一 ArcadeMinigameEntry 蓝图实例和唯一带 Actor Tag `ArcadeMinigameCamera` 的 CameraActor。TestSeat 默认 0。该模式使用正式 BoardController 的 UI 输入设置，没有默认 Pawn、没有输入 Demo 面板。

所有 Actor BeginPlay 后，测试模式创建会话并等待 Ready，再 Start。OnPrepare 默认 30 秒未就绪会报错取消。入口可配置 MinigameWidgetClass；测试模式创建 Widget，在 OnStarted 前将其交给 Entry.GetMinigameWidget。入口只更新 UI，不自己 AddToViewport。结果画面保留到停止 Play；当前不自动重开。

测试模式现在使用与正式宿主相同的 SceneCapture／Render Target／外框，只是全屏放大。不要再把旧的“普通视口 Widget”当成当前测试画面。

## 从现在到团队开发的里程碑

A–E 的公共链路已经接通，B 样例和 D 双实例已由用户试过。组员可以按现有样例做新小游戏。下一公共阶段是 F 格子入口，然后才是 G 归属和 H 正式 UI。集成测试用的 Start Automatically 在正式格子接入后应关掉。

## 验证记录

2026-10-20：UE 5.8 Editor Development 完整构建通过。`Arcade.` 六项自动化全部 Success、零测试警告，包括已有输入／移动回归和新增 `Arcade.Minigame.SessionLifecycle`。

新增测试覆盖：双席独立、准备阶段冻结棋盘、输入取消、开始／返回中立保护、重复进入／提交拒绝、旧会话回调拒绝、资源登记与清理、入口销毁、后续接收者不被覆盖及 World 退出。测试直接驱动现有输入路由，不依赖真实硬件。

报告 `Saved/Automation/Minigame/index.json`，日志 `Saved/Logs/MinigameAutomation.log`。尚未创建或人工试玩蓝图示例。NullRHI 测试不能代替蓝图事件、鼠标 UI、实际设备及渲染人工验收。

C 新增验证：完整构建通过，`Arcade.` 七项 Success、零测试警告，报告 `Saved/Automation/MinigameTestHost`、日志 `Saved/Logs/MinigameTestHost.log`。新增 StandaloneHost 测试检查入口／相机选择、歧义拒绝、无默认 Pawn、使用原会话开始、Exit 不自动结束、结束后不重复启动。完整蓝图与 UMG 可视联调仍需用户按教程执行。
