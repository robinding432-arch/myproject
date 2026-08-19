#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MiningSystem.h"
#include "TradeSystem.generated.h"

class AProceduralPlanet;
class AStationBase;

// —— 贸易商品（动态定价） ——
USTRUCT(BlueprintType)
struct FTradeCommodity
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName CommodityID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EOreType OreType = EOreType::Iron;  // 对应矿石类型

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsRefined = false;              // 是精炼品还是原矿

    // 基础价格（Credits）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BaseBuyPrice = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BaseSellPrice = 8.f;

    // 当前动态价格
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float CurrentBuyPrice = 10.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float CurrentSellPrice = 8.f;

    // 价格波动参数
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PriceVolatility = 0.2f;        // 波动率 0~1

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SupplyDemandCycle = 60.f;       // 供需周期（秒）

    // 当前供需偏移（-1 供过于求 ~ +1 供不应求）
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float SupplyDemandOffset = 0.f;

    // 该站点的库存
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float StockAmount = 1000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxStock = 5000.f;
};

// —— 贸易路线（两站之间的利润差） ——
USTRUCT(BlueprintType)
struct FTradeRoute
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FName FromStation;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FName ToStation;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FName CommodityID;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float ProfitPerUnit = 0.f;           // 每单位利润

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float Distance = 0.f;                // 距离（cm）

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float ProfitPerKM = 0.f;             // 每公里利润（效率指标）
};

// —— 单个贸易站/行星的市场 ——
UCLASS(BlueprintType)
class ATradeStation : public AActor
{
    GENERATED_BODY()

public:
    ATradeStation();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // 所属行星/空间站
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName StationID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString StationName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    AActor* LinkedPlanet = nullptr;

    // 该站的商品列表
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade")
    TArray<FTradeCommodity> Commodities;

    // 派系影响（影响价格偏向）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName ControllingFaction;  // 引用派系系统

    // 经济繁荣度（影响所有价格倍率）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade")
    float EconomicProsperity = 1.f;      // 0.5 贫困 ~ 2.0 繁荣

    // 购买（玩家卖出矿石/物品到站）
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Trade")
    void Server_BuyFromPlayer(APawn* Player, FName CommodityID, float Amount);

    // 卖出（玩家从站买东西）
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Trade")
    void Server_SellToPlayer(APawn* Player, FName CommodityID, float Amount);

    // 获取当前价格
    UFUNCTION(BlueprintCallable, Category = "Trade")
    float GetBuyPrice(FName CommodityID) const;

    UFUNCTION(BlueprintCallable, Category = "Trade")
    float GetSellPrice(FName CommodityID) const;

    // 更新价格（每帧调用）
    void UpdatePrices(float DeltaTime);

    // 事件
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPriceChanged, FName, CommodityID, float, OldPrice, float, NewPrice);
    UPROPERTY(BlueprintAssignable, Category = "Trade")
    FOnPriceChanged OnPriceChanged;

private:
    float PriceUpdateTimer = 0.f;
    const float PriceUpdateInterval = 5.f;  // 每 5 秒更新一次

    // 【Fix 3】价格缓存：避免 O(n) 线性查找
    // Key = CommodityID, Value = 缓存的 (BuyPrice, SellPrice, 过期时间戳)
    UPROPERTY()
    TMap<FName, float> BuyPriceCache;
    UPROPERTY()
    TMap<FName, float> SellPriceCache;
    float CacheValidUntil = 0.f;  // 全局缓存过期时间
    const float CacheDuration = 5.f; // 与 PriceUpdateInterval 一致
};

// —— 全局贸易网络（挂在 GameMode 上） ——
UCLASS(BlueprintType)
class UTradeNetwork : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;

    // 注册贸易站
    UFUNCTION(BlueprintCallable, Category = "Trade")
    void RegisterStation(ATradeStation* Station);

    // 注销
    UFUNCTION(BlueprintCallable, Category = "Trade")
    void UnregisterStation(ATradeStation* Station);

    // 获取所有贸易站
    UFUNCTION(BlueprintCallable, Category = "Trade")
    TArray<ATradeStation*> GetAllStations() const;

    // 计算最优贸易路线（从指定站出发）
    UFUNCTION(BlueprintCallable, Category = "Trade")
    TArray<FTradeRoute> CalculateBestRoutes(FName FromStationID, int32 MaxResults = 10) const;

    // 跨站价格套利提示
    UFUNCTION(BlueprintCallable, Category = "Trade")
    float GetPriceDifference(FName CommodityID, FName StationA, FName StationB) const;

    // 全局经济事件（影响所有站）
    UFUNCTION(BlueprintCallable, Category = "Trade")
    void TriggerEconomicEvent(FName EventType, float Magnitude, float Duration);

    // 当前活跃事件
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FName ActiveEvent = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float EventMagnitude = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float EventRemainingTime = 0.f;

private:
    UPROPERTY()
    TArray<ATradeStation*> AllStations;

    // 全局供需趋势（正弦波叠加）
    float GlobalTime = 0.f;
    float EventTimer = 0.f;
};
