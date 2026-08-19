# ⚔️ 派系系统配置指南

## 概述

派系系统管理 6 个星际势力之间的关系、玩家声望、领土控制和经济影响。

**核心文件：**
- `FactionSystem.h/cpp` — 派系定义/关系矩阵/声望/战争
- `QuestSystemV2.h/cpp` — 派系专属任务

---

## 一、6 个派系

| ID | 名称 | 颜色 | 态度基线 | 科技偏好 | 描述 |
|---|---|---|---|---|---|
| `TerranEmpire` | 地球帝国 | 🔵 深蓝 | 中立 | Weapons/Shields/Armor | 军事强权，秩序与扩张 |
| `CrimsonPirates` | 绯红海盗 | 🔴 绯红 | 敌对 | Weapons/Engines/Salvage | 自由劫掠，弱肉强食 |
| `VerdantGuild` | 翠绿商会 | 🟢 翠绿 | 友好 | Cargo/Shields/Salvage | 贸易联盟，利润即正义 |
| `VoidScholars` | 虚空学者 | 🟣 紫色 | 中立 | Sensors/WarpCores/Reactors | 科技研发，知识即光明 |
| `NomadCollective` | 游牧部落 | 🟡 金色 | 可疑 | Biomass/Armor/Sensors | 原住民联盟，大地即家园 |
| `AutomatedSwarm` | 自动蜂群 | 🔵 青色 | 交战 | Weapons/Reactors/Sensors | AI 机械，敌视一切有机 |

---

## 二、派系关系矩阵（默认）

| | 帝国 | 海盗 | 商会 | 学者 | 游牧 | 蜂群 |
|---|---|---|---|---|---|---|
| **帝国** | — | ⚔️ 交战 | 🤝 友好 | 😐 中立 | ⚠️ 可疑 | ⚔️ 交战 |
| **海盗** | ⚔️ 交战 | — | ⚔️ 交战 | ❌ 敌对 | 😐 中立 | ❌ 敌对 |
| **商会** | 🤝 友好 | ⚔️ 交战 | — | 🤝 友好 | 🤝 友好 | ❌ 敌对 |
| **学者** | 😐 中立 | ❌ 敌对 | 🤝 友好 | — | 🤝 友好 | ⚔️ 交战 |
| **游牧** | ⚠️ 可疑 | 😐 中立 | 🤝 友好 | 🤝 友好 | — | ⚔️ 交战 |
| **蜂群** | ⚔️ 交战 | ❌ 敌对 | ❌ 敌对 | ⚔️ 交战 | ⚔️ 交战 | — |

> 🤝 盟友 😐 中立 ⚠️ 可疑 ❌ 敌对 ⚔️ 交战

---

## 三、声望系统

### 7 级声望

| 等级 | 所需声望 | 效果 |
|---|---|---|
| Outcast（放逐） | < -1000 | 所有该派系 NPC 敌对，站门口被拒 |
| Enemy（敌人） | -1000 ~ -500 | 高关税（×3），随机被攻击 |
| Suspicious（可疑） | -500 ~ -100 | 关税 ×1.5，需要证件 |
| **Neutral（中立）** | **-100 ~ +100** | **默认状态** |
| Friendly（友好） | +100 ~ +300 | 关税 ×0.8，专属商店解锁 |
| Trusted（受信任） | +300 ~ +600 | 关税 ×0.6，任务奖励 +20% |
| Honored（受尊敬） | +600 ~ +1000 | 关税 ×0.4，免费修理 |
| Exalted（崇敬） | > +1000 | 关税 ×0.2，专属飞船蓝图 |

### 声望变化来源

| 行为 | 声望变化 |
|---|---|
| 完成派系任务 | +25~+100（按难度） |
| 击败该派系敌人 | -50~-200 |
| 帮助该派系 NPC | +10~+30 |
| 对话中选择支持 | +5~+15 |
| 对话中选择反对 | -5~-15 |
| 攻击该派系飞船 | -100~-500 |
| 在派系领土采矿（无许可） | -25 |
| 完成派系连环任务链 | +200~+500 |

### 通缉系统

- 声望 < -500 → 自动通缉
- 赏金 = |声望| × 2
- 其他派系玩家可击杀你领取赏金
- 清除通缉：完成该派系高难度任务 / 支付罚款（声望 × 5 Credits）

---

## 四、配置派系

### 方式 A：代码默认（已内置）

`FactionSystem.cpp` 的 `InitializeFactions()` 已创建全部 6 个派系和关系矩阵。

### 方式 B：DataAsset 覆盖

创建 `DA_FactionOverrides.uasset`（类型 `PrimaryDataAsset`）：

```
FactionOverrides[0]:
  FactionId = TerranEmpire
  DisplayName = "地球帝国"
  Description = "以军事力量统一核心星系的人类政权"
  FactionColor = (0.2, 0.3, 0.9)
  Motto = "秩序即力量"
  TechSpecializations = ["Weapons", "Shields"]
  DefaultRelation = Neutral
  RankThresholds:
    Outcast = -1000
    Enemy = -500
    Suspicious = -100
    Neutral = 0
    Friendly = 100
    Trusted = 300
    Honored = 600
    Exalted = 1000
```

---

## 五、C++ API 速查

```cpp
// 获取派系管理器
UFactionManager* FM = GetWorld()->GetSubsystem<UFactionManager>();

// 查询声望
int32 Rep = FM->GetReputation(PlayerController, EFactionId::TerranEmpire);

// 修改声望（服务端）
FM->Server_ModifyReputation(PlayerController, EFactionId::VerdantGuild, 50);

// 查询关系
EFactionRelation Rel = FM->GetFactionRelation(
    EFactionId::TerranEmpire, EFactionId::CrimsonPirates);
// → AtWar

// 查询对玩家的态度
EFactionRelation Att = FM->GetAttitudeTowardsPlayer(
    PlayerController, EFactionId::CrimsonPirates);
// → 取决于玩家声望

// 宣战
FM->DeclareWar(EFactionId::TerranEmpire, EFactionId::CrimsonPirates);

// 获取玩家所有声望
TArray<FFactionReputation> All = FM->GetAllReputations(PlayerController);
```

---

## 六、蓝图集成

### 在 Widget 中显示声望

```
[Get Player Controller]
  → [Get World Subsystem (FactionManager)]
    → [Get All Reputations (Self)]
      → [For Each Loop]
        → [Format Text: "{Faction}: {Points} ({Rank})"]
          → [Add to List]
```

### 在 HUD 显示当前星域控制派系

```
[Get Current Planet]
  → [Get Controlling Faction]
    → [Get Faction Def]
      → [Get Display Name + Color]
        → [Set HUD Text + Color]
```

---

## 七、任务 × 派系联动

### 派系专属任务池

每个派系有 4 类专属任务：

| 派系 | 任务类型 |
|---|---|
| 帝国 | 巡逻/护航/剿匪/外交护送 |
| 海盗 | 劫掠/走私/勒索/黑市交易 |
| 商会 | 运送/采购/竞价/市场调查 |
| 学者 | 扫描/取样/解密/实验护送 |
| 游牧 | 生态恢复/驱赶入侵者/圣物寻找 |
| 蜂群 | ❌ 不发布任务（玩家只能对抗） |

### AI 自动生成派系任务

```cpp
UQuestManagerV2* QM = GetWorld()->GetSubsystem<UQuestManagerV2>();

// 为玩家在翠绿商会生成一个难度 3 的任务
FName QuestID = QM->GenerateProceduralQuest(
    PlayerController,
    EFactionId::VerdantGuild,
    3  // 难度 1~5
);
```

---

## 八、已知限制

1. **领土扩张** 仅记录数据，无视觉表现（需美术做派系旗帜/颜色覆盖）
2. **战争 AI** 仅记录状态，自动战斗需后续接入
3. **声望衰减** 未实现（建议长时间不互动缓慢回归中立）
4. **派系专属飞船** 仅定义逻辑名，需美术创建对应 Mesh
