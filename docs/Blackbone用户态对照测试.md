# Blackbone 用户态组件对照测试（历史阶段）

核实日期：2026-11-14。目标 PID 为 2324，磁盘映像 SHA-256 为 `0c1d47575261c139a8730ce67b795d24ab2238ffc426a41a1b9dacda823906f9`。

本文保存依赖补齐前的组件测试事实，不作为当前构建状态。完整库的依赖安装、typename 修正及实际 Inject 结果见 [完整编译与加载测试](E:/Users/MYX/Desktop/apex/docs/Blackbone完整编译与加载测试.md)。

## 结论与测试范围

没有加载诊断 DLL。实际调用原版 Blackbone 的 `Native::VirtualAllocExT` 时，目标进程的全部测试句柄均返回 `STATUS_ACCESS_DENIED (0xC0000022)` 和 Win32 错误 5。管理员进程成功启用 SeDebugPrivilege 后，授予权限和分配结果都未改变。

本次不是完整 Blackbone 注入成功或失败的端到端测试：原版项目构建未通过；后续程序链接了原版 NativeSubsystem 与 Utils 对象文件，以及自写的最小初始化适配层，直接验证注入前置条件。完整 `ProcessModules::Inject` 没有执行。

## 源码身份与构建结果

源码来自 [DarthTon/Blackbone](https://github.com/DarthTon/Blackbone)，本地目录为 `sources\blackbone-reference`，固定提交为 `5ede6ce50cd8ad34178bfa6cae05768ff6b3859b`。保留上游 MIT 许可证；没有修改上游源文件。

核对的实现：

| 文件 | 与本次问题相关的行为 |
| --- | --- |
| [ProcessCore.cpp](https://github.com/DarthTon/Blackbone/blob/5ede6ce50cd8ad34178bfa6cae05768ff6b3859b/src/BlackBone/Process/ProcessCore.cpp) | 标准用户态 Open 仍调用 OpenProcess；Init 处理体系结构及 DEP，不验证全部实际句柄权限 |
| [Process.h](https://github.com/DarthTon/Blackbone/blob/5ede6ce50cd8ad34178bfa6cae05768ff6b3859b/src/BlackBone/Process/Process.h) | DEFAULT_ACCESS_P 展开为 0xD7B |
| [NativeSubsystem.cpp](https://github.com/DarthTon/Blackbone/blob/5ede6ce50cd8ad34178bfa6cae05768ff6b3859b/src/BlackBone/Subsystem/NativeSubsystem.cpp) | VirtualAllocExT 调用 VirtualAllocEx，失败返回 LastNtStatus；读写也依赖相应进程接口 |
| [ProcessModules.cpp](https://github.com/DarthTon/Blackbone/blob/5ede6ce50cd8ad34178bfa6cae05768ff6b3859b/src/BlackBone/Process/ProcessModules.cpp) | 标准 Inject 需要目标内存分配及 DLL 路径写入，之后才进入加载调用 |
| [InitOnce.cpp](https://github.com/DarthTon/Blackbone/blob/5ede6ce50cd8ad34178bfa6cae05768ff6b3859b/src/BlackBone/Misc/InitOnce.cpp) | 完整初始化会尝试启用调试、驱动加载权限并加载符号；本次最小适配不运行该完整初始化 |

使用现有 VS 2022 Build Tools/v143 尝试构建原版 BlackBone.vcxproj 的 Release/x64 静态库，禁用构建后复制。编译失败于缺少 `cor.h` 和 `atlcomcli.h`，日志保留在 [upstream-build.log](E:/Users/MYX/Desktop/apex/artifacts/blackbone-user-mode/upstream-build.log)。没有生成可用的完整 Blackbone 库。

该构建过程中，未修改的 `NativeSubsystem.cpp` 和 `Utils.cpp` 已成功生成对象文件。使用 [blackbone_native_probe.cpp](E:/Users/MYX/Desktop/apex/native-diagnostics/blackbone_native_probe.cpp) 链接它们，最小适配只注册所需原生导出，不加载符号、驱动或回调规避代码。

构建入口为 [build-blackbone-probe.cmd](E:/Users/MYX/Desktop/apex/native-diagnostics/build-blackbone-probe.cmd)，依赖上述两个已编译对象；它不是从零构建完整上游项目的脚本。生成程序为 [blackbone_native_probe.exe](E:/Users/MYX/Desktop/apex/artifacts/blackbone-user-mode/blackbone_native_probe.exe)。

## 对照方法

1. 核对目标文件哈希、当前唯一游戏 PID 及诊断 DLL 哈希。
2. 在测试程序自己的进程上验证 4 种权限请求。除有限查询句柄外，其他配置均能通过原版 Native 组件分配并释放 4096 字节；未写入任何载荷。
3. 经标准 UAC 启动管理员测试进程。先禁用本测试进程的 SeDebugPrivilege，再运行同一权限矩阵。
4. 启用本测试进程的 SeDebugPrivilege，再次运行权限矩阵。仅测试一次分配并立即释放任何成功分配，不写 DLL 路径，不创建线程。
5. 分别捕获 OpenProcess 结果、NtQueryObject 授予掩码、体系结构查询、进程基本信息和实际分配错误。
6. 最后禁用该测试进程的 SeDebugPrivilege 并退出；不修改系统持久配置，不启用驱动加载权限。
7. 对相同诊断 DLL 再做标准加载器检查，另存直接加载尝试报告。

每次失败码在对应 API 返回后保存。原版 `VirtualAllocExT` 返回 NTSTATUS；其失败时的 Win32 LastError 另外记录。不能将库返回的 NTSTATUS、操作系统 LastError 与加载器自身校验产生的错误混为一类。

## 权限与错误实测

管理员组件测试宿主 PID 为 12048，SeDebugPrivilege 的禁用、启用和最终禁用均成功，错误码为 0。每种配置在启用前后各执行一次：

| 请求配置 | 请求掩码 | 启用前实际授予 | 启用后实际授予 | 分配结果 |
| --- | --- | --- | --- | --- |
| 有限查询 | 0x1000 | 0x1000 | 0x1000 | 两次均拒绝 |
| 最小诊断加载权限 | 0x43A | 0x1000 | 0x1000 | 两次均拒绝 |
| Blackbone 默认访问 | 0xD7B | 0x1001 | 0x1001 | 两次均拒绝 |
| 全访问请求 | 0x1FFFFF | 0x1FF001 | 0x1FF001 | 两次均拒绝 |

所有 8 次 OpenProcess 均取得句柄，NtQueryObject 状态均为 0，体系结构查询均成功，目标为 x64；但缺少远程内存操作、写入和创建线程所需的访问位。原版 `Native::VirtualAllocExT` 的实际分配全部失败，NTSTATUS 为 `0xC0000022`、Win32 错误为 5，没有产生目标内存分配。

扩展基本信息中的 protected-process 位为 false，只能说明该查询位的结果，不能据此认定没有反作弊保护，也不能反向证明某个具体驱动回调就是权限削减者。

自进程控制组宿主 PID 为 16112。有限查询句柄的 2 次分配按预期失败，另外 6 次成功并全部释放。该非提升控制进程未获授 SeDebugPrivilege，调整返回 1300；不影响其自身 DACL 允许的分配对照。不能将它的权限状态与管理员游戏测试进程混用。

## 对同一诊断 DLL 的直接加载检查

再次启动标准加载器，宿主 PID 为 16456。诊断 DLL SHA-256 仍为 `A3214EF0AB363DE1745EED748B2E66074EE93212340B90AD747EDFC54DE694D9`。

加载器请求 `0x43A`，实际授予 `0x1000`，在 `required_access_not_granted` 阶段停止。其报告中的错误 5 由自身权限校验产生；独立组件测试则已实际调用分配接口并得到操作系统拒绝。

直接加载检查没有申请目标内存、写 DLL 路径或创建线程。没有本轮游戏内 DLL 快照，也未测试其游戏内自动卸载。不能将这些结果描述为完整 Blackbone 驱动分支或手动映射已测试失败。

## DLL 加载与显示人物位置

成功加载 DLL 只是使代码进入进程。当前诊断 DLL 只生成玩家、弹药和队伍快照，不包含人物位置字段解析、世界坐标到屏幕坐标的投影或画面绘制。加载成功也不会自动在训练场显示人物位置。

此前队伍表计数合计为 18，但客户端实体视图只检出一个本地玩家和六个假人。不能据队伍计数推导出 18 个可用的位置，更不能把未取得的远端实体坐标补造成可绘制数据。

## 原始证据

结构化汇总及源码、二进制身份见 [Blackbone 用户态测试报告](E:/Users/MYX/Desktop/apex/artifacts/blackbone-user-mode/verification-2026-11-14.json)。该报告关联自进程控制矩阵、管理员目标矩阵和重新执行的标准加载器报告。
