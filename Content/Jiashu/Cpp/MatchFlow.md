# 主菜单、对局、胜利结算与 UI 接口

2026-10-21。本文件是流程阶段的当前约定，优先于 BoardCells.md 中最初的自动开局描述。

## 生命周期

UE 的 BeginPlay 是 Actor 进入运行状态的通知：加载地图、动态生成 Actor 时都会发生，不代表用户点击了“开始对局”。现在 ArcadeBoardRound 的 BeginPlay 只发现格子、准备两个宿主、绑定通知；不抽取游戏、不清空/开始一局。旧的 InitializeOnBeginPlay 序列化字段保留兼容，但已隐藏且忽略。

逻辑分层：

- 应用/地图层：GameInstance 保持设备席位；BoardGameMode 创建共享相机和角色；L_Main 可以一直承载菜单、对局及结算 UI。
- 对局层：ArcadeBoardRound 维护 FlowState、单局分配、格子纪录、赢家与清理过程。
- 挑战层：每席 Host 加载独立小游戏；每次挑战都有独立 Session，完成后只向本局提交结果。

这次没有新增第二个 GameInstance 或复制小游戏生命周期。流程管理器与 L_Main 同寿命；跨地图保存菜单选择/战绩属于未来需求。

流程：MainMenu → Playing → Ending → Results。主动返回菜单为 Playing/Results → Ending → MainMenu。

MainMenu：可以显示场景与角色，但不把席位输入交给棋盘角色。AssignedGame 在首次开始对局前为空是正常现象。
Playing：才允许角色移动、格子确认和成绩更新。
Ending：赢家/结束决定已锁定，拒绝开新挑战、重开和后续成绩；下一 Tick 系统停止宿主并等待实际卸载。
Results：两席已卸载，允许再来一局或返回菜单。最终格子纪录和 WinningCells 保留给 UI 查看，直到下一局。

## UI 接入步骤

在你的 UI 初始化逻辑中获取当前关卡唯一 ArcadeBoardRound（Get Actor Of Class）。保存引用，绑定 OnFlowStateChanged 和 OnMatchFinished，随后立刻读取 FlowState 初始化界面。首次 MainMenu 是初始状态，不保证 UI 能监听到一个初始广播。

1. 主菜单“开始”：调用 `Start New Round(Seed=-1)`，检查返回值。true 后显示对局 UI；false 时显示 `Last Error`，不要关闭菜单。Seed=-1 使用管理器 InitialSeed；InitialSeed 也是 -1 时才随机。
2. 获胜提示：`On Match Finished(WinnerSeat)` 发出一次。读取 WinnerSeat（0=P1，1=P2）、WinningCells（三个格子）播放胜利提示、特效。此时通常仍在 Ending，不允许立即重开。
3. `On Flow State Changed(Results)`：小游戏已清理，显示完整结算界面并启用“再来一局”。
4. “再来一局”：仍调用 `Start New Round`，重新抽取池、清空各格成绩和赢家、角色回到本次加载地图时的出生位置，并恢复两席棋盘输入。
5. “返回主菜单”：调用 `Return To Main Menu`。true 表示已接受请求，等状态真正成为 MainMenu 再显示可开始新局的菜单。此接口可中止整局，不是小游戏玩家主动退出键。
6. “退出应用”：使用 UE 的 Quit Game 节点。进程/World 退出走原有 EndPlay 清理，不需要先等一个菜单动画完成。

可读查询：`FlowState`、`WinnerSeat`、`WinningCells`、`RoundSeed`、`LastError`、`Can Start New Round`。CanStartNewRound 检查流程/宿主是否空闲；具体棋盘与池配置仍由 StartNewRound 验证。`Get Cell At(Row, Column)` 按本局的坐标快照取格子，首次开始对局前返回空。

UI 的按钮点击调用上述入口。状态/格子事件用于更新显示，不要在同一个通知回调内递归调用 StartNewRound/ReturnToMainMenu；框架保护事务期间的重入请求，自动跳转请延迟到下一 Tick。

菜单与结算 UI 如需手柄/键盘导航：继续使用 ArcadeMenuWidget / ArcadeInputReceiver，通过 InputSubsystem.SetInputReceiver(Seat, Widget) 绑定两席。没有 UI 时菜单阶段的两席 Receiver 为空是预期行为。StartNewRound 成功时框架接回角色；之后销毁菜单不要再调用 RestoreBoardInput 覆盖新目标。系统清理只释放自己拥有的接收对象，不会抢走随后接管的 UI。

小游戏中的 Exit 仍不接 Cancel，不自动弹菜单。暂停、断连整局暂停尚未实现。

## 还没有 UI 时怎么试

当前默认 Play 后停在 MainMenu，所以角色不能移动，也不会立即分配游戏。这是新的正式行为。

临时测试可以在 Level Blueprint：Event BeginPlay → Delay（例如 0.1 秒，等待 GameMode 完成角色创建）→ Get Actor Of Class(ArcadeBoardRound) → Start New Round，检查返回值/LastError。仅用于测试，做好开始按钮后删掉这条自动调用。不要在格子 BeginPlay 里开局，也不要重新打开宿主 Start Automatically。

或者做一个最小 UMG 按钮，OnClicked 调用 StartNewRound，无需先完成整套菜单。

## 坐标与胜利

每个 ArcadeBoardCell（包括蓝图子类）Details 中新增 Row、Column，取值各为 0、1、2。新放置格子的默认 -1 表示尚未配置。

严格要求九格、九种坐标各一次。缺格、多放测试格、无效坐标、重复坐标、空池/无有效地图都会拒绝开始，LastError 指明数量或具体 Actor。失败不会破坏上一局纪录。自动收集模式每次开局重新扫描同关卡；显式 Cells 列表则只使用列表。两种模式在 BeginPlay 根据列表是否为空确定。

已给现有 L_Main 的九个 BP_ArcadeBoardCell 实例填好坐标，保留用户模型和变色蓝图：Row 按世界 X 从大到小（500、0、-500），Column 按 Y 从小到大（-248、252、752）。这只是本次给当前布局填写的约定；运行时不会根据位置、名字或数组顺序猜坐标。移动/旋转整个棋盘无需改逻辑坐标；复制格子时记得修改 Row/Column。

每次有效挑战结算，更新归属后检查三行、三列、两条对角线。任意一条三格同属一席即获胜。先完成并形成三连的结果锁定赢家；之后同帧到达的成绩也不再改盘。九格全满但没有三连仍继续挑战抢格，没有井字棋式平局，因为本游戏允许易主。

胜利时系统会中止另一席尚未完成的小游戏（含 Loading/Preparing/Running），不计成绩；也清理赢家的展示实例。此版本直接收起小游戏画中画，不等待正常关闭动画；胜利演出放在棋盘/UI 的 OnMatchFinished 中。若以后需要“胜利过场后再关窗口”，可在 Ending 阶段追加表现流程，不能继续接受结果。

同一玩家一次占领同时形成多条线时，WinningCells 提供按行、列、对角线顺序发现的第一条线；WinnerSeat 只结算一次。

## 变色蓝图迁移

保留 OnOwnerChanged。新局清空已占领格子时，现在也会发 OnOwnerChanged(原席位,-1)，请显式处理 -1 为中立材质，0 为 P1，1 为 P2，不要把“非0”全部当成 P2。

OnRoundAssigned 每局都会发出，可在这里统一恢复中立外观、更新游戏名称。新局的所有原生状态先统一重置，再逐格通知蓝图。胜利时的格子颜色不会立即清空，便于展示结果。

## 验证与边界

测试覆盖：两席各自全部八种三连、满盘无三连继续、坐标乱序/重复/缺失、菜单启动无分配、显式开局、活跃局拒绝重置、返回菜单、Results 再来一局。

真实 L_Main 运行测试以抽象确认输入启动实际小游戏，模拟两条已存在的占领纪录后由第三次真实 Session 结果触发胜利，检查另一席取消、结果冻结、输入关闭、卸载后重开、加载时返回菜单。自动化不能代替玩家实际走到终点和硬件/焦点验收。

测试报告和最终通过情况见本文件末尾。暂无 UI 资产、胜利动画、音效、跨局累计比分、暂停或存档功能。

2026-10-21 最终验证：UE 5.8.3 Editor Development 完整构建成功。Saved/Automation/MatchFlow：13 项 Success；MatchFlowRuntime：1 项 Success；MatchFlowPresentation：1 项 Success。三个报告均 0 failed、0 succeededWithWarnings。实际 UI、玩家手动完成小游戏、硬件/焦点与打包仍待验收。
