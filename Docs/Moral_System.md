# ⚖️ 道德系统说明

## 概述

道德系统追踪玩家在对话和任务中的选择倾向，影响：
- 可用对话选项
- NPC 和派系对玩家的态度修正
- 任务可用性
- 特定结局解锁

---

## 道德标签

| 标签 | 含义 | 典型来源 |
|---|---|---|
| `Moral.Honorable` | 荣誉感、正直 | 选择保护弱者、信守承诺 |
| `Moral.Ruthless` | 冷酷、实用主义 | 选择牺牲他人、背信弃义 |
| `Moral.Diplomatic` | 外交手腕 | 选择谈判、调解冲突 |
| `Moral.Pragmatic` | 务实、利益导向 | 选择最优报酬、利用规则 |
| `Moral.Savage` | 野蛮、暴力 | 选择无差别攻击、掠夺 |
| `Moral.Merciful` | 仁慈、宽恕 | 选择放过敌人、帮助陌生人 |

---

## 标签获取规则

1. **对话选择**：每个选项可附加 1~2 个道德标签
2. **任务完成方式**：同一任务的不同解法给不同标签
3. **行为追踪**：击杀平民 +`Moral.Savage`，帮助平民 +`Moral.Merciful`
4. **标签不衰减**：一旦获得永久保留（除非特殊任务"洗白"）

---

## 道德对游戏的影响

### 对话过滤

```
需要 Moral.Honorable 的选项：
  → 仅当玩家拥有该标签时显示

示例：
  [谈判] "我以荣誉起誓，会完成这任务。" → 需要 Moral.Honorable
  [威胁] "不给钱就炸了你的站。" → 需要 Moral.Ruthless 或 Moral.Savage
  [利诱] "我可以付双倍，但需要你闭嘴。" → 需要 Moral.Pragmatic
```

### 派系态度修正

| 派系 | 偏好道德 | 厌恶道德 |
|---|---|---|
| 地球帝国 | Honorable/Diplomatic | Savage/Ruthless |
| 绯红海盗 | Ruthless/Savage | Merciful/Honorable |
| 翠绿商会 | Pragmatic/Diplomatic | Savage |
| 虚空学者 | Diplomatic/Pragmatic | Savage |
| 游牧部落 | Merciful/Honorable | Ruthless |
| 自动蜂群 | （无道德概念） | （全部） |

### 任务可用性

```
"圣骑士之路" 任务链 → 需要 Moral.Honorable（等级 3+）
"暗影协议" 任务链 → 需要 Moral.Ruthless（等级 2+）
"灰色地带" 任务链 → 需要 Moral.Pragmatic（任意等级）
"野蛮征服" 任务链 → 需要 Moral.Savage（等级 3+）
```

---

## C++ API

```cpp
UQuestManagerV2* QM = GetWorld()->GetSubsystem<UQuestManagerV2>();

// 查询
FGameplayTagContainer Tags = QM->GetPlayerMoralTags(Player);

// 添加
QM->AddMoralTag(Player, FName("Moral.Honorable"));

// 在对话系统中检查
if (PlayerTags.HasTag(FGameplayTag::RequestGameplayTag(FName("Moral.Ruthless"))))
{
    // 显示威胁选项
}
```

---

## 设计建议

### 不要做"非黑即白"

最好的道德系统让玩家**纠结**：

```
海盗头目："把这批货交给帝国，或者交给我。
  交给帝国 → +50 帝国声望，+Moral.Honorable
  交给我 → +100 海盗声望，+Moral.Ruthless
  第三选项（需要 Moral.Diplomatic 3+）：
  '我帮你们谈判一个双方都能接受的条件'
  → 帝国+20，海盗+20，不获得道德标签"
```

### 道德不是"好/坏"

- **Honorable** ≠ 好（可能迂腐、不灵活）
- **Ruthless** ≠ 坏（可能高效、果断）
- **Pragmatic** = 灰色地带（最"真实"）

设计任务时让每种道德都有**合理动机**和**代价**。

---

## 后续扩展

| 功能 | 状态 | 说明 |
|---|---|---|
| 道德衰减 | 🔲 未实现 | 长时间不做选择缓慢回归中立 |
| 道德冲突任务 | 🔲 未实现 | 同时需要对立标签 → 必须选边 |
| 道德结局 | 🔲 未实现 | 终局根据标签组合解锁不同结局 |
| 道德光环 | 🔲 未实现 | 其他 NPC 的对话/价格受玩家道德影响 |
| 洗白任务 | 🔲 未实现 | 高难度任务清除 Savage 标签 |
