# DLL 偏移诊断记录

核实日期：2026-11-14。目标为本机固定版本 `r5apex_dx12.exe`，SHA-256 为 `0c1d47575261c139a8730ce67b795d24ab2238ffc426a41a1b9dacda823906f9`。

## 结论

诊断 DLL 和标准加载器已编译，本地宿主的 4 项自检通过。针对当前游戏进程的标准加载在权限检查阶段停止，未实际注入。因此没有 DLL 在游戏内取得的快照，不能把本地自检当作游戏偏移的运行时证明。

原有软件采集路径的玩家、弹药及队伍证据仍是独立实验结果，不能转称为 DLL 注入验证成功。

## 文件与行为

| 文件 | 作用 |
| --- | --- |
| [offset_probe.cpp](E:/Users/MYX/Desktop/apex/native-diagnostics/offset_probe.cpp) | DLL 源码：完整字节读取、句柄序列号校验、武器拥有者与弹药、队伍计数、自检导出 |
| [probe_host.cpp](E:/Users/MYX/Desktop/apex/native-diagnostics/probe_host.cpp) | 本地自检宿主与限定目标的标准 Windows 加载器 |
| [build.cmd](E:/Users/MYX/Desktop/apex/native-diagnostics/build.cmd) | VS 2022 x64 编译入口 |
| [offset_probe.dll](E:/Users/MYX/Desktop/apex/artifacts/native-offset-probe/offset_probe.dll) | 生成的 x64 诊断 DLL |
| [offset_probe_host.exe](E:/Users/MYX/Desktop/apex/artifacts/native-offset-probe/offset_probe_host.exe) | 生成的 x64 宿主 |

DLL 在游戏内加载时设计为读取两次快照，间隔 0.4 秒，写入与 DLL 同目录的新 JSON，再调用 `FreeLibraryAndExitThread` 卸载。它不 Hook、不隐藏、不修改弹药或其他游戏字段。这个游戏内路径尚未在本次实验执行成功，自动卸载也不能描述为已在游戏内验证。

读取内容为本地玩家句柄、实例、队伍编号、三个武器槽、武器拥有者和 `+0x1600` 弹药字段，以及队伍对象的编号和 `+0x968` 计数。DLL 不执行全实体扫描，不承担真人、NPC 和观战者的完整分类。

加载器只允许标准 Windows 加载路径，先查询实际授予权限，再验证目标映像路径，解析目标进程内的 LoadLibraryW 地址。后续设计步骤是申请 DLL 路径缓冲区、写入路径、创建加载线程及释放该缓冲区；这些后续步骤在本次游戏尝试中均未发生。

若加载线程超时，加载器不会释放可能仍被线程读取的路径缓冲区，并明确记录该分支。该超时分支未经过实际游戏验证，不能保证任何标准加载尝试没有目标进程副作用。

## 编译与本地自检

在工作区根目录执行：

```powershell
Set-Location -LiteralPath 'E:\Users\MYX\Desktop\apex'
cmd.exe /c native-diagnostics\build.cmd
if ($LASTEXITCODE -ne 0) { throw '编译失败。' }
& .\artifacts\native-offset-probe\offset_probe_host.exe --self-test
if ($LASTEXITCODE -ne 0) { throw 'DLL 本地自检失败。' }
```

使用 VS 2022 Build Tools 的 `vcvars64.bat`、x64 C++17、`/W4 /WX /EHsc /MT`。静态链接 C/C++ 运行库，不要求放入 Python、MemProcFS 或 LeechCore DLL。保留现有系统 Windows API 依赖。

自检宿主在自己的进程内正常加载 DLL，仅调用 `OffsetProbeSelfTest`，使用稀疏人工数据验证：

1. 玩家、武器、拥有者、弹药与队伍字段的完整解析。
2. 无效句柄返回空且不解引用实体 0。
3. 实体序列号不匹配时拒绝。
4. 队伍条目数超过 256 时拒绝。

人工数据中的 31 只是测试输入，不是当前游戏弹药。自检通过证明 DLL 导出及上述逻辑可运行，不独立证明游戏布局正确。

## 标准加载实际结果

本次经标准 UAC 启动加载器，目标 PID 为 2324，加载器 PID 为 27528。先核对了目标文件和已编译 DLL 的 SHA-256。

| 项目 | 结果 |
| --- | --- |
| OpenProcess 后句柄权限查询 | 成功，NT 查询状态 0 |
| 请求访问掩码 | `0x43A`：创建线程、查询信息、虚拟内存操作、读、写 |
| 实际授予 | `0x1000`，有限信息查询 |
| 停止阶段 | `required_access_not_granted` |
| 报告中的 windows_error | 5，由加载器权限校验返回，不是宣称 OpenProcess 本身返回错误 5 |
| 远程内存分配 | false |
| DLL 路径写入 | false |
| 远程线程创建 | false |
| 游戏内 DLL 加载 | 未执行 |

本次没有启用新的驱动、修改安全配置或尝试恢复被限制的权限。权限不足后没有替代注入尝试。具体根源不能仅凭授予掩码确定到某个反作弊回调。

加载器会在自身目录生成唯一的 `loader-<宿主PID>-<TickCount>.json`。若 DLL 将来在允许加载的目标中运行，会生成 `offset-probe-<目标PID>-<TickCount>.json`；两者不能混为同一种证据。

汇总与文件哈希见 [DLL 诊断验证报告](E:/Users/MYX/Desktop/apex/artifacts/native-offset-probe/verification-2026-11-14.json)。
