#include "Economy/TradeSystem.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "Math/UnrealMathUtility.h"

// ========== ATradeStation ==========

ATradeStation::ATradeStation()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
}

void ATradeStation::BeginPlay()
{
    Super::BeginPlay();

    // 初始化价格
    for (FTradeCommodity& C : Commodities)
    {
        C.CurrentBuyPrice = C.BaseBuyPrice;
        C.CurrentSellPrice = C.BaseSellPrice;
    }
}

void ATradeStation::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    UpdatePrices(DeltaTime);
}

void ATradeStation::UpdatePrices(float DeltaTime)
{
    PriceUpdateTimer += DeltaTime;
    if (PriceUpdateTimer < PriceUpdateInterval) return;
    PriceUpdateTimer = 0.f;

    for (FTradeCommodity& C : Commodities)
    {
        float OldBuy = C.CurrentBuyPrice;
        float OldSell = C.CurrentSellPrice;

        // 供需周期：正弦波模拟
        float Cycle = FMath::Sin((GetWorld()->GetTimeSeconds() / C.SupplyDemandCycle) * 2.f * PI);
        C.SupplyDemandOffset = Cycle;

        // 价格波动
        float Volatility = C.PriceVolatility * (1.f / FMath::Max(EconomicProsperity, 0.1f));
        float Fluctuation = FMath::FRandRange(-Volatility, Volatility);

        // 供需影响
        float DemandFactor = 1.f + C.SupplyDemandOffset * 0.3f;

        // 繁荣度影响
        float ProsperityFactor = FMath::Lerp(1.5f, 0.7f, EconomicProsperity / 2.f);
        ProsperityFactor = FMath::Clamp(ProsperityFactor, 0.5f, 2.f);

        // 计算新价格
        C.CurrentBuyPrice = C.BaseBuyPrice * DemandFactor * ProsperityFactor * (1.f + Fluctuation);
        C.CurrentSellPrice = C.BaseSellPrice * DemandFactor * ProsperityFactor * (1.f + Fluctuation * 0.5f);

        // 通知变化
        if (FMath::Abs(OldBuy - C.CurrentBuyPrice) > 0.01f)
        {
            OnPriceChanged.Broadcast(C.CommodityID, OldBuy, C.CurrentBuyPrice);
        }
    }

    // 【Fix 3】刷新价格缓存
    BuyPriceCache.Empty(Commodities.Num());
    SellPriceCache.Empty(Commodities.Num());
    for (const FTradeCommodity& C : Commodities)
    {
        BuyPriceCache.Add(C.CommodityID, C.CurrentBuyPrice);
        SellPriceCache.Add(C.CommodityID, C.CurrentSellPrice);
    }
    if (GetWorld())
    {
        CacheValidUntil = GetWorld()->GetTimeSeconds() + CacheDuration;
    }
}

float ATradeStation::GetBuyPrice(FName CommodityID) const
{
    // 【Fix 3】O(1) 缓存查询
    if (GetWorld() && GetWorld()->GetTimeSeconds() < CacheValidUntil)
    {
        if (const float* Cached = BuyPriceCache.Find(CommodityID))
        {
            return *Cached;
        }
    }
    // 缓存未命中或过期：线性查找（UpdatePrices 会刷新缓存）
    for (const FTradeCommodity& C : Commodities)
    {
        if (C.CommodityID == CommodityID) return C.CurrentBuyPrice;
    }
    return -1.f;
}

float ATradeStation::GetSellPrice(FName CommodityID) const
{
    // 【Fix 3】O(1) 缓存查询
    if (GetWorld() && GetWorld()->GetTimeSeconds() < CacheValidUntil)
    {
        if (const float* Cached = SellPriceCache.Find(CommodityID))
        {
            return *Cached;
        }
    }
    for (const FTradeCommodity& C : Commodities)
    {
        if (C.CommodityID == CommodityID) return C.CurrentSellPrice;
    }
    return -1.f;
}

void ATradeStation::Server_BuyFromPlayer_Implementation(APawn* Player, FName CommodityID, float Amount)
{
    if (!HasAuthority() || !Player) return;

    // 找到商品
    for (FTradeCommodity& C : Commodities)
    {
        if (C.CommodityID != CommodityID) continue;

        // 检查站库存
        if (C.StockAmount >= C.MaxStock) return; // 满了

        float ActualAmount = FMath::Min(Amount, C.MaxStock - C.StockAmount);
        float Cost = ActualAmount * C.CurrentBuyPrice;

        // 扣玩家货币、加站的库存
        // 通过 CurrencyComponent 接口
        // UCurrencyComponent* Curr = Player->FindComponentByClass<UCurrencyComponent>();
        // if (Curr && Curr->SpendCredits(Cost))
        // {
        //     C.StockAmount += ActualAmount;
        // }

        break;
    }
}

void ATradeStation::Server_SellToPlayer_Implementation(APawn* Player, FName CommodityID, float Amount)
{
    if (!HasAuthority() || !Player) return;

    for (FTradeCommodity& C : Commodities)
    {
        if (C.CommodityID != CommodityID) continue;

        // 检查库存
        if (C.StockAmount <= 0) return;

        float ActualAmount = FMath::Min(Amount, C.StockAmount);
        float Revenue = ActualAmount * C.CurrentSellPrice;

        // 给玩家货币、减库存
        // UCurrencyComponent* Curr = Player->FindComponentByClass<UCurrencyComponent>();
        // if (Curr)
        // {
        //     Curr->AddCredits(Revenue);
        //     C.StockAmount -= ActualAmount;
        // }

        break;
    }
}

// ========== UTradeNetwork ==========

void UTradeNetwork::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    AllStations.Empty();
    GlobalTime = 0.f;
}

void UTradeNetwork::Deinitialize()
{
    AllStations.Empty();
    Super::Deinitialize();
}

void UTradeNetwork::Tick(float DeltaTime)
{
    GlobalTime += DeltaTime;

    // 全局经济事件倒计时
    if (EventRemainingTime > 0.f)
    {
        EventRemainingTime -= DeltaTime;
        if (EventRemainingTime <= 0.f)
        {
            ActiveEvent = NAME_None;
            EventMagnitude = 0.f;
        }
    }
}

void UTradeNetwork::RegisterStation(ATradeStation* Station)
{
    if (Station && !AllStations.Contains(Station))
    {
        AllStations.Add(Station);
    }
}

void UTradeNetwork::UnregisterStation(ATradeStation* Station)
{
    AllStations.Remove(Station);
}

TArray<ATradeStation*> UTradeNetwork::GetAllStations() const
{
    return AllStations;
}

TArray<FTradeRoute> UTradeNetwork::CalculateBestRoutes(FName FromStationID, int32 MaxResults) const
{
    TArray<FTradeRoute> Results;

    // 找起点站
    ATradeStation* FromStation = nullptr;
    for (ATradeStation* S : AllStations)
    {
        if (S && S->StationID == FromStationID)
        {
            FromStation = S;
            break;
        }
    }
    if (!FromStation) return Results;

    // 遍历所有其他站，找利润差
    for (ATradeStation* ToStation : AllStations)
    {
        if (ToStation == FromStation || !ToStation) continue;

        for (const FTradeCommodity& Commodity : FromStation->Commodities)
        {
            float ToPrice = ToStation->GetBuyPrice(Commodity.CommodityID);
            if (ToPrice <= 0) continue;

            float Profit = ToPrice - Commodity.CurrentSellPrice;
            if (Profit <= 0) continue;

            float Dist = FVector::Dist(FromStation->GetActorLocation(), ToStation->GetActorLocation());

            FTradeRoute Route;
            Route.FromStation = FromStationID;
            Route.ToStation = ToStation->StationID;
            Route.CommodityID = Commodity.CommodityID;
            Route.ProfitPerUnit = Profit;
            Route.Distance = Dist;
            Route.ProfitPerKM = Profit / FMath::Max(Dist / 100000.f, 0.1f); // per km

            Results.Add(Route);
        }
    }

    // 按利润排序
    Results.Sort([](const FTradeRoute& A, const FTradeRoute& B)
    {
        return A.ProfitPerUnit > B.ProfitPerUnit;
    });

    if (Results.Num() > MaxResults)
        Results.SetNum(MaxResults);

    return Results;
}

float UTradeNetwork::GetPriceDifference(FName CommodityID, FName StationA, FName StationB) const
{
    float PriceA = -1.f, PriceB = -1.f;

    for (ATradeStation* S : AllStations)
    {
        if (!S) continue;
        if (S->StationID == StationA) PriceA = S->GetSellPrice(CommodityID);
        if (S->StationID == StationB) PriceB = S->GetBuyPrice(CommodityID);
    }

    if (PriceA < 0 || PriceB < 0) return 0.f;
    return PriceB - PriceA;
}

void UTradeNetwork::TriggerEconomicEvent(FName EventType, float Magnitude, float Duration)
{
    ActiveEvent = EventType;
    EventMagnitude = Magnitude;
    EventRemainingTime = Duration;

    // 影响所有站的价格
    for (ATradeStation* S : AllStations)
    {
        if (!S) continue;

        for (FTradeCommodity& C : S->Commodities)
        {
            if (EventType == FName("Boom"))
            {
                C.CurrentBuyPrice *= (1.f + Magnitude * 0.5f);
                C.CurrentSellPrice *= (1.f + Magnitude * 0.3f);
            }
            else if (EventType == FName("Crisis"))
            {
                C.CurrentBuyPrice *= (1.f - Magnitude * 0.4f);
                C.CurrentSellPrice *= (1.f - Magnitude * 0.3f);
            }
            else if (EventType == FName("Shortage"))
            {
                C.CurrentSellPrice *= (1.f + Magnitude * 0.6f);
            }
            else if (EventType == FName("Glut"))
            {
                C.CurrentBuyPrice *= (1.f - Magnitude * 0.5f);
            }
        }
    }
}
