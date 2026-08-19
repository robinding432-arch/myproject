// ============================================================
// 路径: Source/StellarSystem/Private/Character/InventoryComponent.cpp
// 作用: 背包/装备/弹药/快捷栏实现
// 依赖: Character/InventoryComponent.h, Economy/ConsumableItem.h
// ============================================================

#include "Character/InventoryComponent.h"
#include "Economy/ConsumableItem.h"
#include "Math/UnrealMathUtility.h"

UInventoryComponent::UInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    Slots.SetNum(MaxSlots);
    HotbarItems.SetNum(10);
}

// ======================== 背包操作 ========================

bool UInventoryComponent::AddItem(FName ItemID, int32 Quantity, float WeightPerUnit)
{
    if (Quantity <= 0) return false;
    if (!CanCarry(WeightPerUnit * Quantity)) return false;

    int32 SlotIdx = FindOrCreateSlot(ItemID);
    if (SlotIdx == INDEX_NONE) return false;

    Slots[SlotIdx].ItemID = ItemID;
    Slots[SlotIdx].Quantity += Quantity;
    RecalculateWeight();

    OnItemAdded.Broadcast(ItemID);
    return true;
}

bool UInventoryComponent::RemoveItem(FName ItemID, int32 Quantity)
{
    for (int32 i = 0; i < Slots.Num(); ++i)
    {
        if (Slots[i].ItemID == ItemID && Slots[i].Quantity >= Quantity)
        {
            Slots[i].Quantity -= Quantity;
            if (Slots[i].Quantity <= 0)
            {
                Slots[i].ItemID = NAME_None;
                Slots[i].Quantity = 0;
            }
            RecalculateWeight();
            OnItemRemoved.Broadcast(ItemID);
            return true;
        }
    }
    return false;
}

int32 UInventoryComponent::GetItemCount(FName ItemID) const
{
    int32 Total = 0;
    for (const FInventorySlot& Slot : Slots)
    {
        if (Slot.ItemID == ItemID) Total += Slot.Quantity;
    }
    return Total;
}

bool UInventoryComponent::HasItem(FName ItemID, int32 MinQuantity) const
{
    return GetItemCount(ItemID) >= MinQuantity;
}

int32 UInventoryComponent::FindOrCreateSlot(FName ItemID)
{
    // 先找已有同物品槽
    for (int32 i = 0; i < Slots.Num(); ++i)
    {
        if (Slots[i].ItemID == ItemID && Slots[i].Quantity > 0) return i;
    }
    // 找空槽
    for (int32 i = 0; i < Slots.Num(); ++i)
    {
        if (Slots[i].ItemID == NAME_None) return i;
    }
    return INDEX_NONE; // 背包满
}

// ======================== 快捷栏 ========================

void UInventoryComponent::SetHotbarSlot(int32 SlotIndex, FName ItemID)
{
    if (HotbarItems.IsValidIndex(SlotIndex))
    {
        HotbarItems[SlotIndex] = ItemID;
    }
}

FName UInventoryComponent::GetHotbarItem(int32 SlotIndex) const
{
    if (HotbarItems.IsValidIndex(SlotIndex)) return HotbarItems[SlotIndex];
    return NAME_None;
}

void UInventoryComponent::UseConsumable(int32 SlotIndex)
{
    FName ItemID = GetHotbarItem(SlotIndex);
    if (ItemID == NAME_None) return;

    // 查找消耗品数据并使用
    // （实际项目中查 UConsumableItem 的 DataAsset）
    RemoveItem(ItemID, 1);
}

// ======================== 装备 ========================

bool UInventoryComponent::EquipItem(EEquipmentSlot Slot, FName ItemID)
{
    if (!HasItem(ItemID)) return false;

    FInventorySlot NewSlot;
    NewSlot.ItemID = ItemID;
    NewSlot.Quantity = 1;
    EquippedItems.Add(Slot, NewSlot);

    RemoveItem(ItemID, 1);
    OnEquipmentChanged.Broadcast(ItemID);
    return true;
}

void UInventoryComponent::UnequipItem(EEquipmentSlot Slot)
{
    if (FInventorySlot* SlotPtr = EquippedItems.Find(Slot))
    {
        FName ItemID = SlotPtr->ItemID;
        // 归还到背包
        AddItem(ItemID, 1, 1.f);
        EquippedItems.Remove(Slot);
        OnEquipmentChanged.Broadcast(ItemID);
    }
}

FInventorySlot UInventoryComponent::GetEquippedItem(EEquipmentSlot Slot) const
{
    if (const FInventorySlot* Ptr = EquippedItems.Find(Slot)) return *Ptr;
    return FInventorySlot();
}

// ======================== 弹药 ========================

bool UInventoryComponent::AddAmmo(FName AmmoTypeID, int32 Count)
{
    int32* Existing = AmmoInventory.Find(AmmoTypeID);
    if (Existing)
    {
        *Existing += Count;
    }
    else
    {
        AmmoInventory.Add(AmmoTypeID, Count);
    }
    return true;
}

bool UInventoryComponent::ConsumeAmmo(FName AmmoTypeID, int32 Count)
{
    int32* Existing = AmmoInventory.Find(AmmoTypeID);
    if (!Existing || *Existing < Count) return false;
    *Existing -= Count;
    if (*Existing <= 0) AmmoInventory.Remove(AmmoTypeID);
    return true;
}

int32 UInventoryComponent::GetAmmoCount(FName AmmoTypeID) const
{
    if (const int32* Ptr = AmmoInventory.Find(AmmoTypeID)) return *Ptr;
    return 0;
}

// ======================== 重量 ========================

bool UInventoryComponent::CanCarry(float AdditionalWeight) const
{
    return (CurrentWeight + AdditionalWeight) <= MaxWeight;
}

void UInventoryComponent::RecalculateWeight()
{
    CurrentWeight = 0.f;
    // 简化：每个物品 1kg
    for (const FInventorySlot& Slot : Slots)
    {
        if (Slot.ItemID != NAME_None) CurrentWeight += Slot.Quantity * 1.f;
    }
}

// ======================== 排序/整理 ========================

void UInventoryComponent::SortInventory()
{
    // 按 ItemID 字母序排序
    Slots.Sort([](const FInventorySlot& A, const FInventorySlot& B)
    {
        return A.ItemID.ToString() < B.ItemID.ToString();
    });
}

void UInventoryComponent::CompactSlots()
{
    // 合并分散的同物品
    TMap<FName, int32> Aggregated;
    for (const FInventorySlot& Slot : Slots)
    {
        if (Slot.ItemID != NAME_None)
            Aggregated.FindOrAdd(Slot.ItemID) += Slot.Quantity;
    }

    // 清空并重新填充
    for (FInventorySlot& Slot : Slots)
    {
        Slot.ItemID = NAME_None;
        Slot.Quantity = 0;
    }

    int32 Idx = 0;
    for (auto& Pair : Aggregated)
    {
        if (Slots.IsValidIndex(Idx))
        {
            Slots[Idx].ItemID = Pair.Key;
            Slots[Idx].Quantity = Pair.Value;
            Idx++;
        }
    }
    RecalculateWeight();
}

void UInventoryComponent::UpdateHotbarReferences()
{
    // 确保快捷栏引用的物品仍在背包中
    for (int32 i = 0; i < HotbarItems.Num(); ++i)
    {
        if (HotbarItems[i] != NAME_None && !HasItem(HotbarItems[i]))
        {
            HotbarItems[i] = NAME_None;
        }
    }
}
