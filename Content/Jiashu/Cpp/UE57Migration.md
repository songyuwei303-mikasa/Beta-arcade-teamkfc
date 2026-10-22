# UE 5.7.4 迁移完成记录（2026-10-22）

## 使用哪个工程

新的工作目录：`F:\UnrealProject\BetaArcadeTeamKFC_UE57\BetaArcadeTeamkfc`。
打开其中的 `BetaArcadeTeamkfc.uproject`，关联已经是 **5.7**，本机验证版本为 **5.7.4**。
原目录 `F:\UnrealProject\Beta-arcade-teamkfc` 保持 5.8.3 的源码、资源和工程关联，供回退参考。不要把原工程的 5.8 二进制 Content 再覆盖到新目录。

## 完成的工作

- 原 Source、Config 和项目开发文档已迁移；玩法 C++ 与原工程一致，未用代码绕过资源加载失败。新增菜单端到端自动化，并将旧棋盘测试的 4 条“菜单阶段接收者为空”断言更新为验证真实菜单接收输入。
- 15 个资源包（13 个蓝图/Widget、2 个数据资产）和 3 张地图已在 5.7 中重建并保存，包括小游戏 Level Blueprint。
- 保留原模块名、资源路径、父类、成员变量、事件/函数图、组件、默认值及 UMG 控件树。
- 显式启用 PCG 与 AnimationSharing，使用 **5.7 引擎自带**的白/红/蓝材质。原蓝图引用这些插件资源；不能将对应引脚留空。
- 一次性迁移插件已经移出 Plugins，归档到 `Saved/MigrationTool`。正式工程运行、编译和打包不需要该插件。

这次采用对象属性和图表文本重建，不是修改 5.8 资源包的版本号。早期失败的包级转换试验是历史记录，见 [UE57InitialInvestigation.md](Compatibility/UE57InitialInvestigation.md)。

## 验证证据

- UE 5.7.4 Editor / Win64 / Development 构建成功。
- 13 项 `Arcade.` 编辑器自动化全部通过，零测试警告。
- 真实 GPU 下分别通过 `Arcade.Board.CellRuntime`、`Arcade.Minigame.PresentationRuntime`、`Arcade.Minigame.StandalonePresentationRuntime`，零测试警告。另通过新增的 `Arcade.UI.MenuRuntime`，覆盖蓝图 BeginPlay 创建 UI、Start 按钮实际触发对局、HUD 切换和返回菜单。总计 **17 项成功、0 失败**。
- 图表比对覆盖 1,014 个资产引脚记录及小游戏 Level Blueprint 的 40 个引脚。原有已连接引脚及默认值保留；本地化文本身份由新包重新生成，显示字符串不变。
- 7 个原有未连接引脚由 5.7 编译器重建/移除：未连接的 BreakVector2D 节点 4 个引脚，以及 3 个运算节点的隐藏 ErrorTolerance 引脚。没有删除已连接逻辑。
- 5 个 Widget 的 177 项导出控件树属性一致。
- 三张地图分别核对 22、7、10 个主要 Actor；名称、类型和检查到的位置/旋转/缩放、材质、席位、格子坐标、池及交叉引用一致。引擎自动生成的管理 Actor、WorldSettings 和 Brush 不计入此数量；GameMode 另外由地图回归测试验证。
- Windows Development Build/Cook/Stage/Pak/Archive 全部成功，Cook 处理 485 个依赖包，退出码 0。最终打包程序又通过菜单、九宫格、双实例画中画、独立小游戏 **4 项运行测试**，零测试警告。
- 所有 312 个原有蓝图节点（含小游戏 Level Blueprint）的启用状态与原工程一致。修复了空白蓝图的默认禁用事件覆盖原 BeginPlay/Tick 的问题；菜单及玩家红蓝标识已通过截图检查。
- 打包程序的自动化报告缺少引擎 HTML 模板，因此只生成 JSON；日志中相应报告模板错误不属于玩法测试失败。测试结果以 JSON 中的 state、failed、succeededWithWarnings 为准。

机器可读结果：[测试汇总](Compatibility/UE57FinalValidation.json)、[蓝图/控件结构比对](Compatibility/UE57StructuredAudit.json)、[地图比对](Compatibility/UE57MapAudit.json)、[节点启用状态](Compatibility/UE57NodeStateAudit.json)。
完整本机日志位于 `Saved/Logs/UE57_Final_*.log`，完整自动化报告位于 `Saved/Automation/UE57_Final_*`。渲染截图在 `Saved/Screenshots`。Saved 不纳入 Git。

## 现在如何打开

1. 从 UE 5.7.4 的项目浏览器选择新目录中的 `.uproject`；也可以直接双击它。
2. 若 Windows 文件关联仍指向 5.8，使用右键 **Switch Unreal Engine version → 5.7**，或明确通过 `E:\UE\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe` 打开新工程。不要接受升级到 5.8。
3. 已生成匹配 5.7 的 Visual Studio 工程文件。打开 `/Game/Jiashu/Levels/L_Main`，Play。正式流程应先显示主菜单，不会自动开局。
4. 如需 Visual Studio 解决方案，在新 `.uproject` 上执行 Generate Visual Studio project files。不要复制旧目录的 Binaries、Intermediate、DerivedDataCache 或 `.vs`。

仍需人工验收的范围：真实键盘/手柄操作、双人移动手感、主菜单按钮及再来一局/返回菜单、席位与占格颜色、窗口失焦和设备断连。自动化已验证菜单按钮链路与部分视觉内容，但不能代替真实设备试玩；这些不应写成已人工通过。

可直接试玩的 Windows 构建位于 `Saved/Packages/UE57/Windows/BetaArcadeTeamkfc.exe`，无需编辑器。该构建不提交到 Git。

## GitHub 与回退

- 新目录沿用原仓库完整 Git 历史及 origin：`https://github.com/songyuwei303-mikasa/Beta-arcade-teamkfc.git`。
- 工作分支为 `codex/ue57-migration`；本地标签 `ue58-before-migration` 指向原 5.8 基线 `0407e22`。
- 旧目录的 master 不变；迁移改动保存在本地提交中；GitHub 远端尚未推送或合并此次迁移。
- `.uasset` / `.umap` 继续由 Git LFS 管理，原历史 LFS 对象也已复制并验证。新电脑克隆前需安装 Git LFS，克隆后执行 `git lfs pull`。
- 人工试玩通过后，在新目录推送迁移分支，发起 PR 合入 master；无需新建 GitHub 仓库或删除历史。可同时推送回退标签。

```powershell
git push -u origin codex/ue57-migration
git push origin ue58-before-migration
```

合并以后，队友同步代码与 LFS，使用 5.7.4 打开，并重新生成本机编译产物。不要让不同引擎版本轮流保存同一批正式资产。
