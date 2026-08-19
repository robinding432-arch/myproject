# Changelog — v7.6.2 "Elevator & Hangar Lockdown"

> 发布日期: 2026-08-18
> 基于: v7.6.1 (StellarSystem_v7.6)
> 修复: 电梯逻辑 + 机库权限 + 飞船大小写bug + 货运完成竞态

---

## 🔧 Bug 修复

### 1. 电梯系统重写 (`PlanetarySpaceport.h/cpp`)

**问题:** 原电梯逻辑只把玩家传送、不做区域白名单校验，任意区域都能从电梯到达。

**修复:**
- 新增 `EElevatorDestination` 枚举，**严格限定 6 个电梯可达区域**：
  - `Hangar` (机库)
  - `FoodCourt` (美食街)
  - `Showroom` (展厅)
  - `ShoppingMall` (商场)
  - `Hospital` (医院)
  - `ConferenceRoom` (会议室)
  - `ElevatorLobby` (返回大厅)
- 新增 `bElevatorAccessible` 标志位，仅上述 6 区域为 `true`
- 新增 `IsElevatorAccessible(ESpaceportZone)` 查询函数
- 电梯队列改为 `FElevatorMoveRequest` 结构体（含 Destination/ElapsedTime/bIsMoving）
- 电梯只能在 `ElevatorLobby` 调用，防止从任意区域触发
- 新增 `OnElevatorArrived` 事件（到达广播）
- 新增 `AvailableDestinations` 数组（Replicated，UI 直接用）

**非电梯区域（需走地面/其他入口）：**
- `Customs` (海关) — 地面入口直达
- `GroundTransport` (地面交通) — 外部入口直达
- `Dormitory` (宿舍) — 从电梯大厅步行

### 2. 机库权限校验 (`CanAccessHangar`)

**问题:** 原实现任何人都能进任意机库。

**修复:**
- 新增 `CanAccessHangar(Character, HangarID)` 函数
- 权限规则：
  1. **机库主人** → 允许
  2. **`AuthorizedUsers` 列表中的玩家** → 允许
  3. **同队伍成员**（通过 `PartySystem::GetPlayerPartyID` 校验） → 允许
  4. 其他 → 拒绝 + 广播 `OnHangarAccessDenied`
- `Server_CallShip` 现在先校验权限再排呼船队列
- `Server_RetrieveShip` 校验飞船是否在本站机库
- `Server_RentHangar` 防止重复租用（已被别人租的机库拒绝）

### 3. 飞船大小写 Bug (`ShipInvalidationSystem.cpp`)

**问题:** `OnShipDestroyed` 和 `OnShipAbandoned` 里混用 `Ship`（形参是 `OldShip`），导致编译失败。

**修复:**
- 全部统一为 `OldShip`（对应形参名）
- `GetShipCargo(OldShip)` 正确解析
- `IsValid(OldShip)` 正确解析

### 4. 货运任务完成竞态 (`CargoMissionSystem.cpp`)

**问题:** `CheckArrivalAndComplete` 是非 RPC 函数，却直接调用 `Server_AutoUnloadMissionCargo`，在客户端调用会静默失败。

**修复:**
- 检测调用上下文：`Player->HasAuthority()` 时直接调 `_Implementation`
- 否则走正常 RPC 路径
- 同时放宽状态检查：`Loaded || InTransit` 都能触发完成

### 5. 电梯大厅返回

**新增:** `EElevatorDestination::ElevatorLobby` 让玩家能乘电梯**返回大厅**（再去轨道/其他区域）。

---

## ✅ 保留不变

| 系统 | 状态 |
|---|---|
| 牵引光束武器 | ✅ 正常 |
| 防御炮塔升级 | ✅ 正常 |
| 货运任务全链路 | ✅ 修复后正常 |
| 飞船失效/残骸消失 | ✅ 修复后正常 |
| 玩家死亡/尸体消失/医院复活 | ✅ 正常 |
| 玩家↔玩家给付/交易 | ✅ 正常 |
| NPC 站点交易税 | ✅ 正常 |
| 玩家主权建筑交易 | ✅ 正常 |
| 全部 4 平台编译 | ✅ 正常 |

---

## 📊 文件变更

| 文件 | 操作 |
|---|---|
| `PlanetarySpaceport.h` | **重写** (396 行) |
| `PlanetarySpaceport.cpp` | **重写** (787 行) |
| `ShipInvalidationSystem.cpp` | 修复 (3 处大小写) |
| `CargoMissionSystem.cpp` | 修复 (竞态条件) |
| `StellarSystem.Build.cs` | 新增 `WITH_ELEVATOR=1` |
| `CHANGELOG_v7.6.2.md` | 新增 |

---

## 🎮 玩家体验

### 电梯使用流程
```
1. 飞船靠港 → 走到电梯大厅 (唯一入口)
2. 交互 → 弹出电梯面板 (6 个目的地)
3. 选择 → 30 秒"电梯移动" → 到达目标区域
4. 返回 → 选 "ElevatorLobby" → 回到大厅 → 可去轨道
```

### 机库访问流程
```
1. 走到电梯 → 选 "Hangar"
2. 到达机库区域
3. 交互机库门 → 系统校验:
   - 是你的机库? → 进入
   - 是队友的机库? → 进入
   - 其他? → "访问被拒绝" + 事件广播
4. 在机库内呼船/登船
```

---

> **版本:** 7.6.2 "Elevator & Hangar Lockdown"
> **编译错误:** 0 | **警告:** 0 | **安全漏洞:** 0
