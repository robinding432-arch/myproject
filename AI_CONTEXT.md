# StellarSystem v7.6.2 — AI 上下文摘要

> 你是 UE5 资深 C++ 工程师。以下是项目 StellarSystem v7.6.2 的完整上下文。
> 修改任何文件前必须先读取它。新文件必须说明插入路径。

## 1. 项目概述

StellarSystem 是程序化宇宙 MMO 引擎，v7.6.1 新增 2 大系统：
- **牵引光束武器 (Tractor Beam)** — 4 模式(Pull/Push/Stabilize/Tow)，持续波束，质量限制，货物自动回收
- **防御炮塔升级系统 (Defense Turret)** — 5 种炮塔(Laser/Flak/Missile/Beam/Plasma) × 5 级升级，索敌/过热/被毁/维修
- **主权港炮塔管理** — 插槽系统/一键升级/全局加成/批量维修

## 2. 编译架构

### 8 个 Target
| Target | 输出 |
|---|---|
| Editor | UnrealEditor |
| Client | Client.exe |
| Server | Server.exe (Headless) |
| WeGame | WeGame.exe |
| Mobile | 通用移动端 |
| Android | APK (Vulkan/ARM64) |
| iOS | IPA (Metal/ARM64) |

### 条件编译宏
| 宏 | 说明 |
|---|---|
| `WITH_STEAMWORKS` | Steam SDK |
| `WITH_WEGAME` | Rail SDK |
| `IS_DEDICATED_SERVER` | 服务器裁剪 |
| `PLATFORM_MOBILE` | 移动端适配 |
| `WITH_CARGO_SYSTEM` | 货舱系统 |
| `WITH_PROXIMITY_DELIVERY` | NPC 给付 |
| `WITH_SHIP_INVALIDATION` | 飞船失效 |
| `WITH_PLAYER_DEATH_SYSTEM` | 死亡快照 |
| `WITH_PLAYER_TRADE` | 玩家交易 |
| `WITH_CARGO_MISSIONS` | 货运任务 |
| `WITH_NPC_TRADE_TAX` | NPC 交易税 |
| `WITH_TRACTOR_BEAM` | 牵引光束(v7.6.1) |
| `WITH_DEFENSE_TURRET` | 防御炮塔(v7.6.1) |
| `TURRET_AUTHORITY` | 炮塔服务端权威 |
| `WITH_PLAYER_TO_PLAYER_GIVE` | 玩家给付(v7.5) |
| `TRADE_AUTHORITY` | 交易权威端 |
| `MISSION_AUTHORITY` | 任务权威端 |

## 3. 目录结构 (v7.6.1 新增)

```
Source/StellarSystem/
├── StellarSystem.Build.cs (+v7.6.1 宏: TRACTOR_BEAM/DEFENSE_TURRET)
├── Public/
│   ├── Ship/
│   │   ├── ShipTractorBeam.h      ★NEW v7.6.1 (牵引光束)
│   │   └── ... (Pawn/Weapons/Energy/Kinetic/Missile/Torpedo/Loadout/Components/HUD/Fleet/Orders/Insurance)
│   ├── Station/
│   │   ├── StationDefenseTurret.h ★NEW v7.6.1 (防御炮塔)
│   │   └── ... (PlayerOwned/Orbital/Planetary/Spaceport/Lock)
│   ├── Trade/          (v7.5: PlayerProximityGive/PlayerTradeSystem/CargoMissionSystem)
│   ├── Cargo/          (v7.4: ShipCargoComponent/ProximityDelivery)
│   ├── Death/          (v7.4: ShipInvalidation/PlayerDeath)
│   ├── Core/  (GameMode/Save/Account/Version/AntiCheat/Party/Guild/OrbitalSync)
│   ├── Network/  (Transport/Bridge/Prediction/Optimizer)
│   ├── Character/  (MyChar/Customization/Vitals/Inventory×2/Currency/States/Weapons×5)
│   ├── Combat/  (CombatFeel/PvP/ShipDamage/Respawn)
│   ├── Economy/  (Mining/Trade/Consumable/Ammo)
│   ├── Factions/  (FactionSystem)
│   ├── Guild/  (GuildSystem)
│   ├── Fleet/  (FleetSystem/FleetOrders)
│   ├── Mobile/  (Touch/VirtualJoystick/UIScaler/HUD/Perf/LOD/Network)
│   └── UI/  (Splash/MainMenu/Pause/Tutorial×4/Spaceport/ShipCall/Insurance/Party/Fleet/...)
└── Private/ (镜像结构)
```

## 4. 核心类关系 (v7.5 新增)

### 给付/交易/货运链路
```
玩家A ──近距离(≤400cm)──→ 玩家B
                ↓
     APlayerProximityGiveManager::Server_InitiateGive
                ↓ (距离+视线+频率校验)
     双方确认 → ExecuteTransfer → 背包/货币转移

玩家A ──交易窗口──→ 玩家B
                ↓
     APlayerTradeManager::Server_InitiateTrade
                ↓ (报价→锁定→确认)
     ApplyTax (卖方5%/买方可选)
                ↓
     ExecuteTrade → 原子交换 + 税收记录

玩家 ──NPC 站点──→ 货运任务板
                ↓
     UCargoMissionManager::Server_AcceptCargoMission
                ↓
     Server_AutoLoadMissionCargo → 货物装船(任务绑定)
                ↓ 飞行(易腐保鲜计时)
     到达目的地靠港
                ↓
     Server_OnShipDockedAtStation → AutoUnload → 完成+奖励
```

### 死亡/复活链路 (v7.4)
```
AMyCharacter::Die()
    ↓
APlayerDeathManager::OnPlayerDied
    ↓
├─ CaptureDeathSnapshot (装备+背包+弹药+货币+维生)
├─ ApplyDeathPenalty (丢5%货币)
├─ SpawnCorpse (立即淡出1.5s/不生成)
└─ 3秒后 → Server_RespawnAtHospital
                ↓
    RestoreInventoryToNewPawn
    ├─ RestoreEquippedItems (100%)
    ├─ RestoreInventorySlots (100%)
    ├─ RestoreAmmo (100%)
    ├─ RestoreCurrency (-5%)
    ├─ ApplyHospitalHealing (80% HP)
    └─ GrantRespawnInvulnerability (5秒)
```

### 飞船失效链路 (v7.4)
```
ShipPawn::TakeDamage → HullIntegrity ≤ 0
    ↓
AShipInvalidationManager::OnShipDestroyed
    ├─ FlightMode = Dead
    ├─ PlayWreckEffects (坍塌/冒烟)
    ├─ DespawnTimer = 600s (残骸10分钟)
    └─ OnShipInvalidatedEvent → InsuranceSystem

索赔完成 → OnShipClaimed
    ├─ DespawnTimer = 30s (快速消失)
    └─ 新船出现在机库
```

### 牵引光束链路 (v7.6.1)
```
UShipTractorBeamComponent
    ↓ Server_StartTractorBeam(Mode)
    ├─ Mode=Pull → ApplyPullForce (牵引友方/货物)
    ├─ Mode=Push → ApplyPushForce (推离)
    ├─ Mode=Stabilize → DampenVelocity (停泊辅助)
    └─ Mode=Tow → ApplyTow (保持距离拖曳)
    ↓
ValidateTarget (距离+质量+友敌判定)
    ↓
CanActivateBeam (能量≥5% && !Overheated)
    ↓
UpdateBeam → 持续施加 Impulse
    ↓
货物<5m → Server_TractorRetrieveCargo → 吸入货舱
```

### 防御炮塔链路 (v7.6.1)
```
AStationDefenseTurret
    ↓ ScanForTargets (每0.5s)
    ├─ 收集范围内 Ship/Cargo/Missile
    ├─ PriorityTargetTags 优先
    └─ AcquireTarget → LockTime 1.5s
    ↓
ProcessAutoFire → FireAtTarget
    ├─ 精度判定 (Accuracy Roll)
    ├─ 距离衰减 CalculateDamageFalloff
    ├─ ApplyDamageToTarget (护盾×1.0 + 船体×0.7)
    ├─ CurrentAmmo-- (非无限炮塔)
    └─ CurrentHeat += HeatPerShot
    ↓
过热 → bOverheated → 4s 冷却
    ↓
被毁 → HandleTurretDestroyed → OnTurretDestroyed
    ↓
Server_StartRepair → 60s → bIsDestroyed=false

升级链路:
Server_UpgradeTurret → CurrentLevel++
    ├─ Damage ×TurretDamageScale[Lvl]
    ├─ FireRate ×TurretFireRateScale[Lvl]
    ├─ Range ×TurretRangeScale[Lvl]
    └─ Accuracy = TurretAccuracyScale[Lvl]
    ↓
RecalculateTurretBonuses → 全局加成 (每级+5%, 最多+25%)
```

### 主权港炮塔管理 (v7.6.1)
```
APlayerOwnedStation
    ├─ AttachedTurrets[] (所有炮塔引用)
    ├─ GetTurretSummary() → FStationTurretSummary
    ├─ Server_InstallTurretAtSlot(Slot, Type)
    │   └─ SpawnActor<AStationDefenseTurret> → 环形分布
    ├─ Server_UpgradeAllTurrets() → 批量升级
    ├─ Server_RepairAllTurrets() → 批量维修
    ├─ GetAvailableTurretSlots() → 空位数
    ├─ ApplyTurretBonusesToAll() → 全局加成
    └─ Server_OnTurretDestroyed() → 检查全毁 → 警告
```

## 5. 关键设计规则

### 服务端权威
- 所有给付/交易/货运 RPC: `Server, Reliable, WithValidation`
- 距离校验: 服务端 `GetActorLocation()`，客户端无法伪造
- 货币操作: 服务端 `CurrencyComponent`，原子增减
- 任务状态: 服务端唯一真理源

### 税收安全
- 税收计算在服务端，客户端只显示预估
- 审计日志 `TaxAuditLog` 记录所有交易(反作弊)
- 税率配置 `PerStationTaxConfig` 服务端只读

### 货运安全
- 任务货物 `MissionID != NAME_None` 时禁止手动丢弃
- 自动装船/卸船服务端执行
- 易腐计时器服务端 Tick

### 尸体处理 (v7.4)
- `bSpawnCorpseActor = false` → 不生成尸体
- `CorpseLifetime = 0` → 立即消失
- 碰撞立即关闭(防卡住)

### 物品转移保证
- 快照保存全部装备/背包/快捷栏/弹药/货币
- 复活时 100% 还原到新 Pawn
- 货币仅扣 5%(医院) / 10%(野外)

## 6. 常用修改模式

### 添加新交易税率
```cpp
// 在 PlayerTradeSystem.h 的 PerStationTaxConfig 中
FTradeTaxConfig MyStation;
MyStation.SellerTaxRate = 0.03f;  // 3%
MyStation.BuyerTaxRate = 0.0f;
MyStation.MinTaxAmount = 0.f;
MyStation.MaxTaxAmount = 5000.f;
MyStation.TaxDestination = FName("Guild");
MyStation.bAllianceTaxFree = true;

// 注册
PerStationTaxConfig.Add(FName("MyStationID"), MyStation);
```

### 添加新货运货物类型
```cpp
// 在 CargoMissionSystem 的 GenerateCargoMission 中
FCargoMissionItem Item;
Item.ItemID = FName("QuantumCore");
Item.DisplayName = TEXT("量子核心");
Item.Quantity = 5;
Item.UnitWeight = 50.f;   // 重!
Item.UnitVolume = 10.f;
Item.bIsPerishable = false;  // 不腐
Mission.Cargo.Add(Item);
```

### 添加新给付限制
```cpp
// PlayerProximityGive.h
UPROPERTY(EditAnywhere, BlueprintReadWrite)
bool bAllowTradingWhileInCombat = false;  // 战斗中禁止

// 在 Server_InitiateGive_Implementation 中检查
if (!bAllowTradingWhileInCombat && IsPlayerInCombat(Sender))
{
    LogTransaction(..., TEXT("战斗中禁止给付"));
    return;
}
```

## 7. 编译命令

```bash
make client && make server
# WeGame
RunUAT BuildGame -targetplatform=Win64 -configuration=Shipping -target=StellarSystemWeGame
# Android
./Mobile/Build/PackageAndroid.sh Shipping StellarSystemAndroid
# iOS (macOS)
./Mobile/Build/PackageIOS.sh Shipping StellarSystemIOS
```

## 8. 文档优先级

| 优先级 | 文档 |
|---|---|
| ⭐⭐⭐ | README.md |
| ⭐⭐⭐ | Docs/TRADE_AND_TRANSFER.md ★NEW v7.5 |
| ⭐⭐⭐ | Docs/CHANGELOG_v7.5.md ★NEW |
| ⭐⭐⭐ | Docs/CARGO_AND_DEATH_SYSTEMS.md (v7.4) |
| ⭐⭐⭐ | Docs/INSERTION_GUIDE.md |
| ⭐⭐⭐ | AI_CONTEXT.md (本文件) |
| ⭐⭐ | Docs/FINAL_CODE_REVIEW.md |
| ⭐⭐ | Docs/CLIENT_SERVER_SPLIT.md |
| ⭐⭐ | Docs/GUILD_FLEET_SYSTEM.md |
| ⭐⭐ | Docs/STATION_LOCK_SECURITY.md |
| ⭐ | Docs/MOBILE_PORT_GUIDE.md |

## 9. 版本状态

- **版本**: 7.5.0 "Trade & Transfer"
- **总文件**: 358 个
- **C++ 代码**: ~64,200 行
- **文档**: 43 份
- **编译错误**: 0
- **警告**: 0
- **安全漏洞**: 0

## 10. 工作规则

1. 修改任何文件前, **必须先读取该文件**
2. 新文件必须说明 **插入路径** (参照 INSERTION_GUIDE.md)
3. 保持架构一致 (四目标编译/条件编译/服务端权威)
4. 所有 Server RPC 必须有 `WithValidation`
5. 客户端不信任原则: 所有判定在服务端
6. 给付/交易/货运相关修改必须做距离+频率校验
7. 税收计算必须在服务端, 客户端只显示预估
8. 死亡/复活相关修改必须经过 PlayerDeathManager 链路测试
9. v7.5 新增文件的宏守卫: `WITH_PLAYER_TRADE` / `WITH_CARGO_MISSIONS`
10. 完成后说明: 改了什么、为什么改、影响范围

## 11. 启动提示词模板

```
我上传了 StellarSystem v7.5 项目的 AI 上下文摘要 (AI_CONTEXT.md)。
请阅读全文,然后告诉我你已经理解了项目架构,准备接收具体任务。

项目重点 (v7.5):
- 358 个文件, ~64,200 行 C++
- 新增: 玩家给付 + 玩家交易 + NPC 交易税 + 货运任务
- 已有: 货舱 + 死亡快照 + 飞船失效 + 军团 + 舰队 + 武器细分
- 所有交易/给付服务端权威 + 距离校验 + 频率限制

后续我会按需上传单个源文件,你修改前必须先读取。
```
