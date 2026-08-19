// ============================================================
// 路径: Source/StellarSystem/Private/Trade/PlayerTradeSystem.cpp
// 作用: 玩家↔玩家 交易系统 + NPC 站点交易税 + 玩家主权建筑交易
// 修改于: v7.6 (NPC收税/玩家建筑交易/会议室终端/税收归所有者)
// ============================================================

#include "Trade/PlayerTradeSystem.h"
#include "Character/MyCharacter.h"
#include "Character/InventoryComponent.h"
#include "Character/CurrencyComponent.h"
#include "Station/PlanetarySpaceport.h"
#include "Station/PlayerOwnedStation.h"
#include "Core/StellarGameMode.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Math/UnrealMathUtility.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogPlayerTrade, Log, All);

APlayerTradeManager::APlayerTradeManager()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;

    // 默认税收配置
    DefaultTaxConfig.NPCSellerTaxRate = 0.05f;
    DefaultTaxConfig.NPCBuyerTaxRate = 0.0f;
    DefaultTaxConfig.NPCTaxDestination = FName("Station");
    DefaultTaxConfig.PlayerStationSellerTaxRate = 0.03f;
    DefaultTaxConfig.PlayerStationBuyerTaxRate = 0.0f;
    DefaultTaxConfig.bTaxGoesToStationOwner = true;
    DefaultTaxConfig.bConferenceRoomTaxFree = true;
    DefaultTaxConfig.bRequireGuildMembership = true;
    DefaultTaxConfig.MinTaxAmount = 1.f;
    DefaultTaxConfig.MaxTaxAmount = 10000.f;
    DefaultTaxConfig.TaxCurrency = FName("Credits");
    DefaultTaxConfig.FriendlyFactionDiscount = 0.5f;
    DefaultTaxConfig.bAllianceTaxFree = true;
    DefaultTaxConfig.GuildMemberDiscount = 0.3f;
}

void APlayerTradeManager::BeginPlay()
{
    Super::BeginPlay();
}

void APlayerTradeManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    float Now = GetWorld()->GetTimeSeconds();
    TickExpireTrades(Now);
}

// ==================== 距离校验 ====================

bool APlayerTradeManager::ValidateTradeDistance(AController* A, AController* B) const
{
    if (!A || !B) return false;
    APawn* PawnA = A->GetPawn();
    APawn* PawnB = B->GetPawn();
    if (!PawnA || !PawnB) return false;

    float DistSq = FVector::DistSquared(PawnA->GetActorLocation(), PawnB->GetActorLocation());
    return DistSq <= (MaxTradeDistance * MaxTradeDistance);
}

bool APlayerTradeManager::HasTradeLineOfSight(AController* A, AController* B) const
{
    if (!bRequireLineOfSight) return true;
    if (!A || !B) return false;

    APawn* PawnA = A->GetPawn();
    APawn* PawnB = B->GetPawn();
    if (!PawnA || !PawnB) return false;

    FVector Start = PawnA->GetActorLocation() + FVector(0, 0, 80.f);
    FVector End = PawnB->GetActorLocation() + FVector(0, 0, 80.f);
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(PawnA);
    Params.AddIgnoredActor(PawnB);

    return !GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
}

// ==================== 频率限制 ====================

bool APlayerTradeManager::CheckRateLimit(FString NetID, float CurrentTime)
{
    TArray<float>& Timestamps = PlayerTradeTimestamps.FindOrAdd(NetID);
    Timestamps.RemoveAll([&](float T) { return CurrentTime - T > 60.f; });
    return Timestamps.Num() < MaxTradesPerMinute;
}

// ==================== ★ 地点类型检测 ====================

ETradeLocationType APlayerTradeManager::DetectLocationType(FName LocationID, AController* Player) const
{
    if (LocationID == NAME_None) return ETradeLocationType::FreeSpace;

    // 检查是否是会议室(通过 GameMode)
    FName RoomID;
    if (IsInConferenceRoom(Player, RoomID))
    {
        return ETradeLocationType::ConferenceRoom;
    }

    // 检查是否是玩家主权建筑
    if (AStellarGameMode* GM = Cast<AStellarGameMode>(GetWorld()->GetAuthGameMode()))
    {
        if (GM->IsPlayerOwnedStation(LocationID))
        {
            return ETradeLocationType::PlayerOwnedStation;
        }
    }

    // 默认: NPC 站点
    return ETradeLocationType::NPCStation;
}

bool APlayerTradeManager::IsInConferenceRoom(AController* Player, FName& OutRoomID) const
{
    if (!Player) return false;

    // 通过 GameMode 查询玩家当前所在房间
    if (AStellarGameMode* GM = Cast<AStellarGameMode>(GetWorld()->GetAuthGameMode()))
    {
        return GM->GetPlayerConferenceRoom(Player, OutRoomID);
    }
    return false;
}

// ==================== ★ 发起交易(自动检测地点) ====================

void APlayerTradeManager::Server_InitiateTrade_Implementation(
    AController* Initiator, AController* Target, FName LocationID)
{
    if (!Initiator || !Target || Initiator == Target)
    {
        UE_LOG(LogPlayerTrade, Warning, TEXT("[Trade] 无效请求"));
        return;
    }

    FString InitNet = Initiator->GetName();
    FString TargNet = Target->GetName();
    float Now = GetWorld()->GetTimeSeconds();

    // 频率检查
    if (!CheckRateLimit(InitNet, Now))
    {
        OnTradeFailed.Broadcast(FName(), TEXT("频率超限"), true);
        return;
    }

    // 距离校验
    if (!ValidateTradeDistance(Initiator, Target))
    {
        OnTradeFailed.Broadcast(FName(), TEXT("距离过远"), false);
        return;
    }

    // 视线校验
    if (!HasTradeLineOfSight(Initiator, Target))
    {
        OnTradeFailed.Broadcast(FName(), TEXT("视线被阻挡"), false);
        return;
    }

    // ★ 检测地点类型
    ETradeLocationType LocType = DetectLocationType(LocationID, Initiator);

    // 创建交易会话
    FTradeSession Session;
    Session.TradeID = GenerateTradeID();
    Session.TraderA = InitNet;
    Session.TraderB = TargNet;
    Session.Status = ETradeStatus::WaitingAccept;
    Session.LocationType = LocType;
    Session.LocationID = LocationID;
    Session.CreatedAt = Now;
    Session.ExpiresAt = Now + TradeTimeout;

    // 设置地点所有者
    if (LocType == ETradeLocationType::PlayerOwnedStation)
    {
        if (AStellarGameMode* GM = Cast<AStellarGameMode>(GetWorld()->GetAuthGameMode()))
        {
            Session.LocationOwnerID = GM->GetStationOwnerID(LocationID);
        }
    }

    // 会议室免税
    if (LocType == ETradeLocationType::ConferenceRoom)
    {
        Session.bIsConferenceTrade = true;
    }

    ActiveTrades.Add(Session.TradeID, Session);

    // 记录频率
    PlayerTradeTimestamps.FindOrAdd(InitNet).Add(Now);

    OnTradeRequested.Broadcast(Session.TradeID, InitNet, TargNet);

    UE_LOG(LogPlayerTrade, Log, TEXT("[Trade] Initiated: %s ↔ %s (loc=%d, taxfree=%s)"),
        *InitNet, *TargNet, (int)LocType,
        Session.bIsConferenceTrade ? TEXT("yes") : TEXT("no"));
}

bool APlayerTradeManager::Server_InitiateTrade_Validate(AController*, AController*, FName)
{
    return true;
}

// ==================== ★ 会议室终端发起(免税) ====================

void APlayerTradeManager::Server_InitiateTradeInConferenceRoom_Implementation(
    AController* Initiator, AController* Target, FName ConferenceRoomID)
{
    if (!Initiator || !Target) return;

    // 验证发起人在会议室
    FName CurrentRoom;
    if (!IsInConferenceRoom(Initiator, CurrentRoom))
    {
        OnTradeFailed.Broadcast(FName(), TEXT("不在会议室"), false);
        return;
    }

    // 验证目标也在同一会议室
    FName TargetRoom;
    if (!IsInConferenceRoom(Target, TargetRoom) || TargetRoom != CurrentRoom)
    {
        OnTradeFailed.Broadcast(FName(), TEXT("目标不在同一会议室"), false);
        return;
    }

    // 调用通用发起(地点类型会自动检测为 ConferenceRoom)
    Server_InitiateTrade_Implementation(Initiator, Target, ConferenceRoomID);
}

bool APlayerTradeManager::Server_InitiateTradeInConferenceRoom_Validate(AController*, AController*, FName)
{
    return true;
}

// ==================== ★ 玩家主权建筑发起(收税归所有者) ====================

void APlayerTradeManager::Server_InitiateTradeAtPlayerStation_Implementation(
    AController* Initiator, AController* Target, FName StationID)
{
    if (!Initiator || !Target) return;

    // 验证地点是玩家主权建筑
    if (AStellarGameMode* GM = Cast<AStellarGameMode>(GetWorld()->GetAuthGameMode()))
    {
        if (!GM->IsPlayerOwnedStation(StationID))
        {
            OnTradeFailed.Broadcast(FName(), TEXT("不是玩家主权建筑"), false);
            return;
        }
    }

    Server_InitiateTrade_Implementation(Initiator, Target, StationID);
}

bool APlayerTradeManager::Server_InitiateTradeAtPlayerStation_Validate(AController*, AController*, FName)
{
    return true;
}

// ==================== 响应交易 ====================

void APlayerTradeManager::Server_AcceptTrade_Implementation(AController* Target, FName TradeID)
{
    if (!Target || !ActiveTrades.Contains(TradeID)) return;

    FTradeSession& Session = ActiveTrades[TradeID];
    Session.Status = ETradeStatus::Negotiating;

    OnTradeStatusChanged.Broadcast(TradeID, ETradeStatus::WaitingAccept, ETradeStatus::Negotiating, TEXT(""));
}

bool APlayerTradeManager::Server_AcceptTrade_Validate(AController*, FName)
{
    return true;
}

void APlayerTradeManager::Server_RejectTrade_Implementation(AController* Target, FName TradeID, FString Reason)
{
    if (!ActiveTrades.Contains(TradeID)) return;

    FTradeSession& Session = ActiveTrades[TradeID];
    ETradeStatus OldStatus = Session.Status;
    Session.Status = ETradeStatus::Cancelled;

    OnTradeStatusChanged.Broadcast(TradeID, OldStatus, ETradeStatus::Cancelled, Reason);
    ActiveTrades.Remove(TradeID);
}

bool APlayerTradeManager::Server_RejectTrade_Validate(AController*, FName, FString)
{
    return true;
}

// ==================== 添加/移除物品 ====================

void APlayerTradeManager::Server_AddItemToTrade_Implementation(
    AController* Player, FName TradeID, FName ItemID, int32 Quantity, float UnitPrice)
{
    if (!Player || !ActiveTrades.Contains(TradeID)) return;

    FTradeSession& Session = ActiveTrades[TradeID];
    if (Session.Status != ETradeStatus::Negotiating) return;

    FString PlayerNet = Player->GetName();
    FTradeOffer* Offer = GetOffer(Session, PlayerNet);
    if (!Offer) return;

    // 检查物品是否可交易(如任务物品不可交易)
    if (AMyCharacter* Char = Cast<AMyCharacter>(Player->GetPawn()))
    {
        if (UInventoryComponent* Inv = Char->FindComponentByClass<UInventoryComponent>())
        {
            if (!Inv->CanTradeItem(ItemID, Quantity))
            {
                OnTradeFailed.Broadcast(TradeID, TEXT("物品不可交易或数量不足"), false);
                return;
            }
        }
    }

    FTradeSlot Slot;
    Slot.ItemID = ItemID;
    Slot.Quantity = Quantity;
    Slot.UnitPrice = UnitPrice;
    Slot.DisplayName = ItemID.ToString(); // 实际应从物品数据库查

    Offer->Items.Add(Slot);
    Offer->bLocked = false; // 修改后解锁

    UE_LOG(LogPlayerTrade, Log, TEXT("[Trade] %s added %s x%d @%.0f"), *PlayerNet, *ItemID.ToString(), Quantity, UnitPrice);
}

bool APlayerTradeManager::Server_AddItemToTrade_Validate(AController*, FName, FName, int32, float)
{
    return true;
}

void APlayerTradeManager::Server_RemoveItemFromTrade_Implementation(
    AController* Player, FName TradeID, FName ItemID)
{
    if (!Player || !ActiveTrades.Contains(TradeID)) return;

    FTradeSession& Session = ActiveTrades[TradeID];
    FTradeOffer* Offer = GetOffer(Session, Player->GetName());
    if (!Offer) return;

    Offer->Items.RemoveAll([&](const FTradeSlot& S) { return S.ItemID == ItemID; });
    Offer->bLocked = false;
}

bool APlayerTradeManager::Server_RemoveItemFromTrade_Validate(AController*, FName, FName)
{
    return true;
}

// ==================== 货币报价 ====================

void APlayerTradeManager::Server_SetCreditsOffer_Implementation(
    AController* Player, FName TradeID, float Amount)
{
    if (!Player || !ActiveTrades.Contains(TradeID)) return;
    if (Amount < MinCreditsPerTrade || Amount > MaxCreditsPerTrade) return;

    FTradeSession& Session = ActiveTrades[TradeID];
    FTradeOffer* Offer = GetOffer(Session, Player->GetName());
    if (!Offer) return;

    Offer->CreditsOffered = Amount;
    Offer->bLocked = false;
}

bool APlayerTradeManager::Server_SetCreditsOffer_Validate(AController*, FName, float)
{
    return true;
}

// ==================== 锁定/确认 ====================

void APlayerTradeManager::Server_LockOffer_Implementation(AController* Player, FName TradeID)
{
    if (!Player || !ActiveTrades.Contains(TradeID)) return;

    FTradeSession& Session = ActiveTrades[TradeID];
    FTradeOffer* Offer = GetOffer(Session, Player->GetName());
    if (!Offer) return;

    Offer->bLocked = true;

    // 检查是否双方都锁定
    FTradeOffer* OfferA = GetOffer(Session, Session.TraderA);
    FTradeOffer* OfferB = GetOffer(Session, Session.TraderB);

    if (OfferA && OfferB && OfferA->bLocked && OfferB->bLocked)
    {
        Session.Status = ETradeStatus::Locked;
        // 自动应用税收
        ApplyTax(Session);
        // 自动确认(双方已锁定 = 同意)
        ExecuteTrade(Session);
    }
    else
    {
        Session.Status = ETradeStatus::Locked;
    }
}

bool APlayerTradeManager::Server_LockOffer_Validate(AController*, FName)
{
    return true;
}

void APlayerTradeManager::Server_ConfirmTrade_Implementation(AController* Player, FName TradeID)
{
    if (!Player || !ActiveTrades.Contains(TradeID)) return;

    FTradeSession& Session = ActiveTrades[TradeID];
    FTradeOffer* Offer = GetOffer(Session, Player->GetName());
    if (!Offer) return;

    Offer->bAccepted = true;

    // 检查双方确认
    FTradeOffer* OfferA = GetOffer(Session, Session.TraderA);
    FTradeOffer* OfferB = GetOffer(Session, Session.TraderB);

    if (OfferA && OfferB && OfferA->bAccepted && OfferB->bAccepted)
    {
        ApplyTax(Session);
        ExecuteTrade(Session);
    }
}

bool APlayerTradeManager::Server_ConfirmTrade_Validate(AController*, FName)
{
    return true;
}

// ==================== 取消 ====================

void APlayerTradeManager::Server_CancelTrade_Implementation(
    AController* Player, FName TradeID, FString Reason)
{
    if (!ActiveTrades.Contains(TradeID)) return;

    FTradeSession& Session = ActiveTrades[TradeID];
    ETradeStatus OldStatus = Session.Status;
    Session.Status = ETradeStatus::Cancelled;

    OnTradeStatusChanged.Broadcast(TradeID, OldStatus, ETradeStatus::Cancelled, Reason);
    ActiveTrades.Remove(TradeID);
}

bool APlayerTradeManager::Server_CancelTrade_Validate(AController*, FName, FString)
{
    return true;
}

// ==================== ★ 核心: 执行交易(根据地点类型应用税收) ====================

void APlayerTradeManager::ExecuteTrade(FTradeSession& Session)
{
    // 查找双方 Character
    AController* PlayerA = nullptr;
    AController* PlayerB = nullptr;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        AController* PC = It->Get();
        if (!PC) continue;
        if (PC->GetName() == Session.TraderA) PlayerA = PC;
        else if (PC->GetName() == Session.TraderB) PlayerB = PC;
    }

    if (!PlayerA || !PlayerB) return;

    AMyCharacter* CharA = Cast<AMyCharacter>(PlayerA->GetPawn());
    AMyCharacter* CharB = Cast<AMyCharacter>(PlayerB->GetPawn());
    if (!CharA || !CharB) return;

    UInventoryComponent* InvA = CharA->FindComponentByClass<UInventoryComponent>();
    UInventoryComponent* InvB = CharB->FindComponentByClass<UInventoryComponent>();
    UCurrencyComponent* CurA = CharA->FindComponentByClass<UCurrencyComponent>();
    UCurrencyComponent* CurB = CharB->FindComponentByClass<UCurrencyComponent>();
    if (!InvA || !InvB || !CurA || !CurB) return;

    // 税收已从 ApplyTax() 中扣除到 Session 的 SellerTax/BuyerTax
    // 这里执行实际物品/货币交换

    // A → B 物品转移
    for (const FTradeSlot& Slot : Session.OfferA.Items)
    {
        InvA->RemoveItem(Slot.ItemID, Slot.Quantity);
        InvB->AddItem(Slot.ItemID, Slot.Quantity);
    }

    // B → A 物品转移
    for (const FTradeSlot& Slot : Session.OfferB.Items)
    {
        InvB->RemoveItem(Slot.ItemID, Slot.Quantity);
        InvA->AddItem(Slot.ItemID, Slot.Quantity);
    }

    // 货币转移(已扣税)
    if (Session.OfferA.CreditsOffered > 0.f)
    {
        float AmountA = Session.OfferA.CreditsOffered - Session.BuyerTax;
        CurA->DeductCurrency(DefaultTaxConfig.TaxCurrency, Session.OfferA.CreditsOffered);
        CurB->AddCurrency(DefaultTaxConfig.TaxCurrency, AmountA);
    }
    if (Session.OfferB.CreditsOffered > 0.f)
    {
        float AmountB = Session.OfferB.CreditsOffered - Session.SellerTax;
        CurB->DeductCurrency(DefaultTaxConfig.TaxCurrency, Session.OfferB.CreditsOffered);
        CurA->AddCurrency(DefaultTaxConfig.TaxCurrency, AmountB);
    }

    // 税收去向
    if (Session.TotalTax > 0.f)
    {
        if (Session.LocationType == ETradeLocationType::PlayerOwnedStation && Session.bTaxApplied)
        {
            // ★ 税收归玩家主权建筑所有者
            if (AStellarGameMode* GM = Cast<AStellarGameMode>(GetWorld()->GetAuthGameMode()))
            {
                GM->AddTaxToPlayerStation(Session.LocationID, Session.TotalTax);
            }
            OnPlayerStationTaxCollected.Broadcast(Session.LocationID, Session.LocationOwnerID, Session.TotalTax);
        }
        else if (Session.LocationType == ETradeLocationType::NPCStation)
        {
            // NPC 站点税收归站点所有者(派系/城市)
            if (AStellarGameMode* GM = Cast<AStellarGameMode>(GetWorld()->GetAuthGameMode()))
            {
                GM->AddTaxToStation(Session.LocationID, Session.TotalTax);
            }
        }
        // ConferenceRoom: 无税, 不处理
    }

    // 完成
    Session.Status = ETradeStatus::Completed;
    OnTradeCompleted.Broadcast(Session.TradeID, Session.TraderA, Session.TraderB);
    AuditTrade(Session);
    ActiveTrades.Remove(Session.TradeID);

    UE_LOG(LogPlayerTrade, Log, TEXT("[Trade] Completed: %s ↔ %s (tax=%.0f, loc=%d)"),
        *Session.TraderA, *Session.TraderB, Session.TotalTax, (int)Session.LocationType);
}

// ==================== ★ 核心: 计算并扣除税收 ====================

void APlayerTradeManager::ApplyTax(FTradeSession& Session)
{
    // 会议室免税
    if (Session.bIsConferenceTrade || Session.LocationType == ETradeLocationType::ConferenceRoom)
    {
        Session.SellerTax = 0.f;
        Session.BuyerTax = 0.f;
        Session.TotalTax = 0.f;
        Session.bTaxApplied = false; // 无税
        return;
    }

    // 获取税收配置
    FTradeTaxConfig TaxCfg = GetTaxConfig(Session.LocationID, Session.LocationType);

    // 计算卖方税额(基于卖方提供的物品+货币总价值)
    float SellerValue = Session.OfferA.CreditsOffered;
    for (const FTradeSlot& Slot : Session.OfferA.Items)
    {
        SellerValue += Slot.UnitPrice * Slot.Quantity;
    }

    float SellerTaxRaw = SellerValue * TaxCfg.NPCSellerTaxRate;
    // 买方税(如果有)
    float BuyerValue = Session.OfferB.CreditsOffered;
    for (const FTradeSlot& Slot : Session.OfferB.Items)
    {
        BuyerValue += Slot.UnitPrice * Slot.Quantity;
    }
    float BuyerTaxRaw = BuyerValue * TaxCfg.NPCBuyerTaxRate;

    // 应用封顶/保底
    SellerTaxRaw = FMath::Clamp(SellerTaxRaw, TaxCfg.MinTaxAmount, TaxCfg.MaxTaxAmount);
    BuyerTaxRaw = FMath::Clamp(BuyerTaxRaw, TaxCfg.MinTaxAmount, TaxCfg.MaxTaxAmount);

    // 派系/军团减免
    float SellerMultiplier = GetFactionTaxMultiplier(Session.TraderA, Session.LocationID);
    float BuyerMultiplier = GetFactionTaxMultiplier(Session.TraderB, Session.LocationID);

    Session.SellerTax = SellerTaxRaw * SellerMultiplier;
    Session.BuyerTax = BuyerTaxRaw * BuyerMultiplier;
    Session.TotalTax = Session.SellerTax + Session.BuyerTax;

    // 税收去向
    if (Session.LocationType == ETradeLocationType::PlayerOwnedStation)
    {
        Session.TaxDestination = Session.LocationOwnerID; // 归所有者
    }
    else if (Session.LocationType == ETradeLocationType::NPCStation)
    {
        Session.TaxDestination = TaxCfg.NPCTaxDestination;
    }

    Session.bTaxApplied = Session.TotalTax > 0.f;

    OnTradeTaxApplied.Broadcast(Session.TradeID, Session.SellerTax, Session.BuyerTax, Session.TaxDestination);

    UE_LOG(LogPlayerTrade, Log, TEXT("[Trade] Tax applied: seller=%.0f buyer=%.0f total=%.0f dest=%s"),
        Session.SellerTax, Session.BuyerTax, Session.TotalTax, *Session.TaxDestination.ToString());
}

// ==================== 税收配置获取 ====================

FTradeTaxConfig APlayerTradeManager::GetTaxConfig(FName LocationID, ETradeLocationType LocType) const
{
    // 优先站点特定配置
    if (PerStationTaxConfig.Contains(LocationID))
    {
        return PerStationTaxConfig[LocationID];
    }

    // 根据地点类型返回默认
    if (LocType == ETradeLocationType::PlayerOwnedStation)
    {
        FTradeTaxConfig Cfg = DefaultTaxConfig;
        Cfg.NPCSellerTaxRate = DefaultTaxConfig.PlayerStationSellerTaxRate;
        Cfg.NPCBuyerTaxRate = DefaultTaxConfig.PlayerStationBuyerTaxRate;
        return Cfg;
    }

    return DefaultTaxConfig;
}

float APlayerTradeManager::GetFactionTaxMultiplier(FString PlayerNetID, FName LocationID) const
{
    // 简化: 返回1.0(实际应查询派系关系)
    // 友好派系 0.5x, 同盟 0x, 军团成员 0.7x
    return 1.0f;
}

// ==================== ★ NPC 站点交易(带税收) ====================

void APlayerTradeManager::Server_SellToNPCStation_Implementation(
    AController* Player, FName StationID, FName ItemID, int32 Quantity, float UnitPrice)
{
    if (!Player) return;

    AMyCharacter* Char = Cast<AMyCharacter>(Player->GetPawn());
    if (!Char) return;

    UInventoryComponent* Inv = Char->FindComponentByClass<UInventoryComponent>();
    UCurrencyComponent* Cur = Char->FindComponentByClass<UCurrencyComponent>();
    if (!Inv || !Cur) return;

    // 计算税收
    float GrossValue = Quantity * UnitPrice;
    float TaxRate = GetStationTaxRate(StationID);
    float Tax = GrossValue * TaxRate;
    Tax = FMath::Clamp(Tax, DefaultTaxConfig.MinTaxAmount, DefaultTaxConfig.MaxTaxAmount);

    float NetValue = GrossValue - Tax;

    // 执行: 移除物品, 加货币(税后)
    if (Inv->RemoveItem(ItemID, Quantity))
    {
        Cur->AddCurrency(DefaultTaxConfig.TaxCurrency, NetValue);

        // 税收归站点
        if (AStellarGameMode* GM = Cast<AStellarGameMode>(GetWorld()->GetAuthGameMode()))
        {
            GM->AddTaxToStation(StationID, Tax);
        }

        UE_LOG(LogPlayerTrade, Log, TEXT("[Trade] NPC Sell: %s x%d gross=%.0f tax=%.0f net=%.0f"),
            *ItemID.ToString(), Quantity, GrossValue, Tax, NetValue);
    }
}

bool APlayerTradeManager::Server_SellToNPCStation_Validate(AController*, FName, FName, int32, float)
{
    return true;
}

void APlayerTradeManager::Server_BuyFromNPCStation_Implementation(
    AController* Player, FName StationID, FName ItemID, int32 Quantity, float UnitPrice)
{
    if (!Player) return;

    AMyCharacter* Char = Cast<AMyCharacter>(Player->GetPawn());
    if (!Char) return;

    UInventoryComponent* Inv = Char->FindComponentByClass<UInventoryComponent>();
    UCurrencyComponent* Cur = Char->FindComponentByClass<UCurrencyComponent>();
    if (!Inv || !Cur) return;

    float TotalCost = Quantity * UnitPrice;
    float TaxRate = GetStationTaxRate(StationID);
    float Tax = TotalCost * TaxRate;
    float TotalWithTax = TotalCost + Tax;

    // 检查货币
    if (!Cur->CanAfford(DefaultTaxConfig.TaxCurrency, TotalWithTax)) return;

    // 执行
    if (Inv->AddItem(ItemID, Quantity))
    {
        Cur->DeductCurrency(DefaultTaxConfig.TaxCurrency, TotalWithTax);

        // 税收归站点
        if (AStellarGameMode* GM = Cast<AStellarGameMode>(GetWorld()->GetAuthGameMode()))
        {
            GM->AddTaxToStation(StationID, Tax);
        }
    }
}

bool APlayerTradeManager::Server_BuyFromNPCStation_Validate(AController*, FName, FName, int32, float)
{
    return true;
}

// ==================== ★ 玩家主权建筑交易 ====================

void APlayerTradeManager::Server_TradeAtPlayerStation_Implementation(
    AController* Player, FName StationID, FName ItemID, int32 Quantity, float UnitPrice, bool bIsSelling)
{
    if (!Player) return;

    // 验证是玩家主权建筑
    if (AStellarGameMode* GM = Cast<AStellarGameMode>(GetWorld()->GetAuthGameMode()))
    {
        if (!GM->IsPlayerOwnedStation(StationID))
        {
            OnTradeFailed.Broadcast(FName(), TEXT("不是玩家主权建筑"), false);
            return;
        }
    }

    if (bIsSelling)
    {
        // 卖给建筑(建筑所有者收钱+收税)
        Server_SellToNPCStation_Implementation(Player, StationID, ItemID, Quantity, UnitPrice);
    }
    else
    {
        Server_BuyFromNPCStation_Implementation(Player, StationID, ItemID, Quantity, UnitPrice);
    }
}

bool APlayerTradeManager::Server_TradeAtPlayerStation_Validate(AController*, FName, FName, int32, float, bool)
{
    return true;
}

void APlayerTradeManager::Server_SetPlayerStationTaxRate_Implementation(
    AController* Owner, FName StationID, float NewTaxRate)
{
    if (!Owner) return;

    // 验证是所有者
    if (AStellarGameMode* GM = Cast<AStellarGameMode>(GetWorld()->GetAuthGameMode()))
    {
        FName OwnerID = GM->GetStationOwnerID(StationID);
        if (OwnerID != FName(*Owner->GetName()))
        {
            return; // 不是所有者, 拒绝
        }

        // 限制税率范围(0%~10%)
        NewTaxRate = FMath::Clamp(NewTaxRate, 0.f, 0.10f);

        // 更新该站点的税收配置
        FTradeTaxConfig Cfg = GetTaxConfig(StationID, ETradeLocationType::PlayerOwnedStation);
        Cfg.NPCSellerTaxRate = NewTaxRate;
        Cfg.NPCBuyerTaxRate = 0.f;
        PerStationTaxConfig.Add(StationID, Cfg);

        UE_LOG(LogPlayerTrade, Log, TEXT("[Trade] Station %s tax rate set to %.1f%% by %s"),
            *StationID.ToString(), NewTaxRate * 100.f, *Owner->GetName());
    }
}

bool APlayerTradeManager::Server_SetPlayerStationTaxRate_Validate(AController*, FName, float)
{
    return true;
}

// ==================== 查询 ====================

FTradeSession APlayerTradeManager::GetTradeSession(FName TradeID) const
{
    const FTradeSession* Found = ActiveTrades.Find(TradeID);
    return Found ? *Found : FTradeSession();
}

TArray<FTradeSession> APlayerTradeManager::GetActiveTradesForPlayer(AController* Player) const
{
    TArray<FTradeSession> Result;
    if (!Player) return Result;
    FString NetID = Player->GetName();

    for (const auto& Pair : ActiveTrades)
    {
        if (Pair.Value.TraderA == NetID || Pair.Value.TraderB == NetID)
        {
            Result.Add(Pair.Value);
        }
    }
    return Result;
}

TArray<FString> APlayerTradeManager::GetNearbyPlayersForTrade(AController* Player) const
{
    TArray<FString> Result;
    if (!Player || !Player->GetPawn()) return Result;

    FVector MyLoc = Player->GetPawn()->GetActorLocation();
    float RangeSq = MaxTradeDistance * MaxTradeDistance;

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        AController* Other = It->Get();
        if (!Other || Other == Player || !Other->GetPawn()) continue;

        float DistSq = FVector::DistSquared(MyLoc, Other->GetPawn()->GetActorLocation());
        if (DistSq <= RangeSq)
        {
            Result.Add(Other->GetName());
        }
    }
    return Result;
}

float APlayerTradeManager::CalculateEstimatedTax(float TransactionAmount, FName LocationID, FString PlayerNetID) const
{
    FTradeTaxConfig Cfg = GetTaxConfig(LocationID, DetectLocationType(LocationID, nullptr));
    float Tax = TransactionAmount * Cfg.NPCSellerTaxRate;
    float Multiplier = GetFactionTaxMultiplier(PlayerNetID, LocationID);
    return FMath::Clamp(Tax * Multiplier, Cfg.MinTaxAmount, Cfg.MaxTaxAmount);
}

float APlayerTradeManager::GetStationTaxRate(FName LocationID) const
{
    FTradeTaxConfig Cfg = GetTaxConfig(LocationID, DetectLocationType(LocationID, nullptr));
    return Cfg.NPCSellerTaxRate;
}

bool APlayerTradeManager::IsConferenceRoomTaxFree(FName ConferenceRoomID) const
{
    return DefaultTaxConfig.bConferenceRoomTaxFree;
}

// ==================== 过期检查 ====================

void APlayerTradeManager::TickExpireTrades(float CurrentTime)
{
    TArray<FName> ToExpire;
    for (auto& Pair : ActiveTrades)
    {
        if (CurrentTime >= Pair.Value.ExpiresAt)
        {
            ToExpire.Add(Pair.Key);
        }
    }

    for (FName ID : ToExpire)
    {
        FTradeSession& Session = ActiveTrades[ID];
        ETradeStatus OldStatus = Session.Status;
        Session.Status = ETradeStatus::Failed;
        OnTradeStatusChanged.Broadcast(ID, OldStatus, ETradeStatus::Failed, TEXT("超时"));
        ActiveTrades.Remove(ID);
    }
}

// ==================== 审计 ====================

void APlayerTradeManager::AuditTrade(const FTradeSession& Session)
{
    if (bLogAllTrades)
    {
        TaxAuditLog.Add(Session);
        UE_LOG(LogPlayerTrade, Log, TEXT("[Trade Audit] ID=%s A=%s B=%s Tax=%.0f LocType=%d"),
            *Session.TradeID.ToString(), *Session.TraderA, *Session.TraderB,
            Session.TotalTax, (int)Session.LocationType);
    }
}

// ==================== 辅助 ====================

FTradeOffer* APlayerTradeManager::GetOffer(FTradeSession& Session, FString PlayerNetID)
{
    if (Session.TraderA == PlayerNetID) return &Session.OfferA;
    if (Session.TraderB == PlayerNetID) return &Session.OfferB;
    return nullptr;
}

const FTradeOffer* APlayerTradeManager::GetOffer(const FTradeSession& Session, FString PlayerNetID) const
{
    if (Session.TraderA == PlayerNetID) return &Session.OfferA;
    if (Session.TraderB == PlayerNetID) return &Session.OfferB;
    return nullptr;
}

FName APlayerTradeManager::GenerateTradeID() const
{
    static int32 Counter = 0;
    Counter++;
    return FName(*FString::Printf(TEXT("TRD_%08d_%d"), FMath::RandRange(10000000, 99999999), Counter));
}

// ==================== 网络 ====================

void APlayerTradeManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(APlayerTradeManager, ActiveTrades);
}
