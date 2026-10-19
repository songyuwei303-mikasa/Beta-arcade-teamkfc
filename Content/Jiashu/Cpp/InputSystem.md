# 输入系统第一版

## 运行

本工程现在包含原生 C++ 模块 BetaArcadeTeamkfc。打开工程后，使用项目默认 GameMode 按 Play，会显示输入测试面板。不需要连接蓝图节点。

如果某张地图已有 GameMode Override，把它设为 None（继承项目默认值），或临时选择 ArcadeInputDemoGameMode。该 GameMode 只是输入测试工作台，正式棋盘实现时应替换；输入 Subsystem 不依赖它。

面板使用英文以避免系统字体缺少中文字形。蓝色为 P1 的导航选择，橙色为 P2，紫色表示两人选中同一个按钮。鼠标可以直接点击任意按钮。方向导航采用列表顺序、循环和按住重复（首次 0.4 秒，后续 0.12 秒）。选择按键在按下时触发 OnClicked；同时按着两个确认键只算一次确认。

Escape 是 P1 退出动作，在面板中仅显示退出请求，不关闭程序。PIE 中使用 Shift+F1 释放焦点，再点编辑器停止按钮。方向 X 向右为正、Y 向上为正；键盘斜向归一化，摇杆采用径向死区和幅度重映射。

## 默认来源与绑定

- Keyboard1：WASD，Space 确认，Escape 退出。
- Keyboard2：方向键，Enter 或 NumPad0 确认，Delete 退出。
- Gamepad1–4：Windows XInput 槽位 0–3；左摇杆或十字键方向，南/西面键（Xbox A/X）确认，东面键（B）退出。
- PlayStation1–4：原生 Sony Windows HID/DirectInput 设备；DualShock 4（PID 05C4/09CC，含 0BA0 接收器）、DualSense（0CE6）、DualSense Edge（0DF2）。左摇杆或十字键方向，叉/方块确认，圆圈退出。USB 和蓝牙通过相同逻辑映射；连接方式切换可能产生新的设备身份，需要重新分配。
- Mouse：共享 UI 指针，不属于任何玩家，不发送玩法输入。移动、点击、滚轮恢复显示，默认静止 3 秒隐藏。

方向合并规则：十字键非零时优先，否则使用经死区处理的摇杆。十字键斜向归一化，避免加速；放开十字键立即恢复摇杆。

自动分配：连接的 XInput 手柄按槽位顺序排列，其后为原生 Sony 手柄，其后 Keyboard1、Keyboard2，前两项分别给 P1/P2。初次启动立即轮询手柄，无需按键才能发现。运行过程中不自动抢占席位；点击测试面板的 Reassign 按钮可重新应用优先级。

断连会发送取消并保留原席位。重连后需要所有控制回到中立再继续。XInput 槽位不是永久硬件身份；重连若换槽位，需重新分配。这一版不包含正式断连弹窗或全局暂停规则。

## 蓝图接入

1. 通过 Get Game Instance Subsystem，选择 ArcadeInputSubsystem。
2. 让目标 Actor、Actor Component 或 Widget 在 Class Settings 中实现 ArcadeInputReceiver 接口。
3. 调用 Set Input Receiver：Seat 为 0（P1）或 1（P2），Receiver 指向目标对象。
4. 在目标实现 Arcade Direction（二维向量）、Arcade Button（Confirm/Exit 与 Pressed）、Arcade Input Cancelled。

Arcade Direction 在值改变时触发，回到中立会发送零向量；持续运动应存储该值，在玩法更新中使用。Arcade Button 在聚合按住状态改变时触发。取消事件不是正常松开：收到取消时应停止移动、取消蓄力并清除本地输入状态，不要当成发射。

Set Input Receiver 会取消旧目标的输入，并要求输入回到中立，防止进入小游戏的那次确认键顺便触发小游戏操作。输入目标是弱引用，不会延长被销毁 Actor 的生命周期。用空 Receiver 可以停止该席位的事件投递。当前没有接收对象栈，弹窗关闭后由调用者恢复先前目标。

Get Seat State 可观察当前来源、连接状态、方向和按钮状态。On Seat Changed 通知分配/断连/重连。Get Available Sources、Assign Seat、Auto Assign Seats 可用于后续输入选择界面；Assign Seat 拒绝占用另一个席位的设备，交换设备需要先解除原分配。None 表示解除分配。

## UI

测试面板新增 Toggle UI navigation / input-only test 按钮。关闭导航后仍显示方向和按键事件，但不会导航或激活按钮；鼠标仍可操作。该开关只影响测试面板，不改变公共输入接口。

用 ArcadeMenuWidget 作为 Widget Blueprint 父类。在 Construct 时用 Set Navigation Buttons 提供有序 UButton 数组，再把 Widget 设为相应席位的接收对象。按钮的 OnClicked 同时用于鼠标点击和玩家确认。每个席位的选择独立，鼠标不改变设备归属。On Menu Exit 和 On Selection Changed 可在蓝图扩展。

这是显式列表导航，不是自动发现任意 Widget 的空间布局；文本输入框、滑条和复杂网格导航不在第一版支持范围。面板消失时自动解除仍指向它的接收对象。以后若自行覆盖接口事件，应按需调用父类实现来保留导航。

鼠标展示/输入模式目前由测试 Controller 设置；将来正式 Controller 应设置 GameAndUI、取消鼠标锁定，并由 Subsystem 控制 bShowMouseCursor。不要再将鼠标轴或按键接到玩法对象。

## 实现边界

- 来源层为 Slate 键盘预处理器 + Windows XInput 轮询 + Sony DirectInput 后端；没有将这两种后端伪装成两名 UE LocalPlayer，也没有创建 Enhanced Input 资源。两套键盘是同一硬件上的逻辑来源。
- 采用这一实现是为了在按键进入 UMG 前统一消费自定义确认/退出映射，并在第一帧识别 XInput 手柄。后续增加其它设备后端时应保持接收接口不变。
- XInput 支持 Xbox/兼容手柄；Sony 后端覆盖上述原装 PS4/PS5 型号的 Windows 标准 HID 布局，设备每秒发现一次、已连接设备逐帧读取。PS3、未知第三方型号和 Switch 尚未支持。非 Windows 平台目前只有键盘来源。原生 Sony 后端不需要 DS4Windows；若同时使用虚拟 Xbox 映射且未隐藏原始手柄，可能出现同一手柄两份来源，不应把两份都分配给玩家。某些软件切换的蓝牙增强报告模式不符合 Windows 标准 DirectInput 布局，应退出该软件并重新连接原生模式。
- 只处理当前游戏视口拥有焦点时的玩家输入。失焦取消所有输入，避免后台窗口控制游戏。鼠标事件继续交给原生 UMG。
- 当前按键由代码固定，来源选择接口已暴露；任意重绑定界面尚未实现。
- 输入取消/连接变化是基础通知，不会自行实施整个游戏的暂停。正式游戏应监听后决定暂停/重连策略。
- 本地试玩请保持 PIE Number of Players = 1；两名逻辑席位在同一个游戏实例内。这不是网络多人 PIE，也不启用引擎自动分屏。

## 验证

自动化测试位于 Source/BetaArcadeTeamkfc/Private/ArcadeInputTests.cpp，在 Session Frontend / Automation 中搜索 Arcade.Input。

覆盖键盘方向与独立性、对向抵消、双确认键聚合、鼠标隔离、设备优先级、重复认领拒绝、断连取消与重连中立保护；另覆盖十字键方向、归一化、摇杆/十字键优先级、PS 来源分配和断连。

实际硬件验收：无手柄、一个手柄、两个手柄分别启动；同按两套方向；A/X 重叠按住；拔插手柄；Alt+Tab；鼠标闲置隐藏与恢复；两位玩家确认和鼠标点击同一按钮。自动化状态测试不代表物理硬件与编辑器焦点路径已经人工验证。


本次验证：UE 5.8.3 编译通过，Arcade.Input 三项自动化测试通过。离屏运行实际识别到蓝牙 DualShock 4（PID 09CC），与 XInput 手柄分别分配到两席；日志见 Saved/Logs/InputSonySmoke.log。截图见 Saved/Screenshots/WindowsEditor/ScreenShot00002.png。此验证确认设备读取成功，不等同于人工逐键测试；PS5/USB 组合尚无实机验证。

