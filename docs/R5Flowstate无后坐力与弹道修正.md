# R5Flowstate 无后坐力与弹道修正

## 范围与操作

适用于 `D:\R5Flowstate\Game` 的独立离线训练客户端及本机服务端 `127.0.0.1:37015`。工作区为 `E:\Users\MYX\Desktop\apex`。

Insert 打开中文菜单，两个开关独立控制：

全向模式的现行实现和背向实弹验证另见 [360° 发射寻敌](R5Flowstate360发射寻敌.md)；下文的小角度对照保留其原始构建身份。

- **无后坐力**：清除客户端预测及服务端实际玩家的后坐力弹簧角度、速度。关闭后恢复正常后坐力生成；普通开火动画与武器散布保留。
- **弹道修正（不穿墙）**：开火时在所设角度圆锥内选择最接近原始发射方向的可见 NPC／玩家，修正发射方向，不自动开火。
- **弹道修正范围（度）**：0–30，默认 10。它与视角瞄准的屏幕像素范围圆分别控制。

弹道修正排除自己、无生命值目标和两类 Mirage 诱饵脚本实体。目标点使用实体真实 `EyePosition()`，与视角瞄准的五部位选择独立。先按原始开火位置进行射线遮挡检查；没有合格目标时保留原方向。原有子弹碰撞、速度、散布及弹道物理继续执行，不新增穿透能力，也不使飞行中的子弹追踪目标。

## 实现与依赖

客户端通过正常 userinfo 参数传递开关；服务端只在 `-offline` 下读取。连接参数包与服务端的离线参数登记均包含三个固定名称：`local_no_recoil`、`local_bullet_assist`、`local_bullet_angle`。角度在服务端重新限制到 0–30；其他参数名的现有检查保留。

客户端从当前预测 datamap 解析实际后坐力字段，在输入与视角阶段清除；服务端在现有 `Hook_PlayerRunCommand` 入口前后清除对应当前帧字段，同时覆盖机器人包装调用。没有再给同一命令函数挂第二个入口钩子。

弹道方向辅助函数安装在既有 R-301 共享脚本中，包装 23 个武器脚本内的 24 处 `FireWeapon_Default`／`FireWeaponBullet` 调用。没有匹配到的开火路径不作支持承诺。R-301 和 R-99 已分别实测；弓、霰弹枪等其他包装脚本尚未逐枪验收。

SDK 基于 [R5Flowstate/s21-r5sdk](https://github.com/R5Flowstate/s21-r5sdk)，本地来源提交记录为 `10077bbf020292fa0bab69d9a9a05fef125d6ba5`。编译沿用已有 VS 2022 Build Tools、MSVC v143、Windows SDK 与 CMake；接口及测试脚本使用 `E:\Python\Python313\python.exe`。

主要源码位于 SDK 目录 `E:\Users\MYX\Desktop\apex\sources\r5flowstate-sdk\s21-r5sdk-s21-unify`：

- `src\engine\client\training_weapon.cpp`：客户端开关、预测字段和诊断。
- `src\gameui\ITrainingMenu.cpp`：中文菜单。
- `src\game\shared\weapon_script_vars.cpp`：服务端开关读取和诊断接口。
- `src\game\server\player.cpp`、`translocation.cpp`：实际命令入口中的后坐力处理。
- `src\engine\client\net_bridge_c2s.cpp`、`client.cpp`：固定离线参数的两端登记。

## 部署

以下命令在工作区 PowerShell 执行。部署会重启独立客户端或服务端，不要在需要保留当前对局状态时运行。

```powershell
& E:\Python\Python313\python.exe native-diagnostics/r5flowstate_install_weapon_training.py
$cmake = 'C:\Program Files\cmake-3.29.3-windows-x86_64\bin\cmake.exe'
& $cmake -S sources/r5flowstate-sdk/s21-r5sdk-s21-unify -B artifacts/r5flowstate-draw-build -G 'Visual Studio 17 2022' -A x64 -DOPTION_RETAIL=ON -DOPTION_CERTAIN=OFF -DOPTION_WIP=ON -DOPTION_LTCG_MODE=ALL -DOPTION_WARNINGS_AS_ERRORS=OFF -DBOOST_REGEX_STANDALONE=OFF
if ($LASTEXITCODE -ne 0) { throw 'Configuration failed' }
& $cmake --build artifacts/r5flowstate-draw-build --config Release --target client server -- /m:2
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
```

若客户端仍运行，先通过 SDK 停止测试射击并关闭新增开关，再部署两端：

```powershell
& E:\Python\Python313\python.exe native-diagnostics/r5flowstate_agent.py --role client --cmd '-attack;local_no_recoil 0;local_bullet_assist 0'
& .\native-diagnostics\start_r5flowstate_weapon_server.ps1 -StopExisting
& .\native-diagnostics\start_r5flowstate_training.ps1 -StopExisting
```

等待服务端地图及客户端主菜单完成加载，再连接：

```powershell
& E:\Python\Python313\python.exe native-diagnostics/r5flowstate_agent.py --role client --cmd 'connect 127.0.0.1:37015'
```

新选项默认关闭，进入地图后可从菜单开启。客户端与服务端 DLL 必须配套：只更新客户端菜单不能完成服务端后坐力清理或开关读取。

## 实测快照

核实日期：2026-11-15，Asia/Shanghai。测试构建：

- 客户端 SHA-256：`05A7526ED596685BDAA492B8FDA062748290DBBD542374967ECE06EC517D48E4`。
- 服务端 SHA-256：`A315F6B81394F95A4FB87019C45E17056EB166C3D71D57B172B1A9BBCBCC7320`。

| 对照 | R-301 | R-99 |
| --- | --- | --- |
| 后坐力关闭，服务端角度模长峰值 | 5.66073179° | 10.25750256° |
| 后坐力开启，同样短点射 | 0° | 0° |
| 再关闭，正常后坐力恢复 | 5.64323044° | 10.60788536° |
| 偏离目标约 8°，弹道修正关闭 | 0 伤害 | 0 伤害 |
| 同样初始偏差，弹道修正开启 | 98 伤害 | 90 伤害 |

伤害对照同时核对生命值和护盾，不把护盾吸收、生命值未变误判为未命中。可见目标阶段每组实际消耗子弹；这些是通过正常开火命令产生的实际射击，不是直接调用伤害接口。

弹道函数的关闭、可见目标、圆锥外和石柱遮挡四项对照均通过。正常地面高度的补充实弹验证中，角色眼睛位于 `(-6399.93994,-930,-25.5902252)`，射线先撞石柱（fraction 0.31537184、并未命中目标），R-99 消耗 7 发，目标生命值和护盾均未减少。原始低于地面的测试不单独用于正常站位结论。

最终已停止自动射击、恢复假人正常生命值及满盾，将角色放回安全武器平台，并恢复 R-99 型号、颈部选择、232 像素范围与速度 60。两个新增选项开启，修正范围 10°，菜单保持打开。只记录武器型号恢复，未记录、也不声称恢复更新前全部拾取附件。新一次启动仍按默认关闭新选项。

原始证据目录为 `E:\Users\MYX\Desktop\apex\artifacts\r5flowstate-aim\weapon-training`：

- `recoil-r301.json`、`recoil-r99.json`：关闭→开启→关闭的实际服务端采样。
- `bullet-hit-r301.json`、`bullet-hit-r99.json`：实际消耗弹药、护盾与生命值、遮挡射线。
- `bullet-direction-verification.json`：四种方向函数与实际几何对照。
- `bullet-occlusion-safe.json`：正常角色高度下的实际遮挡点射及清理。
- `menu-weapon-training.png`：两个开关、10° 范围及原菜单选项的实际画面。
- `script-manifest.json`：23 个修改脚本的原始及安装后哈希，旁边保留原文件。
- `server-deployment.json`、上一级 `deployment-current.json`：实际启动参数与 DLL 哈希。

## 复核与退出清理

`r5flowstate_test_recoil.py` 自动执行短点射；`r5flowstate_test_bullet_hits.py` 还会调整人物位置与朝向、临时提高假人生命值。运行前需固定视角并停止实际输入。

后坐力测试需要玩家持有主武器；脚本在各阶段填充弹匣，关闭视角瞄准与弹道修正，最后发送 `-attack`。命中测试必须先重新枚举 NPC 顺序，给 `--target-array-index` 指定负 Y 方向石柱附近的假人，不能把历史数组索引当成固定实体。

命中测试退出时发送 `-attack`，关闭测试开关，恢复目标原生命值上限及生命值，并恢复正常满盾。恢复报告中的具体结果需要一并检查。关闭两个菜单开关会恢复普通开火方向和正常后坐力；无需卸载 DLL。

若需要完整还原武器脚本，停止两端后执行 `r5flowstate_install_weapon_training.py --restore`。工具会核对安装后哈希再复制原文件，拒绝覆盖其他修改。安装前客户端和服务端 DLL 分别保存在 `client-before-weapon-training.dll`、`server-before-weapon-training.dll`。

## 附录：诊断过程与限制

最初的后坐力逻辑只接到机器人包装路径，真人路径仍有约 5° 峰值，已改为扩展实际入口。userinfo 初始包存在先后顺序差异，已在两端登记固定离线键。

一次命中诊断错误地用整数格式输出布尔值，在开枪前触发脚本错误与地图退出；中止报告单独保留。随后重载地图、重新枚举目标并完成命中测试。另一次恢复界面时出现引擎任务等待，线程栈为 `combination-stack.txt`。当时测试角色曾低于地面，不能仅凭栈证明二者因果；已恢复客户端并改用地面高度完成复核。恢复后接口可响应，瞄准、高亮与两个新选项同时启用，实际菜单画面已保存；这不构成长时间稳定性结论。

本轮不证明全部武器、移动真人和长时间运行均已验收，也不证明历史主菜单退出问题已解决。
