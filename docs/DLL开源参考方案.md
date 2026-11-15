# DLL 开源参考方案

核实日期：2026-11-14。当前选择的主参考是 [R5Flowstate/s21-r5sdk](https://github.com/R5Flowstate/s21-r5sdk)，固定 HEAD 为 `10077bbf020292fa0bab69d9a9a05fef125d6ba5`。

## 为什么选它

它的 README 明确描述了两种产品：`client` 输出 `game/client.dll`，由客户端加载；`server` 输出 `game/server.dll`。它还保留 CMake、Visual Studio、启动配置和调试入口，适合学习 DLL 的初始化、客户端接口、渲染回调、日志和退出生命周期。[仓库说明](https://github.com/R5Flowstate/s21-r5sdk)

它不是当前正式 Apex 的适配方案。README 明确支持的是 S21 客户端构建 `R5pc_r5-211_J12_CL6933961_2024_06_14_13_58`，以及 S3 dedicated server；本机目标是 `r5apex_dx12.exe` build 26445400，所以不能把这个仓库生成的 DLL 直接放进当前游戏。

## 可复用的学习结构

```text
client product
  → 生成 game/client.dll
  → 宿主加载 DLL
  → 初始化客户端接口和模块
  → 注册渲染/帧回调
  → 读取受控状态
  → 绘制文本或 ImGui
  → 收到停止信号后解除回调并卸载
```

重点看这些概念：

- `client` 与 `server` 的构建分支和预处理宏；
- `loader.dll` 根据宿主可执行文件选择匹配产品；
- CMake 生成工程和 `game` 输出目录；
- `-devsdk`、日志和崩溃记录的调试路径；
- DLL 初始化线程、回调注册、资源释放和卸载顺序。

## 第二个参考：渲染层

[AuDowty/overlay-engine](https://github.com/AuDowty/overlay-engine) 适合单独学习 DX11 进程内 Overlay：创建 dummy swapchain，获取 `Present`，首次真实 `Present` 初始化 ImGui，随后每帧绘制，最后 detach。它是通用 DX11 项目，不能直接证明 Apex DX12 兼容。

因此，学习顺序应是：

1. 在自己的小型 DirectX Demo 中验证 `overlay-engine` 的渲染生命周期。
2. 在 R5Reloaded/S21 SDK 环境中验证 `client.dll` 的初始化、状态读取和绘制。
3. 最后把读取逻辑替换成一个自有 `PlayerState { ammo, weaponName }`，确认 DLL 能显示 `Ammo: 19` 再变成 `Ammo: 18`。

## 不作为当前主参考的项目

- [Mauler125/r5sdk](https://github.com/Mauler125/r5sdk)：同样是旧版 R5/S3 SDK，README 只支持更早的 S3 构建。
- [Apex-Mizu-Base](https://github.com/NaiJii/Apex-Mizu-Base)：内部 DLL/Hook 示例，适配版本和安全边界不清楚，不能当作当前客户端的可运行方案。
- [apex-legends-internal](https://github.com/trash-code/apex-legends-internal)：旧内部 ESP 示例，README 没有当前 build 证据。
- [apex-external-base](https://github.com/cromekk/apex-external-base)：外部 Overlay 和内存缓存架构，不是进程内 DLL。

## 当前本机结论

本机当前 Apex 的正常用户态加载仍被权限削减阻断；Blackbone 完整用户态库在自有宿主中可以加载 DLL，但在 Apex 中返回 `0xC0000022 / Win32 5`。因此本参考方案用于学习可控宿主的 DLL 和渲染结构，不继续尝试绕过当前客户端的访问控制。

现有外部采样窗口仍可作为数据验证工具；进程内显示实验应转到自有 Demo 或 R5Reloaded 环境。这样可以分别验证 DLL 生命周期、渲染、状态更新和卸载，而不会把旧版 SDK 的成功误认为当前 Apex 的注入成功。
