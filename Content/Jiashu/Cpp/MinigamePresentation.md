# 画中画、相机与可编辑界面（阶段 E）

用户已经在 L_Main 人工验证 D：两席各自进入、移动、提交、卸载和恢复棋盘输入。团队已决定小游戏使用局部光照，不放全局灯光、天空盒等环境对象。

## 现在的公共显示结构

每个实例独有 `ArcadeMinigamePresentation`、SceneCapture、RenderTarget、`ArcadeMinigameView` 外框以及小游戏 Widget。没有第二 LocalPlayer，也不切换棋盘主相机。

```
Entry 指定的 CameraComponent
        ↓ 每帧同步相机，按 Capture FPS 拍摄
本实例 RenderTarget
        ↓
外框 Widget（ArcadeMinigameView 的蓝图子类）
  ├─ ViewImage：显示场景纹理
  ├─ GameUIRoot：放置原有计时/玩法 Widget
  └─ 设计师的边框图片、装饰和动画
```

小游戏 UI 是真正的嵌套 UMG，不是 SceneCapture 拍下的屏幕 UI。它与画面放在同一设计尺寸内，一起缩放，鼠标依然可以操作 UMG。小游戏不要 AddToViewport，不要自己创建第二份 UI；继续在 OnStarted 中使用 GetMinigameWidget。

拍摄采用仅显示本实例 Level Actor 的列表；动态生成物体也必须属于本实例 Level。这个列表隔离可见几何体，不创建独立光照世界。团队的局部光照约定仍然适用。

## 第一次测试：无需制作新 Widget

1. 完整构建后打开 UE，直接打开已有 L_Main。
2. 两个宿主保留此前的 Seat、小游戏地图、位置偏移和 Start Automatically。
3. Details 新增 Presentation Settings：Frame Class 留空，使用原生默认布局。
4. 默认 Render Resolution 为 1280×720，Design Resolution 为 1920×1080，Window Size 为 640×360，Screen Margin 为 (24,24)，Capture FPS 为 30。
5. PIE 玩家数仍为 1。P1 窗口在左上角，P2 在右上角。主相机继续显示棋盘。
6. 小窗口内应同时看到场景和原有计时 UI。分别完成两个小游戏；完成一份只关闭那一份，另一份继续。

Window Size / Screen Margin 的单位是经过 DPI 缩放的 UMG 单位，不是渲染纹理像素；窗口太大或屏幕太窄时两个窗口可能重叠，需要设计侧调整。这一版不擅自确定最终布局。

Render Resolution 决定清晰度与绘制开销，允许 16～4096 每边；Design Resolution 是小游戏 UI 的设计坐标。两者须保持相同宽高比，例如 1920×1080 对应 1280×720。改窗口大小不必提高纹理分辨率。窗口比例与设计比例不一致时，默认布局保持比例并留黑边，不拉伸画面。

## 相机如何设置

### 已有独立 CameraActor

原地图中唯一带 Actor Tag `ArcadeMinigameCamera` 的相机继续有效，无需迁移。也可以选中地图中的 Entry，在 Camera Source Actor 用吸管明确指定 CameraActor；显式指定优先，不再依赖地图标签查找。

### 第三人称或第一人称角色内部的 CameraComponent

1. 在角色蓝图内添加 Camera；第三人称通常放在 SpringArm 下，第一人称通常放在头部附近。
2. 角色仍不 Auto Possess，不创建额外 PlayerController。相机和 SpringArm 的 Use Pawn Control Rotation 通常应取消勾选，因为这里没有 Controller 驱动它们；用角色自身变换或玩法逻辑更新相机/摇臂旋转。
3. 在地图中选 Entry，将 Camera Source Actor 指向该角色。
4. 如果角色仅一个 Active 的 CameraComponent，Camera Component Tag 留空即可。
5. 如果角色有多个 CameraComponent，为所需组件添加 **Component Tags**（例如 `GameplayView`），再把 Entry 的 Camera Component Tag 填成 `GameplayView`。这次是组件标签，不是 Actor 标签。该显式标签模式不要求相机 Active，但必须唯一。

框架复制组件最终世界位置、旋转、FOV、透视/正交模式、正交宽度和相机后处理。第一人称相机的特殊 FOV/Scale 与透视近裁剪设置也被传递；Only Owner See / Owner No See 以来源角色作为视角所有者。因此不必依靠 Possess 才能让角色内相机被显示。

第一人称是拍摄位置，不代表自动提供鼠标转头。项目仍只有二维方向、确认、退出，鼠标只用于 UI；具体玩法需在这个输入范围内设计朝向控制。

### 动态切换或复杂角色

Entry 的 `Get Minigame Camera` 可以在蓝图的 Overrides 中覆写，直接返回想使用的 CameraComponent。或者在运行中修改 Camera Source Actor / Camera Component Tag。公共系统每帧重新查询，能够跟随切换。

必须返回本实例 Level 内的相机。返回别的实例/主棋盘相机、无相机、多个无法区分的相机，会拒绝开始或取消异常会话并清理；不会悄悄选择某个全局对象。异步创建的相机应在 MarkReady 前准备好。

画面比例由 Render Resolution 决定，建议相机的 Aspect Ratio 也设成对应比例；不承诺复制 PlayerCameraManager 的相机震动/Modifier、专用自定义投影或所有 CineCamera 效果。相机组件本身的运动/普通后处理走公共路径。

## 从创建蓝图开始定制外框

### 1. 创建正确的 Widget 父类

Content Browser → User Interface → Widget Blueprint。选择 All Classes 中的 `ArcadeMinigameView` 作为父类，命名 `WBP_MinigameFrame`。如果先创建了普通 UserWidget，可以在 Class Settings 中修改 Parent Class。

这不是原来的 WBP_WTGTimer。Timer 是小游戏内容 UI；Frame 是所有小游戏共用的外壳。

### 2. 在 Designer 搭建最小结构

按以下结构摆放，三个英文名字必须完全相同并勾选 Is Variable，公共代码通过它们连接内容：

```
Border / Overlay（最外层，可放边框）
  ScaleBox（Stretch = Scale To Fit）
    SizeBox：DesignSurface
      Overlay（Clipping = Clip to Bounds）
        Image：ViewImage
        Overlay：GameUIRoot
```

- DesignSurface 的宽高由代码根据 Design Resolution 设置。Designer 中可先设 1920×1080，方便预览。
- ViewImage 与 GameUIRoot 的 Overlay Slot，Horizontal Alignment / Vertical Alignment 都设 Fill。
- ViewImage 先不用设置 Brush 图片，运行时会自动连接 RenderTarget。
- GameUIRoot 留空，运行时自动加入当前小游戏的 Widget。
- ViewImage 的 Visibility 会设为 Not Hit-Testable，避免背景图片挡住 UI 鼠标点击。
- 外框 Is Focusable 取消勾选，小游戏的展示型 Widget 也建议不获取键盘焦点。
- 外层 Border 可设置 Padding，预留边框厚度。边框艺术图片可放在外层 Overlay 的最上层；装饰应设为 Not Hit-Testable，避免挡住内容 UI。

ScaleBox 很重要：它把固定设计尺寸的“画面＋小游戏 UI”整体缩放到窗口可用区域。如果只缩放 Image 而不缩放 GameUIRoot，文字和目标提示会错位。不要直接让渲染图和 HUD 各自使用屏幕尺寸定位。

### 3. 将外框交给宿主

编译并保存 WBP_MinigameFrame。在 L_Main 两个宿主的 Presentation Settings → Frame Class 中选择它。Window Size 控制外壳总尺寸；边框内部的 Padding、背景、图案在 Widget Designer 中改。

独立测试也使用同一 SceneCapture/RT/嵌套 UI 路径，默认将外框铺满视口并等比显示。如果希望独立测试显示自己的外框，创建 ArcadeMinigameTestGameMode 蓝图子类，在其 Class Defaults → Presentation Settings → Frame Class 选择同一 WBP；地图 Override 选择这个测试模式蓝图。它只是共享的测试配置，不为每个小游戏编写不同 GameMode。

## 出现与消失动画

### 出现

1. WBP_MinigameFrame 的 Animations 中创建 `Anim_Open`，例如从 Render Opacity 0 到 1。
2. 实现 Event On Presentation Opened，调用 Play Animation(Anim_Open)。
3. 使用动画结束事件（或 Bind to Animation Finished 对应 Anim_Open）调用 **Finish Opening**，Target 是 Self。

如果覆写事件，不要一开始调用父类默认实现；父类会立即 FinishOpening，导致动画未结束便开始游戏。公共宿主会等待 FinishOpening，然后才调用 Run.Start / Entry.OnStarted，正式计时不包括出现动画。忘记完成通知会触发准备超时并清理。

### 消失

1. 创建 `Anim_Close`。
2. Event On Presentation Closing 带本轮 Result；可显示 Result.Score、播放退出动画。
3. 动画结束调用 **Finish Closing**，Target 是 Self。

结束时 Entry.OnStopped / OnCleanup 已经执行；画面冻结为最近捕获的一帧，外框和小游戏 UI 保留到动画完成。不要在关闭动画中依赖仍活跃的玩法计时器或临时 Actor。游戏输入在成绩确定时就归还棋盘，不会等退出动画；外框不是再次接管输入的菜单。

Close Timeout 默认 5 秒（最长 30 秒），防止忘记 FinishClosing 导致永久保留场景。动画应短于这个超时，或者调大宿主参数。无覆写时默认立即开/关。

独立测试默认 Keep Result Visible=true，结束后冻结并保留结果直到停止 PIE。要测试退出动画，在测试 GameMode 蓝图取消该选项；此时也会调用 OnPresentationClosing。

## 材质与未来美术打磨

颜色修正验证状态：2026-10-21 关闭编辑器后已完成完整编译和链接；9 项编辑器回归测试与 1 项 L_Main 实际渲染测试全部通过，无测试警告或错误。报告位于 Saved/Automation/ColorRegression 与 Saved/Automation/PresentationColor。已检查 Saved/Screenshots/MinigamePresentation.png，两个画中画正常显示，发白现象改善；尚未做与编辑器相机预览同条件的逐像素比较。本次未重跑独立小游戏地图的渲染测试。

2026-10-21 颜色路径修正：SceneCapture 使用 `Final Color (with tone curve) in Linear sRGB gamut`，RenderTarget 使用线性 RGBA16f、TargetGamma=1，再由 UMG 做最终显示转换。不要在外框材质中额外添加 Pow(1/2.2)、Gamma 或重复色调映射来补偿亮度。自定义 UI 材质直接将 MinigameTexture 的 RGB 接到 Final Color。RGBA16f 每像素 8 字节，相比之前 RGBA8 的 4 字节，颜色纹理显存增加；默认 1280×720 两份约 14.1 MiB（不含其它渲染资源）。相机预览与 SceneCapture 仍可能因曝光历史、全局环境和渲染特性不同而有差别，不保证逐像素一致。

原生布局直接显示 RenderTarget。定制材质可以制作屏幕曲面感、扫描线、遮罩等效果：新建材质，Material Domain 选 User Interface，创建 TextureSampleParameter2D，参数名 `MinigameTexture`；连接 RGB 到 Final Color。作为矩形游戏屏幕时建议 Blend Mode=Opaque，忽略捕获纹理 Alpha。把材质指定到 WBP_MinigameFrame 的 View Material，公共代码为每个窗口创建独立动态材质并设置纹理。

这类 View Material 只影响场景 Image；叠在上面的玩法 UMG 不会自动受到同一屏幕材质变形。若未来需要所有文字也随 CRT 曲面一起弯曲，要单独设计 UI 合成路径，不能仅替换场景材质就声称完成。

二维边框、图片材质、颜色和动画现在即可在 Widget 中调整。**真正的 3D 模型边框目前没有实现**：UMG Image 不能直接填 StaticMesh。后续可以用独立的模型拍摄层，或搭建带屏幕材质的 3D 游戏机外壳。Frame 已暴露 ViewTexture、MinigameUI、Seat 和开/关事件，便于扩展，不必改小游戏玩法接口；3D 方案仍需要评估额外渲染成本和鼠标交互。

## 验收清单

- 两个窗口同时显示，计时/方向/确认互不影响，棋盘主相机不变。
- 一人完成后只关闭自己的窗口，另一人继续；再次进入创建新的 RT/UI，不遗留旧画面或旧计时器。
- 独立地图与正式画中画构图一致；改变窗口大小时 UI 与画面一起缩放。
- 测试 CameraActor、角色内相机、相机移动、相机切换；第一人称角色身体/手部可见性按预期。
- 出现动画期间不计玩法时间；消失动画完成后卸载；未配置动画时无需额外节点。
- Alt+Tab / 手柄断连仍只取消持续输入，不自动退出或暂停小游戏。
- 两个真实小游戏同时运行时测帧率。默认两张 1280×720、30 FPS 是起点，不是所有设备的性能保证。

## 已完成的验证与尚需人工验证的部分

2026-10-20 最终源码已完成 UE 5.8 Editor Development 构建。中断恢复时核对了实际源码、DLL 时间与以下报告，未重复改写或重新执行已经通过的流程：

- `Saved/Automation/Presentation/index.json`：9 项编辑器自动化成功，0 项失败，0 项测试警告。覆盖输入、会话、动态实例，以及 CameraActor/角色内相机选择、切换、跨实例拒绝、视角参数同步和原生嵌套 Widget。
- `Saved/Automation/PresentationRendered/index.json`：1 项真实 GPU 渲染测试成功，0 项测试警告。在现有 L_Main 加载两份用户样例，读取不同 RT 的有效像素，检查左右窗口实际位置、嵌套 UI、主相机保持不变；提交 P1 成绩后验证其显示/实例清理、棋盘输入恢复及 P2 继续。
- `Saved/Automation/StandalonePresentationRendered/index.json`：1 项独立运行渲染测试成功，0 项测试警告。验证同一显示路径启动、嵌套 UI、成绩接受与结果画面保留。此测试不证明场景照明足够明亮。
- 已人工检查生成的截图：`Saved/Screenshots/MinigamePresentation.png` 为左右双窗口，`MinigameSeat0.png` / `MinigameSeat1.png` 为场景纹理，`MinigameStandalone.png` 为独立测试结果。

只读灯光核查记录在 `Saved/Logs/MinigameLightAudit.log`：样例地图 10 个 Actor，**没有 LightComponent**。因此独立地图的场景偏黑，而 L_Main 中的实例可以受到主地图灯光影响。这不是 UI 丢失或相机引用失败。请在小游戏地图里添加有限范围的 Point Light / Spot Light / Rect Light（初次测试使用 Movable，范围覆盖玩法区域），调节强度至可读，不恢复全局天空或 Directional Light。核查没有保存或修改地图。

尚需人工验收：真实键盘/手柄下的双窗口完整游玩、实际角色第一/第三人称视角、设计师创建的自定义 Frame Widget 及开关动画、不同窗口尺寸与性能。角色内相机已通过接口/参数自动化，不应写成已经实机试玩所有第一人称角色模板。3D 模型边框与打包没有完成，也不属于本次默认外框的已验证功能。
