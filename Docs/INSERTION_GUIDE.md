# 📍 INSERTION GUIDE — 每个文件的精确插入路径

> 本指南告诉你**每一个文件**放在项目的**哪个位置**。按顺序操作即可。

---

## 1. 项目根目录

| 文件 | 路径 | 说明 |
|---|---|---|
| `StellarSystem.uproject` | `/StellarSystem_Final/StellarSystem.uproject` | 项目文件，双击打开 UE |
| `README.md` | `/StellarSystem_Final/README.md` | 项目总览 |
| `VERSION.txt` | `/StellarSystem_Final/VERSION.txt` | 版本信息 |
| `Makefile` | `/StellarSystem_Final/Makefile` | 快捷命令 |
| `Docs/` | `/StellarSystem_Final/Docs/` | 所有文档目录 |

---

## 2. 源码根

所有 C++ 源码在 `Source/StellarSystem/` 下：

```
Source/StellarSystem/
├── StellarSystem.Build.cs     ← 模块依赖配置
│
├── Public/                    ← 头文件（.h）
│   ├── Core/                  ← 核心系统
│   ├── Online/               ← 网络/账号/Steam
│   ├── Planet/               ← 行星/建筑/海洋/LOD
│   ├── Character/            ← 角色/捏脸/货币/维生
│   ├── Equipment/            ← 护甲/武器数据
│   ├── Inventory/            ← 背包/弹药/消耗品
│   ├── Ship/                 ← 飞船/武器/组件
│   ├── Shop/                 ← 商城
│   ├── Combat/               ← PvP 战斗
│   ├── AI/                   ← 任务/NPC/对话
│   ├── Space/                ← 星云/小行星/天气
│   ├── UI/                   ← 启动画面/主菜单/暂停菜单
│   └── Audio/                ← 音效管理
│
└── Private/                  ← 实现文件（.cpp）
    └── （镜像 Public 结构）
```

---

## 3. 文件清单 × 插入路径

### 3.1 Build.cs

| 文件 | 完整路径 |
|---|---|
| `StellarSystem.Build.cs` | `Source/StellarSystem/StellarSystem.Build.cs` |

---

### 3.2 Core 模块

| 头文件 (.h) | 完整路径 |
|---|---|
| `StellarGameMode.h` | `Source/StellarSystem/Public/Core/StellarGameMode.h` |
| `SaveSystem.h` | `Source/StellarSystem/Public/Core/SaveSystem.h` |
| `AssetRegistry.h` | `Source/StellarSystem/Public/Core/AssetRegistry.h` |
| `GalaxyGenerator.h` | `Source/StellarSystem/Public/Core/GalaxyGenerator.h` |

| 实现文件 (.cpp) | 完整路径 |
|---|---|
| `StellarGameMode.cpp` | `Source/StellarSystem/Private/Core/StellarGameMode.cpp` |
| `SaveSystem.cpp` | `Source/StellarSystem/Private/Core/SaveSystem.cpp` |
| `AssetRegistry.cpp` | `Source/StellarSystem/Private/Core/AssetRegistry.cpp` |

> `GalaxyGenerator.cpp` 在 v5.0 中实现，v6.0 新增恒星+8行星逻辑需补充。

---

### 3.3 Online 模块（**本次新增**）

| 头文件 (.h) | 完整路径 |
|---|---|
| `SteamOnlineSubsystem.h` | `Source/StellarSystem/Public/Online/SteamOnlineSubsystem.h` |
| `AccountSystem.h` | `Source/StellarSystem/Public/Online/AccountSystem.h` |
| `SteamAchievements.h` | `Source/StellarSystem/Public/Online/SteamAchievements.h` |

| 实现文件 (.cpp) | 完整路径 |
|---|---|
| `SteamOnlineSubsystem.cpp` | `Source/StellarSystem/Private/Online/SteamOnlineSubsystem.cpp` |
| `AccountSystem.cpp` | `Source/StellarSystem/Private/Online/AccountSystem.cpp` |
| `SteamAchievements.cpp` | `Source/StellarSystem/Private/Online/SteamAchievements.cpp` |

---

### 3.4 Planet 模块

| 头文件 (.h) | 完整路径 |
|---|---|
| `ProceduralPlanet.h` | `Source/StellarSystem/Public/Planet/ProceduralPlanet.h` |
| `OceanShader.h` | `Source/StellarSystem/Public/Planet/OceanShader.h` |
| `ProceduralBuildings.h` | `Source/StellarSystem/Public/Planet/ProceduralBuildings.h` |
| `PlanetLOD.h` | `Source/StellarSystem/Public/Planet/PlanetLOD.h` |

| 实现文件 (.cpp) | 完整路径 |
|---|---|
| `ProceduralPlanet.cpp` | `Source/StellarSystem/Private/Planet/ProceduralPlanet.cpp` |
| `OceanShader.cpp` | `Source/StellarSystem/Private/Planet/OceanShader.cpp` |
| `ProceduralBuildings.cpp` | `Source/StellarSystem/Private/Planet/ProceduralBuildings.cpp` |
| `PlanetLOD.cpp` | `Source/StellarSystem/Private/Planet/PlanetLOD.cpp` |

---

### 3.5 Character 模块

| 头文件 (.h) | 完整路径 |
|---|---|
| `CharacterCustomization.h` | `Source/StellarSystem/Public/Character/CharacterCustomization.h` |
| `CurrencyComponent.h` | `Source/StellarSystem/Public/Character/CurrencyComponent.h` |
| `VitalsSystem.h` | `Source/StellarSystem/Public/Character/VitalsSystem.h` |

| 实现文件 (.cpp) | 完整路径 |
|---|---|
| `CharacterCustomization.cpp` | `Source/StellarSystem/Private/Character/CharacterCustomization.cpp` |
| `CurrencyComponent.cpp` | `Source/StellarSystem/Private/Character/CurrencyComponent.cpp` |
| `VitalsSystem.cpp` | `Source/StellarSystem/Private/Character/VitalsSystem.cpp` |

---

### 3.6 Equipment + Inventory 模块

| 头文件 (.h) | 完整路径 |
|---|---|
| `ProceduralEquipment.h` | `Source/StellarSystem/Public/Equipment/ProceduralEquipment.h` |
| `AmmoAndConsumables.h` | `Source/StellarSystem/Public/Inventory/AmmoAndConsumables.h` |

| 实现文件 (.cpp) | 完整路径 |
|---|---|
| `ProceduralEquipment.cpp` | `Source/StellarSystem/Private/Equipment/ProceduralEquipment.cpp` |
| `AmmoAndConsumables.cpp` | `Source/StellarSystem/Private/Inventory/AmmoAndConsumables.cpp` |

---

### 3.7 Ship 模块

| 头文件 (.h) | 完整路径 |
|---|---|
| `ShipPawn.h` | `Source/StellarSystem/Public/Ship/ShipPawn.h` |
| `ShipWeapons.h` | `Source/StellarSystem/Public/Ship/ShipWeapons.h` |
| `ShipComponents.h` | `Source/StellarSystem/Public/Ship/ShipComponents.h` |

| 实现文件 (.cpp) | 完整路径 |
|---|---|
| `ShipPawn.cpp` | `Source/StellarSystem/Private/Ship/ShipPawn.cpp` |
| `ShipWeapons.cpp` | `Source/StellarSystem/Private/Ship/ShipWeapons.cpp` |
| `ShipComponents.cpp` | `Source/StellarSystem/Private/Ship/ShipComponents.cpp` |

---

### 3.8 Shop 模块

| 头文件 (.h) | 完整路径 |
|---|---|
| `ShopSystem.h` | `Source/StellarSystem/Public/Shop/ShopSystem.h` |

| 实现文件 (.cpp) | 完整路径 |
|---|---|
| `ShopSystem.cpp` | `Source/StellarSystem/Private/Shop/ShopSystem.cpp` |

---

### 3.9 Combat 模块（**本次新增**）

| 头文件 (.h) | 完整路径 |
|---|---|
| `PvPSystem.h` | `Source/StellarSystem/Public/Combat/PvPSystem.h` |

| 实现文件 (.cpp) | 完整路径 |
|---|---|
| `PvPSystem.cpp` | `Source/StellarSystem/Private/Combat/PvPSystem.cpp` |

---

### 3.10 AI 模块（**本次新增**）

| 头文件 (.h) | 完整路径 |
|---|---|
| `QuestSystem.h` | `Source/StellarSystem/Public/AI/QuestSystem.h` |

| 实现文件 (.cpp) | 完整路径 |
|---|---|
| `QuestSystem.cpp` | `Source/StellarSystem/Private/AI/QuestSystem.cpp` |

---

### 3.11 Space 模块

| 头文件 (.h) | 完整路径 |
|---|---|
| `NebulaSystem.h` | `Source/StellarSystem/Public/Space/NebulaSystem.h` |
| `AsteroidBelt.h` | `Source/StellarSystem/Public/Space/AsteroidBelt.h` |
| `SpaceWeather.h` | `Source/StellarSystem/Public/Space/SpaceWeather.h` |

| 实现文件 (.cpp) | 完整路径 |
|---|---|
| `NebulaSystem.cpp` | `Source/StellarSystem/Private/Space/NebulaSystem.cpp` |
| `AsteroidBelt.cpp` | `Source/StellarSystem/Private/Space/AsteroidBelt.cpp` |
| `SpaceWeather.cpp` | `Source/StellarSystem/Private/Space/SpaceWeather.cpp` |

---

### 3.12 UI 模块（**本次新增/更新**）

| 头文件 (.h) | 完整路径 |
|---|---|
| `SplashScreen.h` | `Source/StellarSystem/Public/UI/SplashScreen.h` |
| `MainMenuWidget.h` | `Source/StellarSystem/Public/UI/MainMenuWidget.h` |
| `PauseMenuWidget.h` | `Source/StellarSystem/Public/UI/PauseMenuWidget.h` |

| 实现文件 (.cpp) | 完整路径 |
|---|---|
| `SplashScreen.cpp` | `Source/StellarSystem/Private/UI/SplashScreen.cpp` |
| `MainMenuWidget.cpp` | `Source/StellarSystem/Private/UI/MainMenuWidget.cpp` |
| `PauseMenuWidget.cpp` | `Source/StellarSystem/Private/UI/PauseMenuWidget.cpp` |

---

### 3.13 Audio 模块（**本次新增**）

| 头文件 (.h) | 完整路径 |
|---|---|
| `AudioManager.h` | `Source/StellarSystem/Public/Audio/AudioManager.h` |

| 实现文件 (.cpp) | 完整路径 |
|---|---|
| `AudioManager.cpp` | `Source/StellarSystem/Private/Audio/AudioManager.cpp` |

---

## 4. 快速部署步骤

```
STEP 1: 解压 StellarSystem_Final.zip 到目标目录
STEP 2: 打开 StellarSystem.uproject → 等待编译
STEP 3: 右键 .uproject → Generate Visual Studio project files
STEP 4: VS 中 Build Solution（Ctrl+Shift+B）
STEP 5: 按 Docs/InputSetup.md 创建 Input Actions
STEP 6: 按 README.md 配置 DefaultEngine.ini（Steam）
STEP 7: Play！
```

---

## 5. 新增文件摘要（v5.0 → v6.0）

| 新增文件 | 功能 |
|---|---|
| `Public/Online/SteamOnlineSubsystem.h/cpp` | Steam 会话管理（创建/搜索/加入） |
| `Public/Online/AccountSystem.h/cpp` | 账号注册/登录（SHA-256 + JSON） |
| `Public/Online/SteamAchievements.h/cpp` | 30 个成就 + Steam 云存档 |
| `Public/Combat/PvPSystem.h/cpp` | PvP 战斗 + 爆炸/死亡/复活点 |
| `Public/AI/QuestSystem.h/cpp` | AI 任务生成 + NPC + 对话 |
| `Public/Planet/ProceduralBuildings.h/cpp` | 9 种程序化建筑 |
| `Public/UI/SplashScreen.h/cpp` | 启动画面（Logo+声明） |
| `Public/UI/MainMenuWidget.h/cpp` | 主菜单（登录/注册/设置） |
| `Public/UI/PauseMenuWidget.h/cpp` | 暂停菜单（存档/任务/死亡） |
| `Public/Audio/AudioManager.h/cpp` | 音效系统（6 类 + 程序化） |

**共新增 20 个文件，更新 3 个文件（Build.cs/README/VERSION）**

---

## 6. 编译顺序提示

如果编译报错，按以下顺序检查：

1. **Build.cs** 是否正确包含所有模块 → 右键 .uproject 重新生成
2. **Online 模块** 依赖 Steamworks → 确认 Steam SDK 路径
3. **Audio 模块** 依赖 AudioMixer → UE5 自带
4. **UI 模块** 依赖 UMG/Slate → UE5 自带
5. **Combat 模块** 依赖 Niagara → 确认插件启用
6. **AI 模块** 依赖 GameplayTags → UE5 自带

---

**按本指南操作，所有文件放入正确路径后，编译即可运行完整多人在线宇宙。**
