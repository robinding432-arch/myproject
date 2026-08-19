# StellarSystem v6.7 — Client / Server 分离架构

## 概述

从 v6.7 开始，项目分为 **三个编译目标（Target）**，分别产出不同的可执行文件：

| Target 文件 | 产出 | 用途 | 包含模块 |
|---|---|---|---|
| `StellarSystemEditor.Target.cs` | `UnrealEditor.exe` | 开发期编辑器 | **全部**（含编辑器专用） |
| `StellarSystemClient.Target.cs` | `StellarSystemClient.exe` | 玩家客户端 | 渲染/音频/UI/输入/物理/AI/程序化 Mesh |
| `StellarSystemServer.Target.cs` | `StellarSystemServer.exe` | 专用游戏服务器 | 网络/存档/反作弊/经济/物理查询 |

---

## 架构图

```
                    ┌─────────────────────────────┐
                    │   StellarSystemServer.exe   │
                    │     (Dedicated Server)      │
                    │                             │
                    │  AStellarDedicatedServer   │
                    │  ┌───────────────────────┐  │
                    │  │ SaveManager            │  │
                    │  │ AntiCheatManager(Srv)  │  │
                    │  │ NetworkOptimizer(Srv)  │  │
                    │  │ ObjectPoolManager(Srv) │  │
                    │  │ SolarSystem(逻辑)      │  │
                    │  │ Planets/Ships(状态)    │  │
                    │  └───────────────────────┘  │
                    │                             │
                    │  NO: Render/Audio/UI/Niagara│
                    └──────────┬──────────────────┘
                               │
                      Replication (30Hz)
                     Relevancy Culling + Delta
                               │
              ┌────────────────┼────────────────┐
              ▼                ▼                ▼
    ┌──────────────┐  ┌──────────────┐  ┌──────────────┐
    │  Client #1   │  │  Client #2   │  │  Client #N   │
    │  StellarSys- │  │  StellarSys- │  │  StellarSys- │
    │  temClient   │  │  temClient   │  │  temClient   │
    │              │  │              │  │              │
    │ AStellarCli- │  │ AStellarCli- │  │ AStellarCli- │
    │ entGameMode  │  │ entGameMode  │  │ entGameMode  │
    │              │  │              │  │              │
    │ ✓ Render    │  │ ✓ Render    │  │ ✓ Render    │
    │ ✓ Audio     │  │ ✓ Audio     │  │ ✓ Audio     │
    │ ✓ UI/UMG    │  │ ✓ UI/UMG    │  │ ✓ UI/UMG    │
    │ ✓ Input     │  │ ✓ Input     │  │ ✓ Input     │
    │ ✓ Procedural│  │ ✓ Procedural│  │ ✓ Procedural│
    │   Mesh      │  │   Mesh      │  │   Mesh      │
    │ ✓ Niagara   │  │ ✓ Niagara   │  │ ✓ Niagara   │
    │ ✓ Physics   │  │ ✓ Physics   │  │ ✓ Physics   │
    │ ✓ AI        │  │ ✓ AI        │  │ ✓ AI        │
    │ ✓ AudioMgr  │  │ ✓ AudioMgr  │  │ ✓ AudioMgr  │
    │ ✓ PerfMgr   │  │ ✓ PerfMgr   │  │ ✓ PerfMgr   │
    │ ✓ PoolMgr   │  │ ✓ PoolMgr   │  │ ✓ PoolMgr   │
    │ ✗ Save      │  │ ✗ Save      │  │ ✗ Save      │
    │ ✗ AntiCheat │  │ ✗ AntiCheat │  │ ✗ AntiCheat │
    │  (local)    │  │  (local)    │  │  (local)    │
    └──────────────┘  └──────────────┘  └──────────────┘
```

---

## 目录结构

```
StellarSystem_v6.7/
├── StellarSystem.uproject          ← 项目文件（三个 Target 共享）
│
├── Source/
│   ├── StellarSystemEditor.Target.cs   ← 编辑器 Target
│   ├── StellarSystemClient.Target.cs  ← 客户端 Target ⭐
│   ├── StellarSystemServer.Target.cs  ← 服务器 Target ⭐
│   │
│   └── StellarSystem/
│       ├── StellarSystem.Build.cs     ← 条件编译（按 Target 裁剪模块）
│       │
│       ├── Public/
│       │   ├── Client/                     ⭐ 客户端专用
│       │   │   └── StellarClientGameMode.h
│       │   ├── Server/                     ⭐ 服务器专用
│       │   │   └── StellarDedicatedServer.h
│       │   ├── Core/                       ← 共享（两端都有）
│       │   │   ├── StellarGameMode.h       ← 单人/Listen Server 用
│       │   │   ├── SaveSystem.h
│       │   │   ├── PerformanceManager.h
│       │   │   ├── ObjectPool.h
│       │   │   ├── NetworkOptimizer.h
│       │   │   ├── StartupOptimizer.h
│       │   │   ├── AssetRegistry.h
│       │   │   └── ...
│       │   ├── Planet/  Ship/  Combat/  Equipment/
│       │   ├── Inventory/  Economy/  Factions/
│       │   ├── AI/  ModSupport/  Online/
│       │   ├── Steam/  Space/  Starmap/
│       │   ├── Station/  Audio/  UI/
│       │   └── Character/  (所有模块两端共享)
│       │
│       └── Private/  (镜像结构)
│
├── Config/
│   ├── DefaultEngine.ini
│   ├── DefaultGame.ini
│   ├── DefaultInput.ini
│   └── Server.ini                   ⭐ 服务器专用配置
│
├── Server/
│   └── Build/
│       ├── BuildServer.sh             ⭐ Linux 编译脚本
│       ├── RunServer.sh               ⭐ Linux 启动脚本
│       └── RunServer.bat             ⭐ Windows 启动脚本
│
├── Client/
│   └── Build/
│       └── PackageClient.bat         ⭐ Windows 客户端打包脚本
│
├── Docs/
│   ├── CLIENT_SERVER_SPLIT.md       ⭐ 本文档
│   ├── SERVER_DEPLOYMENT.md         ⭐ 服务器部署指南
│   ├── SERVER_ADMIN_GUIDE.md       ⭐ 服务器管理命令
│   └── ... (其他文档)
│
└── README.md
```

---

## 编译指南

### 编辑器（开发用）

```bash
# Windows
右键 StellarSystem.uproject → Generate Visual Studio project files
打开 StellarSystem.sln → 编译 Editor 配置

# Linux
~/UnrealEngine/Engine/Build/BatchFiles/Linux/GenerateProjectFiles.sh \
    -project="$(pwd)/StellarSystem.uproject" -game
make StellarSystemEditor
```

### 客户端

```bash
# Windows (CMD)
cd Source
dotnet ..\..\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll ^
    StellarSystemClient Win64 Shipping ^
    -project="..\StellarSystem.uproject" -progress

# Linux
~/UnrealEngine/Engine/Build/BatchFiles/Linux/Build.sh \
    StellarSystemClient Linux Shipping \
    -project="$(pwd)/StellarSystem.uproject" -progress
```

产出：`Binaries/Win64/StellarSystemClient.exe` 或 `Binaries/Linux/StellarSystemClient`

### 服务器

```bash
# Windows (CMD)
cd Source
dotnet ..\..\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll ^
    StellarSystemServer Win64 Shipping ^
    -project="..\StellarSystem.uproject" -progress -nullrhi

# Linux
~/UnrealEngine/Engine/Build/BatchFiles/Linux/Build.sh \
    StellarSystemServer Linux Shipping \
    -project="$(pwd)/StellarSystem.uproject" -progress -nullrhi
```

产出：`Binaries/Win64/StellarSystemServer.exe` 或 `Binaries/Linux/StellarSystemServer`

> **`-nullrhi` 标志**：告诉 UE 不初始化渲染硬件接口 → 服务器无头运行，内存节省 ~500MB

---

## 模块裁剪对照表

| 模块 | Editor | Client | Server | 说明 |
|---|:---:|:---:|:---:|---|
| Core / Engine | ✅ | ✅ | ✅ | 基础 |
| Networking / OnlineSubsystem | ✅ | ✅ | ✅ | 网络基础 |
| InputCore / EnhancedInput | ✅ | ✅ | ❌ | 服务器无输入 |
| Slate / UMG | ✅ | ✅ | ❌ | 服务器无 UI |
| AudioMixer / SoundModulation | ✅ | ✅ | ❌ | 服务器无音频 |
| Renderer / RenderCore / RHI | ✅ | ✅ | ❌ | 服务器无渲染 |
| Niagara / NiagaraCore | ✅ | ✅ | ❌ | 服务器无粒子 |
| CinematicCamera / LevelSequence | ✅ | ✅ | ❌ | 服务器无过场 |
| ProceduralMeshComponent | ✅ | ✅ | ❌ | 服务器不生成 Mesh |
| GeometryFramework / MeshDescription | ✅ | ✅ | ❌ | 同上 |
| PhysicsCore / Chaos | ✅ | ✅ | 轻量 | 服务器只做碰撞查询 |
| AIModule / NavigationSystem | ✅ | ✅ | ❌ | AI 表现只在客户端 |
| AnimGraphRuntime | ✅ | ✅ | ❌ | 无骨骼动画 |
| ShaderCore / ComputeFramework | ✅ | ✅ | ❌ | 无 GPU 渲染 |
| **ReplicationGraph / Iris** | ✅ | ❌ | ✅ | **服务器专用高级网络** |
| AssetRegistry / StreamCore | ✅ | ✅ | ✅ | 资源引用两端都要 |
| Json / JsonUtilities | ✅ | ✅ | ✅ | 存档/配置 |
| Profiler / Stats / TraceLog | ✅ | ✅ | ✅ | 性能监控 |
| OnlineSubsystemSteam | ✅ | ✅ | ✅ | 服务器验证 Ticket |
| UnrealEd / Kismet (编辑器专用) | ✅ | ❌ | ❌ | 编辑器模块 |

---

## 网络架构

### 复制策略

| Actor | 服务端权威 | 客户端预测 | 说明 |
|---|:---:|:---:|---|
| `AStellarDedicatedServer` | ✅ | ❌ | 服务器状态（玩家数/FPS/带宽） |
| `ASolarSystem` | ✅ | 渲染 | 位置/轨道参数服务端算，客户端只渲染 |
| `AProceduralPlanet` | ✅ | 生成 | 种子同步，客户端本地生成 Mesh |
| `AShipPawn` | ✅ | ✅ | 位置/速度服务端校验，客户端预测 |
| `APlayerCharacter` | ✅ | ✅ | 同上 |
| `AAntiCheatManager` | ✅ | 轻量 | 服务端仲裁，客户端只做本地检测 |
| `APerformanceManager` | ❌ | ✅ | 纯客户端 |
| `UAudioManager` | ❌ | ✅ | 纯客户端 |
| `APoolManager` | 部分 | 部分 | 服务端池化子弹/伤害，客户端池化粒子/UI |

### RPC 方向

```
Client → Server (Server, Reliable)
  ├── PlayerMove (位置上报 + 预测)
  ├── FireWeapon (开火请求)
  ├── MiningAction (采矿请求)
  ├── TradeRequest (交易请求)
  ├── ChatMessage (聊天)
  └── Heartbeat (心跳)

Server → Client (Client, Reliable)
  ├── OnKicked (踢出通知)
  ├── OnBanned (封禁通知)
  ├── SyncGameState (状态同步)
  └── PlaySound (服务端触发的音效)

Server → All Clients (NetMulticast, Reliable)
  ├── BroadcastMessage (公告)
  ├── OnPlayerJoined (玩家加入通知)
  └── OnServerShutdown (服务器关闭预告)
```

---

## 服务器部署

### 最低配置

| 资源 | 最低 | 推荐（32 人） | 推荐（64 人） |
|---|---|---|---|
| CPU | 4 核 | 8 核 | 16 核 |
| 内存 | 4 GB | 8 GB | 16 GB |
| 磁盘 | 2 GB | 5 GB | 10 GB |
| 带宽 | 10 Mbps | 50 Mbps | 100 Mbps |
| OS | Ubuntu 20.04 / Win Server 2019 | 同左 | 同左 |

### Linux 快速部署

```bash
# 1. 上传打包好的服务器
scp -r Server/Build/Packaged/ user@server:/opt/stellarsystem/

# 2. SSH 登录
ssh user@server
cd /opt/stellarsystem

# 3. 安装依赖
sudo apt update
sudo apt install -y libvulkan1 libxcb-keysyms1 libxcb-icccm4 \
    libxcb-image0 libxcb-randr0 libxcb-render-util0 \
    libxcb-shape0 libxcb-sync1 libxcb-xfixes0 libxcb-xkb1 \
    libxkbcommon0 libxkbcommon-x11-0 libfontconfig1

# 4. 配置
nano Config/Server.ini
# 修改 ServerName / MaxPlayers / Port 等

# 5. 启动（后台运行）
chmod +x RunServer.sh
nohup ./RunServer.sh -log > server.log 2>&1 &

# 6. 查看日志
tail -f server.log

# 7. 停止
kill $(pgrep StellarSystemServer)
```

### Windows 服务器部署

```cmd
REM 1. 复制 Packaged/ 到服务器
REM 2. 编辑 Config\Server.ini
REM 3. 运行
RunServer.bat -port=7777 -maxplayers=32 -log
```

### Docker 部署（推荐生产环境）

```dockerfile
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libvulkan1 libxcb-keysyms1 ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY Packaged/ /opt/stellarsystem/
WORKDIR /opt/stellarsystem

RUN chmod +x StellarSystemServer RunServer.sh

EXPOSE 7777/udp 7777/tcp 27015/udp

CMD ["./RunServer.sh", "-log", "-port=7777", "-maxplayers=32"]
```

---

## 服务器管理命令

在服务器控制台（或 RCON）输入：

| 命令 | 说明 |
|---|---|
| `SetMaxPlayers 64` | 调整最大玩家数 |
| `ListPlayers` | 列出所有在线玩家 |
| `ServerSay Hello!` | 发送公告 |
| `KickPlayer <Name>` | 踢出玩家 |
| `BanPlayer <Name>` | 封禁玩家 |
| `BanPlayer <Name> true` | 硬件封禁 |
| `SaveNow` | 立即存档 |
| `ShutdownServer 30` | 30 秒后关闭 |
| `SetNetworkTickRate 60` | 调整网络频率 |
| `SetRelevancyDistance 100000` | 调整相关性距离 |

---

## 客户端连接流程

```
1. 启动 StellarSystemClient.exe
2. 显示启动画面 → 法律声明
3. 账号登录/注册
4. 主菜单 → 多人游戏
5. 服务器浏览器（从 Steam 获取列表）
   ├── 显示：服务器名/IP/玩家数/Ping/版本
   └── 双击连接
6. 版本校验（不匹配 → 提示更新）
7. 连接服务器 → 认证
8. 接收初始状态（星系/星球/玩家位置）
9. 进入游戏世界
10. 自动重连（断线 ≤5 次自动重试）
```

---

## 常见问题

| 问题 | 原因 | 解决 |
|---|---|---|
| 客户端连不上服务器 | 防火墙/端口未开放 | 开放 UDP 7777 + UDP 27015 |
| 服务器启动报错 "nullrhi not supported" | 显卡驱动缺失 | 服务器不需要显卡，检查是否误装了客户端 |
| 玩家 ping 高 | 服务器地理位置远 | 部署多区域服务器 |
| 服务器内存持续增长 | 内存泄漏 / GC 未触发 | 降低 MaxPlayers / 增加 AutoSave 频率 |
| 客户端版本不匹配 | 客户端未更新 | 从 Steam 自动更新 |
| 服务器 CPU 100% | 网络 TickRate 过高 / 玩家过多 | 降低 NetworkTickRate / 增加 RelevancyDistance |
| 玩家卡在连接中 | 认证超时 | 检查 Steam 网络 / 增加超时时间 |

---

## 版本号约定

| 文件 | 修改位置 |
|---|---|
| `VERSION.txt` | 项目根目录 |
| `Server.ini` → `RequiredClientVersion` | 服务器强制客户端版本 |
| `StellarDedicatedServer.cpp` → 启动日志 | 服务器版本显示 |
| `StellarClientGameMode.cpp` → 启动日志 | 客户端版本显示 |

升级流程：
1. 修改 `VERSION.txt` → `6.8.0`
2. 修改 `Server.ini` → `RequiredClientVersion="6.8.0"`
3. 编译新的 Client + Server
4. 部署新服务器（旧客户端自动被拒绝）
5. 通过 Steam 推送客户端更新

---

## 下一步

- [ ] 集成 Steam GameServer API（认证/心跳/统计）
- [ ] 实现 RCON 远程管理协议
- [ ] 添加服务器浏览器 Web API（REST + WebSocket）
- [ ] 容器化部署（Kubernetes + Helm Chart）
- [ ] 自动扩缩容（根据玩家数动态调整服务器实例）
- [ ] 跨区域服务器同步（跨服传送/统一经济）
