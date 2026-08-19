# CARGO & DEATH SYSTEMS — v7.4

> 本文档说明 v7.4 新增的 6 大功能及其文件位置、调用链路和配置方法。

## 1. 飞船货舱系统 (Cargo System)

### 文件
| 文件 | 作用 |
|---|---|
| `Public/Cargo/ShipCargoComponent.h` | 货舱组件定义 |
| `Private/Cargo/ShipCargoComponent.cpp` | 容量/重量/体积/易腐/装卸实现 |

### 关键类
- `UShipCargoComponent` (ActorComponent, 挂在 ShipPawn 上)
- `FCargoEntry` (USTRUCT: ItemID/Quantity/UnitWeight/UnitVolume/bIsPerishable/PerishTimer/QuestBinding)

### 容量属性
- `MaxCargoWeight` (默认 5000 kg)
- `MaxCargoVolume` (默认 2500 m³)
- 自动计算 `CurrentWeight` / `CurrentVolume` / `GetCargoFillPercent()`

### 装卸模式 (ECargoTransferMode)
- `Manual` — 玩家手动调用 LoadCargo/UnloadCargo
- `AutoOnDock` — 靠港时自动装卸 (默认)
- `AutoOnQuest` — 任务提交时自动

### 网络
- `Cargo` 数组 Replicated
- `Server_AutoLoadFromStation` / `Server_AutoUnloadToStation` (Reliable + Validate)

### 集成点
- `ShipPawn` 需挂载 `UShipCargoComponent`
- `ProximityDeliveryManager::AutoLoadCargoOnAccept` 接取任务时自动装船
- `ProximityDeliveryManager::OnShipDockedAtStation` 靠港时自动卸货并提交任务

---

## 2. 近距离给付系统 (Proximity Delivery)

### 文件
| 文件 | 作用 |
|---|---|
| `Public/Cargo/ProximityDeliverySystem.h` | 近距离给付管理器 |
| `Private/Cargo/ProximityDeliverySystem.cpp` | 玩家/NPC/飞船/空间站交付逻辑 |

### 核心功能
1. **玩家地面交付** — 靠近 NPC (≤300cm) 按 E → 自动检测可交付任务 → 扣除背包物品 → 推进任务目标
2. **飞船靠港自动交付** — 飞船进入对接范围 (≤1500cm) → 自动从货舱卸下任务货物 → 完成任务
3. **接取任务自动装船** — 在空间站/NPC 接取货运任务 → 货物自动装入飞船货舱 (带易腐+保鲜计时)

### 关键参数
- `PlayerInteractRange = 300.f` (cm)
- `ShipDockRange = 1500.f` (cm)
- `bAutoSubmitOnEnterRange = true`
- `bRequirePlayerConfirm = true` (地面NPC需按E确认)

### 事件
- `OnNearbyDeliveriesChanged` — 附近可交付任务更新
- `OnDeliverySucceeded` — 交付成功
- `OnDeliveryFailed` — 交付失败 (含原因)

---

## 3. 飞船失效系统 (Ship Invalidation)

### 文件
| 文件 | 作用 |
|---|---|
| `Public/Death/ShipInvalidationSystem.h` | 失效管理器 |
| `Private/Death/ShipInvalidationSystem.cpp` | 失效/计时/淡出/搜刮实现 |

### 失效原因 (EShipInvalidationReason)
- `Claimed` — 已索赔 (30秒后消失)
- `Destroyed` — 被摧毁 → 残骸 (10分钟后消失)
- `Abandoned` — 玩家遗弃 (5分钟后消失)
- `Impounded` — 管理员扣押 (无限期)

### 核心机制
1. **OnShipDestroyed** → 标记 FlightMode=Dead → 禁用移动 → 播放坍塌/冒烟 → 启动 10 分钟计时
2. **OnShipClaimed** → 立即失效 → 短延迟 30 秒 → 淡出销毁
3. **IsShipValid** → 任何系统检查飞船是否可用 (GameMode/登船/交互前调用)
4. **CanPlayerBoard** → 登船权限校验

### 残骸搜刮
- `Server_LootWreck` — 服务端验证后允许搜刮
- `OnWreckLooted` — 搜刮事件

---

## 4. 玩家死亡系统 (Player Death)

### 文件
| 文件 | 作用 |
|---|---|
| `Public/Death/PlayerDeathSystem.h` | 死亡管理器 + 快照结构 |
| `Private/Death/PlayerDeathSystem.cpp` | 快照/恢复/惩罚/尸体/复活实现 |

### 死亡快照 (FPlayerDeathSnapshot)
完整保存:
- ✅ 全部装备 (EquippedItems map)
- ✅ 全部背包 (InventorySlots array)
- ✅ 快捷栏 (HotbarItems)
- ✅ 全部弹药 (AmmoInventory)
- ✅ 货币 (CurrencyAtDeath + CurrencyLost)
- ✅ 维生指标 (Health/Stamina/Hunger/Thirst/Oxygen)
- ✅ 死亡信息 (Cause/Killer/Time/Location)

### 尸体处理
- **默认: 立即消失** (`CorpseLifetime = 0`)
- 可选: 生成尸体 Actor → 淡出销毁 (`bFadeOutCorpse = true`, `CorpseFadeDuration = 1.5s`)
- `InstantDespawnCorpse` / `FadeOutAndDespawnCorpse` 两种模式

### 复活流程 (核心)
```
PlayerDeath
  ↓ CaptureDeathSnapshot (保存全部物品)
  ↓ ApplyDeathPenalty (丢货币)
  ↓ SpawnCorpse (立即消失)
  ↓ 3秒后自动触发复活
  ↓
RespawnManager.QuickRespawn
  ↓ 生成新 Pawn
  ↓
PlayerDeathManager.RestoreInventoryToNewPawn ★核心★
  ↓ RestoreEquippedItems (装备完整恢复)
  ↓ RestoreInventorySlots (背包完整恢复)
  ↓ RestoreAmmo (弹药完整恢复)
  ↓ RestoreCurrency (货币恢复-扣除惩罚)
  ↓ ApplyHospitalHealing (80% HP)
  ↓ GrantRespawnInvulnerability (5秒无敌)
  ↓ OnPlayerRespawnedWithGear 事件
```

### 死亡惩罚规则 (FDeathPenaltyRules)
| 规则 | 默认值 | 说明 |
|---|---|---|
| CreditLossPercent | 5% | 医院复活丢 5% 货币 |
| PremiumLossPercent | 0% | 付费货币不丢 |
| bDropEquippedItems | false | 医院复活保留装备 |
| bDropInventoryItems | false | 医院复活保留背包 |
| bDropAmmo | false | 医院复活保留弹药 |
| CorpseLifetime | 0s | 0 = 立即消失 |
| bHospitalRespawnFree | true | 医院免费复活 |
| WildernessRespawnPenalty | 10% | 野外复活丢 10% |

### 医院复活 vs 野外复活
| 项目 | 医院复活 | 野外复活 |
|---|---|---|
| 恢复 HP | 80% | 50% |
| 货币丢失 | 5% | 10% |
| 物品保留 | ✅ 全部 | ✅ 全部 |
| 无敌时间 | 5 秒 | 5 秒 |

---

## 5. 完整货运任务链路

```
1. 玩家在 NPC/空间站 接取货运任务
   ↓ QuestSystemV2.Server_AcceptQuest
   
2. 自动装船
   ↓ ProximityDeliveryManager.AutoLoadCargoOnAccept
   ↓ ShipCargoComponent.LoadCargo (ItemID, Qty, Weight, Volume, QuestBinding, bPerishable, PerishTime)
   ↓ 货物显示: "任务货物 - 保鲜中 12:34"
   
3. 玩家驾驶飞船飞往目的地
   ↓ 途中可查看货舱 (按 TAB → 货舱页)
   ↓ 易腐货物倒计时
   
4. 到达目的地, 靠近空间站/服务台
   ↓ OnShipDockedAtStation (距离 ≤ 1500cm)
   ↓ AutoSubmitCargoToStation
   ↓ ShipCargoComponent.ConsumeQuestCargo
   ↓ QuestManager.CompleteObjective
   ↓ OnDeliverySucceeded 事件
   ↓ 任务完成, 发放奖励
```

---

## 6. 飞船被毁完整链路

```
1. 飞船 HullIntegrity ≤ 0
   ↓ ShipPawn.TakeDamage → FlightMode = Dead
   
2. 通知失效系统
   ↓ ShipInvalidationManager.OnShipDestroyed(Ship, KillerID)
   ↓ 记录 FInvalidatedShipState
   ↓ FlightMode = Dead (禁止登船)
   ↓ PlayWreckEffects (坍塌/冒烟/物理)
   ↓ DespawnTimer = 600s (10分钟)
   
3. 通知保险系统
   ↓ OnShipInvalidatedEvent
   ↓ InsuranceSystem 创建索赔 (30-120秒处理)
   
4. 玩家死亡 (如果在船上)
   ↓ PlayerDeathManager.OnPlayerDied
   ↓ 保存快照 → 尸体立即消失 → 医院复活
   
5. 10分钟后 / 索赔完成后
   ↓ DespawnShip (销毁 Actor)
   ↓ 或 FadeOutActor (淡出)
   ↓ OnShipDespawned 事件
   
6. 索赔完成 → 新船出现在机库
   ↓ Server_SpawnReplacementShip
   ↓ 玩家可从机库呼船
```

---

## 7. 配置示例

### 在 ShipPawn 上挂载货舱
```cpp
// 在 ShipPawn 构造函数或 BeginPlay
CargoComp = CreateDefaultSubobject<UShipCargoComponent>(TEXT("CargoComponent"));
CargoComp->MaxCargoWeight = 10000.f;  // 重型货船
CargoComp->MaxCargoVolume = 5000.f;
CargoComp->TransferMode = ECargoTransferMode::AutoOnDock;
```

### 配置死亡规则
```ini
; 在 DefaultGame.ini 或 Spawner 配置
[/Script/StellarSystem.PlayerDeathManager]
bSpawnCorpseActor=false
bFadeOutCorpse=true
CorpseFadeDuration=1.5
bPreferHospitalRespawn=true
HospitalHealPercent=0.8
WildernessHealPercent=0.5
RespawnInvulnerabilityTime=5.0
```

### 连接死亡系统到角色
```cpp
// 在 MyCharacter 受伤害致死时
void AMyCharacter::Die(EDeathCause Cause, const FName& KillerID, const FString& KillerName)
{
    if (APlayerDeathManager* DM = GetWorld()->SpawnActor<APlayerDeathManager>())
    {
        DM->OnPlayerDied(this, Cause, KillerID, KillerName);
    }
}
```

---

## 8. 网络权威说明

| 系统 | 权威端 | 验证方式 |
|---|---|---|
| ShipCargo | Server | Server RPC + Validate |
| ProximityDelivery | Server | 距离校验 + RPC Validate |
| ShipInvalidation | Server | 仅服务端 Tick 计时 |
| PlayerDeath | Server | 快照仅服务端保存 |

所有关键操作都是 `Server, Reliable, WithValidation`，客户端无法伪造。

---

## 9. 与现有系统集成点

| 现有系统 | 集成方式 |
|---|---|
| `InsuranceSystem` | 监听 `OnShipInvalidatedEvent` → 自动创建索赔 |
| `RespawnManager` | `PlayerDeathManager` 调用 `QuickRespawn` 复活 |
| `QuestManagerV2` | `ProximityDeliveryManager` 调用 `CompleteObjective` |
| `ShipPawn` | 挂载 `ShipCargoComponent` + 监听死亡事件 |
| `MyCharacter` | 死亡时调用 `PlayerDeathManager::OnPlayerDied` |
| `Build.cs` | 新增 `WITH_CARGO_SYSTEM` / `WITH_SHIP_INVALIDATION` 等宏 |
