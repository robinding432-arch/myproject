# 🌌 StellarSystem — 完整操作手册

> **从打开 UE5 到拥有一套完整程序化宇宙引擎的每一步**
> 版本 v4.0 | UE 5.3+ | C++ / Blueprint

---

## 目录

- [第一阶段：环境搭建](#第一阶段环境搭建)
- [第二阶段：导入代码](#第二阶段导入代码)
- [第三阶段：输入系统配置](#第三阶段输入系统配置)
- [第四阶段：资产注册表（美术覆盖层）](#第四阶段资产注册表)
- [第五阶段：材质创建](#第五阶段材质创建)
- [第六阶段：GameMode 与场景搭建](#第六阶段场景搭建)
- [第七阶段：运行与测试](#第七阶段运行与测试)
- [第八阶段：美术替换模型](#第八阶段美术替换模型)
- [第九阶段：存档与多人](#第九阶段存档与多人)
- [附录：文件结构总览](#附录文件结构总览)
- [附录：操作按键速查](#附录操作按键速查)
- [附录：命名规范速查表](#附录命名规范速查表)
- [附录：已知问题与解决方案](#附录已知问题)

---

## 第一阶段：环境搭建

### 1.1 安装 UE5

| 步骤 | 操作 |
|---|---|
| 1 | 下载安装 Epic Games Launcher |
| 2 | 左侧点 **Unreal Engine** → **Library** → **+ 安装引擎** |
| 3 | 选 **5.3** 或更高版本 → 安装（勾选 "Editor Symbols for Debugging" 可选） |
| 4 | 等待下载完成（约 20~40 GB） |

### 1.2 安装 Visual Studio 2022

| 步骤 | 操作 |
|---|---|
| 1 | 下载 VS 2022 Community（免费） |
| 2 | 安装时勾选工作负载：**使用 C++ 的游戏开发** |
| 3 | 右侧勾选：**Windows 10/11 SDK** + **.NET Framework 4.8** |
| 4 | 安装完成重启 |

> **替代方案**：JetBrains Rider（对 UE 支持也很好）

### 1.3 验证安装

1. 打开 Epic Games Launcher → **Unreal Engine** → **Launch**
2. 能看到 UE 编辑器正常启动 = 安装成功

---

## 第二阶段：导入代码

### 2.1 新建 C++ 空白项目

1. UE 编辑器 → **新建项目**
2. 选 **游戏（Games）** → **下一步**
3. 选 **空白（Blank）** → **下一步**
4. 关键设置：
   - ✅ **C++**（不是蓝图！）
   - ✅ **桌面/主机（Desktop / Console）**
   - ✅ **最高画质（Maximum Quality）**
   - ❌ **光线追踪** → 关（程序化星球不需要）
   - ❌ **Starter Content** → 不要（我们全程序化生成）
5. 项目名：`StellarSystem`
6. 选好保存路径 → **创建**

> UE 会自动编译、打开编辑器。第一次编译约 2~5 分钟。

### 2.2 关闭编辑器

**重要**：后续操作需要关闭编辑器，用 VS/Rider 操作代码。

### 2.3 解压代码包

把你拿到的 `StellarSystem_Complete.zip` 解压，得到：

```
StellarSystem_Complete/
├── StellarSystem.uproject
├── README_MANUAL.md          ← 本文件
├── README.md                  ← 代码功能说明
├── InputSetup.md             ← 输入配置详解
├── KNOWN_ISSUES.md          ← 已知问题
├── VERSION.txt
└── Source/
    └── StellarSystem/
        ├── StellarSystem.Build.cs
        ├── Public/
        │   ├── Core/           ← GameMode/Save/Galaxy/DataAssets
        │   ├── Planet/         ← ProceduralPlanet/OceanShader
        │   ├── Character/     ← MyCharacter/Customization/Vitals/States
        │   ├── Ship/          ← ShipPawn/ShipAI/ShipWeapons/ShipHUD
        │   ├── Station/       ← ProceduralStation
        │   ├── Starmap/       ← StarmapSystem
        │   ├── Space/         ← NebulaSystem/AsteroidBelt
        │   ├── AssetRegistry/  ← AssetRegistry
        │   ├── Inventory/     ← Inventory/AmmoAndConsumables
        │   ├── Shop/          ← ShopSystem
        │   └── Equipment/     ← ProceduralEquipment
        └── Private/           ← 对应的 .cpp 实现
```

### 2.4 覆盖 Source 目录

1. 打开你的项目文件夹：`Documents/Unreal Projects/StellarSystem/`
2. **删除** 现有的 `Source/` 文件夹
3. 把 `StellarSystem_Complete/Source/` **复制** 进去
4. 把 `StellarSystem.uproject` **覆盖** 到项目根目录（替换原来的）

### 2.5 重命名 .uproject（如果有问题）

如果 UE 提示项目名不匹配：
1. 右键 `.uproject` → **重命名** → 改成你的项目名
2. 同时修改 `Source/` 文件夹名为你的项目名

### 2.6 生成项目文件

右键 `.uproject` → **Generate Visual Studio project files**

等待完成（约 10~30 秒）。

### 2.7 编译

**方式 A：用 Visual Studio**
1. 双击 `.sln` 打开 VS
2. 顶部工具栏：配置选 **Development Editor**
3. 菜单 **生成 → 生成解决方案**（或 Ctrl+Shift+B）
4. 等待编译完成（首次约 5~15 分钟）
5. 看到 `========== 生成: 成功 X 个，失败 0 个 ==========` 即可

**方式 B：用命令行**
```bash
# Windows
"c:\Program Files\Epic Games\UE_5.3\Engine\Build\BatchFiles\Build.bat" StellarSystemEditor Win64 Development -Project="%CD%\StellarSystem.uproject"
```

### 2.8 验证编译

1. 双击 `.uproject` 打开编辑器
2. 菜单 **工具 → 打开 C++ 类** → 能看到所有类 = 成功

---

## 第三阶段：输入系统配置

### 3.1 启用插件

编辑器菜单：**编辑 → 插件**

搜索并勾选以下插件 → **重启编辑器**：

| 插件名 | 作用 |
|---|---|
| **Enhanced Input** | 新输入系统（必需） |
| **Procedural Mesh Component** | 运行时生成 Mesh（必需） |
| **Gameplay Tags** | 标签系统（资产过滤用） |
| **Niagara** | 粒子系统（星云/引擎尾焰） |

### 3.2 创建 Input Actions（18 个）

Content Browser 里操作：右键 → **输入（Input）** → **Input Action**

逐个创建以下 18 个，注意**值类型**要对：

| 序号 | 名称 | 值类型 | 用途 |
|---|---|---|---|
| 1 | `IA_Move` | **Axis2D** (Vector2D) | WASD 移动 |
| 2 | `IA_Look` | **Axis2D** (Vector2D) | 鼠标视角 |
| 3 | `IA_Jump` | **Bool** | 空格跳 |
| 4 | `IA_Interact` | **Bool** | E 键交互 |
| 5 | `IA_ToggleFlight` | **Bool** | F 键起飞/降落 |
| 6 | `IA_Warp` | **Bool** | G 键跃迁 |
| 7 | `IA_AutoWarp` | **Bool** | R 键 AI 自动跃迁 |
| 8 | `IA_Thrust` | **Axis1D** (Float) | Shift 推进 |
| 9 | `IA_Strafe` | **Axis1D** (Float) | A/D 平移 |
| 10 | `IA_Pitch` | **Axis1D** (Float) | 鼠标 Y |
| 11 | `IA_Yaw` | **Axis1D** (Float) | 鼠标 X |
| 12 | `IA_Roll` | **Axis1D** (Float) | Q/E 滚转 |
| 13 | `IA_Fire` | **Bool** | 左键开火 |
| 14 | `IA_LockOn` | **Bool** | 右键锁定 |
| 15 | `IA_Starmap` | **Bool** | T 键星图 |
| 16 | `IA_ExitShip` | **Bool** | F 键离船 |
| 17 | `IA_Sprint` | **Bool** | Shift 冲刺 |
| 18 | `IA_CycleConsumable` | **Axis1D** (Float) | 鼠标滚轮切消耗品 |

> **详细映射规则见 `InputSetup.md`**

### 3.3 创建 Input Mapping Context

1. Content Browser → 右键 → **输入** → **Input Mapping Context**
2. 命名 `IMC_Default`
3. 双击打开，按以下表格添加映射：

| Action | 键/轴 | 修饰 |
|---|---|---|
| IA_Move | W | Swizzle → Y 轴正 |
| IA_Move | S | Swizzle → Y 轴负 |
| IA_Move | D | Swizzle → X 轴正 |
| IA_Move | A | Swizzle → X 轴负 |
| IA_Look | Mouse XY | 无修饰 |
| IA_Jump | Space | — |
| IA_Interact | E | — |
| IA_ToggleFlight | F | — |
| IA_Warp | G | — |
| IA_AutoWarp | R | — |
| IA_Thrust | Left Shift (Axis1D) | — |
| IA_Roll | Q / E | — |
| IA_Fire | Left Mouse | — |
| IA_LockOn | Right Mouse | — |
| IA_Starmap | T | — |
| IA_ExitShip | F | — |
| IA_Sprint | Left Shift | — |
| IA_CycleConsumable | Mouse Wheel Up/Down | — |

### 3.4 在蓝图中指认 Input Actions

这一步最容易忘！

1. Content Browser → 找到 C++ 类 `MyCharacter` → 右键 → **创建蓝图子类** → 命名 `BP_MyCharacter`
2. 双击打开 → **Class Defaults**
3. Details 面板搜索 "Input"，逐个指认：

| 变量 | 值 |
|---|---|
| Default Mapping Context | `IMC_Default` |
| Move Action | `IA_Move` |
| Look Action | `IA_Look` |
| Jump Action | `IA_Jump` |
| Interact Action | `IA_Interact` |
| Toggle Flight Action | `IA_ToggleFlight` |
| Warp Action | `IA_Warp` |
| Sprint Action | `IA_Sprint` |
| Cycle Consumable Action | `IA_CycleConsumable` |

4. **编译 + 保存**

---

## 第四阶段：资产注册表

### 4.1 创建 DataAsset

1. Content Browser → 右键 → **Miscellaneous** → **Data Asset**
2. 父类选 **AssetRegistry** → 命名 `DA_AssetRegistry_Main`
3. 双击打开 → 暂时不填（美术来了再填）
4. 保存

### 4.2 创建目录结构

Content Browser 里建以下文件夹：

```
/Game/
├── Art/
│   ├── StaticMeshes/
│   ├── SkeletalMeshes/
│   ├── Materials/
│   └── Animations/
├── Data/
│   └── DA_AssetRegistry_Main.uasset
├── Maps/
│   └── (你的地图)
└── Blueprints/
    ├── BP_MyCharacter.uasset
    ├── BP_ShipPawn.uasset
    ├── BP_ProceduralPlanet.uasset
    ├── BP_ProceduralShip.uasset
    ├── BP_GameMode.uasset
    └── BP_StellarHUD.uasset
```

---

## 第五阶段：材质创建

### 5.1 海洋材质

1. Content Browser → 右键 → **Material** → 命名 `M_Ocean`
2. 双击打开材质编辑器：

**节点连接：**
```
[Time] → [Sine × Frequency] → [Panner UV] → [Normal Map]
                                        ↓
[Vertex Normal Z] → [Lerp(浅蓝, 深蓝, 深度差)] → [Emissive]
                                        ↓
[DepthFade] → [Lerp(白色泡沫, 蓝色, Fade)] → [Opacity]
```

**关键参数（右键 → New Parameter）：**

| 参数名 | 类型 | 默认值 |
|---|---|---|
| `BaseColor` | Vector3 | (0.02, 0.05, 0.2) |
| `DeepColor` | Vector3 | (0.0, 0.01, 0.08) |
| `FoamColor` | Vector3 | (1, 1, 1) |
| `WaveAmplitude` | Scalar | 500.0 |
| `WaveFrequency` | Scalar | 0.02 |
| `Time` | Scalar | 0.0 |
| `NormalStrength` | Scalar | 0.8 |
| `FoamThreshold` | Scalar | 0.15 |
| `Opacity` | Scalar | 0.85 |
| `FresnelPower` | Scalar | 3.0 |

3. 勾选 **Two Sided** + **Translucent**
4. 保存

### 5.2 植被材质（简单版）

1. 新建 Material → `M_Tree`
2. Base Color: (0.1, 0.3, 0.05) 绿色
3. 勾选 **Two Sided**
4. 保存

### 5.3 星空天空球材质

1. 新建 Material → `M_Starfield`
2. Domain = **Sky** | Shading Model = **Unlit**
3. 用 **SkyLight** 或 Cubemap 采样
4. 保存

---

## 第六阶段：场景搭建

### 6.1 创建地图

1. **文件 → 新建关卡** → 选 **空关卡（Empty Level）**
2. 保存为 `Maps/MainMap.umap`

### 6.2 放行星

1. 左侧 **放置（Place）** 面板 → 搜 `ProceduralPlanet` → 拖入场景
2. 选中行星 → Details 面板：
   - `Planet Radius` = **100000**（1km 小行星，测试用）
   - `Cube Face Resolution` = **64**
   - `Random Seed` = **42**（随便改，每颗星球不同）
   - `Amplitude` = **80000**
   - `Ocean Threshold` = **0.3**
3. **再拖一个行星** → 改 Seed = **99** → 位置 (2000000, 0, 0)

### 6.3 放飞船

1. 搜 `ProceduralShip` → 拖到行星旁边空中
2. Details：
   - `Ship Class` = **Explorer**
   - `Seed` = **12345**

### 6.4 放 Player Start

1. 搜 `Player Start` → 拖到**第一颗行星表面附近**
2. 旋转让它箭头朝上（背对行星中心）
3. 位置 Z ≈ 行星半径 + 200

### 6.5 放大气

1. 选中行星 → **Add Component** → 搜 `SkyAtmosphere`
2. 位置自动在行星中心
3. 添加 `DirectionalLight` → 勾选 `Atmosphere Sun Light`

### 6.6 放星云 + 小行星带（可选）

1. 拖入 `NebulaSystem` → 位置 (0, 0, 5000000)
2. 拖入 `AsteroidBelt` → 位置 (0,0,0) → 轨道半径 3000000

### 6.7 配置 GameMode

1. Content Browser → 找到 C++ 类 `StellarGameMode` → 右键 → **创建蓝图子类** → `BP_GameMode`
2. 双击打开 → Class Defaults：
   - `Default Pawn Class` → `BP_MyCharacter`
   - `Player Controller Class` → 你的 PlayerController（如有）
   - `HUD Class` → `BP_StellarHUD`（如有）
3. 保存

### 6.8 设置项目默认 GameMode

1. 菜单 **编辑 → 项目设置 → Maps & Modes**
2. `Default GameMode` → 选 `BP_GameMode`
3. `Editor Startup Map` 和 `Game Default Map` → 选 `MainMap`
4. 保存

---

## 第七阶段：运行与测试

### 7.1 编译 + 运行

1. 编辑器点 **编译（Compile）** 按钮（或 Ctrl+Alt+F11）
2. 看到 `Successfully compiled` 后点 **播放（Play）**
3. 等待 5~15 秒（程序化生成需要时间）

### 7.2 你应该看到

| 阶段 | 画面 |
|---|---|
| 加载完成 | 角色站在星球表面，脚下是彩色地形 |
| WASD | 角色在球面行走，脚始终贴地 |
| 鼠标 | 视角随鼠标旋转 |
| F | 角色起飞 → 进入轨道模式 |
| G | 跃迁到第二颗星球 |
| 靠近飞船按 E | 切换到飞船 Pawn |
| W/S + 鼠标 | 飞船推进 + 转向 |
| R | AI 自动选星跃迁 |
| T | 打开星图 |

### 7.3 性能检查

按 **~** 打开控制台，输入：

```
stat fps          → 看帧率（目标 >60fps）
stat unit         → 看 CPU/GPU 耗时
stat scenerendering → 看 Draw Call（目标 <200）
```

### 7.4 常见问题排查

| 症状 | 原因 | 解决 |
|---|---|---|
| 编译报错 `EnhancedInput.h not found` | Build.cs 没加模块 | 检查 Build.cs 第 11 行 |
| 按 WASD 没反应 | IMC 没指认 | 回看 3.4 步 |
| 角色掉出星球 | 行星碰撞没开 | `CreateMeshSection` 时 `bCreateCollision=true` |
| 地形全黑 | 顶点色没输出 | 检查 Biome 着色代码 |
| 海洋看不见 | 材质没设 Transparent | 材质 Blend Mode 改 Translucent |
| 帧率极低（<10fps） | 植被没用 ISM | 确认 FoliageISMs 存在 |
| 跃迁后卡住 | 目标星球没放场景里 | 确保至少 2 颗行星 |
| 存档失败 | SaveGame 类没注册 | 确认 SaveSystem.h 在 Build.cs 里 |

---

## 第八阶段：美术替换模型

> 这是给后期接手的美术/TA 看的。程序员不需要管。

### 方式 A：自动发现（最简单，推荐）

```
1. 按命名规范导出模型：
   /Game/Art/StaticMeshes/Ship_Hull_Fighter/Ship_Hull_Fighter_High.fbx

2. UE 导入 FBX → 自动转 .uasset

3. 运行游戏 → 自动替换 ✅
```

### 方式 B：DataAsset 精确控制

```
1. 打开 /Game/Data/DA_AssetRegistry_Main.uasset
2. OverrideRules → + 添加元素
3. LogicalName = "Ship_Hull_Fighter"
4. OverrideMesh = 拖入你的模型
5. Priority = 100
6. 保存 → 运行 ✅
```

### 回退到程序化

```
删文件 或 取消 bEnabled → 自动 fallback 到程序化生成
```

### 完整命名速查

| 模块 | LogicalName |
|---|---|
| 飞船船体 | `Ship_Hull_{Fighter/Freighter/Explorer/Capital}` |
| 飞船机翼 | `Ship_Wing_{ShipClass}` |
| 角色头部 | `Character_Head_{Male/Female}_{Heroic/Villainous/Cute/...}` |
| 护甲 | `Armor_{Light/Medium/Heavy/Powered/Stealth/Hazard}_{Helmet/Chest/...}_{Common/Rare/Epic/...}` |
| 武器 | `Weapon_{Pistol/Rifle/SMG/...}_{Rarity}` |
| 植被 | `Foliage_{Forest/Grassland/Desert/...}_{Variant}` |

详见 `InputSetup.md` 和代码注释中的命名规范。

---

## 第九阶段：存档与多人

### 9.1 存档

- 自动存档：每 5 分钟自动保存（可在 `BP_GameMode` 里改间隔）
- 手动存档：调用 `USaveManager::QuickSave()`
- 多槽位：支持 10 个存档槽（0~9）

### 9.2 多人

- 服务端权威：所有关键操作走 `Server RPC`
- Seed 同步：所有客户端收到相同 Seed → 本地确定性生成**完全相同**的星球
- 位置同步：角色/飞船位置 10Hz 同步，误差超 5m 才发包

---

## 附录：文件结构总览

```
StellarSystem_Complete/
├── StellarSystem.uproject
├── README_MANUAL.md          ← 本文件（操作手册）
├── README.md                  ← 代码功能说明
├── InputSetup.md             ← 输入配置详解
├── KNOWN_ISSUES.md          ← 已知问题
├── VERSION.txt
└── Source/
    └── StellarSystem/
        ├── StellarSystem.Build.cs
        ├── Public/
        │   ├── Core/
        │   │   ├── StellarGameMode.h     ← 游戏总控
        │   │   ├── SaveSystem.h         ← 存档系统
        │   │   ├── GalaxyGenerator.h    ← 星系生成
        │   │   ├── StellarDataAssets.h  ← 共享枚举/基类
        │   │   └── CharacterStates.h    ← 状态机
        │   ├── Planet/
        │   │   ├── ProceduralPlanet.h   ← 行星生成（fBm+Biome+LOD）
        │   │   └── OceanShader.h        ← 海洋参数+材质驱动
        │   ├── Character/
        │   │   ├── MyCharacter.h        ← 玩家角色（球面重力）
        │   │   ├── CharacterCustomization.h ← AI 捏脸
        │   │   ├── VitalsSystem.h       ← 维生系统
        │   │   └── CurrencyComponent.h  ← 货币管理
        │   ├── Ship/
        │   │   ├── ShipPawn.h           ← 飞船 Pawn（6DOF）
        │   │   ├── ProceduralShip.h      ← 程序化飞船
        │   │   ├── ShipAIController.h   ← AI 自主跃迁
        │   │   ├── ShipWeapons.h        ← 武器系统
        │   │   ├── ShipComponents.h     ← 组件装配
        │   │   └── ShipHUD.h            ← HUD 显示
        │   ├── Station/
        │   │   └── ProceduralStation.h  ← 空间站生成
        │   ├── Starmap/
        │   │   └── StarmapSystem.h     ← 星图/扫描/锁定
        │   ├── Space/
        │   │   ├── NebulaSystem.h       ← 星云系统
        │   │   └── AsteroidBelt.h       ← 小行星带
        │   ├── AssetRegistry/
        │   │   └── AssetRegistry.h      ← 美术覆盖层
        │   ├── Inventory/
        │   │   ├── InventoryComponent.h  ← 总背包
        │   │   └── AmmoAndConsumables.h ← 弹药+消耗品
        │   ├── Shop/
        │   │   └── ShopSystem.h         ← 商城系统
        │   └── Equipment/
        │       └── ProceduralEquipment.h  ← 护甲+武器生成
        └── Private/  ← 对应的 .cpp 实现
```

---

## 附录：操作按键速查

| 场景 | 按键 | 效果 |
|---|---|---|
| **地面行走** | WASD + 鼠标 | 球面重力移动 |
| 跳跃 | 空格 | 消耗能量 |
| 冲刺 | Shift(按住) | 3× 耗氧/能量 |
| 起飞→轨道 | F | 切飞行模式 |
| 轨道飞行 | WASD + 鼠标 | 6DOF 平移/旋转 |
| 跃迁(最近星) | G | 三段式跃迁 |
| 自动跃迁(AI选) | R | AI 选下一个目标 |
| 登船 | E(靠近飞船) | Possess 飞船 |
| 离船 | F(飞船内) | 回到星球表面 |
| 开火 | 左键(需锁定) | 消耗弹药 |
| 锁定目标 | 右键 | 雷达锁定 |
| 使用消耗品 | 1~9 | 快捷栏 |
| 打开星图 | T | 扫描/标记/航线 |
| 打开商城 | B | 购买/出售 |
| 打开背包 | Tab | 弹药/消耗品管理 |

---

## 附录：命名规范速查表

| 模块 | LogicalName 格式 | 示例 |
|---|---|---|
| 飞船船体 | `Ship_Hull_{Class}` | `Ship_Hull_Fighter` |
| 飞船机翼 | `Ship_Wing_{Class}` | `Ship_Wing_Explorer` |
| 飞船引擎 | `Ship_Engine_{Class}_{N}` | `Ship_Engine_Capital_0` |
| 角色头部 | `Character_Head_{Gender}_{Style}` | `Character_Head_Male_Heroic` |
| 角色身体 | `Character_Body_{Build}` | `Character_Body_Athletic` |
| 护甲 | `Armor_{Type}_{Slot}_{Rarity}` | `Armor_Heavy_Chest_Rare` |
| 武器 | `Weapon_{Class}_{Rarity}` | `Weapon_LaserRifle_Epic` |
| 植被 | `Foliage_{Biome}_{Variant}` | `Foliage_Forest_Oak_02` |
| 行星地表 | `Planet_Surface_{Biome}` | `Planet_Surface_Desert` |
| 空间站模块 | `Station_{Type}_{Module}` | `Station_Ring_Habitat` |

---

## 附录：已知问题

| # | 问题 | 解决方案 |
|---|---|---|
| 1 | 捏脸 Mesh 与 SkeletalMesh 骨骼绑定需手动适配 | 建议用 MetaHuman 或自制骨骼 |
| 2 | 商城服务端校验需在 GameMode 实现完整货币扣减 | 在 `BP_GameMode` 里实现 `ValidatePurchase` |
| 3 | 护甲/武器 Mesh 为简化几何 | 配合材质模板 + 美术替换 |
| 4 | 维生 Radiation/Toxin 累积无上限 | 配合剧情设计或加硬上限 |
| 5 | 弹药弹道为简化物理 | 未考虑科里奥利力（星球自转影响） |
| 6 | 消耗品 Buff 系统需外部 Timer | 已留接口 `ActiveBuffs` |
| 7 | 飞船组件热平衡为简化模型 | 未模拟温度梯度 |
| 8 | AI 捏脸变异可能产生极端面孔 | 加合法性检查 |
| 9 | 商城折扣刷新未持久化 | 重启后重置 |
| 10 | 弹药堆叠上限未做背包重量限制 | 在 `InventoryComponent` 里加重量检查 |
| 11 | LOD mesh 重建未异步 | 需在 `UpdateLOD()` 里加 AsyncTask |
| 12 | 植被 ISM 未做重量级流式 | 大场景需按区域分块 |

---

## 版本信息

```
Version: 4.0.0
Date: 2026-08-17
Engine: Unreal Engine 5.3+
Modules: 12 (Core/Planet/Character/Ship/Station/Starmap/Space/AssetRegistry/Inventory/Shop/Equipment/HUD)
Classes: 50+
Lines of Code: ~12000+
Status: Compilable (requires UE5.3+ setup)
```

## License

MIT — 随便用，出事了别找我。

---

**恭喜！你现在已经拥有一套完整的程序化宇宙引擎。**
**从零到星际，全程可控、可扩展、可替换。**
