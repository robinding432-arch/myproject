#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "FactionSystem.generated.h"

class AProceduralPlanet;
class ATradeStation;

// —— 派系 ID ——
UENUM(BlueprintType)
enum class EFactionId : uint8
{
    None = 0,
    TerranEmpire,       // 地球帝国：军事强权，控制核心星系
    CrimsonPirates,     // 绯红海盗：自由劫掠，黑市贸易
    VerdantGuild,        // 翠绿商会：贸易联盟，经济主导
    VoidScholars,        // 虚空学者：科技研发，中立探索
    NomadCollective,     // 游牧部落：原住民，远古知识
    AutomatedSwarm,      // 自动蜂群：AI 机械，敌视所有
    MAX
};

// —— 派系关系 ——
UENUM(BlueprintType)
enum class EFactionRelation : uint8
{
    Ally = 0,           // 盟友：可以共享基地/贸易
    Friendly,            // 友好：正常贸易，不攻击
    Neutral,             // 中立：无交互加成
    Suspicious,          // 可疑：高关税，偶尔检查
    Hostile,             // 敌对：攻击优先级低
    AtWar               // 交战：见即攻击
};

// —— 派系声望等级 ——
UENUM(BlueprintType)
enum class EFactionRank : uint8
{
    Outcast = 0,        // 被放逐（-1000）
    Enemy,               // 敌人（-500）
    Suspicious,          // 可疑（-100）
    Neutral,             // 中立（0）
    Friendly,            // 友好（100）
    Trusted,             // 受信任（300）
    Honored,             // 受尊敬（600）
    Exalted              // 崇敬（1000）
};

// —— 单个派系定义 ——
USTRUCT(BlueprintType)
struct FFactionDef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EFactionId FactionId = EFactionId::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString DisplayName = TEXT("Unknown");

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Description = TEXT("No description.");

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor FactionColor = FLinearColor(0.5f, 0.5f, 0.5f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Motto = TEXT("");

    // 控制的核心星系/行星
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FName> ControlledSystems;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FName> ControlledPlanets;

    // 派系专属飞船蓝图（逻辑名，走资产覆盖层）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName ShipHullLogicalName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName ShipWingLogicalName;

    // 派系科技偏好（影响商店物品）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FName> TechSpecializations; // "Weapons", "Shields", "Engines", "Mining"

    // 派系态度基线（对所有人的默认关系）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EFactionRelation DefaultRelation = EFactionRelation::Neutral;

    // 派系关系矩阵（对其他派系的态度）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<EFactionId, EFactionRelation> FactionRelations;

    // 声望等级阈值
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<EFactionRank, int32> RankThresholds;

    // 派系专属任务池标签
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGameplayTagContainer QuestTags;
};

// —— 玩家对单个派系的声望 ——
USTRUCT(BlueprintType)
struct FFactionReputation
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    EFactionId FactionId = EFactionId::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    int32 ReputationPoints = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    EFactionRank CurrentRank = EFactionRank::Neutral;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float LastInteractionTime = 0.f;

    // 声望增益倍率（穿特定护甲/完成连环任务时提升）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float ReputationMultiplier = 1.f;

    // 是否为该派系通缉
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    bool bIsWanted = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float BountyAmount = 0.f;
};

// —— 派系系统管理器（GameState 子系统） ——
UCLASS(BlueprintType)
class UFactionManager : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // 初始化所有派系
    UFUNCTION(BlueprintCallable, Category = "Factions")
    void InitializeFactions();

    // 注册派系定义（从 DataAsset 加载）
    UFUNCTION(BlueprintCallable, Category = "Factions")
    void RegisterFaction(const FFactionDef& Def);

    // 获取派系定义
    UFUNCTION(BlueprintCallable, Category = "Factions")
    FFactionDef GetFactionDef(EFactionId FactionId) const;

    // 获取玩家对派系的声望
    UFUNCTION(BlueprintCallable, Category = "Factions")
    int32 GetReputation(AController* Player, EFactionId FactionId) const;

    // 修改声望
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Factions")
    void Server_ModifyReputation(AController* Player, EFactionId FactionId, int32 Delta);

    // 获取声望等级
    UFUNCTION(BlueprintCallable, Category = "Factions")
    EFactionRank GetRankForPoints(int32 Points) const;

    UFUNCTION(BlueprintCallable, Category = "Factions")
    EFactionRank GetPlayerRank(AController* Player, EFactionId FactionId) const;

    // 获取两个派系之间的关系
    UFUNCTION(BlueprintCallable, Category = "Factions")
    EFactionRelation GetFactionRelation(EFactionId A, EFactionId B) const;

    // 获取派系对玩家的态度（综合派系关系和玩家声望）
    UFUNCTION(BlueprintCallable, Category = "Factions")
    EFactionRelation GetAttitudeTowardsPlayer(AController* Player, EFactionId FactionId) const;

    // 派系间外交行动
    UFUNCTION(BlueprintCallable, Category = "Factions")
    void SetFactionRelation(EFactionId A, EFactionId B, EFactionRelation Relation);

    // 宣战/停战
    UFUNCTION(BlueprintCallable, Category = "Factions")
    void DeclareWar(EFactionId Aggressor, EFactionId Defender);

    UFUNCTION(BlueprintCallable, Category = "Factions")
    void MakePeace(EFactionId A, EFactionId B);

    // 通缉系统
    UFUNCTION(BlueprintCallable, Category = "Factions")
    void PlaceBounty(EFactionId FactionId, AController* Target, float Amount);

    UFUNCTION(BlueprintCallable, Category = "Factions")
    void ClearBounty(EFactionId FactionId, AController* Target);

    // 获取玩家所有派系声望
    UFUNCTION(BlueprintCallable, Category = "Factions")
    TArray<FFactionReputation> GetAllReputations(AController* Player) const;

    // 派系专属商店过滤（只显示该派系科技）
    UFUNCTION(BlueprintCallable, Category = "Factions")
    TArray<FName> GetFactionShopItems(EFactionId FactionId) const;

    // 动态事件
    // 派系领土扩张/收缩
    UFUNCTION(BlueprintCallable, Category = "Factions")
    void ExpandTerritory(EFactionId FactionId, FName PlanetName);

    // 派系战争状态更新（每帧调用）
    void UpdateWarfare(float DeltaTime);

    // 事件委托
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnReputationChanged, AController*, Player, EFactionId, FactionId, int32, NewPoints);
    UPROPERTY(BlueprintAssignable, Category = "Factions")
    FOnReputationChanged OnReputationChanged;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFactionRelationChanged, EFactionId, A, EFactionId, B);
    UPROPERTY(BlueprintAssignable, Category = "Factions")
    FOnFactionRelationChanged OnFactionRelationChanged;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWarDeclared, EFactionId, Aggressor, EFactionId, Defender);
    UPROPERTY(BlueprintAssignable, Category = "Factions")
    FOnWarDeclared OnWarDeclared;

private:
    // 所有派系定义
    UPROPERTY()
    TMap<EFactionId, FFactionDef> FactionDatabase;

    // 派系关系矩阵（运行时可变的）
    UPROPERTY()
    TMap<TPair<EFactionId, EFactionId>, EFactionRelation> RelationMatrix;

    // 【Fix 4】增量同步：仅同步变化的派系对
    // 全量同步 6 派系 = 36 对，增量只同步实际变化的 1~4 对
    UPROPERTY()
    TSet<TPair<EFactionId, EFactionId>> DirtyRelations;

    // 战争状态
    UPROPERTY()
    TSet<TPair<EFactionId, EFactionId>> ActiveWars;

    // 玩家声望存储（PlayerNetID → Reputation 数组）
    UPROPERTY()
    TMap<FString, TArray<FFactionReputation>> PlayerReputations;

    // 初始化默认关系矩阵
    void InitializeDefaultRelations();

    // 【Fix 4】增量同步：仅返回/同步脏的派系对
    UFUNCTION(BlueprintCallable, Category = "Factions")
    TMap<TPair<EFactionId, EFactionId>, EFactionRelation> GetDirtyRelations();

    // 清除脏标记（同步完成后调用）
    UFUNCTION(BlueprintCallable, Category = "Factions")
    void ClearDirtyRelations();

    // 计算声望变化后的等级
    void RecalculateRank(FFactionReputation& Rep);

    // 声望持久化 Key
    FString GetReputationKey(AController* Player) const;
};
