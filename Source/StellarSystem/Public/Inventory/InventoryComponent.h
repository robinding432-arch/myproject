// InventoryComponent.h
// 总背包：统一管理物品/装备/货币（被 Shop/Vitals/Ammo/Consumable 引用）
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryComponent.generated.h"

class UCurrencyComponent;
class UAmmoInventoryComponent;
class UConsumableInventoryComponent;

// 物品条目
USTRUCT(BlueprintType)
struct FInventoryItem
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName ItemID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Quantity = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<UObject> ItemAsset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EItemRarity Rarity = EItemRarity::Common;
};

// 装备槽
UENUM(BlueprintType)
enum class EEquipSlot : uint8
{
    Head, Chest, Arms, Legs, Feet, // 护甲
    Weapon1, Weapon2, Weapon3,       // 武器
    ShipComponent1, ShipComponent2,  // 飞船组件
    Utility1, Utility2
};

// 主背包组件
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UInventoryComponent();

    virtual void BeginPlay() override;

    // —— 物品管理 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
    TArray<FInventoryItem> Items;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxSlots = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxWeight = 200.f; // kg

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    float CurrentWeight = 0.f;

    // 增删查
    UFUNCTION(BlueprintCallable, Server, Reliable)
    void ServerAddItem(FName ItemID, int32 Quantity, UObject* Asset = nullptr);

    UFUNCTION(BlueprintCallable, Server, Reliable)
    bool ServerRemoveItem(FName ItemID, int32 Quantity);

    UFUNCTION(BlueprintCallable, BlueprintPure)
    int32 GetItemCount(FName ItemID) const;

    UFUNCTION(BlueprintCallable, BlueprintPure)
    bool HasItem(FName ItemID, int32 MinQuantity = 1) const;

    UFUNCTION(BlueprintCallable)
    TArray<FInventoryItem> GetAllItems() const;

    // —— 装备管理 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
    TMap<EEquipSlot, FName> EquippedItems;

    UFUNCTION(BlueprintCallable, Server, Reliable)
    bool ServerEquipItem(FName ItemID, EEquipSlot Slot);

    UFUNCTION(BlueprintCallable, Server, Reliable)
    void ServerUnequipSlot(EEquipSlot Slot);

    UFUNCTION(BlueprintCallable, BlueprintPure)
    FName GetEquippedItem(EEquipSlot Slot) const;

    // —— 重量 ——
    UFUNCTION(BlueprintCallable)
    bool CanCarry(float AdditionalWeight) const;

    UFUNCTION(BlueprintCallable, Server, Reliable)
    void ServerUpdateWeight();

    // —— 出售 ——
    UFUNCTION(BlueprintCallable, Server, Reliable)
    int32 ServerSellItem(FName ItemID, int32 Quantity, int32 UnitPrice);

    // —— 事件 ——
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryChanged, const TArray<FInventoryItem>&, Items);
    UPROPERTY(BlueprintAssignable)
    FOnInventoryChanged OnInventoryChanged;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

private:
    FInventoryItem* FindItem(FName ItemID);
    const FInventoryItem* FindItem(FName ItemID) const;
};
