# Windows 11 build 26200 开源方案审计

核实日期：2026-11-14，Asia/Shanghai。目标系统的用户态 `RtlGetVersion` 实测为 Windows 10.0 build 26200；目标程序是固定哈希的 Apex 客户端。本文只记录公开资料审计和本机已有实测，未加载候选内核驱动，也未修改签名校验、Secure Boot、HVCI 或保护回调。

## 审计标准

候选必须同时满足以下条件，才进入本机正常加载测试：

1. 明确写出支持 Windows 11 build 26200，或有与该 build 完全对应的源码/发布证据。
2. 驱动文件通过本机正常内核签名策略，不依赖测试签名、DSE bypass、漏洞驱动或关闭安全功能。
3. 目标能力与当前任务匹配：标准用户态程序能够获得必要的进程访问或通过公开、受支持的驱动接口完成只读诊断。
4. 候选的版本、提交号、二进制身份和清理方式可以固定记录。

“README 写着 Windows 11”或“仓库里有驱动源码”都不算满足前置条件。仓库的用户态代码、内核驱动代码、发布二进制和实际加载结果必须分开记录。

## 候选矩阵

| 候选 | 固定 HEAD | build 26200 证据 | 正常签名证据 | 结论 | 本轮动作 |
| --- | --- | --- | --- | --- | --- |
| [KernelResearchKit](https://github.com/wesmar/KernelResearchKit) | `bf587821909664a67d05565be588c208b84147c0` | README 明确写 Windows 11 25H2 build 26200 | 其流程依赖 DSE bypass、启动阶段部署、漏洞驱动；README 还描述 HVCI/Defender 处理 | 拒绝：违反用户约束 | 未下载、未编译、未执行 |
| [KVC](https://github.com/wesmar/kvc) | `7ef485f13e26fb0adabc04fd98628f1aa66c8d22` | release 页面声明 Windows 11 25H2 `26200–27999` | 方案包含 DSE bypass、SeCiCallbacks/g_CiOptions 操作、HVCI/保护机制处理 | 拒绝：属于绕过系统保护 | 未下载、未编译、未执行 |
| [Valthrun kernel driver](https://github.com/Valthrun/valthrun-driver-kernel) | `bc8e18a27b15fbd55c4b37ebeb8252c60cec934f` | 已合并的 PR 只针对 `26200.7462` 内核偏移；主 README 没有完整 26200 支持矩阵 | README 只说明自行构建 `.sys`，没有本机正常签名发布证据 | 不满足签名和当前补丁级别前提 | 未下载、未编译、未执行 |
| [ReadWriteDriver](https://github.com/ryan-weil/ReadWriteDriver) | `5276f48555ca2580e47a243ab624bcf335a4d8da` | 公开资料提到 Apex/EAC，但硬编码内核版本为 Windows 11 `22000.376` | 未发现当前 build 26200 的正常签名证据 | 版本不匹配 | 未下载、未编译、未执行 |
| [Blackbone](https://github.com/DarthTon/Blackbone) | `5ede6ce50cd8ad34178bfa6cae05768ff6b3859b` | 用户态库没有当前内核 build 适配声明；驱动源码显式分支只到 19041 | 本机现成驱动 `/kp` 校验失败，证书过期且根不受信任 | 已实测拒绝 | 用户态完整库已编译并测试；驱动未加载 |
| [System Informer KSystemInformer](https://github.com/winsiderss/systeminformer) | 公开主线 | 支持范围随内核数据变化；其 issue 也会标注新 build 的 reduced driver functionality | 驱动只允许验证过的调用方和签名状态 | 适合权限诊断，不是任意注入后端 | 未作为 Apex 注入器执行 |
| [Windows ObCallback sample](https://github.com/microsoft/Windows-driver-samples/tree/main/general/obcallback) | 公开主线 | 与特定 build 无关的机制示例 | 示例用于受控测试，不是目标机生产驱动 | 适合构造本地对照实验 | 未安装或加载 |

## 关键公开证据

KernelResearchKit 的 README 明确写出 build 26200，但同时描述 BootBypass、DSE bypass、漏洞驱动和 HVCI/Defender 处理；这不符合本次“正常签名、不得修改系统保护”的条件。[README](https://github.com/wesmar/KernelResearchKit/blob/bf587821909664a67d05565be588c208b84147c0/README.md)

KVC 的 release 页面明确给出 26200–27999 的支持范围，但项目定位是 DSE bypass、未签名驱动加载和保护进程操作；它不是正常签名的只读进程诊断方案。[release 说明](https://github.com/wesmar/kvc/releases)

Valthrun 的主 README 只要求 VS2022、WDK 和 Cargo Make，自行构建 `valthrun-driver.sys`；没有提供与本机正常内核签名策略相匹配的发布驱动身份。[主仓库](https://github.com/Valthrun/valthrun-driver-kernel)；针对 26200.7462 的 PR 只证明某个补丁级别的偏移修正讨论，不证明当前 26200.9974 或签名加载。[PR #7](https://github.com/Valthrun/valthrun-driver-kernel/pull/7)

ReadWriteDriver 的公开资料把函数地址固定在 Windows 11 10.0.22000.376，因此不能直接套到当前 build 26200。[项目说明](https://github.com/ryan-weil/ReadWriteDriver)

Blackbone 的用户态和驱动能力确实包含句柄提升、进程内存和 DLL 注入，但这只是功能范围；本机完整用户态测试已经证实目标句柄只得到 `0x1001`，`ProcessModules::Inject` 返回 `0xC0000022 / Win32 5`。[本机完整库报告](E:/Users/MYX/Desktop/apex/artifacts/blackbone-full/verification-2026-11-14.json)

微软文档说明对象回调可以修改 `DesiredAccess`，这与本机“OpenProcess 成功但访问位被削减”的现象相符；它不提供绕过方法。[OB_PRE_CREATE_HANDLE_INFORMATION](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ns-wdm-_ob_pre_create_handle_information)

## 本机停止条件

本轮没有候选同时满足“当前 build 26200、正常可信签名、无系统保护绕过”。因此没有执行候选驱动安装或 DLL 加载测试，也没有编造权限变化、StartService 错误或 DriverEntry 结果。

已有实际结果保持独立：

- Blackbone 用户态完整库：自进程注入成功，Apex 目标注入被拒绝。
- Blackbone 驱动：本机 `/kp` 签名验证失败，证书到期，源码显式兼容分支不含 build 26200。
- KernelResearchKit/KVC：只完成公开资料审计，未在本机执行，因为要求 DSE/HVCI/保护机制绕过。
- Valthrun/ReadWriteDriver：没有当前 build 和正常签名的足够证据，未执行。

如果目标是继续学习 DLL，下一步应使用自有宿主或独立训练程序测试进程内绘制；如果目标是继续验证 Apex 内存，现有已成功的软件采集路径仍是符合当前约束的方案。

机器可读摘要见 [开源方案审计结果](E:/Users/MYX/Desktop/apex/artifacts/open-source-26200-audit-2026-11-14.json)。
