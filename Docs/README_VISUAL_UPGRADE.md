# 视觉升级指南 — 美术资产覆盖流程

> 本文档指导美术/维护人员**用自建模型替换 AI 程序化生成的 3D 模型**。
> 核心原则：**零代码替换** —— 按命名规范放文件 → 自动生效 → 删文件自动回退。

## 一、架构：双层渲染

```
程序化生成（AI/噪声）  ← 默认兜底，永远可用
        ↕ 自动检测
美术资产覆盖（自建模型） ← 存在则优先使用
```

**优先级链**：
1. 查运行时缓存 → 2. 查 DataAsset 显式规则 → 3. 自动发现路径 → 4. 回退程序化

## 二、资产注册表（DataAsset）

### 创建步骤

1. Content Browser → 右键 → **Miscellaneous** → **Data Asset**
2. 父类选 `AssetRegistry` → 命名 `DA_AssetRegistry_Main`
3. 双击打开 → 编辑 `OverrideRules` 数组

### 单条规则字段

| 字段 | 说明 | 示例 |
|---|---|---|
| `LogicalName` | 程序化时的逻辑名 | `Ship_Hull_Fighter` |
| `Priority` | 数字越大越优先 | `100` |
| `OverrideMesh` | 静态网格覆盖 | 拖入 `.uasset` |
| `OverrideSkeletalMesh` | 骨骼网格覆盖 | 拖入 `.uasset` |
| `OverrideMaterial` | 材质覆盖 | 拖入 `.uasset` |
| `OverrideAnimBlueprint` | 动画蓝图覆盖 | 拖入 `.uasset` |
| `bEnabled` | 是否启用此规则 | `true` |
| `RequiredTags` | 标签过滤 | `Ship.Class.Fighter` |

### 示例：覆盖战斗机船体

```
LogicalName = "Ship_Hull_Fighter"
Priority    = 100
OverrideMesh = Fighter_Hull_High.uasset
bEnabled    = true
RequiredTags = Ship.Class.Fighter
```

## 三、自动发现（更简单）

美术只需按命名规范导出 FBX → UE 导入 → **自动生效**。

### 目录结构

```
/Game/
├── Art/
│   ├── StaticMeshes/
│   │   ├── Ship_Hull_Fighter/
│   │   │   ├── Ship_Hull_Fighter_High.uasset
│   │   │   ├── Ship_Hull_Fighter_Medium.uasset
│   │   │   └── Ship_Hull_Fighter_Low.uasset
│   │   ├── Ship_Wing_Explorer/
│   │   ├── Weapon_LaserRifle/
│   │   └── Armor_Heavy_Chest/
│   ├── SkeletalMeshes/
│   │   ├── Character_Head_Male_Heroic/
│   │   └── Character_Body_Athletic/
│   ├── Materials/
│   │   ├── M_Ship_Hull_Fighter.uasset
│   │   └── M_Armor_Heavy.uasset
│   └── Animations/
│       └── Character_Run_AnimBP.uasset
```

### 命名模板

```
{LogicalName}_{Quality}.uasset

Quality ∈ {High, Medium, Low}
```

系统按 Quality 从高到低尝试 → 自动 LOD 适配硬件。

## 四、LogicalName 速查表

### 飞船

| 模块 | LogicalName |
|---|---|
| 船体 | `Ship_Hull_{Fighter/Freighter/Explorer/Capital}` |
| 机翼 | `Ship_Wing_{ShipClass}` |
| 引擎 | `Ship_Engine_{ShipClass}_{Index}` |
| 驾驶舱 | `Ship_Cockpit_{ShipClass}` |

### 角色

| 模块 | LogicalName |
|---|---|
| 头部（男） | `Character_Head_Male_{Heroic/Villainous/Cute/Rugged/Elegant/Alien/Elder/Youthful}` |
| 头部（女） | `Character_Head_Female_{...}` |
| 身体 | `Character_Body_{Athletic/Slim/Heavy/Custom}` |
| 毛发 | `Character_Hair_{Style}_{Color}` |

### 护甲

| 槽位 | LogicalName |
|---|---|
| 头盔 | `Armor_{Light/Medium/Heavy/Powered/Stealth/Hazard}_Helmet_{Common/Uncommon/Rare/Epic/Legendary/Prototype}` |
| 胸甲 | `Armor_{Type}_Chest_{Rarity}` |
| 护腿 | `Armor_{Type}_Legs_{Rarity}` |
| 护臂 | `Armor_{Type}_Arms_{Rarity}` |
| 靴子 | `Armor_{Type}_Boots_{Rarity}` |
| 手套 | `Armor_{Type}_Gloves_{Rarity}` |

### 武器

| 类型 | LogicalName |
|---|---|
| 激光步枪 | `Weapon_LaserRifle_{Rarity}` |
| 等离子炮 | `Weapon_PlasmaCannon_{Rarity}` |
| 磁轨枪 | `Weapon_Railgun_{Rarity}` |
| 粒子束 | `Weapon_ParticleBeam_{Rarity}` |
| 导弹发射器 | `Weapon_MissileLauncher_{Rarity}` |
| 散弹枪 | `Weapon_Shotgun_{Rarity}` |
| 狙击枪 | `Weapon_SniperRifle_{Rarity}` |
| 手枪 | `Weapon_Pistol_{Rarity}` |
| 能量剑 | `Weapon_EnergySword_{Rarity}` |
| 地雷 | `Weapon_MineLayer_{Rarity}` |

### 植被 & 星球

| 模块 | LogicalName |
|---|---|
| 树木 | `Foliage_{Forest/Grassland/Tundra/Desert/Jungle}_{Variant}` |
| 草 | `Grass_{Biome}_{Variant}` |
| 岩石 | `Rock_{Biome}_{Size}` |
| 星球地表 | `Planet_Surface_{Biome}` |

## 五、材质覆盖

### 程序化材质参数

程序化生成的材质暴露这些参数（在材质实例中覆盖）：

| 参数名 | 类型 | 说明 |
|---|---|---|
| `BaseColor` | Vector3 | 基础色 |
| `Roughness` | Scalar | 粗糙度 |
| `Metallic` | Scalar | 金属度 |
| `NormalStrength` | Scalar | 法线强度 |
| `EmissiveColor` | Vector3 | 自发光色 |
| `EmissiveIntensity` | Scalar | 自发光强度 |

### 替换流程

1. 在 Content Browser 创建 `Material Instance`
2. 父材质选程序化材质（如 `M_Ship`）
3. 覆盖参数或整个材质
4. 在 `DA_AssetRegistry_Main` 中设置 `OverrideMaterial`

## 六、动画覆盖

### 角色动画

| 状态 | LogicalName |
|---|---|
| 待机 | `Anim_Idle_{Gender}_{Style}` |
| 行走 | `Anim_Walk_{Gender}_{Style}` |
| 奔跑 | `Anim_Run_{Gender}_{Style}` |
| 跳跃 | `Anim_Jump_{Gender}` |
| 降落 | `Anim_Land_{Gender}` |
| 飞行 | `Anim_Fly_{Gender}` |
| 驾驶 | `Anim_Drive_{Gender}` |

### 飞船动画

| 状态 | LogicalName |
|---|---|
| 待机 | `ShipAnim_Idle_{Class}` |
| 推进 | `ShipAnim_Thrust_{Class}` |
| 转向 | `ShipAnim_Turn_{Class}` |
| 跃迁 | `ShipAnim_Warp_{Class}` |

## 七、音频资产

### 音效分类

| 类别 | 命名规范 | 示例 |
|---|---|---|
| 引擎 | `SFX_Engine_{ShipClass}_{Throttle}` | `SFX_Engine_Fighter_Low.uasset` |
| 跃迁 | `SFX_Warp_{Phase}` | `SFX_Warp_Accelerate.uasset` |
| 武器 | `SFX_Weapon_{Type}_{Variant}` | `SFX_Weapon_Laser_Single.uasset` |
| 命中 | `SFX_Hit_{Material}` | `SFX_Hit_Metal.uasset` |
| 爆炸 | `SFX_Explosion_{Size}` | `SFX_Explosion_Large.uasset` |
| UI | `SFX_UI_{Action}` | `SFX_UI_Click.uasset` |
| 脚步 | `SFX_Footstep_{Surface}` | `SFX_Footstep_Metal.uasset` |
| 环境 | `SFX_Ambient_{Location}` | `SFX_Ambient_Space.uasset` |

### 指认到 AudioManager

1. 打开 `BP_GameMode` → 找到 `AudioMgr` 组件
2. 编辑 `SoundLibrary` 数组
3. 每项：`Category` 选类别 + `SoundAsset` 拖入 `.uasset`

## 八、验证流程（美术自检）

### 第一步：放文件

```
✅ FBX 导出按命名规范
✅ UE 导入成功（无错误）
✅ 文件路径在 /Game/Art/ 下
```

### 第二步：运行验证

```
✅ Play → 模型自动替换
✅ 检查 LOD 切换（走近/走远）
✅ 检查材质正确显示
✅ 检查动画播放
✅ 检查音效触发
```

### 第三步：回退测试

```
✅ 删除/重命名文件 → 自动回退程序化
✅ 取消 bEnabled → 立即回退
✅ 关闭 RequiredTags → 规则失效
```

## 九、常见坑

| 问题 | 原因 | 解决 |
|---|---|---|
| 模型不显示 | 路径不对 / 命名错 | 对照速查表 |
| 材质全黑 | 缺少 UV | 检查导入设置 |
| LOD 跳变 | 质量降级间隔太大 | 调整 `QualityFallbackOrder` |
| 动画不播 | AnimBP 未绑定 | 在 `DA_AssetRegistry` 设置 |
| 音效没声 | SoundAsset 未拖入 | 编辑 `SoundLibrary` |
| 模型倒置 | 导入旋转错误 | FBX 导出时 Y-up |
| 尺寸不对 | 单位不一致 | 确认 1U = 1cm |

## 十、进阶：远程资产包（DLC / 热更新）

```cpp
// 支持从 Web/PAK 动态下载资产
void UAssetRegistry::RegisterRemoteAssetPack(const FString& PakPath)
{
    // 挂载 Pak → 注册到 AssetRegistry → 自动可用
}
```

美术更新模型不用发新版游戏 → 推 PAK 到 CDN → 客户端自动下载。

---

**美术替换 = 放文件 + 命名规范 + 零代码。程序化兜底永远在线。**
