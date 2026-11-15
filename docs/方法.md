# 本机内存读取与验证操作手册

本文对应本工作区保留的修复后版本。武器采样按第 1—6 节顺序执行；界面对照和单次开火验证分别见第 7、8 节；玩家列表与队伍人数采用第 11 节的独立模式。字段、组件与已验证环境见附录。所有报告使用新的 RunId，历史数值不作为下次运行的预期输入。

## 1. 执行条件与数据路径

工作区为 `E:\Users\MYX\Desktop\apex`，目标文件为 `D:\Steam\steamapps\common\Apex Legends\r5apex_dx12.exe`。当前代码固定了目标文件哈希和对象布局；游戏更新后必须重新核对，不允许只修改哈希让旧偏移继续运行。

数据路径为：管理员父脚本管理官方签名 WinPmem 采集驱动 → 本地修改版 LeechCore → MemProcFS 原生 C API → Python 读取器 → 原始 JSON → 独立界面对照。

CE 和 Lua 桥接不参与这条成功路径；它也不修改 CE 原有游戏进程句柄的读取权限。直接运行保留的二进制不需要重新编译、不需要 DMA 硬件，也不需要重新安装 Python wheel 的标准包装层。

执行时要求：

- 使用 64 位 Windows PowerShell 5.1 和指定的 x64 Python。
- 有效的管理员令牌由标准 UAC 获得；系统授权窗口由用户确认。
- 第一层不要求游戏已启动；第二、三层要求恰好一个 `r5apex_dx12` 进程存在。
- 第三层应在训练场角色生成、武器已装备且界面稳定后执行。不要在采样中换枪、重生或换弹。
- 同一时间只执行一轮。临时服务名固定为 `ApexLabWinPmemProbe`。
- 保持现有路径。父脚本的参数拼接没有验证迁移到带空格工作区的情况。

## 2. 文件、身份与运行环境检查

以下 PowerShell 代码块在同一会话中按顺序执行。每次检查失败都先停止，保留错误，不自动替换组件或修改安全策略。

### 2.1 定义路径并确认依赖存在

```powershell
$ErrorActionPreference = 'Stop'
$workspace = 'E:\Users\MYX\Desktop\apex'
$ps64 = 'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe'
$python = 'E:\Python\Python313\python.exe'
$game = 'D:\Steam\steamapps\common\Apex Legends\r5apex_dx12.exe'
$probeScript = Join-Path $workspace 'tmp\Test-WinPmemReadOnly.ps1'
Set-Location -LiteralPath $workspace
if (-not [Environment]::Is64BitProcess) { throw '请使用 64 位 PowerShell。' }
$requiredFiles = @(
    $ps64, $python, $game, $probeScript,
    '.\static-analysis\live_weapon_verify.py', '.\static-analysis\native_vmm.py',
    '.\static-analysis\population_verify.py',
    '.\sources\leechcore-readonly\files\leechcore.dll',
    '.\downloads\forensics-reference\winpmem_x64.sys',
    '.\.tools\forensics-python\memprocfs\vmm.dll',
    '.\.tools\forensics-python\memprocfs\vcruntime140.dll',
    '.\.tools\forensics-python\memprocfs\info.db',
    '.\.tools\forensics-python\memprocfs\dbghelp.dll',
    '.\.tools\forensics-python\memprocfs\symsrv.dll',
    '.\.tools\forensics-python\memprocfs\vmmyara.dll',
    '.\tests\test_leechcore_read_contract.py',
    '.\tests\test_live_weapon_verify.py', '.\tests\test_result_persistence.py',
    '.\tests\test_population_verify.py'
)
foreach ($path in $requiredFiles) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "缺少文件：$path" }
}
foreach ($path in @('.\artifacts', '.\tmp')) {
    if (-not (Test-Path -LiteralPath $path -PathType Container)) { throw "缺少目录：$path" }
}
& $python -B -c "import sys, struct; print(sys.executable); print(sys.version); assert struct.calcsize('P') == 8"
if ($LASTEXITCODE -ne 0) { throw 'Python 位数检查失败。' }
```

保留整个 `.tools\forensics-python\memprocfs` 目录及相关许可证，不只复制 `vmm.dll`。部分文件用于按需功能，整目录保留不表示最终采样会调用每一个文件。

### 2.2 检查目标、驱动和保留文件身份

```powershell
$driver = Join-Path $workspace 'downloads\forensics-reference\winpmem_x64.sys'
$expectedGameHash = '0C1D47575261C139A8730CE67B795D24AB2238FFC426A41A1B9DACDA823906F9'
$expectedDriverHash = 'B1B866F7CF86BD2BBBD8BECA081E9DF836911D938FD129CD615CFCB04399212B'
if ((Get-FileHash -LiteralPath $game -Algorithm SHA256).Hash -ne $expectedGameHash) {
    throw '目标映像已变化，停止使用本版本布局。'
}
if ((Get-FileHash -LiteralPath $driver -Algorithm SHA256).Hash -ne $expectedDriverHash) {
    throw '驱动身份不匹配。'
}
$signature = Get-AuthenticodeSignature -LiteralPath $driver
if ($signature.Status.ToString() -ne 'Valid') { throw '驱动签名验证失败。' }
$manifestPath = Join-Path $workspace 'artifacts\method-file-manifest-2026-11-14.json'
$manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
foreach ($entry in $manifest.files) {
    if (-not (Test-Path -LiteralPath $entry.path -PathType Leaf)) { throw "清单文件缺失：$($entry.path)" }
    if ((Get-FileHash -LiteralPath $entry.path -Algorithm SHA256).Hash -ne $entry.sha256) {
        throw "清单文件已变化：$($entry.path)"
    }
}
```

清单包含工作区文档和实验记录，不是操作系统完整依赖锁。修改文档或清理已归档证据也会触发哈希不一致，应辨明差异；不能为了通过检查而自动重写清单。

### 2.3 只装载用户态 DLL，检查所用导出

```powershell
@'
import ctypes
import os
from pathlib import Path

root = Path(r'E:\Users\MYX\Desktop\apex')
library_dir = root / 'sources/leechcore-readonly/files'
with os.add_dll_directory(str(library_dir)):
    lc = ctypes.WinDLL(str(library_dir / 'leechcore.dll'))
    vmm = ctypes.WinDLL(str(root / '.tools/forensics-python/memprocfs/vmm.dll'))
    for name in ('LcCreate', 'LcRead', 'LcClose', 'LcGetOption'):
        getattr(lc, name)
    for name in ('VMMDLL_Initialize', 'VMMDLL_Close', 'VMMDLL_MemReadEx',
                 'VMMDLL_ProcessGetModuleBaseU', 'VMMDLL_PidGetFromName'):
        getattr(vmm, name)
print('DLL_LOAD_AND_EXPORTS_OK')
'@ | & $python -B -
if ($LASTEXITCODE -ne 0) { throw 'DLL 装载或导出检查失败。' }
```

该检查不调用 `LcCreate`、`VMMDLL_Initialize`，不加载内核驱动或读取游戏内存。成功只证明当前 DLL 的装载和所需接口存在。

### 2.4 运行无驱动回归

```powershell
foreach ($suite in @('test_leechcore_read_contract.py', 'test_live_weapon_verify.py', 'test_result_persistence.py', 'test_population_verify.py')) {
    & $python -B -m unittest discover -s tests -p $suite -v
    if ($LASTEXITCODE -ne 0) { throw "回归失败：$suite" }
}
```

总计 22 项：普通文件读取 6 项、模拟对象和页表解释 7 项、日志保存与关闭异常 4 项、人口分类及队伍边界 5 项。测试使用普通文件或模拟对象，不加载采集驱动。

## 3. 第一层：加载、设备读取与清理

先只读查询临时服务：

```powershell
& sc.exe query ApexLabWinPmemProbe
if ($LASTEXITCODE -ne 1060) { throw '实验服务可能已存在或查询异常，请先核对其所有者。' }
```

1060 表示服务不存在，是新运行的预期状态。Windows 自带的 `pmem` 服务不是本实验服务，不要停止、删除或替换它。

通过标准 UAC 启动第一层：

```powershell
$runId = [guid]::NewGuid()
$runId
$probeArgs = @('-NoProfile', '-File', $probeScript, '-RunId', $runId.ToString())
Start-Process -FilePath $ps64 -ArgumentList $probeArgs -Verb RunAs -WindowStyle Hidden -Wait
$probePath = Join-Path $workspace ('artifacts\winpmem-load-probe-' + $runId + '.json')
if (-not (Test-Path -LiteralPath $probePath)) { throw '本轮管理报告不存在，检查 UAC 或脚本启动错误。' }
$probe = Get-Content -LiteralPath $probePath -Raw -Encoding UTF8 | ConvertFrom-Json
$probe | Select-Object RunId, Elevated, DriverStarted, ReadAttempted, ReadBytes, ReadError,
    FailureStage, FailureCode, FailureMessage, DriverStopped, TemporaryServiceDeleted,
    StagedFileRemoved, CleanupErrors
if (-not $probe.Elevated -or -not $probe.DriverStarted -or -not $probe.ReadAttempted -or
    $probe.ReadError -ne 0 -or $probe.ReadBytes -ne 16 -or $probe.FailureStage -or
    -not $probe.DriverStopped -or -not $probe.TemporaryServiceDeleted -or
    -not $probe.StagedFileRemoved -or $probe.CleanupErrors.Count -ne 0) {
    throw '第一层未通过，不继续下一层。'
}
```

这一层会临时加载驱动并实际读取设备 16 字节，不只是签名检查。当前父脚本在设备短读时不一定设置 `FailureStage`，因此必须同时检查实际字节数。脚本整体退出码为 0 也不能替代报告字段。

工具允许执行命令不等于 Windows 令牌已提升。若 `Elevated=false` 或报 `AdministratorCheck`，说明实际子进程未提升；取消 UAC 或系统拒绝加载时停止并记录，不更改执行策略或安全配置。

## 4. 第二层：采集与页表解析

确认只有一个游戏进程，然后执行采集诊断：

```powershell
$games = @(Get-Process -Name r5apex_dx12 -ErrorAction Stop)
if ($games.Count -ne 1) { throw '要求恰好一个目标游戏进程。' }
$runId = [guid]::NewGuid()
$runId
$probeArgs = @('-NoProfile', '-File', $probeScript, '-RunId', $runId.ToString(), '-VerifyGame', '-AcquisitionOnly')
Start-Process -FilePath $ps64 -ArgumentList $probeArgs -Verb RunAs -WindowStyle Hidden -Wait
$probePath = Join-Path $workspace ('artifacts\winpmem-load-probe-' + $runId + '.json')
$probe = Get-Content -LiteralPath $probePath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($probe.FailureStage -or -not $probe.ConsumerReport) { throw '读取器未正常启动，请查看管理报告。' }
$capture = Get-Content -LiteralPath $probe.ConsumerReport -Raw -Encoding UTF8 | ConvertFrom-Json
$capture | ConvertTo-Json -Depth 8
if ($capture.status -ne 'acquisition_checked' -or -not $capture.acquisition.dtb_page_read -or
    -not $capture.acquisition.kernel_header_mz -or -not $capture.finished_at_utc -or
    -not $probe.DriverStopped -or -not $probe.TemporaryServiceDeleted -or
    -not $probe.StagedFileRemoved -or $probe.CleanupErrors.Count -ne 0 -or
    $probe.ConsumerTimedOut -or $probe.ReaderReapedAfterCompletedSnapshot -or $probe.ConsumerExitCode -ne 0) {
    throw '第二层未通过，不继续完整采样。'
}
```

`-AcquisitionOnly` 必须与 `-VerifyGame` 一起传给父脚本。该模式不读取武器字段，所以 `GameMemoryRead=false`、`ui_verified=false` 可以正常出现；成功判据是采集结果及清理结果。

LeechCore 的有效范围可能不包含物理地址 0，因此 `physical_zero_read=false` 不单独构成失败。父脚本直接设备读取与 LeechCore 按映射读取是不同检查。

## 5. 第三层：完整武器与弹药采样

角色和武器就绪后执行。不要复用第二层的 RunId：

```powershell
$runId = [guid]::NewGuid()
$runId
$probeArgs = @('-NoProfile', '-File', $probeScript, '-RunId', $runId.ToString(), '-VerifyGame')
Start-Process -FilePath $ps64 -ArgumentList $probeArgs -Verb RunAs -WindowStyle Hidden -Wait
$probePath = Join-Path $workspace ('artifacts\winpmem-load-probe-' + $runId + '.json')
if (-not (Test-Path -LiteralPath $probePath)) { throw '管理报告未生成。' }
$probe = Get-Content -LiteralPath $probePath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($probe.FailureStage -or -not $probe.ConsumerReport) { throw '读取器未正常启动，请查看管理报告。' }
if (-not (Test-Path -LiteralPath $probe.ConsumerReport)) { throw '原始采样报告不存在。' }
$snapshot = Get-Content -LiteralPath $probe.ConsumerReport -Raw -Encoding UTF8 | ConvertFrom-Json
$snapshot.samples | ConvertTo-Json -Depth 8
```

这些命令在父进程结束后才读取 JSON。不要另外开一个循环用 `Get-Content` 轮询活跃报告；父脚本内部只检查独立 `.ready` 标记是否存在。

默认比较值仍为 21，只影响 `matches_current_user_report`，不会制造或筛选 `ammo_in_clip`。直接查看实际弹药值，不要求下次继续是 19、18、13 或 31。父脚本没有转发 `--expected-ammo`，不能给它添加该参数并假设生效。

读取器遍历三个槽位，槽位 0 不保证在所有游戏状态下都对应当前 UI。必须核对槽位、拥有者和独立界面。`game_memory_read=true` 只说明存在玩家读取状态为 `read` 的样本，不自动证明非空武器与界面一致。

## 6. 验收日志、正常退出与清理

以下代码继续使用第三层的 `$runId`、`$probe`、`$snapshot`。若换了 PowerShell 会话，先按保存的 GUID 重新加载同一轮报告，不要选择“最新文件”代替明确关联。

```powershell
if ($snapshot.status -ne 'snapshot_read' -or -not $snapshot.game_memory_read -or
    $snapshot.samples.Count -ne 2 -or -not $snapshot.data_completed_at_utc -or
    -not $snapshot.native_cleanup_completed -or -not $snapshot.finished_at_utc -or
    $snapshot.native_cleanup_error -or $snapshot.log_write_errors.Count -gt 0) {
    throw '采样或最终日志不完整。'
}
foreach ($sample in $snapshot.samples) {
    $owned = @($sample.weapons | Where-Object { $_.owner_matches_player -eq $true })
    if ($sample.status -ne 'read' -or $owned.Count -eq 0) { throw '没有经拥有者校验的本地武器。' }
}
if ($probe.ConsumerExitCode -ne 0 -or $probe.ConsumerTimedOut -or
    $probe.ReaderReapedAfterCompletedSnapshot -or -not $probe.GameMemoryRead -or
    -not $probe.DriverStopped -or -not $probe.TemporaryServiceDeleted -or
    -not $probe.StagedFileRemoved -or $probe.CleanupErrors.Count -ne 0) {
    throw '退出或资源清理未通过。'
}
$stderrPath = Join-Path $workspace ('artifacts\live-weapon-' + $runId + '.stderr.log')
$stdoutPath = Join-Path $workspace ('artifacts\live-weapon-' + $runId + '.stdout.log')
if ((Get-Item -LiteralPath $stderrPath).Length -ne 0) { throw 'stderr 非空，请查看原始错误。' }
if (-not (Select-String -LiteralPath $stdoutPath -Pattern 'native_vmm_close_returned' -Quiet)) {
    throw '没有观察到原生关闭返回。'
}
if (-not (Select-String -LiteralPath $snapshot.native_log -Pattern 'SHUTDOWN COMPLETED' -Quiet)) {
    throw '原生日志没有完整关闭标记。'
}
& sc.exe query ApexLabWinPmemProbe
if ($LASTEXITCODE -ne 1060) { throw '实验服务仍存在或查询异常。' }
$stagingFolder = Split-Path -Parent $probe.StagedDriverPath
if (Test-Path -LiteralPath $stagingFolder) { throw '本轮暂存目录仍存在。' }
'PASS: sampling, final logs, normal exit and cleanup'
```

通过必须同时满足业务采样、日志、正常退出、资源清理四组条件。退出码为空、强制回收后已有快照、或 `.ready` 存在都不能算正常退出。

`INFODB INIT: FAIL` 是已观察到的可选数据库初始化提示；不要把它解释为完整初始化成功，也不要单凭它否定已经通过的业务和关闭检查。新出现的错误仍应单独排查。

## 7. 独立截图与界面对照

使用 `computer-use` 技能中的 Node REPL 与官方 `@oai/sky` 接口。初始化导入 `sky`，通过 `list_windows()` 选择唯一返回的 Apex Legends 窗口；使用返回的 `id/app` 调用 `get_window()`，再调用 `get_window_state()` 截图。被其他窗口遮挡时，先 `activate_window()`，重新观察后再判断。

不要通过旧的、缺少 `browser-service.mjs` 的接口反复尝试截图。这里恢复的是可用的官方桌面截图路径，不表示缺失浏览器插件已被修复。普通 PowerShell 复现读取不依赖该桌面插件；没有可用接口时可由用户独立截图并保存原图。

截图保存到 `artifacts`，使用包含本轮 RunId 的新文件名。记录截图保存时间、HUD 数值、枪身数字及 SHA-256，关联同一轮原始 JSON 和管理报告。截图哈希可用 `Get-FileHash -LiteralPath <截图绝对路径> -Algorithm SHA256` 计算；尖括号内容必须替换成实际路径。

原始读取器不会观察 UI，`ui_verified=false` 和父脚本 `AmmoVerified=false` 保持原样。完成独立对照后另写关联报告，不能把原始值改成界面值，不能覆盖旧实验报告。

若采样和截图相隔数分钟，只能称为先后快照一致；同屏显示相同数字不能证明那段时间内从未换过武器。

## 8. 单次开火前后验证

1. 完成第 5、6 节，保存开火前两次样本及对象身份；重新截图确认仍为计划的初值。
2. 如果收到人工输入提示，重新观察窗口，丢弃旧截图坐标。不要声称完全隔离了人工干预。
3. 在训练场对唯一目标窗口执行一次左键单击并立即重新截图，不长按，不自动补发第二次点击。
4. 独立核对 HUD、枪身数字和训练场射击计数。若实际发射多发或没有发射，记录真实结果。
5. 使用新的 RunId 再执行第 5、6 节，比较前后句柄、序列号、实例、拥有者及两次弹药样本。
6. 同一武器内存和界面都减少 1、射击计数增加 1，才认定该轮单发对照通过；同时单独检查两轮退出与清理。

该方法是先后采样，不是逐帧监控。动态数值通过但退出异常时，应分别报告两项结论。历史 19→18 成功只适用于那次观察，不作为下次强制预期值。

## 9. 文件输出与异常处理

| 文件 | 含义 |
| --- | --- |
| `artifacts\winpmem-load-probe-<RunId>.json` | 提升、加载、子进程状态及资源清理 |
| `artifacts\live-weapon-<RunId>.json` | 武器模式的 samples 或人口模式的 population_samples，以及关闭、结束字段 |
| 同前缀 `.stdout.log`、`.stderr.log`、`.native.log` | 标准输出、异常及原生库日志 |
| 同前缀 `.ready` | 业务快照已保存；不是退出完成标记 |
| 同前缀 `.tmp` | 原子替换用临时文件；失败时可能保留，先审计再处理 |

| 情况 | 处理 |
| --- | --- |
| 缺少 ConsumerReport | 检查提升、设备读取门槛和游戏进程数量，不读取空路径 |
| `FailureStage=null` 但未采样 | 仍检查设备 `ReadError` 与 `ReadBytes=16`，不以空失败阶段判成功 |
| `no_local_player` 或空槽 | 确认角色和武器状态；不将其解释为真实零弹药 |
| 目标哈希变化 | 停止当前布局，重新静态核对；不只改允许哈希 |
| `PermissionError / WinError 5` | 查 stderr、保留旧 JSON 和临时文件，确认没有外部程序持续打开活跃报告 |
| `native_cleanup_error`、缺少结束字段 | 关闭未完整，不按正常退出验收 |
| `ReaderReapedAfterCompletedSnapshot=true` | 异常兜底回收，业务值可能有效，但正常退出检查失败 |
| 临时服务或目录残留 | 先辨认本轮所有者与存活进程，不清理 Windows 的 `pmem` 服务 |
| 错误 87、模块列表为空 | 保存具体阶段、权限与实际读取结果；单一错误码不能证明某个 Hook |

正常路径不修改系统安全设置，不写游戏内存。LeechCore 的设备句柄包含读写访问位，并发送元信息和采集模式 IOCTL；“代码未执行游戏内存写入”不等于所有底层句柄只申请只读权限。

## 10. 维护与清理

保留脚本、完整用户态依赖目录、源归档、许可证、成功原始报告、截图、关闭日志及证据引用。零字节日志也可能被引用，不能仅按大小删除。

每次只处理确属本轮的临时资源。驱动暂存目录是 `C:\ProgramData\ApexLabReadOnly\<RunId>`；不删除整个 ProgramData，不接管别的轮次或系统服务。清理前记录路径、大小和哈希，确认工作区边界及引用关系。

目标、Python、DLL、驱动或系统策略变化时重新做第 2 节检查。不要仅凭工具同名或签名有效就认定运行兼容。重建 DLL 后运行全部回归并记录新身份；未完成真实退出复测前，不称为完整复现成功。

### 10.1 Windows 26200 开源方案审计

针对当前 Windows build 26200 的候选仓库已单独审计，结果见[Windows 26200 开源方案审计](E:/Users/MYX/Desktop/apex/docs/Windows26200开源方案审计.md)和[机器可读摘要](E:/Users/MYX/Desktop/apex/artifacts/open-source-26200-audit-2026-11-14.json)。本轮没有候选同时满足当前 build 支持、正常内核签名和不修改系统保护三个条件；因此没有加载候选驱动，也没有虚构驱动介入后的权限变化。

## 11. 玩家列表与队伍人数读取

先完成第 1—3 节的环境、文件、测试及设备检查。此模式与第三层武器采样二选一，不要求先运行一次武器采样。目标仍为训练场，要求恰好一个游戏进程存在。

```powershell
$runId = [guid]::NewGuid()
$runId
$probeArgs = @('-NoProfile', '-File', $probeScript, '-RunId', $runId.ToString(), '-VerifyGame', '-PopulationOnly')
Start-Process -FilePath $ps64 -ArgumentList $probeArgs -Verb RunAs -WindowStyle Hidden -Wait
$probePath = Join-Path $workspace ('artifacts\winpmem-load-probe-' + $runId + '.json')
$probe = Get-Content -LiteralPath $probePath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($probe.FailureStage -or -not $probe.ConsumerReport) { throw '人口读取器未正常启动。' }
$population = Get-Content -LiteralPath $probe.ConsumerReport -Raw -Encoding UTF8 | ConvertFrom-Json
if ($population.status -ne 'population_snapshot_read' -or $population.population_samples.Count -ne 2 -or
    -not $population.game_memory_read -or -not $population.native_cleanup_completed -or
    -not $population.finished_at_utc -or $population.native_cleanup_error -or
    $population.log_write_errors.Count -gt 0 -or $probe.ConsumerExitCode -ne 0 -or
    $probe.ConsumerTimedOut -or $probe.ReaderReapedAfterCompletedSnapshot -or
    -not $probe.DriverStopped -or -not $probe.TemporaryServiceDeleted -or
    -not $probe.StagedFileRemoved -or $probe.CleanupErrors.Count -ne 0) {
    throw '人口采样、日志、正常退出或清理未通过。'
}
foreach ($sample in $population.population_samples) {
    $sample | Select-Object time_utc, client_slot_upper_bound, entity_slots_scanned,
        player_class_count, npc_class_count, decoy_class_count, spectator_team_player_count
    $sample.entities | Format-Table index, address, is_local_player, is_player, is_npc,
        is_decoy, signifier, team_id, life_state, human_control
    $sample.team_counts | Format-Table team_id, reported_player_count
    if ($sample.unreadable_or_recycled_entries.Count -gt 0 -or $sample.unknown_boolean_vtables -gt 0) {
        Write-Warning '存在不可读条目或未识别类型，分类统计可能不完整，请检查原始报告。'
    }
}
$stderrPath = Join-Path $workspace ('artifacts\live-weapon-' + $runId + '.stderr.log')
if ((Get-Item -LiteralPath $stderrPath).Length -ne 0) { throw '人口读取 stderr 非空。' }
if (-not (Select-String -LiteralPath $population.native_log -Pattern 'SHUTDOWN COMPLETED' -Quiet)) {
    throw '原生日志未确认关闭完成。'
}
& sc.exe query ApexLabWinPmemProbe
if ($LASTEXITCODE -ne 1060) { throw '实验服务仍存在或查询异常。' }
if (Test-Path -LiteralPath (Split-Path -Parent $probe.StagedDriverPath)) { throw '暂存目录仍存在。' }
```

`-PopulationOnly` 必须与 `-VerifyGame` 一起传给父脚本，不能与 `-AcquisitionOnly` 同时使用。它选择 [population_verify.py](E:/Users/MYX/Desktop/apex/static-analysis/population_verify.py)，复用相同的原生接口与日志退出机制；第 6 节要求 `snapshot_read` 的武器验收不能原样用于此模式。

每个快照扫描实体索引空间 0—65535，按 4096 字节分块读取，排除索引 0、空指针与重复实例。单次扫描预算为 35 秒；队伍数组元素数最多接受 256。读取候选对象后再次检查条目指针和序列号，记录不可读、回收复用或未知类型，而不是将它们当作零人数。

类型判断只解读 `IsPlayer / IsNPC / IsPlayerDecoy` 虚函数的简单常量返回，不在游戏进程中调用函数。玩家类型不等于真人；本地用户控制身份依据当前实验上下文，其他玩家的控制身份为未知。`npc_dummie` 与 NPC 谓词联合用于区分训练假人。观战队伍过滤与本地观战模式分别记录，不把死亡状态直接当作观战者。

这份列表是客户端当前实体视图，不是直接执行脚本 `GetPlayerArray` 的完整返回结果，也不是服务器全部真人账号清单。队伍计数不一定能对应到本地复制的玩家实体；不要将队伍表长度、客户端槽位上限或非空实体数当作玩家数。

实测列表、22 个队伍条目和各项验证限制见 [玩家与人数分析](E:/Users/MYX/Desktop/apex/docs/玩家与人数分析.md)及 [人口验证报告](E:/Users/MYX/Desktop/apex/artifacts/population-verification-2026-11-14.json)。

## 附录 A：固定环境与依赖快照

核实日期：**2026-11-14，Asia/Shanghai**。这些是本机事实，不是跨版本兼容保证。

| 项目 | 核实值 |
| --- | --- |
| 游戏文件 | `D:\Steam\steamapps\common\Apex Legends\r5apex_dx12.exe` |
| 游戏版本 | v3.0.1.30；Steam build 26445400 |
| 构建标识 | `R5pc_r5-310_J30_CL11734752_FSv31_0_2026_11_04_15_03` |
| 游戏 SHA-256 | `0c1d47575261c139a8730ce67b795d24ab2238ffc426a41a1b9dacda823906f9` |
| Windows 注册表构建 | 26200.9974，DisplayVersion 25H2 |
| Python | `E:\Python\Python313\python.exe`，3.13.3 AMD64 |
| MemProcFS 包 | 5.19.0，保留于 `.tools\forensics-python\` |
| LeechCore | 本地修改并编译，日志标识 v2.23.5 |
| WinPmem 文件 | `downloads\forensics-reference\winpmem_x64.sys`，46,520 字节 |
| 驱动 SHA-256 | `B1B866F7CF86BD2BBBD8BECA081E9DF836911D938FD129CD615CFCB04399212B` |
| 驱动签名 | Authenticode Valid，Binalyze LLC |

父脚本为 [Test-WinPmemReadOnly.ps1](E:/Users/MYX/Desktop/apex/tmp/Test-WinPmemReadOnly.ps1)，读取器为 [live_weapon_verify.py](E:/Users/MYX/Desktop/apex/static-analysis/live_weapon_verify.py)，原生客户端为 [native_vmm.py](E:/Users/MYX/Desktop/apex/static-analysis/native_vmm.py)。

当前原生读取路径只使用 Python 标准库和 ctypes。wheel 的 `leechcorepyc>=2.22.0` 是标准 Python 包入口依赖，不能据此替换本地 DLL。`pefile`、`capstone` 属于静态分析依赖；VS Build Tools 属于重建依赖，不是运行保留 DLL 的要求。

## 附录 B：当前实现参数

参数核实日期：**2026-11-14**。下列值与源文件和固定映像对应，动态实例地址不能跨会话复用。

| 项目 | 当前值与含义 |
| --- | --- |
| 服务与暂存 | `ApexLabWinPmemProbe`；按需内核驱动服务；独立 GUID 暂存目录 |
| 服务访问 | `0x10034`；CreateServiceW 可选指针传原生 NULL |
| 父脚本设备探测 | `\\.\pmem`，GENERIC_READ，要求完整 16 字节 |
| 样本 | 两次，间隔约 0.4 秒；完整字节检查，不失败补零 |
| 读取标志 | `0x101`，NOCACHE 与 NOCACHEPUT |
| 长度限制 | 每次虚拟读取 1—4096 字节 |
| 采样期限 | 完整读取 120 秒；采集诊断 20 秒 |
| 关闭宽限 | `.ready` 出现后最多 20 秒；超时回收本轮子进程 |
| 本地玩家句柄 RVA | `0x3D3DCD0` |
| 实体数组 RVA | `0x6B515D8`，条目步长 `0x20` |
| 句柄解释 | 索引低 16 位、序列号高 16 位；条目序列号在 +8、实例指针在 +0 |
| 无效句柄 | `0xFFFFFFFF` |
| 库存访问器 | 虚表字节偏移 `0x9C0`；函数 RVA `0x22CBD30` |
| 访问器字节 | `48 8D 81 58 19 00 00 C3` |
| 武器句柄数组 | 玩家内 `0x1958 + 0x58 + slot*4`，slot=0、1、2 |
| 武器拥有者 | 对象内 `0x15D0`，解析后与本地玩家比较 |
| 弹匣字段 | 对象内 `0x1600`，小端 int32 |

MemProcFS 参数数组依次为：空字节串、`-printf`、`-device pmem`、`-v`、`-disable-python`、`-disable-symbols`、`-dtb <系统DTB清除低12位>`、`-logfile <本轮原生日志>`、`-loglevel f:4`。保留后台刷新，不使用 `-norefresh`。

结果保存采用 `.tmp` 原子替换，遇 PermissionError 最多尝试 21 次、间隔 50 毫秒。清理检查点写入失败仍尝试关闭原生库，错误可见且不伪报成功。父进程运行中不读 JSON，只检测 `.ready`；取得子进程 Handle 并等待退出后读取最终报告。

本地 LeechCore 的 PMEM 入口只附加外部已加载设备，服务生命周期由父脚本拥有；`LcRead` 已修正单页短读首尾边界。底层散读仍未对每次 ReadFile 的实际字节数追加比较，完整性保证主要来自原生客户端返回值与字节数检查；这个边界不能因两轮成功而认定全面覆盖。

## 附录 C：成功证据、修复与验证范围

核实日期：**2026-11-14**。

| 证据 | 已验证结果 |
| --- | --- |
| [修复后完整验证](E:/Users/MYX/Desktop/apex/artifacts/logging-exit-fix-verification-2026-11-14.json) | 两轮采样、最终日志、正常关闭及清理通过 |
| [第一轮原始采样](E:/Users/MYX/Desktop/apex/artifacts/live-weapon-7cb06cbd-4e23-4614-a1d0-e6c1f046b697.json) | `[13,13]`，原生清理完成，结束时间存在 |
| [第二轮原始采样](E:/Users/MYX/Desktop/apex/artifacts/live-weapon-91eb9691-c7c5-47ea-afd5-49ee8f23159d.json) | `[31,31]`，原生清理完成，结束时间存在 |
| [单发动态验证](E:/Users/MYX/Desktop/apex/artifacts/ammo-dynamic-verification-2026-11-14.json) | 同一武器内存与界面 19→18，射击计数 +1；该历史轮次退出日志曾异常 |
| [独立界面对照](E:/Users/MYX/Desktop/apex/artifacts/weapon-reproduction-verification-2026-11-14.json) | 两次内存 19 与随后 HUD、枪身 19 一致 |
| [文件占用对照](E:/Users/MYX/Desktop/apex/artifacts/winerror5-lock-proof-2026-11-14.json) | 无句柄时替换成功；保持读句柄时复现 WinError 5 |
| [本手册核查](E:/Users/MYX/Desktop/apex/artifacts/method-current-audit-2026-11-14.json) | 本次文档命令、路径、组件与回归核对 |
| [保留文件清单](E:/Users/MYX/Desktop/apex/artifacts/method-file-manifest-2026-11-14.json) | 文件大小及 SHA-256 |

修复后两轮 RunId 分别为 `7cb06cbd-4e23-4614-a1d0-e6c1f046b697` 和 `91eb9691-c7c5-47ea-afd5-49ee8f23159d`，退出码均为 0，强制回收为 false，stderr 为空，原生日志含 SHUTDOWN COMPLETED；关闭耗时约 0.314、0.306 秒。两轮未作对应界面截图，不推断 13 与 31 变化的原因。

文件占用机制可复现，且原代码存在读写重叠窗口；但没有为每一次历史拒绝取得内核调用栈。禁用 PDB 查找后两轮正常退出，禁用后台刷新未解决，因此没有保留后者；仍未定位上游内部具体阻塞线程。这些结论不足以证明未来所有运行或长期运行稳定。

已完成一次单发变化；尚未完成多次重复、换弹、切枪、重生、跨版本和长期稳定性验证。历史异常、原始数值和截图全部保留，不改写为成功。

## 附录 D：可选的用户态 DLL 重建

只有修改本地 LeechCore 源码时才需要此步骤。需要 VS 2022 Build Tools、v143 x64 C/C++、Windows SDK/MIDL，以及完整源码树的 `leechcore` 与 `includes`。历史成功构建采用单项目，不构建整个解决方案中的 Python 扩展或 agent。

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe' `
    'E:\Users\MYX\Desktop\apex\sources\leechcore-readonly\leechcore\leechcore.vcxproj' `
    '/p:SolutionDir=E:\Users\MYX\Desktop\apex\sources\leechcore-readonly\' `
    /t:Build /p:Configuration=Release /p:Platform=x64 /p:PostBuildEventUseInBuild=false /m:1
```

输出为 `sources\leechcore-readonly\files\leechcore.dll`。该命令不构建 WinPmem 驱动。本次手册整理只核对工程、命令和历史日志，没有重新编译；工具链改变可能改变二进制哈希，重建后应另行记录并验证。

## 附录 E：仓库、文档、社区与历史记录

以下链接的资料索引核实于 **2026-11-14**；实际运行以本机归档、文件身份和原始日志为准。

| 项目或主题 | 链接与用途 |
| --- | --- |
| WinPmem | [源码](https://github.com/Velocidex/WinPmem) · [发布页](https://github.com/Velocidex/WinPmem/releases)，采集来源 |
| LeechCore | [源码](https://github.com/ufrisk/LeechCore) · [WinPmem 后端](https://github.com/ufrisk/LeechCore/wiki/Device_WinPMEM)，本地源码及接口参考 |
| MemProcFS | [源码](https://github.com/ufrisk/MemProcFS) · [头文件](https://github.com/ufrisk/MemProcFS/blob/master/includes/vmmdll.h) · [初始化与关闭](https://github.com/ufrisk/MemProcFS/blob/master/vmm/vmmdll_core.c) · [参数文档](https://github.com/ufrisk/MemProcFS/wiki/_CommandLine)，原生解析与符号选项 |
| Cheat Engine | [源码](https://github.com/cheat-engine/cheat-engine) · [Wiki](https://wiki.cheatengine.org/) · [社区](https://forum.cheatengine.org/)，初期对照材料，不是最终读取依赖 |
| 静态布局参考 | [CApexDumpWasm](https://github.com/ccdescipline/CApexDumpWasm) · [R5SDK](https://github.com/Mauler125/r5sdk)，历史类型和元数据参考，不证明当前布局 |
| Apex 社区 | [UnknownCheats 结构讨论](https://www.unknowncheats.me/forum/apex-legends/319804-apex-legends-reversal-structs-offsets.html)，部分页面未能核验，不作为本机成功证据 |
| DMA 对比 | [Metick/CheatEngine-DMA](https://github.com/Metick/CheatEngine-DMA)，本次未使用，不是软件采集依赖 |
| Windows 权限 | [进程访问权限](https://learn.microsoft.com/en-us/windows/win32/procthread/process-security-and-access-rights) · [ReadProcessMemory](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-readprocessmemory) |
| 驱动服务 | [CreateServiceW](https://learn.microsoft.com/en-us/windows/win32/api/winsvc/nf-winsvc-createservicew) · [签名要求](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/kernel-mode-code-signing-requirements--windows-vista-and-later-) |

LeechCore 源码保留 GPL-3.0；MemProcFS wheel 保留 AGPL-3.0 文本，同时 METADATA 显示 Other/Proprietary 分类，均保留不自行改写。附带 DLL、数据库及符号资源的许可证与分发说明也应保留。

详细历史过程、旧的 21/19/18 发观察、失败试验和原审查记录见 [历史实验记录](E:/Users/MYX/Desktop/apex/docs/archive/method-history-2026-11-14.md)。该归档不是现行执行手册；旧异常状态不能替代本手册的正常退出判据。

## 12. 独立离线 R5Flowstate 最终复现路径

本节是独立离线客户端的现行复现入口，与第 1—11 节的 Steam 受保护客户端读取实验分开。执行本节不需要 CE、Blackbone、WinPmem、DMA、驱动注入或修改 Windows 安全策略，也不会把 DLL 部署到 Steam Apex。保留前文是为了记录历史读取方法和失败原因；下次复现离线功能时从本节开始。

### 12.1 固定身份与目录

核实时间：**2026-11-15，Asia/Shanghai**。路径是本机实际路径；换机器或换盘符必须先修改本节所有命令并重新计算哈希。

| 项目 | 当前值 |
| --- | --- |
| 独立游戏根目录 | `D:\R5Flowstate\Game` |
| 独立客户端 | `D:\R5Flowstate\Game\r5apex.exe` |
| 独立服务器 | `D:\R5Flowstate\Game\r5apex_ds.exe` |
| 启动器 | `D:\R5Flowstate\Launcher\current\R5FlowstateLauncher.exe` |
| SDK 源码 | `E:\Users\MYX\Desktop\apex\sources\r5flowstate-sdk\s21-r5sdk-s21-unify` |
| SDK 源码归档 | `E:\Users\MYX\Desktop\apex\downloads\r5flowstate\s21-r5sdk-s21-unify.zip` |
| SDK 分支/提交记录 | `s21-unify` / `10077bbf020292fa0bab69d9a9a05fef125d6ba5` |
| 客户端身份 SHA-256 | `3F44BF9AEC8A18E7FA578B8A4927D8D27BBA5AB8980119BE9935137ADCD04CB5` |
| HD 资源 | 未启用；`INSTALL_STATE.json` 中 `hd_enabled=false`、`hd_ready=false` |

安装器为 `downloads\r5flowstate\R5Flowstate-win-Setup.exe`。首次安装使用安装器将启动器放到 `D:\R5Flowstate\Launcher`，再在启动器中把游戏目录设为 `D:\R5Flowstate\Game`，只安装基础客户端、服务器和平台资源，不选择高清材质。已有可用安装时不要重复覆盖目录；先检查 `D:\R5Flowstate\Game\INSTALL_STATE.json`、客户端哈希和下表文件是否存在。

### 12.2 依赖检查

当前重建使用 Visual Studio 2022 Build Tools 17.14.60、MSVC v143 14.44.35207、Windows SDK 10.0.26100（目标系统用户态版本为 10.0.26200）、CMake 3.29.3 和 Python 3.13.3 x64。保留 DLL 运行不需要 Python；Python 只用于本地 Agent Link 诊断脚本。

```powershell
Set-Location 'E:\Users\MYX\Desktop\apex'
& 'E:\Python\Python313\python.exe' --version
& 'C:\Program Files\cmake-3.29.3-windows-x86_64\bin\cmake.exe' --version
Test-Path 'D:\R5Flowstate\Game\r5apex.exe'
Test-Path 'D:\R5Flowstate\Game\r5apex_ds.exe'
Test-Path 'sources\r5flowstate-sdk\s21-r5sdk-s21-unify\CMakeLists.txt'
```

### 12.3 源码编译

从 ZIP 源码树构建时，构建后可能出现 `fatal: not a git repository`，这是版本信息后处理对 ZIP 树的提示，不是编译失败。必须以 CMake/MSBuild 的最终退出码以及 `game\client.dll`、`game\server.dll` 是否生成作为判据。C4819 代码页警告也应保留在构建日志中，不能改写为错误或成功。

```powershell
Set-Location 'E:\Users\MYX\Desktop\apex'
$cmake='C:\Program Files\cmake-3.29.3-windows-x86_64\bin\cmake.exe'
& $cmake -S 'sources/r5flowstate-sdk/s21-r5sdk-s21-unify' `
  -B 'artifacts/r5flowstate-draw-build' -G 'Visual Studio 17 2022' -A x64 `
  -DOPTION_RETAIL=ON -DOPTION_CERTAIN=OFF -DOPTION_WIP=ON `
  -DOPTION_LTCG_MODE=ALL -DOPTION_WARNINGS_AS_ERRORS=OFF `
  -DBOOST_REGEX_STANDALONE=OFF
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
& $cmake --build 'artifacts/r5flowstate-draw-build' --config Release --target client server -- /m:2
if ($LASTEXITCODE -ne 0) { throw 'Release client/server build failed' }
```

本轮 2026-11-15 实际结果：配置成功，client/server 均成功链接；源码输出 `client.dll` SHA-256 为 `3D13A3EB1F63FC7D8EE2693D7E49C39DB66E1037E48AA8ACFB1F3B22BC1DAC22`，`server.dll` 为 `DACD2D7BF66D306DCD9609DE7EFEB386E0F12B8EE7B5D3E7F74DE05A09E9AC91`。链接器、工具链或源码时间变化可能导致 client 哈希变化，因此不要把旧的 `05B5...` 固定值当成重建必得值；每轮都应以源文件、部署文件和复现报告中的同轮哈希为准。

### 12.4 部署与启动

部署脚本只匹配 `D:\R5Flowstate\Game` 下的独立进程。没有 `-StopExisting` 时，若发现同路径进程会拒绝覆盖；使用该参数表示明确允许停止并重启本地离线实例。

```powershell
Set-Location 'E:\Users\MYX\Desktop\apex'
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
  '.\native-diagnostics\start_r5flowstate_weapon_server.ps1' -StopExisting
if ($LASTEXITCODE -ne 0) { throw 'Server deployment failed' }
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
  '.\native-diagnostics\start_r5flowstate_training.ps1' -StopExisting
if ($LASTEXITCODE -ne 0) { throw 'Client deployment failed' }
```

脚本会校验客户端 EXE 身份、复制 SDK 输出 DLL、计算源/目标哈希并把参数写入：

`artifacts\r5flowstate-aim\deployment-current.json` 以及 `artifacts\r5flowstate-aim\weapon-training\server-deployment.json`。

本轮实际启动结果：服务器 PID `26476`、客户端 PID `29104`；两进程启动后均保持存活并完成日志检查，随后按退出清理步骤停止，复查无残留。客户端安装 DLL 使用本轮构建哈希 `3D13A3EB...1DAC22`，服务器使用 `DACD2D7B...9EAC91`。`platform\logs\loader_trace.txt` 记录了 `client.dll` 的 PE 检查、`LoadLibraryExW` 和 `SDK_Init` 返回；服务器日志进入游戏状态初始化。PID 仅属于本轮，下一次必须重新枚举。

### 12.5 进入训练场和 Agent Link

客户端启动到主菜单后，通过本地开发控制台连接服务器；token 只从本机 `D:\R5Flowstate\Game\platform\agentlink_client.token` 与 `agentlink.token` 读取，不写入文档或命令历史。

```powershell
& 'E:\Python\Python313\python.exe' '.\native-diagnostics\r5flowstate_agent.py' `
  --role client --cmd='connect 127.0.0.1:37015'
& 'E:\Python\Python313\python.exe' '.\native-diagnostics\r5flowstate_agent.py' `
  --role server --logs-since 0
```

第一条命令的用途是建立客户端到本地服务器的连接；第二条命令读取启动日志。日志中出现 `[AGENT-LINK]`、脚本注册、`GameState changed` 等条目只能证明控制台和启动链路可用，不能单独证明弹药、瞄准或命中功能。需要执行功能测试时，使用现有 `native-diagnostics` 测试脚本，并在每轮结束后执行 `-attack` 释放、恢复开关和进程退出检查。

### 12.6 已验证功能与证据

最终功能验证报告为 [omni360/verification.json](E:/Users/MYX/Desktop/apex/artifacts/r5flowstate-aim/weapon-training/omni360/verification.json)，记录 `back_facing_fixture`、`disabled_misses`、`enabled_hits`、`occluded_target_not_damaged`、`attack_released`、`cleanup_completed` 六项均为 true；该报告中的机枪夹具结果是禁用伤害 0、启用伤害 81、遮挡目标伤害 0。对应的最终已验证二进制和 PDB 保留在同目录，截图和 `verified-real-shots.json` 也保留。

此前已完成并保留的离线功能包括：SDK 内置 GUI 菜单、中文字体、右键平滑瞄准与范围圆、模型可见部分绿色轮廓、遮挡部分红色填充、弹匣字段 HUD 对照、无后坐力、有限锥角弹道修正及 360° 发射时目标选择。弹匣字段是 S21 `C_WeaponX::m_ammoInClip` 的 `0x1590`；早期 Steam 内存实验中的 `0x1600` 不适用于此独立 SDK，不能混用。最终功能实现使用 SDK loader 正常加载的 `client.dll/server.dll`，不是 Blackbone、CE 或外部注入器。

这些是已保存的功能级事实，不代表所有武器、移动目标、远距离弹道或长时间运行都已覆盖。当前 2026-11-15 复现轮重新验证了源码编译、DLL 部署、客户端/服务器启动和 loader 日志；没有在本轮重复开火，因此不要把本轮启动记录写成新的命中样本。

### 12.7 脚本改动、回滚和常见错误

弹道脚本安装器为 `native-diagnostics\r5flowstate_install_weapon_training.py`。首次运行会保存原始 `.nut` 文件和 `script-manifest.json`；更新 360° 方向辅助使用 `--update`，回滚使用 `--restore`，两者不能同时指定。回滚后重新安装前应保留当前 manifest 和原始备份，先按清单核对文件哈希，不要手工覆盖未知版本脚本。

最常见的复现错误及处理如下：

- DLL 被运行中的客户端锁定：先停止同一路径客户端，再由部署脚本复制；不要删除或覆盖 Steam 目录文件。
- `client.dll` 哈希与报告不同：确认是否刚刚重新链接；若是，更新本轮报告和部署清单，不要只改允许哈希。
- `server_ready=false`：这是安装器内容清单状态，不能替代 `r5apex_ds.exe`、`server.dll` 和启动日志检查；服务器部署脚本会直接检查实际文件。
- Agent Link 连接失败：先确认客户端/服务器进程仍存活、37019/37020 仅监听 `127.0.0.1`，再读取对应启动日志；不要输出 token。
- 游戏退出或测试中断：先释放攻击键、恢复 `local_aim_enable/local_model_highlight/local_no_recoil/local_bullet_assist`，再停止本轮启动的两个进程；不要结束其他路径的 Apex 进程。
- 旧的 Steam CE/Blackbone 报告出现 WinError 5、`0xC0000022` 或权限不足：这些属于受保护客户端历史实验，不是本离线路线的前置条件。

### 12.8 复现完成判据

一次完整复现必须同时满足：CMake 配置和 Release client/server 构建退出码为 0；源 DLL、D 盘部署 DLL 和本轮 JSON 哈希一致；客户端与服务器从 `D:\R5Flowstate\Game` 启动且 loader 日志确认 SDK 初始化；Agent Link 能读取启动日志；若执行功能回归，则对应验证 JSON 的功能项和清理项均为 true。只满足“进程启动”时，结论应写成启动复现成功；只有重新完成六项功能回归，才能写成该轮功能回归通过。

### 12.9 离线路线参考资料

| 资料 | 用途与边界 |
| --- | --- |
| [R5Flowstate SDK](https://github.com/R5Flowstate/s21-r5sdk) | 当前 SDK 源码和 S21 构建参考；本机实际以归档源码和提交记录为准 |
| [R5Flowstate Launcher](https://github.com/R5Flowstate/launcher) | 启动器和安装流程参考；本机使用 v0.1.62 源码归档 |
| [R5Flowstate 官网](https://r5flowstate.com/) | 离线项目入口、版本和社区索引 |
| [r5reloaded/r5reloaded](https://github.com/r5reloaded/r5reloaded) | 独立客户端路线的公开参考；不作为本机 SDK 二进制依赖 |
| [Dear ImGui](https://github.com/ocornut/imgui) | SDK 内置菜单绘制参考；本机使用 SDK 自带第三方源码 |
| [DirectX 11 CreatePixelShader](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createpixelshader) | 模型高亮 shader 验证的 API 语义参考 |
| [R5Flowstate 社区/讨论入口](https://github.com/R5Flowstate/s21-r5sdk/discussions) | 公开讨论入口；本轮没有把社区帖子当作本机成功证据 |

Blackbone、Cheat Engine、WinPmem、LeechCore、MemProcFS 和 DMA 仓库仍列在附录 E，作为历史 Steam 读取路线和对照资料；它们不是本独立离线路线的依赖，也不应为了本路线关闭 Secure Boot、签名校验或修改保护回调。

### 12.10 本轮复现记录

本轮记录日期：**2026-11-15，Asia/Shanghai**。源码编译和部署命令按 12.3、12.4 执行；客户端/服务器均正常运行。当前构建产物和启动清单见 `artifacts\r5flowstate-aim\deployment-current.json`、`artifacts\r5flowstate-aim\weapon-training\server-deployment.json`；功能历史最终报告见 12.6。构建过程中出现的代码页警告和 ZIP 树 Git 提示已保留为非阻断信息。未进行新的射击、换弹、长时间稳定性或所有武器回归，因此这些项目继续标记为待验证。
