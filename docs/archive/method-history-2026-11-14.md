# Historical experiment archive

Archived on 2026-11-14. This file preserves previous experimental states; use docs/方法.md for current operating steps.

# 本地内存读取实验方法与验证记录

## 阅读导航

| 章节 | 解决的问题 |
| --- | --- |
| 1—3 | 本次结论、适用边界、术语与组件分工 |
| 4—6 | 环境与文件检查、证据等级、成功判据 |
| 7—9 | 无保护测试复现、边界读取测试、对象可信度检查 |
| 10—12 | 故障分类、输出文件解释、生命周期与清理 |
| 13—15 | 文件维护、版本变化、验收清单 |
| 16 | 本机执行入口、参数、逐层检查和结束步骤 |
| 附录 A | 已核验的实验环境和结果快照 |
| 附录 B | 本地文件索引与证据关系 |
| 附录 C | 开源仓库、官方文档和社区入口 |
| 附录 D | 实验过程中的关键判断与未采用路线 |
| 附录 E | 本机成功实现、加载参数、对象访问链与排错细节 |
| 附录 F | 运行与构建依赖、无驱动检查及本机复核结果 |

## 1. 目标与适用范围

本实验的目标是取得本地进程中一个可解释的对象实例及数值，并将读取结果与独立界面观察进行核对。数值必须来自实际读取，不能由界面文本、预期输入值、磁盘中的初始数据或错误补零代替。

最终验证记录包含两次一致的内存采样、对象关系检查和随后取得的界面截图。实验目标数值随实际界面状态调整；最终对照值为 21。

本文提供实验方法、证据审计、组件分工、故障判断、文件管理、无保护测试复现，以及本机已成功执行的软件采集路径。第 16 节记录现有脚本的执行入口，附录 E 记录该版本的加载参数与对象访问链。现有代码包含本机路径和版本约束，不能视为可跨环境直接运行的通用工具。

第 7 节的基础练习使用自己开发的测试程序、CE 自带教程、可合法调试的离线程序及人工构造的普通文件。第 16 节和附录 E 则对应本机固定版本的成功实验，两类复现的目标和依赖不同。

## 2. 当前结论与验证边界

最终结果是一次成功的快照与界面对照：

| 项目 | 结论 |
| --- | --- |
| 对象实例 | 取得了本地玩家及当前武器实例 |
| 数值读取 | 连续两次取得相同弹药整数 |
| 独立界面观察 | HUD 和枪身计数器均与读取值一致 |
| 对象关系 | 武器拥有者匹配本地玩家 |
| 数据完整性 | 读取要求完整字节数，未以补零伪造成功 |
| 内存写入 | 本次读取程序未执行游戏内存写入 |
| 结束状态 | 退出与清理均有独立记录；当前修复版本两轮正常退出，见附录 F.8 |

该结论不能扩展为以下主张：

- 所有游戏版本、地图、武器、角色和运行环境都已验证。
- 任何公开读取器都能直接适配本机。
- 只读操作必然不会被目标应用识别或限制。
- 同一个实例地址在重启、重生、换武器或资源重建后仍然有效。
- 已完成多次开火、换弹、换枪及其他生命周期的连续变化实验。
- 原生采集库的关闭过程完全稳定。

独立界面截图与内存采样不是同一毫秒完成的同步采集。这里确认的是本次采样窗口和随后界面观察的一致性，不是逐帧同步测量。

## 3. 术语和组件分工

### 3.1 地址与数值

| 术语 | 定义 | 常见误解 |
| --- | --- | --- |
| 文件偏移 | 数据在磁盘文件中的位置 | 将文件偏移当作进程地址 |
| RVA | 相对于映像加载基址的位置 | 将首选映像基址当作实际加载基址 |
| 虚拟地址 | 某个进程地址空间中的地址 | 将不同进程的同一数值地址视为同一对象 |
| 物理地址 | 底层内存采集来源中的地址 | 将物理地址直接当作进程地址 |
| 对象内偏移 | 成员相对于对象实例起点的位置 | 将成员偏移直接加到程序模块基址 |
| 实体句柄 | 用于定位并校验实体引用的值 | 将句柄直接当作裸指针 |
| 采样值 | 在明确时间窗口内实际读到的值 | 将预期值或界面值写入结果后称为读取成功 |

ASLR 会使实际模块加载地址变化。即使磁盘文件没有变化，也必须区分首选地址、RVA 和实际运行地址。

对象存在不代表它就是目标对象。需要结合类型相关访问器、生命周期、句柄校验和拥有者等关系判断。

### 3.2 实验涉及的层次

```text
采集来源
    ↓
底层采集接口
    ↓
进程地址空间解析
    ↓
完整性受控的字节读取
    ↓
对象与字段解释
    ↓
独立界面对照
    ↓
结果保存与资源清理
```

| 组件 | 在本次实验中的职责 | 不能据此推断的能力 |
| --- | --- | --- |
| CE / CE Local Lab | 初期扫描、接口状态和标准读取诊断 | 成功连接进程不等于已获得完整读取能力 |
| WinPmem | 本地软件采集来源 | 驱动加载成功不等于所有物理页或进程都可读取 |
| LeechCore | 统一采集接口及普通文件读取接口 | 库能初始化不等于每次小块读取都正确 |
| MemProcFS 原生组件 | 进程、模块和虚拟地址空间解析 | 底层采集可读不等于进程解析一定完成 |
| 本地 Python 读取器 | 类型解释、完整字节检查和对象关系校验 | 不能独立观察或确认画面内容 |
| 管理进程 | 有界运行、结果收集、退出回收和清理 | 退出码本身不是业务成功证明 |

最终数据由软件采集来源和原生进程解析接口取得，未使用 DMA 硬件。DMA 插件属于另一类采集接入方案，不是本次运行的依赖。

## 4. 环境和依赖检查

### 4.1 先固定目标身份

每次分析至少记录：

1. 目标文件路径和 SHA-256。
2. 文件构建标识及可确认的版本信息。
3. 目标进程 PID 和启动时间。
4. 读取工具自身的位数。
5. 相关组件版本、原始下载来源及本地修改范围。
6. 管理员令牌的实际检查结果。

PID、窗口标题和程序文件名都不能单独证明目标身份。PID 会复用，程序可能更新，窗口标题也可能不包含完整信息。

对保存的普通文件做哈希检查可使用：

```powershell
Get-FileHash -LiteralPath 'E:\Users\MYX\Desktop\apex\artifacts\weapon-ui-21-2026-11-14.jpg' -Algorithm SHA256
```

此类命令只校验已有文件，不访问运行中的游戏内存。

### 4.2 检查实际权限

应用以管理员身份启动、父进程具有管理员权限、命令子进程实际提升，是不同的检查项。

普通 PowerShell 可以检查当前命令进程：

```powershell
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
$principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
```

这只报告当前进程的权限，不会提升权限。

还应分别核对：

| 检查对象 | 应记录的证据 |
| --- | --- |
| 命令执行进程 | 当前进程令牌 |
| CE 进程 | CE 自身令牌 |
| 目标进程句柄 | 实际授予权限掩码 |
| 特权操作 | 对应操作是否在明确授权的执行上下文中成功 |

如果命令进程与 CE 的权限不同，不能由其中一个的检查结果代替另一个。

### 4.3 区分权限申请与实际授予

`OpenProcess` 返回非空句柄，说明取得了一个句柄，但不说明所有请求权限都已授予或后续操作一定成功。

应同时保存请求掩码和实际授予掩码。标准读取所需的权限和有限信息查询所需的权限不是同一组。

| 现象 | 可得结论 |
| --- | --- |
| 能查询目标路径 | 部分基础信息查询可用 |
| 句柄信息查询成功 | 句柄本身可查询且未失效 |
| 模块枚举成功 | 对应枚举路径在本次条件下可用 |
| 实际读到完整字节 | 对应地址、长度和接口的这次读取成功 |

这些结论逐层成立，不能相互跳过。

### 4.4 依赖来源和许可证

区分源码仓库、发布包、Python wheel、驱动文件和本地编译产物。文件名中的 `signed` 不能代替签名验证；GitHub 提交签名也不能代替 Windows 驱动签名验证。

保留原始压缩包、发布文件哈希、许可证和本地修改说明。对于库的开发分支，应记录实际取得的归档哈希，不能只记录会继续变化的 `master` 地址。

## 5. 分层成功判据

### 5.1 采集来源

至少区分以下状态：

- 文件或驱动存在。
- 文件身份、哈希和签名检查完成。
- 相关接口可以打开。
- 采集端能报告内存范围等元信息。
- 对一个确定有效、允许读取的测试范围取得完整数据。

低地址的一次小读取成功，只能证明该次读取成功。元信息查询成功，也不能代替实际数据读取。

如果采集端公布的范围不包含某个地址，读该地址失败不能作为整个来源失效的证据。尤其不能把不在范围内的零地址当作唯一对照。

### 5.2 地址空间解析

需要确认进程识别、模块识别和读取内容相互一致。不能仅凭“返回了一个看起来正常的基址”认定解析成功。

允许调试的测试程序可采用以下检查：

1. 目标 PID 与程序身份一致。
2. 返回模块名称与指定目标一致。
3. 读取的格式头与预期格式一致。
4. 格式头中的结构位置和长度合理。
5. 后续读取没有通过失败补零而伪造成功。

某些插件初始化、符号查询或资源清理与核心读取不是同一个阶段。日志应说明具体阶段，避免把整个初始化过程统一称为“读不到”。

### 5.3 对象可信度

对于带句柄的对象模型，应检查引用仍指向同一代对象。一个实体槽位可能被释放并重新利用，只有索引正确仍不足以排除旧引用。

应根据可确认的对象模型，选择独立关系校验，例如拥有者、所属容器、合法访问器或生命周期状态。不能仅用对象附近出现了一个预期整数作为类型证明。

### 5.4 数值与界面对照

通过一次数值对照，至少需要：

| 条件 | 原因 |
| --- | --- |
| 数值确实由读取接口取得 | 排除把预期值直接写入结果 |
| 数据宽度明确 | 排除字符串、整数和浮点解释混淆 |
| 对象关系校验通过 | 排除同名字段或无关对象 |
| 两次采样保留原始记录 | 观察对象和数值是否稳定 |
| 界面观察独立保存 | 避免读取器自行宣称看到了 UI |
| 记录采样与观察时间 | 区分同步测量与先后快照 |

## 6. 证据组织方式

### 6.1 四类证据

| 类型 | 内容 | 证明范围 |
| --- | --- | --- |
| 静态证据 | 文件哈希、格式、指令、属性或类型元数据 | 说明这个文件中存在何种结构或访问 |
| 原始采样 | 时间、实例身份、实际读取值和读取阶段 | 说明运行时这次读取取得了什么 |
| 界面证据 | 独立截图或人工观察记录 | 说明界面在观察时显示了什么 |
| 管理与清理证据 | 子进程、临时资源及清理状态 | 说明实验如何结束 |

四类证据各有职责。静态偏移表不能证明运行时数值；界面截图不能证明内存读取；清理成功也不能证明业务读取成功。

### 6.2 结论强度

建议采用以下分级：

| 等级 | 含义 |
| --- | --- |
| 线索 | 字符串或原始字节命中，尚未确认结构 |
| 静态确认 | 指令边界、注册信息或类型关系得到核对 |
| 运行时读取 | 取得完整字节，并能解释其对象身份 |
| 快照对照 | 运行时值与独立界面观察一致 |
| 动态对照 | 在可调试测试环境中，值随已知输入按预期变化 |
| 稳定性验证 | 多次启动及相关生命周期变化下仍能正确运行 |

当前证据已达到单次开火前后的动态对照等级：同一武器对象的弹药字段与界面均从 19 变为 18。多次重复、换弹、换枪和长期稳定性验证仍未完成，具体证据见附录 F.7。

## 7. 无保护测试复现

### 7.1 选择受控目标

使用自己编写的最小程序或 CE 自带教程。目标应满足：

- 初始数值已知。
- 可以通过程序自身正常操作改变数值。
- 数据类型已知。
- 没有反作弊或其他进程保护组件。
- 允许调试，且不会影响他人会话。

### 7.2 标准扫描闭环

1. 启动受控目标，令测试整数显示为 21。
2. 在 CE 中选择这个测试进程。
3. 使用 4 字节整数和精确数值执行首次搜索。
4. 通过测试程序自身操作将整数变成 20。
5. 在 CE 中执行再次搜索。
6. 再改变一次数值，检查候选是否跟随变化。
7. 关闭并重新启动测试程序，确认旧地址是否失效。

这个闭环用于验证扫描工具、数据类型和筛选操作。候选数量下降并不自动证明最终候选就是目标变量，仍需进行数值变化对照。

### 7.3 扫描失败时的最小排查

| 失败位置 | 优先检查 |
| --- | --- |
| 首次结果为零 | 目标身份、类型、扫描范围、实际读取能力 |
| 再次筛选变成零 | 数值是否已变化、比较条件、对象是否重建 |
| 重启后地址失效 | 动态分配、ASLR、对象生命周期 |
| 多个地址始终同步 | 状态副本、显示缓存、派生值 |
| 进度完成但没有结果 | 读取成功统计，而不只是进度条 |

### 7.4 普通文件的原生读取回归

文件回归使用人工构造的普通数据文件，不依赖运行中的游戏，也不加载采集驱动。

工作目录为：

```text
E:\Users\MYX\Desktop\apex
```

执行文件读取回归：

```powershell
Set-Location -LiteralPath 'E:\Users\MYX\Desktop\apex'
& 'E:\Python\Python313\python.exe' -B -m unittest discover -s tests -p test_leechcore_read_contract.py -v
```

此测试需要保留本地用户态库 `leechcore.dll`。测试自动生成已知内容的文件，完成后关闭文件上下文并删除该测试文件。

测试覆盖：

1. 对齐的小块读取。
2. 非对齐的小块读取。
3. 跨页的小块读取。
4. 首尾均为部分页的多页读取。
5. 完整对齐页读取。
6. 超出文件范围时返回失败。

判断依据是读取成功状态与实际字节逐一相等，而不仅是“没有抛出异常”。

### 7.5 读取器可信度回归

执行读取器测试：

```powershell
Set-Location -LiteralPath 'E:\Users\MYX\Desktop\apex'
& 'E:\Python\Python313\python.exe' -B -m unittest discover -s tests -p test_live_weapon_verify.py -v
```

该命令使用模拟内存，不访问实际游戏，不加载驱动。

测试覆盖：

| 测试 | 要防止的问题 |
| --- | --- |
| 短读判定 | 将不足长度的数据解释为完整整数 |
| 无效引用处理 | 将空引用或无效句柄误解为有效对象 |
| 序列号不匹配 | 使用已复用槽位中的旧句柄 |
| 合法引用解析 | 正确引用仍然必须返回完整指针 |
| 页表上下文位处理 | 将上下文标记当作页表地址的一部分 |
| 大页解释 | 忽略页粒度差异 |
| 非存在项处理 | 将不可用映射解释为零值成功 |

两组测试共 13 项。它们证明本地读取代码的相关约束，不证明某个受保护目标一定可读。

### 7.6 结果保存与关闭异常回归

```powershell
Set-Location -LiteralPath 'E:\Users\MYX\Desktop\apex'
& 'E:\Python\Python313\python.exe' -B -m unittest discover -s tests -p test_result_persistence.py -v
```

另外 4 项测试覆盖短暂替换拒绝后重试成功、持续拒绝时有界失败并保留旧快照、清理阶段日志失败仍调用原生关闭、原生关闭异常被保存且不伪报完成。这些测试使用普通文件与模拟对象，不加载驱动。三组共 17 项。

## 8. 小块读取与边界条件

### 8.1 为什么完整页成功而小块失败

原生读取库可能先将请求分为首个部分页、中间完整页和末尾部分页，再调用底层接口。

对于只请求少量字节的情况，首个部分页的实际覆盖长度不能直接按一整页扣减。若以无符号整数执行超过请求长度的扣减，会得到一个极大的数，随后可能被参数校验拒绝。

这种拒绝可能发生在真正调用底层读取之前。因此，没有设备层失败日志时，要考虑客户端本地校验和缓冲区分配路径，而不应立即认定是目标程序再次拦截。

### 8.2 通用计算原则

```text
首部分覆盖量 = 请求总量与首部分剩余容量的较小值
末部分覆盖量 = 请求实际覆盖的末部分量
中间量 = 请求总量 - 首部分覆盖量 - 末部分覆盖量
```

计算应满足：

- 每一部分都不能为负。
- 各部分合计等于请求总量。
- 小请求不得因为内部对齐而要求调用方提供更大的结果缓冲区。
- 完整页、部分页和跨页请求均需有字节级回归。
- 地址超出允许范围时应失败，而不是返回一块补零数据。

### 8.3 不通过放宽检查掩盖错误

应纠正长度和边界计算，保留原本的上限检查。直接移除长度校验，可能使一个可诊断的错误变成越界访问或不受控分配。

普通文件回归应先于进一步的实际采样，避免将读取库自身的缺陷误归因于目标应用。

## 9. 对象和字段解释

### 9.1 从类型元数据建立语义

同名字符串可能在多个实体或多个属性表中出现。必须区分：

- 字段名称。
- 所属类型或属性表。
- 字段宽度和类型。
- 对象内相对位置。
- 注册或访问该成员的代码证据。

一个字段名称在磁盘上存在，只能作为线索。静态解释应结合注册信息或数据映射，并在可能时由独立读取函数印证。

### 9.2 当前状态与历史选择状态

对象模型可能同时保存活动对象、最近选择对象、最后使用对象和切换中的对象。它们通常具有不同语义。

确认当前对象时，应选择描述当前状态的关系，并用独立对象身份检查排除最近历史记录。不能因为某个历史字段能解析出对象，就认定该对象正对应当前 UI。

### 9.3 原始整数和显示整数

界面显示值可能受状态条件影响。一个读取函数即使包含整数字段访问，也可能在其他分支返回默认值、特殊值或错误结果。

因此需要分别记录：

| 内容 | 核对方式 |
| --- | --- |
| 字段实际宽度 | 类型元数据和读取指令 |
| 当前对象身份 | 引用与关系检查 |
| 当前读取分支 | 可确认的运行条件或测试环境观察 |
| 界面显示 | 独立画面证据 |

本次没有验证所有状态分支，也没有建立“所有 UI 状态下都直接显示同一个原始字段”的结论。

## 10. 故障分类与诊断

### 10.1 错误码 87

87 对应 `ERROR_INVALID_PARAMETER`。它不是某个特定反作弊组件的唯一特征。

诊断应同时检查参数类型、句柄、地址、缓冲区、实际权限及对照目标。只有在取得额外证据后，才能进一步讨论接口 Hook、驱动回调或其他组件。

调用 `ntdll` 的导出函数与自行生成原始系统调用指令不是同一件事。记录测试结果时必须写清接口层次。

### 10.2 模块列表为空

空列表存在多种来源：

- 读取器将底层失败归一化成空集合。
- 查询与内存读取权限不足。
- 位数或结构解释不一致。
- 目标身份变化或进程已经退出。
- 对应解析器尚未完成初始化。

需要保存底层成功标志和错误信息。不能把“返回空表”自动解释为“进程没有模块”。

### 10.3 查看器中的问号

查看器中的 `??` 通常表示没有取得可显示的字节。应先检查所选地址的区域状态，区分已提交区域、仅预留区域、空闲区域及不可访问区域。

默认地址并不一定有效。对一个无效默认地址读不到，不能证明整个地址空间都不可读。

### 10.4 签名与加载

以下检查各自独立：

| 检查 | 说明 |
| --- | --- |
| 文件哈希 | 校验文件身份 |
| Authenticode | 验证对应签名链与文件签名状态 |
| 系统加载策略 | 决定当前系统是否允许该内核映像加载 |
| 接口可访问性 | 决定客户端能否使用暴露的接口 |
| 数据读取 | 决定指定范围能否取得完整数据 |

其中任何一步都不能替代后续步骤。文件名中带有 `signed` 也不属于有效验证。

### 10.5 本地原生调用的错误采集

错误信息应尽量在对应原生调用返回后立即捕获。跨过脚本函数绑定、其他 API 或清理操作后再取“最后错误”，可能取得无关值。

应保留操作名称、原始返回值、立即取得的错误码和独立系统事件。较晚发生的异常不应覆盖更早的业务读取结果。

### 10.6 初始化失败与初始化超时

初始化可能包含多个阶段：采集接口、内存范围、地址空间、符号、插件和后台任务。每个阶段应有独立的检查点。

“调用原生初始化返回失败”和“调用未在期限内返回”是两种不同结果。观察超时不能单独证明进程已经停止；必须查询实际进程或管理器结果。

当前实验中的原生解析接口参数格式也需要与通常的命令行入口区分。不能假定所有原生库都按传统可执行程序的参数规则处理程序名。

## 11. 结果文件解释

### 11.1 原始读取快照

重点字段包括：

| 字段 | 含义 |
| --- | --- |
| `status` | 当前业务读取状态 |
| `stage` | 最后到达的阶段 |
| `samples` | 原始采样数组 |
| `game_memory_read` | 是否完成了目标对象读取 |
| `ui_verified` | 读取器自身是否确认了 UI；本地读取器不会自动确认 |
| `write_operations` | 本地程序记录的目标写入操作数量 |
| `data_completed_at_utc` | 业务采样完成时间 |
| `native_cleanup_started_at_utc` | 原生关闭开始时间 |

业务采样结果应在开始关闭原生组件之前持久化。否则关闭阻塞可能使文件长时间停留在 `starting`，即使内存采样已经完成。

### 11.2 管理进程探测报告

重点字段包括：

| 字段 | 含义 |
| --- | --- |
| `Elevated` | 本次测试进程是否实际提升 |
| `DriverStarted` | 测试管理器记录的启动结果 |
| `ReadBytes` | 低层对照读取的字节数，不是业务弹药数 |
| `GameMemoryRead` | 是否收集到业务读取成功结果 |
| `ConsumerTimedOut` | 读取器是否超过总期限 |
| `ReaderReapedAfterCompletedSnapshot` | 完成快照后，读取器是否仍需由父进程回收 |
| `DriverStopped` | 临时驱动停止结果 |
| `TemporaryServiceDeleted` | 临时服务删除结果 |
| `StagedFileRemoved` | 临时副本删除结果 |
| `CleanupErrors` | 清理错误，不应静默忽略 |

`ConsumerExitCode` 为空不能视为成功，也不能视为业务值未读取。应检查业务结果与管理状态各自的证据。

### 11.3 最终独立验证报告

最终报告将原始读取、管理结果和独立界面观察关联起来，因此可以出现：

```text
原始读取器：ui_verified = false
最终独立验证报告：ui_verified = true
```

这不表示原始记录被修改。前者说明读取器自身不观察 UI，后者说明随后完成了独立界面对照。

最终报告不能修改原始采样值来匹配界面，也不能把读取失败重新标为真实的零值。

## 12. 生命周期与资源清理

### 12.1 资源所有者

管理进程、采集库和读取器应明确谁负责创建、谁负责使用、谁负责停止及删除临时资源。

库连接一个已存在的采集来源时，不应默认拥有其生命周期。尤其不能因为一个服务名已经存在，就假定这个服务是当前实验创建的。

本机存在与采集库默认名称相同的 Windows 组件，这类名称冲突需要在任何写操作之前识别。实验资源应使用可明确归属的隔离名称，并且只由其所有者清理。

### 12.2 清理的先后关系

```text
完成业务读取
    ↓
原始结果持久化
    ↓
尝试正常关闭读取器
    ↓
必要时回收本次创建的一次性工作进程
    ↓
确认相关句柄已释放
    ↓
清理本次创建的临时资源
    ↓
独立检查清理结果
```

不能为了结束一个原生库的关闭阻塞，而终止游戏、其他应用或来源不明的进程。

### 12.3 退出阻塞的处理边界

正常路径要求保存业务结果、完成原生关闭、保存结束日志，再以退出码 0 结束。父进程在子进程存活期间只检查独立 `.ready` 标记是否存在，不打开正在被原子替换的 JSON；子进程退出后才读取最终报告。

原生解析使用 `-disable-symbols` 禁用当前采样不需要的 PDB 查找，不关闭后台刷新。仍保留有界回收作为异常兜底；若触发回收，必须标注异常退出，不能因业务值已读出就标为全部正常。两轮正常退出的本机证据及限制见附录 F.8。

### 12.4 只读检查临时服务状态

以下命令只查询状态：

```powershell
sc.exe query ApexLabWinPmemProbe
```

本次最终记录对应的预期结果是服务不存在。历史清理成功不代表任意未来时刻都不存在，查询结果也不能替代对本次报告的核对。

## 13. 文件管理与清理规则

### 13.1 必须保留

- 最终验证报告。
- 最终原始快照与对应管理报告。
- 界面对照截图及其哈希。
- 可解释主要结论的静态证据和分析脚本。
- 成功方案实际使用的源码、用户态二进制、依赖、测试和许可证。
- 原始源码归档及所用文件的哈希。
- 关键的权限差异和标准接口诊断记录。

### 13.2 可以清理

- 已由完整成功记录替代的重复失败运行输出。
- 没有证据引用的空日志。
- 与当前流程无关的废弃临时检查。
- 未参与成功流程的重复下载器和空提取清单。
- 能从保留源码重新生成的编译中间文件。

空文件不一定无用。状态标记、协议文件或被结果引用的空日志，不能仅因为长度为零就删除。

### 13.3 不在本次清理范围

较早安装的工具链、其他项目源码、驱动诊断档案、磁盘审计目录、游戏安装目录、系统驱动和 Windows 配置均不因本次工作区清理而删除。

本次文件清理不需要再次启动游戏、重新加载驱动或修改系统安全配置。

### 13.4 清理必须可审计

每个删除项记录原路径、大小、哈希和删除理由。目录清理需要先确认绝对路径位于工作区，并排除重解析点。

先保留最终证据，再执行精确清单删除。不要对整个临时目录、下载目录或源码目录执行模糊匹配递归删除。

## 14. 版本变化与维护

目标文件哈希变化时，已有静态结论不再自动有效。应重新核对类型、对象关系和读取语义，而不是只改一个全局常量后继续运行。

依赖变化时至少检查：

| 变化 | 需要重新核对 |
| --- | --- |
| 用户态库版本 | API、结构大小、读取约束和生命周期 |
| Python / ABI | 原生扩展与 DLL 的位数、搜索路径和依赖 |
| 系统构建 | 解析器支持情况和实际返回结果 |
| 源码修改 | 文件回归、短读、引用校验和清理 |
| 目标生命周期 | 实例身份和对象关系 |

以下信息不应写死成跨环境规则：某次 PID、某次动态地址、某次弹药数、某台机器的实际加载基址，以及某个历史版本的成功经验。

本次代码仍保留了实验工作区路径、安装目录和版本固定条件。它不是可以任意换目标、换版本的通用发行工具。

## 15. 验收清单

### 15.1 结果验收

- [ ] 目标身份和文件哈希明确。
- [ ] 读取接口返回完整数据，未以补零代替失败。
- [ ] 对象身份和相关关系校验通过。
- [ ] 原始采样至少保留两次。
- [ ] 界面观察独立保存。
- [ ] 采样和观察时间可追溯。
- [ ] 结论没有超出实际覆盖范围。

### 15.2 结束验收

- [ ] 业务结果已在原生关闭前保存。
- [ ] 工作进程已结束或由明确所有者回收。
- [ ] 临时资源清理完成，错误没有被忽略。
- [ ] 未误改同名系统服务。
- [ ] 成功证据仍然存在且引用可解析。
- [ ] 无保护测试和文件读取回归通过。
- [ ] 已记录原生关闭等已知限制。

---

## 16. 本机执行与逐层检查

### 16.1 选择实际成功的入口

入口是 [Test-WinPmemReadOnly.ps1](E:/Users/MYX/Desktop/apex/tmp/Test-WinPmemReadOnly.ps1)。它管理临时资源并启动独立读取器；[live_weapon_verify.py](E:/Users/MYX/Desktop/apex/static-analysis/live_weapon_verify.py) 负责对象采样；[native_vmm.py](E:/Users/MYX/Desktop/apex/static-analysis/native_vmm.py) 负责原生解析接口。

这条路径没有把 CE 原来的进程句柄变成可读句柄，也没有要求在 CE Lua 控制台内执行读取器。CE 窗口和 Lua 桥接不参与最终成功样本的数据链。正常的进程句柄读取失败，与独立采集路径成功可以同时存在。

操作顺序为：确认文件身份和环境 → 选择检查层级 → 由父脚本加载并管理采集源 → 读取器保存原始结果 → 父脚本清理 → 独立观察界面 → 核对证据。

### 16.2 执行前检查

使用 64 位管理员 Windows PowerShell 5.1，解释器为 `C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe`。脚本需要管理员令牌来创建临时驱动服务；管理员身份本身不保证系统允许加载任何驱动，也不保证目标进程的标准读取接口恢复可用。

先进入现有工作区并查看脚本：

```powershell
Set-Location -LiteralPath 'E:\Users\MYX\Desktop\apex'
Get-Content -LiteralPath '.\tmp\Test-WinPmemReadOnly.ps1'
Get-FileHash -LiteralPath '.\downloads\forensics-reference\winpmem_x64.sys' -Algorithm SHA256
Get-AuthenticodeSignature -LiteralPath '.\downloads\forensics-reference\winpmem_x64.sys' |
    Select-Object Status, StatusMessage, SignerCertificate
Get-FileHash -LiteralPath 'D:\Steam\steamapps\common\Apex Legends\r5apex_dx12.exe' -Algorithm SHA256
```

将哈希与附录 A 的快照及保留文件清单比较。驱动哈希和签名还会由脚本再次检查；目标映像哈希由读取器检查。任一身份检查失败时，应先查明文件变化，不能直接删除检查或把新文件哈希填进去当作完成适配。

确认 `sources\leechcore-readonly\files\leechcore.dll`、`.tools\forensics-python\memprocfs\vmm.dll` 与 `E:\Python\Python313\python.exe` 存在。使用现有本地修改版 DLL，不能仅凭文件名相同就换成任意上游预编译包。Python 和所有原生库均须为 x64；本地 `LCConfig` 的大小检查值为 824 字节。

```powershell
[Environment]::Is64BitProcess
& 'E:\Python\Python313\python.exe' -B -c "import sys, struct; print(sys.executable); print(sys.version); print(struct.calcsize('P') * 8)"
```

第一条应为 `True`，Python 输出位数应为 `64`。父脚本固定使用上述 Python 路径，当前工作区路径不含空格。迁移到带空格的路径后，现有 `Start-Process -ArgumentList` 拼接不能直接视为已经适配。

如果父脚本报告 `AdministratorCheck`，说明实际执行进程没有管理员令牌。工具允许执行命令不等于 Windows UAC 提升。可以从普通 PowerShell 使用标准 UAC 启动同一脚本；在系统窗口确认后，仍以报告中的 `Elevated=true` 为准：

```powershell
$runId = [guid]::NewGuid()
$reproArgs = @('-NoProfile', '-File', 'E:\Users\MYX\Desktop\apex\tmp\Test-WinPmemReadOnly.ps1',
    '-RunId', $runId.ToString(), '-VerifyGame')
Start-Process -FilePath 'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe' `
    -ArgumentList $reproArgs -Verb RunAs -WindowStyle Hidden -Wait
```

上例执行第三层。第一层删去 `-VerifyGame`，第二层保留它并追加 `-AcquisitionOnly`；各轮使用新的 GUID，按顺序运行并先确认清理成功。取消 UAC 或提升失败时停止，不把旧报告当作本轮结果。后续按第 16.6 节读取同一 `$runId` 对应报告。

运行依赖和构建依赖见附录 F。直接使用保留的 DLL 时不需要重新编译，也不需要把 Python wheel 的依赖安装当作原生 C API 路径的前置步骤。

检查临时服务名有无冲突：

```powershell
sc.exe query ApexLabWinPmemProbe
```

服务不存在时，查询可能返回 1060。这对新的独立运行是正常前置状态。若服务已存在，现有脚本会停止，而不会接管或覆盖它；应先核对它是否属于尚未结束的实验。Windows 的 `pmem` 系统服务不是本实验服务，不要停止、删除或替换它。

### 16.3 第一层：仅检查加载、设备读取和清理

在管理员 PowerShell 中运行新的子 PowerShell：

```powershell
& 'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe' -NoProfile -File 'E:\Users\MYX\Desktop\apex\tmp\Test-WinPmemReadOnly.ps1'
```

不加 `-VerifyGame` 时，脚本不会启动游戏对象读取器。它仍会加载驱动、打开设备、尝试读取 16 字节并清理资源，因此不是单纯的文件签名检查。

每次使用新的 PowerShell 进程，是因为脚本通过 `Add-Type` 定义固定名称的原生接口类型。在同一会话重复加载类型可能引入额外错误。这里不修改系统执行策略；若脚本被组织或系统策略禁止执行，应记录策略错误并停止这一层检查。

输出写到 `artifacts\winpmem-load-probe-<RunId>.json`。检查 `Elevated`、`DriverStarted`、`DeviceReadable`、`ReadBytes`、`ReadError`、`FailureStage`、`FailureCode` 与清理字段。单纯进程退出码为 0 不能替代这些检查，因为父脚本捕获异常后仍会保存报告。

后续两个层级还要求本层 `ReadAttempted=true`、`ReadError=0`、`ReadBytes=16`。当前父脚本对直接设备短读或失败只记录字段，不一定设置 `FailureStage`；因此 `FailureStage=null` 不能证明本层通过。若缺少 `ConsumerReport`，先核对上述入口条件，不要直接读取不存在的报告路径。

### 16.4 第二层：检查 LeechCore 采集与页表解析

```powershell
& 'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe' -NoProfile -File 'E:\Users\MYX\Desktop\apex\tmp\Test-WinPmemReadOnly.ps1' -VerifyGame -AcquisitionOnly
```

现有脚本只有进入 `-VerifyGame` 分支后才向子进程传递 `-AcquisitionOnly`，所以这两个参数必须一起使用。该模式仍要求恰好一个 `r5apex_dx12` 进程存在，且磁盘映像身份检查通过；它不是独立于游戏进程的通用采集诊断器。

这一层取得采集元信息、检查 DTB 所在页是否可读，并根据系统 DTB 解析内核映像头。预期业务状态为 `acquisition_checked`，不应要求 `GameMemoryRead=true`，因为此模式没有采样玩家和武器。

物理地址 0 的读取失败，不等于所有物理内存不可读。父脚本直接设备读取和 LeechCore 按有效内存范围读取，是不同检查，必须分别解释。

### 16.5 第三层：保存玩家和武器快照

在目标映像仍与固定版本一致、训练场角色已生成且界面数值稳定时运行：

```powershell
$runId = [guid]::NewGuid()
$runId
& 'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe' -NoProfile -File 'E:\Users\MYX\Desktop\apex\tmp\Test-WinPmemReadOnly.ps1' -RunId $runId -VerifyGame
```

不要同时启动多轮：临时服务名称固定，已有运行会触发冲突检查。不要在父脚本外单独启动实时读取器，然后假设它会替你安装或卸载驱动；本地 LeechCore 入口按外部生命周期管理设计。

参数含义：

| 参数 | 用途 |
| --- | --- |
| 无参数 | 加载、设备 16 字节读取与清理检查 |
| `-VerifyGame` | 启动完整对象读取器，保存两次样本 |
| `-VerifyGame -AcquisitionOnly` | 启动采集诊断分支，不读取武器字段 |
| `-RunId <GUID>` | 指定本轮文件标识；使用新的 GUID，不重复旧成功轮次 |

读取器的比较预期值默认是 21。这个默认值只影响 `matches_current_user_report`，不会用于制造、覆盖或筛选 `ammo_in_clip`。若下次界面不是 21，先核对实际读值；比较标记为 false 不等于读取失败。父脚本当前没有转发 `--expected-ammo` 的参数，不能在父脚本命令行直接添加该参数并假设生效。

### 16.6 找到本轮文件并独立核对

保留执行第 16.5 节时的 PowerShell 会话，使用该节输出的 GUID 定位本轮报告，不按修改时间猜测是哪一轮：

```powershell
if (-not $runId) { throw '请先执行第 16.5 节，或将本轮 GUID 赋给 $runId。' }
$probePath = Join-Path 'E:\Users\MYX\Desktop\apex\artifacts' ('winpmem-load-probe-' + $runId + '.json')
if (-not (Test-Path -LiteralPath $probePath)) { throw '本轮管理报告不存在，请检查父脚本启动错误。' }
$probeReport = Get-Content -LiteralPath $probePath -Raw | ConvertFrom-Json
$probeReport | Select-Object RunId, FailureStage, FailureCode, FailureMessage,
    ReadAttempted, ReadBytes, ReadError, GameMemoryRead, ConsumerStatus,
    ConsumerTimedOut, ReaderReapedAfterCompletedSnapshot, DriverStopped,
    TemporaryServiceDeleted, StagedFileRemoved, CleanupErrors
```

若在新会话中继续核对，先将刚才记录的 GUID 赋给 `$runId`。不要复用附录中的旧成功 GUID，否则会把历史结果误认为本轮结果。

完整采样报告由 `ConsumerReport` 指向：

```powershell
if (-not $probeReport.ConsumerReport) { throw '本轮未启动读取器，先检查管理报告的入口条件。' }
if (-not (Test-Path -LiteralPath $probeReport.ConsumerReport)) { throw '读取器结果文件不存在。' }
$snapshotReport = Get-Content -LiteralPath $probeReport.ConsumerReport -Raw | ConvertFrom-Json
$snapshotReport | Select-Object status, stage, pid, module_base, game_memory_read,
    write_operations, data_completed_at_utc, error
$snapshotReport.samples | ConvertTo-Json -Depth 8
```

逐项检查模块头、玩家、武器拥有者和实际弹药值。`game_memory_read=true` 的实现只说明存在一次玩家读取状态为 `read` 的样本，不自动保证存在非空武器、拥有者匹配或弹药与界面相同。最终通过结论必须另外核对这些条件。

界面对照独立完成：记录截图时间、HUD 数字、枪身数字及截图哈希，再关联本轮原始样本。读取器不会自己观察屏幕，因此原始报告的 `ui_verified=false` 和父脚本的 `AmmoVerified=false` 可以与另行完成的最终对照报告一致存在。

截图用新的文件名保存在 `artifacts` 内，保留观察时间和原图。记录本轮 `RunId`、`ConsumerReport`、截图绝对路径及 SHA-256，并在核对记录中写明两次样本的槽位、拥有者是否匹配、实际弹药值、HUD 值和枪身值。两次内存样本一致但没有截图，只能称为读取成功，不能直接称为界面对照通过。

不要覆盖旧成功报告来“更新”数值；新的运行应使用新的原始文件和新的最终核对记录。`live-weapon-current-run.json` 是此前整理的引用记录，现有父脚本不会自动更新它。

### 16.7 结束检查与失败定位

正常结束后核对父报告中的 `DriverStopped=true`、`TemporaryServiceDeleted=true`、`StagedFileRemoved=true` 和空的 `CleanupErrors`，再查询 `ApexLabWinPmemProbe` 是否已经不存在。文件暂存目录在 `C:\ProgramData\ApexLabReadOnly\<RunId>`，不能据此删除整个 `C:\ProgramData` 或别的轮次目录。

若发生服务同名冲突、签名拒绝或加载拒绝，保存原始错误并停止，不把关闭安全功能作为流程中的自动下一步。若父报告表明清理失败，应先辨认仍存活的本轮子进程和资源所有者，再处理对应轮次；不能删除 Windows 的 `pmem` 服务来排除冲突。

完整读取的等待上限为 120 秒，采集诊断为 20 秒。业务结果保存并创建 `.ready` 标记后，父脚本允许最多 20 秒完成关闭；如果仍阻塞，则只回收自己创建的一次性读取进程。`ReaderReapedAfterCompletedSnapshot=true` 表示异常兜底路径，不能认定正常退出。若标记在总期限末尾出现，关闭宽限可能使总墙钟时间略超过采样期限。

## 附录 A：已核验的环境与结果快照

本附录是事实快照，核实日期为 **2026-11-14，Asia/Shanghai**。除本附录及原始记录外，文中的方法和判据不以该日期作为有效性保证。

### A.1 目标与运行环境

| 项目 | 核实值或说明 |
| --- | --- |
| 工作区 | `E:\Users\MYX\Desktop\apex` |
| 目标文件 | `D:\Steam\steamapps\common\Apex Legends\r5apex_dx12.exe` |
| 游戏版本 | `v3.0.1.30` |
| 构建标识 | `R5pc_r5-310_J30_CL11734752_FSv31_0_2026_11_04_15_03` |
| Steam build ID | `26445400` |
| 目标 SHA-256 | `0c1d47575261c139a8730ce67b795d24ab2238ffc426a41a1b9dacda823906f9` |
| 目标位数 | x64 |
| 系统注册表构建 | `26200.9974`，DisplayVersion `25H2` |
| Python 位置 | `E:\Python\Python313\python.exe` |
| 用户态编译工具 | Visual Studio 2022 Build Tools，v143；日志中出现 MSVC `14.44.35207` |
| MemProcFS 包 | `5.19.0`，`cp36-abi3-win_amd64` wheel |
| LeechCore 运行日志标识 | `v2.23.5`，本地用户态源码修改后编译 |
| 读取方式 | 软件采集和原生 C API；未使用 DMA 硬件 |

系统注册表中的 ProductName 存在遗留名称，MemProcFS 日志又显示了其解析识别的内核标识。这些不同来源的字符串没有被混用来证明某个系统版本必然受支持。

### A.2 驱动文件身份

| 项目 | 核实值 |
| --- | --- |
| 本地文件 | `downloads/forensics-reference/winpmem_x64.sys` |
| 文件大小 | 46,520 字节 |
| SHA-256 | `B1B866F7CF86BD2BBBD8BECA081E9DF836911D938FD129CD615CFCB04399212B` |
| Authenticode 检查 | `Valid` |
| 签名者 | Binalyze LLC |
| 时间戳机构 | GlobalSign TSA |
| 本次最终加载记录 | 成功，随后停止并清理 |

以上只是该文件在该次实验中的记录，不是要求在其他机器上绕过加载策略。完整签名元数据保留在本地预检记录中。

### A.3 最终快照

| 项目 | 实测值 |
| --- | --- |
| 目标 PID | `2324` |
| 模块加载基址 | `0x7FF75DE60000` |
| 本地玩家实例 | `0x121762F70` |
| 当前武器实例 | `0x12C0E97F0` |
| 已验证弹药字段地址 | `0x12C0EADF0` |
| 本次字段相对位置 | 武器对象内 `0x1600` |
| 字段类型 | `int32` |
| 第一次采样 | 21 |
| 第二次采样 | 21 |
| HUD 观察 | 21 |
| 枪身计数器观察 | 21 |
| 最终验证状态 | `PASS_SNAPSHOT_UI_MATCH` |

以上动态地址只描述这次运行，不用于生成跨会话的操作步骤。

原始两次采样对应北京时间约 **14:00:00.018** 和 **14:00:00.455**。最终独立验证记录的保存时间为北京时间约 **14:03:36**。这些时间应结合原始 JSON 的 UTC 字段阅读。

### A.4 最终资源状态

| 项目 | 最终记录 |
| --- | --- |
| 实际对象读取 | 成功 |
| 界面对照 | 成功 |
| 游戏内存写入计数 | 0 |
| 原生关闭 | 出现阻塞 |
| 一次性工作进程 | 完成快照后由父进程回收 |
| 临时驱动 | 已停止 |
| 临时服务 | 已删除，并独立查询为不存在 |
| 临时驱动副本 | 已删除 |
| 清理错误 | 空集合 |
| Windows `pmem` 服务路径 | 保持 `System32\drivers\pmem.sys` |
| 安全设置变更 | 未执行 |

## 附录 B：本地文件和证据关系

### B.1 最终证据

| 文件 | 作用 |
| --- | --- |
| [最终独立验证报告](E:/Users/MYX/Desktop/apex/artifacts/weapon-verification-2026-11-14.json) | 汇总实际采样、界面观察和清理结果 |
| [界面对照截图](E:/Users/MYX/Desktop/apex/artifacts/weapon-ui-21-2026-11-14.jpg) | 独立显示 HUD 与枪身计数器的 21 |
| [最终原始采样](E:/Users/MYX/Desktop/apex/artifacts/live-weapon-96c14819-0567-47e7-b8ab-66b6142fc290.json) | 两次真实读取及对象关系 |
| [最终管理报告](E:/Users/MYX/Desktop/apex/artifacts/winpmem-load-probe-96c14819-0567-47e7-b8ab-66b6142fc290.json) | 采集、进程回收和临时资源清理 |
| [独立采集检查](E:/Users/MYX/Desktop/apex/artifacts/live-weapon-73adc8bd-0878-4a71-85de-dc5e48dc6ad5.json) | 采集元信息、页表读取及格式头验证 |

原始读取快照中的 `ui_verified=false` 保持原样。最终报告的独立界面校验记录为 `true`，两者职责不同。

### B.2 分析与测试材料

| 文件或目录 | 保留目的 |
| --- | --- |
| `static-analysis/ammo_fields.py` | 磁盘元数据分析及证据提取 |
| `static-analysis/ammo-fields.json` | 静态结构候选与交叉校验 |
| `static-analysis/ammo-metadata-evidence.asm` | 指令证据 |
| `static-analysis/weapon_chain.py` | 静态对象关系研究材料 |
| `static-analysis/weapon-chain.json` | 静态关系结果 |
| `static-analysis/weapon-chain-evidence.asm` | 关系与读取函数的指令证据 |
| `tests/test_leechcore_read_contract.py` | 普通文件原生读取回归 |
| `tests/test_live_weapon_verify.py` | 模拟内存中的短读、引用和转换校验 |
| `sources/leechcore-readonly/` | 本地用户态源码及许可证 |
| `sources/leechcore-readonly/files/leechcore.dll` | 文件回归所用的本地用户态库 |
| `.tools/forensics-python/` | 已保存的原生组件、相关资源和许可证 |
| `downloads/forensics-reference/LeechCore-master.zip` | 原始源码归档 |
| `downloads/forensics-reference/memprocfs-5.19.0-cp36-abi3-win_amd64.whl` | 本次依赖发布文件 |
| `downloads/forensics-reference/vmmdll.h` | 原生 API 类型参考 |

实验访问程序和特权测试管理脚本保留在原位置；其执行入口见第 16 节，本机实现及版本参数见附录 E，依赖清单见附录 F。

### B.3 诊断与管理记录

| 文件 | 作用 |
| --- | --- |
| [标准与原生接口诊断](E:/Users/MYX/Desktop/apex/artifacts/module-read-diagnostic-2026-11-14.json) | 普通目标与游戏目标的对照、句柄与 PEB 检查 |
| [请求与授予权限](E:/Users/MYX/Desktop/apex/artifacts/requested-vs-granted-access-2026-11-14.json) | 权限掩码对照 |
| [管理员 CE 原句柄检查](E:/Users/MYX/Desktop/apex/artifacts/ce-admin-original-handle-2026-11-14.json) | 管理员身份不等于取得完整目标访问能力的证据 |
| [签名预检](E:/Users/MYX/Desktop/apex/artifacts/winpmem-signature-preflight.json) | 文件来源、哈希和签名元数据 |
| [公开资料检索清单](E:/Users/MYX/Desktop/apex/artifacts/public-memory-research-2026-11-14.json) | 过程中的资料索引；适配状态以最终验证为准 |
| [保留文件清单](E:/Users/MYX/Desktop/apex/artifacts/method-file-manifest-2026-11-14.json) | 方法相关文件的哈希和大小 |
| [本次文件清理清单](E:/Users/MYX/Desktop/apex/artifacts/cleanup-manifest-2026-11-14.json) | 删除对象、理由、大小和哈希 |

## 附录 C：仓库、官方文档和社区资料

本附录链接核实日期为 **2026-11-14**。网页和仓库可能继续更新；实际实验以本地归档、文件哈希和原始结果为准。

### C.1 本次涉及的主要项目

| 项目 | 链接 | 在本次实验中的用途 |
| --- | --- | --- |
| WinPmem | [源码仓库](https://github.com/Velocidex/WinPmem) · [发布页](https://github.com/Velocidex/WinPmem/releases) | 软件采集来源及公开实现参考 |
| LeechCore | [源码仓库](https://github.com/ufrisk/LeechCore) · [发布页](https://github.com/ufrisk/LeechCore/releases) | 采集抽象、普通文件回归和用户态实现参考 |
| MemProcFS | [源码仓库](https://github.com/ufrisk/MemProcFS) · [发布页](https://github.com/ufrisk/MemProcFS/releases) | 原生进程、模块和虚拟内存解析组件 |
| Cheat Engine | [源码仓库](https://github.com/cheat-engine/cheat-engine) · [官方 Wiki](https://wiki.cheatengine.org/) | 初期标准读取和工具对照 |
| CApexDumpWasm | [源码仓库](https://github.com/ccdescipline/CApexDumpWasm) | 静态属性表和 DataMap 解释参考 |
| R5SDK | [源码仓库](https://github.com/Mauler125/r5sdk) | 类型名称、历史引擎结构和概念参考 |

R5SDK 的 README 明确限制支持构建。不能将其历史结构定义视为当前正式客户端的完整布局证明。

### C.2 API 和系统文档

| 主题 | 链接 | 应用方式 |
| --- | --- | --- |
| 进程权限 | [Process Security and Access Rights](https://learn.microsoft.com/en-us/windows/win32/procthread/process-security-and-access-rights) | 区分不同访问位和实际授权 |
| 打开进程 | [OpenProcess](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-openprocess) | 核对请求、返回值和权限语义 |
| 标准读取 | [ReadProcessMemory](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-readprocessmemory) | 核对读取结果和完整字节数 |
| 内存区域 | [VirtualQueryEx](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualqueryex) | 理解区域状态，不把无效地址的失败推广到全进程 |
| 服务参数 | [CreateServiceW](https://learn.microsoft.com/en-us/windows/win32/api/winsvc/nf-winsvc-createservicew) | 区分原生 NULL、空字符串与可选参数 |
| Windows 驱动策略 | [The Windows Driver Policy](https://support.microsoft.com/en-us/windows/hardware/drivers/the-windows-driver-policy) | 理解签名显示有效与系统允许加载的差别 |
| 内核签名要求 | [Kernel-Mode Code Signing Requirements](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/kernel-mode-code-signing-requirements--windows-vista-and-later-) | 核对环境前提，不依据过时经验降低系统检查 |
| LeechCore 软件来源 | [WinPmem 后端说明](https://github.com/ufrisk/LeechCore/wiki/Device_WinPMEM) | 解释组件关系与公开前提 |
| MemProcFS 原生接口 | [API 文档入口](https://github.com/ufrisk/MemProcFS/wiki) · [原生头文件](https://github.com/ufrisk/MemProcFS/blob/master/includes/vmmdll.h) | 核对本地客户端的接口类型 |

### C.3 社区和对比资料

| 资料 | 链接 | 已核实边界 |
| --- | --- | --- |
| CE 官方社区 | [论坛](https://forum.cheatengine.org/) | 社区入口，不代表任意建议适用于当前环境 |
| CE 设置文档 | [Extra / Advanced](https://wiki.cheatengine.org/index.php?title=Help_File%3AExtra_%28Advanced%29) | 解释工具具备不同后端；不证明本机后端可用 |
| CE 相关问题报告 | [Issue #1892](https://github.com/cheat-engine/cheat-engine/issues/1892) | 反映系统配置、版本和内核功能的兼容性问题 |
| UnknownCheats Apex 讨论 | [Reversal / Structs / Offsets 主帖](https://www.unknowncheats.me/forum/apex-legends/319804-apex-legends-reversal-structs-offsets.html) | 本次未能打开部分页面，未据此认定具体成功案例 |
| CE DMA 接入 | [Metick/CheatEngine-DMA](https://github.com/Metick/CheatEngine-DMA) | 已核对其 DMA 接入定位；未用于本次实验 |
| 进程诊断驱动设计 | [System Informer KSystemInformer](https://github.com/winsiderss/systeminformer/blob/master/KSystemInformer/README.md) | 文档说明调用方和签名限制；未作为本次后端 |
| 当前产品背景 | [EA Javelin 与 Apex 公告](https://www.ea.com/games/apex-legends/apex-legends/news/javelin-anticheat-announcement) | 用于核对产品更替，不能单独证明某个具体拦截函数 |

社区帖、搜索摘要、仓库 README、可编译代码和本机成功运行，是不同的证据等级。尤其不能将反作弊服务启动时的错误 87 解决帖，直接当作内存读取错误 87 的解决案例。

### C.4 许可证与分发记录

LeechCore 本地源码保留了 GPL-3.0 许可证文本。MemProcFS wheel 内保留了 AGPL-3.0 许可证文本，同时包的 METADATA 分类显示 `Other/Proprietary License`。这些记录应完整保留，不根据单一分类字段自行替换许可证结论。

其他随包 DLL 和符号组件有各自的许可证或分发说明文件。整理实验目录不等于取得重新分发第三方二进制的许可。

## 附录 D：过程背景和关键判断

### D.1 初期失败与最终成功的区别

初期标准读取诊断中，普通对照目标可以正常读取，而游戏句柄的实际授予权限缺少相应访问位。管理员 CE 的新实例也没有通过标准读取接口取得对照数据。

最终成功记录使用了不同的软件采集和进程解析路径。不能将最终成功倒推为“原先标准读取已经恢复”，也不能将两条路径的权限结果混为一谈。

### D.2 排查中纠正的判断

| 判断 | 处理结果 |
| --- | --- |
| 搜不到弹药就说明弹药加密 | 缺少证据；先区分读取失败和数值表示 |
| 错误 87 就能确定某个 syscall Hook | 不成立，单个错误码不足以归因 |
| 只要成功打开句柄就有读取权限 | 不成立，需检查实际授权及实际读取 |
| 管理员启动能保证读目标 | 不成立，需分开验证执行令牌与目标访问条件 |
| 内存查看器全是问号就说明全进程不可读 | 不成立，默认地址可能只是预留区 |
| 能取得采集元信息就代表 RAM 读取已经完全可用 | 不成立，需检查实际数据读取 |
| 原生库关闭失败就代表此前没有读到数据 | 不成立，业务结果应先保存并单独审计 |
| 本次静态资料可直接套到任何版本 | 不成立，需要固定文件身份并重新核对 |

### D.3 本次本地用户态调整

本地用户态实现的调整集中在三类问题：实验资源生命周期与同名系统资源隔离、小块读取的边界计算、结果保存与有界回收。

普通文件回归独立复现了小块读取失败、整页读取成功的差异；修正后，非对齐、跨页和多页测试通过。该问题发生在用户态库中，不能据此认定目标游戏执行了新的拦截。

最终客户端使用原生解析接口，未把 Python 包装层的插件初始化过程作为读取成功的前提。独立界面观察由读取器之外完成。

### D.4 未完成或未采用的部分

- 没有进行换弹的数值变化实验。
- 没有读取并验证敌人数量。
- 没有验证全部活动槽位、所有角色或所有武器。
- 没有将历史社区漏洞案例用于本次实验。
- 没有采用 DMA 硬件或 CE DMA 插件。
- 没有将旧 S3 调试环境替换为当前正式客户端。
- 没有解决上游原生关闭阻塞的全部原因。
- 没有将本次结果推广为长期运行、跨版本稳定的产品能力。

### D.5 本次工作区清理

清理以最终验证报告的依赖为保护集合，保留成功原始记录、界面图像、源码、测试、许可证及可复核的关键诊断。重复失败轮次、无引用空日志、内部自动化状态快照、未使用的下载器和可重建缓存按精确清单处理。

清理过程不修改成功原始记录，不重写采样数值，不再次访问游戏内存。实际删除数量、回收空间和哈希以清理清单为准。

## 附录 E：本机成功实现与版本参数

本附录为 **2026-11-14** 核对的代码与结果快照。字段偏移、RVA、文件哈希、安装路径及接口布局只适用于这里固定的版本。下列命令记录现有文件的使用方式；后续实际复跑结果见附录 F.5。

### E.1 数据来源与权限关系

实际数据路径为：

```text
管理员 PowerShell 父脚本
  ├─ 校验并暂存官方签名 winpmem_x64.sys
  ├─ 通过 Windows 服务管理器加载临时内核驱动
  ├─ 启动独立 Python 读取进程
  │    ├─ 本地 LeechCore DLL → 已存在的 WinPmem 设备
  │    ├─ MemProcFS 原生 C API → 重建进程和模块、转换虚拟地址
  │    └─ 读取模块头、玩家、库存和武器字段 → 保存 JSON
  └─ 等待或回收本轮读取进程 → 停止驱动 → 删除临时服务和暂存文件

独立界面观察 → 与原始读取 JSON 对照 → 最终验证报告
```

这条路径不依赖通过 `OpenProcess` 得到含 `PROCESS_VM_READ` 的游戏句柄。它也不修改 CE 已有句柄的访问掩码，不修补游戏代码，不安装 Hook 来改变 `ReadProcessMemory` 的返回值。

就观测结果而言，新的数据路径避开了原先标准进程读取路径的限制；这不能用来证明原限制由哪个具体回调或函数施加。当前没有完整调用栈或拦截组件证据，不能把原因写死为某个用户态 Hook。

### E.2 临时驱动加载的实际参数

[父脚本](E:/Users/MYX/Desktop/apex/tmp/Test-WinPmemReadOnly.ps1) 的配置如下：

| 项目 | 参数或行为 |
| --- | --- |
| 驱动来源 | `downloads\forensics-reference\winpmem_x64.sys` |
| 预期 SHA-256 | `B1B866F7CF86BD2BBBD8BECA081E9DF836911D938FD129CD615CFCB04399212B` |
| 签名要求 | `Get-AuthenticodeSignature` 返回 `Valid` |
| 暂存位置 | `C:\ProgramData\ApexLabReadOnly\<RunId>\winpmem_x64.sys` |
| 服务名 | `ApexLabWinPmemProbe` |
| 服务管理器请求访问 | `3`，即连接与创建服务所需位 |
| `CreateServiceW` 服务访问 | `0x10034`，包含查询状态、启动、停止、删除 |
| 服务类型 | `1`，内核驱动 |
| 启动类型 | `3`，按需启动 |
| 错误控制 | `1`，普通错误处理 |
| 可选组、依赖、账号等参数 | 原生空指针 `IntPtr.Zero` |
| 父脚本设备路径 | `\\.\pmem` |
| 父脚本设备访问 | `GENERIC_READ`，数值 `0x80000000` |
| 设备共享 | 读共享与写共享，数值 `3` |
| 首轮设备检查 | `ReadFile` 请求 16 字节，记录实际字节数和错误 |

脚本先检查同名服务是否存在；存在时拒绝覆盖。服务、暂存文件和读取器均由本轮父脚本拥有，清理与创建记录绑定。

这里使用的是 Windows 正常服务管理器与驱动加载路径，没有记录测试签名、关闭完整性检查、漏洞驱动利用或 DMA 硬件步骤。本机一次加载成功不代表签名相同的文件在未来系统策略下必定能加载。

#### E.2.1 原生 NULL 与空字符串

`CreateServiceW` 的可选指针参数通过 P/Invoke 声明为 `IntPtr` 并传入 `IntPtr.Zero`。排查中曾出现空字符串传参引起的错误 1243；不能把字符串类型的空值一概当作底层 NULL。

`StartServiceError` 是一个短 C# 包装函数：在 `StartServiceW` 返回失败后立即调用 `Marshal.GetLastWin32Error()`，将错误码作为整数返回。这样避免跨越额外 PowerShell 函数与其他原生调用后，错误值已经变化。早期出现的 203 不能替代这次直接捕获的错误。

### E.3 LeechCore 的本地修改

主要源码为 [device_pmem.c](E:/Users/MYX/Desktop/apex/sources/leechcore-readonly/leechcore/device_pmem.c) 与 [leechcore.c](E:/Users/MYX/Desktop/apex/sources/leechcore-readonly/leechcore/leechcore.c)。

| 位置 | 本地行为 | 目的 |
| --- | --- | --- |
| `DEVICEPMEM_SERVICENAME` | 使用独立实验服务名 | 避免误操作 Windows 同名 `pmem` 服务 |
| `DevicePMEM_Open2` | 仅附加已存在的实验服务和设备 | 不让库自行接管外部资源生命周期 |
| `DevicePMEM_SvcClose` | 返回，不停止或删除服务 | 由父脚本在读取进程结束后统一清理 |
| `DevicePMEM_ReadScatter` | 增加 seek/read 错误日志 | 保留错误阶段、地址和请求大小 |
| `LcRead` | 修正首尾部分页的长度计算 | 解决小块读取失败而整页读取正常的问题 |

源码仍包含 `DevicePMEM_SvcStart` 的上游加载函数，但当前 `Open2` 的调用路径不调用它。不能仅因源码里出现该函数，就认定本次由 DLL 自动安装了服务。

#### E.3.1 小块读取的具体边界

本地 `LcRead` 中相关计算为：

```c
cMEMs = ((pa & 0xfff) + cb + 0xfff) >> 12;
fFirst = (pa & 0xfff) || (cb < 0x1000);
fLast = (cMEMs > 1) && ((pa + cb) & 0xfff);
cbFirst = fFirst ? min(cb, (DWORD)(0x1000 - (pa & 0xfff))) : 0;
cbLast = fLast ? (DWORD)((pa + cb) & 0xfff) : 0;
```

单页短请求不能同时被当作两个互不相同的首尾部分页，否则 `cb - cbFirst - cbLast` 可能无符号下溢。修正需要保留每页读取结果检查，不能用补零或删除失败检查让接口“成功”。

对应 [普通文件测试](E:/Users/MYX/Desktop/apex/tests/test_leechcore_read_contract.py) 覆盖短读、非对齐、跨页、多页及越界失败，避免把库的算法问题误判为目标限制。

#### E.3.2 “只读”与句柄访问位

父脚本的直接设备探测使用只读访问，但当前 LeechCore 的 `DevicePMEM_SvcStatusRunning` 以 `GENERIC_READ | GENERIC_WRITE` 打开设备。读取器也会发送获取元信息和选择采集模式的 IOCTL。

因此准确描述是“实验代码未执行游戏内存写入，LeechCore 未提供可写采集回调”，而不是“所有句柄都只请求只读位”或“从未向设备发送控制请求”。`config.writable=false` 和 `write_operations=0` 是代码能力与运行记录，不能代替对所有底层实现的独立审计。

#### E.3.3 用户态 DLL 重建

保留了 [构建日志](E:/Users/MYX/Desktop/apex/artifacts/leechcore-readonly-build.log)、解决方案、源码和 `files\leechcore.dll`。已清理 `files\temp` 中间文件，不影响保留 DLL 的使用。

如要从现有本地源码重新生成用户态 DLL，可用本机 VS 2022 Build Tools 的 MSBuild：

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe' `
    'E:\Users\MYX\Desktop\apex\sources\leechcore-readonly\leechcore\leechcore.vcxproj' `
    '/p:SolutionDir=E:\Users\MYX\Desktop\apex\sources\leechcore-readonly\' `
    /t:Build /p:Configuration=Release /p:Platform=x64 /p:PostBuildEventUseInBuild=false /m:1
```

历史日志的根项目为 `leechcore.vcxproj`，并使用 `PostBuildEventUseInBuild=false`。直接构建该项目并显式设置带末尾反斜杠的 `SolutionDir`，使头文件目录和输出目录与源码树一致；禁用构建后事件，避免执行此轮不需要的额外复制。不要将这条命令替换为整个 `LeechCore.sln` 的 Build；整个解决方案还包含 Python 扩展及 agent 项目，会引入本次成功路径不需要的构建条件。

本轮仅核对命令及工程属性，未重新执行构建。生成后应重新运行普通文件回归、记录输出哈希，并保留原成功二进制的身份记录。它不构建或修改 WinPmem 驱动。构建环境变化可能使二进制哈希不同，不能仅据此判断功能相同或不同。

### E.4 MemProcFS 原生初始化与读取

原生客户端先加载本地 LeechCore，调用 `LcCreate` 使用设备名 `pmem`，再通过选项 `0x0200000700000000` 取得系统 DTB。随后使用 `VMMDLL_Initialize` 初始化 `vmm.dll`。

本地参数数组如下，`dtb` 与日志路径来自当前运行：

```text
空字节串
-printf
-device pmem
-v
-disable-python
-disable-symbols
-dtb <当前采集源 DTB 清除低 12 位后的值>
-logfile <本轮 .native.log 路径>
-loglevel f:4
```

当前接口参数数组首项实际使用 `b''`。这与常规命令行工具的可执行文件名占位约定不同，不应把程序名随意塞进原生 API 参数列表。

实际用到的接口：

| 接口 | 作用 |
| --- | --- |
| `VMMDLL_PidGetFromName` | 按进程名取得 PID，并与父脚本选中的 PID 比较 |
| `VMMDLL_ProcessGetModuleBaseU` | 取得本轮模块加载基址 |
| `VMMDLL_MemReadEx` | 读取指定进程虚拟地址，要求完整字节数 |
| `VMMDLL_Close` | 原生结束；存在已记录的阻塞限制 |

目标读取使用标志 `0x101`，即代码注释中的 `NOCACHE` 与 `NOCACHEPUT`，不启用 `ZEROPAD_ON_FAIL`。每次读取限制为 1—4096 字节。接口返回失败或实际字节数不符时抛出异常，而不是返回全零并继续解析。

这项完整字节数保证来自 `native_vmm.py` 对 `VMMDLL_MemReadEx` 的返回值检查。底层 `DevicePMEM_ReadScatter` 当前检查 `ReadFile` 的布尔结果，但没有进一步比较 `cbRead` 与请求长度；不能把客户端检查概括为所有底层层次均已验证短读。这是尚未覆盖的底层短读边界，应与快照成功证据分别记录。

### E.5 固定版本的对象访问参数

下表来自 [静态访问链证据](E:/Users/MYX/Desktop/apex/static-analysis/weapon-chain.json) 和最终读取器，不是从其他版本的公开偏移表直接套用：

| 名称 | 本次值 | 语义 |
| --- | --- | --- |
| 本地玩家句柄槽 RVA | `0x3D3DCD0` | 保存 32 位实体句柄 |
| 实体条目数组 RVA | `0x6B515D8` | 实体条目数组起点 |
| 条目步长 | `0x20` | 每个索引的条目大小 |
| 条目指针偏移 | `0` | 条目中的实例指针 |
| 条目序列号偏移 | `8` | 与句柄高 16 位比较 |
| 句柄索引掩码 | `0xFFFF` | 句柄低 16 位 |
| 无效句柄 | `0xFFFFFFFF` | 空槽位 |
| 库存访问器 RVA | `0x22CBD30` | 本次虚表访问器校验目标 |
| 虚表槽字节偏移 | `0x9C0` | 读取器核对的访问器槽 |
| 玩家内库存偏移 | `0x1958` | 访问器返回的内嵌库存地址 |
| 库存内武器句柄数组偏移 | `0x58` | 3 个 32 位句柄 |
| 玩家内等效数组偏移 | `0x19B0` | `0x1958 + 0x58` |
| 武器拥有者句柄偏移 | `0x15D0` | 解算后必须等于本地玩家 |
| 弹匣字段偏移 | `0x1600` | 按小端有符号 32 位整数读取 |

`0x9C0` 是虚表内的字节偏移，不是虚函数索引。库存访问器的八字节指令必须匹配 `48 8D 81 58 19 00 00 C3`，对应返回 `player + 0x1958`。

#### E.5.1 句柄解析

下面是当前读取器的等价逻辑；`base` 必须来自本轮模块解析，`u32/u64` 必须使用完整读取接口：

```text
resolve(handle):
    若 handle == 0xFFFFFFFF，返回空
    index = handle & 0xFFFF
    serial = handle >> 16
    entry = base + 0x6B515D8 + index * 0x20
    要求 u32(entry + 8) == serial
    pointer = u64(entry)
    若 pointer == 0，返回空
    要求 pointer 是合理、8 字节对齐的用户态地址
    返回 pointer

player = resolve(u32(base + 0x3D3DCD0))
vtable = u64(player)
要求 u64(vtable + 0x9C0) == base + 0x22CBD30
要求访问器指令字节与固定版本一致

对于 slot = 0、1、2：
    handle = u32(player + 0x19B0 + slot * 4)
    weapon = resolve(handle)
    若非空：
        owner = resolve(u32(weapon + 0x15D0))
        要求 owner == player 才将其作为本地武器证据
        ammo = i32(weapon + 0x1600)
```

序列号用于排除已经回收并复用的实体条目。仅比较索引、仅取条目首地址、仅判断字段碰巧等于 21，均不满足本次证据要求。

#### E.5.2 成功轮次中的实际对象

最终原始记录中，本地玩家句柄为 `0x40003`，槽位 0 武器句柄为 `0x3201FE`，对应索引分别为 3 和 `0x1FE`，序列号分别为 4 和 `0x32`。槽位 1、2 为无效句柄。

成功轮次解析到 `weapon=0x12C0E97F0`，读取位置为 `weapon+0x1600=0x12C0EADF0`，两次采样均为 21，并与随后独立观察的 HUD、枪身计数器一致。地址只属于该会话，重启后不得直接复用。

这次场景支持槽位 0 与界面对象的对应关系，不支持“槽位 0 在所有状态下都是当前武器”的结论。后续多武器、切换、死亡或观战状态需要额外核对活动槽位和对象生命周期。

### E.6 静态记录与最终运行记录的关系

`weapon-chain.json` 保留了静态分析当时的 `runtime_verified=false`、用户报告 19 和当时的读取阻断描述。这些是历史事实，不应为了与后来的成功匹配而改写成 21 或 true。

最终结论由 `weapon-verification-2026-11-14.json` 关联新的原始样本、管理报告和截图。旧静态文件仍可证明指令和字段来源，但其中 `runtime_result_file` 指向的临时失败文件已按清理清单删除，不能再把那个路径当作可用证据附件。

### E.7 下次分析的最小记录集合

- 本轮目标映像哈希、软件构建标识及采集组件哈希。
- 新 `RunId` 对应的管理报告、原始采样、标准输出和原生日志。
- 从本轮解析得到的 PID、模块基址、实体句柄、序列号与武器实例。
- 字段原始值、采样时间、拥有者校验及独立截图。
- 完整的清理结果与原生退出状态。

若目标版本变化，先回到静态证据核对，不能仅修改 `EXPECTED_HASH` 让旧偏移通过。若系统拒绝签名驱动，保留拒绝阶段与错误码；这里没有实测证据支持通过降低系统安全设置来解决。

## 附录 F：依赖与本机复核

本附录为 **2026-11-14** 的本机事实快照。F.1—F.4 记录静态核查轮次，F.5 单独记录后续实际执行轮次，两者的执行范围不同。

### F.1 直接运行所需文件

以下相对路径均相对于 `E:\Users\MYX\Desktop\apex`：

| 依赖 | 本机路径 | 用途 |
| --- | --- | --- |
| x64 Windows PowerShell 5.1 | `C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe` | 父脚本执行环境 |
| Python 3.13.3 AMD64 | `E:\Python\Python313\python.exe` | 读取器与测试执行环境 |
| 父脚本 | `tmp\Test-WinPmemReadOnly.ps1` | 临时资源生命周期管理 |
| 读取器 | `static-analysis\live_weapon_verify.py` | 采样与输出 |
| 原生包装代码 | `static-analysis\native_vmm.py` | ctypes API 绑定 |
| 官方驱动文件 | `downloads\forensics-reference\winpmem_x64.sys` | 采集来源；单独核对哈希与签名 |
| 本地修改版 LeechCore | `sources\leechcore-readonly\files\leechcore.dll` | x64 用户态采集接口 |
| MemProcFS 主库 | `.tools\forensics-python\memprocfs\vmm.dll` | x64 原生解析 |
| 随包支持文件 | 同一 `memprocfs` 目录下的 `info.db`、`vcruntime140.dll`、`dbghelp.dll`、`symsrv.dll`、`vmmyara.dll` | 数据库、运行库及相关功能资源 |
| 输出目录 | `artifacts\` | 父脚本与读取器写入报告的目录，必须存在且可写 |
| 普通文件测试目录 | `tmp\` | 文件回归临时数据目录，必须存在且可写 |

应保留整个 `.tools\forensics-python\memprocfs` 目录及许可证，不能只复制 `vmm.dll`。其中部分文件服务于按需功能，未逐一证明每个文件都在最终快照路径中被调用；完整保留可避免遗漏运行库和资源。

运行路径使用 Python 标准库和 ctypes，不导入 `memprocfs` Python 包，也不导入 `leechcorepyc`。wheel 的 METADATA 虽声明 `leechcorepyc>=2.22.0`，但这属于标准 Python 包入口的依赖，不能据此替换当前原生路径已加载的本地 LeechCore。

`pefile`、`capstone` 用于重新执行静态分析脚本，不是当前原生读取器或两组回归测试的导入依赖。重新分析磁盘镜像、重新构建 DLL、直接运行保留二进制，应分开处理依赖。

### F.2 无驱动的 DLL 装载与导出检查

可在普通 PowerShell 中运行以下检查。它只装载用户态 DLL 和查找接口符号，不调用 `LcCreate`、`VMMDLL_Initialize`，不加载内核驱动，也不读取游戏进程：

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
'@ | & 'E:\Python\Python313\python.exe' -B -
```

本机检查返回 `DLL_LOAD_AND_EXPORTS_OK`。若失败，先检查位数、完整目录和具体 DLL 装载错误；文件存在不能替代装载检查。该成功标记仍不代表采集初始化或目标读取成功。

### F.3 仅重建用户态 DLL 时的依赖

- Visual Studio 2022 Build Tools 的 MSBuild 和 v143 x64 C/C++ 工具链。
- Windows SDK，提供项目使用的头文件、系统导入库及 MIDL 工具。
- 完整的 `sources\leechcore-readonly` 源码树，尤其 `leechcore` 与 `includes`；只有修改过的两个 C 文件不足以重建。
- 附录 E.3.3 的单项目命令及 `SolutionDir`、`PostBuildEventUseInBuild` 参数。

本机 MSBuild 文件存在，历史日志记录成功构建。保留二进制的日常复现不需要重新构建；本轮未执行编译，不能把命令静态一致写成已经重新构建通过。

### F.4 静态核查轮次的证据与边界

| 检查 | 本轮结果 |
| --- | --- |
| 驱动文件身份 | SHA-256 与固定值一致，Authenticode 为 `Valid` |
| 目标磁盘映像 | SHA-256 与读取器固定值一致 |
| Python | 3.13.3，64 位 AMD64，路径与父脚本一致 |
| 主要 DLL 位数与装载 | LeechCore、MemProcFS 均为 x64，装载及所用导出检查通过 |
| 普通文件读取回归 | 6 项通过 |
| 模拟读取器回归 | 7 项通过 |
| 命令语法 | PowerShell 代码块仅解析检查，未执行特权命令 |
| 原始成功证据 | 原始 JSON 和截图存在，保留清单中的身份可核对 |
| 完整实时复现 | 本轮未执行，不能承诺未来版本、系统策略和游戏状态相同 |

详细机器可读结果见 [方法复现核查记录](E:/Users/MYX/Desktop/apex/artifacts/method-reproduction-audit-2026-11-14.json)，审查问题与处置见 [本轮审查报告](E:/Users/MYX/Desktop/apex/docs/review/REVIEW-2026-11-14.md)。保留文件清单覆盖用户态原生目录、源码、脚本和证据；本轮没有改变读取器、驱动或原始成功数值。

### F.5 完整执行轮次

核实时间为 **2026-11-14 15:04，Asia/Shanghai**。文件身份、签名、DLL 装载以及 13 项回归再次通过，随后顺序执行三层流程。

| 阶段 | RunId | 结果 |
| --- | --- | --- |
| 未提升的初次尝试 | `4cf39e1f-e16f-4a43-b097-59dbf1348f25` | 停在 AdministratorCheck，未创建服务或加载驱动 |
| 标准 UAC 第一层 | `ca3b7b9d-8643-409e-98d9-648747b22e77` | 16 字节读取成功，清理完成 |
| 标准 UAC 第二层 | `c6180ee2-64b3-4208-ad67-e4c29dc6c644` | acquisition_checked，DTB 页及内核 MZ 验证通过，清理完成 |
| 标准 UAC 第三层 | `2c16cbdb-980b-484c-b4dd-7c600b80dcda` | snapshot_read，两次弹药均为 19，清理完成 |

第三层两次采样为北京时间 15:04:26.563 与 15:04:26.966。本地玩家实例 `0x16CE120A0`；槽位 0 武器句柄 `0x21E0D`、实例 `0x1B337FFF0`，拥有者匹配。弹药位置为实例内 `0x1600`，即 `0x1B33815F0`，类型为 int32。槽位 1、2 为空。

默认比较值为 21，故 `matches_current_user_report=false`；真实读取的 19 未被默认值覆盖。本轮没有解析具体武器名称。原生关闭仍需父进程在快照落盘后回收，不能描述为正常关闭已修复。

本轮独立查询临时服务返回 1060，暂存目录不存在。未执行内存写入或修改系统安全设置。随后通过官方桌面接口取得独立截图，HUD 与枪身计数器均为 19，与两次内存采样一致。结论为读取、清理及先后快照与界面对照复现成功，不是同步逐帧测量或受控开火实验。

本轮原始采样见 [读取结果](E:/Users/MYX/Desktop/apex/artifacts/live-weapon-2c16cbdb-980b-484c-b4dd-7c600b80dcda.json)，对应 [管理及清理结果](E:/Users/MYX/Desktop/apex/artifacts/winpmem-load-probe-2c16cbdb-980b-484c-b4dd-7c600b80dcda.json)。这些记录不覆盖此前 21 发的历史证据。

### F.6 桌面接口恢复与独立界面对照

核实日期为 **2026-11-14**，截图保存时间为北京时间 **15:08:02.599**。截图显示训练场界面，右下角 HUD 为 19，枪身数字为 19。内存采样发生在 15:04:26 左右；截图与内存样本间隔约数分钟，不应描述成同一时刻的同步观测。原始读取器的 `ui_verified=false` 保持原样，独立验证报告另行关联界面证据。

可用桌面接口为 `computer-use` 技能提供的 Node REPL 加 `@oai/sky`。首次初始化导入 `sky`，使用 `list_windows()` 取得真实窗口列表，筛选唯一的 Apex Legends 窗口，再用返回的 `id/app` 调用 `get_window()` 与 `get_window_state()`。若截图被其他窗口遮挡，使用 `activate_window()` 前置游戏窗口后重新截图；不能沿用被遮挡画面来认定游戏数字。

本次仅前置游戏窗口和截图，没有点击开火、换弹或切换武器。截图接口不依赖 CE Lua 桥接，也不需要重新加载采集驱动。

此前 `cua_repl` 初始化错误指出浏览器插件中的 `browser-service.mjs` 缺失；本次通过技能指定的现有官方桌面接口恢复了截图能力，没有修改或重新安装那个浏览器插件。因此确认的是可用截图路径恢复，不是缺失模块已经被补装。

证据文件：

| 文件 | 用途 |
| --- | --- |
| [19 发界面截图](E:/Users/MYX/Desktop/apex/artifacts/weapon-ui-19-reproduction-2026-11-14.png) | HUD、枪身计数器独立视觉证据 |
| [截图元数据](E:/Users/MYX/Desktop/apex/artifacts/weapon-ui-19-reproduction-2026-11-14.json) | 保存时间、窗口身份和 SHA-256 |
| [完整复现对照报告](E:/Users/MYX/Desktop/apex/artifacts/weapon-reproduction-verification-2026-11-14.json) | 关联两次实际采样、界面观察和清理状态 |

截图 SHA-256 为 `9368e38aa69d44962bbdfb4c7d38a6cfee1df8d7f1944653107a0dfc6452fde0`。该轮结论不覆盖旧的 21 发报告，也不证明跨版本、切枪、重生或持续运行已经验证。

### F.7 单次开火前后动态验证

核实时间为 **2026-11-14 15:14—15:15，Asia/Shanghai**。验证目标为同一武器的 `int32` 弹药字段、HUD 和枪身计数器从 19 变为 18，而不是再次寻找一个等于预期值的对象。

#### F.7.1 操作步骤

1. 用官方桌面接口确认处于训练场，HUD 和枪身均为 19。按第 16.5 节或标准 UAC 入口执行开火前只读采样，使用新的 RunId；保留两次原始值、武器句柄、实例地址及拥有者匹配结果。此轮完成清理后才继续。
2. 重新截图作为开火前证据，要求仍显示 19。若接口报告用户输入介入，重新观察窗口，不沿用旧坐标或截图。本次确实在最终前置截图之前收到一次用户输入提示，重新观察后数字仍为 19；这不是完全无人干预的隔离实验。
3. 在最新截图对应的训练场游戏窗口中，通过官方 `sky.click` 执行一次鼠标左键单击，`click_count=1`，随后立即刷新截图。不长按，不补发第二次点击，不通过内存写入改变弹药。
4. 独立检查开火后 HUD 与枪身数字，并核对训练场射击计数是否增加 1。若点击导致多发、未开火、换枪或其他状态变化，按实际结果记录，不将其标为预期的单发验证通过。
5. 以另一个新的 RunId 再执行第 16.5 节读取器，保留两次开火后样本。比较前后句柄、序列号、武器实例和拥有者，要求相同对象的弹药字段减 1；不能只比较两个不同对象的整数。
6. 核对两轮管理报告、stderr、临时服务和暂存目录。最后将原始样本、截图、动作时间、文件哈希及异常关联为动态验证报告，不覆盖旧快照。

这是先后采样：开火前采样结束后才进行输入，开火后再次启动读取器，不是每帧连续追踪。窗口激活和鼠标移动可能改变视角，不构成换枪或换弹证据。

#### F.7.2 实测结果

| 项目 | 开火前 | 开火后 |
| --- | --- | --- |
| RunId | `bc4c2134-5937-436f-8bb3-e424cb50f619` | `4f639951-4f54-4272-85b0-472951f6b7af` |
| 两次内存样本 | `[19, 19]` | `[18, 18]` |
| HUD | 19 | 18 |
| 枪身计数器 | 19 | 18 |
| 训练场射击计数 | 19 | 20 |
| 武器句柄 | `0x21E0D` | `0x21E0D` |
| 武器实例 | `0x1B337FFF0` | `0x1B337FFF0` |
| 本地玩家 | `0x16CE120A0` | `0x16CE120A0` |
| 拥有者匹配 | true | true |
| 临时资源清理报告 | 成功 | 成功 |

弹药字段地址为 `0x1B33815F0`，即同一武器实例加 `0x1600`。开火前采样时间为 15:14:00.925、15:14:01.331；一次点击执行区间约 15:14:40.562—15:14:40.963；开火后采样为 15:15:14.437、15:15:14.844。界面对照与内存值均减少 1，支持本次场景下字段与显示弹药的动态对应关系。

默认比较值仍为 21，`matches_current_user_report=false` 不参与上述通过判定。本次未修改读取器、偏移或默认值。

#### F.7.3 退出异常与结论边界

开火后读取器在两次样本及 `data_completed_at_utc` 保存成功后，写入原生清理阶段标记时发生 `PermissionError: [WinError 5]`，位置为 `save_result()` 的临时文件替换。stderr 已保留。当前证据不能区分文件短暂占用与其他访问条件，不能据此认定需要修改目录 ACL。

因此，动态数值对照通过，不代表该轮读取器正常完成全部结束日志或原生关闭。父脚本报告驱动停止、服务删除和暂存文件删除成功；独立查询服务返回 1060，两轮暂存目录均不存在。开火前轮次仍由父进程回收已完成快照的读取器；开火后轮次没有触发该回收标记，但也不能据此推断原生关闭正常。

本次确认一次 19→18 动态变化；没有执行换弹、自动连射、多武器切换、重生或长期稳定性测试。

#### F.7.4 证据索引

| 文件 | 内容 |
| --- | --- |
| [动态验证报告](E:/Users/MYX/Desktop/apex/artifacts/ammo-dynamic-verification-2026-11-14.json) | 数值、对象一致性、动作、清理和异常汇总 |
| [开火前截图](E:/Users/MYX/Desktop/apex/artifacts/ammo-dynamic-before-2026-11-14.png) | HUD 19、枪身 19、射击计数 19 |
| [开火后截图](E:/Users/MYX/Desktop/apex/artifacts/ammo-dynamic-after-2026-11-14.png) | HUD 18、枪身 18、射击计数 20 |
| [动作与截图元数据](E:/Users/MYX/Desktop/apex/artifacts/ammo-dynamic-action-2026-11-14.json) | 点击时间、界面观察、截图哈希及人工输入提示 |
| [开火前原始读取](E:/Users/MYX/Desktop/apex/artifacts/live-weapon-bc4c2134-5937-436f-8bb3-e424cb50f619.json) | 两次 19 和对象身份 |
| [开火后原始读取](E:/Users/MYX/Desktop/apex/artifacts/live-weapon-4f639951-4f54-4272-85b0-472951f6b7af.json) | 两次 18 和对象身份 |
| [开火后异常日志](E:/Users/MYX/Desktop/apex/artifacts/live-weapon-4f639951-4f54-4272-85b0-472951f6b7af.stderr.log) | 结果落盘后的文件替换异常 |

### F.8 WinError 5 与正常退出修复

核实时间为 **2026-11-14 15:19—15:25，Asia/Shanghai**。本轮未开火、换弹或写入游戏内存，只修复日志并发与退出路径，再执行真实采样。

#### F.8.1 原因和证据

原始错误发生在 `save_result()` 将 `.tmp` 原子替换为最终 JSON 时。普通文件对照实验中，无读取句柄时替换成功；保持读取句柄时复现 `WinError 5`。本机分别尝试共享掩码 3 和 7 都失败，因此单纯添加删除共享不是本机已验证的解决办法。

父脚本原先每 250 毫秒用 `Get-Content` 打开同一 JSON，子进程则频繁替换它，存在读句柄与替换重叠的窗口。对照证明文件占用可以产生相同错误，但不等于每一次历史错误均已取得完整内核调用栈归因。更确定的控制流缺陷是：原生清理之前的日志保存抛错，会跳过 `vmm.close()`。

文件对照记录见 [共享锁实验](E:/Users/MYX/Desktop/apex/artifacts/winerror5-lock-proof-2026-11-14.json)。没有通过修改 ACL、关闭安全功能或提升所有文件权限来处理该问题。

#### F.8.2 当前修复

| 文件 | 当前行为 |
| --- | --- |
| `live_weapon_verify.py` | 先写 `.tmp`，替换遇到 PermissionError 时最多重试 20 次、每次间隔 50 毫秒，总计最多 21 次尝试；持续拒绝仍抛错，旧 JSON 保留 |
| `live_weapon_verify.py` | 两次样本和完成时间保存后创建独立 `.ready` 标记；标记只通知父进程，不代表清理完成 |
| `live_weapon_verify.py` | `finalize_result()` 即使清理检查点保存失败也尝试关闭原生库；记录错误到 stderr 和结果字段，最后保存完成时间 |
| `Test-WinPmemReadOnly.ps1` | 运行中只检查标记是否存在；读取器退出后才打开 JSON，消除自身对该 JSON 的并发读取 |
| `Test-WinPmemReadOnly.ps1` | 提前取得子进程 Handle，并等待退出及重定向输出完成，获取实际 ExitCode；保留 20 秒关闭宽限和异常回收 |
| `native_vmm.py` | 正确声明 void 关闭函数，记录关闭调用与返回；初始化时禁用本次不需要的 PDB 符号查找 |

日志与关闭是两项独立验证：仅修复 JSON 读取冲突后，实际采样没有再报 WinError 5，但仍在关闭阶段超过 20 秒。尝试 `-norefresh` 也未解决，故没有保留该参数。随后仅禁用 PDB 符号查找、恢复后台刷新后，两轮均正常关闭。该对照将现象缩小到符号相关初始化或退出路径，尚未定位上游内部具体阻塞线程。

`-disable-symbols` 的含义见 [官方接口头文件](https://github.com/ufrisk/MemProcFS/blob/master/includes/vmmdll.h) 与 [初始化参数实现](https://github.com/ufrisk/MemProcFS/blob/master/vmm/vmmdll_core.c)。当前对象和字段访问不依赖 PDB；后续若需符号功能，必须另行验证，不能直接恢复参数后宣称退出同样稳定。

#### F.8.3 实际复测

| 项目 | 第一轮 | 第二轮 |
| --- | --- | --- |
| RunId | `7cb06cbd-4e23-4614-a1d0-e6c1f046b697` | `91eb9691-c7c5-47ea-afd5-49ee8f23159d` |
| 弹药样本 | `[13, 13]` | `[31, 31]` |
| 原生清理完成 | true | true |
| 结束时间字段 | 存在 | 存在 |
| 子进程退出码 | 0 | 0 |
| 强制回收标记 | false | false |
| stderr | 空 | 空 |
| 原生日志 | 含 SHUTDOWN COMPLETED | 含 SHUTDOWN COMPLETED |
| 临时驱动、服务和副本清理 | 成功 | 成功 |

原生关闭耗时分别约 0.314 秒和 0.306 秒，结束日志已写入；不是把强制终止改名为正常退出。独立服务查询返回 1060，两轮暂存目录不存在。17 项回归通过，包括新增 4 项保存和关闭错误测试。

这两轮以实际读值记录，没有要求继续保持先前的 18 发；未取得两轮对应截图，不推断 13 与 31 之间的变化原因。本轮验证的是采样、日志与正常退出，不另行声明界面对照通过。stdout 仍有非致命的 `INFODB INIT: FAIL` 提示，不能将退出修复解释为所有可选组件均初始化成功。

结论为该固定版本在两轮复测中完成采样、日志保存、正常原生关闭和资源清理；不是所有未来运行均不可能出现文件拒绝或退出异常。保留了持续拒绝时的失败分支及异常回收，不静默吞错。

验证汇总见 [日志与退出修复报告](E:/Users/MYX/Desktop/apex/artifacts/logging-exit-fix-verification-2026-11-14.json)。原始失败轮次、动态实验错误日志及旧报告均保留，未重写为成功。每轮 `.ready` 标记也保留为运行证据，后续清理不得仅因文件很小而删除仍被引用的文件。
