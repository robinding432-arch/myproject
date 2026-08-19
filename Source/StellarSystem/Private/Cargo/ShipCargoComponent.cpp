// ============================================================
// 路径: Source/StellarSystem/Private/Cargo/ShipCargoComponent.cpp
// 作用: 飞船货舱 —— 容量/重量/自动装卸 实现
// 新增于: v7.4
// ============================================================

#include "Cargo/ShipCargoComponent.h"
#include "Ship/ShipPawn.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

UShipCargoComponent::UShipCargoComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
}

void UShipCargoComponent::BeginPlay()
{
    Super::BeginPlay();
    OwnerShip = Cast<AShipPawn>(GetOwner());
    RecalculateTotals();
}

void UShipCargoComponent::TickComponent(float Dt, ELevelTick Tick, FActorComponentTickFunction* Fn)
{
    Super::TickComponent(Dt, Tick, Fn);
    TickPerishables(Dt);
}

// ========== 查询 ==========
bool UShipCargoComponent::CanFit(FName ItemID, int32 Quantity, float UnitWeight, float UnitVolume) const
{
    const float NewWeight = CurrentWeight + (Quantity * UnitWeight);
    const float NewVolume = CurrentVolume + (Quantity * UnitVolume);
    return (NewWeight <= MaxCargoWeight) && (NewVolume <= MaxCargoVolume);
}

int32 UShipCargoComponent::GetItemQuantity(FName ItemID) const
{
    const int32 Idx = FindCargoIndex(ItemID);
    return (Idx >= 0) ? Cargo[Idx].Quantity : 0;
}

float UShipCargoComponent::GetCargoFillPercent() const
{
    const float W = MaxCargoWeight > 0.f ? (CurrentWeight / MaxCargoWeight) : 0.f;
    const float V = MaxCargoVolume > 0.f ? (CurrentVolume / MaxCargoVolume) : 0.f;
    return FMath::Clamp(FMath::Max(W, V), 0.f, 1.f);
}

bool UShipCargoComponent::HasQuestCargo(FName QuestID) const
{
    for (const FCargoEntry& E : Cargo)
        if (E.QuestBinding == QuestID && E.Quantity > 0)
            return true;
    return false;
}

// ========== 装载 ==========
bool UShipCargoComponent::LoadCargo(FName ItemID, int32 Quantity, float UnitWeight, float UnitVolume,
                                     FName QuestBinding, bool bPerishable, float PerishTime)
{
    if (Quantity <= 0) return false;
    if (!CanFit(ItemID, Quantity, UnitWeight, UnitVolume))
    {
        OnCargoFull.Broadcast(Quantity * UnitWeight - (MaxCargoWeight - CurrentWeight));
        return false;
    }

    int32 Idx = FindCargoIndex(ItemID);
    if (Idx >= 0)
    {
        Cargo[Idx].Quantity += Quantity;
        Cargo[Idx].UnitWeight = UnitWeight;
        Cargo[Idx].UnitVolume = UnitVolume;
    }
    else
    {
        FCargoEntry NewEntry;
        NewEntry.ItemID = ItemID;
        NewEntry.Quantity = Quantity;
        NewEntry.UnitWeight = UnitWeight;
        NewEntry.UnitVolume = UnitVolume;
        NewEntry.bIsPerishable = bPerishable;
        NewEntry.PerishTimer = PerishTime;
        NewEntry.QuestBinding = QuestBinding;
        Cargo.Add(NewEntry);
    }

    if (bPerishable && PerishTime > 0.f)
        PerishTimers.Add(ItemID, PerishTime);

    RecalculateTotals();
    OnCargoAdded.Broadcast(ItemID);
    return true;
}

void UShipCargoComponent::Server_AutoLoadFromStation_Implementation(FName StationID,
    const TArray<FCargoEntry>& Incoming)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    for (const FCargoEntry& E : Incoming)
    {
        LoadCargo(E.ItemID, E.Quantity, E.UnitWeight, E.UnitVolume,
                  E.QuestBinding, E.bIsPerishable, E.PerishTimer);
    }
}

bool UShipCargoComponent::Server_AutoLoadFromStation_Validate(FName, const TArray<FCargoEntry>&)
{
    return true;
}

// ========== 卸载 ==========
bool UShipCargoComponent::UnloadCargo(FName ItemID, int32 Quantity)
{
    const int32 Idx = FindCargoIndex(ItemID);
    if (Idx < 0 || Cargo[Idx].Quantity < Quantity) return false;

    Cargo[Idx].Quantity -= Quantity;
    if (Cargo[Idx].Quantity <= 0)
    {
        PerishTimers.Remove(ItemID);
        Cargo.RemoveAt(Idx);
    }

    RecalculateTotals();
    OnCargoRemoved.Broadcast(ItemID);
    return true;
}

void UShipCargoComponent::Server_AutoUnloadToStation_Implementation(FName StationID,
    const TArray<FName>& ItemIDs)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    for (const FName& ID : ItemIDs)
        UnloadCargo(ID, GetItemQuantity(ID));
}

bool UShipCargoComponent::Server_AutoUnloadToStation_Validate(FName, const TArray<FName>&)
{
    return true;
}

void UShipCargoComponent::UnloadAll()
{
    Cargo.Empty();
    PerishTimers.Empty();
    RecalculateTotals();
}

// ========== 任务集成 ==========
bool UShipCargoComponent::ConsumeQuestCargo(FName QuestID, FName ItemID, int32 RequiredQty)
{
    const int32 Have = GetItemQuantity(ItemID);
    if (Have < RequiredQty) return false;

    // 必须绑定到本任务
    const int32 Idx = FindCargoIndex(ItemID);
    if (Idx < 0 || Cargo[Idx].QuestBinding != QuestID) return false;

    UnloadCargo(ItemID, RequiredQty);
    return true;
}

// ========== 内部 ==========
void UShipCargoComponent::RecalculateTotals()
{
    float W = 0.f, V = 0.f;
    for (const FCargoEntry& E : Cargo)
    {
        W += E.TotalWeight();
        V += E.TotalVolume();
    }
    CurrentWeight = W;
    CurrentVolume = V;
}

int32 UShipCargoComponent::FindCargoIndex(FName ItemID) const
{
    for (int32 i = 0; i < Cargo.Num(); ++i)
        if (Cargo[i].ItemID == ItemID) return i;
    return -1;
}

void UShipCargoComponent::TickPerishables(float Dt)
{
    if (PerishTimers.Num() == 0) return;

    TArray<FName> Expired;
    for (auto& Pair : PerishTimers)
    {
        Pair.Value -= Dt;
        // 同步到 Cargo 条目
        const int32 Idx = FindCargoIndex(Pair.Key);
        if (Idx >= 0) Cargo[Idx].PerishTimer = Pair.Value;
        if (Pair.Value <= 0.f) Expired.Add(Pair.Key);
    }
    for (const FName& ID : Expired)
    {
        OnCargoPerished.Broadcast(ID);
        UnloadCargo(ID, GetItemQuantity(ID)); // 腐烂货物消失
    }
}

// ========== 网络复制 ==========
void UShipCargoComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(UShipCargoComponent, MaxCargoWeight);
    DOREPLIFETIME(UShipCargoComponent, MaxCargoVolume);
    DOREPLIFETIME(UShipCargoComponent, Cargo);
    DOREPLIFETIME(UShipCargoComponent, CurrentWeight);
    DOREPLIFETIME(UShipCargoComponent, CurrentVolume);
}
