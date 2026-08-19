// ShopSystem.h
// 完整内购商城：商品/货币/库存/购买/装备/折扣/稀有度/服务器校验
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ShopSystem.generated.h"

class UInventoryComponent;
class UCurrencyComponent;

// 稀有度
UENUM(BlueprintType)
enum class EItemRarity : uint8
{
    Common      UMETA(DisplayName="普通"),
    Uncommon    UMETA(DisplayName="优秀"),
    Rare        UMETA(DisplayName="稀有"),
    Epic        UMETA(DisplayName="史诗"),
    Legendary   UMETA(DisplayName="传说"),
    Mythic      UMETA(DisplayName="神话")
};

// 商品类型
UENUM(BlueprintType)
enum class EShopItemType : uint8
{
    Weapon, Armor, Helmet, Boots, Gloves,
    Consumable, Ammo, ShipComponent, Cosmetic, Upgrade
};

// 货币类型
UENUM(BlueprintType)
enum class ECurrencyType : uint8
{
    Credits,    // 通用货币（游戏内赚）
    Premium,    // 付费货币（真钱）
    Faction,    // 阵营声望币
    Salvage     // 拆解废料
};

// 商品数据资产
UCLASS(BlueprintType)
class UShopItemData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop")
    FName ItemID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop", meta=(MultiLine=true))
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop")
    EShopItemType ItemType = EShopItemType::Weapon;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop")
    EItemRarity Rarity = EItemRarity::Common;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop")
    ECurrencyType PriceCurrency = ECurrencyType::Credits;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop")
    int32 BasePrice = 100;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop")
    float DiscountRate = 1.f; // 1=无折扣, 0.8=八折

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop")
    int32 RequiredPlayerLevel = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop")
    FName RequiredFaction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop")
    bool bLimitedStock = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop", meta=(EditCondition="bLimitedStock"))
    int32 StockAmount = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop")
    UTexture2D* Icon;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop")
    TSoftObjectPtr<UObject> ItemAsset; // 实际物品蓝图/数据

    // 动态价格（可被子类/事件覆盖）
    UFUNCTION(BlueprintNativeEvent)
    int32 GetCurrentPrice(UObject* Context) const;
    virtual int32 GetCurrentPrice_Implementation(UObject* Context) const;
};

// 商城组件（挂在 PlayerState 或 Character 上）
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UShopComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UShopComponent();

    // —— 商品目录 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
    TArray<UShopItemData*> ShopCatalog;

    // —— 玩家库存引用 ——
    UPROPERTY()
    UInventoryComponent* InventoryRef;

    // —— 购买 ——
    UFUNCTION(BlueprintCallable, Server, Reliable, Category="Shop")
    void ServerPurchaseItem(FName ItemID, int32 Quantity = 1);

    UFUNCTION(BlueprintCallable, Category="Shop")
    bool CanPurchase(UShopItemData* Item, int32 Quantity = 1, APlayerController* PC = nullptr) const;

    UFUNCTION(BlueprintCallable, Category="Shop")
    int32 GetPriceForPlayer(UShopItemData* Item, APlayerController* PC = nullptr) const;

    // —— 刷新/折扣 ——
    UFUNCTION(BlueprintCallable, Category="Shop")
    void RefreshShop();

    UFUNCTION(BlueprintCallable, Category="Shop")
    void ApplyDiscount(FName ItemID, float Rate);

    UFUNCTION(BlueprintCallable, Category="Shop")
    void ClearDiscount(FName ItemID);

    // —— 热销/推荐 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FName> FeaturedItems;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FName> NewArrivals;

    // —— 服务器校验 ——
    UFUNCTION(BlueprintCallable, Category="Shop")
    bool ValidatePurchase_Server(FName ItemID, APlayerController* PC, FString& OutReason) const;

    // —— 事件 ——
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPurchaseComplete, FName, ItemID, int32, Quantity);
    UPROPERTY(BlueprintAssignable)
    FOnPurchaseComplete OnPurchaseComplete;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPurchaseFailed, FString, Reason);
    UPROPERTY(BlueprintAssignable)
    FOnPurchaseFailed OnPurchaseFailed;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    // 已购计数（用于动态定价）
    UPROPERTY(Replicated)
    TMap<FName, int32> PurchaseCounts;

    // 当前折扣表
    UPROPERTY(Replicated)
    TMap<FName, float> ActiveDiscounts;

    // 刷新时间
    UPROPERTY(Replicated)
    FDateTime LastRefreshTime;

    UPROPERTY(EditAnywhere)
    float ShopRefreshIntervalHours = 24.f;

    // 内部
    UShopItemData* FindItem(FName ItemID) const;
    void DeductCurrency(ECurrencyType Type, int32 Amount, APlayerController* PC);
    void GrantItem(UShopItemData* Item, int32 Quantity, APlayerController* PC);
};
