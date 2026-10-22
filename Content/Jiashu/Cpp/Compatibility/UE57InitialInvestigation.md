# UE 5.7 降级检查（2026-10-22）

## 当前结论：尚未完成资源降级，不要直接切换原工程

本机目标引擎为 `E:\UE\UE_5.7`，实际版本 **5.7.4**；原引擎为 `E:\UE\UE_5.8`，实际版本 **5.8.3**。

C++ 模块已经在 5.7.4 上完整编译成功，包括 UHT、编译、链接。没有遇到需要修改的 C++ API 不兼容。**这不代表地图和蓝图兼容。** 全部 15 个 `.uasset` 和 3 个 `.umap` 都是 5.8 保存的，5.7 的资源注册器明确拒绝读取；项目目前仍保持 `EngineAssociation=5.8`。

本次没有改动玩法源码、正式蓝图或关卡，没有关闭引擎的版本检查。原始 18 个资源与检查前备份的 SHA-256 全部一致。失败的转换试验只发生在 `Saved/UE57Migration/Staging`，这个目录**不是可用的 5.7 工程**。

## 检查证据

- 5.7.4 Editor / Win64 / Development 完整构建成功。Windows XInput、DirectInput、Slate、UMG、动态关卡实例、SceneCapture 等项目代码参与了实际编译。
- `Arcade.` 编辑器测试共 13 项：11 项 Success，2 项 Fail，失败项为 `Arcade.Board.MapAudit`、`Arcade.Minigame.DynamicInstances`，涉及项目地图读取。不能把本次测试写成全部通过。
- 本次没有完成 5.7 游戏运行、GPU 画中画、硬件输入或打包验证：资源尚不能正确加载，不能开展有效的完整玩法验收。
- 原始完整报告已保存在 [Compatibility/UE57Baseline.json](UE57Baseline.json)。资源错误见 [Compatibility/UE57AssetLoadErrors.txt](UE57AssetLoadErrors.txt)。
- 18 个资源的包版本、导入类及导入/导出对象数量见 [Compatibility/UE58AssetInventory.json](UE58AssetInventory.json)。类清单用于重建参考，不代表每个蓝图节点已在 5.7 中编译验证。
- 检查结束后，用原 UE 5.8.3 重新构建模块并运行 13 项编辑器测试，全部 Success，0 项测试警告、0 项失败；报告见 [Compatibility/UE58Recovery.json](UE58Recovery.json)。当前 Binaries 与 5.8 工程关联匹配。

基础包格式 `UE4=522 / UE5=1018 / Legacy=-9` 虽然相同，自定义版本仍不兼容。资源按实际使用的序列化器记录以下版本：

- FortniteMain：5.8 资源为 268，5.7 上限 225。
- UE5-Main：123 → 121。
- UE5-Release：68 → 61。
- FortniteRelease：20 → 17。

这里的 Fortnite 名称是引擎内部通用序列化版本标识，不表示本项目使用了 Fortnite 玩法插件。

已对照本机两版引擎源码，发现 FortniteMain 的 `AddDevNotesToFText=260` 在编辑器 FText 序列化中增加数据。另有枚举、动态委托、物理/动画等格式变化。不能把所有自定义版本号调低就认定完成转换。

## 已尝试的转换及失败原因

1. 使用 5.8 自带 `TextAsset -mode=ResaveText` 导出全部 18 个资源：0 errors、0 warnings。原始二进制资源未被覆盖。
2. 在独立 Staging 工程中试验结构化文本的旧版本读取。只在副本中调整版本描述，未修改正式资源。
3. 5.7 读取复杂蓝图时触发引擎数组越界断言；单独读取 `BP_Testing` 时也报告 `SCS_Node:CategoryName` 文本属性读取长度不符：Read 37B, expected 41B。
4. 因此该文本路径仍包含不兼容的二进制序列化段，不能直接作为可靠的降级工具。没有将试验文件发布到正式 Content。

本机诊断文件（Saved 不进 Git，清理前自行保留需要的证据）：

- `Saved/Logs/UE57Baseline.log`
- `Saved/Logs/UE58TextExport.log`
- `Saved/Logs/UE57TextLoad.log`
- `Saved/Logs/UE57TextIsolated.log`
- `Saved/UE57Migration/OriginalContent/Jiashu/`：原始文档及二进制资源额外备份。
- `Saved/UE57Migration/Text58/Jiashu/`：5.8 结构化文本导出，供核对对象、属性、引用；不是可以直接导入 5.7 的成品。

## 如何真正迁移到 5.7

UE 不提供官方的通用“另存为旧版本”转换。Epic 的规则是新版可以升级读取旧版资源，而旧版不能保证读取新版保存的资源：

- [资源与包的版本机制](https://dev.epicgames.com/documentation/unreal-engine/versioning-of-assets-and-packages-in-unreal-engine)
- [Unreal Engine 迁移指南](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-migration-guide)

### 首选：有 5.7 保存的历史资源

如果 Git 历史或其它备份中存在升级前的 5.7 资源，可在**新副本**中恢复这些资源，保留本次已验证可编译的当前 Source。仅有当前 5.8 工程的备份不能解决旧版读取问题。升级后新增的蓝图和关卡改动仍需在 5.7 重做或可靠转换。

本仓库最早提交 `050f543` 的工程关联已经是 5.8，未找到 5.7 工程关联的历史版本；不要假设 Git 一定有可恢复的 5.7 基线。

### 没有旧资源：在 5.7 重建资源容器并迁移内容

1. 保留当前 5.8 工程为参考。另建工程目录，保持项目名及模块名 `BetaArcadeTeamkfc`，复制 `.uproject`、`Source`、`Config`。在新副本中将 `EngineAssociation` 设为 `5.7`，不要复制 5.8 的 `Binaries`、`Intermediate` 和缓存。
2. 新副本的启动地图暂设为 5.7 新建的空地图，避免启动时引用尚未重建的 L_Main。使用下面的 5.7 Build 命令生成模块。
3. 在 5.7 创建相同目录、名称、父类的蓝图/Widget/数据资产，再迁移默认值、变量、组件、事件和函数。图表节点可以尝试从 5.8 复制到 5.7，但每张图都必须检查引脚、事件绑定、变量类型并重新编译；复制节点不等于复制完整蓝图。
4. UMG 同时核对 Designer 层级、控件名称、Is Variable、插槽、锚点和事件。`WBP_MinigameFrame` 的 `ViewImage`、`GameUIRoot`、`DesignSurface` 等绑定约定见 MinigamePresentation.md。
5. 在 5.7 新建三张地图。可尝试分组复制旧地图的 Actor，但必须重绑跨 Actor 引用、小游戏起终点/相机、GameMode Override、宿主配置与 UI 管理器；Level Blueprint 需单独迁移。FBX 导出只能辅助几何迁移，不能保存完整蓝图逻辑或关卡配置。
6. 恢复启动地图为 `/Game/Jiashu/Levels/L_Main`，按下方验收通过后，才将新副本作为团队正式工程。

重建资产顺序建议（同一组有互相引用时先建空资产再接线）：

- `CharacterBP/BP_ArcadeBoardCharacter`，`GameModeOverride/BP_ArcadeBoardGameMode`。
- `UI/WBP_MinigameFrame`，`SystemBlueprint/BP_MiniGameInstanceHost`。
- `MiniGames/TestMiniGame_WalkToGoal/` 下的 `BP_WTGCharacter`、`BP_WTGEntry`、`WBP_WTGTimer`、`L_WalkToGoalMiniGame`。
- `Board/DA_WalkToGoal`、`Board/DA_DefaultPool`，`SystemBlueprint/BP_ArcadeBoardCell`。
- `UI/WBP_MainMenu`、`WBP_Results`、`WBP_RootUI`，`SystemBlueprint/BP_ArcadeUIManager`。
- `SystemBlueprint/BP_Testing`，`Levels/L_TestingLevel`、`L_Main`。

以上是迁移步骤，**尚未完成上述重建**。本次没有为了“能打开”而删除或用空蓝图替换现有玩法。

### 5.7 构建命令

在 PowerShell 中执行，`-Project` 要替换为最终 5.7 副本的路径：

```powershell
& 'E:\UE\UE_5.7\Engine\Build\BatchFiles\Build.bat' BetaArcadeTeamkfcEditor Win64 Development '-Project=你的5.7副本绝对路径\BetaArcadeTeamkfc.uproject' -WaitMutex -NoHotReloadFromIDE
```

如果需要 IDE 工程文件，右键新副本 `.uproject` → Switch Unreal Engine version → 5.7，再 Generate Visual Studio project files。这两项操作只处理引擎关联和构建工程，**不会转换资源**。

### 切换完成的验收条件

- 18 个资源均能被 5.7 正常发现、加载和保存；全部 Blueprint、Widget Blueprint 及 Level Blueprint 编译成功，无丢失类/函数/引用。
- 13 项 `Arcade.` 编辑器测试全部成功。
- L_Main 的 `Arcade.Board.CellRuntime`、`Arcade.Minigame.PresentationRuntime` 以及小游戏地图的 `Arcade.Minigame.StandalonePresentationRuntime` 在真实渲染下分别通过。
- 人工验证主菜单 → 开始 → 双人移动 → 两个独立小游戏 → 占格 → 三连结算 → 再来一局 → 返回菜单，以及 UI 文本、占领颜色、镜头和鼠标/手柄。
- Win64 打包验证成功，软引用小游戏地图纳入打包；发布前进行手柄、失焦、断连等人工回归。

只有资源完成转换或重建并经过验收，才能回答“可以直接用 5.7 打开最终工程”。**当前根目录工程还不满足这个条件。**
