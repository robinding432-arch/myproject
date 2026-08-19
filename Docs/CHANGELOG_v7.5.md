# StellarSystem v7.5 — Changelog

> **版本代号**: "Trade & Transfer"  
> **发布日期**: 2026-08-18  
> **总文件**: 358 个  
> **C++ 代码**: ~64,200 行  
> **文档**: 43 份  

---

## 🆕 新增功能（4 大系统）

### 1. 玩家↔玩家 近距离给付系统

**文件**: `Public/Trade/PlayerProximityGive.h` + `Private/Trade/PlayerProximityGive.cpp`

| 功能 | 说明 |
|---|---|
| 面对面给付 | 两玩家距离 ≤400cm + 视线，发起给付请求 |
| 物品给付 | 最多 20 件/次，支持装备栏卸下给付 |
| 货币给付 | 单次最多 100 万信用点 |
| 双向确认 | 发起方自动确认 → 接收方接受/拒绝 |
| 超时取消 | 请求 30 秒过期，确认后 15 秒未完成自动取消 |
| 频率限制 | 每玩家每分钟最多 10 次给付（防刷） |
| 全操作日志 | 服务端记录所有给付（反作弊审计） |
| 飞船↔飞船给付 | 飞船距离 ≤1500cm，货物/货币互转 |

**核心流程**:
```
玩家A 靠近 玩家B (≤400cm, 有视线)
  → A 打开给付界面 → 选择物品/货币 → 发起请求
  → B 收到通知 → 检查距离 → 接受
  → 服务端校验 → 执行转移 → 双方确认
```

### 2. 玩家↔玩家 交易系统

**文件**: `Public/Trade/PlayerTradeSystem.h` + `Private/Trade/PlayerTradeSystem.cpp`

| 功能 | 说明 |
|---|---|
| 交易窗口 | 双向报价（类似星际公民贸易界面）|
| 物品报价 | 每方最多 20 件，可设单价 |
| 货币报价 | 单方最多 1000 万信用点 |
| 锁定机制 | 双方各自锁定报价 → 再各自最终确认 |
| 状态机 | Idle→WaitingAccept→Negotiating→Locked→Completed |
| 超时 | 总超时 120 秒，锁定后 30 秒必须确认 |
| 距离校验 | 每次操作重新校验，移动过远自动取消 |
| 视线校验 | 可选（默认开启）|
| 频率限制 | 每玩家每分钟最多 5 次交易 |
| 战斗中禁止 | 可选配置 |

### 3. NPC 站点交易税

**集成在 PlayerTradeSystem 中**

| 功能 | 说明 |
|---|---|
| 卖方税 | 默认 5%，从卖方收入扣除 |
| 买方税 | 可选 0~10%，从买方支出扣除 |
| 最低/最高税额 | 1 ~ 10000 信用点（封顶）|
| 按站点覆盖 | 每个 NPC 空间站可设不同税率 |
| 派系减免 | 友好派系 50% off，同盟全免 |
| 军团减免 | 同军团成员 30% off |
| 税收去向 | Station（站点收入）/ Faction（派系）/ Guild（军团）|
| NPC 买卖收税 | `Server_SellToNPCStation` / `Server_BuyFromNPCStation` |
| 税收审计日志 | 所有交易记录（反作弊 + 经济分析）|

**税率配置示例**:
```cpp
// 高端商业站: 低税率吸引贸易
FTradeTaxConfig HighEndStation;
HighEndStation.SellerTaxRate = 0.02f;  // 2%
HighEndStation.BuyerTaxRate = 0.0f;
HighEndStation.TaxDestination = FName("Station");

// 黑市站: 高税率但隐蔽
FTradeTaxConfig BlackMarket;
BlackMarket.SellerTaxRate = 0.15f;  // 15%
BlackMarket.BuyerTaxRate = 0.05f;
BlackMarket.TaxDestination = FName("Guild");

// 注册
PerStationTaxConfig.Add(FName("ZurichStation"), HighEndStation);
PerStationTaxConfig.Add(FName("ShadowMarket"), BlackMarket);
```

### 4. 货运任务系统

**文件**: `Public/Trade/CargoMissionSystem.h` + `Private/Trade/CargoMissionSystem.cpp`

| 功能 | 说明 |
|---|---|
| 任务板 | 每个 NPC 空间站/太空港有任务板 |
| 自动装船 | 接取任务后货物**自动装入**飞船货舱 |
| 飞行运输 | 持货飞行，易腐货物有保鲜计时器 |
| 自动卸船 | 到达目的地靠港 → **自动卸货** → 完成任务 |
| 奖励发放 | 信用点 + 物品 + 派系声望 |
| 时限 | 每任务有倒计时，超时失败 |
| 易腐机制 | 33% 货物易腐，腐坏 → 任务失败 |
| 难度分级 | 1~5 级，影响货物量/奖励/风险 |
| 高风险路线 | 高难度任务标记危险区域 |
| 自动生成 | 每 10 分钟为新站点生成 3 个任务 |
| 手动生成 | `GenerateCargoMission(From, To, Tier)` |

**完整流程**:
```
① 飞到 NPC 空间站 → 打开任务板
② 选择货运任务 → 点击接受
③ ★ 货物自动装入飞船货舱（带任务绑定，不可丢弃）
④ 查看星图 → 飞往目的地（注意易腐计时器！）
⑤ 到达目的地 → 飞船靠港
⑥ ★ 货物自动卸下 → 任务完成 → 奖励到账
```

---

## ✅ 已有功能确认（v7.4 已覆盖）

| 功能 | 状态 | 实现位置 |
|---|---|---|
| 飞船被毁后自动失效 | ✅ | `ShipInvalidationSystem::OnShipDestroyed` |
| 飞船索赔后 30 秒消失 | ✅ | `OnShipClaimed` → `DespawnTimer=30s` |
| 残骸 10 分钟消失 | ✅ | `WreckDespawnTime=600s` |
| 飞船爆炸后自动淡出销毁 | ✅ | `FadeOutActor` + `DespawnShip` |
| 玩家死亡时装备完整保存 | ✅ | `CaptureDeathSnapshot` → `EquippedItems` |
| 玩家死亡时背包完整保存 | ✅ | `CaptureDeathSnapshot` → `InventorySlots` |
| 玩家死亡时弹药保存 | ✅ | `CaptureDeathSnapshot` → `AmmoInventory` |
| 玩家死亡时货币部分保留 | ✅ | `ApplyDeathPenalty` → 丢 5%/10% |
| 尸体立即消失 | ✅ | `CorpseLifetime=0`, `bSpawnCorpseActor=false` |
| 尸体淡出 1.5s | ✅ | `FadeOutCorpse=true`, `CorpseFadeDuration=1.5f` |
| 医院复活 100% 物品恢复 | ✅ | `RestoreInventoryToNewPawn` |
| 医院复活 80% HP | ✅ | `HospitalHealPercent=0.8f` |
| 复活无敌 5 秒 | ✅ | `RespawnInvulnerabilityTime=5f` |

---

## 📂 新增文件清单（v7.5）

| 文件 | 行数 | 功能 |
|---|---|---|
| `Public/Trade/PlayerProximityGive.h` | 233 | 玩家近距离给付系统定义 |
| `Private/Trade/PlayerProximityGive.cpp` | 600 | 给付实现（距离/视线/确认/转移/日志）|
| `Public/Trade/PlayerTradeSystem.h` | 359 | 玩家交易系统 + 税收定义 |
| `Private/Trade/PlayerTradeSystem.cpp` | 780 | 交易实现（报价/锁定/确认/税收/审计）|
| `Public/Trade/CargoMissionSystem.h` | 325 | 货运任务系统定义 |
| `Private/Trade/CargoMissionSystem.cpp` | 643 | 货运实现（接取/装船/飞行/卸船/奖励）|
| `Docs/CHANGELOG_v7.5.md` | 本文件 | 更新日志 |
| `Docs/TRADE_AND_TRANSFER.md` | 新增 | 交易/给付/货运完整设计文档 |
| `AI_CONTEXT.md` | 更新 | 新增 v7.5 模块说明 |

---

## 🔧 修改文件（v7.5）

| 文件 | 修改 |
|---|---|
| `StellarSystem.Build.cs` | +4 个宏定义（`WITH_PLAYER_TRADE` 等）+ Trade 模块依赖 |
| `VERSION.txt` | 7.5.0 |
| `README.md` | 新增交易/给付/货运章节 |
| `_FILE_MANIFEST.txt` | +9 个新文件 |
| `AI_CONTEXT.md` | 新增 §12 交易系统、§13 货运任务 |

---

## 🔒 安全设计

| 机制 | 实现 |
|---|---|
| 所有给付 RPC `WithValidation` | 距离/数量/货币三重校验 |
| 所有交易 RPC `WithValidation` | 距离/容量/货币/频率校验 |
| 所有货运 RPC `WithValidation` | 状态机校验/距离校验 |
| 距离服务端权威 | 客户端无法伪造位置 |
| 频率限制 | 给付 10次/分，交易 5次/分 |
| 审计日志 | 所有交易/给付记录到 `TaxAuditLog` |
| 视线校验 | 可选，防止穿墙交易 |
| 战斗中禁止交易 | 可选配置 `bAllowTradingWhileInCombat` |

---

## 📋 编译命令

```bash
# 客户端
make client

# 服务器
make server

# WeGame
RunUAT BuildGame -targetplatform=Win64 -configuration=Shipping -target=StellarSystemWeGame

# Android
./Mobile/Build/PackageAndroid.sh Shipping StellarSystemAndroid

# iOS (macOS)
./Mobile/Build/PackageIOS.sh Shipping StellarSystemIOS
```

---

## 📊 版本对比

| 版本 | 文件数 | C++ 行数 | 新增重点 |
|---|---|---|---|
| v7.0 | 306 | 55,800 | 军团/舰队/主权港 |
| v7.1 | 306 | 55,800 | 移动端基础 |
| v7.2 | 317 | 60,000 | 武器细分(4×18+5×23) |
| v7.3 | 340 | 62,900 | 弓弩/火箭弹/配件 |
| v7.4 | 352 | 63,500 | 货舱/给付/NPC交付/死亡快照 |
| **v7.5** | **358** | **64,200** | **玩家交易/税收/货运任务** |
