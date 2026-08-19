# WeGame SDK 安装与配置指南

> 快速上手 Rail SDK 集成

## 1. 前置条件

| 条件 | 说明 |
|---|---|
| 操作系统 | Windows 10/11（Rail SDK 仅支持 Windows） |
| UE 版本 | Unreal Engine 5.3+ |
| 编译器 | Visual Studio 2022（C++17） |
| 账号 | WeGame 开发者账号（免费注册） |
| 资质 | 中国大陆运营需公司资质 + 游戏版号 |

## 2. 注册开发者

1. 访问 https://developer.wegame.com
2. 点击「注册开发者」
3. 填写公司信息 + 营业执照
4. 等待审核（通常 1-3 个工作日）

## 3. 创建游戏

1. 登录开发者后台
2. 点击「新建游戏」
3. 填写：
   - 游戏名称（中/英）
   - 游戏类型
   - 引擎：Unreal Engine
   - 预计上线时间
4. 提交审核 → 获得 **GameID**

## 4. 下载 SDK

1. 进入「SDK 及工具包下载」页面
2. 下载：
   - **Rail SDK**（C++ 头文件 + DLL）
   - **WeGame 开发者版本客户端**（用于调试）
3. 安装 WeGame 开发者客户端

## 5. 放置 SDK 文件

将 SDK 解压到项目目录：

```
StellarSystem_v6.9/
└── ThirdParty/
    └── RailSDK/
        ├── include/
        │   └── rail/
        │       └── sdk/
        │           ├── rail_api.h
        │           ├── rail_anti_addiction_define.h
        │           ├── rail_game_id.h
        │           └── ... (约 50 个 .h 文件)
        └── lib/
            └── win/
                ├── Release_32/
                │   ├── rail_api.dll
                │   └── rail_api.lib
                └── Release_64/
                    ├── rail_api64.dll
                    └── rail_api64.lib
```

## 6. 配置 Build.cs

`StellarSystem.Build.cs` 已自动检测 `ThirdParty/RailSDK/` 目录：

```csharp
// 自动检测逻辑（已内置）
string RailSDKPath = Path.Combine(ModuleDirectory, "../../../ThirdParty/RailSDK");
if (Directory.Exists(RailSDKPath))
{
    PublicIncludePaths.Add(Path.Combine(RailSDKPath, "include"));
    PublicAdditionalLibraries.Add(Path.Combine(RailSDKPath, "lib/win/Release_64/rail_api64.lib"));
    PublicDelayLoadDLLs.Add("rail_api64.dll");
    RuntimeDependencies.Add(Path.Combine(RailSDKPath, "lib/win/Release_64/rail_api64.dll"));
}
else
{
    PublicDefinitions.Add("WITH_WEGAME=0"); // Stub 模式
}
```

## 7. 在开发者客户端测试

### 7.1 添加游戏到 WeGame

1. 打开 WeGame 开发者版本客户端
2. 拖拽 `StellarSystem_v6.9/Binaries/Win64/StellarSystemWeGame.exe` 到客户端左侧
3. 客户端识别后显示游戏图标

### 7.2 配置启动项

在开发者后台 → 技术配置 → 游戏启动项：
- 启动程序：`StellarSystemWeGame.exe`
- 命令行参数：`-wegame -rail_app_id=<你的GameID>`
- 工作目录：`Binaries/Win64/`

### 7.3 运行测试

1. 点击「开始游戏」按钮
2. 观察输出日志（`Saved/Logs/StellarSystem.log`）
3. 确认看到：`LogWeGame: Rail SDK initialized. AppID=xxxxx`

## 8. 调试模式（不用 WeGame 客户端启动）

在 IDE（Visual Studio / Rider）中直接运行：

1. 右键项目 → 属性 → 调试
2. 命令行参数添加：`-wegame -rail_app_id=0`
3. 以**管理员权限**运行 IDE
4. 启动调试（F5）

此时 `UWeGameLauncher::DetectWeGameEnvironment()` 会检测到 `-wegame` 参数，进入调试模式。

## 9. 验证清单

启动游戏后检查日志中是否包含：

```
LogWeGameLauncher: WeGame environment detected: YES
LogWeGame: Rail SDK initialized. AppID=xxxxx
LogWeGame: AsyncAcquireSessionTicket initiated
LogWeGame: Stub session ticket acquired    ← Stub 模式会显示这个
LogWeGameAuth: WeGame login SUCCESS: RailID=xxxxx Username=WeGame_xxxxx
```

## 10. 常见问题

### SDK 初始化返回 `kErrorUnauthorized`

→ GameID 未授权或开发者账号无权限。检查开发者后台的权限设置。

### `DllNotFoundException: rail_api64.dll`

→ DLL 未复制到输出目录。检查 Build.cs 中的 `RuntimeDependencies` 路径是否正确。

### `RailNeedRestartAppForCheckingEnvironment` 返回失败

→ 游戏未从 WeGame 客户端启动。在 IDE 调试时这是正常的，SDK 会进入降级模式。

### 防沉迷回调未触发

→ 需要在开发者客户端设置中取消勾选「跳过防沉迷检查」，并点击「触发防沉迷」按钮手动测试。

### 云存档不生效

→ 需要在开发者后台「技术配置 → 云存档」中启用并配置路径前缀。

## 11. 文件清单

v6.9 WeGame 相关文件：

```
Source/StellarSystem/
├── StellarSystemWeGame.Target.cs       ← WeGame Shipping 编译目标
├── StellarSystem.Build.cs             ← 已添加 WITH_WEGAME 支持
├── Public/
│   ├── WeGame/
│   │   ├── WeGameIntegration.h        ← 核心 SDK 封装
│   │   ├── WeGameOnlineSubsystem.h   ← 会话/好友/网络
│   │   ├── WeGameAccountBridge.h     ← 账号桥接
│   │   ├── WeGameLauncher.h          ← 平台启动器
│   │   └── WeGameAntiAddictionWidget.h ← 防沉迷 UI
│   └── Online/
│       └── AccountSystem.h            ← 已添加 LoginWithWeGame()
└── Private/
    └── WeGame/
        ├── WeGameIntegration.cpp
        ├── WeGameOnlineSubsystem.cpp
        ├── WeGameAccountBridge.cpp
        ├── WeGameLauncher.cpp
        └── WeGameAntiAddictionWidget.cpp

Docs/
├── WEGAME_INTEGRATION.md              ← 完整集成文档
└── WEGAME_SDK_SETUP.md              ← 本文档
```
