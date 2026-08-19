#include "AI/QuestSystemV2.h"
#include "Factions/FactionSystem.h"
#include "Character/MyCharacter.h"
#include "GameFramework/Controller.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/Guid.h"

void UQuestManagerV2::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    QuestDatabase.Empty();
    ChainDatabase.Empty();
    DialogueDatabase.Empty();
    PlayerQuestStates.Empty();
    PlayerCompletedQuests.Empty();
    PlayerMoralTags.Empty();
    PlayerActiveChains.Empty();
}

void UQuestManagerV2::Deinitialize()
{
    QuestDatabase.Empty();
    ChainDatabase.Empty();
    DialogueDatabase.Empty();
    Super::Deinitialize();
}

void UQuestManagerV2::Tick(float DeltaTime)
{
    // 更新所有活跃任务的计时器
    TArray<FString> PlayersToUpdate;
    PlayerQuestStates.GetKeys(PlayersToUpdate);

    for (const FString& Key : PlayersToUpdate)
    {
        TArray<FPlayerQuestState>* States = PlayerQuestStates.Find(Key);
        if (!States) continue;

        for (FPlayerQuestState& State : *States)
        {
            if (State.TimeRemaining > 0.f)
            {
                State.TimeRemaining -= DeltaTime;
                if (State.TimeRemaining <= 0.f)
                {
                    // 超时失败
                    // 需要通过 Key 找到 Controller
                    // 简化：直接标记失败
                }
            }
        }
    }
}

// ===== 注册 =====

void UQuestManagerV2::RegisterQuest(const FQuestDefinition& Quest)
{
    QuestDatabase.Add(Quest.QuestID, Quest);
}

void UQuestManagerV2::RegisterQuestChain(const FQuestChain& Chain)
{
    ChainDatabase.Add(Chain.ChainID, Chain);
}

void UQuestManagerV2::RegisterDialogueTree(const FDialogueTree& Tree)
{
    DialogueDatabase.Add(Tree.TreeID, Tree);
}

// ===== 接取 =====

bool UQuestManagerV2::CanAcceptQuest(FName QuestID, AController* Player) const
{
    const FQuestDefinition* Quest = QuestDatabase.Find(QuestID);
    if (!Quest) return false;
    if (Player && !CheckQuestPrerequisites(Player, *Quest)) return false;

    // 检查互斥
    for (const FName& ExclusiveID : Quest->MutuallyExclusiveQuests)
    {
        if (const TArray<FName>* Completed = PlayerCompletedQuests.Find(GetPlayerKey(Player)))
        {
            if (Completed->Contains(ExclusiveID)) return false;
        }
    }

    // 检查是否已接取
    if (const TArray<FPlayerQuestState>* States = PlayerQuestStates.Find(GetPlayerKey(Player)))
    {
        for (const FPlayerQuestState& S : *States)
            if (S.QuestID == QuestID) return false;
    }

    return true;
}

void UQuestManagerV2::Server_AcceptQuest_Implementation(AController* Player, FName QuestID)
{
    if (!HasAuthority() || !Player) return;

    if (!CanAcceptQuest(QuestID, Player)) return;

    const FQuestDefinition* QuestDef = QuestDatabase.Find(QuestID);
    if (!QuestDef) return;

    FPlayerQuestState NewState;
    NewState.QuestID = QuestID;
    NewState.Objectives = QuestDef->Objectives;
    NewState.TimeRemaining = QuestDef->TimeLimit;
    NewState.bIsTracked = true;

    FString Key = GetPlayerKey(Player);
    PlayerQuestStates.FindOrAdd(Key).Add(NewState);

    OnQuestAccepted.Broadcast(Player, QuestID, *QuestDef);
}

// ===== 进度更新 =====

void UQuestManagerV2::UpdateObjective(AController* Player, FName QuestID, FName ObjectiveID, int32 ProgressAmount)
{
    FString Key = GetPlayerKey(Player);
    TArray<FPlayerQuestState>* States = PlayerQuestStates.Find(Key);
    if (!States) return;

    for (FPlayerQuestState& State : *States)
    {
        if (State.QuestID != QuestID) continue;

        for (FQuestObjective& Obj : State.Objectives)
        {
            if (Obj.ObjectiveID != ObjectiveID) continue;
            if (Obj.bCompleted) break;

            Obj.CurrentAmount = FMath::Min(Obj.CurrentAmount + ProgressAmount, Obj.RequiredAmount);
            OnObjectiveUpdated.Broadcast(Player, QuestID, ObjectiveID, Obj.CurrentAmount);

            if (Obj.CurrentAmount >= Obj.RequiredAmount)
            {
                CompleteObjective(Player, QuestID, ObjectiveID);
            }
            break;
        }
        break;
    }
}

void UQuestManagerV2::CompleteObjective(AController* Player, FName QuestID, FName ObjectiveID)
{
    FString Key = GetPlayerKey(Player);
    TArray<FPlayerQuestState>* States = PlayerQuestStates.Find(Key);
    if (!States) return;

    for (FPlayerQuestState& State : *States)
    {
        if (State.QuestID != QuestID) continue;

        bool AllCompleted = true;
        for (FQuestObjective& Obj : State.Objectives)
        {
            if (Obj.ObjectiveID == ObjectiveID) Obj.bCompleted = true;
            if (!Obj.bCompleted) AllCompleted = false;
        }

        if (AllCompleted) CheckQuestCompletion(Player, QuestID);
        break;
    }
}

void UQuestManagerV2::CheckQuestCompletion(AController* Player, FName QuestID)
{
    const FQuestDefinition* Quest = QuestDatabase.Find(QuestID);
    if (!Quest) return;

    GrantQuestRewards(Player, *Quest);

    // 标记为完成
    FString Key = GetPlayerKey(Player);
    if (TArray<FPlayerQuestState>* States = PlayerQuestStates.Find(Key))
    {
        States->RemoveAll([QuestID](const FPlayerQuestState& S) { return S.QuestID == QuestID; });
    }
    PlayerCompletedQuests.FindOrAdd(Key).Add(QuestID);

    // 声望奖励
    UFactionManager* FactionMgr = GetWorld()->GetSubsystem<UFactionManager>();
    if (FactionMgr)
    {
        for (const auto& Pair : Quest->ReputationRewards)
        {
            FactionMgr->Server_ModifyReputation(Player, Pair.Key, Pair.Value);
        }
    }

    OnQuestCompleted.Broadcast(Player, QuestID, Quest->CurrencyRewards);

    // 处理分支任务
    for (const auto& BranchPair : Quest->BranchQuests)
    {
        // BranchPair.Key = ChoiceTag, BranchPair.Value = QuestIDs
        // 由对话系统决定走哪个分支
    }

    // 推进任务链
    for (const auto& ChainPair : PlayerActiveChains)
    {
        if (ChainPair.Key != Key) continue;
        for (FName ChainID : ChainPair.Value)
        {
            FQuestChain* Chain = ChainDatabase.Find(ChainID);
            if (!Chain) continue;
            if (Chain->QuestIDs.IsValidIndex(Chain->CurrentQuestIndex) &&
                Chain->QuestIDs[Chain->CurrentQuestIndex] == QuestID)
            {
                Chain->CurrentQuestIndex++;
                if (Chain->CurrentQuestIndex >= Chain->QuestIDs.Num())
                {
                    // 链完成 → 给链奖励
                }
            }
        }
    }
}

void UQuestManagerV2::FailQuest(AController* Player, FName QuestID, FString Reason)
{
    FString Key = GetPlayerKey(Player);
    if (TArray<FPlayerQuestState>* States = PlayerQuestStates.Find(Key))
    {
        // 找对应的 QuestDef 拿惩罚
        const FQuestDefinition* Quest = QuestDatabase.Find(QuestID);
        if (Quest)
        {
            UFactionManager* FactionMgr = GetWorld()->GetSubsystem<UFactionManager>();
            if (FactionMgr)
            {
                for (const auto& Pair : Quest->Objectives[0].FailureReputationPenalty)
                {
                    FactionMgr->Server_ModifyReputation(Player, Pair.Key, Pair.Value);
                }
            }
        }

        States->RemoveAll([QuestID](const FPlayerQuestState& S) { return S.QuestID == QuestID; });
    }

    OnQuestFailed.Broadcast(Player, QuestID, Reason);
}

// ===== 查询 =====

TArray<FQuestDefinition> UQuestManagerV2::GetAvailableQuests(AController* Player) const
{
    TArray<FQuestDefinition> Result;
    for (const auto& Pair : QuestDatabase)
    {
        if (CanAcceptQuest(Pair.Key, Player))
            Result.Add(Pair.Value);
    }
    return Result;
}

TArray<FQuestDefinition> UQuestManagerV2::GetActiveQuests(AController* Player) const
{
    TArray<FQuestDefinition> Result;
    FString Key = GetPlayerKey(Player);
    if (const TArray<FPlayerQuestState>* States = PlayerQuestStates.Find(Key))
    {
        for (const FPlayerQuestState& S : *States)
        {
            if (const FQuestDefinition* Q = QuestDatabase.Find(S.QuestID))
                Result.Add(*Q);
        }
    }
    return Result;
}

TArray<FQuestDefinition> UQuestManagerV2::GetCompletedQuests(AController* Player) const
{
    TArray<FQuestDefinition> Result;
    FString Key = GetPlayerKey(Player);
    if (const TArray<FName>* Completed = PlayerCompletedQuests.Find(Key))
    {
        for (FName QuestID : *Completed)
        {
            if (const FQuestDefinition* Q = QuestDatabase.Find(QuestID))
                Result.Add(*Q);
        }
    }
    return Result;
}

FQuestDefinition UQuestManagerV2::GetQuestDef(FName QuestID) const
{
    if (const FQuestDefinition* Q = QuestDatabase.Find(QuestID))
        return *Q;
    return FQuestDefinition();
}

TArray<FPlayerQuestState> UQuestManagerV2::GetPlayerQuestStates(AController* Player) const
{
    FString Key = GetPlayerKey(Player);
    if (const TArray<FPlayerQuestState>* States = PlayerQuestStates.Find(Key))
        return *States;
    return TArray<FPlayerQuestState>();
}

// ===== 任务链 =====

TArray<FQuestChain> UQuestManagerV2::GetAvailableChains(AController* Player) const
{
    TArray<FQuestChain> Result;
    for (const auto& Pair : ChainDatabase)
    {
        // 检查前置
        bool bCanAccept = true;
        for (FName TagName : Pair.Value.Prerequisites)
        {
            // 简化检查
        }
        if (bCanAccept) Result.Add(Pair.Value);
    }
    return Result;
}

void UQuestManagerV2::AdvanceChain(AController* Player, FName ChainID)
{
    FQuestChain* Chain = ChainDatabase.Find(ChainID);
    if (!Chain) return;

    if (Chain->QuestIDs.IsValidIndex(Chain->CurrentQuestIndex))
    {
        FName NextQuest = Chain->QuestIDs[Chain->CurrentQuestIndex];
        Server_AcceptQuest(Player, NextQuest);
    }
}

// ===== 对话系统 =====

FDialogueTree UQuestManagerV2::GetDialogueTree(FName TreeID) const
{
    if (const FDialogueTree* T = DialogueDatabase.Find(TreeID))
        return *T;
    return FDialogueTree();
}

FDialogueNode UQuestManagerV2::GetDialogueNode(FName TreeID, FName NodeID) const
{
    if (const FDialogueTree* T = DialogueDatabase.Find(TreeID))
    {
        if (const FDialogueNode* N = T->GetNode(NodeID))
            return *N;
    }
    return FDialogueNode();
}

void UQuestManagerV2::Server_SelectDialogueResponse_Implementation(
    AController* Player, FName TreeID, FName CurrentNodeID, int32 ResponseIndex)
{
    if (!HasAuthority() || !Player) return;

    const FDialogueTree* Tree = DialogueDatabase.Find(TreeID);
    if (!Tree) return;

    const FDialogueNode* CurrentNode = Tree->GetNode(CurrentNodeID);
    if (!CurrentNode) return;

    if (!CurrentNode->NextNodeIDs.IsValidIndex(ResponseIndex)) return;

    // 应用后果
    if (CurrentNode->ResponseConsequences.IsValidIndex(ResponseIndex))
    {
        ApplyDialogueConsequence(Player, CurrentNode->ResponseConsequences[ResponseIndex]);
    }

    FName NextNodeID = CurrentNode->NextNodeIDs[ResponseIndex];
    OnDialogueNodeChanged.Broadcast(Player, TreeID, NextNodeID);

    // 检查是否结束
    const FDialogueNode* NextNode = Tree->GetNode(NextNodeID);
    if (NextNode && NextNode->bIsEndNode)
    {
        // 应用进入后果
        ApplyDialogueConsequence(Player, NextNode->OnEnterConsequence);
    }
}

void UQuestManagerV2::ApplyDialogueConsequence(AController* Player, const FDialogueConsequence& Consequence)
{
    // 声望变化
    UFactionManager* FactionMgr = GetWorld()->GetSubsystem<UFactionManager>();
    if (FactionMgr)
    {
        for (const auto& Pair : Consequence.ReputationChanges)
        {
            FactionMgr->Server_ModifyReputation(Player, Pair.Key, Pair.Value);
        }
    }

    // 触发任务
    if (Consequence.TriggerQuestID != NAME_None)
    {
        Server_AcceptQuest(Player, Consequence.TriggerQuestID);
    }

    // 完成目标
    if (Consequence.CompleteObjectiveID != NAME_None)
    {
        // 遍历玩家活跃任务找到对应目标
        FString Key = GetPlayerKey(Player);
        if (TArray<FPlayerQuestState>* States = PlayerQuestStates.Find(Key))
        {
            for (FPlayerQuestState& S : *States)
            {
                for (FQuestObjective& Obj : S.Objectives)
                {
                    if (Obj.ObjectiveID == Consequence.CompleteObjectiveID)
                    {
                        CompleteObjective(Player, S.QuestID, Obj.ObjectiveID);
                        break;
                    }
                }
            }
        }
    }

    // 失败任务
    if (Consequence.FailQuestID != NAME_None)
    {
        FailQuest(Player, Consequence.FailQuestID, TEXT("Dialogue choice"));
    }

    // 货币奖励
    for (const auto& Pair : Consequence.CurrencyRewards)
    {
        // 通过 CurrencyComponent 发放
    }

    // 道德标签
    for (FName Tag : Consequence.MoralTags)
    {
        AddMoralTag(Player, Tag);
    }
}

// ===== 道德系统 =====

FGameplayTagContainer UQuestManagerV2::GetPlayerMoralTags(AController* Player) const
{
    FString Key = GetPlayerKey(Player);
    if (const FGameplayTagContainer* Tags = PlayerMoralTags.Find(Key))
        return *Tags;
    return FGameplayTagContainer();
}

void UQuestManagerV2::AddMoralTag(AController* Player, FName MoralTag)
{
    FString Key = GetPlayerKey(Player);
    FGameplayTagContainer& Tags = PlayerMoralTags.FindOrAdd(Key);
    Tags.AddTag(FGameplayTag::RequestGameplayTag(MoralTag));
}

// ===== AI 任务生成 =====

FName UQuestManagerV2::GenerateProceduralQuest(AController* Player, EFactionId FactionId, int32 DifficultyTier)
{
    // 【Fix 5】任务模板缓存：相同 (Faction, Tier, Type) 只生成一次
    int32 TypeRoll = FMath::RandRange(0, 3);
    FString TypeName;
    switch (TypeRoll)
    {
    case 0:  TypeName = TEXT("Mining"); break;
    case 1:  TypeName = TEXT("Combat"); break;
    case 2:  TypeName = TEXT("Diplomatic"); break;
    default: TypeName = TEXT("Delivery"); break;
    }

    FString CacheKey = FString::Printf(TEXT("%d_%d_%s"),
        (int32)FactionId, DifficultyTier, *TypeName);

    // 缓存命中 → 直接克隆模板（改 QuestID 即可）
    if (FQuestDefinition* Cached = QuestTemplateCache.Find(CacheKey))
    {
        ++CacheHits;
        FQuestDefinition NewQuest = *Cached;
        NewQuest.QuestID = GenerateQuestID(); // 新唯一 ID
        QuestDatabase.Add(NewQuest.QuestID, NewQuest);

        // 复用对话树（如果已存在）
        FName TreeID = FName(*FString::Printf(TEXT("Tree_%s"), *CacheKey));
        if (FDialogueTree* CachedTree = DialogueDatabase.Find(TreeID))
        {
            DialogueDatabase.Add(FName(*NewQuest.QuestID.ToString()), *CachedTree);
        }
        else
        {
            FDialogueTree Tree = GenerateNPCDialogue(FactionId, NewQuest);
            DialogueDatabase.Add(Tree.TreeID, Tree);
        }
        return NewQuest.QuestID;
    }

    // 缓存未命中 → 正常生成
    ++CacheMisses;
    FQuestDefinition NewQuest;

    switch (TypeRoll)
    {
    case 0: NewQuest = GenerateMiningQuest(Player, FactionId, DifficultyTier); break;
    case 1: NewQuest = GenerateCombatQuest(Player, FactionId, DifficultyTier); break;
    case 2: NewQuest = GenerateDiplomaticQuest(Player, FactionId, DifficultyTier); break;
    default: NewQuest = GenerateDeliveryQuest(Player, FactionId, DifficultyTier); break;
    }

    // 注册到数据库
    QuestDatabase.Add(NewQuest.QuestID, NewQuest);

    // 存入缓存（用原始模板，不带唯一 ID）
    FQuestDefinition Template = NewQuest;
    Template.QuestID = FName(*CacheKey); // 模板用稳定 Key
    QuestTemplateCache.Add(CacheKey, Template);

    // 同时生成对话树
    FDialogueTree Tree = GenerateNPCDialogue(FactionId, NewQuest);
    DialogueDatabase.Add(Tree.TreeID, Tree);

    return NewQuest.QuestID;
}

FQuestDefinition UQuestManagerV2::GenerateMiningQuest(AController* Player, EFactionId Faction, int32 Tier)
{
    FQuestDefinition Q;
    Q.QuestID = GenerateQuestID();
    Q.Title = FString::Printf(TEXT("采矿委托 T%d"), Tier);
    Q.Description = TEXT("当地采矿站需要特定矿石供应，请前往采集。");
    Q.PrimaryType = EObjectiveType::Mine;
    Q.OwningFaction = Faction;
    Q.DifficultyTier = Tier;

    // 目标：采集 N 单位矿石
    FQuestObjective Obj;
    Obj.ObjectiveID = FName("MineOre");
    Obj.Description = FText::FromString(TEXT("采集指定矿石"));
    Obj.Type = EObjectiveType::Mine;
    Obj.RequiredAmount = 50 * Tier;
    Q.Objectives.Add(Obj);

    // 奖励按难度
    Q.CurrencyRewards.Add(FName("Credits"), 500.f * Tier);
    int32 RepReward = 25 * Tier;
    Q.ReputationRewards.Add(Faction, RepReward);

    // 时限
    Q.TimeLimit = 300.f * Tier; // 5分钟 × 难度

    return Q;
}

FQuestDefinition UQuestManagerV2::GenerateCombatQuest(AController* Player, EFactionId Faction, int32 Tier)
{
    FQuestDefinition Q;
    Q.QuestID = GenerateQuestID();
    Q.Title = FString::Printf(TEXT("清除威胁 T%d"), Tier);
    Q.Description = TEXT("该区域出现敌对势力，需要清除威胁。");
    Q.PrimaryType = EObjectiveType::Eliminate;
    Q.OwningFaction = Faction;
    Q.DifficultyTier = Tier;

    FQuestObjective Obj;
    Obj.ObjectiveID = FName("KillTargets");
    Obj.Description = FText::FromString(TEXT("消灭敌对目标"));
    Obj.Type = EObjectiveType::Eliminate;
    Obj.RequiredAmount = 3 * Tier;
    Q.Objectives.Add(Obj);

    Q.CurrencyRewards.Add(FName("Credits"), 800.f * Tier);
    Q.ReputationRewards.Add(Faction, 40 * Tier);

    return Q;
}

FQuestDefinition UQuestManagerV2::GenerateDiplomaticQuest(AController* Player, EFactionId Faction, int32 Tier)
{
    FQuestDefinition Q;
    Q.QuestID = GenerateQuestID();
    Q.Title = FString::Printf(TEXT("外交斡旋 T%d"), Tier);
    Q.Description = TEXT("派系间出现分歧，需要你从中调解。");
    Q.PrimaryType = EObjectiveType::Talk;
    Q.OwningFaction = Faction;
    Q.DifficultyTier = Tier;

    FQuestObjective Obj1;
    Obj1.ObjectiveID = FName("Talk1");
    Obj1.Description = FText::FromString(TEXT("与派系代表对话"));
    Obj1.Type = EObjectiveType::Talk;
    Obj1.RequiredAmount = 1;
    Q.Objectives.Add(Obj1);

    FQuestObjective Obj2;
    Obj2.ObjectiveID = FName("DeliverMessage");
    Obj2.Description = FText::FromString(TEXT("传递外交信函"));
    Obj2.Type = EObjectiveType::Deliver;
    Obj2.RequiredAmount = 1;
    Q.Objectives.Add(Obj2);

    Q.CurrencyRewards.Add(FName("Credits"), 600.f * Tier);
    Q.ReputationRewards.Add(Faction, 50 * Tier);

    // 道德标签
    Q.MoralTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Moral.Diplomatic")));

    return Q;
}

FQuestDefinition UQuestManagerV2::GenerateDeliveryQuest(AController* Player, EFactionId Faction, int32 Tier)
{
    FQuestDefinition Q;
    Q.QuestID = GenerateQuestID();
    Q.Title = FString::Printf(TEXT("紧急运送 T%d"), Tier);
    Q.Description = TEXT("将重要物资从一站运往另一站。");
    Q.PrimaryType = EObjectiveType::Deliver;
    Q.OwningFaction = Faction;
    Q.DifficultyTier = Tier;

    FQuestObjective Obj;
    Obj.ObjectiveID = FName("DeliverCargo");
    Obj.Description = FText::FromString(TEXT("运送货物到目标站"));
    Obj.Type = EObjectiveType::Deliver;
    Obj.RequiredAmount = 1;
    Q.Objectives.Add(Obj);

    Q.CurrencyRewards.Add(FName("Credits"), 400.f * Tier);
    Q.ReputationRewards.Add(Faction, 20 * Tier);
    Q.TimeLimit = 180.f * Tier; // 时间紧迫

    return Q;
}

FDialogueTree UQuestManagerV2::GenerateNPCDialogue(EFactionId Faction, const FQuestDefinition& Quest)
{
    FDialogueTree Tree;
    Tree.TreeID = FName(*FString::Printf(TEXT("DTree_%s"), *Quest.QuestID.ToString()));
    Tree.TreeName = FString::Printf(TEXT("对话: %s"), *Quest.Title);

    // 入口节点
    FDialogueNode Entry;
    Entry.NodeID = FName("Entry");
    Entry.SpeakerLine = FText::FromString(FString::Printf(
        TEXT("旅行者，我有一个任务给你：%s"), *Quest.Description));
    Entry.SpeakerName = TEXT("NPC");

    // 分支选项
    Entry.PlayerResponses.Add(FText::FromString(TEXT("我接受这个任务。")));
    Entry.NextNodeIDs.Add(FName("Accept"));
    Entry.ResponseConsequences.Add(FDialogueConsequence()); // 默认空

    Entry.PlayerResponses.Add(FText::FromString(TEXT("报酬能再高些吗？")));
    Entry.NextNodeIDs.Add(FName("Negotiate"));
    FDialogueConsequence NegotiateConsequence;
    // 谈判成功 = 额外声望
    NegotiateConsequence.ReputationChanges.Add(Faction, 10);
    Entry.ResponseConsequences.Add(NegotiateConsequence);

    Entry.PlayerResponses.Add(FText::FromString(TEXT("我没兴趣。")));
    Entry.NextNodeIDs.Add(FName("Decline"));
    FDialogueConsequence DeclineConsequence;
    DeclineConsequence.ReputationChanges.Add(Faction, -5);
    Entry.ResponseConsequences.Add(DeclineConsequence);

    Tree.Nodes.Add(Entry);

    // 接受节点
    FDialogueNode Accept;
    Accept.NodeID = FName("Accept");
    Accept.SpeakerLine = FText::FromString(TEXT("太好了！祝你好运。"));
    Accept.bIsEndNode = true;
    Accept.OnEnterConsequence.TriggerQuestID = Quest.QuestID;
    Tree.Nodes.Add(Accept);

    // 谈判节点
    FDialogueNode Negotiate;
    Negotiate.NodeID = FName("Negotiate");
    Negotiate.SpeakerLine = FText::FromString(TEXT("好吧，我可以向上面申请加点报酬。"));
    Negotiate.bIsEndNode = true;
    Negotiate.OnEnterConsequence.ReputationChanges.Add(Faction, 5);
    Tree.Nodes.Add(Negotiate);

    // 拒绝节点
    FDialogueNode Decline;
    Decline.NodeID = FName("Decline");
    Decline.SpeakerLine = FText::FromString(TEXT("哼，那算了。下次别来求我。"));
    Decline.bIsEndNode = true;
    Tree.Nodes.Add(Decline);

    Tree.EntryNodeID = FName("Entry");
    return Tree;
}

// ===== NPC 生成 =====

AAICharacter* UQuestManagerV2::SpawnQuestNPC(FName QuestID, FVector Location, AActor* WorldContext)
{
    // 简化：返回一个占位 Actor
    // 实际应该 Spawn 一个带 AI 行为的 Character
    return nullptr;
}

// ===== 奖励发放 =====

void UQuestManagerV2::GrantQuestRewards(AController* Player, const FQuestDefinition& Quest)
{
    // 货币
    for (const auto& Pair : Quest.CurrencyRewards)
    {
        // UCurrencyComponent* Curr = ...;
        // Curr->AddCurrency(Pair.Key, Pair.Value);
    }

    // 物品
    for (const auto& Pair : Quest.ItemRewards)
    {
        // UInventoryComponent* Inv = ...;
        // Inv->AddItem(Pair.Key, Pair.Value);
    }
}

// ===== 辅助 =====

FString UQuestManagerV2::GetPlayerKey(AController* Player) const
{
    if (!Player) return TEXT("Unknown");
    if (APlayerState* PS = Player->GetPlayerState<APlayerState>())
        return PS->GetUniqueId().ToString();
    return Player->GetName();
}

bool UQuestManagerV2::CheckQuestPrerequisites(AController* Player, const FQuestDefinition& Quest) const
{
    // 检查前置任务
    for (FName PrereqID : Quest.PrerequisiteQuests)
    {
        bool bCompleted = false;
        if (const TArray<FName>* Completed = PlayerCompletedQuests.Find(GetPlayerKey(Player)))
        {
            bCompleted = Completed->Contains(PreqID);
        }
        if (!bCompleted) return false;
    }

    // 检查道德标签
    if (Quest.RequiredMoralTags.Num() > 0)
    {
        FGameplayTagContainer PlayerTags = GetPlayerMoralTags(Player);
        if (!PlayerTags.HasAll(Quest.RequiredMoralTags)) return false;
    }

    return true;
}

FName UQuestManagerV2::GenerateQuestID() const
{
    static int32 Counter = 0;
    Counter++;
    return FName(*FString::Printf(TEXT("AutoQuest_%d"), Counter));
}
