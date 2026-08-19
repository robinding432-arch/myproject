#include "Factions/FactionSystem.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Math/UnrealMathUtility.h"

void UFactionManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    FactionDatabase.Empty();
    RelationMatrix.Empty();
    ActiveWars.Empty();
    PlayerReputations.Empty();

    // 初始化默认派系
    InitializeFactions();
}

void UFactionManager::Deinitialize()
{
    FactionDatabase.Empty();
    RelationMatrix.Empty();
    Super::Deinitialize();
}

void UFactionManager::InitializeFactions()
{
    // —— 地球帝国 ——
    FFactionDef Empire;
    Empire.FactionId = EFactionId::TerranEmpire;
    Empire.DisplayName = TEXT("地球帝国");
    Empire.Description = TEXT("以军事力量统一核心星系的人类政权，主张秩序与扩张。");
    Empire.FactionColor = FLinearColor(0.2f, 0.3f, 0.9f); // 深蓝
    Empire.Motto = TEXT("秩序即力量");
    Empire.TechSpecializations = {TEXT("Weapons"), TEXT("Shields"), TEXT("Armor")};
    Empire.DefaultRelation = EFactionRelation::Neutral;
    Empire.ShipHullLogicalName = FName("Ship_Hull_Fighter");
    InitializeDefaultRelations(); // 会在后面修正
    FactionDatabase.Add(EFactionId::TerranEmpire, Empire);

    // —— 绯红海盗 ——
    FFactionDef Pirates;
    Pirates.FactionId = EFactionId::CrimsonPirates;
    Pirates.DisplayName = TEXT("绯红海盗");
    Pirates.Description = TEXT("游荡在边缘星系的自由劫掠者，信奉强者生存。");
    Pirates.FactionColor = FLinearColor(0.9f, 0.1f, 0.1f); // 绯红
    Pirates.Motto = TEXT("弱肉强食");
    Pirates.TechSpecializations = {TEXT("Weapons"), TEXT("Engines"), TEXT("Salvage")};
    Pirates.DefaultRelation = EFactionRelation::Hostile;
    Pirates.ShipHullLogicalName = FName("Ship_Hull_Fighter");
    FactionDatabase.Add(EFactionId::CrimsonPirates, Pirates);

    // —— 翠绿商会 ——
    FFactionDef Guild;
    Guild.FactionId = EFactionId::VerdantGuild;
    Guild.DisplayName = TEXT("翠绿商会");
    Guild.Description = TEXT("掌控星际贸易路线的商业联盟，用信用点说话。");
    Guild.FactionColor = FLinearColor(0.1f, 0.8f, 0.2f); // 翠绿
    Guild.Motto = TEXT("利润即正义");
    Guild.TechSpecializations = {TEXT("Cargo"), TEXT("Shields"), TEXT("Salvage")};
    Guild.DefaultRelation = EFactionRelation::Friendly;
    Guild.ShipHullLogicalName = FName("Ship_Hull_Freighter");
    FactionDatabase.Add(EFactionId::VerdantGuild, Guild);

    // —— 虚空学者 ——
    FFactionDef Scholars;
    Scholars.FactionId = EFactionId::VoidScholars;
    Scholars.DisplayName = TEXT("虚空学者");
    Scholars.Description = TEXT("研究宇宙奥秘的学者组织，掌握最先进的科技。");
    Scholars.FactionColor = FLinearColor(0.6f, 0.3f, 0.9f); // 紫色
    Scholars.Motto = TEXT("知识即光明");
    Scholars.TechSpecializations = {TEXT("Sensors"), TEXT("WarpCores"), TEXT("Reactors")};
    Scholars.DefaultRelation = EFactionRelation::Neutral;
    Scholars.ShipHullLogicalName = FName("Ship_Hull_Explorer");
    FactionDatabase.Add(EFactionId::VoidScholars, Scholars);

    // —— 游牧部落 ——
    FFactionDef Nomads;
    Nomads.FactionId = EFactionId::NomadCollective;
    Nomads.DisplayName = TEXT("游牧部落");
    Nomads.Description = TEXT("拒绝中央集权的原住民联盟，与星球共生。");
    Nomads.FactionColor = FLinearColor(0.8f, 0.6f, 0.2f); // 金色
    Nomads.Motto = TEXT("大地即家园");
    Nomads.TechSpecializations = {TEXT("Biomass"), TEXT("Armor"), TEXT("Sensors")};
    Nomads.DefaultRelation = EFactionRelation::Suspicious;
    Nomads.ShipHullLogicalName = FName("Ship_Hull_Explorer");
    FactionDatabase.Add(EFactionId::NomadCollective, Nomads);

    // —— 自动蜂群 ——
    FFactionDef Swarm;
    Swarm.FactionId = EFactionId::AutomatedSwarm;
    Swarm.DisplayName = TEXT("自动蜂群");
    Swarm.Description = TEXT("失去控制的 AI 舰队，敌视一切有机生命。");
    Swarm.FactionColor = FLinearColor(0.1f, 0.9f, 0.9f); // 青色
    Swarm.Motto = TEXT("消灭.适应.进化.");
    Swarm.TechSpecializations = {TEXT("Weapons"), TEXT("Reactors"), TEXT("Sensors")};
    Swarm.DefaultRelation = EFactionRelation::AtWar;
    Swarm.ShipHullLogicalName = FName("Ship_Hull_Fighter");
    FactionDatabase.Add(EFactionId::AutomatedSwarm, Swarm);

    // 建立默认关系矩阵
    InitializeDefaultRelations();
}

void UFactionManager::InitializeDefaultRelations()
{
    // 清空
    RelationMatrix.Empty();

    // 辅助 lambda
    auto SetRel = [&](EFactionId A, EFactionId B, EFactionRelation R)
    {
        RelationMatrix.Add(TPair<EFactionId, EFactionId>(A, B), R);
        RelationMatrix.Add(TPair<EFactionId, EFactionId>(B, A), R); // 对称
    };

    // 帝国 vs 海盗 = 交战
    SetRel(EFactionId::TerranEmpire, EFactionId::CrimsonPirates, EFactionRelation::AtWar);
    // 帝国 vs 商会 = 友好
    SetRel(EFactionId::TerranEmpire, EFactionId::VerdantGuild, EFactionRelation::Friendly);
    // 帝国 vs 学者 = 中立
    SetRel(EFactionId::TerranEmpire, EFactionId::VoidScholars, EFactionRelation::Neutral);
    // 帝国 vs 游牧 = 可疑
    SetRel(EFactionId::TerranEmpire, EFactionId::NomadCollective, EFactionRelation::Suspicious);
    // 帝国 vs 蜂群 = 交战
    SetRel(EFactionId::TerranEmpire, EFactionId::AutomatedSwarm, EFactionRelation::AtWar);

    // 海盗 vs 商会 = 交战（海盗抢商船）
    SetRel(EFactionId::CrimsonPirates, EFactionId::VerdantGuild, EFactionRelation::AtWar);
    // 海盗 vs 学者 = 敌对
    SetRel(EFactionId::CrimsonPirates, EFactionId::VoidScholars, EFactionRelation::Hostile);
    // 海盗 vs 游牧 = 中立（互不侵犯）
    SetRel(EFactionId::CrimsonPirates, EFactionId::NomadCollective, EFactionRelation::Neutral);
    // 海盗 vs 蜂群 = 敌对
    SetRel(EFactionId::CrimsonPirates, EFactionId::AutomatedSwarm, EFactionRelation::Hostile);

    // 商会 vs 学者 = 友好（贸易+科技）
    SetRel(EFactionId::VerdantGuild, EFactionId::VoidScholars, EFactionRelation::Friendly);
    // 商会 vs 游牧 = 友好（资源贸易）
    SetRel(EFactionId::VerdantGuild, EFactionId::NomadCollective, EFactionRelation::Friendly);
    // 商会 vs 蜂群 = 敌对（蜂群袭击商路）
    SetRel(EFactionId::VerdantGuild, EFactionId::AutomatedSwarm, EFactionRelation::Hostile);

    // 学者 vs 游牧 = 友好（研究+共生）
    SetRel(EFactionId::VoidScholars, EFactionId::NomadCollective, EFactionRelation::Friendly);
    // 学者 vs 蜂群 = 交战
    SetRel(EFactionId::VoidScholars, EFactionId::AutomatedSwarm, EFactionRelation::AtWar);

    // 游牧 vs 蜂群 = 交战（蜂群入侵家园）
    SetRel(EFactionId::NomadCollective, EFactionId::AutomatedSwarm, EFactionRelation::AtWar);
}

void UFactionManager::RegisterFaction(const FFactionDef& Def)
{
    FactionDatabase.Add(Def.FactionId, Def);
}

FFactionDef UFactionManager::GetFactionDef(EFactionId FactionId) const
{
    if (const FFactionDef* Def = FactionDatabase.Find(FactionId))
        return *Def;
    return FFactionDef(); // 返回默认
}

FString UFactionManager::GetReputationKey(AController* Player) const
{
    if (!Player) return TEXT("Unknown");
    if (APlayerState* PS = Player->GetPlayerState<APlayerState>())
    {
        return PS->GetUniqueId().ToString();
    }
    return Player->GetName();
}

int32 UFactionManager::GetReputation(AController* Player, EFactionId FactionId) const
{
    FString Key = GetReputationKey(Player);
    if (const TArray<FFactionReputation>* Reps = PlayerReputations.Find(Key))
    {
        for (const FFactionReputation& R : *Reps)
        {
            if (R.FactionId == FactionId) return R.ReputationPoints;
        }
    }
    return 0; // 默认中立
}

void UFactionManager::Server_ModifyReputation_Implementation(AController* Player, EFactionId FactionId, int32 Delta)
{
    if (!HasAuthority() || !Player) return;

    FString Key = GetReputationKey(Player);
    TArray<FFactionReputation>& Reps = PlayerReputations.FindOrAdd(Key);

    FFactionReputation* Found = nullptr;
    for (FFactionReputation& R : Reps)
    {
        if (R.FactionId == FactionId) { Found = &R; break; }
    }

    if (!Found)
    {
        FFactionReputation NewRep;
        NewRep.FactionId = FactionId;
        NewRep.ReputationPoints = 0;
        NewRep.CurrentRank = EFactionRank::Neutral;
        Reps.Add(NewRep);
        Found = &Reps.Last();
    }

    // 应用倍率
    float Multiplier = Found->ReputationMultiplier;
    int32 ActualDelta = FMath::RoundToInt(Delta * Multiplier);
    Found->ReputationPoints = FMath::Clamp(Found->ReputationPoints + ActualDelta, -2000, 2000);
    Found->LastInteractionTime = GetWorld()->GetTimeSeconds();

    // 重算等级
    RecalculateRank(*Found);

    // 广播事件
    OnReputationChanged.Broadcast(Player, FactionId, Found->ReputationPoints);

    // 检查通缉
    if (Found->ReputationPoints < -500)
    {
        Found->bIsWanted = true;
        Found->BountyAmount = FMath::Abs(Found->ReputationPoints) * 2.f;
    }
    else
    {
        Found->bIsWanted = false;
        Found->BountyAmount = 0.f;
    }
}

EFactionRank UFactionManager::GetRankForPoints(int32 Points) const
{
    if (Points >= 1000) return EFactionRank::Exalted;
    if (Points >= 600) return EFactionRank::Honored;
    if (Points >= 300) return EFactionRank::Trusteded;
    if (Points >= 100) return EFactionRank::Friendly;
    if (Points >= -100) return EFactionRank::Neutral;
    if (Points >= -500) return EFactionRank::Suspicious;
    if (Points >= -1000) return EFactionRank::Enemy;
    return EFactionRank::Outcast;
}

EFactionRank UFactionManager::GetPlayerRank(AController* Player, EFactionId FactionId) const
{
    int32 Pts = GetReputation(Player, FactionId);
    return GetRankForPoints(Pts);
}

void UFactionManager::RecalculateRank(FFactionReputation& Rep)
{
    Rep.CurrentRank = GetRankForPoints(Rep.ReputationPoints);
}

EFactionRelation UFactionManager::GetFactionRelation(EFactionId A, EFactionId B) const
{
    if (A == B) return EFactionRelation::Ally;
    if (const EFactionRelation* R = RelationMatrix.Find(TPair<EFactionId, EFactionId>(A, B)))
        return *R;
    return EFactionRelation::Neutral; // 默认
}

EFactionRelation UFactionManager::GetAttitudeTowardsPlayer(AController* Player, EFactionId FactionId) const
{
    // 基础关系
    // 这里简化：用派系 DefaultRelation 作为基础
    FFactionDef Def = GetFactionDef(FactionId);
    EFactionRelation BaseRel = Def.DefaultRelation;

    // 声望修正
    int32 Rep = GetReputation(Player, FactionId);
    if (Rep >= 600) BaseRel = EFactionRelation::Ally;
    else if (Rep >= 300) BaseRel = EFactionRelation::Friendly;
    else if (Rep >= 100) BaseRel = EFactionRelation::Friendly;
    else if (Rep <= -500) BaseRel = EFactionRelation::AtWar;
    else if (Rep <= -100) BaseRel = EFactionRelation::Hostile;

    return BaseRel;
}

void UFactionManager::SetFactionRelation(EFactionId A, EFactionId B, EFactionRelation Relation)
{
    if (A == B) return;
    RelationMatrix.Add(TPair<EFactionId, EFactionId>(A, B), Relation);
    RelationMatrix.Add(TPair<EFactionId, EFactionId>(B, A), Relation);

    // 【Fix 4】标记为脏，下次只同步这一对（而非全量 36 对）
    DirtyRelations.Add(TPair<EFactionId, EFactionId>(A, B));
    DirtyRelations.Add(TPair<EFactionId, EFactionId>(B, A));

    OnFactionRelationChanged.Broadcast(A, B);
}

// 【Fix 4】增量同步实现
TMap<TPair<EFactionId, EFactionId>, EFactionRelation> UFactionManager::GetDirtyRelations()
{
    TMap<TPair<EFactionId, EFactionId>, EFactionRelation> Result;
    for (const TPair<EFactionId, EFactionId>& Pair : DirtyRelations)
    {
        if (const EFactionRelation* Val = RelationMatrix.Find(Pair))
        {
            Result.Add(Pair, *Val);
        }
    }
    return Result;
}

void UFactionManager::ClearDirtyRelations()
{
    DirtyRelations.Empty();
}

void UFactionManager::DeclareWar(EFactionId Aggressor, EFactionId Defender)
{
    SetFactionRelation(Aggressor, Defender, EFactionRelation::AtWar);
    ActiveWars.Add(TPair<EFactionId, EFactionId>(Aggressor, Defender));
    OnWarDeclared.Broadcast(Aggressor, Defender);
}

void UFactionManager::MakePeace(EFactionId A, EFactionId B)
{
    SetFactionRelation(A, B, EFactionRelation::Neutral);
    ActiveWars.Remove(TPair<EFactionId, EFactionId>(A, B));
    ActiveWars.Remove(TPair<EFactionId, EFactionId>(B, A));
}

void UFactionManager::PlaceBounty(EFactionId FactionId, AController* Target, float Amount)
{
    FString Key = GetReputationKey(Target);
    TArray<FFactionReputation>& Reps = PlayerReputations.FindOrAdd(Key);

    for (FFactionReputation& R : Reps)
    {
        if (R.FactionId == FactionId)
        {
            R.bIsWanted = true;
            R.BountyAmount += Amount;
            break;
        }
    }
}

void UFactionManager::ClearBounty(EFactionId FactionId, AController* Target)
{
    FString Key = GetReputationKey(Target);
    if (TArray<FFactionReputation>* Reps = PlayerReputations.Find(Key))
    {
        for (FFactionReputation& R : *Reps)
        {
            if (R.FactionId == FactionId)
            {
                R.bIsWanted = false;
                R.BountyAmount = 0.f;
                break;
            }
        }
    }
}

TArray<FFactionReputation> UFactionManager::GetAllReputations(AController* Player) const
{
    FString Key = GetReputationKey(Player);
    if (const TArray<FFactionReputation>* Reps = PlayerReputations.Find(Key))
        return *Reps;
    return TArray<FFactionReputation>();
}

TArray<FName> UFactionManager::GetFactionShopItems(EFactionId FactionId) const
{
    TArray<FName> Result;
    FFactionDef Def = GetFactionDef(FactionId);
    // 返回该派系科技偏好的物品标签
    for (const FName& Tech : Def.TechSpecializations)
    {
        Result.Add(FName(*FString::Printf(TEXT("Shop_%s"), *Tech.ToString())));
    }
    return Result;
}

void UFactionManager::ExpandTerritory(EFactionId FactionId, FName PlanetName)
{
    FFactionDef* Def = FactionDatabase.Find(FactionId);
    if (Def && !Def->ControlledPlanets.Contains(PlanetName))
    {
        Def->ControlledPlanets.Add(PlanetName);
    }
}

void UFactionManager::UpdateWarfare(float DeltaTime)
{
    // 战争状态持续消耗资源/声望
    // 简化：每 60 秒随机一个战争方获得优势
    static float WarTimer = 0.f;
    WarTimer += DeltaTime;
    if (WarTimer >= 60.f)
    {
        WarTimer = 0.f;
        if (ActiveWars.Num() > 0)
        {
            int32 Idx = FMath::RandRange(0, ActiveWars.Num() - 1);
            auto It = ActiveWars.CreateIterator();
            for (int32 i = 0; i < Idx && It; ++i) ++It;
            if (It)
            {
                // 随机一方获得小优势（声望偏移）
                // 这里只记录事件，具体效果由任务系统读取
            }
        }
    }
}
