# Blackbone 完整用户态编译与 DLL 加载测试

核实日期：2026-11-14，Asia/Shanghai。目标文件为 `D:\Steam\steamapps\common\Apex Legends\r5apex_dx12.exe`，SHA-256 为 `0c1d47575261c139a8730ce67b795d24ab2238ffc426a41a1b9dacda823906f9`。

## 结论

缺失依赖已通过微软官方 Visual Studio Installer 安装，`typename` 兼容性错误已修正。完整 Blackbone 用户态 Release/x64 静态库编译成功，并链接到测试宿主；不是此前只链接 NativeSubsystem 与 Utils 的组件程序。

完整库在自进程对照中成功通过原版 `ProcessModules::Inject` 加载诊断 DLL，导出自检通过，并完成卸载。对当前 Apex 进程，启用 SeDebugPrivilege 前后的实际权限相同，两次原版 Inject 均返回 `0xC0000022`，没有返回模块或产生游戏内诊断快照。

编译成功与目标允许注入是独立条件。本轮没有调用 Blackbone 内核接口、修改句柄权限、修改对象回调或更改目标进程防护。

## 依赖安装与身份

| 项目 | 本机结果 |
| --- | --- |
| VS 安装实例 | Build Tools 2022，17.14.37906.3，显示版本 17.14.45 |
| ATL 组件 | `Microsoft.VisualStudio.Component.VC.ATL` |
| .NET Framework SDK 组件 | `Microsoft.Net.Component.4.8.SDK` |
| cor.h | `C:\Program Files (x86)\Windows Kits\NETFXSDK\4.8\Include\um\cor.h` |
| ATL 头文件 | `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\atlmfc\include\atlcomcli.h` |
| 编译工具链 | MSVC 14.44.35207，v143，x64 |
| 安装后实例状态 | isComplete=true，isRebootRequired=false |

使用现有官方 Installer 的 modify 操作，仅添加这两个组件，参数包括 `--quiet --norestart --noUpdateInstaller`。没有移除原有组件或自动重启。安装器启动进程可能早于后台安装服务结束，因此没有仅凭启动器退出码判定安装成功，而是核对组件注册、实际头文件及后续编译。

组件与命令参数来源：[微软 Build Tools 组件目录](https://learn.microsoft.com/en-us/visualstudio/install/workload-component-id-vs-build-tools?view=vs-2022)、[安装器命令行文档](https://learn.microsoft.com/en-us/visualstudio/install/use-command-line-parameters-to-install-visual-studio?view=visualstudio)。

## 源码及兼容性补丁

上游仓库为 [DarthTon/Blackbone](https://github.com/DarthTon/Blackbone)，提交 `5ede6ce50cd8ad34178bfa6cae05768ff6b3859b`，本地目录 `sources\blackbone-reference`，MIT 许可证保留。

`ProcessModules.cpp` 的泛型 lambda 使用依赖类型作为模板参数，本机编译器报 C2275/C2672。只增加 `typename`：

```cpp
ustr.Buffer = modName->ptr<typename std::decay_t<decltype(ustr)>::type>() + sizeof( ustr );
```

没有改变该表达式的数据类型、地址计算或注入流程。原文件保留为 [ProcessModules.upstream.cpp](E:/Users/MYX/Desktop/apex/artifacts/blackbone-full/ProcessModules.upstream.cpp)，差异见 [typename-compat.patch](E:/Users/MYX/Desktop/apex/artifacts/blackbone-full/typename-compat.patch)。检查确认文本差异仅为这一处类型消歧。

依赖补齐后的首次编译错误见 [full-build.log](E:/Users/MYX/Desktop/apex/artifacts/blackbone-full/full-build.log)，补丁后的成功构建见 [full-build-compat.log](E:/Users/MYX/Desktop/apex/artifacts/blackbone-full/full-build-compat.log)。这是“固定上游提交加一处编译兼容补丁”，不能标为完全未修改的上游源码。

## 完整用户态库与宿主编译

在工作区根目录构建 BlackBone.vcxproj，而非驱动或整个解决方案：

```powershell
Set-Location -LiteralPath 'E:\Users\MYX\Desktop\apex'
& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe' `
    'sources\blackbone-reference\src\BlackBone\BlackBone.vcxproj' `
    /t:Build /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 `
    '/p:OutDir=E:\Users\MYX\Desktop\apex\artifacts\blackbone-full\' `
    '/p:IntDir=E:\Users\MYX\Desktop\apex\artifacts\blackbone-full\obj\' `
    /p:PostBuildEventUseInBuild=false /m:1 /nologo /v:minimal
if ($LASTEXITCODE -ne 0) { throw '完整用户态库编译失败。' }
cmd.exe /c native-diagnostics\build-blackbone-full-probe.cmd
if ($LASTEXITCODE -ne 0) { throw '完整库宿主链接失败。' }
```

产物为 [BlackBone.lib](E:/Users/MYX/Desktop/apex/artifacts/blackbone-full/BlackBone.lib) 和 [blackbone_full_probe.exe](E:/Users/MYX/Desktop/apex/artifacts/blackbone-full/blackbone_full_probe.exe)。没有构建内核驱动或 GUI 示例。

测试源为 [blackbone_full_probe.cpp](E:/Users/MYX/Desktop/apex/native-diagnostics/blackbone_full_probe.cpp)，链接脚本为 [build-blackbone-full-probe.cmd](E:/Users/MYX/Desktop/apex/native-diagnostics/build-blackbone-full-probe.cmd)。宿主使用完整库的原版初始化、Process、Attach、模块枚举和 Inject，没有此前的最小初始化替代实现。

符号搜索环境仅在测试宿主进程内指向其 `symbols` 本地目录，避免无界网络下载；原版 InitOnce 和其模式扫描回退仍执行。这不证明所有 PDB 功能在本机可用。

## 自进程加载与卸载对照

```powershell
& .\artifacts\blackbone-full\blackbone_full_probe.exe --self-test
if ($LASTEXITCODE -ne 0) { throw '完整库对照失败，请检查 JSONL 日志。' }
```

该测试不以游戏为目标，验证真实库和加载链路能够运行：

1. 构造完整 `blackbone::Process`，调用 `Attach` 附加自身。
2. 调用 `process.modules().Inject` 加载同一份诊断 DLL。
3. 确认返回模块，调用 DLL 的 `OffsetProbeSelfTest`，返回 0，4 个用例通过。
4. 调用库的 Unload，确认模块从自进程中消失。
5. 检查原版分配接口可以分配并释放 4096 字节，再销毁 Process 并保存结束记录。

对照结果：Inject 状态 0、模块返回 true、自检错误 0、模块卸载后不存在、分配及释放均成功。上游 Unload 函数的返回值为 1，源码在该处返回 true；没有将其解释为 Win32 错误 1。宿主退出码为 0。

对照宿主不叫 `r5apex_dx12.exe`，DLL 不会在其内自动执行游戏读取线程。人工数据自检不能代替游戏偏移验证。

## 当前游戏的完整 Inject 测试

确认目标文件和诊断 DLL 哈希后，经标准 UAC 启动完整库宿主，目标 PID 为 2324。每轮阶段使用 Blackbone 的默认访问掩码 `0xD7B`。

```powershell
$games = @(Get-Process -Name r5apex_dx12 -ErrorAction Stop)
if ($games.Count -ne 1) { throw '要求恰好一个游戏进程。' }
$hostProcess = Start-Process `
    -FilePath 'E:\Users\MYX\Desktop\apex\artifacts\blackbone-full\blackbone_full_probe.exe' `
    -ArgumentList @($games[0].Id.ToString()) -Verb RunAs -WindowStyle Hidden -PassThru
$finished = $hostProcess.WaitForExit(30000)
$hostProcess.Id
$finished
```

没有将等待超时自动解释为失败阶段或终止目标游戏；若超时，应先查看该宿主的最后一个已保存事件。本轮完整库宿主正常结束。

| 检查 | SeDebugPrivilege 禁用阶段 | SeDebugPrivilege 启用后 |
| --- | --- | --- |
| 调整调试权限 | 成功，错误 0 | 成功，错误 0 |
| Attach | NTSTATUS 0 | NTSTATUS 0 |
| 请求访问掩码 | 0xD7B | 0xD7B |
| 实际授予掩码 | 0x1001 | 0x1001 |
| NtQueryObject | NTSTATUS 0 | NTSTATUS 0 |
| ProcessModules::Inject | 0xC0000022 | 0xC0000022 |
| NTSTATUS 映射的 Win32 错误 | 5 | 5 |
| 返回后观察到的 LastError | 5 | 5 |
| 返回模块 | false | false |
| 独立原版分配检查 | 0xC0000022，Win32 5 | 0xC0000022，Win32 5 |

`0x1001` 仅含有限信息查询和终止访问位，不含远程内存操作、读取、写入或创建线程所需权限。Attach 成功不能代替后续 API 成功，启用 SeDebugPrivilege 也没有改变这组实际授予权限。

**直接观察到的失败入口是完整 `ProcessModules::Inject` 的返回值。**没有为库内部每个分支插桩。结合句柄缺少远程内存操作权限、独立分配检查失败，以及源码在路径写入前调用 Allocate 的顺序，结果与早期分配被拒绝的路径一致；不把这一推断写成已取得内部调用栈。

没有诊断模块返回，也没有生成目标 PID 的 DLL 快照文件。宿主退出码 0 仅表示测试与日志正常完成，不能解释为 DLL 加载成功。

原版初始化会尝试启用进程令牌特权；宿主在构造完成后显式禁用 SeLoadDriverPrivilege，并在测试后禁用 SeDebugPrivilege。记录这些过程内令牌操作，不将其描述为完全没有权限状态变化；未修改系统持久配置、调用驱动加载接口或变更目标防护。

## 日志、身份与限制

日志为 `artifacts\blackbone-full\full-<宿主PID>-<TickCount>.jsonl`，逐事件写入并 FlushFileBuffers。包含构造、令牌操作、Attach、实际权限、Inject 入口和返回、独立分配、Process 析构及结束事件。

`ntstatus` 是库的直接返回；`mapped_win32_error` 由 RtlNtStatusToDosError 映射；`last_error_observed` 是函数返回后观察到的线程 LastError，未把它当作库内部每条 API 的独立错误码。

结构化结果、最终宿主 PID、原始日志路径与文件哈希见 [完整库验证报告](E:/Users/MYX/Desktop/apex/artifacts/blackbone-full/verification-2026-11-14.json)。

本轮证明完整用户态库在对照进程中可加载、在当前游戏中被拒绝；不证明 Blackbone 的其他模式或未来游戏版本会得到相同结果，也没有测试驱动分支。当前诊断 DLL仍只输出快照，没有人物坐标投影或画面绘制功能。
