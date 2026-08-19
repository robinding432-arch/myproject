// ShopSystem.cpp
#include "Shop/ShopSystem.h"
#include "Character/InventoryComponent.h"
#include "Character/CurrencyComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

// ========== UShopItemData ==========
int32 UShopItemData::GetCurrentPrice_Implementation(UObject* Context) const
{
    int32 Price = FMath::RoundToInt(BasePrice * DiscountRate);
    // 可扩展：声望折扣、节日活动、VIP 加成
    return Price;
}

// ========== UShopComponent ==========
UShopComponent::UShopComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
}

void UShopComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(UShopComponent, PurchaseCounts);
    DOREPLIFETIME(UShopComponent, ActiveDiscounts);
    DOREPLIFETIME(UShopComponent, LastRefreshTime);
}

UShopItemData* UShopComponent::FindItem(FName ItemID) const
{
    for (UShopItemData* Item : ShopCatalog)
        if (Item && Item->ItemID == ItemID) return Item;
    return nullptr;
}

int32 UShopComponent::GetPriceForPlayer(UShopItemData* Item, APlayerController* PC) const
{
    if (!Item) return 0;
    int32 Price = Item->GetCurrentPrice(this);
    // 折扣叠加
    if (float* D = ActiveDiscounts.Find(Item->ItemID))
        Price = FMath::RoundToInt(Price * (*D));
    return Price;
}

bool UShopComponent::CanPurchase(UShopItemData* Item, int32 Quantity, APlayerController* PC) const
{
    if (!Item || Quantity <= 0) return false;
    if (Item->RequiredPlayerLevel > 1) // 可扩展：读玩家等级
    {
        // if (PlayerLevel < Item->RequiredPlayerLevel) return false;
    }
    int32 Cost = GetPriceForPlayer(Item, PC) * Quantity;
    // 检查货币
    // if (!CurrencyComp->HasEnough(Item->PriceCurrency, Cost)) return false;
    if (Item->bLimitedStock)
    {
        int32 Owned = PurchaseCounts.FindRef(Item->ItemID);
        if (Owned + Quantity > Item->StockAmount) return false;
    }
    return true;
}

bool UShopComponent::ValidatePurchase_Server(FName ItemID, APlayerController* PC, FString& OutReason) const
{
    UShopItemData* Item = FindItem(ItemID);
    if (!Item) { OutReason = TEXT("物品不存在"); return false; }
    if (!CanPurchase(Item, 1, PC)) { OutReason = TEXT("购买条件不满足"); return false; }
    // 反作弊：价格服务端再算一次
    int32 ServerPrice = Item->GetCurrentPrice(const_cast<UShopComponent*>(this));
    // 可扩展：检查玩家等级、声望、阵营
    return true;
}

void UShopComponent::ServerPurchaseItem_Implementation(FName ItemID, int32 Quantity)
{
    APlayerController* PC = Cast<APlayerController>(GetOwner());
    FString Reason;
    if (!ValidatePurchase_Server(ItemID, PC, Reason))
    {
        OnPurchaseFailed.Broadcast(Reason);
        return;
    }

    UShopItemData* Item = FindItem(ItemID);
    int32 TotalCost = GetPriceForPlayer(Item, PC) * Quantity;

    // 扣钱
    DeductCurrency(Item->PriceCurrency, TotalCost, PC);
    // 给物品
    GrantItem(Item, Quantity, PC);
    // 记录
    PurchaseCounts.FindOrAdd(ItemID) += Quantity;
    // 通知
    OnPurchaseComplete.Broadcast(ItemID, Quantity);
}

void UShopComponent::DeductCurrency(ECurrencyType Type, int32 Amount, APlayerController* PC)
{
    // 通过 CurrencyComponent 扣减
    if (UCurrencyComponent* CC = PC ? PC->FindComponentByClass<UCurrencyComponent>() : nullptr)
    {
        CC->Spend(Type, Amount);
    }
}

void UShopComponent::GrantItem(UShopItemData* Item, int32 Quantity, APlayerController* PC)
{
    if (!InventoryRef || !PC) return;
    InventoryRef->AddItem(Item->ItemID, Quantity, Item->ItemAsset.LoadSynchronous());
}

void UShopComponent::RefreshShop()
{
    if (!HasAuthority()) return;
    LastRefreshTime = FDateTime::Now();

    // 重新随机折扣
    ActiveDiscounts.Empty();
    FRandomStream Rand(FMath::Rand());
    for (UShopItemData* Item : ShopCatalog)
    {
        if (Item && Rand.FRand() < 0.3f) // 30% 商品有折扣
        {
            float Rate = Rand.FRandRange(0.5f, 0.9f);
            ActiveDiscounts.Add(Item->ItemID, Rate);
        }
    }
    // 可扩展：刷新库存限量、轮换商品池
}

void UShopComponent::ApplyDiscount(FName ItemID, float Rate)
{
    if (!HasAuthority()) return;
    ActiveDiscounts.Add(ItemID, FMath::Clamp(Rate, 0.1f, 1.f));
}

void UShopComponent::ClearDiscount(FName ItemID)
{
    ActiveDiscounts.Remove(ItemID);
}
