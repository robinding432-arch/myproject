// ============================================================
// 路径: Source/StellarSystem/Public/Station/PlayerOwnedStation.h
// 作用: 玩家主权建筑(空间站/太空港/会议室/贸易中心)
// 新增于: v7.6 (会议室终端/贸易/税收/所有权)
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PlayerOwnedStation.generated.h"

class AMyCharacter;
class AController;

// 建筑类型
UENUM(BlueprintType)
enum class EPlayerStationType : uint8
{
    ConferenceHall  UMETA(DisplayName = "会议室大厅(免税交易)"),
    TradeCenter     UMETA(DisplayName = "贸易中心(收税交易)"),
    Spaceport       UMETA(DisplayName = "私人太空港"),
    MiningOutpost   UMETA(DisplayName = "采矿前哨"),
    DefensePlatform UMETA(DisplayName = "防御平台"),
    ResearchLab     UMETA(DisplayName = "研究实验室"),
    LivingQuarters  UMETA(DisplayName = "居住区"),
    MAX
};

// 建筑升级等级
UENUM(BlueprintType)
enum class EStationTier : uint8
{
    Basic      UMETA(DisplayName = "基础(1级)"),
    Improved   UMETA(DisplayName = "改进(2级)"),
    Advanced   UMETA(DisplayName = "高级(3级)"),
    Fortified  UMETA(DisplayName = "强化(4级)"),
    Capital    UMETA(DisplayName = "首都级(5级)")
};

// 会议室配置
USTRUCT(BlueprintType)
struct FConferenceRoomConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName RoomID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString RoomName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector RoomLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RoomRadius = 500.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxOccupants = 12;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bTaxFreeTrades = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bRequireGuildMembership = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bAllowPublicAccess = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName OwningGuild = NAME_None;
};

// 贸易中心配置
USTRUCT(BlueprintType)
struct FTradeCenterConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SellerTaxRate = 0.03f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BuyerTaxRate = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MinTaxAmount = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxTaxAmount = 5000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxConcurrentTrades = 20;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bOwnerCanModifyTax = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DailyRevenueEstimate = 0.f;
};

// 建筑防御配置
USTRUCT(BlueprintType)
struct FStationDefenseConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ShieldHP = 5000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 HullHP = 10000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 TurretCount = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TurretRange = 50000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MissileBatteries = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bHasPointDefense = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DetectionRange = 100000.f;
};
// ============================================================
// 路径: Source/StellarSystem/Public/Station/PlayerOwnedStation.h
// (续) APlayerOwnedStation 类声明
// ============================================================

UCLASS(BlueprintType)
class APlayerOwnedStation : public AActor
{
    GENERATED_BODY()

public:
    APlayerOwnedStation();

    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;

    // ---- 基本信息 ----
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Station")
    FName StationID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Station")
    FString StationName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Station")
    EPlayerStationType StationType = EPlayerStationType::ConferenceHall;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Station")
    EStationTier Tier = EStationTier::Basic;

    // ---- 所有权 ----
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Station|Ownership")
    FName OwnerPlayerID = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Station|Ownership")
    FName OwningGuild = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station|Ownership")
    TArray<FName> AuthorizedPlayers;

    // ---- 经济 ----
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Station|Economy")
    float TotalTaxCollected = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Station|Economy")
    float TodayTaxCollected = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Station|Economy")
    int32 TotalTradesProcessed = 0;

    // ---- 会议室 ----
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station|Conference")
    TArray<FConferenceRoomConfig> ConferenceRooms;

    // ---- 贸易中心 ----
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station|Trade")
    FTradeCenterConfig TradeConfig;

    // ---- 防御 ----
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station|Defense")
    FStationDefenseConfig DefenseConfig;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Station|Defense")
    float CurrentShield = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Station|Defense")
    float CurrentHull = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Station|Defense")
    bool bUnderAttack = false;

    // ========== 核心接口 ==========

    // 所有权管理
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Station|Ownership")
    void Server_SetOwner(AController* NewOwner, FName NewOwnerID);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Station|Ownership")
    bool IsOwnedBy(FName PlayerID) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Station|Ownership")
    bool CanPlayerAccess(AController* Player) const;

    // 会议室管理
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Station|Conference")
    void Server_CreateConferenceRoom(AController* Owner, const FConferenceRoomConfig& Config);

    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Station|Conference")
    void Server_ModifyConferenceRoom(AController* Owner, FName RoomID, const FConferenceRoomConfig& NewConfig);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Station|Conference")
    bool IsPlayerInConferenceRoom(AController* Player, FName& OutRoomID) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Station|Conference")
    TArray<FConferenceRoomConfig> GetAllConferenceRooms() const;

    // 会议室交易接口(免税)
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Station|Conference|Trade")
    void Server_StartConferenceTrade(AController* PlayerA, AController* PlayerB, FName RoomID);

    // 贸易管理
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Station|Trade")
    void Server_SetTaxRate(AController* Owner, float NewRate);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Station|Trade")
    float GetCurrentTaxRate() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Station|Trade")
    float GetTaxCollected() const;

    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Station|Trade")
    void Server_WithdrawTaxRevenue(AController* Owner, float Amount);

    // 贸易中心交易(收税归所有者)
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Station|Trade")
    void Server_ProcessTradeAtStation(AController* PlayerA, AController* PlayerB, float TransactionValue);

    // 升级
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Station|Upgrade")
    void Server_UpgradeStation(AController* Owner);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Station|Upgrade")
    int32 GetNextUpgradeCost() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Station|Upgrade")
    bool CanUpgrade() const;

    // 防御
    UFUNCTION(BlueprintCallable, Category = "Station|Defense")
    void TakeDamage(float Amount, const FVector& HitLocation);

    UFUNCTION(BlueprintCallable, Category = "Station|Defense")
    void RepairStation(float Amount);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Station|Defense")
    float GetDefenseRating() const;

    // 每日结算
    UFUNCTION(BlueprintCallable, Category = "Station|Economy")
    void DailySettlement();

    // ========== 事件 ==========
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnTaxCollected, FName, StationID, float, Amount, FName, OwnerID);
    UPROPERTY(BlueprintAssignable, Category = "Station|Events")
    FOnTaxCollected OnTaxCollected;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStationUpgraded, FName, StationID, int32, NewTier);
    UPROPERTY(BlueprintAssignable, Category = "Station|Events")
    FOnStationUpgraded OnStationUpgraded;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPlayerEnteredConference, FName, RoomID, FName, PlayerID, int32, OccupantCount);
    UPROPERTY(BlueprintAssignable, Category = "Station|Events")
    FOnPlayerEnteredConference OnPlayerEnteredConference;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStationDestroyed, FName, StationID, FName, DestroyerID);
    UPROPERTY(BlueprintAssignable, Category = "Station|Events")
    FOnStationDestroyed OnStationDestroyed;

    // ========== 网络 ==========
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

private:
    UPROPERTY()
    TMap<FName, FName> PlayersInConferenceRoom;

    UPROPERTY()
    TArray<float> TaxHistory;

    float LastSettlementTime = 0.f;

    void InitializeDefaultRooms();
    void CheckConferenceRoomOccupancy();
    void ApplyDamageToShield(float Amount);
    void ApplyDamageToHull(float Amount);
    bool ValidateOwner(AController* Player) const;
};
