// ============================================================
// 路径: Source/StellarSystem/Public/Trade/PlayerTradeSystem.h
// 作用: 玩家↔玩家 交易系统 + NPC 站点交易税 + 玩家主权建筑交易
// 修改于: v7.6 (NPC站点收税/玩家建筑交易/会议室终端统一操作)
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PlayerTradeSystem.generated.h"

class AMyCharacter;
class UInventoryComponent;
class UCurrencyComponent;
class APlayerOwnedStation;

// 交易状态机
UENUM(BlueprintType)
enum class ETradeStatus : uint8
{
    Idle         UMETA(DisplayName = "空闲"),
    Initiating   UMETA(DisplayName = "发起中"),
    WaitingAccept UMETA(DisplayName = "等待对方接受"),
    Negotiating  UMETA(DisplayName = "议价中"),
    Locked       UMETA(DisplayName = "已锁定(双方确认中)"),
    Completed    UMETA(DisplayName = "已完成"),
    Cancelled    UMETA(DisplayName = "已取消"),
    Failed       UMETA(DisplayName = "失败")
};

// ★ 交易地点类型(决定税收规则)
UENUM(BlueprintType)
enum class ETradeLocationType : uint8
{
    NPCStation        UMETA(DisplayName = "NPC空间站/太空港"),
    PlayerOwnedStation UMETA(DisplayName = "玩家主权建筑"),
    ConferenceRoom  UMETA(DisplayName = "会议室终端(免税)"),
    FreeSpace        UMETA(DisplayName = "自由空间(无税)"),
    ShipInterior     UMETA(DisplayName = "飞船内部")
};

// ★ 税收配置(按地点类型区分)
USTRUCT(BlueprintType)
struct FTradeTaxConfig
{
    GENERATED_BODY()

    // —— NPC 站点税收(默认) ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tax|NPC")
    float NPCSellerTaxRate = 0.05f; // 卖方5%

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tax|NPC")
    float NPCBuyerTaxRate = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tax|NPC")
    FName NPCTaxDestination = FName("Station"); // 归站点所有者

    // —— 玩家主权建筑税收 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tax|PlayerStation")
    float PlayerStationSellerTaxRate = 0.03f; // 卖方3%(更低吸引交易)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tax|PlayerStation")
    float PlayerStationBuyerTaxRate = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tax|PlayerStation")
    bool bTaxGoesToStationOwner = true; // 税收归建筑所有者

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tax|PlayerStation")
    bool bOwnerCanSetTaxRate = true; // 所有者可调税率

    // —— 会议室终端(无税/公会特权) ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tax|Conference")
    bool bConferenceRoomTaxFree = true; // 会议室交易免税

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tax|Conference")
    bool bRequireGuildMembership = true; // 需同公会

    // —— 通用 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tax")
    float MinTaxAmount = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tax")
    float MaxTaxAmount = 10000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tax")
    FName TaxCurrency = FName("Credits");

    // 友好派系减免
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tax|Discount")
    float FriendlyFactionDiscount = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tax|Discount")
    bool bAllianceTaxFree = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tax|Discount")
    float GuildMemberDiscount = 0.3f;
};

// 单个交易槽位
USTRUCT(BlueprintType)
struct FTradeSlot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FName ItemID;

    UPROPERTY(BlueprintReadOnly)
    int32 Quantity = 0;

    UPROPERTY(BlueprintReadOnly)
    float UnitPrice = 0.f;

    UPROPERTY(BlueprintReadOnly)
    FString DisplayName;

    UPROPERTY(BlueprintReadOnly)
    bool bIsEquipped = false;
};

// 玩家报价(一侧)
USTRUCT(BlueprintType)
struct FTradeOffer
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString PlayerNetID;

    UPROPERTY(BlueprintReadOnly)
    TArray<FTradeSlot> Items;

    UPROPERTY(BlueprintReadOnly)
    float CreditsOffered = 0.f;

    UPROPERTY(BlueprintReadOnly)
    bool bLocked = false;

    UPROPERTY(BlueprintReadOnly)
    bool bAccepted = false;
};

// 完整交易会话(增强: 地点类型/税收明细)
USTRUCT(BlueprintType)
struct FTradeSession
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FName TradeID;

    UPROPERTY(BlueprintReadOnly)
    FString TraderA;

    UPROPERTY(BlueprintReadOnly)
    FString TraderB;

    UPROPERTY(BlueprintReadOnly)
    FTradeOffer OfferA;

    UPROPERTY(BlueprintReadOnly)
    FTradeOffer OfferB;

    UPROPERTY(BlueprintReadOnly)
    ETradeStatus Status = ETradeStatus::Idle;

    // ★ 地点信息
    UPROPERTY(BlueprintReadOnly)
    ETradeLocationType LocationType = ETradeLocationType::NPCStation;

    UPROPERTY(BlueprintReadOnly)
    FName LocationID = NAME_None;

    UPROPERTY(BlueprintReadOnly)
    FName LocationOwnerID = NAME_None; // 玩家主权建筑所有者

    UPROPERTY(BlueprintReadOnly)
    float CreatedAt = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float ExpiresAt = 0.f;

    // ★ 税收信息(明细)
    UPROPERTY(BlueprintReadOnly)
    float SellerTax = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float BuyerTax = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float TotalTax = 0.f;

    UPROPERTY(BlueprintReadOnly)
    FName TaxDestination = FName("Station");

    UPROPERTY(BlueprintReadOnly)
    bool bTaxApplied = false;

    // 是否通过会议室终端
    UPROPERTY(BlueprintReadOnly)
    bool bIsConferenceTrade = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnTradeRequested, FName, TradeID, FString, FromPlayer, FString, ToPlayer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnTradeStatusChanged, FName, TradeID, ETradeStatus, OldStatus, ETradeStatus, NewStatus, FString, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnTradeCompleted, FName, TradeID, FString, TraderA, FString, TraderB);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnTradeTaxApplied, FName, TradeID, float, SellerTax, float, BuyerTax, FName, Destination);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnTradeFailed, FName, TradeID, FString, Reason, bool, bSecurityViolation);
// ★ 新增: 玩家建筑税收事件
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPlayerStationTaxCollected, FName, StationID, FName, OwnerID, float, TaxAmount);

// ============================================================
// 玩家交易管理器（挂在 GameMode / WorldSubsystem）
// ★ 修改于v7.6: 统一在所有站点(含玩家主权建筑会议室)操作
// ============================================================
UCLASS(BlueprintType)
class APlayerTradeManager : public AActor
{
    GENERATED_BODY()

public:
    APlayerTradeManager();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // —— 距离参数 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade|Range")
    float MaxTradeDistance = 500.f; // 面对面交易

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade|Range")
    float ShipTradeDistance = 2000.f; // 飞船间交易

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade|Range")
    float ConferenceRoomRange = 300.f; // 会议室终端范围

    // —— 税收配置(默认) ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade|Tax")
    FTradeTaxConfig DefaultTaxConfig;

    // ★ 按站点覆盖税收(玩家主权建筑可自定义)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade|Tax")
    TMap<FName, FTradeTaxConfig> PerStationTaxConfig;

    // —— 限制 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade|Limits")
    int32 MaxItemsPerSide = 20;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade|Limits")
    float MaxCreditsPerTrade = 10000000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade|Limits")
    float MinCreditsPerTrade = 1.f;

    // —— 超时 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade|Timing")
    float TradeTimeout = 120.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade|Timing")
    float LockTimeout = 30.f;

    // —— 安全 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade|Security")
    bool bRequireLineOfSight = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade|Security")
    bool bLogAllTrades = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade|Security")
    int32 MaxTradesPerMinute = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade|Security")
    bool bAllowTradingWhileInCombat = false;

    // ========== ★ 发起交易(增强: 自动检测地点类型) ==========
    // 玩家靠近另一玩家发起
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Trade")
    void Server_InitiateTrade(AController* Initiator, AController* Target,
                              FName LocationID = NAME_None);

    // ★ 在会议室终端发起(免税)
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Trade|Conference")
    void Server_InitiateTradeInConferenceRoom(AController* Initiator, AController* Target,
                                             FName ConferenceRoomID);

    // ★ 在玩家主权建筑发起(收税归所有者)
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Trade|PlayerStation")
    void Server_InitiateTradeAtPlayerStation(AController* Initiator, AController* Target,
                                            FName StationID);

    // ========== 响应交易 ==========
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Trade")
    void Server_AcceptTrade(AController* Target, FName TradeID);

    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Trade")
    void Server_RejectTrade(AController* Target, FName TradeID, FString Reason);

    // ========== 添加/移除物品 ==========
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Trade")
    void Server_AddItemToTrade(AController* Player, FName TradeID, FName ItemID, int32 Quantity, float UnitPrice);

    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Trade")
    void Server_RemoveItemFromTrade(AController* Player, FName TradeID, FName ItemID);

    // ========== 货币报价 ==========
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Trade")
    void Server_SetCreditsOffer(AController* Player, FName TradeID, float Amount);

    // ========== 锁定/确认 ==========
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Trade")
    void Server_LockOffer(AController* Player, FName TradeID);

    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Trade")
    void Server_ConfirmTrade(AController* Player, FName TradeID);

    // ========== 取消 ==========
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Trade")
    void Server_CancelTrade(AController* Player, FName TradeID, FString Reason);

    // ========== 查询 ==========
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Trade")
    FTradeSession GetTradeSession(FName TradeID) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Trade")
    TArray<FTradeSession> GetActiveTradesForPlayer(AController* Player) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Trade")
    TArray<FString> GetNearbyPlayersForTrade(AController* Player) const;

    // 计算预估税额(不执行)
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Trade|Tax")
    float CalculateEstimatedTax(float TransactionAmount, FName LocationID, FString PlayerNetID) const;

    // 获取某站点的税率
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Trade|Tax")
    float GetStationTaxRate(FName LocationID) const;

    // ★ 获取会议室是否免税
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Trade|Conference")
    bool IsConferenceRoomTaxFree(FName ConferenceRoomID) const;

    // ========== ★ NPC 站点交易(带税收) ==========
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Trade|NPC")
    void Server_SellToNPCStation(AController* Player, FName StationID, FName ItemID, int32 Quantity, float UnitPrice);

    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Trade|NPC")
    void Server_BuyFromNPCStation(AController* Player, FName StationID, FName ItemID, int32 Quantity, float UnitPrice);

    // ★ 在玩家主权建筑交易(税收归所有者)
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Trade|PlayerStation")
    void Server_TradeAtPlayerStation(AController* Player, FName StationID, FName ItemID, int32 Quantity, float UnitPrice, bool bIsSelling);

    // ★ 设置玩家建筑税率(所有者专用)
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Trade|PlayerStation")
    void Server_SetPlayerStationTaxRate(AController* Owner, FName StationID, float NewTaxRate);

    // ========== 事件 ==========
    UPROPERTY(BlueprintAssignable, Category = "Trade|Events")
    FOnTradeRequested OnTradeRequested;

    UPROPERTY(BlueprintAssignable, Category = "Trade|Events")
    FOnTradeStatusChanged OnTradeStatusChanged;

    UPROPERTY(BlueprintAssignable, Category = "Trade|Events")
    FOnTradeCompleted OnTradeCompleted;

    UPROPERTY(BlueprintAssignable, Category = "Trade|Events")
    FOnTradeTaxApplied OnTradeTaxApplied;

    UPROPERTY(BlueprintAssignable, Category = "Trade|Events")
    FOnTradeFailed OnTradeFailed;

    UPROPERTY(BlueprintAssignable, Category = "Trade|Events")
    FOnPlayerStationTaxCollected OnPlayerStationTaxCollected;

    // ========== 网络 ==========
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

private:
    UPROPERTY(Replicated)
    TMap<FName, FTradeSession> ActiveTrades;

    UPROPERTY()
    TMap<FString, TArray<float>> PlayerTradeTimestamps;

    // 税收记录(审计)
    UPROPERTY()
    TArray<FTradeSession> TaxAuditLog;

    // 过期检查
    void TickExpireTrades(float CurrentTime);

    // 距离校验
    bool ValidateTradeDistance(AController* A, AController* B) const;

    // 视线校验
    bool HasTradeLineOfSight(AController* A, AController* B) const;

    // ★ 执行交易(根据地点类型应用不同税收)
    void ExecuteTrade(FTradeSession& Session);

    // ★ 计算并扣除税收(核心)
    void ApplyTax(FTradeSession& Session);

    // 获取税收配置(优先站点特定)
    FTradeTaxConfig GetTaxConfig(FName LocationID, ETradeLocationType LocType) const;

    // 检查玩家派系关系(减免)
    float GetFactionTaxMultiplier(FString PlayerNetID, FName LocationID) const;

    // 记录审计
    void AuditTrade(const FTradeSession& Session);

    // 频率检查
    bool CheckRateLimit(FString NetID, float CurrentTime);

    // 生成 ID
    FName GenerateTradeID() const;

    // 获取玩家报价引用
    FTradeOffer* GetOffer(FTradeSession& Session, FString PlayerNetID);
    const FTradeOffer* GetOffer(const FTradeSession& Session, FString PlayerNetID) const;

    // ★ 检测交易地点类型
    ETradeLocationType DetectLocationType(FName LocationID, AController* Player) const;

    // ★ 检查是否在会议室
    bool IsInConferenceRoom(AController* Player, FName& OutRoomID) const;
};
