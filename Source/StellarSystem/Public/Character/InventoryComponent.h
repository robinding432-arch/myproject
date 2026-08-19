// ============================================================
// 路径: Source/StellarSystem/Public/Character/InventoryComponent.h
// 作用: 背包/装备/消耗品/弹药管理
// 依赖: Economy/ConsumableItem.h, Economy/AmmoItem.h
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryComponent.generated.h"

class UConsumableItem;
class UAmmoItem;

// —— 背包物品槽 ——
USTRUCT(BlueprintType)
struct FInventorySlot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName ItemID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Quantity = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Durability = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 QualityLevel = 1; // 1=Common ... 5=Alien
};

// —— 装备槽位 ——
UENUM(BlueprintType)
enum class EEquipmentSlot : uint8
{
    Head        UMETA(DisplayName = "Head"),
    Chest       UMETA(DisplayName = "Chest"),
    Legs        UMETA(DisplayName = "Legs"),
    Feet        UMETA(DisplayName = "Feet"),
    Hands       UMETA(DisplayName = "Hands"),
    PrimaryWeapon UMETA(DisplayName = "Primary Weapon"),
    SecondaryWeapon UMETA(DisplayName = "Secondary Weapon"),
    Backpack    UMETA(DisplayName = "Backpack"),
    Accessory1  UMETA(DisplayName = "Accessory 1"),
    Accessory2  UMETA(DisplayName = "Accessory 2")
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UInventoryComponent();

    // —— 背包 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
    int32 MaxSlots = 40;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
    float MaxWeight = 100.f; // kg

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    TArray<FInventorySlot> Slots;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    float CurrentWeight = 0.f;

    // —— 快捷栏（0~9） ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Hotbar")
    TArray<FName> HotbarItems; // 长度 10

    // —— 装备 ——
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory|Equipment")
    TMap<EEquipmentSlot, FInventorySlot> EquippedItems;

    // —— 弹药库存 ——
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory|Ammo")
    TMap<FName, int32> AmmoInventory; // AmmoTypeID → Count

    // ========== 背包操作 ==========
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool AddItem(FName ItemID, int32 Quantity, float WeightPerUnit);

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool RemoveItem(FName ItemID, int32 Quantity);

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    int32 GetItemCount(FName ItemID) const;

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool HasItem(FName ItemID, int32 MinQuantity = 1) const;

    // ========== 快捷栏 ==========
    UFUNCTION(BlueprintCallable, Category = "Inventory|Hotbar")
    void SetHotbarSlot(int32 SlotIndex, FName ItemID);

    UFUNCTION(BlueprintCallable, Category = "Inventory|Hotbar")
    FName GetHotbarItem(int32 SlotIndex) const;

    UFUNCTION(BlueprintCallable, Category = "Inventory|Hotbar")
    void UseConsumable(int32 SlotIndex);

    // ========== 装备 ==========
    UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment")
    bool EquipItem(EEquipmentSlot Slot, FName ItemID);

    UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment")
    void UnequipItem(EEquipmentSlot Slot);

    UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment")
    FInventorySlot GetEquippedItem(EEquipmentSlot Slot) const;

    // ========== 弹药 ==========
    UFUNCTION(BlueprintCallable, Category = "Inventory|Ammo")
    bool AddAmmo(FName AmmoTypeID, int32 Count);

    UFUNCTION(BlueprintCallable, Category = "Inventory|Ammo")
    bool ConsumeAmmo(FName AmmoTypeID, int32 Count);

    UFUNCTION(BlueprintCallable, Category = "Inventory|Ammo")
    int32 GetAmmoCount(FName AmmoTypeID) const;

    // ========== 重量 ==========
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool CanCarry(float AdditionalWeight) const;

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void RecalculateWeight();

    // ========== 排序/整理 ==========
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void SortInventory();

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void CompactSlots();

    // —— 事件 ——
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryChanged, FName, ItemID);
    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FOnInventoryChanged OnItemAdded;

    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FOnInventoryChanged OnItemRemoved;

    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FOnInventoryChanged OnEquipmentChanged;

private:
    int32 FindOrCreateSlot(FName ItemID);
    void UpdateHotbarReferences();
};
