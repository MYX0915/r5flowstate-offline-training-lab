# R5Flowstate 人物统计与游戏内方框

## 适用环境

本功能用于独立的本机离线 R5Flowstate：游戏目录 `D:\R5Flowstate\Game`，DX11 客户端为 `r5apex.exe`，专用服务器为 `r5apex_ds.exe`。不向 Steam Apex 部署本 DLL。

SDK 源码固定为 `10077bbf020292fa0bab69d9a9a05fef125d6ba5`。构建使用 Visual Studio 2022、x64、Release、`OPTION_WIP=ON`。这是开发诊断版本，不代表其他地图、其他 SDK 版本或在线服务器均已验证。

最终二进制：`E:\Users\MYX\Desktop\apex\artifacts\r5flowstate-entities\client.dll`。

SHA-256：`9B6AD26B692E76253C4E666870A381994CF188DF739A775303C2A6B6BCDB614D`。

## 统计范围

显示分为两个独立范围：

| 显示 | 数据来源 | 含义 |
| --- | --- | --- |
| Player entities / NPC / Decoy | 客户端实体内存 | 当前客户端已接收且被识别的实体；Player 包含本地玩家 |
| Local server | 自己的离线专用服务器 SDK | 当前地图的玩家实体、NPC、训练假人和诱饵总数 |
| boxes | 客户端位置、碰撞边界、当前相机 | 通过投影和屏幕范围检查后，本帧实际绘制的方框数 |

假人是 NPC 的子集，不能再次加到总数中。玩家实体数也不是通用的真人判定：存在 bot 时需要进一步结合服务器状态。本轮服务器状态为 1 human、0 bots。

客户端可能只接收全图实体中的一部分，屏幕方框还受视角和近裁剪限制。因此不能用 boxes 或客户端 NPC 数反推全图 NPC 数。未在客户端实体视图中的对象不编造位置。

## 分类与显示

| 类别 | 已接入的网络类名 | 颜色 |
| --- | --- | --- |
| 玩家 | `CPlayer` / `C_Player` | 青蓝色 |
| NPC | `CAI_BaseNPC` / `C_AI_BaseNPC` | 橙色 |
| 诱饵、假体 | `CPlayerDecoy` / `C_PlayerDecoy` | 紫色 |

本地玩家参与计数，但不画自身方框。特殊模组新增的其他网络类不会自动被上述分类覆盖。

方框使用八个碰撞边界顶点的屏幕包围矩形，不是骨骼命中盒。取不到有效碰撞边界时使用站立人形估计尺寸，并在标签添加 `~`、增加 estimated 计数。本轮实际采样的 NPC 和诱饵均为真实边界，estimated 为 0。

没有执行射线遮挡检测，框不表示目标与相机之间一定没有遮挡。

## 采样与绘制

1. 客户端已有 `Hook_Host_RunFrame` 更新后采样。实体索引表通过 SDK 已有解析器取得。
2. 每 250 ms 重扫分类与实体句柄。缓存句柄，不缓存跨帧的实体指针。
3. 每个主机帧重新校验句柄序列号，读取有效实体的世界位置和碰撞边界。
4. 世界位置复用 SDK 的 S21 世界姿态读取器；本机该版本的绝对位置字段为实体相对 `0x188`，已用服务器坐标交叉核对。
5. 碰撞属性从实体实际接收表递归查找，限制递归深度、属性数量和查询预算。
6. DX11 当前使用已有角色视角回调采集相机位置、朝向和 FOV，构造投影。没有再对同一引擎函数增加第二个 Hook。
7. 固定容量快照通过互斥保护交给 ImGui 绘制线程。最多缓存 512 个待绘制实体，超出时显示 overflow。
8. 绘制发生在游戏原有 ImGui 帧中，沿用弹药显示所在 `client.dll`，不创建透明叠加窗口。

计数采集器每秒从本地服务器调用 `GetPlayerArray()`、`GetNPCArray()`、`GetNPCArrayByClass("npc_dummie")`、`GetPlayerDecoyArray()`。结果经范围校验后发送到客户端的诊断命令。数据超过 3 秒没有更新时，显示 `STALE`，不冒充新的采样。

这条全图计数通道经过 SDK API 读取服务器状态；方框几何数据直接来自客户端实体内存。两者的数据来源没有混写。

## 启动与控制

先参考 [独立安装与运行方法](R5Flowstate独立安装与绘制验证.md) 启动离线射击场服务器。只有客户端退出后才能替换 DLL：

```powershell
Copy-Item -LiteralPath 'E:\Users\MYX\Desktop\apex\artifacts\r5flowstate-entities\client.dll' -Destination 'D:\R5Flowstate\Game\client.dll'
```

启动 DX11 客户端：

```powershell
Start-Process -FilePath 'D:\R5Flowstate\Game\r5apex.exe' -WorkingDirectory 'D:\R5Flowstate\Game' -ArgumentList '-nodiscord -allowmultiple -devsdk -dev -offline -fnf -logfiles -windowed -language schinese -width 1920 -height 1080 +miles_language english +net_usesocketsforloopback 1 +cl_onlineAuthEnable 0 +timeout_during_load 600 +local_ammo_probe 1 +local_entity_probe 1 +connect 127.0.0.1:37015'
```

一个独立安装目录只运行一个受本方法管理的客户端。枚举本轮实际 PID，再启动全图计数采集器，不照抄报告中的历史 PID：

```powershell
$r5Clients = @(Get-Process r5apex -ErrorAction SilentlyContinue | Where-Object Path -eq 'D:\R5Flowstate\Game\r5apex.exe')
if ($r5Clients.Count -ne 1) { throw '需要唯一的独立 R5 客户端' }
New-Item -ItemType Directory -Path 'E:\Users\MYX\Desktop\apex\artifacts\r5flowstate-entities' -Force | Out-Null
$monitor = Start-Process -FilePath 'E:\Python\Python313\python.exe' -WorkingDirectory 'E:\Users\MYX\Desktop\apex' -WindowStyle Hidden -ArgumentList 'E:\Users\MYX\Desktop\apex\native-diagnostics\r5flowstate_map_counts.py','--client-pid',$r5Clients[0].Id,'--log','E:\Users\MYX\Desktop\apex\artifacts\r5flowstate-entities\map-counts.jsonl' -PassThru
$monitor.Id
```

采集器只连接 `127.0.0.1:37019/37020`，使用独立安装生成的每轮令牌；不打印令牌。它会验证客户端进程实际路径，客户端退出后关闭进程句柄并结束循环。采集器没有绘制窗口。

控制命令：

```powershell
& 'E:\Python\Python313\python.exe' 'E:\Users\MYX\Desktop\apex\native-diagnostics\r5flowstate_agent.py' --role client --cmd 'local_entity_probe 0'
& 'E:\Python\Python313\python.exe' 'E:\Users\MYX\Desktop\apex\native-diagnostics\r5flowstate_agent.py' --role client --cmd 'local_entity_probe 1'
& 'E:\Python\Python313\python.exe' 'E:\Users\MYX\Desktop\apex\native-diagnostics\r5flowstate_agent.py' --role client --cmd local_entity_dump
```

`local_entity_probe 0` 关闭人物统计与方框；弹药显示由 `local_ammo_probe` 单独控制。关闭采集器后，客户端内存统计与方框仍可工作，全图栏会过期。停止采集器时仅操作本次启动取得的 PID，不按 python 进程名批量结束。

## 构建与测试

源码目录为 `E:\Users\MYX\Desktop\apex\sources\r5flowstate-sdk\s21-r5sdk-s21-unify`。源码覆盖包是 `artifacts\r5flowstate-entities\source-overlay.zip`，应覆盖到固定提交的 SDK 根目录。

覆盖包 SHA-256：`4631911CD6CFF9905186709E1FE2C65D2E93543FC3950233DEB1CB1A8500D67F`。

```powershell
& 'C:\Program Files\cmake-3.29.3-windows-x86_64\bin\cmake.exe' -S 'E:\Users\MYX\Desktop\apex\sources\r5flowstate-sdk\s21-r5sdk-s21-unify' -B 'E:\Users\MYX\Desktop\apex\artifacts\r5flowstate-draw-build' -G 'Visual Studio 17 2022' -A x64 -DOPTION_RETAIL=ON -DOPTION_CERTAIN=OFF -DOPTION_WIP=ON -DOPTION_LTCG_MODE=ALL -DOPTION_WARNINGS_AS_ERRORS=OFF -DBOOST_REGEX_STANDALONE=OFF
& 'C:\Program Files\cmake-3.29.3-windows-x86_64\bin\cmake.exe' --build 'E:\Users\MYX\Desktop\apex\artifacts\r5flowstate-draw-build' --config Release --target client -- /m:2
```

分类与投影测试位于 `tests\r5flowstate-entity-probe`，覆盖诱饵不误分为玩家、排除非人物、视口偏移、屏幕方向、相机后方、近裁剪、非有限坐标、无效视口、相机基向量与位置平移、无效 FOV。CTest 已通过 15 项检查。它不能代替游戏内的相机和边界验证。

恢复只显示弹药的上一版时，先退出独立客户端，再将 `artifacts\r5flowstate-entities\previous-ammo-build\client.dll` 复制回 `D:\R5Flowstate\Game\client.dll`。

## 本轮实测快照

核实日期：2026-11-15，Asia/Shanghai。人数和实体会随游戏状态变化，下面是本轮最后保存的快照。

| 范围 | 玩家实体 | NPC | 训练假人子集 | 诱饵 |
| --- | ---: | ---: | ---: | ---: |
| 本地服务器全图 | 1 | 13 | 13 | 0 |
| 客户端实体内存 | 1（本地 1） | 7 | 不单独猜测 | 0 |

本轮移动前曾读到客户端 NPC 3，移动到假人区域后读到 7。服务器总数也在测试过程中发生变化，因此没有用首次读到的 7 作为固定“地图人数”。

验证结果：

- 实际看见橙色框包围 NPC，近距离边界对齐；7 个客户端 NPC 均有有效位置，overflow 0，estimated 0。
- 实际创建了一个带唯一脚本名、60 秒超时的临时诱饵。客户端读到 kind 3、decoy 1，并实际显示紫色框；全图栏同步变为 decoy 1。
- 测试诱饵随后超时消失，清理查询返回 `removed=0 remaining_decoys=0`，客户端及服务器均恢复 decoy 0。没有声称该对象是由显式 Destroy 请求移除。
- 截图帧间隔包含 4.91、7.35、7.93、7.11 ms；这不是端到端延迟测量或长期性能保证。
- 当前只有本地玩家，没有第二名真人，其他玩家青蓝色框未完成非零真人运行验证。
- 过期提示已实现，本轮未主动中断采集器进行失联场景验收。

当前客户端、服务器和无窗口计数采集器保持运行，方便继续观察。

## 附录：原始证据与来源

证据目录：`E:\Users\MYX\Desktop\apex\artifacts\r5flowstate-entities`。

| 文件 | 内容 |
| --- | --- |
| `verification-2026-11-15.json` | 哈希、测试范围、刷新策略与最终统计 |
| `running-identity.json` | 本轮实际进程路径与启动参数 |
| `final-client-census.json` | 客户端原始分类、句柄、世界坐标与相机样本 |
| `final-server-census.json` | 全图原始计数 |
| `final-server-status.json` | 人类玩家与 bot 连接状态 |
| `npc-close-boxes.png` | NPC 近距离方框 |
| `decoy-boxes.png` | 临时诱饵紫色框及 1 个诱饵计数 |
| `decoy-create.json` / `decoy-cleanup.json` | 创建和清理查询结果 |
| `final-entities-boxes.png` | 清理后最终画面 |
| `map-counts.jsonl` | 全图计数变化和采集状态 |

早期失败构建及首次坐标/相机诊断仍保留为原始记录；最终验收以 `final-*` 和 `verification-2026-11-15.json` 为准。

- [R5Flowstate SDK 固定源码](https://github.com/R5Flowstate/s21-r5sdk/tree/10077bbf020292fa0bab69d9a9a05fef125d6ba5)
- [项目本地运行说明](https://r5flowstate.org/getting-started/)
