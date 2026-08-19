// InventoryComponent.cpp
#include "Inventory/InventoryComponent.h"
#include "Inventory/AmmoAndConsumables.h"
#include "Net/UnrealNetwork.h"

UInventoryComponent::UInventoryComponent(){SetIsReplicatedByDefault(true);}

void UInventoryComponent::BeginPlay(){Super::BeginPlay();ServerUpdateWeight();}

void UInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    DOREPLIFETIME(UInventoryComponent, Items);
    DOREPLIFETIME(UInventoryComponent, CurrentWeight);
    DOREPLIFETIME(UInventoryComponent, EquippedItems);
}

FInventoryItem* UInventoryComponent::FindItem(FName ItemID)
{
    for (FInventoryItem& I : Items) if (I.ItemID == ItemID) return &I;
    return nullptr;
}

const FInventoryItem* UInventoryComponent::FindItem(FName ItemID) const
{
    for (const FInventoryItem& I : Items) if (I.ItemID == ItemID) return &I;
    return nullptr;
}

void UInventoryComponent::ServerAddItem_Implementation(FName ItemID, int32 Quantity, UObject* Asset)
{
    if (Quantity <= 0) return;
    FInventoryItem* Existing = FindItem(ItemID);
    if (Existing) { Existing->Quantity += Quantity; }
    else
    {
        FInventoryItem NewItem; NewItem.ItemID = ItemID; NewItem.Quantity = Quantity;
        NewItem.ItemAsset = TSoftObjectPtr<UObject>(Asset);
        Items.Add(NewItem);
    }
    ServerUpdateWeight();
    OnInventoryChanged.Broadcast(Items);
}

bool UInventoryComponent::ServerRemoveItem_Implementation(FName ItemID, int32 Quantity)
{
    FInventoryItem* I = FindItem(ItemID);
    if (!I || I->Quantity < Quantity) return false;
    I->Quantity -= Quantity;
    if (I->Quantity <= 0) Items.RemoveAll([](const FInventoryItem& It) { return It.Quantity <= 0; });
    ServerUpdateWeight();
    OnInventoryChanged.Broadcast(Items);
    return true;
}

int32 UInventoryComponent::GetItemCount(FName ItemID) const
{
    if (const FInventoryItem* I = FindItem(ItemID)) return I->Quantity;
    return 0;
}

bool UInventoryComponent::HasItem(FName ItemID, int32 MinQuantity) const
{
    return GetItemCount(ItemID) >= MinQuantity;
}

TArray<FInventoryItem> UInventoryComponent::GetAllItems() const { return Items; }

bool UInventoryComponent::ServerEquipItem_Implementation(FName ItemID, EEquipSlot Slot)
{
    if (!HasItem(ItemID)) return false;
    EquippedItems.Add(Slot, ItemID);
    // 通知外部系统（护甲→维生抗性、武器→弹药绑定等）
    return true;
}

void UInventoryComponent::ServerUnequipSlot_Implementation(EEquipSlot Slot)
{
    EquippedItems.Remove(Slot);
}

FName UInventoryComponent::GetEquippedItem(EEquipSlot Slot) const
{
    if (const FName* N = EquippedItems.Find(Slot)) return *N;
    return NAME_None;
}

bool UInventoryComponent::CanCarry(float AdditionalWeight) const
{
    return (CurrentWeight + AdditionalWeight) <= MaxWeight;
}

void UInventoryComponent::ServerUpdateWeight_Implementation()
{
    // 简化：每件物品 1kg，弹药 0.1kg/发
    float W = 0.f;
    for (const FInventoryItem& I)
    {
        // 按 ItemID 前缀判断类型
        FString S = I.ItemID.ToString();
        if (S.StartsWith(TEXT("AMMO_"))) W += I.Quantity * 0.1f;
        else if (S.StartsWith(TEXT("CON_"))) W += I.Quantity * 0.5f;
        else if (S.StartsWith(TEXT("ARM_"))) W += 5.f;
        else if (S.StartsWith(TEXT("WPN_"))) W += 3.f;
        else if (S.StartsWith(TEXT("CMP_"))) W += 8.f;
        else W += 1.f * I.Quantity;
    }
    CurrentWeight = W;
}

int32 UInventoryComponent::ServerSellItem_Implementation(FName ItemID, int32 Quantity, int32 UnitPrice)
{
    if (!ServerRemoveItem(ItemID, Quantity)) return 0;
    int32 Total = Quantity * UnitPrice;
    // 给钱：通过 Owner 的 CurrencyComponent
    if (UCurrencyComponent* CC = GetOwner() ? GetOwner()->FindComponentByClass<UCurrencyComponent>() : nullptr)
    {
        CC->Add(ECurrencyType::Credits, Total);
    }
    return Total;
}
