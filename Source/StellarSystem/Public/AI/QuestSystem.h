#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AI/QuestSystem.generated.h"

class AProceduralPlanet;
class AProceduralBuildings;

// —— 任务类型 ——
UENUM(BlueprintType)
enum class EQuestType : uint8
{
    Gather,           // 采集资源
    Kill,             // 击杀目标
    Deliver,          // 运送物品
    Explore,          // 探索区域
    Scan,             // 扫描异常
    Hack,             // 入侵终端
    Escort,           // 护送 NPC
    Defend,           // 防御阵地
    Mine,             // 采矿
    Research,         // 研究样本
    Diplomatic,       // 外交谈判
    Sabotage          // 破坏设施
};

// —— 任务难度 ——
UENUM(BlueprintType)
enum class EQuestDifficulty : uint8
{
    Trivial,    // ⭐
    Easy,       // ⭐⭐
    Medium,     // ⭐⭐⭐
    Hard,       // ⭐⭐⭐⭐
    Extreme,    // ⭐⭐⭐⭐⭐
    Legendary   // 💀
};

// —— 任务目标 ——
USTRUCT(BlueprintType)
struct FQuestObjective
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 RequiredAmount = 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    int32 CurrentAmount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TargetType;  // "AsteroidIron" / "ShipPirate" / etc

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector TargetLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TargetRadius = 5000.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    bool bCompleted = false;

    bool IsComplete() const { return CurrentAmount >= RequiredAmount; }
};

// —— 任务奖励 ——
USTRUCT(BlueprintType)
struct FQuestReward
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CreditsReward = 100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 PremiumReward = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ExperienceReward = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ReputationReward = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> ItemRewards;  // 物品名

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ItemRewardCounts = 1;
};

// —— 完整任务数据 ——
USTRUCT(BlueprintType)
struct FQuestData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString QuestID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Title;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EQuestType QuestType = EQuestType::Gather;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EQuestDifficulty Difficulty = EQuestDifficulty::Easy;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FQuestObjective> Objectives;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FQuestReward Rewards;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString GiverNPCName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString GiverFaction;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 RecommendedLevel = 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    bool bIsActive = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    bool bIsCompleted = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    bool bIsFailed = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TimeLimit = 0.f;  // 0 = 无限制

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    float TimeElapsed = 0.f;

    // 对话文本
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> AcceptDialogue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> ProgressDialogue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> CompleteDialogue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> FailDialogue;
};

// —— NPC 数据 ——
USTRUCT(BlueprintType)
struct FNPCData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString NPCID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Faction;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EBuildingType HomeBuildingType = EBuildingType::Habitation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> OfferedQuestIDs;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Personality;  // "Friendly"/"Gruff"/"Mysterious"/"Witty"

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> GreetingLines;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> IdleLines;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> FarewellLines;

    // 外观
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 AppearanceSeed = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsVendor = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsQuestGiver = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsHostile = false;

    // 行为
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PatrolRadius = 3000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float InteractionRange = 500.f;
};

// —— 任务系统管理器 ——
UCLASS()
class STELLARSYSTEM_API AQuestSystem : public AActor
{
    GENERATED_BODY()

public:
    AQuestSystem();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // —— AI 生成任务 ——
    UFUNCTION(BlueprintCallable, Category = "Quest|Generation")
    FQuestData GenerateQuest(int32 Seed, AProceduralPlanet* Planet,
        const FNPCData& QuestGiver);

    UFUNCTION(BlueprintCallable, Category = "Quest|Generation")
    TArray<FQuestData> GenerateQuestsForPlanet(AProceduralPlanet* Planet,
        AProceduralBuildings* Buildings, int32 Seed = 0);

    // —— AI 生成 NPC ——
    UFUNCTION(BlueprintCallable, Category = "Quest|NPC")
    FNPCData GenerateNPC(int32 Seed, const FString& Faction,
        EBuildingType HomeBuilding);

    UFUNCTION(BlueprintCallable, Category = "Quest|NPC")
    TArray<FNPCData> GenerateNPCsForPlanet(AProceduralPlanet* Planet,
        AProceduralBuildings* Buildings, int32 Seed = 0);

    // —— 对话生成 ——
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quest|Dialogue")
    FString GenerateDialogueLine(int32 Seed, const FString& Personality,
        const FString& Context, const FString& NPCName) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quest|Dialogue")
    TArray<FString> GenerateFullDialogue(int32 Seed, const FNPCData& NPC,
        const FQuestData& Quest, const FString& Context) const;

    // —— 任务管理 ——
    UFUNCTION(BlueprintCallable, Category = "Quest")
    void AcceptQuest(const FString& QuestID, APawn* Player);

    UFUNCTION(BlueprintCallable, Category = "Quest")
    void CompleteQuest(const FString& QuestID, APawn* Player);

    UFUNCTION(BlueprintCallable, Category = "Quest")
    void FailQuest(const FString& QuestID);

    UFUNCTION(BlueprintCallable, Category = "Quest")
    void UpdateObjective(const FString& QuestID, const FString& TargetType,
        int32 Amount, APawn* Player);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quest")
    const FQuestData& GetQuest(const FString& QuestID) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quest")
    TArray<FQuestData> GetActiveQuests() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quest")
    TArray<FQuestData> GetAvailableQuests(const FString& NPCID) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quest")
    TArray<FQuestData> GetCompletedQuests() const;

    // —— NPC 查询 ——
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quest|NPC")
    const FNPCData& GetNPC(const FString& NPCID) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quest|NPC")
    TArray<FNPCData> GetAllNPCs() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quest|NPC")
    TArray<FNPCData> GetNPCsByFaction(const FString& Faction) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quest|NPC")
    TArray<FNPCData> GetNPCsNearLocation(const FVector& Location, float Radius) const;

    // —— 事件 ——
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestAccepted, const FQuestData&, Quest);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestCompleted, const FQuestData&, Quest);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestFailed, const FQuestData&, Quest);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnObjectiveUpdated, const FQuestData&, Quest, int32, ObjectiveIndex);

    UPROPERTY(BlueprintAssignable, Category = "Quest|Events")
    FOnQuestAccepted OnQuestAccepted;

    UPROPERTY(BlueprintAssignable, Category = "Quest|Events")
    FOnQuestCompleted OnQuestCompleted;

    UPROPERTY(BlueprintAssignable, Category = "Quest|Events")
    FOnQuestFailed OnQuestFailed;

    UPROPERTY(BlueprintAssignable, Filter="Quest|Events")
    FOnObjectiveUpdated OnObjectiveUpdated;

    // —— 配置 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Generation")
    int32 MaxQuestsPerPlanet = 15;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Generation")
    int32 MaxNPCsPerPlanet = 20;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Generation")
    float QuestLevelVariance = 3.f;  // 玩家等级 ± 浮动

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Generation")
    TArray<FString> FactionNames = {
        TEXT("Stellar Federation"), TEXT("Crimson Syndicate"),
        TEXT("Verdant Collective"), TEXT("Void Traders"),
        TEXT("Iron Dominion"), TEXT("Nomad Clans")
    };

private:
    // 数据存储
    UPROPERTY()
    TMap<FString, FQuestData> AllQuests;

    UPROPERTY()
    TMap<FString, FNPCData> AllNPCs;

    // 玩家任务状态
    TMap<FString, TArray<FString>> PlayerActiveQuests;  // PlayerName → QuestIDs
    TMap<FString, TArray<FString>> PlayerCompletedQuests;

    // 对话模板
    TArray<FString> GreetingTemplates;
    TArray<FString> QuestAcceptTemplates;
    TArray<FString> QuestProgressTemplates;
    TArray<FString> QuestCompleteTemplates;
    TArray<FString> QuestFailTemplates;
    TArray<FString> FarewellTemplates;
    TArray<FString> IdleTemplates;

    // 初始化模板
    void InitDialogueTemplates();

    // 辅助
    FString GenerateQuestTitle(EQuestType Type, const FString& Faction, int32 Seed) const;
    FString GenerateQuestDescription(EQuestType Type, const FString& Target,
        const FString& Location, int32 Seed) const;
    EQuestDifficulty CalculateDifficulty(int32 PlayerLevel, int32 RecommendedLevel) const;
    FQuestReward GenerateRewards(EQuestDifficulty Difficulty, int32 Seed) const;
    TArray<FQuestObjective> GenerateObjectives(EQuestType Type, AProceduralPlanet* Planet,
        int32 Seed) const;
    FVector FindQuestTargetLocation(EQuestType Type, AProceduralPlanet* Planet,
        AProceduralBuildings* Buildings, int32 Seed) const;

    // 随机数
    mutable FRandomStream RandStream;
    void SeedRand(int32 Seed) const;
};
