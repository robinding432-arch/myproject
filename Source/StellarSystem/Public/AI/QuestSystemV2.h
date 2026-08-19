#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "FactionSystem.h"
#include "QuestSystemV2.generated.h"

class AAICharacter;
class UDialogueWidget;

// —— 对话选择结果对声望/任务的影响 ——
USTRUCT(BlueprintType)
struct FDialogueConsequence
{
    GENERATED_BODY()

    // 声望变化
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<EFactionId, int32> ReputationChanges;

    // 触发任务
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName TriggerQuestID = NAME_None;

    // 完成任务目标
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName CompleteObjectiveID = NAME_None;

    // 失败任务
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName FailQuestID = NAME_None;

    // 解锁商店
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName UnlockShopTag = NAME_None;

    // 获得货币
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FName, float> CurrencyRewards; // Credits/Premium/...

    // 获得物品
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FName, int32> ItemRewards;

    // 道德标记（影响后续对话可用选项）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGameplayTagContainer MoralTags; // "Moral.Honorable", "Moral.Ruthless"
};

// —— 单个对话节点 ——
USTRUCT(BlueprintType)
struct FDialogueNode
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName NodeID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText SpeakerLine; // NPC 说的台词

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SpeakerName = TEXT("Unknown");

    // 玩家可选回复
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FText> PlayerResponses;

    // 每个选项对应的下一节点
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FName> NextNodeIDs;

    // 每个选项的后果
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FDialogueConsequence> ResponseConsequences;

    // 进入此节点时触发的后果（无条件）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDialogueConsequence OnEnterConsequence;

    // 对话结束？
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsEndNode = false;

    // 语音音频事件（交给 AudioManager）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName VoiceSoundEvent = NAME_None;

    // 表情/姿态标签
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGameplayTagContainer GestureTags;
};

// —— 完整对话树 ——
USTRUCT(BlueprintType)
struct FDialogueTree
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName TreeID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TreeName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName EntryNodeID; // 对话入口

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FDialogueNode> Nodes;

    // 条件节点（根据玩家状态跳过）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FName, FGameplayTagContainer> NodeRequirements; // NodeID → 需要的 Tag

    // 获取节点
    const FDialogueNode* GetNode(FName NodeID) const
    {
        for (const FDialogueNode& N : Nodes)
            if (N.NodeID == NodeID) return &N;
        return nullptr;
    }
};

// —— 任务目标类型 ——
UENUM(BlueprintType)
enum class EObjectiveType : uint8
{
    None = 0,
    Travel,           // 到达某地
    Collect,          // 收集物品
    Eliminate,        // 消灭目标
    Mine,             // 采矿
    Deliver,          // 交付物品
    Hack,             // 骇入终端
    Escort,           // 护送 NPC
    Scan,             // 扫描物体
    Talk,             // 与 NPC 对话
    Repair,           // 修复设施
    Survive,          // 存活 N 秒
    Race,             // 竞速/到达
    Stealth,          // 潜行通过
    MAX
};

// —— 任务目标 ——
USTRUCT(BlueprintType)
struct FQuestObjective
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName ObjectiveID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EObjectiveType Type = EObjectiveType::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 RequiredAmount = 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    int32 CurrentAmount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    bool bCompleted = false;

    // 目标位置（Travel/Eliminate 用）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector TargetLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName TargetActorTag = NAME_None;

    // 所需物品（Collect/Deliver 用）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName RequiredItemID = NAME_None;

    // 时限（秒，0=无限制）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TimeLimit = 0.f;

    // 失败惩罚声望
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<EFactionId, int32> FailureReputationPenalty;
};

// —— 任务链（串联多个任务） ——
USTRUCT(BlueprintType)
struct FQuestChain
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName ChainID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ChainName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Description;

    // 有序任务列表（按顺序完成）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FName> QuestIDs;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    int32 CurrentQuestIndex = 0;

    // 链完成后奖励
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FName, float> CompletionRewards;

    // 链所属派系
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EFactionId OwningFaction = EFactionId::None;

    // 链的前置条件
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGameplayTagContainer Prerequisites;

    // 道德对齐要求
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGameplayTagContainer RequiredMoralTags;
};

// —— 完整任务定义 ——
USTRUCT(BlueprintType)
struct FQuestDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName QuestID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Title;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EObjectiveType PrimaryType = EObjectiveType::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FQuestObjective> Objectives;

    // 任务状态
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    bool bIsActive = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    bool bIsCompleted = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    bool bIsFailed = false;

    // 奖励
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FName, float> CurrencyRewards;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FName, int32> ItemRewards;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<EFactionId, int32> ReputationRewards;

    // 前置任务
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FName> PrerequisiteQuests;

    // 互斥任务（接了这个就不能接那个）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FName> MutuallyExclusiveQuests;

    // 分支任务（完成此任务后解锁的选择）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FName, TArray<FName>> BranchQuests; // ChoiceTag → QuestIDs

    // 所属派系
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EFactionId OwningFaction = EFactionId::None;

    // 任务难度（影响奖励倍率）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 DifficultyTier = 1; // 1~5

    // 道德标签（影响哪些对话选项可用）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGameplayTagContainer MoralTags;

    // 任务类型标签
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGameplayTagContainer QuestTags; // "Type.Assassination", "Type.Diplomacy"

    // 时限
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TimeLimit = 0.f; // 0 = 无限制

    // 接取对话树
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName AcceptDialogueTree = NAME_None;

    // 完成对话树
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName CompleteDialogueTree = NAME_None;

    // 失败对话树
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName FailDialogueTree = NAME_None;
};

// —— 玩家任务状态（运行时） ——
USTRUCT(BlueprintType)
struct FPlayerQuestState
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    FName QuestID;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    TArray<FQuestObjective> Objectives;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    float TimeRemaining = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    bool bIsTracked = false; // 是否在 HUD 上追踪
};

// —— 增强任务管理器 ——
UCLASS(BlueprintType)
class UQuestManagerV2 : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;

    // ===== 任务定义注册 =====
    UFUNCTION(BlueprintCallable, Category = "Quest")
    void RegisterQuest(const FQuestDefinition& Quest);

    UFUNCTION(BlueprintCallable, Category = "Quest")
    void RegisterQuestChain(const FQuestChain& Chain);

    UFUNCTION(BlueprintCallable, Category = "Quest")
    void RegisterDialogueTree(const FDialogueTree& Tree);

    // ===== 任务接取 =====
    UFUNCTION(BlueprintCallable, Category = "Quest")
    bool CanAcceptQuest(FName QuestID, AController* Player) const;

    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Quest")
    void Server_AcceptQuest(AController* Player, FName QuestID);

    // ===== 任务进度 =====
    UFUNCTION(BlueprintCallable, Category = "Quest")
    void UpdateObjective(AController* Player, FName QuestID, FName ObjectiveID, int32 ProgressAmount = 1);

    UFUNCTION(BlueprintCallable, Category = "Quest")
    void CompleteObjective(AController* Player, FName QuestID, FName ObjectiveID);

    UFUNCTION(BlueprintCallable, Category = "Quest")
    void FailQuest(AController* Player, FName QuestID, FString Reason = TEXT(""));

    // ===== 任务查询 =====
    UFUNCTION(BlueprintCallable, Category = "Quest")
    TArray<FQuestDefinition> GetAvailableQuests(AController* Player) const;

    UFUNCTION(BlueprintCallable, Category = "Quest")
    TArray<FQuestDefinition> GetActiveQuests(AController* Player) const;

    UFUNCTION(BlueprintCallable, Category = "Quest")
    TArray<FQuestDefinition> GetCompletedQuests(AController* Player) const;

    UFUNCTION(BlueprintCallable, Category = "Quest")
    FQuestDefinition GetQuestDef(FName QuestID) const;

    UFUNCTION(BlueprintCallable, Category = "Quest")
    TArray<FPlayerQuestState> GetPlayerQuestStates(AController* Player) const;

    // ===== 任务链 =====
    UFUNCTION(BlueprintCallable, Category = "Quest")
    TArray<FQuestChain> GetAvailableChains(AController* Player) const;

    UFUNCTION(BlueprintCallable, Category = "Quest")
    void AdvanceChain(AController* Player, FName ChainID);

    // ===== 对话系统 =====
    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    FDialogueTree GetDialogueTree(FName TreeID) const;

    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    FDialogueNode GetDialogueNode(FName TreeID, FName NodeID) const;

    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Dialogue")
    void Server_SelectDialogueResponse(AController* Player, FName TreeID, FName CurrentNodeID, int32 ResponseIndex);

    // ===== AI 任务生成 =====
    UFUNCTION(BlueprintCallable, Category = "Quest")
    FName GenerateProceduralQuest(AController* Player, EFactionId FactionId, int32 DifficultyTier = 1);

    // 生成 NPC + 对话
    UFUNCTION(BlueprintCallable, Category = "Quest")
    AAICharacter* SpawnQuestNPC(FName QuestID, FVector Location, AActor* WorldContext);

    // ===== 道德系统 =====
    UFUNCTION(BlueprintCallable, Category = "Quest")
    FGameplayTagContainer GetPlayerMoralTags(AController* Player) const;

    UFUNCTION(BlueprintCallable, Category = "Quest")
    void AddMoralTag(AController* Player, FName MoralTag);

    // ===== 事件 =====
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnQuestAccepted, AController*, Player, FName, QuestID, FQuestDefinition, QuestDef);
    UPROPERTY(BlueprintAssignable, Category = "Quest")
    FOnQuestAccepted OnQuestAccepted;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnQuestCompleted, AController*, Player, FName, QuestID, TMap<FName, float>, Rewards);
    UPROPERTY(BlueprintAssignable, Category = "Quest")
    FOnQuestCompleted OnQuestCompleted;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnQuestFailed, AController*, Player, FName, QuestID, FString, Reason);
    UPROPERTY(BlueprintAssignable, Category = "Quest")
    FOnQuestFailed OnQuestFailed;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnObjectiveUpdated, AController*, Player, FName, QuestID, FName, ObjectiveID, int32, Progress);
    UPROPERTY(BlueprintAssignable, Category = "Quest")
    FOnObjectiveUpdated OnObjectiveUpdated;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDialogueNodeChanged, AController*, Player, FName, TreeID, FName, NodeID);
    UPROPERTY(BlueprintAssignable, Category = "Dialogue")
    FOnDialogueNodeChanged OnDialogueNodeChanged;

private:
    // 所有任务定义
    UPROPERTY()
    TMap<FName, FQuestDefinition> QuestDatabase;

    // 所有任务链
    UPROPERTY()
    TMap<FName, FQuestChain> ChainDatabase;

    // 所有对话树
    UPROPERTY()
    TMap<FName, FDialogueTree> DialogueDatabase;

    // 玩家任务状态：PlayerNetID → QuestStates
    UPROPERTY()
    TMap<FString, TArray<FPlayerQuestState>> PlayerQuestStates;

    // 玩家已完成的任务
    UPROPERTY()
    TMap<FString, TArray<FName>> PlayerCompletedQuests;

    // 玩家道德标签
    UPROPERTY()
    TMap<FString, FGameplayTagContainer> PlayerMoralTags;

    // 活跃任务链：PlayerNetID → ActiveChains
    UPROPERTY()
    TMap<FString, TArray<FName>> PlayerActiveChains;

    // 【Fix 5】任务模板缓存：相同 (Faction, Tier, Type) 只生成一次
    // Key = FactionID_Tier_Type, Value = 已生成的 QuestDefinition
    UPROPERTY()
    TMap<FString, FQuestDefinition> QuestTemplateCache;

    // 缓存命中统计（调试用）
    int32 CacheHits = 0;
    int32 CacheMisses = 0;

    // 内部方法
    FString GetPlayerKey(AController* Player) const;
    void CheckQuestCompletion(AController* Player, FName QuestID);
    void GrantQuestRewards(AController* Player, const FQuestDefinition& Quest);
    void ApplyDialogueConsequence(AController* Player, const FDialogueConsequence& Consequence);
    bool CheckQuestPrerequisites(AController* Player, const FQuestDefinition& Quest) const;
    FName GenerateQuestID() const;
    FQuestDefinition GenerateMiningQuest(AController* Player, EFactionId Faction, int32 Tier);
    FQuestDefinition GenerateCombatQuest(AController* Player, EFactionId Faction, int32 Tier);
    FQuestDefinition GenerateDiplomaticQuest(AController* Player, EFactionId Faction, int32 Tier);
    FQuestDefinition GenerateDeliveryQuest(AController* Player, EFactionId Faction, int32 Tier);
    FDialogueTree GenerateNPCDialogue(EFactionId Faction, const FQuestDefinition& Quest);
};
