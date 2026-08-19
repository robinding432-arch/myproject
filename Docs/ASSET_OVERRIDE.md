# 🎨 ASSET OVERRIDE GUIDE — 美术替换完整流程

> 给后期接手的美术/TA/维护人员看的。目标：**零代码替换 AI 生成的 3D 模型**。

---

## 核心原理

```
程序化生成（AI/噪声）  ← 默认兜底，永远可用
        ↕ 自动检测
美术资产覆盖（自建模型） ← 存在则优先使用
```

**三原则：**
1. **数据驱动**：所有"用什么模型"走配置表（DataAsset），不写死在 C++ 里
2. **路径约定 > 硬编码**：按命名规则自动查找，美术只需按规则放文件
3. **Soft Reference**：用 `TSoftObjectPtr` 延迟加载，不占内存

---

## 快速替换（3 步）

### Step 1: 按命名规范导出 FBX

```
/Game/Art/StaticMeshes/{LogicalName}/{LogicalName}_{Quality}.fbx
```

例：替换战斗机船体
```
/Game/Art/StaticMeshes/Ship_Hull_Fighter/Ship_Hull_Fighter_High.fbx
```

### Step 2: UE 导入

1. 打开编辑器 → Content Browser
2. 导航到对应目录
3. 拖入 FBX → 勾选 "Generate Lightmap UVs"
4. 保存 → 自动转 `.uasset`

### Step 3: 运行游戏

```
启动 → 自动检测到美术资产 → 替换程序化模型 ✅
```

**不需要改任何代码。不需要重启编辑器。**

---

## 完整命名速查表

### 飞船

| 模块 | LogicalName | 质量级别 |
|---|---|---|
| 船体 | `Ship_Hull_Fighter` | High/Medium/Low |
| 船体 | `Ship_Hull_Freighter` | High/Medium/Low |
| 船体 | `Ship_Hull_Explorer` | High/Medium/Low |
| 船体 | `Ship_Hull_Capital` | High/Medium/Low |
| 机翼 | `Ship_Wing_Fighter` | High/Medium/Low |
| 引擎 | `Ship_Engine_Capital_0` | High/Medium/Low |
| 武器 | `Ship_Weapon_Laser` | High/Medium/Low |

### 角色

| 模块 | LogicalName |
|---|---|
| 头部（男） | `Character_Head_Male_Heroic` |
| 头部（女） | `Character_Head_Female_Cute` |
| 身体 | `Character_Body_Athletic` |
| 头发 | `Character_Hair_Short_01` |
| 胡须 | `Character_Beard_Medium_01` |

### 护甲

| 槽位 | LogicalName |
|---|---|
| 头盔 | `Armor_Heavy_Helmet_Rare` |
| 胸甲 | `Armor_Medium_Chest_Epic` |
| 护腿 | `Armor_Light_Legs_Common` |
| 手套 | `Armor_Powered_Gloves_Prototype` |

### 武器

| 类型 | LogicalName |
|---|---|
| 激光步枪 | `Weapon_LaserRifle_Rare` |
| 等离子炮 | `Weapon_PlasmaCannon_Epic` |
| 轨道炮 | `Weapon_Railgun_Legendary` |

### 建筑

| 类型 | LogicalName |
|---|---|
| 居住区 | `Building_Habitation_01` |
| 工业区 | `Building_Industrial_01` |
| 科研站 | `Building_Research_01` |
| 军事基地 | `Building_Military_01` |
| 贸易站 | `Building_Trade_01` |
| 农业穹顶 | `Building_Farm_01` |
| 采矿设施 | `Building_Mining_01` |
| 通信塔 | `Building_Communication_01` |
| 能源站 | `Building_Energy_01` |
| 仓储 | `Building_Storage_01` |

### 植被

| Biome | LogicalName |
|---|---|
| 森林 | `Foliage_Forest_Oak_01` |
| 草原 | `Foliage_Grassland_Bush_02` |
| 沙漠 | `Foliage_Desert_Cactus_01` |

### 特效

| 类型 | LogicalName |
|---|---|
| 飞船爆炸 | `FX_ShipExplosion_Large` |
| 角色死亡 | `FX_DeathCharacter_Blood` |
| 火花 | `FX_Sparks_Small` |
| 碎片 | `FX_Debris_Metal` |
| 引擎尾焰 | `FX_EngineTrail_Large` |

### 音频

| 类型 | LogicalName |
|---|---|
| 引擎轰鸣 | `Audio_EngineHum_Loop` |
| 跃迁嗡嗡 | `Audio_WarpTravel_Loop` |
| 武器开火 | `Audio_WeaponLaser_Shot` |
| 大爆炸 | `Audio_Explosion_Large` |
| UI 点击 | `Audio_UI_Click` |

---

## 方式 B: DataAsset 精确控制

如果需要**不按命名规则**的精确控制：

### 1. 创建 DataAsset

```
Content Browser → 右键 → Miscellaneous → Data Asset
→ 选择父类: AssetRegistry
→ 命名: DA_AssetRegistry_Main
```

### 2. 添加规则

打开 DA_AssetRegistry_Main → OverrideRules → + 添加：

| 字段 | 填什么 |
|---|---|
| LogicalName | `Ship_Hull_Fighter` |
| OverrideMesh | 拖入你的 `.uasset` |
| OverrideMaterial | （可选）拖入材质 |
| Priority | 100（高于自动发现的 0） |
| RequiredTags | （可选）过滤条件 |
| bEnabled | ✅ 勾选 |

### 3. 注册到 GameMode

```
BP_GameMode → Class Defaults → AssetRegistry → 选 DA_AssetRegistry_Main
```

---

## 回退到程序化

三种方式，都**不需要删文件**：

| 方式 | 操作 |
|---|---|
| 临时禁用 | DataAsset 中取消 `bEnabled` 勾选 |
| 删除文件 | 从 `/Game/Art/` 删除对应文件 → 自动 fallback |
| 重命名 | 改成不匹配的名称 → 自动 fallback |

**程序化生成永远兜底，不会空模型。**

---

## 目录结构推荐

```
/Game/
├── Art/
│   ├── StaticMeshes/
│   │   ├── Ship_Hull_Fighter/
│   │   │   ├── Ship_Hull_Fighter_High.uasset
│   │   │   ├── Ship_Hull_Fighter_Medium.uasset
│   │   │   └── Ship_Hull_Fighter_Low.uasset
│   │   ├── Character_Head_Male_Heroic/
│   │   ├── Armor_Heavy_Chest_Rare/
│   │   └── Building_Habitation_01/
│   ├── SkeletalMeshes/
│   │   ├── Character_Body_Athletic/
│   │   └── Weapon_LaserRifle_Rare/
│   ├── Materials/
│   │   ├── M_Ship_Hull_Fighter.uasset
│   │   └── M_Armor_Heavy.uasset
│   ├── Textures/
│   │   ├── T_Ship_Hull_D.uasset
│   │   └── T_Ship_Hull_N.uasset
│   ├── Animations/
│   │   ├── ABP_Character.uasset
│   │   └── Anim_Character_Run.uasset
│   └── Effects/
│       ├── NS_ShipExplosion.uasset
│       └── NS_EngineTrail.uasset
├── Audio/
│   ├── SFX/
│   │   ├── EngineHum_Loop.uasset
│   │   └── WeaponLaser_Shot.uasset
│   └── Music/
│       └── MainTheme.uasset
├── Data/
│   └── DA_AssetRegistry_Main.uasset
└── Blueprints/
    ├── BP_GameMode.uasset
    └── BP_MainMenu.uasset
```

---

## 常见 Q&A

**Q: 我换了模型但游戏里没变？**
A: 检查 ① 文件名是否完全匹配 LogicalName ② Priority 是否够高 ③ bEnabled 是否勾选

**Q: 模型显示但材质是紫色？**
A: 材质引用丢失。重新导入 FBX 时勾选 "Import Materials" 或在编辑器里手动指定。

**Q: 模型比程序化的小/大很多？**
A: 在导入设置里调整 Import Scale，或在 DataAsset 的 Transform 里设 Scale。

**Q: 能否用骨骼网格（Skeletal Mesh）？**
A: 可以。AssetRegistry 同时支持 `OverrideSkeletalMesh` 和 `OverrideMesh`。

**Q: LOD 自动切换怎么配？**
A: 在 UE 的 Static Mesh Editor 里设置 LOD 层级 → 自动发现按 Quality 命名查找。

**Q: 多人游戏里别人能看到我的自定义模型吗？**
A: 需要确保所有玩家都有相同的资产包（DLC/热更新）。

---

## 维护人员 Checklist

- [ ] 所有美术资产按 LogicalName 命名
- [ ] Quality 级别至少提供 High（Low 可选）
- [ ] 材质正确引用（无紫色）
- [ ] 碰撞体已生成（Collision → Auto Generate）
- [ ] LOD 已设置（至少 2 级）
- [ ] 音频文件采样率 44100Hz
- [ ] Niagara 特效已编译
- [ ] DataAsset 规则已验证
- [ ] 回退测试（删文件 → 程序化兜底正常）

---

**按本指南操作，美术替换零代码、零编译、零停机。**
