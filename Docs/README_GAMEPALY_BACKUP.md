# StellarSystem — UE5 程序化宇宙引擎 完整项目

> **版本**：v4.0 | **引擎**：UE 5.3+ | **语言**：C++ / Blueprint
> **包含模块**：行星生成 → 角色控制 → 飞船系统 → AI 跃迁 → 空间站 → 星云 → 星图 → HUD → 维生 → 商城 → 捏脸 → 护甲武器 → 资产覆盖层

---

## 快速开始（5 步）

1. **新建 UE5 C++ 空白项目**，项目名 `StellarSystem`
2. **解压本包的 `Source/`** → 覆盖到你的项目 `Source/StellarSystem/` 目录
3. **解压本包的 `Config/`** → 覆盖到你的项目 `Config/` 目录
4. **启用插件**：Enhanced Input + Procedural Mesh Component + Gameplay Tags + Niagara
5. **编译** → 按 `Docs/操作手册.md` 逐步配置输入和场景 → Play

---

## 文件清单与插入位置

每个 `.h` / `.cpp` 文件头部都标注了：
- **路径**：相对于项目根目录的存放位置
- **作用**：这个文件负责什么
- **依赖**：需要哪些前置模块

---

## 项目目录结构

```
StellarSystem/
├── StellarSystem.uproject          ← 项目文件（已配置模块依赖）
├── Config/                         ← 引擎配置（已预设）
│   ├── DefaultEngine.ini
│   ├── DefaultGame.ini
│   └── DefaultInput.ini
├── Source/
│   └── StellarSystem/
│       ├── StellarSystem.Build.cs   ← 模块依赖定义
│       ├── Public/                  ← 所有 .h 头文件
│       │   ├── Core/               ← 核心框架
│       │   ├── Planet/             ← 行星系统
│       │   ├── Character/          ← 角色系统
│       │   ├── Ship/               ← 飞船系统
│       │   ├── Station/            ← 空间站
│       │   ├── Starmap/            ← 星图
│       │   ├── Space/              ← 太空环境
│       │   └── Economy/            ← 商城/货币
│       └── Private/                ← 所有 .cpp 实现
│           ├── Core/
│           ├── Planet/
│           ├── Character/
│           ├── Ship/
│           ├── Station/
│           ├── Starmap/
│           ├── Space/
│           └── Economy/
├── Content/                        ← 蓝图/材质/DataAsset（编辑器内创建）
│   ├── Blueprints/
│   ├── Materials/
│   ├── Data/
│   └── Art/                       ← 美术覆盖资源（后期放入）
└── Docs/                          ← 文档
    ├── 操作手册.md
    ├── 按键速查.md
    ├── 美术替换指南.md
    └── 命名规范.md
```

---

## 编译顺序（模块依赖链）

```
StellarSystem (Build.cs)
 │
 ├─ Core/       ← 无依赖，最先编译
 │   ├── StellarGameMode
 │   ├── StellarPlayerController
 │   ├── SaveSystem
 │   ├── GalaxyGenerator
 │   └── AssetRegistry
 │
 ├─ Planet/     ← 依赖 Core
 │   ├── ProceduralPlanet
 │   └── OceanShader
 │
 ├─ Character/  ← 依赖 Core + Planet
 │   ├── MyCharacter
 │   ├── CharacterCustomization
 │   ├── VitalsComponent
 │   └── InventoryComponent
 │
 ├─ Ship/       ← 依赖 Core + Character
 │   ├── ShipPawn
 │   ├── ShipAIController
 │   ├── ShipWeapons
 │   ├── ShipLoadout
 │   └── ShipHUD
 │
 ├─ Station/    ← 依赖 Core + Ship
 │   └── ProceduralStation
 │
 ├─ Starmap/    ← 依赖 Core
 │   └── StarmapSystem
 │
 ├─ Space/      ← 依赖 Core
 │   ├── NebulaSystem
 │   └── AsteroidBelt
 │
 └─ Economy/    ← 依赖 Core + Character
     ├── ShopComponent
     ├── CurrencyComponent
     └── ConsumableItem
```

---

## 各文件用途速查

| 文件 | 路径 | 用途 |
|---|---|---|
| StellarSystem.Build.cs | Source/StellarSystem/ | 模块依赖声明 |
| StellarGameMode.h/.cpp | Public/Private → Core/ | 游戏总控/星球注册表/存档接口 |
| StellarPlayerController.h/.cpp | Public/Private → Core/ | 输入模式切换/Pawn 管理 |
| SaveSystem.h/.cpp | Public/Private → Core/ | 自动存档/多槽位/JSON |
| GalaxyGenerator.h/.cpp | Public/Private → Core/ | 多恒星+行星系统生成 |
| AssetRegistry.h/.cpp | Public/Private → Core/ | 美术资源覆盖层 |
| ProceduralPlanet.h/.cpp | Public/Private → Planet/ | fBm 地形+8 Biome+LOD |
| OceanShader.h/.cpp | Public/Private → Planet/ | 海洋波浪+深度+泡沫 |
| MyCharacter.h/.cpp | Public/Private → Character/ | 球面重力角色 |
| CharacterCustomization.h/.cpp | Public/Private → Character/ | AI 捏脸 30+ 参数 |
| VitalsComponent.h/.cpp | Public/Private → Character/ | 8 项维生模拟 |
| InventoryComponent.h/.cpp | Public/Private → Character/ | 背包/装备/消耗品 |
| ShipPawn.h/.cpp | Public/Private → Ship/ | 6DOF 飞船驾驶 |
| ShipAIController.h/.cpp | Public/Private → Ship/ | AI 自主跃迁循环 |
| ShipWeapons.h/.cpp | Public/Private → Ship/ | 4 种武器+锁定+追踪 |
| ShipLoadout.h/.cpp | Public/Private → Ship/ | 组件装配+热平衡 |
| ShipHUD.h/.cpp | Public/Private → Ship/ | 雷达/速度/燃料 UI |
| ProceduralStation.h/.cpp | Public/Private → Station/ | 4 种空间站生成 |
| StarmapSystem.h/.cpp | Public/Private → Starmap/ | 扫描/锁定/航线 |
| NebulaSystem.h/.cpp | Public/Private → Space/ | 5 种星云+密度场 |
| AsteroidBelt.h/.cpp | Public/Private → Space/ | 轨道力学+ISM |
| ShopComponent.h/.cpp | Public/Private → Economy/ | 商城/折扣/校验 |
| CurrencyComponent.h/.cpp | Public/Private → Economy/ | 6 种货币 |
| ConsumableItem.h/.cpp | Public/Private → Economy/ | 20+ 种消耗品 |

---

## 完整操作手册

详见 `Docs/操作手册.md` — 从打开 UE5 到运行完整宇宙，每一步截图级指引。

---

## 按键总览

详见 `Docs/按键速查.md`

---

## 美术资源替换

详见 `Docs/美术替换指南.md` — 零代码替换 AI 生成模型。

---

## 版本信息

```
v4.0 (2026-08-18)
- 新增：AI 捏脸系统（30+ 参数 / 8 预设 / 程序化 Mesh）
- 新增：完整商城（6 货币 / 6 稀有度 / 服务端校验）
- 新增：AI 生成护甲（6 槽位 × 6 类型 × 程序化几何）
- 新增：AI 生成武器（10 种 × 5 火控 × 5 元素 × 词条）
- 新增：飞船组件系统（10 槽位 × 5 稀有度 × 热平衡）
- 新增：维生系统（8 项全模拟 / 太空服 / 辐射）
- 新增：弹药系统（15 种 / 弹道物理 / 特种弹药）
- 新增：消耗品系统（20+ 种 / Buff / 快捷栏）
- 新增：资产覆盖层（DataAsset 规则 + 自动发现 + 命名约定）

v3.0 (2026-08-17)
- 新增：程序化飞船生成（4 种船型）
- 新增：飞船驾驶（6DOF + 推进/滚转）
- 新增：飞船 AI（自主选星→跃迁→循环）
- 新增：武器系统（激光/导弹/等离子/轨道炮）
- 新增：空间站生成（4 种类型）
- 新增：星云系统（5 种类型）
- 新增：小行星带（轨道力学）
- 新增：星图系统（扫描/锁定/航线）
- 新增：飞船 HUD（雷达/速度/燃料）
- 新增：存档系统（自动 5min / 多槽位）

v2.0 (2026-08-16)
- 新增：海洋 Shader（波浪/深度/泡沫）
- 新增：地形 LOD 四叉树
- 新增：多人同步（Seed 复制）
- 新增：超光速跃迁（三段式）
- 新增：HUD 系统

v1.0 (2026-08-15)
- 基础：程序化行星（fBm + 8 Biome）
- 基础：球面重力角色（WASD + 鼠标）
- 基础：行星自转 + 跟随
- 基础：大气散射 + 天空球
- 基础：起飞→轨道→降落
- 基础：ISM 批量植被 + 流式加载
```
