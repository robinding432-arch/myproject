#include "AI/QuestSystem.h"
#include "Planet/ProceduralPlanet.h"
#include "Planet/ProceduralBuildings.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AQuestSystem::AQuestSystem()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
}

void AQuestSystem::BeginPlay()
{
    Super::BeginPlay();
    InitDialogueTemplates();
    RandStream.Initialize(FMath::Rand());
    UE_LOG(LogTemp, Log, TEXT("[Quest] System initialized"));
}

void AQuestSystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 更新任务计时
    for (auto& Pair : AllQuests)
    {
        FQuestData& Quest = Pair.Value;
        if (Quest.bIsActive && !Quest.bIsCompleted && !Quest.bIsFailed)
        {
            if (Quest.TimeLimit > 0.f)
            {
                Quest.TimeElapsed += DeltaTime;
                if (Quest.TimeElapsed >= Quest.TimeLimit)
                {
                    FailQuest(Quest.QuestID);
                }
            }
        }
    }
}

// —— 初始化对话模板 ——

void AQuestSystem::InitDialogueTemplates()
{
    GreetingTemplates = {
        TEXT("Greetings, traveler. You look like someone who gets things done."),
        TEXT("Another wanderer? The stars must be aligning today."),
        TEXT("I've been waiting for someone like you to come along."),
        TEXT("You're not from around here, are you? Good. Fresh blood."),
        TEXT("Step closer, friend. I have a proposition for you."),
        TEXT("The dust on your boots tells me you've seen things. Listen."),
        TEXT("By the cosmos, finally! Someone who might actually help."),
        TEXT("Don't mind the weapon. Mind the offer I'm about to make."),
    };

    QuestAcceptTemplates = {
        TEXT("Excellent. The [TARGET] won't know what hit them."),
        TEXT("I knew I could count on you. Here are the details."),
        TEXT("The [FACTION] will hear of your cooperation. Go."),
        TEXT("Don't die out there. I need this done right."),
        TEXT("Take this data chip. It has everything you need."),
        TEXT("The [LOCATION] is dangerous. Watch your back."),
        TEXT("If you succeed, there's triple the pay waiting."),
        TEXT("Time is critical. The window closes in [TIME] hours."),
    };

    QuestProgressTemplates = {
        TEXT("How goes the hunt? The [TARGET] still breathing?"),
        TEXT("I'm tracking your progress. Don't disappoint me."),
        TEXT("Halfway there, and still in one piece. Impressive."),
        TEXT("The [FACTION] is asking questions. Hurry up."),
        TEXT("Sensors show movement near the objective. Be careful."),
    };

    QuestCompleteTemplates = {
        TEXT("You did it. I'll process the payment immediately."),
        TEXT("The [FACTION] thanks you, even if they'll never say it."),
        TEXT("Clean work. The intel you brought back is invaluable."),
        TEXT("I admit, I doubted you. Never again."),
        TEXT("The galaxy just became a slightly better place. Thanks."),
    };

    QuestFailTemplates = {
        TEXT("You failed. The [FACTION] won't forget this."),
        TEXT("I should have known better than to trust an outsider."),
        TEXT("The objective is compromised. Get out of there."),
        TEXT("Survive. We'll discuss your failure later."),
        TEXT("This isn't over. Regroup and try again."),
    };

    FarewellTemplates = {
        TEXT("Safe travels, wanderer. The stars watch over you."),
        TEXT("Until our paths cross again, keep your blaster warm."),
        TEXT("Don't be a stranger. Or do—strangers are less likely to get shot."),
        TEXT("May your thrusters burn bright and your shields hold strong."),
        TEXT("The cosmos is vast. Don't get lost out there."),
    };

    IdleTemplates = {
        TEXT("*polishes weapon absentmindedly*"),
        TEXT("*glances at a flickering holoscreen*"),
        TEXT("*mutters something about supply shipments*"),
        TEXT("*adjusts collar against the artificial wind*"),
        TEXT("*taps fingers on a datapad*"),
    };
}

// —— 随机工具 ——

void AQuestSystem::SeedRand(int32 Seed) const
{
    RandStream.Initialize(Seed == 0 ? FMath::Rand() : Seed);
}

FString AQuestSystem::GenerateQuestTitle(EQuestType Type, const FString& Faction, int32 Seed) const
{
    SeedRand(Seed);

    TMap<EQuestType, TArray<FString>> TitlePrefixes;
    TitlePrefixes.Add(EQuestType::Gather,    {TEXT("Harvest"), TEXT("Collect"), TEXT("Acquire"), TEXT("Salvage")});
    TitlePrefixes.Add(EQuestType::Kill,      {TEXT("Eliminate"), TEXT("Hunt"), TEXT("Terminate"), TEXT("Dispatch")});
    TitlePrefixes.Add(EQuestType::Deliver,   {TEXT("Transport"), TEXT("Deliver"), TEXT("Courer"), TEXT("Relocate")});
    TitlePrefixes.Add(EQuestType::Explore,   {TEXT("Survey"), TEXT("Chart"), TEXT("Investigate"), TEXT("Scout")});
    TitlePrefixes.Add(EQuestType::Scan,      {TEXT("Analyze"), TEXT("Scan"), TEXT("Probe"), TEXT("Examine")});
    TitlePrefixes.Add(EQuestType::Hack,      {TEXT("Infiltrate"), TEXT("Breach"), TEXT("Crack"), TEXT("Slice")});
    TitlePrefixes.Add(EQuestType::Escort,    {TEXT("Protect"), TEXT("Escort"), TEXT("Guard"), TEXT("Shepherd")});
    TitlePrefixes.Add(EQuestType::Defend,    {TEXT("Hold the Line"), TEXT("Defend"), TEXT("Fortify"), TEXT("Repel")});
    TitlePrefixes.Add(EQuestType::Mine,      {TEXT("Extract"), TEXT("Mine"), TEXT("Drill"), TEXT("Excavate")});
    TitlePrefixes.Add(EQuestType::Research,  {TEXT("Study"), TEXT("Research"), TEXT("Catalog"), TEXT("Decode")});
    TitlePrefixes.Add(EQuestType::Diplomatic,{TEXT("Negotiate"), TEXT("Mediate"), TEXT("Parley"), TEXT("Treaty")});
    TitlePrefixes.Add(EQuestType::Sabotage, {TEXT("Sabotage"), TEXT("Disable"), TEXT("Cripple"), TEXT("Undermine")});

    TArray<FString> Suffixes = {
        TEXT("the Void"), TEXT("the Forgotten"), TEXT("the Lost"), TEXT("the Iron"), 
        TEXT("the Crimson"), TEXT("the Verdant"), TEXT("the Stellar"), TEXT("the Final"),
        TEXT("Hollow"), TEXT("Nebula"), TEXT("Frontier"), TEXT("Deep")
    };

    FString Prefix = TitlePrefixes[Type][RandStream.RandRange(0, TitlePrefixes[Type].Num() - 1)];
    FString Suffix = Suffixes[RandStream.RandRange(0, Suffixes.Num() - 1)];

    return FString::Printf(TEXT("%s %s"), *Prefix, *Suffix);
}

FString AQuestSystem::GenerateQuestDescription(EQuestType Type, const FString& Target,
    const FString& Location, int32 Seed) const
{
    SeedRand(Seed);

    switch (Type)
    {
    case EQuestType::Gather:
        return FString::Printf(TEXT("Collect %s from %s. The local ecosystem has produced a bumper crop—or mutation. Either way, we need it."), *Target, *Location);
    case EQuestType::Kill:
        return FString::Printf(TEXT("Eliminate the threat at %s. %s has been terrorizing the area. Make it stop."), *Location, *Target);
    case EQuestType::Deliver:
        return FString::Printf(TEXT("Transport %s to %s. Handle with care—and don't ask what's inside."), *Target, *Location);
    case EQuestType::Explore:
        return FString::Printf(TEXT("Chart the unknown region at %s. Map anomalies, document lifeforms, and try not to die."), *Location);
    case EQuestType::Scan:
        return FString::Printf(TEXT("Scan %s at %s for unusual energy signatures. Something doesn't add up."), *Target, *Location);
    case EQuestType::Hack:
        return FString::Printf(TEXT("Breach the terminal at %s. %s is guarding secrets we need."), *Location, *Target);
    case EQuestType::Escort:
        return FString::Printf(TEXT("Escort %s safely to %s. They're a target. You're the shield."), *Target, *Location);
    case EQuestType::Defend:
        return FString::Printf(TEXT("Defend %s from incoming waves. Hold the line at all costs."), *Location);
    case EQuestType::Mine:
        return FString::Printf(TEXT("Extract %s from the asteroid field near %s. The ore is rich—and dangerous."), *Target, *Location);
    case EQuestType::Research:
        return FString::Printf(TEXT("Collect research samples from %s. The data could rewrite everything we know."), *Location);
    case EQuestType::Diplomatic:
        return FString::Printf(TEXT("Negotiate with %s at %s. Words are your weapons now."), *Target, *Location);
    case EQuestType::Sabotage:
        return FString::Printf(TEXT("Sabotage %s at %s. Leave no trace. Take no prisoners."), *Target, *Location);
    }
    return TEXT("Complete the objective.");
}

EQuestDifficulty AQuestSystem::CalculateDifficulty(int32 PlayerLevel, int32 RecommendedLevel) const
{
    int32 Diff = RecommendedLevel - PlayerLevel;
    if (Diff <= -5) return EQuestDifficulty::Trivial;
    if (Diff <= -2) return EQuestDifficulty::Easy;
    if (Diff <= 1)  return EQuestDifficulty::Medium;
    if (Diff <= 3)  return EQuestDifficulty::Hard;
    if (Diff <= 5)  return EQuestDifficulty::Extreme;
    return EQuestDifficulty::Legendary;
}

FQuestReward AQuestSystem::GenerateRewards(EQuestDifficulty Difficulty, int32 Seed) const
{
    SeedRand(Seed);
    FQuestReward Reward;

    int32 BaseCredits = 50;
    int32 Multiplier = 1;

    switch (Difficulty)
    {
    case EQuestDifficulty::Trivial:   Multiplier = 1; break;
    case EQuestDifficulty::Easy:      Multiplier = 3; break;
    case EQuestDifficulty::Medium:     Multiplier = 8; break;
    case EQuestDifficulty::Hard:      Multiplier = 20; break;
    case EQuestDifficulty::Extreme:    Multiplier = 50; break;
    case EQuestDifficulty::Legendary:  Multiplier = 150; break;
    }

    Reward.CreditsReward = BaseCredits * Multiplier + RandStream.RandRange(0, 50 * Multiplier);
    Reward.PremiumReward = (Difficulty >= EQuestDifficulty::Hard) ? RandStream.RandRange(1, 5) : 0;
    Reward.ExperienceReward = 25 * Multiplier;
    Reward.ReputationReward = (int32)Difficulty + 1;

    // 物品奖励
    TArray<FString> PossibleItems = {
        TEXT("MedKit_Advanced"), TEXT("OxygenTank_Large"), TEXT("EnergyCell_High"),
        TEXT("PlasmaCore"), TEXT("StealthModule"), TEXT("ShieldBooster"),
        TEXT("AsteroidSample_Rare"), TEXT("AlienArtifact"), TEXT("EncryptionKey"),
        TEXT("WarpCoil"), TEXT("NaniteCluster"), TEXT("VoidEssence")
    };

    if (Difficulty >= EQuestDifficulty::Medium && RandStream.FRand() < 0.6f)
    {
        Reward.ItemRewards.Add(PossibleItems[RandStream.RandRange(0, PossibleItems.Num() - 1)]);
        Reward.ItemRewardCounts = RandStream.RandRange(1, 3);
    }

    return Reward;
}

TArray<FQuestObjective> AQuestSystem::GenerateObjectives(EQuestType Type,
    AProceduralPlanet* Planet, int32 Seed) const
{
    SeedRand(Seed);
    TArray<FQuestObjective> Objectives;

    FQuestObjective Obj;
    Obj.Description = TEXT("Complete the primary objective");

    switch (Type)
    {
    case EQuestType::Gather:
        Obj.RequiredAmount = RandStream.RandRange(3, 15);
        Obj.TargetType = TEXT("PlantSample");
        Obj.Description = FString::Printf(TEXT("Gather %d samples"), Obj.RequiredAmount);
        break;
    case EQuestType::Kill:
        Obj.RequiredAmount = RandStream.RandRange(1, 5);
        Obj.TargetType = TEXT("HostileNPC");
        Obj.Description = FString::Printf(TEXT("Eliminate %d hostiles"), Obj.RequiredAmount);
        break;
    case EQuestType::Deliver:
        Obj.RequiredAmount = 1;
        Obj.TargetType = TEXT("DeliveryPoint");
        Obj.Description = TEXT("Deliver the package to the marked location");
        break;
    case EQuestType::Explore:
        Obj.RequiredAmount = RandStream.RandRange(3, 8);
        Obj.TargetType = TEXT("SurveyPoint");
        Obj.Description = FString::Printf(TEXT("Survey %d points of interest"), Obj.RequiredAmount);
        break;
    case EQuestType::Scan:
        Obj.RequiredAmount = RandStream.RandRange(2, 6);
        Obj.TargetType = TEXT("Anomaly");
        Obj.Description = FString::Printf(TEXT("Scan %d anomalies"), Obj.RequiredAmount);
        break;
    case EQuestType::Hack:
        Obj.RequiredAmount = 1;
        Obj.TargetType = TEXT("Terminal");
        Obj.Description = TEXT("Breach the secured terminal");
        break;
    case EQuestType::Escort:
        Obj.RequiredAmount = 1;
        Obj.TargetType = TEXT("VIP");
        Obj.Description = TEXT("Escort the VIP to safety");
        break;
    case EQuestType::Defend:
        Obj.RequiredAmount = RandStream.RandRange(3, 10);
        Obj.TargetType = TEXT("Wave");
        Obj.Description = FString::Printf(TEXT("Survive %d attack waves"), Obj.RequiredAmount);
        break;
    case EQuestType::Mine:
        Obj.RequiredAmount = RandStream.RandRange(10, 50);
        Obj.TargetType = TEXT("OreNode");
        Obj.Description = FString::Printf(TEXT("Extract %d units of ore"), Obj.RequiredAmount);
        break;
    case EQuestType::Research:
        Obj.RequiredAmount = RandStream.RandRange(3, 7);
        Obj.TargetType = TEXT("Sample");
        Obj.Description = FString::Printf(TEXT("Collect %d research samples"), Obj.RequiredAmount);
        break;
    case EQuestType::Diplomatic:
        Obj.RequiredAmount = 1;
        Obj.TargetType = TEXT("Diplomat");
        Obj.Description = TEXT("Reach a diplomatic agreement");
        break;
    case EQuestType::Sabotage:
        Obj.RequiredAmount = RandStream.RandRange(1, 3);
        Obj.TargetType = TEXT("TargetStructure");
        Obj.Description = FString::Printf(TEXT("Sabotage %d structures"), Obj.RequiredAmount);
        break;
    }

    Objectives.Add(Obj);
    return Objectives;
}

FVector AQuestSystem::FindQuestTargetLocation(EQuestType Type, AProceduralPlanet* Planet,
    AProceduralBuildings* Buildings, int32 Seed) const
{
    SeedRand(Seed);

    // 如果有建筑，从建筑里选
    if (Buildings)
    {
        TArray<FBuildingInstance> AllB = Buildings.GetAllBuildings();
        if (AllB.Num() > 0)
        {
            return AllB[RandStream.RandRange(0, AllB.Num() - 1)].WorldPosition;
        }
    }

    // 否则在行星表面随机
    if (Planet)
    {
        float Theta = RandStream.FRandRange(0, 2.f * PI);
        float Phi = FMath::Acos(RandStream.FRandRange(-1.f, 1.f));
        FVector Dir(FMath::Sin(Phi) * FMath::Cos(Theta),
                     FMath::Sin(Phi) * FMath::Sin(Theta),
                     FMath::Cos(Phi));
        return Planet->GetActorLocation() + Dir * 100000.f; // 近似
    }

    return FVector::ZeroVector;
}

// —— 生成任务 ——

FQuestData AQuestSystem::GenerateQuest(int32 Seed, AProceduralPlanet* Planet,
    const FNPCData& QuestGiver)
{
    SeedRand(Seed);
    if (Seed == 0) Seed = RandStream.Rand();

    FQuestData Quest;
    Quest.QuestID = FString::Printf(TEXT("QST_%08X"), Seed);
    Quest.QuestType = (EQuestType)RandStream.RandRange(0, (int32)EQuestType::Sabotage);
    Quest.GiverNPCName = QuestGiver.DisplayName;
    Quest.GiverFaction = QuestGiver.Faction;

    // 标题和描述
    Quest.Title = GenerateQuestTitle(Quest.QuestType, Quest.GiverFaction, Seed);
    FString TargetName = TEXT("the target");
    Quest.Description = GenerateQuestDescription(Quest.QuestType, TargetName,
        Planet ? Planet->GetName() : TEXT("the region"), Seed);

    // 难度
    int32 PlayerLevel = 1; // 实际应从 PlayerState 获取
    Quest.RecommendedLevel = FMath::Clamp(PlayerLevel + RandStream.RandRange(-2, 4), 1, 50);
    Quest.Difficulty = CalculateDifficulty(PlayerLevel, Quest.RecommendedLevel);

    // 目标
    Quest.Objectives = GenerateObjectives(Quest.QuestType, Planet, Seed);
    for (FQuestObjective& Obj : Quest.Objectives)
    {
        Obj.TargetLocation = FindQuestTargetLocation(Quest.QuestType, Planet, nullptr, Seed);
        Obj.TargetRadius = RandStream.FRandRange(2000.f, 10000.f);
    }

    // 奖励
    Quest.Rewards = GenerateRewards(Quest.Difficulty, Seed);

    // 时间限制（30% 概率有限时）
    if (RandStream.FRand() < 0.3f)
    {
        Quest.TimeLimit = RandStream.RandRange(300.f, 3600.f); // 5min ~ 1hour
    }

    // 对话
    Quest.AcceptDialogue = { QuestAcceptTemplates[RandStream.RandRange(0, QuestAcceptTemplates.Num()-1)] };
    Quest.ProgressDialogue = { QuestProgressTemplates[RandStream.RandRange(0, QuestProgressTemplates.Num()-1)] };
    Quest.CompleteDialogue = { QuestCompleteTemplates[RandStream.RandRange(0, QuestCompleteTemplates.Num()-1)] };
    Quest.FailDialogue = { QuestFailTemplates[RandStream.RandRange(0, QuestFailTemplates.Num()-1)] };

    // 存储
    AllQuests.Add(Quest.QuestID, Quest);

    UE_LOG(LogTemp, Log, TEXT("[Quest] Generated: %s (Type=%d, Diff=%d, Reward=%d)"),
        *Quest.Title, (int32)Quest.QuestType, (int32)Quest.Difficulty, Quest.Rewards.CreditsReward);

    return Quest;
}

TArray<FQuestData> AQuestSystem::GenerateQuestsForPlanet(AProceduralPlanet* Planet,
    AProceduralBuildings* Buildings, int32 Seed)
{
    SeedRand(Seed == 0 ? FMath::Rand() : Seed);

    TArray<FQuestData> Result;
    int32 QuestCount = RandStream.RandRange(FMath::Max(1, MaxQuestsPerPlanet / 3), MaxQuestsPerPlanet);

    // 获取该星球的 NPC
    TArray<FNPCData> PlanetNPCs;
    for (const auto& Pair : AllNPCs)
    {
        // 简化：随机分配
        PlanetNPCs.Add(Pair.Value);
    }

    for (int32 i = 0; i < QuestCount; ++i)
    {
        FNPCData Giver;
        if (PlanetNPCs.Num() > 0)
        {
            Giver = PlanetNPCs[RandStream.RandRange(0, PlanetNPCs.Num() - 1)];
        }
        else
        {
            Giver.DisplayName = TEXT("Unknown Contact");
            Giver.Faction = FactionNames[RandStream.RandRange(0, FactionNames.Num() - 1)];
        }

        FQuestData Quest = GenerateQuest(RandStream.Rand(), Planet, Giver);
        Result.Add(Quest);
    }

    UE_LOG(LogTemp, Log, TEXT("[Quest] Generated %d quests for planet"), Result.Num());
    return Result;
}

// —— 生成 NPC ——

FNPCData AQuestSystem::GenerateNPC(int32 Seed, const FString& Faction,
    EBuildingType HomeBuilding)
{
    SeedRand(Seed == 0 ? FMath::Rand() : Seed);

    FNPCData NPC;

    TArray<FString> FirstNames = {
        TEXT("Kael"), TEXT("Nyx"), TEXT("Zara"), TEXT("Rho"), TEXT("Vex"),
        TEXT("Oren"), TEXT("Lyra"), TEXT("Kade"), TEXT("Mira"), TEXT("Thor"),
        TEXT("Sera"), TEXT("Jax"), TEXT("Nia"), TEXT("Drake"), TEXT("Pia"),
        TEXT("Rex"), TEXT("Vela"), TEXT("Orin"), TEXT("Sana"), TEXT("Bran")
    };

    TArray<FString> LastNames = {
        TEXT("Voss"), TEXT("Kaida"), TEXT("Rell"), TEXT("Thorne"), TEXT("Osiris"),
        TEXT("Nox"), TEXT("Vega"), TEXT("Cael"), TEXT("Mire"), TEXT("Drake"),
        TEXT("Zane"), TEXT("Rune"), TEXT("Sola"), TEXT("Pyre"), TEXT("Glim")
    };

    TArray<FString> Personalities = { TEXT("Friendly"), TEXT("Gruff"), TEXT("Mysterious"), TEXT("Witty"), TEXT("Paranoid") };

    NPC.NPCID = FString::Printf(TEXT("NPC_%08X"), Seed);
    NPC.DisplayName = FirstNames[RandStream.RandRange(0, FirstNames.Num()-1)] + TEXT(" ") +
                      LastNames[RandStream.RandRange(0, LastNames.Num()-1)];
    NPC.Faction = Faction.IsEmpty() ? FactionNames[RandStream.RandRange(0, FactionNames.Num()-1)] : Faction;
    NPC.HomeBuildingType = HomeBuilding;
    NPC.Personality = Personalities[RandStream.RandRange(0, Personalities.Num()-1)];
    NPC.AppearanceSeed = RandStream.Rand();
    NPC.bIsVendor = RandStream.FRand() < 0.3f;
    NPC.bIsQuestGiver = RandStream.FRand() < 0.7f;
    NPC.bIsHostile = RandStream.FRand() < 0.15f;
    NPC.PatrolRadius = RandStream.FRandRange(1000.f, 5000.f);
    NPC.InteractionRange = 500.f;

    // 对话行
    NPC.GreetingLines = { GenerateDialogueLine(Seed, NPC.Personality, TEXT("Greeting"), NPC.DisplayName) };
    NPC.IdleLines = { GenerateDialogueLine(Seed + 1, NPC.Personality, TEXT("Idle"), NPC.DisplayName) };
    NPC.FarewellLines = { GenerateDialogueLine(Seed + 2, NPC.Personality, TEXT("Farewell"), NPC.DisplayName) };

    AllNPCs.Add(NPC.NPCID, NPC);
    return NPC;
}

TArray<FNPCData> AQuestSystem::GenerateNPCsForPlanet(AProceduralPlanet* Planet,
    AProceduralBuildings* Buildings, int32 Seed)
{
    SeedRand(Seed == 0 ? FMath::Rand() : Seed);

    TArray<FNPCData> Result;
    int32 NPCCount = RandStream.RandRange(FMath::Max(3, MaxNPCsPerPlanet / 3), MaxNPCsPerPlanet);

    // 从建筑类型决定 NPC 角色
    TMap<EBuildingType, FString> BuildingFactions;
    BuildingFactions.Add(EBuildingType::Habitation, TEXT(""));
    BuildingFactions.Add(EBuildingType::Industrial, TEXT("Iron Dominion"));
    BuildingFactions.Add(EBuildingType::Research, TEXT("Stellar Federation"));
    BuildingFactions.Add(EBuildingType::Military, TEXT("Crimson Syndicate"));
    BuildingFactions.Add(EBuildingType::Trade, TEXT("Void Traders"));
    BuildingFactions.Add(EBuildingType::Farm, TEXT("Verdant Collective"));

    TArray<FBuildingInstance> AllB;
    if (Buildings) AllB = Buildings.GetAllBuildings();

    for (int32 i = 0; i < NPCCount; ++i)
    {
        EBuildingType Home = EBuildingType::Habitation;
        FString Faction = TEXT("");

        if (AllB.Num() > 0)
        {
            FBuildingInstance B = AllB[RandStream.RandRange(0, AllB.Num()-1)];
            Home = B.Type;
            Faction = B.OwnerFaction;
        }

        if (Faction.IsEmpty() && BuildingFactions.Contains(Home))
        {
            Faction = BuildingFactions[Home];
        }

        FNPCData NPC = GenerateNPC(RandStream.Rand(), Faction, Home);
        Result.Add(NPC);
    }

    UE_LOG(LogTemp, Log, TEXT("[NPC] Generated %d NPCs for planet"), Result.Num());
    return Result;
}

// —— 对话生成 ——

FString AQuestSystem::GenerateDialogueLine(int32 Seed, const FString& Personality,
    const FString& Context, const FString& NPCName) const
{
    SeedRand(Seed);

    if (Context == TEXT("Greeting"))
    {
        FString Base = GreetingTemplates[RandStream.RandRange(0, GreetingTemplates.Num()-1)];
        if (Personality == TEXT("Gruff"))
            return TEXT("*gruff nod* ") + Base;
        if (Personality == TEXT("Mysterious"))
            return Base + TEXT(" *whispers*");
        if (Personality == TEXT("Witty"))
            return Base + TEXT(" *smirks*");
        if (Personality == TEXT("Paranoid"))
            return Base + TEXT(" *eyes the corridor*");
        return Base;
    }
    else if (Context == TEXT("Idle"))
    {
        return IdleTemplates[RandStream.RandRange(0, IdleTemplates.Num()-1)];
    }
    else if (Context == TEXT("Farewell"))
    {
        return FarewellTemplates[RandStream.RandRange(0, FarewellTemplates.Num()-1)];
    }

    return TEXT("...");
}

TArray<FString> AQuestSystem::GenerateFullDialogue(int32 Seed, const FNPCData& NPC,
    const FQuestData& Quest, const FString& Context) const
{
    TArray<FString> Lines;

    if (Context == TEXT("Accept"))
    {
        Lines.Add(GenerateDialogueLine(Seed, NPC.Personality, TEXT("Greeting"), NPC.DisplayName));
        for (const FString& L : Quest.AcceptDialogue) Lines.Add(L);
        Lines.Add(Quest.Description);
        Lines.Add(FString::Printf(TEXT("Reward: %d credits"), Quest.Rewards.CreditsReward));
    }
    else if (Context == TEXT("Progress"))
    {
        for (const FString& L : Quest.ProgressDialogue) Lines.Add(L);
        for (const FQuestObjective& Obj : Quest.Objectives)
        {
            Lines.Add(FString::Printf(TEXT("[%d/%d] %s"), Obj.CurrentAmount, Obj.RequiredAmount, *Obj.Description));
        }
    }
    else if (Context == TEXT("Complete"))
    {
        for (const FString& L : Quest.CompleteDialogue) Lines.Add(L);
        Lines.Add(FString::Printf(TEXT("+%d credits. +%d XP."), Quest.Rewards.CreditsReward, Quest.Rewards.ExperienceReward));
    }
    else if (Context == TEXT("Fail"))
    {
        for (const FString& L : Quest.FailDialogue) Lines.Add(L);
    }

    return Lines;
}

// —— 任务管理 ——

void AQuestSystem::AcceptQuest(const FString& QuestID, APawn* Player)
{
    if (!AllQuests.Contains(QuestID) || !Player) return;

    FQuestData& Quest = AllQuests[QuestID];
    Quest.bIsActive = true;

    FString PlayerName = Player->GetName();
    PlayerActiveQuests.FindOrAdd(PlayerName).Add(QuestID);

    UE_LOG(LogTemp, Log, TEXT("[Quest] Accepted: %s by %s"), *Quest.Title, *PlayerName);
    OnQuestAccepted.Broadcast(Quest);
}

void AQuestSystem::CompleteQuest(const FString& QuestID, APawn* Player)
{
    if (!AllQuests.Contains(QuestID) || !Player) return;

    FQuestData& Quest = AllQuests[QuestID];
    Quest.bIsActive = false;
    Quest.bIsCompleted = true;

    FString PlayerName = Player->GetName();

    // 从活跃列表移除，加入完成列表
    if (TArray<FString>* Active = PlayerActiveQuests.Find(PlayerName))
    {
        Active->Remove(QuestID);
    }
    PlayerCompletedQuests.FindOrAdd(PlayerName).Add(QuestID);

    // 发放奖励（简化：直接 log）
    UE_LOG(LogTemp, Warning, TEXT("[Quest] ★ COMPLETED: %s → +%d credits, +%d XP"),
        *Quest.Title, Quest.Rewards.CreditsReward, Quest.Rewards.ExperienceReward);

    OnQuestCompleted.Broadcast(Quest);
}

void AQuestSystem::FailQuest(const FString& QuestID)
{
    if (!AllQuests.Contains(QuestID)) return;

    FQuestData& Quest = AllQuests[QuestID];
    Quest.bIsActive = false;
    Quest.bIsFailed = true;

    UE_LOG(LogTemp, Warning, TEXT("[Quest] ✗ FAILED: %s"), *Quest.Title);
    OnQuestFailed.Broadcast(Quest);
}

void AQuestSystem::UpdateObjective(const FString& QuestID, const FString& TargetType,
    int32 Amount, APawn* Player)
{
    if (!AllQuests.Contains(QuestID)) return;

    FQuestData& Quest = AllQuests[QuestID];
    if (!Quest.bIsActive) return;

    for (int32 i = 0; i < Quest.Objectives.Num(); ++i)
    {
        FQuestObjective& Obj = Quest.Objectives[i];
        if (Obj.TargetType == TargetType && !Obj.bCompleted)
        {
            Obj.CurrentAmount = FMath::Min(Obj.CurrentAmount + Amount, Obj.RequiredAmount);
            OnObjectiveUpdated.Broadcast(Quest, i);

            if (Obj.IsComplete())
            {
                Obj.bCompleted = true;
                UE_LOG(LogTemp, Log, TEXT("[Quest] Objective complete: %s"), *Obj.Description);
            }

            // 检查是否全部完成
            bool bAllDone = true;
            for (const FQuestObjective& O : Quest.Objectives)
            {
                if (!O.bCompleted) { bAllDone = false; break; }
            }
            if (bAllDone)
            {
                CompleteQuest(QuestID, Player);
            }
            break;
        }
    }
}

// —— 查询 ——

const FQuestData& AQuestSystem::GetQuest(const FString& QuestID) const
{
    static FQuestData Empty;
    const FQuestData* Found = AllQuests.Find(QuestID);
    return Found ? *Found : Empty;
}

TArray<FQuestData> AQuestSystem::GetActiveQuests() const
{
    TArray<FQuestData> Result;
    for (const auto& Pair : AllQuests)
    {
        if (Pair.Value.bIsActive) Result.Add(Pair.Value);
    }
    return Result;
}

TArray<FQuestData> AQuestSystem::GetAvailableQuests(const FString& NPCID) const
{
    TArray<FQuestData> Result;
    const FNPCData* NPC = AllNPCs.Find(NPCID);
    if (!NPC) return Result;

    for (const FString& QuestID : NPC->OfferedQuestIDs)
    {
        const FQuestData* Quest = AllQuests.Find(QuestID);
        if (Quest && !Quest->bIsActive && !Quest->bIsCompleted && !Quest->bIsFailed)
        {
            Result.Add(*Quest);
        }
    }
    return Result;
}

TArray<FQuestData> AQuestSystem::GetCompletedQuests() const
{
    TArray<FQuestData> Result;
    for (const auto& Pair : AllQuests)
    {
        if (Pair.Value.bIsCompleted) Result.Add(Pair.Value);
    }
    return Result;
}

const FNPCData& AQuestSystem::GetNPC(const FString& NPCID) const
{
    static FNPCData Empty;
    const FNPCData* Found = AllNPCs.Find(NPCID);
    return Found ? *Found : Empty;
}

TArray<FNPCData> AQuestSystem::GetAllNPCs() const
{
    TArray<FNPCData> Result;
    for (const auto& Pair : AllNPCs) Result.Add(Pair.Value);
    return Result;
}

TArray<FNPCData> AQuestSystem::GetNPCsByFaction(const FString& Faction) const
{
    TArray<FNPCData> Result;
    for (const auto& Pair : AllNPCs)
    {
        if (Pair.Value.Faction == Faction) Result.Add(Pair.Value);
    }
    return Result;
}

TArray<FNPCData> AQuestSystem::GetNPCsNearLocation(const FVector& Location, float Radius) const
{
    TArray<FNPCData> Result;
    float RadSq = Radius * Radius;
    for (const auto& Pair : AllNPCs)
    {
        // 简化：假设 NPC 在原点附近
        // 实际应从 NPC Actor 取位置
        Result.Add(Pair.Value); // 暂全返回
    }
    return Result;
}
