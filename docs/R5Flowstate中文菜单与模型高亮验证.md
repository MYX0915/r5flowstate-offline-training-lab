# R5Flowstate 中文训练菜单与模型高亮验证

## 适用范围与操作

独立离线安装为 `D:\R5Flowstate\Game`，使用 DX11 `r5apex.exe`，连接 `127.0.0.1:37015`。功能由此安装的 SDK `client.dll` 加载。工作区为 `E:\Users\MYX\Desktop\apex`；本文命令在该工作区的 PowerShell 执行。

- `Insert` 打开或关闭中文菜单。
- 勾选“启用辅助瞄准”，关闭菜单，将目标放入中心圆，持续按住鼠标右键；松开停止，不自动开火。菜单打开或游戏失焦时暂停。
- 部位可选头、颈、胸、手、腿，使用实际模型骨骼；手对应手腕，腿对应膝盖。NPC 和其他玩家分别开关，自己和诱饵排除。
- 圆半径为屏幕像素，范围 20–600。指定部位必须位于圆内且射线可见。
- “模型高亮（实验）”控制绿色外围与被遮挡部分的红色填充，无遮挡区域保留原模型材质。人物方框关闭。
- 启动脚本默认关闭瞄准和模型高亮，进入训练场后从菜单开启。

右键写入路径的现行修复、对应 DLL 哈希与实际角度对照见 [右键瞄准故障排查](R5Flowstate右键瞄准故障排查.md)。以下高亮截图快照对应其注明的构建；不能用早期写入计数代替修复后的跟随验证。

菜单使用 0–100 的“平滑度”调节快慢，数值越大越柔和，具体映射与更新构建见 [平滑度菜单](R5Flowstate平滑度菜单.md)。

无后坐力与保留遮挡判定的弹道修正为独立开关，两端部署与 R-301／R-99 实弹验证见 [射击训练选项](R5Flowstate无后坐力与弹道修正.md)。

360° 最近可见目标模式及背向实弹验证见 [360° 发射寻敌](R5Flowstate360发射寻敌.md)。

## 实现

菜单使用 SDK 自带 ImGui。中文字体来自本机 `C:\Windows\Fonts\msyh.ttc`，副本安装于 `D:\R5Flowstate\Game\platform\resource\training_msyh.ttc`，字号 18；字体配置为同目录 `imgui_fonts.txt`。字体不作为公开分发资产。

骨骼与射线可见性由引擎客户端脚本逐帧采样。DLL 接收有界快照，核对实体句柄与序列、生命值、投影范围和数据新鲜度，然后平滑调整视角。瞄准不依赖高亮颜色。

高亮使用实际角色模型，不创建代理模型。原生上下文项在当前镜像中为 52 字节；测试配置内部函数 2、外围函数 169、绿色参数和半径 2。客户端脚本关闭时恢复保存的上下文、颜色、内外高亮开关并交回原生高亮管理。

SDK 在 D3D11 像素着色器创建时识别指定高亮合成着色器，仅在长度、SHEX 指纹和目标指令全部匹配时修改。内部函数沿用真实模型深度和场景深度，遮挡阈值改为 0.05，填充强度改为 1，颜色改为红色；外围继续使用绿色参数。修正 DXBC 校验后由实际 D3D11 设备创建。未匹配或创建失败时保留原着色器，不启用此高亮配置。

填充由 GPU 逐片段深度比较决定。验证脚本的骨骼射线只用于寻找视角，不能代替画面判定。透明植被碰撞与可见深度可能不同，完整遮挡对照采用实心石柱。

## 构建与部署

依赖已有 VS 2022 Build Tools、MSVC v143、Windows SDK 和 CMake。新增原生代码在客户端 `SDK_WIP` 构建启用。Python 为 `E:\Python\Python313\python.exe`。

```powershell
$cmake = 'C:\Program Files\cmake-3.29.3-windows-x86_64\bin\cmake.exe'
& $cmake -S sources/r5flowstate-sdk/s21-r5sdk-s21-unify -B artifacts/r5flowstate-draw-build -G 'Visual Studio 17 2022' -A x64 -DOPTION_RETAIL=ON -DOPTION_CERTAIN=OFF -DOPTION_WIP=ON -DOPTION_LTCG_MODE=ALL -DOPTION_WARNINGS_AS_ERRORS=OFF -DBOOST_REGEX_STANDALONE=OFF
if ($LASTEXITCODE -ne 0) { throw 'Configuration failed' }
& $cmake --build artifacts/r5flowstate-draw-build --config Release --target client -- /m:2
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
& .\native-diagnostics\start_r5flowstate_training.ps1 -StopExisting
```

部署脚本只停止路径匹配的独立客户端，等待退出后复制 DLL，仅对共享冲突重试，复制或哈希不一致时停止。保存同构建 DLL、PDB 与 `deployment-current.json`。已有客户端且不准备重启时不要执行部署。

启动脚本不启动服务端。已有服务端需为本机独立离线训练场；尚未运行时：

```powershell
Start-Process -FilePath 'D:\R5Flowstate\Game\r5apex_ds.exe' -WorkingDirectory 'D:\R5Flowstate\Game' -WindowStyle Hidden -ArgumentList @('-dedicated','-port','37015','-devsdk','-dev','-logfiles','+net_usesocketsforloopback','1','-offline','+sv_onlineAuthEnable','0','+spire_host_visibility','0','+sv_cheats','1','+launchplaylist','survival_firingrange','+map','mp_rr_canyonlands_staging_mu1','+sv_modPolicy','0')
```

等待客户端到主菜单，再通过 SDK 本机认证接口连接：

```powershell
& E:\Python\Python313\python.exe native-diagnostics/r5flowstate_agent.py --role client --cmd 'local_model_shader_status; connect 127.0.0.1:37015'
```

进入训练场后：

```powershell
& E:\Python\Python313\python.exe native-diagnostics/r5flowstate_agent.py --role client --cmd 'local_model_highlight 1; local_aim_enable 1; local_aim_circle 1; local_entity_boxes 0'
```

接口读取本机 SDK 认证文件，不输出令牌。不要通过关闭“R5F Client”控制台来隐藏它，该窗口属于游戏进程。

## 实测快照

核实时间：2026-11-15，Asia/Shanghai。客户端 PID 为 27392，仅标识本次进程，后续需重新枚举。

已部署 DLL SHA-256：`AF75BA8C0F825E5BBC0466988A0CF6A15248DBF1E7716D2FD563AC56F5EEA638`。

| 项目 | 本轮观察与证据 |
| --- | --- |
| 原生 GPU 创建 | `ready=1 created=7073 matched=1 dumped=0`；实际设备接受修改后的 84768 字节着色器 |
| 无遮挡 | `highlight-native-visible.png`：保留原模型材质，只有绿色外围，无红色填充 |
| 局部遮挡 | `highlight-native-partial.png`：石柱挡住的身体红色，露出的头部未填充，绿色外围保留 |
| 完全遮挡 | `highlight-native-full.png`：实心石柱后全身红色、外围绿色 |
| 关闭恢复 | `highlight-native-disabled.png`：同一石柱视角，自定义红色实填和绿色外围消失，恢复训练场原有样式；实体上下文恢复为 32 |
| 菜单 | `menu-native-final.png`：中文正常，开关、部位、半径与速度可见，菜单打开状态为“菜单已打开” |
| 方框 | 上述画面均无人物方框，`local_entity_boxes=0` |
| 临时相机 | 每次 16 秒后清除菜单相机、销毁相机实体并恢复原瞄准开关；随后回到玩家视角 |
| 数学测试 | 本轮 CTest 再次运行，人物投影与瞄准数学 2/2 通过 |
| 离线着色器检查 | D3D11 WARP `CreatePixelShader` 返回 HRESULT 0，与实际设备创建分别记录 |

图片及报告位于 `E:\Users\MYX\Desktop\apex\artifacts\r5flowstate-aim`。图片均为实际游戏截图。证据哈希与当前接口响应为 `verification-native-final.json`。

前序用户持续按右键测试见 `manual-rmb-after.json`，累计视角写入由 0 增为 660。前序五部位报告为 `body-part-runtime.json`，当前假人模型索引分别为 12、11、7、41、66；不是其他模型的固定索引。这些不是本次重启后重新执行的人工按键结果。最终 DLL 已包含自动启动的脚本采样，无需额外从接口加载采样脚本。

另一名真人参与的对照尚未执行。静态假人截图不能证明移动真人跟随精度、全部动画姿态和所有材质边缘行为；持续跟随精度与全部边界情形仍需实操评估。

## 再次验证

先枚举当前 NPC 索引，不复用历史索引。`native-diagnostics\r5flowstate_occlusion_camera_probe.py` 支持 `visible`、`partial`、`full`：

```powershell
& E:\Python\Python313\python.exe native-diagnostics/r5flowstate_occlusion_camera_probe.py partial --target-index 8119
```

`8119` 仅为本轮目标，原点 `(-6399.9375,-1150.09375,-86.34375)`。本轮局部遮挡相机 `(-6399.9375,-800.09375,93.65625)`；完全遮挡相机 `(-6399.9375,-930.09375,-56.34375)`。工具寻找已有几何，不生成遮挡物。每次等待至少 16 秒并确认视角恢复再启动下次，避免清理覆盖。它会恢复测试前的瞄准开关，所以最终启用瞄准应在相机结束后执行。

关闭对照命令为 `local_model_highlight 0`，重新开启为 `local_model_highlight 1`。保存最终状态：

```powershell
& E:\Python\Python313\python.exe native-diagnostics/r5flowstate_capture_final_status.py
```

## 附录：源码、证据与参考

SDK 目录为 `E:\Users\MYX\Desktop\apex\sources\r5flowstate-sdk\s21-r5sdk-s21-unify`：

- `src\gameui\ITrainingMenu.cpp`：中文菜单。
- `src\engine\client\training_aim.cpp`：逐帧采样与瞄准。
- `src\engine\client\training_highlight.cpp`、`training_highlight_script.h`：实际模型上下文与恢复。
- `src\engine\client\training_highlight_shader.cpp`：限定指纹修改与实际 D3D11 创建。
- `src\thirdparty\dxbc_hash`：Microsoft 校验实现及许可证。

固定版本参考：

- [R5Flowstate/s21-r5sdk](https://github.com/R5Flowstate/s21-r5sdk)：本地来源记录提交 `10077bbf020292fa0bab69d9a9a05fef125d6ba5`。
- [Microsoft DirectXShaderCompiler](https://github.com/microsoft/DirectXShaderCompiler/blob/49fea47e722b14066d679e605e04b62ba0816b8d/lib/DxilHash/DxilHash.cpp)：提交 `49fea47e722b14066d679e605e04b62ba0816b8d`，采用 `ComputeHashRetail`，保留其许可证。
- [powzix/ooz](https://github.com/powzix/ooz/tree/05038060aa68f9187ae9923b2388ca8db40e58d1)：提交 `05038060aa68f9187ae9923b2388ca8db40e58d1`，GPLv3 独立分析工具，未链接客户端。分析中分别解压有效前缀和尾段后重建包体，不是原版工具单次完整解压成功。

反汇编、修改记录与 WARP 检查在 `artifacts\r5flowstate-aim\shaders`、`runtime-shaders`；构建日志为 `build-runtime-shader-profile.log`。本轮客户端日志为 `D:\R5Flowstate\Game\platform\logs\client\de036d9c-3d59-402c-8969-2725e90330ac`，error.log 仅观察到原有 `engine/lightsprite` 材质缺失提示，不能据此概括历史错误均已修复。

前序代理模型未达到颜色组合要求，不用于最终实现。连续原生骨骼调用候选曾出现 `0xC0000374`，根因未独立证实，生产路径改用引擎脚本采样。文件锁曾导致旧 DLL 继续运行，因此部署必须等待退出并核对哈希。历史故障证据保留，不能当作本轮结果。

## 附录：主菜单自行退出的未决问题

核实时间：2026-11-15，Asia/Shanghai。用户确认没有手动关闭当时的游戏或“R5F Client”控制台。会话 `963d60ac-c770-4a76-8d68-1cdc4f357588` 在 15:51:54.971 进入 SDK 资源卸载；这只能证明进入了卸载路径，不能证明用户主动退出或正常退出。随后资源重新关联出现 `relink-stuck`，它发生在卸载开始后，尚不能认定是触发退出的原因。

该会话 script.log 在 15:51:51.126 和 15:51:54.045 两次记录打开 `menu_ConfirmExitToDesktopDialog`，中间曾关闭该对话框。bridge_trace.log 在 15:51:59.074 记录 `ExitProcess(code=0) signon=0 caller_va=0x1412663C5`。因此已观察到退出确认菜单路径和退出函数调用，不能把它直接描述为已捕获的异常崩溃；退出码 0 也不能证明用户主动操作或程序稳定。

退出菜单由什么输入或代码触发尚未确定，现有记录不含对应输入事件或完整调用栈。应记录为“非用户手动关闭，经退出菜单路径调用 ExitProcess(0)，触发原因未明”，不得声称已修复。之后另一次会话完成高亮截图验证，与该次退出应分别记录；画面验证通过不能替代稳定性结论。

## 附录：退出入口对照与右键采样

核实时间：2026-11-15，Asia/Shanghai。本轮诊断 DLL SHA-256 为 `D333276061EF95D53DC92B6B2412728DF9373EF9D4BF529CEBF388635A8BA4E1`，客户端 PID 为 22664，日志会话为 `fe9e8f6a-c758-4714-be3d-4e88616eca43`。这与前述高亮截图构建不同；本轮增加输入和视角诊断，未修改遮挡着色器。

本轮通过 `coding-guidelines` 的最小修改流程增加诊断，没有禁止退出、替换退出码或吞掉 `quit`。源码中实际入口为：

1. `ui/menu_main.nut` 的 `OnMainMenu_NavigateBack` 调用 `OpenConfirmExitToDesktopDialog`。
2. `ui/menu_system.nut` 的退出按钮也可调用同一函数。
3. `ui/dialogs/confirm_exit_to_desktop.nut` 的确认回调执行 `ClientCommand("quit")`。

受控实测：

| 时间 | 操作与观察 |
| --- | --- |
| 16:17:56–16:19:12 | 主菜单约 76 秒未发送测试按键，未观察到自行打开退出框 |
| 16:18:32 | UI 实测 Escape=false、手柄 B=false、控制器模式=false |
| 16:19:12 | Insert 打开中文训练菜单 |
| 16:19:20 | 菜单打开时发送 Escape；窗口日志为 `modal=1`，未触发退出菜单 |
| 16:19:30 | Insert 关闭中文菜单 |
| 16:19:39 | 受控发送 Escape，先记录键盘事件，再记录 `source=MainMenu MENU_NAVIGATE_BACK` 与退出框打开 |
| 16:19:53 | 受控发送 Escape 取消退出框，客户端继续运行；未点击确认退出 |

因此，本轮确认了 Escape 到主菜单返回回调的调用链，并未复现“中文菜单打开时 Escape 穿透”。这些受控输入不能证明历史退出也是同一输入源。历史时间窗口内的桌面自动化操作提取结果为空，只能说明现有检索未找到对应调用，不能排除其他输入源。历史触发原因仍未确定，尚无可证实的根因修复。

诊断文件：

- `native-diagnostics\r5flowstate_exit_trace.py`：安装或 `--restore` 恢复三处 UI 来源日志；原脚本、原始及修改后 SHA-256 在 `artifacts\r5flowstate-aim\exit-trace\manifest.json` 与同目录备份中。恢复前核对当前文件哈希，防止覆盖其他修改。重启客户端后脚本变更生效。
- `local_training_input_trace 1`：离线模式下记录 Escape、Enter、Insert、F4、右键及窗口关闭相关消息，含焦点与原生菜单状态。此诊断不记录普通文字输入。
- `local_aim_trace 1`：每秒五次记录右键状态、选中实体、骨骼、像素误差、写入计数与实际引擎视角。
- `native-diagnostics\r5flowstate_trace_report.py`：汇总当前日志到 `artifacts\r5flowstate-aim\exit-trace\runtime-verification.json`，保留原始事件并分段统计按住右键期间的误差变化。
- 构建日志为 `artifacts\r5flowstate-aim\build-aim-input-trace.log`；构建退出码为 0，CTest 再次 2/2 通过。

本轮把玩家放到 `(-6220,-1150,-80)`，视角 `(0,180,0)`，实际客户端位置、眼睛朝向与相机位置均另行读取确认。目标假人头部骨骼 12 位于中心圆内，右键松开时视角 `(0,180,0)` 保持不变、写入计数为 0；误差随假人待机动画约在 75–82 像素变化。必须记录实际按下、角度变化、误差收敛和松开后的停止，才可将本轮右键跟随验收标记为通过。尚未观察到人工持续按下时不能用历史写入次数代替。

诊断结束后用 `local_training_input_trace 0;local_aim_trace 0` 关闭持续日志。角色位置调整仅发生于独立本机训练服务端。
