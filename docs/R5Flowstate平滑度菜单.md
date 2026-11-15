# R5Flowstate 平滑度菜单

## 操作

独立离线客户端 `D:\R5Flowstate\Game\r5apex.exe` 中，按 Insert 打开中文菜单，拖动“平滑度”：

- 0 最快，100 最柔和。
- Ctrl+单击滑条数值可以精确输入。
- 修改后关闭菜单，再按住鼠标右键体验；松开停止，菜单打开时暂停瞄准。
- 本轮更新沿用原有跟随速度 20，对应新滑条约 26.8，不自动改变既有手感。

## 参数与实现

菜单将 0–100 的平滑度以对数刻度转换为既有 `local_aim_speed` 的 60–1：`speed = exp(log(60) × (1 − 平滑度 / 100))`。滑条越大，跟随越慢、越柔和。不同帧率下的时间增量处理、角速度上限、右键触发、目标筛选、输入命令同步与模型高亮算法沿用现有实现。

不合法数值使用既有速度 20，范围外输入会限制到 0–100。菜单只在用户修改数值时写入参数，显示不会反复改写当前设置。

## 验证快照

核实时间：2026-11-15，Asia/Shanghai。

构建 DLL SHA-256：`65ABFD00B6562D2EE9805FDDC50803864E8B0CCF036CF8917A4B8EAFA7E34FBB`。

已通过 CTest 2/2；新增断言覆盖端点、方向单调性、原速度往返转换、越界与非有限输入，以及实际角度响应快慢与不越过目标。构建日志为 `E:\Users\MYX\Desktop\apex\artifacts\r5flowstate-aim\build-smoothness-menu.log`。

实际游戏菜单显示平滑度 26.8，对应原速度 20。通过鼠标点击菜单滑条，显示变为 75.6，独立读取的实际参数变为 2.71561956，反算与滑条一致；随后恢复速度 20。保留了胸部选项、180 像素范围、NPC／其他玩家筛选、瞄准与高亮开关，并恢复更新前记录的位置和视角。菜单留在打开状态，关闭后才恢复右键瞄准。

画面证据为 `smoothness-menu-default.png`、`smoothness-menu-adjusted.png`，参数与恢复响应为 `smoothness-menu-verification.json`，均在 `E:\Users\MYX\Desktop\apex\artifacts\r5flowstate-aim`。这些证明控件实际生效，不代表替用户评定主观手感。
