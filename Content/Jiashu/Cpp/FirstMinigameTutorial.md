# 第一个小游戏：走到终点，确认后提交用时

后续 E 更新：本教程的玩法接线保持有效；独立测试现在也通过 SceneCapture/RT/外框显示 UI，相机可以来自角色内部组件，详见 [MinigamePresentation.md](MinigamePresentation.md)。原文中“尚无画中画”的说明是历史状态。计时 Widget 不需要改为自己 AddToViewport。

本教程对应阶段 B。C 的公共测试 GameMode 已加入源码：`ArcadeMinigameTestGameMode`。重开编译后的编辑器再创建蓝图。教程中的 BP/WBP/Level 资产需要你在编辑器创建；不是源码自动生成的现成资产。

目标：一个角色自由移动，到终点附近按确认获胜；计时从**第一次非零方向输入**开始，到成功确认止，停下时仍继续计时。这个定义对应“玩家开始操作”，不是检测角色脚底实际发生位移。如果以后要精确到实际离开起点，再单独调整规则。

小游戏中途不能由玩家退出；Exit 分支不调用 Cancel。Cancel 保留给准备失败、对象销毁、关卡卸载等异常清理。退出键仍可用于菜单返回。

## 0. 蓝图阅读约定

蓝图类是模板，拖进关卡的是实例。变量的 Object Reference 指向一个具体实例，Class Reference 指向模板。本教程的 Runner、StartPoint、GoalPoint 都是 **Object Reference**。

白色箭头是执行顺序，彩色线是数据。“A → B”表示将 A 的白色输出接到 B 的白色输入；纯计算节点没有白线，只接数据。添加变量后先 Compile（编译），再设置默认值。

变量旁的小眼睛（Instance Editable）打开后，才能选中关卡里的实例设置引用；普通运行变量不要打开。默认值不是关卡内某一个对象的引用。

每完成下面一小节就 Compile、Save。如果节点找不到，优先从正确类型的对象引脚拖线搜索，例如从 Run 拖线搜 Finish，而不是在空白处搜一个不确定的同名节点。

## 1. 建文件夹与角色蓝图

在 Content Browser 创建 `/Game/Jiashu/Minigames/WalkToGoal/`，本样例的资源都放这里。

右键 → Blueprint Class → **Character**，命名 `BP_WTGCharacter`。不要选择项目的 ArcadeBoardCharacter：这是独立小游戏角色，不是棋盘角色，也不需要棋盘 Seat 初始化。

在角色蓝图设置：

- Class Defaults：Auto Possess Player = Disabled、Auto Possess AI = Disabled、Auto Receive Input = Disabled。输入由公共会话送来，不让角色抢 UE 玩家控制器。
- Use Controller Rotation Yaw = false。转向交给移动组件，不依赖未绑定的 Controller。
- CharacterMovement：搜索 **Run Physics with No Controller**，勾选（可能要展开高级属性）。否则没有 Controller 的 Character 可能收到方向却不走。
- CharacterMovement：Max Walk Speed = 300、Max Acceleration = 2400、Braking Deceleration Walking = 2400、Orient Rotation to Movement = true、Rotation Rate 的 Yaw = 720。
- CapsuleComponent：Capsule Radius = 34、Capsule Half Height = 88。角色站在 Z=0 的地面上时中心约为 Z=88。
- Components → Add → Static Mesh，命名 Body，Static Mesh 选引擎 Cube，Scale=(0.6,0.6,1.5)，Collision Presets=NoCollision。立方体只负责外观，碰撞由 Capsule 负责。
- 保留角色 Actor Tick 的 Start with Tick Enabled。下面的持续移动每帧执行。

新增变量 `MoveDirection`，类型 Vector2D，默认 (0,0)。

创建函数 `SetMoveDirection`，输入 `NewDirection`（Vector2D），白线接 Set MoveDirection，值接 NewDirection。此函数只是记录方向。

创建函数 `StopMovement`：

```text
函数入口 → Set MoveDirection=(0,0)
         → Consume Movement Input Vector（Target=Self）
         → CharacterMovement 的 Stop Movement Immediately
```

为什么三步：既清除我们缓存的方向，也清掉引擎尚未处理的移动输入，再立刻清零速度；否则取消后可能仍多走一帧。

Event Graph 中连接：

```text
Event Tick
 → Add Movement Input：World Direction=(1,0,0)，Scale Value=MoveDirection.Y
 → Add Movement Input：World Direction=(0,1,0)，Scale Value=MoveDirection.X
```

MoveDirection 拖出来 Get，右键输出引脚 → Split Struct Pin，可取得 X/Y。两次 Add Movement Input 的 Target 都是 Self，Force 不勾选。

**这里不要乘 Delta Seconds。** Add Movement Input 提供输入强度，CharacterMovement 自己按时间计算移动；重复乘会使移动异常慢。第一版相机 Yaw 固定为 0，因此“向上”是世界 +X、“向右”是 +Y。后续双实例只平移地图时这仍成立；旋转实例要再统一移动方向基准。

## 2. 创建计时 UI

右键 → User Interface → Widget Blueprint，命名 `WBP_WTGTimer`。

Designer 中放一个 Canvas Panel 作根，再放 Text：

- 命名 `TimerText`，勾选 Is Variable，允许 Graph 访问并改变文字。
- Anchor 设左上，Position=(40,40)，Size=(800,100)，字体 32，白色。文本先写 `Ready  0.00 s`。
- Class Defaults 的 Is Focusable 不勾选。这是只读显示，不抢键盘焦点。

Graph 创建函数 `UpdateDisplay`，两个输入：

- `Seconds`：Float（数值，若编辑器提供 Double 也可以）。
- `Message`：Text。

连接：

```text
Seconds → To Text (Float)
           Minimum Fractional Digits=2
           Maximum Fractional Digits=2
         → Format Text 的 Time 输入
Message  → Format Text 的 Message 输入
Format Text 的格式：{Message}  {Time} s

函数入口 → TimerText 的 SetText，In Text 接 Format Text 的结果
```

To Text 节点的小展开箭头会显示小数位设置。保留完整秒数作为成绩，只在 UI 显示时保留两位小数。不要在这个 Widget 调用 Add to Viewport、Get Player Character 或读取按键。

## 3. 创建入口蓝图和变量

右键 → Blueprint Class → All Classes，搜索 **ArcadeMinigameEntry**，创建 `BP_WTGEntry`。

Class Defaults → Minigame Widget Class 选择 `WBP_WTGTimer`。这只告诉公共显示层“我需要哪种 UI”，不负责创建或覆盖整个屏幕。

新增变量（下列名称保持一致，方便接线）：

- `Runner`：BP_WTGCharacter **Object Reference**，打开 Instance Editable。
- `StartPoint`：TargetPoint **Object Reference**，打开 Instance Editable。
- `GoalPoint`：TargetPoint **Object Reference**，打开 Instance Editable。
- `GoalRadius`：Float，默认 100，打开 Instance Editable，单位厘米。
- `ActiveRun`：ArcadeMinigameSession **Object Reference**。
- `HUD`：WBP_WTGTimer **Object Reference**。
- `Playing`：Boolean，默认 false。
- `ClockStarted`：Boolean，默认 false。
- `StartSeconds`、`FinalSeconds`：Float，默认 0。
- `HUDTimer`：TimerHandle。

这里不用 Entry 的 Event Tick：公共入口基类默认不 Tick。角色的移动用自己的 Tick，数字刷新使用 Timer。

## 4. 准备与开始

在入口 Event Graph 搜索并添加 **Event On Prepare**。它的 Run 引脚是公共系统传入的“本次挑战”，不是玩家角色。

连接顺序：

```text
On Prepare(Run)
 → Set ActiveRun=Run
 → 依次 Is Valid：Runner、StartPoint、GoalPoint
 → Runner.StopMovement
 → Runner.SetActorLocation：
      New Location = StartPoint.GetActorLocation
      Sweep=false，Teleport=true
 → Set Playing=false
 → Set ClockStarted=false
 → Set StartSeconds=0
 → Set FinalSeconds=0
 → ActiveRun.Mark Ready
```

使用带白色执行引脚的 **Is Valid** 节点。任何引用无效的分支连接 Print String（例如 `WTG: missing Runner/StartPoint/GoalPoint`），然后 `ActiveRun.Cancel`，不要连接 Mark Ready。这里是配置错误时的系统中止，不是玩家主动退出。

Sweep=false 是准备阶段精确复位；Teleport=true 不把复位当成运动。必须把起点放在安全位置，不能让角色胶囊陷入地面。

**覆盖 On Prepare 后，本教程不调用 Parent: On Prepare，而是最后自己 Mark Ready。** 父类默认会立即 MarkReady；我们需要先验证引用、复位角色，再宣布准备完成。

再添加 **Event On Started**：

```text
On Started(Run)
 → Set ActiveRun=Run
 → Set Playing=true
 → Get Minigame Widget（Target=Self）
 → Cast To WBP_WTGTimer
 → Set HUD=As WBP WTGTimer
 → RefreshHUD（下一节创建）
 → Set Timer by Event：Time=0.05，Looping=true
 → 把返回的 TimerHandle 存入 HUDTimer
```

Timer 的红色 Event 引脚连接 Create Event，Object=Self，函数选择下一节的 `RefreshHUD`（无输入、无输出，所以可作为计时器事件）。也可以从红色引脚创建无参数 Custom Event，该事件调用 RefreshHUD。

若 Cast Failed，Print String `WTG: check Minigame Widget Class`。角色玩法不必因此失败，但计时 UI 不会显示，需要修正设置。

为什么这里才拿 UI：测试环境在 Prepare 返回后创建 UI，确保它在 OnStarted 前存在；OnPrepare 时不保证有 UI。

## 5. 显示当前时间

入口创建函数 `RefreshHUD`，无输入、无输出：

1. Is Valid(HUD) 且 Is Valid(ActiveRun)，否则结束函数。
2. 如果 Playing=false，直接结束，避免结束画面被下一次刷新覆盖。
3. ClockStarted=false 时：调用 HUD.UpdateDisplay，Seconds=0，Message=`Move to start`。
4. ClockStarted=true 时：`ActiveRun.Get Elapsed Seconds - StartSeconds` → Max（与 0 比较）→ HUD.UpdateDisplay 的 Seconds；Message=`Reach goal and confirm`。

0.05 秒是刷新频率，**不是每次给成绩加 0.05**。成绩使用两个时间点的差，低帧率或计时器少触发一次不会直接少算时间。

## 6. 输入方向与开始计时

添加 **Event On Direction(Run, Value)**：

```text
On Direction
 → Branch(Playing)
 → Runner.SetMoveDirection(Value)
 → Branch：NOT ClockStarted AND（Value 的 Vector2D Length > 0.01）
 → True：Set StartSeconds=ActiveRun.Get Elapsed Seconds
         → Set ClockStarted=true
```

Value 是 Vector2D，不要把 X/Y 当成世界位置。Vector2D Length 节点可从 Value 引脚拖线搜索 Length。False 分支不做事。

这会在第一次有效方向输入开始计时。输入回到 0 不重置 ClockStarted，停下等待或跑错方向仍花费时间。进入地图后站着没动，UI 保持 0。

添加 **Event On Input Cancelled**：

```text
On Input Cancelled → Is Valid(Runner) → Runner.StopMovement
```

只停止持续运动，不清零成绩，不调用 Finish，不调用 Cancel，也不把 Playing 改为 false。失焦后会话仍在，恢复焦点并重新输入能继续。当前没有全局／局部暂停策略，因此已开始的计时继续推进。

## 7. 到达终点后确认，提交结果

添加 **Event On Button(Run, Action, Pressed)**：

```text
On Button
 → Branch：Playing AND Pressed
 → Switch on EArcadeInputAction（Selection=Action）
 → Confirm 分支：继续检查
 → Exit 分支：留空
```

Pressed=true 只处理按下，防止松开再触发一次。Exit 留空实现“中途不能退出”。

Confirm 分支继续：

```text
Branch(ClockStarted)
 → Distance2D (Vector)
      V1 = Runner.GetActorLocation
      V2 = GoalPoint.GetActorLocation
 → <= GoalRadius
 → Branch
 → True：
      Set FinalSeconds = Max(ActiveRun.Get Elapsed Seconds - StartSeconds, 0)
      → ActiveRun.Finish
           Score = FinalSeconds
           Succeeded = true
```

Distance2D 只比较 XY，忽略角色中心和地面标记的高度差。半径 100 表示距终点水平距离 1 米内可提交；它是本小游戏的规则变量，不是公共输入规则。

False 分支暂时不连接，意味着不在终点时按确认无效。ClockStarted 检查避免角色一开始就在目标里、没移动也获胜。

**Finish 就是返回数据的节点。** 从 ActiveRun 引脚拖线找它，Target 必须是这次 Run。只调用一次，由它通知公共系统；不用 Cast 到棋盘 GameMode，也不用自己修改格子。

本游戏 Score 保存实际用时，例如 4.27。将来比较规则应配置为“越小越好”，本阶段不取负数、不比较赢家。结果自带的 ElapsedSeconds 是从会话 Start 起的总耗时，可能包含玩家尚未开始移动的等待；因此本样例用 Score 作为比赛用时。

## 8. 结束画面与清理

添加 **Event On Stopped(Run, Result)**：

```text
On Stopped
 → Set Playing=false
 → Is Valid(Runner) → Runner.StopMovement
 → Clear and Invalidate Timer by Handle（HUDTimer）
 → Break ArcadeMinigameResult（输入 Result）
 → Switch on EArcadeMinigameOutcome（输入 Outcome）
```

Completed 分支：Is Valid(HUD) → HUD.UpdateDisplay，Seconds=Result.Score，Message=`Finished`。

Failed/Cancelled 分支：Is Valid(HUD) → HUD.UpdateDisplay，Seconds=0，Message=`Stopped`。不要把配置错误或关卡卸载显示成胜利。

添加 **Event On Cleanup**：Clear and Invalidate Timer by Handle(HUDTimer)。添加 **Event EndPlay**：同样清理计时器。前者照顾正常结束，后者照顾直接停止 Play 或销毁对象；重复清理无效 Handle 是安全的。

不要 Remove From Parent。UI 的生命周期归公共测试环境管理；本版完成后保留结果画面，停止 Play 时统一移除。继续测试请 Stop 后重新 Play。

## 9. 创建独立 Level，指定对象

File → New Level → **Basic**（普通非 World Partition 小地图），另存为 `/Game/Jiashu/Minigames/WalkToGoal/L_WalkToGoal`。不要改 L_Main。

场景准备：

1. 保留或添加灯光、天空；保留一个有 BlockAll 碰撞的地面。可用 Cube，Location=(0,0,-50)，Scale=(20,14,1)，顶面 Z=0。地面宽约 20×14 米，原型起终点都在其上。
2. 放两个 **Target Point**，重命名 StartPoint、GoalPoint；Location 分别设 `(-500,-300,100)`、`(500,300,100)`。TargetPoint 是编辑器定位标记，Play 时不会当作角色显示；所以再放一个绿色 Cylinder/Cube 作终点可见标志，Collision=NoCollision，放在终点下方，不能堵住角色。
3. 拖入一个 BP_WTGCharacter，命名 Runner，初始放在 StartPoint 位置。
4. 拖入一个 BP_WTGEntry。选中它，在 Details 的变量里用滴管／下拉框设置 Runner、StartPoint、GoalPoint，分别指向刚放置的三个实例。
5. 放一个 **CameraActor**：Location=(-1000,0,1732)，Rotation=(Pitch=-60,Yaw=0,Roll=0)，FOV=60。选中 Actor，在 **Actor Tags** 数组点 `+`，填 `ArcadeMinigameCamera`。名字不是 Tag；不要用 Component Tags。场景中有且只有一个带这个 Tag 的相机。
6. World Settings → GameMode Override 选择 **ArcadeMinigameTestGameMode**。不要选棋盘 GameMode 或输入 Demo。
7. 如果需要改 TestSeat，可创建该 GameMode 的蓝图子类 `BP_WTGTestGameMode`，在 Class Defaults 设置 Test Seat（0 或 1），再让地图 Override 使用此子类。初次默认 Seat 0 即可。
8. PIE Number of Players=1，使用 Play，不用 Simulate；不要创建第二个 LocalPlayer。PlayerStart 不是本测试的必要对象，因为不会生成默认 Pawn，我们已经手动放置 Runner。

独立测试 GameMode 自动寻找唯一入口和带 Tag 的相机，使用同一公共会话调用 Prepare/Start，并根据入口的 Minigame Widget Class 创建 UI。缺少／重复入口或相机时会拒绝启动并写 Output Log；OnPrepare 超过默认 30 秒没 MarkReady 会报错取消。

只有独立测试环境切换共享 ViewTarget。未来地图被正式世界流入时，这个 GameMode 不会启动，入口蓝图不能自己切主屏相机。

## 10. 第一次验收

保存所有蓝图与地图，按 Play，点游戏视口获取焦点；刚进入时松开全部方向与确认键一次。

- 只插键盘时，Seat 0 默认 WASD／Space。若连接手柄，设备仍按既有优先级分配，不能假设 Seat 0 必然是键盘。运行中不自动重新分配。
- 未移动：显示 0.00；第一次移动后计时，停下也继续增加。
- 到达终点但不按确认：不结束。离终点很远按确认：不结束。
- 在终点内按确认：角色停止，显示 Finished 和秒数；按住确认不会重复提交。
- Escape／手柄退出键不结束小游戏。
- 移动中 Alt+Tab：角色停止；回到游戏、松开再操作，可以继续，已开始的计时不重置。
- Output Log 搜索 `LogArcadeMinigame`，应看到 outcome=0、score=你的用时。也可在入口 OnStopped 中 Break Result → Score → Print String 临时检查。

如果只显示 Ready 不动，优先检查 Run Physics with No Controller、入口引用、角色 Tick 的 AddMovementInput、焦点和当前设备分配。如果 30 秒后报准备超时，检查 OnPrepare 最后是否执行 MarkReady。如果没有 HUD，检查 Minigame Widget Class 和 OnStarted 的 Cast。

## 11. UI 与画中画的边界

公共层创建并承载 Widget，再放进 E 的外框，与 Render Target 叠放。**不要自己 AddToViewport**：那样的 UI 不会被 SceneCapture 拍进画面，也会和正式容器重复。

本教程可以验证独立 Level 的移动、计时、确认、提交和 UI 更新。动态双实例和画中画已另有实现，见 DynamicMinigameInstances.md 与 MinigamePresentation.md；本教程不能代替那两次验收。
