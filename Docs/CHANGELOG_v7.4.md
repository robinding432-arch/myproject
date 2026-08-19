# CHANGELOG — v7.4 "Cargo & Death"

> 发布日期: 2026-08-18
> 版本: 7.4.0

## 新增功能 (6 大项)

### 1. 飞船货舱系统 ✅
- 新增 `UShipCargoComponent` (ActorComponent)
- 容量: 重量 (kg) + 体积 (m³) 双维度
- 装卸模式: Manual / AutoOnDock / AutoOnQuest
- 易腐货物: 保鲜计时器, 过期自动腐烂消失
- 任务绑定: 货物绑定到 QuestID, 不可丢弃
- 网络: 全 Replicated + Server RPC + Validate

**文件:**
- `Source/StellarSystem/Public/Cargo/ShipCargoComponent.h` (170 行)
- `Source/StellarSystem/Private/Cargo/ShipCargoComponent.cpp` (219 行)

### 2. 近距离给付系统 ✅
- 玩家靠近 NPC (≤300cm) 按 E → 自动检测可交付任务 → 扣除背包 → 推进任务
- 飞船靠港 (≤1500cm) → 自动从货舱卸货 → 自动完成任务
- 接取货运任务时 → 货物自动装入飞船货舱 (带保鲜计时)
- 距离进度条 UI 支持
- 服务端距离校验 (防作弊)

**文件:**
- `Source/StellarSystem/Public/Cargo/ProximityDeliverySystem.h` (149 行)
- `Source/StellarSystem/Private/Cargo/ProximityDeliverySystem.cpp` (260 行)

### 3. 飞船失效系统 ✅
- 飞船被毁 → 立即标记 `FlightMode=Dead` → 禁止登船
- 残骸: 物理坍塌/冒烟特效, 10 分钟自动消失
- 索赔后: 30 秒快速消失 + 淡出
- 扣押: 无限期保留
- 残骸可搜刮 (服务端验证)

**文件:**
- `Source/StellarSystem/Public/Death/ShipInvalidationSystem.h` (179 行)
- `Source/StellarSystem/Private/Death/ShipInvalidationSystem.cpp` (268 行)

### 4. 玩家死亡快照系统 ✅
- 死亡时完整保存: 装备/背包/快捷栏/弹药/货币/维生指标
- `FPlayerDeathSnapshot` 结构体 (USTRUCT, Replicated)
- 支持多种死亡原因 (Combat/Environment/Space/Drowning/Falling/Explosion/ShipDestroyed/Suicide/Admin)

**文件:** (同上 PlayerDeathSystem)

### 5. 尸体立即消失 ✅
- 默认 `CorpseLifetime = 0` → 立即淡出/销毁
- 可选: 生成尸体 Actor → 淡出 1.5 秒后销毁
- 材质透明度动画 (UMaterialInstanceDynamic)
- 碰撞立即关闭

**文件:** (同上)

### 6. 医院复活完整转移 ✅
- 复活时从快照 **100% 恢复**: 装备/背包/快捷栏/弹药/货币
- 医院复活: 恢复 80% HP, 仅丢 5% 货币
- 野外复活: 恢复 50% HP, 丢 10% 货币
- 复活无敌时间: 5 秒
- 自动选择最近复活点 (优先医院)

**文件:**
- `Source/StellarSystem/Public/Death/PlayerDeathSystem.h` (301 行)
- `Source/StellarSystem/Private/Death/PlayerDeathSystem.cpp` (476 行)

## 修改的文件

| 文件 | 修改 |
|---|---|
| `Source/StellarSystem/StellarSystem.Build.cs` | +Cargo/Death 模块定义 + Server 权威宏 |
| `Source/StellarSystem/Public/Ship/ShipPawn.h` | 需挂载 ShipCargoComponent |
| `Source/StellarSystem/Public/AI/QuestSystemV2.h` | Deliver 目标与 ProximityDelivery 集成 |
| `Source/StellarSystem/Public/Combat/RespawnSystem.h` | 与 PlayerDeathManager 协同 |
| `README.md` | 更新到 v7.4 |
| `VERSION.txt` | 7.4.0 |
| `_FILE_MANIFEST.txt` | 更新文件清单 |

## 新增条件编译宏

| 宏 | 默认 | 说明 |
|---|---|---|
| `WITH_CARGO_SYSTEM` | 1 | 启用货舱系统 |
| `WITH_PROXIMITY_DELIVERY` | 1 | 启用近距离给付 |
| `WITH_SHIP_INVALIDATION` | 1 | 启用飞船失效 |
| `WITH_PLAYER_DEATH_SYSTEM` | 1 | 启用玩家死亡系统 |
| `CARGO_AUTHORITY` | Server=1 | 货舱权威端 |
| `DEATH_AUTHORITY` | Server=1 | 死亡权威端 |

## 文件统计

| 指标 | v7.3 | v7.4 | 增量 |
|---|---|---|---|
| 总文件数 | 340 | 352 | +12 |
| C++ 头文件 | 107 | 113 | +6 |
| C++ 源文件 | 105 | 111 | +6 |
| C++ 代码总行数 | ~60,000 | ~63,500 | +3,500 |
| 文档数 | 39 | 40 | +1 |
| 压缩包大小 | 752 KB | ~800 KB | +~50 KB |

## 升级指南 (从 v7.3)

1. 替换 `Source/StellarSystem/Build.cs`
2. 新增 `Source/StellarSystem/Public/Cargo/` 目录 (2 文件)
3. 新增 `Source/StellarSystem/Private/Cargo/` 目录 (2 文件)
4. 新增 `Source/StellarSystem/Public/Death/` 目录 (2 文件)
5. 新增 `Source/StellarSystem/Private/Death/` 目录 (2 文件)
6. 在 `ShipPawn` 构造函数中挂载 `UShipCargoComponent`
7. 在 GameMode 中 Spawn `AProximityDeliveryManager` / `AShipInvalidationManager` / `APlayerDeathManager`
8. 在 `MyCharacter::Die()` 中调用 `PlayerDeathManager::OnPlayerDied`
9. 重新生成项目文件并编译

## 向后兼容

- 所有新系统默认启用, 不影响现有功能
- `bSpawnCorpseActor = false` 保持旧行为 (无尸体)
- `CorpseLifetime = 0` 保持立即消失
- 未挂载 `ShipCargoComponent` 的飞船仍可正常使用 (货舱为空)
- 现有任务系统无需修改, `Deliver` 目标自动接入新给付系统

## 已知限制

- 尸体材质淡出使用简化实现 (生产环境应使用 Timeline/Material Parameter Collection)
- 搜刮系统为框架, 需配合具体物品掉落表
- 医院复活点需手动配置 (在 RespawnManager 中注册医院类型复活点)
- 货币恢复需配合 `CurrencyComponent` 完整实现
