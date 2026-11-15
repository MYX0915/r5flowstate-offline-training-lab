# Blackbone 驱动模式前置检查

核实日期：2026-11-14，Asia/Shanghai。仓库提交为 `5ede6ce50cd8ad34178bfa6cae05768ff6b3859b`。

## 结论与执行范围

当前仓库版本没有满足本机驱动模式测试的前提：现成驱动未通过内核签名策略校验，源码的 Windows 10 构建分支也没有本机 build 26200 的支持。

本轮实际执行了操作系统版本查询、文件身份及签名检查、源码版本分支核对和服务状态查询。没有创建驱动服务、加载驱动、调用驱动注入函数或改变句柄权限，因此没有驱动模式下的 DLL 加载结果或加载后的权限数据。

这不是“驱动已加载但注入失败”，也不是“已验证驱动能够恢复被削减权限”。本次停止阶段为驱动兼容性与签名前置检查。

## 文件与环境

| 项目 | 核实值 |
| --- | --- |
| 驱动 | `sources\blackbone-reference\src\BlackBoneDrv\bin\x64\Win10Release\BlackBoneDrv10.sys` |
| 文件大小 | 64,936 字节 |
| SHA-256 | `5F369DC17AC752EC3C76980AB4CED66B95E82E70606A4DD56ABFE0B3E780FF59` |
| 用户态 RtlGetVersion | NTSTATUS 0，版本 10.0，build 26200 |
| 默认驱动服务 BlackBone | 查询返回 1060，不存在 |
| BlackBoneDrv 服务名 | 查询返回 1060，不存在 |

现成二进制与仓库源码分别记录身份；没有假定它们必定由同一次构建生成。用户态版本查询也没有伪称为已在内核执行 DriverEntry 得到的版本值。

## 签名实际结果

有效的最终命令为：

```powershell
& 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe' verify /kp /v `
    'E:\Users\MYX\Desktop\apex\sources\blackbone-reference\src\BlackBoneDrv\bin\x64\Win10Release\BlackBoneDrv10.sys'
```

| 检查 | 实际结果 |
| --- | --- |
| SignTool 内核策略校验 | 退出码 1，未验证成功 |
| SignTool 错误 | 证书链终止于不受信任的根证书 |
| Authenticode 状态 | UnknownError，同样报告不受信任根证书 |
| 签名者 | WDKTestCert Ton,131029624036001818 |
| 证书到期时间 | 2026-03-20 00:00:00 UTC，核实时已过期 |
| 文件时间戳 | 无 |
| 独立 Generic Authenticode WinVerifyTrust | 返回 HRESULT `0x800B0109`，CERT_E_UNTRUSTEDROOT |

Generic Authenticode 的 HRESULT 与 SignTool `/kp` 的内核策略结果分开记录，不能互相替代。SignTool `/kp` 用途见 [微软官方说明](https://learn.microsoft.com/en-us/windows/win32/seccrypto/signtool)。

最初附加的版本及目录查询选项在本机 SignTool 中产生参数兼容错误；这些退出码没有作为签名失败的依据。最终有效日志为 [kernel-signature-check-host.txt](E:/Users/MYX/Desktop/apex/artifacts/blackbone-driver-preflight/kernel-signature-check-host.txt)，对应 [signature-result-host.json](E:/Users/MYX/Desktop/apex/artifacts/blackbone-driver-preflight/signature-result-host.json)。

普通真实用户命令上下文和管理员上下文的 Authenticode 均报告不受信任根证书；有效的最终 SignTool 命令在真实用户命令上下文执行，不要求提升。检查脚本为 [check-blackbone-driver-signature.ps1](E:/Users/MYX/Desktop/apex/native-diagnostics/check-blackbone-driver-signature.ps1)。

PowerShell 5.1 会把本机工具的 stderr 包装为 NativeCommandError。脚本只在运行签名工具时临时允许收集 stderr，随后显式保存其退出码和错误日志，不将脚本正常写完报告视为签名通过。

## 源码兼容性判断

[BlackBoneDrv.c](E:/Users/MYX/Desktop/apex/sources/blackbone-reference/src/BlackBoneDrv/BlackBoneDrv.c) 的 `BBInitDynamicData` 先查询版本，再按 Windows 构建号选择与内核结构相关的数据。其显式 Windows 10 构建分支包含：

`10586、14393、15063、16299、17134、17763、18362、18363、19041`。

本机用户态 API 返回的 26200 不在这些分支中；对该输入，源码会走 `STATUS_NOT_SUPPORTED` 返回路径。该状态值为 `0xC00000BB`，映射为 Win32 50。

**这是源码推断，不是实际驱动加载返回。**DriverEntry 没有执行，StartService/NtLoadDriver 的错误码没有取得。DriverEntry 在版本初始化前还有其他初始化，不能保证实际强制加载一定只会安全地返回该状态。

不修改版本检查、硬填内核结构数据、关闭签名校验或导入测试根证书来使本次检查“通过”。重新编译相同的不兼容源码也不能自动获得当前系统支持。

## 权限与 DLL 加载状态

| 项目 | 本次记录 |
| --- | --- |
| 最近完整用户态参考 | 请求 0xD7B，实际授予 0x1001，SeDebugPrivilege 启用后不变 |
| 驱动加载后的权限 | 未测，驱动没有加载 |
| 驱动恢复句柄权限 | 未调用 |
| 驱动 DLL 注入 | 未调用 |
| 诊断 DLL 通过驱动加载 | 未发生 |
| 目标权限或系统安全设置更改 | 未执行 |

用户态参考来自 [完整库验证报告](E:/Users/MYX/Desktop/apex/artifacts/blackbone-full/verification-2026-11-14.json)，不是本轮驱动介入前后的差分。没有将“未测试”写成“权限恢复失败”或“权限保持不变”。

若继续驱动模式验证，需要一个明确支持当前系统构建并满足本机内核签名策略的驱动版本；本轮没有找到或验证这样的版本。当前资料不足以承诺某个驱动能够安全加载并恢复目标访问。

实际检查结果与预测值的区分见 [驱动前置检查报告](E:/Users/MYX/Desktop/apex/artifacts/blackbone-driver-preflight/verification-2026-11-14.json)。
