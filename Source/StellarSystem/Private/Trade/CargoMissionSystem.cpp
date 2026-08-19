// ============================================================
// 路径: Source/StellarSystem/Private/Trade/CargoMissionSystem.cpp
// 作用: 货运任务系统实现 —— 接取→自动装船→飞行→到达→自动卸船→完成
// 新增于: v7.5
// ============================================================

#include "Trade/CargoMissionSystem.h"
#include "Cargo/ShipCargoComponent.h"
#include "Ship/ShipPawn.h"
#include "Character/MyCharacter.h"
#include "Character/CurrencyComponent.h"
#include "Character/InventoryComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCargoMission, Log, All);

void UCargoMissionManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogCargoMission, Log, TEXT("[CargoMission] 初始化"));
}

void UCargoMissionManager::Deinitialize()
{
    AllMissions.Empty();
    MissionBoards.Empty();
    PlayerMissions.Empty();
    Super::Deinitialize();
}

void UCargoMissionManager::Tick(float DeltaTime)
{
    TickMissionTimers(DeltaTime);
    TickPerishables(DeltaTime);
}

// ==================== 任务板查询 ====================

TArray<FCargoMission> UCargoMissionManager::GetAvailableMissionsAtStation(FName StationID) const
{
    TArray<FCargoMission> Result;
    if (const FCargoMissionBoard* Board = MissionBoards.Find(StationID))
    {
        for (FName ID : Board->AvailableMissionIDs)
        {
            if (const FCargoMission* M = AllMissions.Find(ID))
            {
                if (M->Status == ECargoMissionStatus::Available)
                {
                    Result.Add(*M);
                }
            }
        }
    }
    return Result;
}

TArray<FCargoMission> UCargoMissionManager::GetAllAvailableMissions() const
{
    TArray<FCargoMission> Result;
    for (const auto& Pair : AllMissions)
    {
        if (Pair.Value.Status == ECargoMissionStatus::Available)
        {
            Result.Add(Pair.Value);
        }
    }
    return Result;
}

// ==================== 接取任务 ====================

void UCargoMissionManager::Server_AcceptCargoMission_Implementation(
    AController* Player, FName MissionID)
{
    if (!Player) return;

    FCargoMission* Mission = AllMissions.Find(MissionID);
    if (!Mission) return;

    if (Mission->Status != ECargoMissionStatus::Available)
    {
        UE_LOG(LogCargoMission, Warning, TEXT("[Cargo] 任务 %s 不可接取, 状态=%d"),
               *MissionID.ToString(), (int32)Mission->Status);
        return;
    }

    FString NetID = Player->GetNetConnection() ? Player->GetNetConnection()->PlayerId.ToString() : Player->GetName();

    // 检查玩家活跃任务数
    TArray<FName>& PlayerMissionsList = PlayerMissions.FindOrAdd(NetID);
    if (PlayerMissionsList.Num() >= MaxPlayerActiveMissions)
    {
        UE_LOG(LogCargoMission, Warning, TEXT("[Cargo] %s 活跃任务已满 (%d)"), *NetID, MaxPlayerActiveMissions);
        return;
    }

    // 检查飞船是否在旁边(距离校验)
    AShipPawn* Ship = Cast<AShipPawn>(Player->GetPawn());
    if (!Ship)
    {
        // 玩家可能步行到任务板 → 找附近飞船
        // 简化: 假设玩家在飞船内或旁边
    }

    UShipCargoComponent* Cargo = Ship ? Ship->FindComponentByClass<UShipCargoComponent>() : nullptr;
    if (!Cargo)
    {
        UE_LOG(LogCargoMission, Warning, TEXT("[Cargo] 无飞船货舱"));
        return;
    }

    // 检查货舱容量
    float FreeWeight = Cargo->GetFreeWeight();
    float FreeVolume = Cargo->GetFreeVolume();
    if (Mission->TotalWeight > FreeWeight || Mission->TotalVolume > FreeVolume)
    {
        UE_LOG(LogCargoMission, Warning, TEXT("[Cargo] 货舱不足: 需%.0fkg/%.0fm³, 剩%.0fkg/%.0fm³"),
               Mission->TotalWeight, Mission->TotalVolume, FreeWeight, FreeVolume);
        return;
    }

    // 接受任务
    Mission->Status = ECargoMissionStatus::Accepted;
    Mission->CreatedAt = GetWorld()->GetTimeSeconds();
    if (Mission->TimeLimit <= 0.f) Mission->TimeLimit = DefaultExpiryTime;
    Mission->TimeRemaining = Mission->TimeLimit;

    PlayerMissionsList.Add(MissionID);

    // 从任务板移除
    RemoveMissionFromBoard(Mission->PickupStationID, MissionID);

    // 自动装船
    Server_AutoLoadMissionCargo(Player, MissionID);

    OnMissionAccepted.Broadcast(MissionID, NetID, Mission->Cargo.Num());

    UE_LOG(LogCargoMission, Log, TEXT("[Cargo] 接取 %s: %s | %.0fkg → %s"),
           *MissionID.ToString(), *NetID, Mission->TotalWeight, *Mission->DeliverStationName);
}

bool UCargoMissionManager::Server_AcceptCargoMission_Validate(
    AController* Player, FName MissionID)
{
    return Player != nullptr;
}

// ==================== 自动装船 ====================

void UCargoMissionManager::Server_AutoLoadMissionCargo_Implementation(
    AController* Player, FName MissionID)
{
    if (!Player) return;

    FCargoMission* Mission = AllMissions.Find(MissionID);
    if (!Mission) return;

    AShipPawn* Ship = Cast<AShipPawn>(Player->GetPawn());
    if (!Ship) return;

    UShipCargoComponent* Cargo = Ship->FindComponentByClass<UShipCargoComponent>();
    if (!Cargo) return;

    // 逐件装入货舱(任务绑定, 不可丢弃)
    for (const FCargoMissionItem& Item : Mission->Cargo)
    {
        Cargo->LoadCargo(
            Item.ItemID,
            Item.Quantity,
            Item.UnitWeight,
            Item.UnitVolume,
            MissionID,           // QuestBinding
            Item.bIsPerishable,
            Item.PerishTime
        );
    }

    Mission->Status = ECargoMissionStatus::Loaded;

    FString NetID = Player->GetNetConnection() ? Player->GetNetConnection()->PlayerId.ToString() : Player->GetName();
    OnCargoLoaded.Broadcast(MissionID, NetID, Mission->TotalWeight);

    UE_LOG(LogCargoMission, Log, TEXT("[Cargo] 装船完成 %s: %.0fkg"), *MissionID.ToString(), Mission->TotalWeight);
}

bool UCargoMissionManager::Server_AutoLoadMissionCargo_Validate(
    AController* Player, FName MissionID)
{
    return Player != nullptr;
}

// ==================== 飞船靠港(到达目的地) ====================

void UCargoMissionManager::Server_OnShipDockedAtStation_Implementation(
    AController* Player, FName StationID, AShipPawn* Ship)
{
    if (!Player || !Ship) return;

    FString NetID = Player->GetNetConnection() ? Player->GetNetConnection()->PlayerId.ToString() : Player->GetName();

    // 检查该玩家所有活跃任务
    TArray<FName>* PlayerMissionsList = PlayerMissions.Find(NetID);
    if (!PlayerMissionsList) return;

    for (FName MissionID : *PlayerMissionsList)
    {
        FCargoMission* Mission = AllMissions.Find(MissionID);
        if (!Mission) continue;

        // 检查是否到达目的地
        if (Mission->DeliverStationID == StationID &&
            (Mission->Status == ECargoMissionStatus::Loaded ||
             Mission->Status == ECargoMissionStatus::InTransit))
        {
            // 自动卸船
            Server_AutoUnloadMissionCargo(Player, MissionID);
            return;
        }
    }
}

bool UCargoMissionManager::Server_OnShipDockedAtStation_Validate(
    AController* Player, FName StationID, AShipPawn* Ship)
{
    return Player != nullptr && Ship != nullptr;
}

// ==================== 自动卸船 ====================

void UCargoMissionManager::Server_AutoUnloadMissionCargo_Implementation(
    AController* Player, FName MissionID)
{
    if (!Player) return;

    FCargoMission* Mission = AllMissions.Find(MissionID);
    if (!Mission) return;

    AShipPawn* Ship = Cast<AShipPawn>(Player->GetPawn());
    if (!Ship) return;

    UShipCargoComponent* Cargo = Ship->FindComponentByClass<UShipCargoComponent>();
    if (!Cargo) return;

    // 逐件卸下(从货舱移除任务货物)
    for (const FCargoMissionItem& Item : Mission->Cargo)
    {
        Cargo->UnloadCargo(Item.ItemID, Item.Quantity);
    }

    Mission->Status = ECargoMissionStatus::Completed;

    // 发放奖励
    GrantMissionRewards(Player, *Mission);

    // 从玩家活跃列表移除
    FString NetID = Player->GetNetConnection() ? Player->GetNetConnection()->PlayerId.ToString() : Player->GetName();
    if (TArray<FName>* List = PlayerMissions.Find(NetID))
    {
        List->Remove(MissionID);
    }

    OnMissionCompleted.Broadcast(MissionID, NetID, Mission->CreditReward);

    UE_LOG(LogCargoMission, Log, TEXT("[Cargo] 完成 %s: 奖励 %.0f | 耗时 %.0fs"),
           *MissionID.ToString(), Mission->CreditReward,
           GetWorld()->GetTimeSeconds() - Mission->CreatedAt);
}

bool UCargoMissionManager::Server_AutoUnloadMissionCargo_Validate(
    AController* Player, FName MissionID)
{
    return Player != nullptr;
}

// ==================== 检查到达 ====================

bool UCargoMissionManager::CheckArrivalAndComplete(AController* Player, FName StationID)
{
    if (!Player) return false;

    FString NetID = Player->GetNetConnection() ? Player->GetNetConnection()->PlayerId.ToString() : Player->GetName();
    TArray<FName>* List = PlayerMissions.Find(NetID);
    if (!List) return false;

    for (FName MissionID : *List)
    {
        FCargoMission* Mission = AllMissions.Find(MissionID);
        if (!Mission) continue;

        if (Mission->DeliverStationID == StationID &&
            (Mission->Status == ECargoMissionStatus::Loaded ||
             Mission->Status == ECargoMissionStatus::InTransit))
        {
            // 直接调用内部实现(避免从非-RPC上下文调用 Server 函数)
            if (Player->HasAuthority())
            {
                Server_AutoUnloadMissionCargo_Implementation(Player, MissionID);
            }
            else
            {
                Server_AutoUnloadMissionCargo(Player, MissionID);
            }
            return true;
        }
    }
    return false;
}

// ==================== 放弃任务 ====================

void UCargoMissionManager::Server_AbandonMission_Implementation(
    AController* Player, FName MissionID, bool bReturnCargo)
{
    if (!Player) return;

    FCargoMission* Mission = AllMissions.Find(MissionID);
    if (!Mission) return;

    FString NetID = Player->GetNetConnection() ? Player->GetNetConnection()->PlayerId.ToString() : Player->GetName();

    // 移除货物(如果不归还则销毁)
    if (bReturnCargo)
    {
        // 货物归还到起始站(简化: 直接销毁)
    }

    Mission->Status = ECargoMissionStatus::Failed;

    if (TArray<FName>* List = PlayerMissions.Find(NetID))
    {
        List->Remove(MissionID);
    }

    OnMissionFailed.Broadcast(MissionID, TEXT("玩家放弃"), false);

    UE_LOG(LogCargoMission, Log, TEXT("[Cargo] 放弃 %s"), *MissionID.ToString());
}

bool UCargoMissionManager::Server_AbandonMission_Validate(
    AController* Player, FName MissionID, bool bReturnCargo)
{
    return Player != nullptr;
}

// ==================== 查询 ====================

TArray<FCargoMission> UCargoMissionManager::GetPlayerActiveMissions(AController* Player) const
{
    TArray<FCargoMission> Result;
    if (!Player) return Result;

    FString NetID = Player->GetNetConnection() ? Player->GetNetConnection()->PlayerId.ToString() : Player->GetName();
    if (const TArray<FName>* List = PlayerMissions.Find(NetID))
    {
        for (FName ID : *List)
        {
            if (const FCargoMission* M = AllMissions.Find(ID))
            {
                Result.Add(*M);
            }
        }
    }
    return Result;
}

FCargoMission UCargoMissionManager::GetMission(FName MissionID) const
{
    if (const FCargoMission* M = AllMissions.Find(MissionID))
    {
        return *M;
    }
    return FCargoMission();
}

float UCargoMissionManager::GetMissionProgress(FName MissionID) const
{
    if (const FCargoMission* M = AllMissions.Find(MissionID))
    {
        if (M->TimeLimit <= 0.f) return 0.5f; // 无时限
        float Elapsed = M->TimeLimit - M->TimeRemaining;
        return FMath::Clamp(Elapsed / M->TimeLimit, 0.f, 1.f);
    }
    return 0.f;
}

// ==================== 生成任务 ====================

FName UCargoMissionManager::GenerateCargoMission(FName FromStation, FName ToStation, int32 DifficultyTier)
{
    FCargoMission Mission;
    Mission.MissionID = GenerateMissionID();
    Mission.PickupStationID = FromStation;
    Mission.DeliverStationID = ToStation;
    Mission.PickupStationName = FromStation.ToString();
    Mission.DeliverStationName = ToStation.ToString();
    Mission.DifficultyTier = FMath::Clamp(DifficultyTier, 1, 5);
    Mission.Status = ECargoMissionStatus::Available;

    // 距离决定奖励
    float Distance = CalculateDistance(FromStation, ToStation);
    float BaseReward = Distance * 0.5f * DifficultyTier;
    Mission.CreditReward = BaseReward;

    // 生成货物(1~4种)
    int32 CargoTypes = FMath::RandRange(1, 4);
    float TotalW = 0.f, TotalV = 0.f;

    for (int32 i = 0; i < CargoTypes; i++)
    {
        FCargoMissionItem Item;
        Item.ItemID = FName(*FString::Printf(TEXT("Cargo_%d"), FMath::Rand()));
        Item.DisplayName = FString::Printf(TEXT("货物 #%d"), i + 1);
        Item.Quantity = FMath::RandRange(5, 50) * DifficultyTier;
        Item.UnitWeight = FMath::RandRange(0.5f, 10.f);
        Item.UnitVolume = FMath::RandRange(0.5f, 5.f);
        Item.bIsPerishable = (FMath::Rand() % 3 == 0); // 33% 概率易腐
        Item.PerishTime = Item.bIsPerishable ? FMath::RandRange(300.f, 1800.f) : 0.f;
        Item.MissionID = Mission.MissionID;

        TotalW += Item.Quantity * Item.UnitWeight;
        TotalV += Item.Quantity * Item.UnitVolume;

        Mission.Cargo.Add(Item);
    }

    Mission.TotalWeight = TotalW;
    Mission.TotalVolume = TotalV;
    Mission.TimeLimit = FMath::Max(Distance * 0.3f, 300.f); // 至少5分钟
    Mission.bAnyPerishable = Mission.Cargo.ContainsByPredicate([](const FCargoMissionItem& I) { return I.bIsPerishable; });

    // 高难度 = 高风险
    Mission.bHighRiskRoute = (DifficultyTier >= 4);

    AllMissions.Add(Mission.MissionID, Mission);

    // 添加到起点站任务板
    if (FCargoMissionBoard* Board = MissionBoards.Find(FromStation))
    {
        Board->AvailableMissionIDs.Add(Mission.MissionID);
    }

    UE_LOG(LogCargoMission, Log, TEXT("[Cargo] 生成 %s: %s→%s | %.0fkg | %.0fCr | %.0fs"),
           *Mission.MissionID.ToString(), *Mission.PickupStationName, *Mission.DeliverStationName,
           TotalW, Mission.CreditReward, Mission.TimeLimit);

    return Mission.MissionID;
}

void UCargoMissionManager::GenerateAllStationMissions()
{
    // 收集所有已注册站点
    TArray<FName> Stations;
    for (const auto& Pair : MissionBoards)
    {
        Stations.Add(Pair.Key);
    }

    if (Stations.Num() < 2) return;

    for (const auto& Pair : MissionBoards)
    {
        FName FromStation = Pair.Key;
        for (int32 i = 0; i < MissionsPerStation; i++)
        {
            // 随机选一个不同站点作为目的地
            FName ToStation;
            do {
                ToStation = Stations[FMath::RandRange(0, Stations.Num() - 1)];
            } while (ToStation == FromStation);

            int32 Tier = FMath::RandRange(1, 5);
            GenerateCargoMission(FromStation, ToStation, Tier);
        }
    }

    UE_LOG(LogCargoMission, Log, TEXT("[Cargo] 全站任务生成完成: %d 个站点"), Stations.Num());
}

void UCargoMissionManager::RegisterMissionBoard(FName StationID, int32 MaxConcurrent)
{
    FCargoMissionBoard Board;
    Board.StationID = StationID;
    Board.MaxConcurrentMissions = MaxConcurrent;
    MissionBoards.Add(StationID, Board);
}

// ==================== 计时器 ====================

void UCargoMissionManager::TickMissionTimers(float Dt)
{
    float Now = GetWorld()->GetTimeSeconds();

    TArray<FName> ToExpire;
    for (auto& Pair : AllMissions)
    {
        FCargoMission& M = Pair.Value;

        if (M.Status == ECargoMissionStatus::Available ||
            M.Status == ECargoMissionStatus::Accepted ||
            M.Status == ECargoMissionStatus::Loaded ||
            M.Status == ECargoMissionStatus::InTransit)
        {
            if (M.TimeLimit > 0.f)
            {
                M.TimeRemaining -= Dt;
                if (M.TimeRemaining <= 0.f)
                {
                    ToExpire.Add(M.MissionID);
                }
                else if (M.TimeRemaining < M.TimeLimit * PerishWarningThreshold)
                {
                    // 警告(简化: 可广播事件给 UI)
                }
            }
        }
    }

    for (FName ID : ToExpire)
    {
        ExpireMission(ID, TEXT("时限到期"));
    }

    // 自动重新生成
    GenerationTimer += Dt;
    if (bAutoGenerateMissions && GenerationTimer >= MissionRegenerateInterval)
    {
        GenerationTimer = 0.f;
        GenerateAllStationMissions();
    }
}

void UCargoMissionManager::TickPerishables(float Dt)
{
    for (auto& Pair : AllMissions)
    {
        FCargoMission& M = Pair.Value;
        if (M.Status != ECargoMissionStatus::Loaded && M.Status != ECargoMissionStatus::InTransit)
            continue;

        for (FCargoMissionItem& Item : M.Cargo)
        {
            if (Item.bIsPerishable && Item.PerishTime > 0.f)
            {
                Item.PerishTime -= Dt;
                if (Item.PerishTime <= 0.f)
                {
                    // 货物腐坏 → 任务失败
                    Item.Quantity = 0; // 货物消失
                    ExpireMission(M.MissionID, TEXT("货物腐坏"));
                    OnCargoPerished.Broadcast(M.MissionID, Item.ItemID);
                    break;
                }
            }
        }
    }
}

void UCargoMissionManager::ExpireMission(FName MissionID, FString Reason)
{
    FCargoMission* M = AllMissions.Find(MissionID);
    if (!M) return;

    M->Status = ECargoMissionStatus::Expired;

    // 从玩家列表移除
    for (auto& Pair : PlayerMissions)
    {
        Pair.Value.Remove(MissionID);
    }

    OnMissionFailed.Broadcast(MissionID, Reason, Reason.Contains(TEXT("腐坏")));

    UE_LOG(LogCargoMission, Log, TEXT("[Cargo] 过期 %s: %s"), *MissionID.ToString(), *Reason);
}

// ==================== 奖励发放 ====================

void UCargoMissionManager::GrantMissionRewards(AController* Player, const FCargoMission& Mission)
{
    AMyCharacter* Char = Cast<AMyCharacter>(Player->GetPawn());
    if (!Char) return;

    UCurrencyComponent* Cur = Char->FindComponentByClass<UCurrencyComponent>();
    if (Cur && Mission.CreditReward > 0.f)
    {
        Cur->ServerAddCredits(Mission.CreditReward, TEXT("CargoMission"));
    }

    // 物品奖励
    UInventoryComponent* Inv = Char->FindComponentByClass<UInventoryComponent>();
    if (Inv)
    {
        for (const auto& Pair : Mission.ItemRewards)
        {
            Inv->ServerAddItem(Pair.Key, Pair.Value);
        }
    }

    // 声望奖励(简化: 需 FactionSystem)
    UE_LOG(LogCargoMission, Log, TEXT("[Cargo] 奖励 %.0fCr + %d 物品"), Mission.CreditReward, Mission.ItemRewards.Num());
}

// ==================== 工具方法 ====================

void UCargoMissionManager::RemoveMissionFromBoard(FName StationID, FName MissionID)
{
    if (FCargoMissionBoard* Board = MissionBoards.Find(StationID))
    {
        Board->AvailableMissionIDs.Remove(MissionID);
    }
}

UShipCargoComponent* UCargoMissionManager::GetPlayerShipCargo(AController* Player) const
{
    if (!Player) return nullptr;
    AShipPawn* Ship = Cast<AShipPawn>(Player->GetPawn());
    if (!Ship) return nullptr;
    return Ship->FindComponentByClass<UShipCargoComponent>();
}

AController* UCargoMissionManager::FindPlayerByNetID(FString NetID) const
{
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (!PC) continue;
        FString NID = PC->GetNetConnection() ? PC->GetNetConnection()->PlayerId.ToString() : PC->GetName();
        if (NID == NetID) return PC;
    }
    return nullptr;
}

FName UCargoMissionManager::GenerateMissionID() const
{
    static int32 Counter = 0;
    Counter++;
    return FName(*FString::Printf(TEXT("Cargo_%d_%d"), FMath::Rand(), Counter));
}

float UCargoMissionManager::CalculateDistance(FName FromStation, FName ToStation) const
{
    // 简化: 返回随机距离 1000~50000 单位
    return FMath::RandRange(1000.f, 50000.f);
}

float UCargoMissionManager::CalculateReward(const FCargoMission& Mission) const
{
    return Mission.CreditReward;
}
