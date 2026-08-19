# WeGame 平台适配指南

> StellarSystem v6.9 — WeGame Rail SDK 集成完整说明

## 1. 概述

StellarSystem v6.9 新增了完整的 **WeGame 平台适配层**，通过对标现有的 Steam 集成架构，实现了：

| 功能模块 | 头文件 | 源文件 |
|---|---|---|
| 核心集成（成就/云存档/统计/Rich Presence/排行榜/内购/敏感词） | `WeGameIntegration.h` | `WeGameIntegration.cpp` |
| 在线子系统（会话/好友/服务器浏览/P2P数据） | `WeGameOnlineSubsystem.h` | `WeGameOnlineSubsystem.cpp` |
| 账号桥接（WeGame登录 → 本地账号） | `WeGameAccountBridge.h` | `WeGameAccountBridge.cpp` |
| 平台启动器（SDK生命周期/客户端监控/强退） | `WeGameLauncher.h` | `WeGameLauncher.cpp` |
| 防沉迷 UI（提示/强退/宵禁） | `WeGameAntiAddictionWidget.h` | `WeGameAntiAddictionWidget.cpp` |
| WeGame 专用编译目标 | `StellarSystemWeGame.Target.cs` | — |

## 2. 目录结构

```
Source/StellarSystem/
├── StellarSystem.Build.cs              ← 已添加 WITH_WEGAME 条件编译
├── StellarSystemWeGame.Target.cs      ⭐ 新增：WeGame Shipping 专用目标
├── Public/
│   ├── WeGame/                       ⭐ 新增目录
│   │   ├── WeGameIntegration.h
│   │   ├── WeGameOnlineSubsystem.h
│   │   ├── WeGameAccountBridge.h
│   │   ├── WeGameLauncher.h
│   │   └── WeGameAntiAddictionWidget.h
│   ├── Online/
│   │   └── AccountSystem.h           ← 已添加 LoginWithWeGame()
│   └── ... (其他模块不变)
└── Private/
    ├── WeGame/                       ⭐ 新增目录
    │   ├── WeGameIntegration.cpp
    │   ├── WeGameOnlineSubsystem.cpp
    │   ├── WeGameAccountBridge.cpp
    │   ├── WeGameLauncher.cpp
    │   └── WeGameAntiAddictionWidget.cpp
    └── ... (其他模块不变)
```

## 3. 编译配置

### 3.1 模块依赖（Build.cs）

WeGame 模块通过 `WITH_WEGAME` 宏控制：

```csharp
// 仅 Win64 平台启用
if (Target.Platform == UnrealTargetPlatform.Win64)
{
    PublicDefinitions.Add("WITH_WEGAME=1");

    // Rail SDK 头文件
    string RailSDKPath = Path.Combine(ModuleDirectory, "../../../ThirdParty/RailSDK");
    if (Directory.Exists(RailSDKPath))
    {
        PublicIncludePaths.Add(Path.Combine(RailSDKPath, "include"));
        PublicAdditionalLibraries.Add(Path.Combine(RailSDKPath, "lib/win/Release_64/rail_api64.lib"));
        PublicDelayLoadDLLs.Add("rail_api64.dll");
        RuntimeDependencies.Add(Path.Combine(RailSDKPath, "lib/win/Release_64/rail_api64.dll"));
    }
}
else
{
    PublicDefinitions.Add("WITH_WEGAME=0");
}
```

### 3.2 编译目标

| 目标文件 | 用途 | 配置 |
|---|---|---|
| `StellarSystemEditor.Target.cs` | 编辑器 | Debug/Development |
| `StellarSystemClient.Target.cs` | 通用客户端 | Development/Shipping |
| `StellarSystemServer.Target.cs` | 专用服务器 | Headless Shipping |
| **`StellarSystemWeGame.Target.cs`** ⭐ | **WeGame 发布版** | **Shipping + LTCG** |

### 3.3 编译命令

```bash
# Windows - WeGame 发布版
RunUAT.bat BuildGame -targetplatform=Win64 -configuration=Shipping -target=StellarSystemWeGame

# 输出: Binaries/Win64/StellarSystemWeGame.exe
```

## 4. Rail SDK 安装

### 4.1 下载

1. 访问 https://developer.wegame.com
2. 注册开发者账号并登录
3. 进入「SDK 及工具包下载」页面
4. 下载最新版 Rail SDK + WeGame 开发者版本客户端

### 4.2 目录布局

将 SDK 解压到项目 `ThirdParty/RailSDK/` 目录：

```
ThirdParty/RailSDK/
├── include/
│   └── rail/
│       └── sdk/
│           ├── rail_api.h          ← 主头文件
│           ├── rail_anti_addiction_define.h
│           ├── rail_game_id.h
│           └── ... (其他头文件)
└── lib/
    └── win/
        ├── Release_32/
        │   ├── rail_api.dll
        │   └── rail_api.lib
        └── Release_64/
            ├── rail_api64.dll
            └── rail_api64.lib
```

### 4.3 在 WeGame 开发者平台注册应用

1. 登录 https://developer.wegame.com
2. 创建新游戏，获得 **GameID（AppID）**
3. 在「技术配置」中：
   - 配置游戏启动项（指向 `StellarSystemWeGame.exe`）
   - 启用云存档（选择「接入 RailSDK」方式）
   - 配置成就列表（API 名称需与代码中 `AchievementToString()` 一致）
   - 配置排行榜
   - 配置内购商品
   - 启用防沉迷（2020年5月1日后申请的 GameID 默认开启）
   - 启用敏感词过滤

## 5. 运行时流程

### 5.1 游戏启动顺序

```
WeGame 客户端启动
  ↓
WeGame 加载 StellarSystemWeGame.exe（传入命令行参数）
  ↓
UE5 引擎初始化
  ↓
UWeGameLauncher::InitializeForWeGame(AppID, Version)
  ├─ DetectWeGameEnvironment()     ← 检测命令行/进程/环境变量
  ├─ CheckWeGameClientProcess()    ← 确认客户端在线
  └─ UWeGameIntegration::InitializeSDK()
      ├─ RailNeedRestartAppForCheckingEnvironment()
      ├─ RailInitialize()
      └─ 获取本地玩家 RailID/DisplayName
  ↓
UWeGameAccountBridge::AttemptWeGameLogin()
  ├─ AcquireSessionTicket()
  ├─ HTTPS POST 到游戏服务器验证
  └─ 创建/绑定本地账号
  ↓
进入主菜单 → 游戏开始
```

### 5.2 每帧驱动

```
UWeGameLauncher::Tick(DeltaTime)
  ├─ 每 100ms 调用 RailFireEvents()    ← 驱动 SDK 事件循环
  └─ 检查 WeGame 客户端进程
      └─ 客户端退出 → ForceExitGame()
```

### 5.3 防沉迷流程

```
WeGame 客户端检测到防沉迷触发
  ↓
Rail SDK 触发 kRailEventAntiAddictionCustomizeAntiAddictionActions
  ↓
UWeGameIntegration::ProcessAntiAddictionEvent()
  ├─ Action=ShowTips  → 显示提示对话框（60秒）
  ├─ Action=ForceExit → 保存进度 → ReturnToMainMenu → Exit
  └─ Action=Curfew    → 禁止游戏 → 显示宵禁通知
```

## 6. 与 Steam 版本的差异

| 特性 | Steam 版 | WeGame 版 |
|---|---|---|
| 平台 SDK | Steamworks | Rail SDK |
| 编译宏 | `WITH_STEAMWORKS=1` | `WITH_WEGAME=1` |
| 登录方式 | SteamID | RailID + SessionTicket |
| 成就系统 | Steam Achievements | WeGame Achievements |
| 云存档 | Steam Cloud | WeGame Cloud (IRailFile) |
| 好友系统 | Steam Friends | Rail Friends |
| P2P 网络 | Steam Networking | Rail Network |
| 防沉迷 | 无 | **必须接入** |
| 敏感词过滤 | 无 | **必须接入** |
| 实名认证 | 无 | **必须接入** |
| 内购 | Steam MicroTxn | WeGame InGameStore |
| 编译目标 | StellarSystemClient | **StellarSystemWeGame** |

## 7. 代码中的 Stub 模式

当 `WITH_WEGAME=0` 时（如在 Linux 服务器、IDE 调试、未安装 SDK 时），所有 WeGame 接口自动退化为 **Stub 模式**：

- `InitializeSDK()` → 创建虚拟身份 `WeGame_Stub_0001`
- `WriteCloudFile()` → 写入 `Saved/WeGameCloud/` 本地目录
- `UnlockAchievement()` → 仅打印日志
- `SendDataToPlayer()` → 仅打印日志
- `FilterDirtyWords()` → 简单关键词替换

这意味着**你可以在没有 WeGame 客户端的环境中进行开发调试**，只需在蓝图中绑定对应的委托即可。

## 8. 上架 WeGame 检查清单

### 8.1 必须完成

- [ ] 注册 WeGame 开发者账号
- [ ] 创建游戏并获取 GameID
- [ ] 签署线上合同
- [ ] 下载并集成 Rail SDK
- [ ] 在开发者版本客户端中测试启动
- [ ] 接入防沉迷系统（监听 `kRailEventAntiAddictionCustomizeAntiAddictionActions`）
- [ ] 接入敏感词过滤（所有文本输入处）
- [ ] 启用实名认证
- [ ] 游戏强退逻辑（客户端退出 → 游戏退出）
- [ ] 内购接入（如适用）
- [ ] 云存档接入
- [ ] 成就配置与测试
- [ ] 获取游戏版号（GAPP 批准）
- [ ] 在 QA 分支创建 version 包并测试
- [ ] 在 Release 分支创建 version 包
- [ ] 配置商店信息与价格
- [ ] 提交审核

### 8.2 推荐完成

- [ ] 排行榜配置
- [ ] 好友邀请功能
- [ ] Rich Presence 状态展示
- [ ] 防沉迷调试（开发者客户端设置中调整时长）
- [ ] 日志收集工具测试（`log_collection_tool.exe`）

## 9. 常见问题

### Q: 编辑器里能调试 WeGame 功能吗？

**能。** 在 `UWeGameLauncher::DetectWeGameEnvironment()` 中：
- 如果有 WeGame 开发者客户端在运行 → 正常初始化 SDK
- 如果没有 → 进入 Stub 模式，所有接口打印日志但不报错
- 也可以在命令行加 `-wegame` 强制启用检测

### Q: 服务器需要集成 WeGame SDK 吗？

**需要，但方式不同。** 专用服务器使用 `rail_dedicated_server_launcher_app.exe` 启动器：
- 服务器调用 `RailNeedRestartAppForCheckingEnvironment()` 时**必须传入命令行参数**
- 服务器通过 `IRailGameServer` 接口验证客户端 SessionTicket
- 参考 Rail SDK 文档中的「Dedicated Server」章节

### Q: 如何测试防沉迷？

1. 打开 WeGame 开发者版本客户端
2. 进入「系统设置 → Debug」页面
3. 取消勾选「跳过当前账户的防沉迷检查」
4. 配置测试 GameID
5. 点击「触发防沉迷」按钮
6. 游戏会立即收到 `kRailEventAntiAddictionCustomizeAntiAddictionActions` 回调

### Q: 可以同时支持 Steam 和 WeGame 吗？

**可以。** 架构设计上两者完全独立：
- `WITH_STEAMWORKS` 和 `WITH_WEGAME` 是独立宏
- 两套子系统并存，通过 `UGameInstanceSubsystem` 管理
- 运行时通过 `UWeGameLauncher::IsWeGameEnvironment()` 判断当前平台
- 建议分别出包：`StellarSystemClient.exe`（Steam）和 `StellarSystemWeGame.exe`（WeGame）

## 10. 版本信息

| 项目 | 值 |
|---|---|
| 版本 | 6.9.0 |
| 新增文件 | 8 个 (.h/.cpp) + 1 个 .cs + 1 个 .md |
| 新增代码行数 | ~1,900 行 |
| 总项目文件数 | 248 个 |
| 总 C++ 代码行数 | ~44,000 行 |
