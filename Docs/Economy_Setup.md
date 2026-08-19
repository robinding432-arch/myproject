# 💰 经济系统配置指南

## 概述

经济系统由两大子系统组成：
- **采矿系统** (`MiningSystem.h/cpp`) — 矿脉生成 + 激光采集
- **贸易系统** (`TradeSystem.h/cpp`) — 动态定价 + 跨站套利

---

## 一、矿石类型（14 种）

| 矿石 | 基础价值 | 偏好 Biome | 说明 |
|---|---|---|---|
| Iron（铁） | 10 | Grassland/Rock | 最常见，基础建材 |
| Copper（铜） | 15 | Desert/Rock | 导电，电子设备 |
| Aluminum（铝） | 20 | Beach/Tundra | 轻量合金 |
| Titanium（钛） | 50 | Rock/Snow | 高强度合金 |
| Gold（金） | 100 | Desert/Tundra | 电子+奢侈品 |
| Platinum（铂） | 150 | Rock/Snow | 催化剂 |
| Diamond（钻石） | 300 | Rock | 切割工具 |
| Uranium（铀） | 200 | Tundra/Snow | 核燃料（受监管） |
| Helium3（氦3） | 500 | — | 仅气体巨星 |
| RareEarth（稀土） | 80 | Forest/Grassland | 高级电子 |
| Silicon（硅） | 12 | Desert/Rock | 芯片原料 |
| Carbon（碳） | 8 | Forest/Grassland | 石墨烯 |
| Ice（冰） | 5 | Snow/Tundra | 水+氧气来源 |
| Sulphur（硫） | 10 | Desert | 化工原料 |

---

## 二、配置矿石数据库

在编辑器中创建 `DataAsset` (类型 `PrimaryDataAsset`)，添加 `FOreData` 数组：

```
Content/Data/DA_OreDatabase.uasset

OreDatabase[0]:
  OreType = Iron
  DisplayName = "Iron Ore"
  BaseValue = 10
  Rarity = 0.9          ← 越接近 1 越常见
  Hardness = 1.0
  MassPerUnit = 2.0
  PreferredBiomes = [Grassland, Rock]
  OreColor = (0.6, 0.55, 0.5)

OreDatabase[1]:
  OreType = Diamond
  DisplayName = "Diamond"
  BaseValue = 300
  Rarity = 0.05         ← 极稀有
  Hardness = 5.0          ← 难采
  MassPerUnit = 0.5
  PreferredBiomes = [Rock, Snow]
  OreColor = (0.8, 0.95, 1.0)
```

---

## 三、配置贸易站

每个 `ATradeStation` Actor 在 Details 面板配置：

```
StationID = "Station_Alpha"
StationName = "阿尔法贸易站"
ControllingFaction = "VerdantGuild"
EconomicProsperity = 1.2

Commodities[0]:
  CommodityID = "IronOre"
  DisplayName = "铁矿石"
  OreType = Iron
  bIsRefined = false
  BaseBuyPrice = 10
  BaseSellPrice = 8
  PriceVolatility = 0.15
  SupplyDemandCycle = 120
  MaxStock = 5000

Commodities[1]:
  CommodityID = "TitaniumOre"
  DisplayName = "钛矿石"
  BaseBuyPrice = 50
  BaseSellPrice = 40
  PriceVolatility = 0.25
  ...
```

---

## 四、精炼配方

```
RefineRecipes[0]:
  MaterialName = "SteelPlate"
  DisplayName = "钢制板材"
  BaseValue = 80
  RequiredOres = [Iron, Coal]
  RequiredAmounts = [3, 1]
  RefineTime = 8.0

RefineRecipes[1]:
  MaterialName = "TitaniumAlloy"
  DisplayName = "钛合金"
  BaseValue = 200
  RequiredOres = [Titanium, Aluminum]
  RequiredAmounts = [2, 1]
  RefineTime = 15.0
```

---

## 五、动态定价机制

价格每 5 秒更新一次，公式：

```
Price = BasePrice
      × (1 + sin(t / Cycle) × 0.3)     ← 供需周期
      × ProsperityFactor                    ← 0.5~2.0
      × (1 + random(-Volatility, +Volatility))
```

**经济事件**（GameMode 可触发）：

| 事件 | 效果 |
|---|---|
| `Boom` | 所有买入价 +50%，卖出价 +30% |
| `Crisis` | 所有买入价 -40%，卖出价 -30% |
| `Shortage` | 特定商品卖出价 +60% |
| `Glut` | 特定商品买入价 -50% |

触发示例（蓝图/C++）：
```cpp
UTradeNetwork* Net = GetWorld()->GetSubsystem<UTradeNetwork>();
Net->TriggerEconomicEvent(FName("Boom"), 0.5f, 300.f); // 5分钟繁荣期
```

---

## 六、采矿操作

1. 飞到行星表面附近（距离 < 5000cm）
2. 对准矿脉（按 `IA_Mine` 启动激光）
3. 每 0.5 秒自动采集，存入背包
4. 飞到贸易站，打开贸易界面（按 `IA_Trade`）
5. 卖出矿石 → 获得 Credits

---

## 七、跑商策略

打开星图（T 键）→ 查看贸易路线面板：
- 显示从当前站出发利润最高的 10 条路线
- 绿色 = 高利润，红色 = 亏损
- 考虑距离：利润/公里才是真实效率

**示例**：
```
阿尔法站 → 贝塔站：钛矿 × 1 = 利润 80 Cr/km ✓
阿尔法站 → 伽马站：铁矿石 × 1 = 利润 2 Cr/km ✗
```

---

## 八、与派系联动

| 派系 | 经济特征 |
|---|---|
| 翠绿商会 | 全商品繁荣度 +50%，税率最低 |
| 地球帝国 | 管制品（铀/氦3）溢价 +200% |
| 绯红海盗 | 黑市：赃物价格 +100%，合法商品 -50% |
| 虚空学者 | 稀有材料（钻石/稀土）溢价 +80% |
| 游牧部落 | 生物质/冰溢价 +60%，工业材料折价 |
| 自动蜂群 | 不贸易（敌视一切） |

---

## 九、已知限制

1. **精炼 UI** 未实现，需蓝图补充
2. **背包自动堆叠** 需 `InventoryComponent` 配合
3. **矿石 Mesh** 为占位球体，需美术替换（LogicalName: `Ore_Vein_{OreType}`）
4. **经济事件** 目前为全局，后续可改为区域事件
