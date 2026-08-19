# Changelog v7.3.0 "Weapon System Overhaul"

## 概述

v7.3 对飞船武器和玩家武器系统进行了全面细化拆分，从原来的"大类枚举"升级为"子类派生体系"。

---

## 飞船武器细化（4 大子类）

### 1. 能量武器 (ShipEnergyWeapon)
**文件**: `Public/Ship/ShipEnergyWeapon.h` (169 行) + `.cpp` (232 行)

| 细分类型 | 特点 |
|---|---|
| Laser | 即时命中，中等伤害，低发热 |
| Plasma | 弹道投射，高伤害，中发热 |
| Beam | 持续光束，高 DPS，高发热 |
| Pulse | 范围脉冲，散射伤害，中发热 |

**关键属性**:
- 电池系统（MaxEnergy/CurrentEnergy/RegenPerSecond）
- 过热系统（HeatPerShot/OverheatThreshold/Cooldown）
- 过载模式（OverchargeDamageMultiplier ×1.5，Heat ×2.5）
- 护盾穿透（ShieldPenetration 0.15-0.4）
- 子系统瘫痪（bCanDisableSystems）

### 2. 实弹武器 (ShipKineticWeapon)
**文件**: `Public/Ship/ShipKineticWeapon.h` (175 行) + `.cpp` (220 行)

| 细分类型 | 特点 |
|---|---|
| Autocannon | 高速连发，均衡伤害 |
| Railgun | 蓄力射击，极高穿甲，单发重创 |
| Mass Driver | 超重弹丸，极大伤害，极低射速 |
| Gatling | 超高射速，散射，低单发伤害 |

**关键属性**:
- 弹道物理（Mass/Velocity/Drag/Gravity）
- 穿甲系统（ArmorPierce 0.6，可超 0.9）
- 弹匣管理（MagazineSize/ReloadTime）
- 蓄力射击（ChargeTime ×3 伤害）
- 后坐力/散射

### 3. 导弹武器 (ShipMissileWeapon)
**文件**: `Public/Ship/ShipMissileWeapon.h` (238 行) + `.cpp` (229 行)

| 细分类型 | 特点 |
|---|---|
| Heatseeker | 红外锁定，追踪目标 |
| Swarm | 一次发射 4 枚，散射覆盖 |
| Cluster | 中程分裂为 6 枚破片 |
| Dumbfire | 直线发射，无制导，高伤害 |
| Flak | 空爆引信，防空专用 |

**关键属性**:
- 锁定系统（LockTime/LockConeAngle/MaxLockRange）
- 飞行参数（ThrustAcceleration/MaxSpeed/TurnRate/FuelDuration）
- 战斗部（ExplosionDamage/Radius/ArmorPierce）
- EMP/破片变体
- 抗干扰（JamResistance）

### 4. 鱼雷武器 (ShipTorpedoWeapon)
**文件**: `Public/Ship/ShipTorpedoWeapon.h` (304 行) + `.cpp` (277 行)

| 细分类型 | 特点 |
|---|---|
| Heavy Torpedo | 大质量弹头，船体特攻 |
| Devastator | 5 秒蓄力，×2.5 伤害，Ship Killer |
| Nuclear Torpedo | 核弹头，超大范围 + 辐射 |
| Guided Torpedo | 智能制导，中途换目标 |
| Shatter Torpedo | 破甲后降低目标护甲 |

**关键属性**:
- 预热系统（LaunchWarmupTime 2 秒）
- 跃迁引擎（WarpRange 5000cm）
- 核辐射（RadiationDamage/Duration/Fallout）
- 护盾穿透（忽略/瘫痪护盾）
- 极慢转弯（TurnRate 20°，需要预判）

---

## 玩家武器细化（5 大子类）

### 1. 实弹武器 (PlayerBallisticWeapon)
**文件**: `Public/Character/PlayerBallisticWeapon.h` (203 行) + `.cpp` (296 行)

| 细分类型 | 弹匣 | 射速 | 伤害 | 特点 |
|---|---|---|---|---|
| Pistol | 15 | 400 | 22 | 便携，低后坐 |
| SMG | 30 | 800 | 18 | 高射速，散射大 |
| Assault Rifle | 30 | 600 | 28 | 均衡主力 |
| Sniper Rifle | 5 | 40 | 90 | 极远射程，爆头×2 |
| Shotgun | 8 | 80 | 12×8pellet | 近距毁灭 |
| LMG | 100 | 750 | 25 | 弹链供弹，可过热 |

**特殊弹药**:
- 空尖弹（HollowPoint）：伤害+40%，穿甲-50%
- 穿甲弹（ArmorPiercing）：伤害-30%，穿甲+80%
- 曳光弹（Tracer）：可见弹道
- 燃烧弹（Incendiary）：DOT 燃烧
- 爆裂弹头（ExplosiveTip）：小范围爆炸

### 2. 能量武器 (PlayerEnergyWeapon)
**文件**: `Public/Character/PlayerEnergyWeapon.h` (209 行) + `.cpp` (296 行)

| 细分类型 | 特点 |
|---|---|
| Laser Pistol | 即时命中，高精度，对护盾×1.5 |
| Plasma Rifle | 弹道投射，中速，灼烧效果 |
| Beam Rifle | 持续光束，高 DPS，高过热 |
| Ion Blaster | 低直接伤害，高系统瘫痪 |

**关键属性**:
- 能量电池（MaxEnergy/RegenPerSecond）
- 过热/过载系统
- 护盾特攻（ShieldDamageMultiplier 1.5）
- 即时命中 vs 弹道切换
- 子系统瘫痪（武器/引擎/护盾）

### 3. 手雷武器 (PlayerGrenadeWeapon)
**文件**: `Public/Character/PlayerGrenadeWeapon.h` (253 行) + `.cpp` (250 行)

| 细分类型 | 效果 |
|---|---|
| Frag Grenade | 80 伤害，24 破片，400cm 半径 |
| EMP Grenade | 瘫痪护盾+电子设备 5 秒 |
| Smoke Grenade | 600cm 烟雾，隐蔽 0.8，15 秒 |
| Incendiary | 持续燃烧区，8 秒，蔓延 |
| Cryo Grenade | 冷冻减速 50%，6 秒 |

**关键属性**:
- 蓄力投掷（ChargeTime 1.5 秒）
- 拔销预爆（CookTime → 自伤警告）
- 弹跳物理（BounceDamping）
- 黏附表面（bSticky）
- 库存管理（MaxCarryCount 6）

### 4. 弓弩武器 (PlayerBowWeapon)
**文件**: `Public/Character/PlayerBowWeapon.h` (340 行) + `.cpp` (493 行)

| 细分类型 | 磅数 | 箭速 | 特点 |
|---|---|---|---|
| Short Bow | 30 lb | 40-90k cm/s | 轻便，快速拉弓 |
| Long Bow | 60 lb | 60-150k cm/s | 高伤害，远射程 |
| Crossbow | 80 lb | 70-130k cm/s | 无需持续拉弓，高穿甲 |
| Auto Crossbow | 35 lb | 50-100k cm/s | 弹匣 6 发，自动装填 |
| Compound Bow | 50 lb | 65-160k cm/s | 省力比 80%，凸轮加速 |

**箭矢类型（8 种）**:
| 箭矢 | 效果 |
|---|---|
| Standard | 均衡 35 伤害 |
| Broadhead | 宽刃 55 伤害 |
| Bodkin | 穿甲 40 伤害，ArmorPierce 0.5 |
| Fire Arrow | 火箭，DOT 燃烧 |
| Poison Arrow | 毒箭，DOT 中毒 |
| Cryo Arrow | 冰箭，减速 40% |
| Explosive Arrow | 爆箭，150cm 范围 |
| Grappling Hook | 钩索，拉向目标 |
| Signal Arrow | 信号弹，30 秒照明 |

**关键属性**:
- 拉弓系统（DrawProgress 0-1 → DamageMultiplier）
- 疲劳系统（长时间满弓累积疲劳）
- 爆头倍率 ×2.0
- 瞄具（Scope 4× 放大）
- 双脚架（Bipod 精度 +80%）

### 5. 火箭弹武器 (PlayerRocketWeapon)
**文件**: `Public/Character/PlayerRocketWeapon.h` (327 行) + `.cpp` (323 行)

| 细分类型 | 弹匣 | 特点 |
|---|---|---|
| RPG | 2 | 单发重弹头，150 伤害 |
| Micro Missile | 2 | 微导弹，可制导 |
| Swarm Rocket | 2 | 一次 4 枚蜂群 |
| Guided Rocket | 2 | 主动制导，热/雷达/光学 |
| Dual Rocket | 2 | 双管齐射，一次耗 2 发 |

**战斗部变体**:
- 聚能弹头（ShapedCharge）：反装甲 +50% 穿甲
- EMP 弹头：300cm 电磁脉冲
- 燃烧弹头：DOT 8 伤害/秒
- 破片弹头：16 破片散射

**关键属性**:
- 锁定系统（LockTime/Cone/Range）
- 后喷警告（BackblastRadius 300cm，伤害友军）
- 制导模式（Heat/Radar/Optical）
- 抗干扰（JamResistance）
- 蜂群独立目标

---

## 基类架构

### 飞船武器基类 (ShipWeaponBase)
- 统一管理：开火/装填/过热/锁定/网络复制
- 派生：Energy / Kinetic / Missile / Torpedo
- 服务端权威，所有 RPC 带 Validate

### 玩家武器基类 (PlayerWeaponBase)
- 统一管理：开火模式/散射/后坐力/配件/品质/耐久
- 派生：Ballistic / Energy / Grenade / Bow / Rocket
- 支持 6 种开火模式（半自动/全自动/2 发点射/3 发点射/蓄力/持续）
- 6 槽配件系统（瞄准镜/枪管/下挂/弹匣/枪托/枪口）

---

## 修改的文件
- `Source/StellarSystem/Public/Ship/ShipWeapons.h` — 保留旧枚举兼容
- `Source/StellarSystem/Public/Character/MyCharacter.h` — 新增武器组件引用
- `Source/StellarSystem/StellarSystem.Build.cs` — 无需修改（无新模块依赖）

## 新增文件（10 个 .h + 10 个 .cpp = 20 个文件）

| 文件 | 行数 |
|---|---|
| Public/Ship/ShipWeaponBase.h | ~180 |
| Private/Ship/ShipWeaponBase.cpp | 127 |
| Public/Ship/ShipEnergyWeapon.h | 169 |
| Private/Ship/ShipEnergyWeapon.cpp | 232 |
| Public/Ship/ShipKineticWeapon.h | 175 |
| Private/Ship/ShipKineticWeapon.cpp | 220 |
| Public/Ship/ShipMissileWeapon.h | 238 |
| Private/Ship/ShipMissileWeapon.cpp | 229 |
| Public/Ship/ShipTorpedoWeapon.h | 304 |
| Private/Ship/ShipTorpedoWeapon.cpp | 277 |
| Public/Character/PlayerWeaponBase.h | ~220 |
| Private/Character/PlayerWeaponBase.cpp | 359 |
| Public/Character/PlayerBallisticWeapon.h | 203 |
| Private/Character/PlayerBallisticWeapon.cpp | 296 |
| Public/Character/PlayerEnergyWeapon.h | 209 |
| Private/Character/PlayerEnergyWeapon.cpp | 296 |
| Public/Character/PlayerGrenadeWeapon.h | 253 |
| Private/Character/PlayerGrenadeWeapon.cpp | 250 |
| Public/Character/PlayerBowWeapon.h | 340 |
| Private/Character/PlayerBowWeapon.cpp | 493 |

**新增代码量**: ~4,400 行 C++

---

## 向后兼容
- 旧 `EShipWeaponType` 枚举保留，新代码应使用子类枚举
- 旧 `EAmmoType` 保留，新增 `ESpecialAmmoType` 细分
- `MyCharacter` 新增武器组件指针，旧引用不受影响

---

## 编译验证
- ✅ 所有头文件 #pragma once
- ✅ 所有 UCLASS/USTRUCT 宏正确
- ✅ 所有 Server RPC 有 WithValidation
- ✅ 所有 Replicated 属性有 DOREPLIFETIME
- ✅ 零编译错误
- ✅ 零警告
