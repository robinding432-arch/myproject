// ============================================================
// 路径: Source/StellarSystem/Private/Cargo/ProximityDeliverySystem.cpp
// 作用: 近距离给付 —— 玩家/飞船靠近自动交付
// 新增于: v7.4
// ============================================================

#include "Cargo/ProximityDeliverySystem.h"
#include "Cargo/ShipCargoComponent.h"
#include "AI/QuestSystemV2.h"
#include "Ship/ShipPawn.h"
#include "Character/MyCharacter.h"
#include "Character/InventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

AProximityDeliveryManager::AProximityDeliveryManager()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
}

void AProximityDeliveryManager::BeginPlay()
{
    Super::BeginPlay();
}

void AProximityDeliveryManager::Tick(float Dt)
{
    Super::Tick(Dt);
    // 后续可加节流: 每 0.5s 刷新一次附近可交付列表
}

// ========== 玩家地面交付 ==========
TArray<FAvailableDelivery> AProximityDeliveryManager::GetAvailableDeliveriesForPlayer(
    AController* Player, AActor* NPCActor) const
{
    TArray<FAvailableDelivery> Result;
    if (!Player || !NPCActor) return Result;

    UQuestManagerV2* QM = GetQuestManager();
    if (!QM) return Result;

    const FString Key = QM->GetPlayerKey(Player);
    const TArray<FPlayerQuestState> States = QM->GetPlayerQuestStates(Player);

    for (const FPlayerQuestState& S : States)
    {
        const FQuestDefinition Def = QM->GetQuestDef(S.QuestID);
        for (const FQuestObjective& O : S.Objectives)
        {
            if (O.Type != EObjectiveType::Deliver) continue;
            if (O.bCompleted) continue;

            FAvailableDelivery D;
            D.QuestID = S.QuestID;
            D.ObjectiveID = O.ObjectiveID;
            D.DisplayName = O.Description;
            D.RequiredItemID = O.RequiredItemID;
            D.RequiredQuantity = O.RequiredAmount;

            // 查玩家背包
            AMyCharacter* Char = Cast<AMyCharacter>(Player->GetPawn());
            if (Char && Char->GetInventory())
                D.PlayerHasQuantity = Char->GetInventory()->GetItemCount(O.RequiredItemID);

            D.bCanDeliver = (D.PlayerHasQuantity >= D.RequiredQuantity)
                         && IsPlayerInRange(Player, NPCActor);
            Result.Add(D);
        }
    }
    return Result;
}

void AProximityDeliveryManager::Server_ExecutePlayerDelivery_Implementation(
    AController* Player, AActor* NPCActor, FName QuestID, FName ObjectiveID)
{
    if (!Player || !NPCActor) return;
    if (!IsPlayerInRange(Player, NPCActor)) return; // 距离校验(服务端)

    UQuestManagerV2* QM = GetQuestManager();
    if (!QM) return;

    // 查任务目标
    const FQuestDefinition Def = QM->GetQuestDef(QuestID);
    for (const FQuestObjective& O : Def.Objectives)
    {
        if (O.ObjectiveID != ObjectiveID) continue;
        if (O.Type != EObjectiveType::Deliver) return;

        // 从玩家背包扣除
        AMyCharacter* Char = Cast<AMyCharacter>(Player->GetPawn());
        if (!Char || !Char->GetInventory()) return;
        if (!Char->GetInventory()->RemoveItem(O.RequiredItemID, O.RequiredAmount)) return;

        // 推进任务
        QM->CompleteObjective(Player, QuestID, ObjectiveID);
        OnDeliverySucceeded.Broadcast(QuestID, O.RequiredItemID, O.RequiredAmount);
        return;
    }
}

bool AProximityDeliveryManager::Server_ExecutePlayerDelivery_Validate(
    AController*, AActor*, FName, FName) { return true; }

// ========== 飞船靠港自动交付 ==========
void AProximityDeliveryManager::OnShipDockedAtStation(AActor* Station, AShipPawn* Ship)
{
    if (!Station || !Ship) return;
    if (!bAutoSubmitOnEnterRange) return;

    UQuestManagerV2* QM = GetQuestManager();
    UShipCargoComponent* Cargo = GetShipCargo(Ship);
    if (!QM || !Cargo) return;

    // 遍历玩家任务, 自动提交匹配的货
    AController* PC = Ship->GetController();
    if (!PC) return;

    const TArray<FPlayerQuestState> States = QM->GetPlayerQuestStates(PC);
    for (const FPlayerQuestState& S : States)
    {
        const FQuestDefinition Def = QM->GetQuestDef(S.QuestID);
        for (const FQuestObjective& O : S.Objectives)
        {
            if (O.Type != EObjectiveType::Deliver) continue;
            if (O.bCompleted) continue;
            if (!Cargo->HasQuestCargo(S.QuestID)) continue;

            // 自动卸货 + 完成任务
            Server_AutoSubmitCargoToStation_Implementation(Station, Ship, S.QuestID);
            break;
        }
    }
}

void AProximityDeliveryManager::Server_AutoSubmitCargoToStation_Implementation(
    AActor* Station, AShipPawn* Ship, FName QuestID)
{
    if (!GetAuthority()) return;
    UShipCargoComponent* Cargo = GetShipCargo(Ship);
    UQuestManagerV2* QM = GetQuestManager();
    if (!Cargo || !QM) return;

    AController* PC = Ship ? Ship->GetController() : nullptr;
    if (!PC) return;

    const FQuestDefinition Def = QM->GetQuestDef(QuestID);
    for (const FQuestObjective& O : Def.Objectives)
    {
        if (O.Type != EObjectiveType::Deliver) continue;
        if (O.bCompleted) continue;

        // 从货舱扣货
        if (Cargo->ConsumeQuestCargo(QuestID, O.RequiredItemID, O.RequiredAmount))
        {
            QM->CompleteObjective(PC, QuestID, O.ObjectiveID);
            OnDeliverySucceeded.Broadcast(QuestID, O.RequiredItemID, O.RequiredAmount);
        }
    }
}

bool AProximityDeliveryManager::Server_AutoSubmitCargoToStation_Validate(
    AActor*, AShipPawn*, FName) { return true; }

// ========== 接取任务时自动装船 ==========
void AProximityDeliveryManager::AutoLoadCargoOnAccept(
    AController* Player, FName QuestID, AActor* PickupStation)
{
    if (!Player || !PickupStation) return;

    UQuestManagerV2* QM = GetQuestManager();
    if (!QM) return;

    const FQuestDefinition Def = QM->GetQuestDef(QuestID);
    if (Def.PrimaryType != EObjectiveType::Deliver) return;

    // 找玩家当前飞船
    APawn* Pawn = Player->GetPawn();
    AShipPawn* Ship = Cast<AShipPawn>(Pawn);
    if (!Ship)
    {
        // 玩家在地面, 找附近飞船
        TArray<AActor*> Ships;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), AShipPawn::StaticClass(), Ships);
        for (AActor* A : Ships)
        {
            if (IsShipInDockingRange(Cast<AShipPawn>(A), PickupStation))
            { Ship = Cast<AShipPawn>(A); break; }
        }
    }
    if (!Ship) return;

    UShipCargoComponent* Cargo = Ship->FindComponentByClass<UShipCargoComponent>();
    if (!Cargo) return;

    // 把任务所需货物装船
    for (const FQuestObjective& O : Def.Objectives)
    {
        if (O.Type != EObjectiveType::Deliver) continue;
        // 默认货物: 单件 1kg / 0.5m³, 易腐, 保鲜 = 任务时限
        Cargo->LoadCargo(O.RequiredItemID, O.RequiredAmount, 1.f, 0.5f,
                         QuestID, /*bPerishable*/ true, Def.TimeLimit);
    }
}

// ========== 查询 ==========
bool AProximityDeliveryManager::IsPlayerInRange(AController* Player, AActor* Target) const
{
    if (!Player || !Target) return false;
    APawn* Pawn = Player->GetPawn();
    if (!Pawn) return false;
    const float Dist = FVector::Dist(Pawn->GetActorLocation(), Target->GetActorLocation());
    return Dist <= PlayerInteractRange;
}

bool AProximityDeliveryManager::IsShipInDockingRange(AShipPawn* Ship, AActor* Station) const
{
    if (!Ship || !Station) return false;
    const float Dist = FVector::Dist(Ship->GetActorLocation(), Station->GetActorLocation());
    return Dist <= ShipDockRange;
}

float AProximityDeliveryManager::GetDeliveryProgress(AController* Player, AActor* Target) const
{
    if (!Player || !Target) return 0.f;
    APawn* Pawn = Player->GetPawn();
    if (!Pawn) return 0.f;
    const float Dist = FVector::Dist(Pawn->GetActorLocation(), Target->GetActorLocation());
    return FMath::Clamp(1.f - (Dist / PlayerInteractRange), 0.f, 1.f);
}

// ========== 内部 ==========
UQuestManagerV2* AProximityDeliveryManager::GetQuestManager() const
{
    return GetWorld() ? GetWorld()->GetSubsystem<UQuestManagerV2>() : nullptr;
}

UShipCargoComponent* AProximityDeliveryManager::GetShipCargo(AShipPawn* Ship) const
{
    return Ship ? Ship->FindComponentByClass<UShipCargoComponent>() : nullptr;
}

void AProximityDeliveryManager::TransferInventoryToCargo(
    AController* Player, FName ItemID, int32 Qty, UShipCargoComponent* Cargo)
{
    AMyCharacter* Char = Cast<AMyCharacter>(Player->GetPawn());
    if (!Char || !Char->GetInventory() || !Cargo) return;
    if (Char->GetInventory()->RemoveItem(ItemID, Qty))
        Cargo->LoadCargo(ItemID, Qty, 1.f, 0.5f);
}

void AProximityDeliveryManager::CompleteDeliveryObjective(
    AController* Player, FName QuestID, FName ObjectiveID, int32 Qty)
{
    UQuestManagerV2* QM = GetQuestManager();
    if (QM) QM->CompleteObjective(Player, QuestID, ObjectiveID);
}
