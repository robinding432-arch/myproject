# 🤖 任务系统设计文档

## 概述

增强任务系统支持：
- **分支对话**（玩家选择影响结果）
- **道德系统**（选择累积影响后续可用选项）
- **任务链**（有序串联 + 前置条件）
- **AI 自动生成**（按派系+难度随机生成任务+对话）

**核心文件：** `QuestSystemV2.h/cpp`

---

## 一、任务类型（12 种）

| 类型 | 说明 | 典型奖励 |
|---|---|---|
| Travel | 到达某地 | 声望 +15 |
| Collect | 收集 N 个物品 | 货币 + 物品 |
| Eliminate | 消灭目标 | 货币 + 声望 |
| Mine | 采集矿石 | 货币 + 矿石 |
| Deliver | 交付物品/信息 | 货币 + 声望 |
| Hack | 骇入终端 | 解锁商店 |
| Escort | 护送 NPC | 大额货币 |
| Scan | 扫描物体/地点 | 声望 + 知识 |
| Talk | 与 NPC 对话 | 声望 + 解锁 |
| Repair | 修复设施 | 货币 + 声望 |
| Survive | 存活 N 秒 | 货币 |
| Race | 竞速/到达 | 货币 + 声望 |

---

## 二、对话树设计

### 结构

```
DialogueTree
├── TreeID
├── EntryNodeID
├── Nodes[]
│   ├── NodeID
│   ├── SpeakerLine（NPC 台词）
│   ├── PlayerResponses[]（玩家选项）
│   ├── NextNodeIDs[]（下一节点）
│   ├── ResponseConsequences[]（后果）
│   ├── OnEnterConsequence（进入后果）
│   └── bIsEndNode
└── NodeRequirements（条件节点）
```

### 后果类型

| 后果 | 效果 |
|---|---|
| ReputationChanges | 修改派系声望 |
| TriggerQuestID | 触发新任务 |
| CompleteObjectiveID | 完成目标 |
| FailQuestID | 失败任务 |
| CurrencyRewards | 获得货币 |
| ItemRewards | 获得物品 |
| MoralTags | 添加道德标签 |
| UnlockShopTag | 解锁商店 |

### 示例对话树

```
[NPC] "旅行者，我需要你的帮助。"
  → [玩家] "我接受这个任务。" → Accept 节点 → 触发任务
  → [玩家] "报酬能再高些吗？" → Negotiate 节点 → +10 声望
  → [玩家] "我没兴趣。" → Decline 节点 → -5 声望
```

---

## 三、道德系统

### 道德标签

| 标签 | 含义 | 解锁的对话选项 |
|---|---|---|
| `Moral.Honorable` | 荣誉感 | 忠诚/牺牲选项 |
| `Moral.Ruthless` | 冷酷 | 威胁/背叛选项 |
| `Moral.Diplomatic` | 外交手腕 | 谈判/妥协选项 |
| `Moral.Pragmatic` | 务实 | 交易/利用选项 |
| `Moral.Savage` | 野蛮 | 暴力/掠夺选项 |
| `Moral.Merciful` | 仁慈 | 宽恕/帮助选项 |

### 道德如何影响游戏

1. **对话选项过滤**：某些选项需要特定道德标签
2. **任务可用性**：高道德任务要求 `Moral.Honorable`
3. **NPC 反应**：高 `Ruthless` → 某些 NPC 拒绝交易
4. **派系态度修正**：`Moral.Honorable` + 帝国声望 → 额外 +20%

### 累积规则

- 每次对话选择可添加 1 个道德标签
- 同一任务链的连续选择强化标签
- 标签不衰减（角色定位固定）
- 特殊任务可"洗白/洗黑"

---

## 四、任务链设计

### 结构

```
QuestChain
├── ChainID
├── ChainName
├── QuestIDs[]（有序）
├── CurrentQuestIndex
├── CompletionRewards
├── OwningFaction
├── Prerequisites（前置 Tag）
└── RequiredMoralTags
```

### 示例：帝国新兵训练链

```
Chain: "帝国新兵训练"
  ① "报到登记" → Talk → +10 帝国声望
  ② "靶场训练" → Eliminate → +20 声望
  ③ "护送军官" → Escort → +30 声望
  ④ "首战出征" → Eliminate ×5 → +50 声望
  ⑤ "授勋仪式" → Talk → +100 声望 + 专属武器

完成奖励：解锁帝国精锐商店 + "Honored" 等级加速
```

---

## 五、AI 程序化生成

### 生成流程

```
GenerateProceduralQuest(Player, Faction, Difficulty)
  ↓
随机选类型：Mining / Combat / Diplomatic / Delivery
  ↓
按派系+难度生成目标
  ↓
按派系生成对话树（3~4 节点）
  ↓
注册到 QuestDatabase
  ↓
返回 QuestID
```

### 难度影响

| 难度 | 目标数量 | 时限 | 奖励倍率 | 敌人强度 |
|---|---|---|---|---|
| 1（简单） | ×1 | 5 分钟 | ×1.0 | 弱 |
| 2（普通） | ×2 | 8 分钟 | ×1.5 | 中 |
| 3（困难） | ×3 | 12 分钟 | ×2.0 | 强 |
| 4（极难） | ×4 | 15 分钟 | ×3.0 | 精英 |
| 5（史诗） | ×5 | 20 分钟 | ×5.0 | BOSS |

### AI 生成的对话示例

**采矿任务（难度 2，翠绿商会）：**

```
[NPC] "我们的精炼厂缺钛矿，你能去采 100 单位吗？"
  → "没问题，包在我身上。" → 接受 → +5 声望
  → "先给钱再说。" → 谈判 → +10 声望（但 NPC 不信任）
  → "没空。" → 拒绝 → -5 声望
```

**战斗任务（难度 3，帝国）：**

```
[NPC] "边缘星域出现海盗窝点，需要清除威胁。"
  → "帝国荣耀指引我。" → 接受 → +Moral.Honorable
  → "给多少？" → 谈判 → 奖励 +20%
  → "海盗？我喜欢。" → 接受 → +Moral.Ruthless
```

---

## 六、C++ API 速查

```cpp
UQuestManagerV2* QM = GetWorld()->GetSubsystem<UQuestManagerV2>();

// 注册
QM->RegisterQuest(QuestDef);
QM->RegisterQuestChain(ChainDef);
QM->RegisterDialogueTree(Tree);

// 接取
bool bCan = QM->CanAcceptQuest(QuestID, Player);
QM->Server_AcceptQuest(Player, QuestID);

// 进度
QM->UpdateObjective(Player, QuestID, ObjID, 1);
QM->CompleteObjective(Player, QuestID, ObjID);
QM->FailQuest(Player, QuestID, "超时");

// 查询
TArray<FQuestDefinition> Available = QM->GetAvailableQuests(Player);
TArray<FQuestDefinition> Active = QM->GetActiveQuests(Player);
TArray<FQuestDefinition> Done = QM->GetCompletedQuests(Player);

// 对话
FDialogueTree Tree = QM->GetDialogueTree(TreeID);
FDialogueNode Node = QM->GetDialogueNode(TreeID, NodeID);
QM->Server_SelectDialogueResponse(Player, TreeID, NodeID, ResponseIndex);

// AI 生成
FName NewQuestID = QM->GenerateProceduralQuest(Player, EFactionId::VerdantGuild, 3);

// 道德
FGameplayTagContainer Tags = QM->GetPlayerMoralTags(Player);
QM->AddMoralTag(Player, FName("Moral.Honorable"));
```

---

## 七、与派系系统联动

| 派系 | 偏好任务类型 | 对话风格 |
|---|---|---|
| 帝国 | Eliminate/Escort/Hack | 命令式/荣誉感 |
| 海盗 | Eliminate/Deliver/Mine | 威胁/贪婪 |
| 商会 | Deliver/Collect/Hack | 精明/算计 |
| 学者 | Scan/Repair/Talk | 学术/好奇 |
| 游牧 | Survive/Escort/Repair | 朴素/自然 |
| 蜂群 | ❌ 不发布任务 | ❌ 不可对话 |

---

## 八、已知限制

1. **NPC Spawn** 返回占位符，需接入 AICharacter 蓝图
2. **对话本地化** 未实现，当前硬编码
3. **道德衰减** 未实现（标签永久）
4. **任务链保存** 仅存 Index，不存中间状态
5. **AI 生成对话** 模板化，缺乏上下文记忆
