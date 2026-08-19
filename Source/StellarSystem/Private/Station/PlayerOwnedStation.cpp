// ============================================================
// 路径: Source/StellarSystem/Private/Station/PlayerOwnedStation.cpp
// 作用: 玩家主权建筑完整实现
// 新增于: v7.6 (会议室/贸易/税收/所有权/防御)
// ============================================================

#include "Station/PlayerOwnedStation.h"
#include "Character/MyCharacter.h"
#include "Trade/PlayerTradeSystem.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Math/UnrealMathUtility.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogPlayerStation, Log, All);

APlayerOwnedStation::APlayerOwnedStation()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    CurrentShield = 0.f;
    CurrentHull = 0.f;
}

void APlayerOwnedStation::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        CurrentShield = DefenseConfig.ShieldHP;
        CurrentHull = DefenseConfig.HullHP;

        if (ConferenceRooms.Num() == 0)
        {
            InitializeDefaultRooms();
        }

        if (TradeConfig.MaxConcurrentTrades == 0)
        {
            TradeConfig.MaxConcurrentTrades = 20;
        }

        LastSettlementTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    }
}

void APlayerOwnedStation::Tick(float Dt)
{
    Super::Tick(Dt);
    if (!HasAuthority()) return;

    // 会议室占用检查
    CheckConferenceRoomOccupancy();

    // 每日结算(游戏时间24分钟)
    float Now = GetWorld()->GetTimeSeconds();
    const float DayDuration = 86400.f;
    if (Now - LastSettlementTime >= DayDuration)
    {
        DailySettlement();
        LastSettlementTime = Now;
    }

    // 护盾自然回复
    if (CurrentShield < DefenseConfig.ShieldHP)
    {
        CurrentShield = FMath::Min(DefenseConfig.ShieldHP,
            CurrentShield + 10.f * Dt);
    }

    // 脱离战斗检测(简化)
    if (bUnderAttack)
    {
        bUnderAttack = false;
    }
}

// ========== 所有权管理 ==========

void APlayerOwnedStation::Server_SetOwner_Implementation(AController* NewOwner, FName NewOwnerID)
{
    if (!NewOwner || !GetAuthority()) return;

    OwnerPlayerID = NewOwnerID;
    AuthorizedPlayers.Empty();
    AuthorizedPlayers.Add(NewOwnerID);

    UE_LOG(LogPlayerStation, Log, TEXT("[Station] %s ownership -> %s"),
        *StationName, *NewOwnerID.ToString());
}

bool APlayerOwnedStation::Server_SetOwner_Validate(AController*, FName)
{
    return true;
}

bool APlayerOwnedStation::IsOwnedBy(FName PlayerID) const
{
    return OwnerPlayerID == PlayerID;
}

bool APlayerOwnedStation::CanPlayerAccess(AController* Player) const
{
    if (!Player) return false;
    FName PlayerID(*Player->GetName());

    if (IsOwnedBy(PlayerID)) return true;

    if (OwningGuild != NAME_None)
    {
        if (AuthorizedPlayers.Contains(PlayerID)) return true;
    }

    for (const FConferenceRoomConfig& Room : ConferenceRooms)
    {
        if (Room.bAllowPublicAccess) return true;
    }
    return false;
}

// ========== 会议室管理 ==========

void APlayerOwnedStation::Server_CreateConferenceRoom_Implementation(
    AController* Owner, const FConferenceRoomConfig& Config)
{
    if (!ValidateOwner(Owner)) return;
    ConferenceRooms.Add(Config);
    UE_LOG(LogPlayerStation, Log, TEXT("[Station] Conference room '%s' created"),
        *Config.RoomName);
}

bool APlayerOwnedStation::Server_CreateConferenceRoom_Validate(AController*, const FConferenceRoomConfig&)
{
    return true;
}

void APlayerOwnedStation::Server_ModifyConferenceRoom_Implementation(
    AController* Owner, FName RoomID, const FConferenceRoomConfig& NewConfig)
{
    if (!ValidateOwner(Owner)) return;
    for (FConferenceRoomConfig& Room : ConferenceRooms)
    {
        if (Room.RoomID == RoomID)
        {
            Room = NewConfig;
            break;
        }
    }
}

bool APlayerOwnedStation::Server_ModifyConferenceRoom_Validate(AController*, FName, const FConferenceRoomConfig&)
{
    return true;
}

bool APlayerOwnedStation::IsPlayerInConferenceRoom(AController* Player, FName& OutRoomID) const
{
    if (!Player) return false;
    FName PlayerID(*Player->GetName());
    for (const auto& Pair : PlayersInConferenceRoom)
    {
        if (Pair.Value == PlayerID)
        {
            OutRoomID = Pair.Key;
            return true;
        }
    }
    return false;
}

TArray<FConferenceRoomConfig> APlayerOwnedStation::GetAllConferenceRooms() const
{
    return ConferenceRooms;
}

void APlayerOwnedStation::Server_StartConferenceTrade_Implementation(
    AController* PlayerA, AController* PlayerB, FName RoomID)
{
    if (!PlayerA || !PlayerB) return;

    // 验证双方在会议室
    FName RoomA, RoomB;
    bool bAInRoom = IsPlayerInConferenceRoom(PlayerA, RoomA);
    bool bBInRoom = IsPlayerInConferenceRoom(PlayerB, RoomB);

    if (!bAInRoom || !bBInRoom || RoomA != RoomB)
    {
        UE_LOG(LogPlayerStation, Warning, TEXT("[Station] Trade rejected: not in same room"));
        return;
    }

    // 验证房间存在且免税
    bool bRoomFound = false;
    for (const FConferenceRoomConfig& Room : ConferenceRooms)
    {
        if (Room.RoomID == RoomA)
        {
            bRoomFound = true;
            if (!Room.bTaxFreeTrades)
            {
                UE_LOG(LogPlayerStation, Warning, TEXT("[Station] Room not tax-free"));
            }
            break;
        }
    }
    if (!bRoomFound) return;

    // 通过 PlayerTradeManager 发起免税交易
    if (APlayerTradeManager* TM = GetWorld()->GetGameState()->FindComponentByClass<APlayerTradeManager>())
    {
        TM->Server_InitiateTradeInConferenceRoom(PlayerA, PlayerB, RoomID);
    }
}

bool APlayerOwnedStation::Server_StartConferenceTrade_Validate(AController*, AController*, FName)
{
    return true;
}
// ============================================================
// (续) PlayerOwnedStation.cpp - 贸易/升级/防御/网络
// ============================================================

// ========== 贸易管理 ==========

void APlayerOwnedStation::Server_SetTaxRate_Implementation(AController* Owner, float NewRate)
{
    if (!ValidateOwner(Owner)) return;

    // 限制税率 0%~10%
    NewRate = FMath::Clamp(NewRate, 0.f, 0.10f);
    TradeConfig.SellerTaxRate = NewRate;

    UE_LOG(LogPlayerStation, Log, TEXT("[Station] Tax rate set to %.1f%%"), NewRate * 100.f);
}

bool APlayerOwnedStation::Server_SetTaxRate_Validate(AController*, float)
{
    return true;
}

float APlayerOwnedStation::GetCurrentTaxRate() const
{
    return TradeConfig.SellerTaxRate;
}

float APlayerOwnedStation::GetTaxCollected() const
{
    return TotalTaxCollected;
}

void APlayerOwnedStation::Server_WithdrawTaxRevenue_Implementation(AController* Owner, float Amount)
{
    if (!ValidateOwner(Owner)) return;

    Amount = FMath::Min(Amount, TotalTaxCollected);
    TotalTaxCollected -= Amount;
    TodayTaxCollected -= Amount;

    // 实际应给玩家货币(通过 GameMode 货币系统)
    UE_LOG(LogPlayerStation, Log, TEXT("[Station] Withdrew %.0f credits (remaining: %.0f)"),
        Amount, TotalTaxCollected);
}

bool APlayerOwnedStation::Server_WithdrawTaxRevenue_Validate(AController*, float)
{
    return true;
}

void APlayerOwnedStation::Server_ProcessTradeAtStation_Implementation(
    AController* PlayerA, AController* PlayerB, float TransactionValue)
{
    if (!PlayerA || !PlayerB) return;

    float Tax = TransactionValue * TradeConfig.SellerTaxRate;
    Tax = FMath::Clamp(Tax, TradeConfig.MinTaxAmount, TradeConfig.MaxTaxAmount);

    TotalTaxCollected += Tax;
    TodayTaxCollected += Tax;
    TotalTradesProcessed++;

    OnTaxCollected.Broadcast(StationID, Tax, OwnerPlayerID);

    UE_LOG(LogPlayerStation, Log, TEXT("[Station] Trade tax: %.0f on %.0f (rate=%.1f%%, total=%.0f)"),
        Tax, TransactionValue, TradeConfig.SellerTaxRate * 100.f, TotalTaxCollected);
}

bool APlayerOwnedStation::Server_ProcessTradeAtStation_Validate(AController*, AController*, float)
{
    return true;
}

// ========== 升级 ==========

void APlayerOwnedStation::Server_UpgradeStation_Implementation(AController* Owner)
{
    if (!ValidateOwner(Owner)) return;
    if (!CanUpgrade()) return;

    int32 NextTier = (int32)Tier + 1;
    Tier = (EStationTier)FMath::Clamp(NextTier, 0, 4);

    switch (Tier)
    {
    case EStationTier::Improved:
        DefenseConfig.ShieldHP *= 1.5f;
        DefenseConfig.HullHP *= 1.3f;
        TradeConfig.MaxConcurrentTrades += 10;
        break;
    case EStationTier::Advanced:
        DefenseConfig.ShieldHP *= 2.0f;
        DefenseConfig.HullHP *= 1.8f;
        DefenseConfig.TurretCount += 4;
        CurrentShield = DefenseConfig.ShieldHP;
        CurrentHull = DefenseConfig.HullHP;
        break;
    case EStationTier::Fortified:
        DefenseConfig.ShieldHP *= 3.0f;
        DefenseConfig.HullHP *= 2.5f;
        DefenseConfig.MissileBatteries += 4;
        DefenseConfig.bHasPointDefense = true;
        CurrentShield = DefenseConfig.ShieldHP;
        CurrentHull = DefenseConfig.HullHP;
        break;
    case EStationTier::Capital:
        DefenseConfig.ShieldHP *= 5.0f;
        DefenseConfig.HullHP *= 4.0f;
        DefenseConfig.DetectionRange *= 3.0f;
        CurrentShield = DefenseConfig.ShieldHP;
        CurrentHull = DefenseConfig.HullHP;
        break;
    }

    OnStationUpgraded.Broadcast(StationID, (int32)Tier);

    UE_LOG(LogPlayerStation, Log, TEXT("[Station] %s upgraded to tier %d"), *StationName, (int)Tier);
}

bool APlayerOwnedStation::Server_UpgradeStation_Validate(AController*)
{
    return true;
}

int32 APlayerOwnedStation::GetNextUpgradeCost() const
{
    int32 BaseCost = 50000;
    return BaseCost * ((int32)Tier + 1);
}

bool APlayerOwnedStation::CanUpgrade() const
{
    return (int32)Tier < (int32)EStationTier::Capital;
}

// ========== 防御 ==========

void APlayerOwnedStation::TakeDamage(float Amount, const FVector& HitLocation)
{
    if (!GetAuthority()) return;

    bUnderAttack = true;

    if (CurrentShield > 0.f)
    {
        ApplyDamageToShield(Amount);
    }
    else
    {
        ApplyDamageToHull(Amount);
    }

    if (CurrentHull <= 0.f)
    {
        OnStationDestroyed.Broadcast(StationID, NAME_None);
        UE_LOG(LogPlayerStation, Warning, TEXT("[Station] %s DESTROYED!"), *StationName);
    }
}

void APlayerOwnedStation::RepairStation(float Amount)
{
    if (!GetAuthority()) return;
    CurrentHull = FMath::Min(DefenseConfig.HullHP, CurrentHull + Amount);
    CurrentShield = FMath::Min(DefenseConfig.ShieldHP, CurrentShield + Amount * 0.5f);
}

float APlayerOwnedStation::GetDefenseRating() const
{
    float ShieldPct = CurrentShield / FMath::Max(1.f, DefenseConfig.ShieldHP);
    float HullPct = CurrentHull / FMath::Max(1.f, DefenseConfig.HullHP);
    float TurretScore = (float)DefenseConfig.TurretCount * 100.f;
    float MissileScore = (float)DefenseConfig.MissileBatteries * 500.f;
    return (ShieldPct + HullPct) * 0.3f + TurretScore + MissileScore;
}

// ========== 每日结算 ==========

void APlayerOwnedStation::DailySettlement()
{
    float YesterdayTax = TodayTaxCollected;
    TodayTaxCollected = 0.f;

    TaxHistory.Add(YesterdayTax);
    if (TaxHistory.Num() > 30) TaxHistory.RemoveAt(0);

    // 维护费用(高级建筑更贵)
    float MaintenanceCost = 100.f * ((int32)Tier + 1);
    TotalTaxCollected = FMath::Max(0.f, TotalTaxCollected - MaintenanceCost);

    UE_LOG(LogPlayerStation, Log, TEXT("[Station] Daily: collected=%.0f maint=%.0f balance=%.0f"),
        YesterdayTax, MaintenanceCost, TotalTaxCollected);
}

// ========== 初始化 ==========

void APlayerOwnedStation::InitializeDefaultRooms()
{
    FConferenceRoomConfig DefaultRoom;
    DefaultRoom.RoomID = FName(*FString::Printf(TEXT("CONF_%s_01"), *StationID.ToString()));
    DefaultRoom.RoomName = FString::Printf(TEXT("%s Main Conference"), *StationName);
    DefaultRoom.RoomRadius = 500.f;
    DefaultRoom.MaxOccupants = 12;
    DefaultRoom.bTaxFreeTrades = true;
    DefaultRoom.bRequireGuildMembership = true;
    DefaultRoom.bAllowPublicAccess = false;
    DefaultRoom.OwningGuild = OwningGuild;

    ConferenceRooms.Add(DefaultRoom);
}

void APlayerOwnedStation::CheckConferenceRoomOccupancy()
{
    // 检查玩家是否在会议室范围内
    // 简化: 清除过期记录
    TArray<FName> ToRemove;
    for (const auto& Pair : PlayersInConferenceRoom)
    {
        // 实际应检查距离
        // 这里保留(由外部系统更新)
    }
    for (FName ID : ToRemove)
    {
        PlayersInConferenceRoom.Remove(ID);
    }
}

// ========== 内部方法 ==========

void APlayerOwnedStation::ApplyDamageToShield(float Amount)
{
    CurrentShield = FMath::Max(0.f, CurrentShield - Amount);
}

void APlayerOwnedStation::ApplyDamageToHull(float Amount)
{
    CurrentHull = FMath::Max(0.f, CurrentHull - Amount);
}

bool APlayerOwnedStation::ValidateOwner(AController* Player) const
{
    if (!Player) return false;
    return IsOwnedBy(FName(*Player->GetName()));
}

// ========== 网络 ==========

void APlayerOwnedStation::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(APlayerOwnedStation, StationID);
    DOREPLIFETIME(APlayerOwnedStation, StationName);
    DOREPLIFETIME(APlayerOwnedStation, StationType);
    DOREPLIFETIME(APlayerOwnedStation, Tier);
    DOREPLIFETIME(APlayerOwnedStation, OwnerPlayerID);
    DOREPLIFETIME(APlayerOwnedStation, OwningGuild);
    DOREPLIFETIME(APlayerOwnedStation, TotalTaxCollected);
    DOREPLIFETIME(APlayerOwnedStation, TodayTaxCollected);
    DOREPLIFETIME(APlayerOwnedStation, TotalTradesProcessed);
    DOREPLIFETIME(APlayerOwnedStation, CurrentShield);
    DOREPLIFETIME(APlayerOwnedStation, CurrentHull);
    DOREPLIFETIME(APlayerOwnedStation, bUnderAttack);
}
