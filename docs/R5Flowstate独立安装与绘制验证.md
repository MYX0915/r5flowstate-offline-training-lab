# R5Flowstate 独立安装与绘制验证

当前部署的人物统计与方框版本见 [人物统计与游戏内方框](R5Flowstate人物统计与方框.md)。本文的 `60002FC8…` 二进制与截图是弹药专项验收快照，对应文件仍保存在 `artifacts\r5flowstate-draw`；当前运行版本的哈希、启动参数与数据范围以人物统计文档为准。

## 安装布局

本环境用于本机离线训练和 DLL 绘制实验。

| 用途 | 绝对路径 |
| --- | --- |
| 启动器 | `D:\R5Flowstate\Launcher` |
| 启动器程序 | `D:\R5Flowstate\Launcher\current\R5FlowstateLauncher.exe` |
| 独立游戏内容 | `D:\R5Flowstate\Game` |
| 原有 Steam Apex | `D:\Steam\steamapps\common\Apex Legends` |
| 安装包及源码压缩包 | `E:\Users\MYX\Desktop\apex\downloads\r5flowstate` |
| 启动器对应版本源码 | `E:\Users\MYX\Desktop\apex\sources\r5flowstate-launcher-reference\launcher-launcher-v0.1.62` |
| SDK 源码快照 | `E:\Users\MYX\Desktop\apex\sources\r5flowstate-sdk\s21-r5sdk-s21-unify` |
| 安装证据 | `E:\Users\MYX\Desktop\apex\artifacts\r5flowstate-install` |

安装和后续部署的写入目标限定在独立目录，不将游戏目录设为 Steam Apex，不向 Steam Apex 部署 DLL。两个版本的实体偏移和结构分别验证。

### 用户配置隔离

SDK 源码的 loader 使用 `-fnf` 选择独立用户配置，并将内置 `/Respawn/Apex_fnf` 字符串重定向到 `/Respawn/Apex_r5f`。这是源码观察，还需要与安装完成后的实际 `loader.dll` 和本轮文件写入核对。

首次启动前显式保留 `-fnf` 参数，核对实际 loader，并记录原有 Saved Games 下 `Respawn\Apex` 配置文件的哈希。运行后再次比较原有配置；只看到游戏安装目录不同不足以证明用户配置没有共用。

## 安装步骤

从附录中的项目官网进入 GitHub Releases，下载 `R5Flowstate-win-Setup.exe`。本次使用 Velopack 安装包，指定启动器路径：

```powershell
$installer = 'E:\Users\MYX\Desktop\apex\downloads\r5flowstate\R5Flowstate-win-Setup.exe'
Get-FileHash -LiteralPath $installer -Algorithm SHA256
Start-Process -FilePath $installer -ArgumentList '--silent','--installto','D:\R5Flowstate\Launcher' -Wait
```

执行前应确认目标目录没有其他安装，并把 SHA-256 与对应发布版本的资产摘要比较。安装包未签名；本次核对的是 GitHub 发布资产的 SHA-256，不是发布者 Authenticode 签名。

打开启动器并指定独立游戏目录：

```powershell
Start-Process -FilePath 'D:\R5Flowstate\Launcher\current\R5FlowstateLauncher.exe' -ArgumentList '--install-root','D:\R5Flowstate\Game'
```

确认安装页显示 `D:\R5Flowstate\Game`，再按“安装”。不启用 Settings 中的高清材质。基础安装由启动器下载并校验客户端、专用服务器及平台文件。

`D:\R5Flowstate\Game\INSTALL_STATE.json` 是安装状态，检查 `client_ready`、`server_ready`、`platform_ready` 和 `incomplete`。`hd_enabled=false`、`hd_ready=false` 对应本任务要求。安装期间生成的 `.r5fpart` 文件会预分配完整长度；文件大小不能用作已下载字节统计。“正在检查文件”的已校验字节也不能直接解释为下载完成。

启动器日志位置为当前用户 `%LOCALAPPDATA%\R5Flowstate\logs\launcher.log`。必须在与启动器相同的真实用户上下文读取 HKCU 设置；沙箱用户的 HKCU 可能不同。

### 下载并发

启动器使用 `HKCU\Software\R5Flowstate` 的 `DownloadConcurrency` 设置。源码支持范围为 1–32，0 表示自动选择，自动并发为 CPU 数量限制到 4–8。`DownloadLimitMbps=0` 表示不限速。

调整前先在启动器按“取消”，等日志确认 `Content install cancelled.`，关闭启动器，备份设置后再改值。重开后按“安装”继续。启动器会检查已有分块，因此续传开始前可能有较长磁盘校验过程。

本次修改前设置备份：`E:\Users\MYX\Desktop\apex\artifacts\r5flowstate-install\launcher-settings-before-concurrency.xml`。只调整下载并发，没有修改系统网络设置。

### 启动器专用代理

本机已有 HTTP 代理监听 `127.0.0.1:7890`。可在启动器关闭后，仅给新启动的进程传入代理环境变量；不修改系统代理配置：

```powershell
$env:HTTP_PROXY = 'http://127.0.0.1:7890'
$env:HTTPS_PROXY = 'http://127.0.0.1:7890'
$env:NO_PROXY = 'localhost,127.0.0.1,::1'
Start-Process -FilePath 'D:\R5Flowstate\Launcher\current\R5FlowstateLauncher.exe' -ArgumentList '--install-root','D:\R5Flowstate\Game'
```

这些变量属于当前 PowerShell 及其子进程，关闭该 PowerShell 不会修改系统或用户持久环境变量。该路径依赖现有代理实际运行；其他机器或其他端口需要重新核对。这里只进行官方内容下载，没有登录第三方账号。

本轮对同一官方 CDN 16 MiB 对象进行 20 秒测速，直连收到 1,310,711 字节，经已有代理收到 6,508,170 字节。两次均因测试设置的 20 秒上限返回 curl 28，HTTP 状态均为 200；不能将测试超时误报为安装器失败。

## 运行验收

必须先完成基础内容安装及校验，再执行下列验收：

1. 在启动器选择 Play Local，保持离线，不发布公共服务器。
2. 选择训练场，启动专用服务器与客户端，确认二者实际路径均位于独立游戏目录。
3. 核对客户端画面已经进入地图，可以移动、装备武器、开火。
4. 保存本轮进程身份、客户端及服务端日志、画面证据和实际错误。
5. 退出后确认本轮客户端和服务器已结束，启动器仍可再次启动地图。

首次地图加载可能需要构建 shader cache。不能把启动器“Play”按钮可用、进程创建成功或源码支持视作训练场运行成功。

客户端日志在独立安装目录下 `platform\logs\client\<本轮运行目录>`；服务端日志在 `platform\logs\server\<本轮运行目录>`。常规文件日志需对应 SDK 的 `-logfiles` 等参数启用，崩溃文件与普通日志分开核对。

## DLL 绘制验收

训练场验收成功后再适配。先使用 SDK 已有加载和绘制接口，逐步验证 DLL 加载、帧回调、固定文字绘制，最后连接当前武器和弹药。

当前源码的插件目录为安装目录下 `bin\x64_retail\plugins`，插件要求 `PluginInstance_OnLoad` 与 `PluginInstance_OnUnload` 导出。是否能使用该接口还需与实际安装的 `client.dll` 验证，不能只靠源码确定。

基线构建有 5 个导出，官方下载的 `client.dll` 有 3 个导出，二者均没有 `GetFactorySystem`。源码中的屏幕文字虚函数替换位于非客户端分支。本实现使用 SDK 已有的 ImGui 帧回调，诊断代码编译进独立游戏的 `client.dll`，由原有 `loader.dll` 正常加载。

弹药在现有 `Hook_Host_RunFrame` 完成原始帧更新后读取，通过原子变量传给 ImGui 绘制线程。玩家与武器使用已有 SDK 解析逻辑，武器句柄检查序列号及 `CWeaponX` 类型。显示开启参数为 `+local_ammo_probe 1`，没有有效武器时显示等待状态。诊断代码不写入弹药和玩家字段，不创建额外绘制窗口。

当前诊断使用 S21 `C_WeaponX::m_ammoInClip`，相对武器实例偏移为 `0x1590`。这与旧 Steam 客户端分析使用的字段不同，不能交叉套用。

### 构建

```powershell
& 'C:\Program Files\cmake-3.29.3-windows-x86_64\bin\cmake.exe' -S 'E:\Users\MYX\Desktop\apex\sources\r5flowstate-sdk\s21-r5sdk-s21-unify' -B 'E:\Users\MYX\Desktop\apex\artifacts\r5flowstate-draw-build' -G 'Visual Studio 17 2022' -A x64 -DOPTION_RETAIL=ON -DOPTION_CERTAIN=OFF -DOPTION_WIP=ON -DOPTION_LTCG_MODE=ALL -DOPTION_WARNINGS_AS_ERRORS=OFF -DBOOST_REGEX_STANDALONE=OFF
& 'C:\Program Files\cmake-3.29.3-windows-x86_64\bin\cmake.exe' --build 'E:\Users\MYX\Desktop\apex\artifacts\r5flowstate-draw-build' --config Release --target client -- /m:2
```

这是 WIP 开发构建，和官方下载的发布二进制不同。输出为源码目录下 `game\client.dll`。原始源码文件备份保存在 `artifacts\r5flowstate-install\draw-source-originals`，官方运行 DLL 备份保存在 `artifacts\r5flowstate-install\official-runtime`。

### 本机开发接口

开发模式启用 AgentLink 2：客户端 `127.0.0.1:37020`，服务器 `127.0.0.1:37019`。每轮令牌由 SDK 写到独立游戏 `platform` 目录，脚本只向本机对应端口发送认证，不打印令牌。

```powershell
& 'E:\Python\Python313\python.exe' 'E:\Users\MYX\Desktop\apex\native-diagnostics\r5flowstate_agent.py' --role client
& 'E:\Python\Python313\python.exe' 'E:\Users\MYX\Desktop\apex\native-diagnostics\r5flowstate_agent.py' --role client --cmd 'local_ammo_probe 1'
```

`INFO` 成功只证明开发接口可用，不能代替画面和弹药对照。`CMD` 返回 `ran=true` 也不能单独证明游戏进程已经退出。

### 本轮可见结果

2026-11-15，DX11 射击场使用 Flatline：

- 官方基线 HUD 与枪身计数器：19→18，射击计数 0→1。
- 第一版诊断 DLL 经过只读解析初始化后，原生显示、HUD、枪身计数器三者同步 19→18。
- 两张画面中的帧间隔分别为 5.77 ms、5.70 ms。这是渲染帧间隔，不是端到端延迟测量或长期性能保证。
- 原始画面：`artifacts\r5flowstate-draw\native-dll-ammo-19.png`、`native-dll-ammo-18.png`。
- 第一版 DLL SHA-256：`2408B96477DD5902DBE0E78E4DF8AE27B2263198A7054F3BC1BA0FC7F21DF617`。

弹药专项修复版通过重启复测：`ClassVar_LocalPlayer()` 在首次使用时初始化已有解析表，无需手动类变量查询。该次验收的 DLL SHA-256 为 `60002FC81D057AEA0BE93D32B3CB5D248F6CB79BAB87C1D923E9BBF5398485B8`，当时部署于 `D:\R5Flowstate\Game\client.dll`。

最终一轮先使用服务器 SDK 的 `GiveWeapon("mp_weapon_vinson",0,[])` 授予 Flatline，再通过 `SetActiveWeaponBySlot(0,0)` 选中主手。脚本授予的弹匣初始为 0，随后使用游戏脚本中已有的 `SetWeaponPrimaryClipCount(GetWeaponPrimaryClipCountMax())` 正常装填。这属于验证准备，不是诊断 DLL 的读取逻辑。

准备完成后进行一次实际开火：DLL、HUD、枪身计数器均为 19→18，射击计数 0→1；开火前后画面帧间隔分别为 7.60 ms 和 5.67 ms，采样计数分别为 13,310 和 14,371。没有在最终进程调用手动类变量初始化命令。

最终证据在 `E:\Users\MYX\Desktop\apex\artifacts\r5flowstate-draw`：

| 文件 | 内容 |
| --- | --- |
| `client.dll` | 最终诊断二进制 |
| `source-overlay.zip` | 对固定 SDK 提交应用的源码覆盖包，内含对应 `src` 层级 |
| `verification-2026-11-15.json` | 最终哈希、验证准备、19→18 和帧间隔事实记录 |
| `running-identity.json` | 当时进程路径、完整命令行及已加载 `client.dll` 模块身份 |
| `autoinit-before-shot.png` | 最终版 19 发画面 |
| `autoinit-after-shot.png` | 最终版开火后 18 发画面 |

源码覆盖包 SHA-256：`120294C026C8737DA959EB2ACDF00166D7A6BE385FDD90512CF78D59957A192F`。复现源码时先解压固定原始 SDK 快照，再将覆盖包中的 `src` 覆盖到该源码根目录，按上方 WIP 开发构建命令编译。仅构建成功不能视作运行验证成功。

### 启动最终诊断版

先退出已有独立客户端，检查当前进程实际路径，备份官方 DLL，再把 `artifacts\r5flowstate-draw\client.dll` 复制到独立游戏目录。不能覆盖正在运行进程使用的 DLL。

启动本地专用服务器：

```powershell
Start-Process -FilePath 'D:\R5Flowstate\Game\r5apex_ds.exe' -WorkingDirectory 'D:\R5Flowstate\Game' -WindowStyle Hidden -ArgumentList '-dedicated -port 37015 -devsdk -dev -logfiles +net_usesocketsforloopback 1 -offline +sv_onlineAuthEnable 0 +spire_host_visibility 0 +sv_cheats 1 +launchplaylist survival_firingrange +map mp_rr_canyonlands_staging_mu1 +sv_modPolicy 0'
```

本机基线服务器约 18 秒后就绪。等待服务器能够响应开发接口中的 `status`，再启动 DX11 客户端：

```powershell
& 'E:\Python\Python313\python.exe' 'E:\Users\MYX\Desktop\apex\native-diagnostics\r5flowstate_agent.py' --role server --cmd status
Start-Process -FilePath 'D:\R5Flowstate\Game\r5apex.exe' -WorkingDirectory 'D:\R5Flowstate\Game' -ArgumentList '-nodiscord -allowmultiple -devsdk -dev -offline -fnf -logfiles -windowed -language schinese -width 1920 -height 1080 +miles_language english +net_usesocketsforloopback 1 +cl_onlineAuthEnable 0 +timeout_during_load 600 +local_ammo_probe 1 +connect 127.0.0.1:37015'
```

进入训练场后拾取武器，左上角应显示实际弹药、帧间隔和递增采样次数。没有武器时显示等待；空弹匣显示 0。`local_ammo_probe 0` 关闭显示，`local_ammo_probe 1` 重新启用。

开发模式可能同时创建名为 `R5F Client` 的调试控制台；弹药绘制位于游戏原有帧中，该调试控制台不是绘制窗口。最后一次验收后独立客户端及服务器保持运行，方便继续观察。

### 退出与恢复

先通过本地开发接口执行客户端、服务器的 `quit`，再重新枚举独立目录下的实际进程。基线服务器曾在 `quit` 返回后残留且不再响应 RCON，已核对进程路径后结束该进程，记录在 `artifacts\r5flowstate-install\baseline-server-cleanup.json`。不要把开发接口请求成功当作退出成功。

恢复官方客户端必须在独立客户端退出后执行：

```powershell
Copy-Item -LiteralPath 'E:\Users\MYX\Desktop\apex\artifacts\r5flowstate-install\official-runtime\client.dll' -Destination 'D:\R5Flowstate\Game\client.dll'
```

诊断会话直接使用独立游戏程序与明确的离线参数启动，避免启动器的内容修复覆盖开发二进制。不要将本 DLL 复制到 Steam Apex。

## 实测快照

核实时间：2026-11-14 至 2026-11-15，Asia/Shanghai。

| 检查项 | 本轮观察 |
| --- | --- |
| 安装前 D 盘空闲 | 213,949,513,728 字节，约 199 GiB |
| 目标目录冲突 | 安装前 `D:\R5Flowstate` 不存在 |
| 启动器版本 | 0.1.62 |
| 安装包 SHA-256 | `87059CC5A4FA237484DE7DD73A8171C41E4FEDB23BE81A4D9356065B029C521D`，与发布页一致 |
| 安装包 Authenticode | NotSigned |
| 启动器安装 | 安装器退出码 0，已实际打开中文主窗口 |
| 内容清单 | client 1.0.23，10,573 个文件，46,376,566,371 字节 |
| 高清材质 | 初始状态 `hd_enabled=false`、`hd_ready=false` |
| 下载并发 | 已从自动选择调整为 32，并重新发起续传 |
| SDK 压缩包 SHA-256 | `0AAE4E36FC884D0A14E56BAD82BB8A3C81B5DF7B709F6FAE19C854949FAED9CB` |
| SDK 分支提交 | `s21-unify`，`10077bbf020292fa0bab69d9a9a05fef125d6ba5` |
| 未修改 SDK 的客户端构建 | CMake 配置及 Release `client` 目标均退出 0，已生成 DLL，未部署或运行 |
| 基线 DLL SHA-256 | `B67E945BE51CA0AD804CAF91D93FAA6A185812592B2CF99B887B86447C6276B7` |
| 游戏内容安装完成 | 2026-11-15 00:31:36 完成；client 1.0.23、platform 1.0.39，`incomplete=false` |
| 训练场运行验证 | DX11 实际进入 `mp_rr_canyonlands_staging_mu1`，已装备武器并验证开火 |
| DX12 首次验证 | 服务端显示 spawning，客户端等待 signon 后续数据超时；未验收成功 |
| DLL 游戏内绘制 | 最终版已重启验证自动初始化，并完成 DLL / HUD / 枪身三方 19→18 对照 |

当前频道将专用服务器程序包含在客户端及平台内容中，没有独立 server track，所以 `server_ready=false` 不代表本地专用服务器没有安装。实际已启动服务器并完成 DX11 入服。

### Steam 隔离检查的边界

`artifacts\r5flowstate-install\retail-isolation-after-baseline.json` 比较了启动前 134 个原有文件。Steam 主程序 SHA-256 保持 `0C1D47575261C139A8730CE67B795D24AB2238FFC426A41A1B9DACDA823906F9`。原有设置文件没有检测到变化，但资产缓存存在以下差异：

- `C:\Users\MYX\Saved Games\Respawn\Apex\assets\pin.txt` 内容变化。
- 新增 `C:\Users\MYX\Saved Games\Respawn\Apex\assets\temp\gladcard_cache.bin`。

因此当前实现隔离了游戏安装目录及主要用户设置，尚未证明资产缓存也完全隔离。没有启动前的文件内容备份，不能仅凭旧哈希恢复 `pin.txt`；也未擅自删除这些缓存。

GitHub 更新检查曾返回 HTTP 403（rate limit exceeded），启动器随后成功改用官方 CDN 下载清单。这是发布检查接口限流，不能解释成游戏内容无法下载。

SDK 基线构建报告：`E:\Users\MYX\Desktop\apex\artifacts\r5flowstate-install\sdk-baseline-build.json`。构建日志有 C4819 编码警告；源码来自 ZIP，没有 `.git`，版本脚本出现 Git 提示，PE 版本回退到 `0.0.0.0`。这些提示没有导致本轮构建失败，也不能证明运行兼容性。

## 附录：官方来源

- [Getting Started](https://r5flowstate.org/getting-started/)
- [Launcher 0.1.62](https://github.com/R5Flowstate/launcher/releases/tag/launcher-v0.1.62)
- [启动器源码](https://github.com/R5Flowstate/launcher)
- [安装包装脚本](https://github.com/R5Flowstate/launcher/blob/launcher-v0.1.62/setup/R5Flowstate.iss)
- [SDK s21-unify](https://github.com/R5Flowstate/s21-r5sdk/tree/s21-unify)
- [用户协议与隐私说明](https://r5flowstate.org/privacy/)
