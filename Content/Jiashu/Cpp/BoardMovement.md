# 正式关卡：双席位移动基础版

本阶段按用户确认实现类似胡闹厨房的自由连续移动、固定俯视共享相机和场景碰撞，用户已确认双人移动与输入阶段性测试通过。小游戏会话、动态实例和画中画分别见 [MinigameSession.md](MinigameSession.md)、[DynamicMinigameInstances.md](DynamicMinigameInstances.md)、[MinigamePresentation.md](MinigamePresentation.md)。格子交互和正式菜单页面仍未实现。

## L_Main 编辑器配置（需要手动完成）

以下是首次配置步骤。2026-10-20 用户已完成地图接入并验收双人移动，不需要重新搭建；本次小游戏代码未修改任何 `.umap`。

2026-10-20 引擎只读加载核对：L_Main 已有地面 StaticMeshActor、两个 PlayerStart、环境及 CameraActor，Override 为 BP_ArcadeBoardGameMode；L_TestingLevel Override 为 ArcadeInputDemoGameMode。此前“没有地面／出生点、Override=None”仅是 10-19 初始快照。

1. 打开 `/Game/Jiashu/Levels/L_Main`。World Settings → GameMode Override 设为 `ArcadeBoardGameMode`，或 None 以继承项目默认。可创建 `BP_ArcadeBoardGameMode` 子类并选用它。
2. 添加地面：例如 Cube，Location `(0,0,-50)`，Scale `(16,16,1)`，使地面顶面为 Z=0；碰撞设为 BlockAll。添加灯光。边界用 Blocking Volume 或有碰撞的墙围住；没有隐含坐标边界，走出无围栏地面会掉落。
3. 放置两个 PlayerStart，Location 可分别设为 `(0,-200,100)`、`(0,200,100)`。在各自 **Player Start Tag** 属性填 `Seat0`、`Seat1`（不是 Actor Tags，不是名称）。每个 Tag 恰好一个。出生点须有足够空间；角色胶囊半径 34、高度 176 cm。不要手动再放两名角色。
4. 可以暂不放相机：GameMode 会生成默认相机，位置 `(-1000,0,1732)`、旋转 `(-60,0,0)`、FOV 60，注视原点附近。自定义时放一个 CameraActor，在 **Actor Tags** 添加 `ArcadeBoardCamera`；使用其相机组件设置。初版应保持固定俯视、Roll=0，推荐 Pitch=-60。
5. 保存地图，PIE Number of Players 保持 **1**，使用 Play，不用 Simulate。项目已关闭分屏。编辑器启动地图如需改成 L_Main，可在 Maps & Modes 中单独设置。
6. `L_TestingLevel` 的 World Settings → GameMode Override 明确设为 `ArcadeInputDemoGameMode`，保存。这是独立输入面板入口；项目默认现为正式 GameMode，不能再依赖默认值进入 Demo。

默认角色使用引擎 Cube 占位模型与 P1/P2 彩色标签。**换模、动画、移动数值应全部在蓝图完成，不要为此改 C++。** 详细步骤见下文「角色外观、动画与数值（纯蓝图）」。

## 角色外观、动画与数值（纯蓝图）

更新日期：2026-10-20。结论：**当前源码已足够支持正式角色外观与移动手感调参，无需修改程序。** Codex / 其他代理接手时，优先按本节做编辑器资产；不要为换模或改速度去改 `ArcadeBoardCharacter` / `ArcadeBoardGameMode`，除非用户明确要求新能力（例如公共冲刺、格点吸附）。

### 推荐资产结构

| 资产 | 父类 / 类型 | 建议路径 |
| --- | --- | --- |
| `BP_ArcadeBoardCharacter` | `ArcadeBoardCharacter` | 例如 `/Game/Jiashu/Characters/` |
| `BP_ArcadeBoardGameMode` | `ArcadeBoardGameMode` | 例如 `/Game/Jiashu/GameModes/` |
| Skeletal Mesh / Skeleton / AnimBP | 美术资源 | 与角色同目录或 `Content/.../Animations/` |

两席共用同一 `BoardCharacterClass`；用 `Get Seat` 在蓝图里区分 P1/P2 外观即可，不必做两个 Character 类。

### 创建并挂上角色蓝图

1. Content Browser → 右键 → Blueprint Class → 搜索并选择 **ArcadeBoardCharacter** → 命名 `BP_ArcadeBoardCharacter`。
2. 再创建 **ArcadeBoardGameMode** 子类 `BP_ArcadeBoardGameMode`。打开 Class Defaults，将 **Board Character Class** 设为 `BP_ArcadeBoardCharacter`。
3. 在 `L_Main` 的 World Settings → GameMode Override 选用 `BP_ArcadeBoardGameMode`（或项目默认指向该蓝图）。不要在关卡里手动拖放两名角色；仍由 GameMode 按 `Seat0` / `Seat1` PlayerStart 生成。
4. 在 `BP_ArcadeBoardCharacter` Class Defaults 中确认：
   - **Auto Possess Player** = Disabled
   - **Auto Possess AI** = Disabled
   - 不要开启 **Auto Receive Input**
   - 不要自己 Bind 键盘／手柄轴；移动继续走现有 `ArcadeInputReceiver`

### 替换人物模型

`ACharacter` 自带骨骼网格组件 **Mesh**（常显示为 `CharacterMesh0`）。C++ 额外放了一个占位立方体 **PlaceholderBody** 和头顶 **SeatLabel**。

推荐做法：

1. 打开 `BP_ArcadeBoardCharacter` 的 Viewport / Components。
2. 选中 **PlaceholderBody**：取消勾选 **Visible**，或 **Hidden in Game**，或清空 Static Mesh。不要删组件名依赖；隐藏即可。
3. 选中 **Mesh**：
   - **Skeletal Mesh Asset** 设为正式模型。
   - 调整 **Location / Rotation / Scale**，使脚底贴在胶囊底附近（胶囊默认半径 34、半高 88 → 总高约 176 cm）。俯视相机下脚底对齐比第三人称跟镜更重要。
   - 需要改碰撞体时同步调 **CapsuleComponent** 的 Capsule Radius / Half Height，并保证 PlayerStart 周围仍有生成空隙。
4. **SeatLabel**（P1 蓝 / P2 橙文字）可保留作调试，正式美术做好后同样可隐藏。
5. 若 P1/P2 要用不同 Mesh 或材质：当前 GameMode 在 SpawnActor 返回后才调用 InitializeSeat；运行中生成 Actor 的 BeginPlay 已先执行，**不可假设 BeginPlay 内 Get Seat 有效**。在 BeginPlay 后延迟到下一帧，再检查 Get Seat 为 0 或 1，才 Set Skeletal Mesh / Set Material。手动放置而未由 GameMode 初始化的角色不会自动获得 Seat。

静态网格占位（不打算上骨骼动画）时：可继续只改 **PlaceholderBody** 的 Static Mesh／材质，不必使用 Mesh；动画则无效。

### 添加动画（无需改输入或移动 C++）

角色**没有**被 PlayerController Possess，但 `CharacterMovement` 已开启 `bRunPhysicsWithNoController`，速度与朝向仍由引擎更新。动画蓝图用速度驱动即可，不要依赖 Enhanced Input 或 `Get Player Controller`。

1. 为骨骼创建 **Animation Blueprint**（AnimBP），目标 Skeleton 与 Mesh 一致。
2. AnimBP 中常用：
   - `Try Get Pawn Owner` → `Get Velocity` / `Get Ground Speed`
   - 或读 `Character Movement` → Max Walk Speed，做 Idle / Walk（／Run）混合
   - 方向可用速度在角色局部空间的 X/Y，配合 Blend Space 1D/2D
3. 在 `BP_ArcadeBoardCharacter` 的 **Mesh → Anim Class** 指定该 AnimBP。
4. Class Defaults 中 **Character Movement**：
   - **Orient Rotation to Movement** 保持开启（C++ 默认 true），角色会转向移动方向，AnimBP 可做位移＋转向表现。
   - **Rotation Rate** 可调转身快慢（默认 Yaw 720）。
5. 不要在 AnimBP 或角色蓝图里重新读 WASD／手柄；方向已由 Subsystem 写入并在 C++ Tick 里 `AddMovementInput`。
6. 收到取消时 C++ 会立刻清零速度；AnimBP 应跟速度自然回到 Idle。若蓝图有蓄力等持续逻辑，实现 **On Board Input Cancelled** 自行清理。

### 修改移动等数值（Class Defaults）

在 `BP_ArcadeBoardCharacter` → Class Defaults → **Character Movement**（或选中 CharacterMovement 组件）调整，保存后对两席同时生效：

| 属性 | C++ 默认 | 用途 |
| --- | --- | --- |
| Max Walk Speed | 400 | 最大移动速度 |
| Max Acceleration | 2400 | 加速 |
| Braking Deceleration Walking | 2400 | 松开方向后的减速 |
| Rotation Rate (Yaw) | 720 | 转向速度 |
| Gravity Scale / Jump 相关 | 引擎默认 | 棋盘初版通常不跳；按需关闭或忽略 Jump |

其它常用项：

- **CapsuleComponent**：半径、半高，须与美术身高匹配。
- **Mesh** Transform：与胶囊对齐。
- GameMode 蓝图上的 **Default Camera Transform**、**Camera FOV**：俯视构图，与角色无关但影响手感观感。

不要在 Tick 里每帧 `Auto Assign Seats` 或反复改 Max Walk Speed，除非做明确的技能／状态效果。

### 刻意不要改的程序点（给 Codex）

以下行为已由 C++ 固定，换模／调参时请保持不动：

- 无第二 LocalPlayer、无双 Possess；席位 ≠ `Get Player Character` 索引。
- 移动方向相对**启动时**共享相机水平 Yaw；本阶段无镜头跟随旋转。
- `SetInputReceiver` / 取消 / 中立保护；蓝图接菜单后须 `Restore Board Input`。
- 不要为了“更方便动画”改回 Enhanced Input 或给角色 Auto Possess。

**仅当**产品需要下列能力时再改 C++，并同步更新本文与 ProjectDevelopmentGuide：冲刺／耐力、格点吸附、按席位不同移动参数的专用配置资产、或公共动画接口（例如统一 Play Celebrate）。当前换模、AnimBP、速度三项**不构成**改代码理由。

## 输入和相机

- `AArcadeBoardController` 直接继承 PlayerController，只设置 GameAndUI、鼠标不锁定及捕获时不隐藏；不会创建 Widget。光标闲置显示仍由 ArcadeInputSubsystem 管理。
- `AArcadeBoardGameMode` 在启动时按 PlayerStart Tag 生成两名角色，分别绑定 Seat 0/1。不创建第二个 LocalPlayer，不依赖 GetPlayerCharacter(0/1)，没有 AI Controller。
- `AArcadeBoardCharacter` 实现 ArcadeInputReceiver。CharacterMovement 的 bRunPhysicsWithNoController 开启，缓存方向每 Tick 送入 AddMovementInput。UE 负责重力、落地、斜坡和碰撞。
- 输入上方映射到相机启动时的水平朝向；右方映射到水平右方向。相机 yaw 只在启动时读取，本阶段没有动态旋转/跟随镜头。保留模拟摇杆幅度，斜向长度不超过 1。
- Cancelled 会清空缓存、消费尚未处理的移动向量并立即清零速度。正常松开由 CharacterMovement 制动。失焦、断连、切换接收对象继续走现有输入层保护。
- Confirm/Exit 通过角色蓝图事件 `On Board Button` 转发（含 Pressed）；当前无默认业务操作，不退出程序。`On Board Input Cancelled` 用于取消蓝图持续行为。如果覆盖原始接口事件，须调用父类实现以保留基础行为。
- 不重新分配设备；Subsystem 初次轮询按已有策略自动分配，跨地图保留选择。

## 将某一席位暂交给菜单

已有 ArcadeMenuWidget 可继续复用，本阶段不自动创建正式菜单。

1. 显式创建菜单、配置 Navigation Buttons；使用 `Set Input Receiver(Seat, Widget)`，只改操作菜单的那个席位。
2. 旧角色自动收到 Cancelled 并停止；另一席位继续操作自己的角色。
3. 关闭菜单时在销毁 Widget 前获取当前 GameMode，调用 `Restore Board Input(Seat)`。恢复时须松开方向和按钮至中立才能继续。

通过 `Get Seat Character(Seat)` 获取角色，不用 UE 玩家索引。RestoreBoardInput 是明确恢复操作，会替换该席当前目标；只在确实返回棋盘时调用，不要放 Tick。角色 EndPlay 仅清除仍指向自己的接收对象，不覆盖后来接管的菜单。

## 验证要求

自动化 `Arcade.Input.BoardReceiverLifecycle` 覆盖真实角色与 Subsystem 的两席路由、无 Possess 输入、交接取消、速度和待处理输入清理、另一席独立、返回中立保护和错误席位拒绝。`Arcade.Board.MapAudit` 只读加载两张地图并记录 Actor 和 Override，不会保存地图。

2026-10-19 验证：UE 5.8 完整 Editor Development 构建通过；`Arcade.` 共五项测试均为 Success。新增测试还运行了 60 帧带地面碰撞的 World 模拟：两名无 Controller 角色分别向前/向右移动并保持站立。报告：`Saved/Automation/Board/index.json`；日志：`Saved/Logs/BoardAutomation.log`。该测试使用 NullRHI，不证明画面、鼠标/UI 焦点或实际设备已验收；L_Main 未完成搭建，尚未实际试玩。

编辑器配置完成后仍需人工验证：两套键盘同时移动、斜向与墙壁碰撞、菜单单席切换、Alt+Tab/Shift+F1、拔插手柄、鼠标只点 UI，以及光标闲置隐藏恢复。没有手柄时用 WASD 和方向键；连接手柄时按既有优先级分配来源。自动化不能代替焦点/UI/硬件实测。

## 卡顿排查记录（2026-10-20）

症状：PIE 中每隔约数秒画面卡住，之后角色位置向前闪现；快速点按移动时卡顿后位移仍明显偏大。降画质／换低分辨率副屏后仍存在，故不像单纯 GPU 压力。

原因：`FArcadeSonyInput::Poll` 曾在**游戏线程**每 1 秒调用 `IDirectInput8::EnumDevices`。该 API 已知会长时间占用 DirectInput 锁并解析设备 INF（Allegro／部分游戏曾因此改掉“每秒枚举”）。卡顿放大单帧 `DeltaSeconds`，`CharacterMovement` 仍按缓存方向积分，表现为闪现。即使未连接 PS 手柄，只要 Windows 上创建了 Sony 后端也会每秒枚举。

修复：改为按需扫描（见 `ArcadeSonyInput.cpp` / InputSystem.md）。验收：持续移动 30 秒以上应不再出现约 1Hz 的规律卡顿；PIE 控制台可用 `stat hitch`／`stat unit` 对照。新插入的 PS 手柄需 Reassign／`AutoAssignSeats` 后才会进入来源列表。

2026-10-21 更新：Confirm 按下现在优先尝试所在格子的小游戏交互，成功后消费该按下；未成功才继续 OnBoardButton。详见 [BoardCells.md](BoardCells.md)。覆盖原始接口仍须调用父类。
