# 赛季与 Battle Pass 系统设计文档

> 版本：v6.3 | 状态：设计稿（待实现）

---

## 一、设计目标

1. **留存**：每 3 个月一个赛季，给玩家持续回来的理由
2. **付费**：免费轨 + 付费轨（Premium Pass），付费转化率目标 5~10%
3. **公平**：不卖数值，只卖外观 + 便利（经验加成上限 20%）
4. **内容驱动**：每个赛季围绕一个新主题星球/派系/事件

---

## 二、赛季结构

### 2.1 时间线

| 赛季 | 主题 | 持续时间 | 核心内容 |
|---|---|---|---|
| S1 | 黎明远征 | 2026.10 ~ 2027.01 | 初始赛季，7 颗星球全开放 |
| S2 | 海盗黎明 | 2027.01 ~ 2027.04 | 新增海盗派系 + 海盗船 + 劫掠玩法 |
| S3 | 深渊回响 | 2027.04 ~ 2027.07 | 新增虚空生物 + 深渊星球 + 恐怖氛围 |
| S4 | 帝国崛起 | 2027.07 ~ 2027.10 | 新增帝国阵营任务线 + 旗舰飞船 |
| S5 | ... | ... | 持续迭代 |

### 2.2 赛季数据结构

```json
{
    "seasonId": "S2",
    "displayName": "海盗黎明",
    "startDate": "2027-01-15T00:00:00Z",
    "endDate": "2027-04-15T00:00:00Z",
    "theme": "Pirate",
    "maxLevel": 100,
    "freeRewards": [...],
    "premiumRewards": [...],
    "seasonalQuests": [...],
    "seasonalStore": {...}
}
```

---

## 三、Battle Pass 设计

### 3.1 双轨系统

```
等级 1     ┌─ 免费轨：Credits ×1000, 弹药箱, 普通护甲皮肤
           └─ 付费轨：独特飞船涂装, 表情动作, 专属称号

等级 2     ┌─ 免费轨：矿石 ×5
           └─ 付费轨：武器皮肤（激光步枪·海盗金）

...

等级 50    ┌─ 免费轨：飞船组件（标准护盾）
           └─ 付费轨：🎉 赛季专属飞船「海盗王号」

...

等级 100   ┌─ 免费轨：成就徽章
           └─ 付费轨：🏆 传说级飞船「暗星」
```

### 3.2 经验值来源

| 行为 | XP |
|---|---|
| 完成任务 | 100~500 |
| PvP 击杀 | 50/次 |
| 采矿 | 10/次 |
| 贸易利润 | 1 XP / 10 Credits |
| 每日登录 | 200（首次） |
| 赛季挑战 | 1000/个 |

### 3.3 付费轨价格

| 类型 | 价格（USD） | 内容 |
|---|---|---|
| Battle Pass | $9.99 | 当季付费轨全部奖励 |
| Battle Pass + 10 级 | $14.99 | 含立即升级 10 级 |
| 季票（全年） | $29.99 | 4 个赛季全部 BP |

---

## 四、技术实现方案（代码骨架）

### 4.1 数据表结构

```cpp
// SeasonData.h（待创建）
USTRUCT()
struct FSeasonReward
{
    GENERATED_BODY()
    int32 Level;
    bool bPremiumOnly;
    FName ItemID;       // 引用商城物品
    int32 Quantity;
    FString DisplayName;
};

UCLASS()
class USeasonData : public UPrimaryDataAsset
{
    GENERATED_BODY()
    UPROPERTY() FString SeasonID;
    UPROPERTY() int32 MaxLevel = 100;
    UPROPERTY() TArray<FSeasonReward> FreeRewards;
    UPROPERTY() TArray<FSeasonReward> PremiumRewards;
    UPROPERTY() FDateTime StartDate;
    UPROPERTY() FDateTime EndDate;
};

UCLASS()
class UBattlePassComponent : public UActorComponent
{
    GENERATED_BODY()
    UFUNCTION(Server, Reliable) void ClaimReward(int32 Level, bool bPremium);
    UFUNCTION(Server, Reliable) void PurchasePremiumPass();
    UFUNCTION(BlueprintPure) int32 GetCurrentLevel() const;
    UFUNCTION(BlueprintPure) float GetLevelProgress() const;
};
```

### 4.2 服务端校验

- 所有 XP 增长走 Server RPC
- 奖励领取走 Server RPC + 防重领（已领集合存服务端）
- 付费购买走 Steam MicroTxn API → 服务端确认 → 发放

---

## 五、赛季内容生产流水线

```
1. 策划写赛季圣经（主题/叙事/玩法）
2. 美术生产赛季资产（按 AssetRegistry 命名规范）
3. 程序做赛季专属任务/事件脚本（Lua Mod）
4. QA 测试赛季完整性
5. 服务端推送赛季配置 JSON
6. 客户端自动下载赛季 Pak
7. 赛季开始 → 自动激活
```

---

## 六、反作弊注意事项

- ❌ 不卖：武器伤害加成、护甲值、飞船速度
- ✅ 可卖：涂装、特效、音效、表情、称号、便捷功能（快速旅行 CD -20%）
- 所有付费物品属性走 `ItemID` 查表，客户端无法篡改
- 服务端权威校验每次奖励领取

---

## 七、后续迭代方向

| 版本 | 功能 |
|---|---|
| S2 | 赛季成就系统（跨赛季永久成就） |
| S3 | 赛季排行榜（每周重置） |
| S4 | 赛季专属 PvP 模式（限时地图） |
| S5 | 跨赛季传承系统（老玩家专属标记） |
