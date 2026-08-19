// ============================================================
// 路径: Source/StellarSystem/Private/Trade/PlayerProximityGive.cpp
// 作用: 玩家↔玩家 近距离给付系统实现
// 新增于: v7.5
// ============================================================

#include "Trade/PlayerProximityGive.h"
#include "Character/MyCharacter.h"
#include "Character/InventoryComponent.h"
#include "Character/CurrencyComponent.h"
#include "Ship/ShipPawn.h"
#include "Cargo/ShipCargoComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogProximityGive, Log, All);

APlayerProximityGiveManager::APlayerProximityGiveManager()
{
    PrimaryActorTick.bCanEverTick = true;
    MaxGiveDistanceSq = MaxGiveDistance * MaxGiveDistance;
}

void APlayerProximityGiveManager::BeginPlay()
{
    Super::BeginPlay();
    MaxGiveDistanceSq = MaxGiveDistance * MaxGiveDistance;
}

void APlayerProximityGiveManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    float Now = GetWorld()->GetTimeSeconds();
    TickExpireRequests(Now);
}

// ==================== 距离校验 ====================

bool APlayerProximityGiveManager::ValidateDistance(AController* A, AController* B, float MaxDist) const
{
    if (!A || !B) return false;
    APawn* PawnA = A->GetPawn();
    APawn* PawnB = B->GetPawn();
    if (!PawnA || !PawnB) return false;

    float DistSq = FVector::DistSquared(PawnA->GetActorLocation(), PawnB->GetActorLocation());
    float MaxDistSq = MaxDist * MaxDist;
    return DistSq <= MaxDistSq;
}

bool APlayerProximityGiveManager::ArePlayersInGiveRange(AController* PlayerA, AController* PlayerB) const
{
    return ValidateDistance(PlayerA, PlayerB, MaxGiveDistance);
}

bool APlayerProximityGiveManager::HasLineOfSight(AController* A, AController* B) const
{
    if (!bRequireLineOfSight) return true;
    if (!A || !B) return false;

    APawn* PawnA = A->GetPawn();
    APawn* PawnB = B->GetPawn();
    if (!PawnA || !PawnB) return false;

    FVector Start = PawnA->GetActorLocation() + FVector(0, 0, 80.f); // 胸部高度
    FVector End = PawnB->GetActorLocation() + FVector(0, 0, 80.f);
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(PawnA);
    Params.AddIgnoredActor(PawnB);

    bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
    return !bHit; // 没打到东西 = 有视线
}

// ==================== 频率限制 ====================

bool APlayerProximityGiveManager::CheckRateLimit(FString NetID, float CurrentTime)
{
    TArray<float>& Timestamps = PlayerGiveTimestamps.FindOrAdd(NetID);
    // 移除1分钟前的记录
    Timestamps.RemoveAll([&](float T) { return CurrentTime - T > 60.f; });
    return Timestamps.Num() < MaxGivesPerMinute;
}

// ==================== 发起给付 ====================

void APlayerProximityGiveManager::Server_InitiateGive_Implementation(
    AController* Sender, AController* Receiver,
    const TArray<FGiveItemEntry>& Items, float CreditAmount)
{
    if (!Sender || !Receiver || Sender == Receiver)
    {
        LogTransaction(FGiveRequest(), false, TEXT("无效请求: 空指针或自己给自己"));
        return;
    }

    // 必须是 PlayerController
    if (!Sender->IsPlayerController() || !Receiver->IsPlayerController())
    {
        LogTransaction(FGiveRequest(), false, TEXT("非玩家控制器"));
        return;
    }

    FString SenderNet = Sender->GetNetConnection() ? Sender->GetNetConnection()->PlayerId.ToString() : Sender->GetName();
    FString ReceiverNet = Receiver->GetNetConnection() ? Receiver->GetNetConnection()->PlayerId.ToString() : Receiver->GetName();

    float Now = GetWorld()->GetTimeSeconds();

    // 频率检查
    if (!CheckRateLimit(SenderNet, Now))
    {
        LogTransaction(FGiveRequest(), false, TEXT("频率超限"));
        return;
    }

    // 距离校验
    if (!ValidateDistance(Sender, Receiver, MaxGiveDistance))
    {
        LogTransaction(FGiveRequest(), false, TEXT("距离过远"));
        return;
    }

    // 视线校验
    if (!HasLineOfSight(Sender, Receiver))
    {
        LogTransaction(FGiveRequest(), false, TEXT("无视线"));
        return;
    }

    // 物品数量限制
    if (Items.Num() > MaxItemsPerGive)
    {
        LogTransaction(FGiveRequest(), false, TEXT("物品数量超限"));
        return;
    }

    // 货币限制
    if (CreditAmount > MaxCreditPerGive)
    {
        LogTransaction(FGiveRequest(), false, TEXT("货币超限"));
        return;
    }

    // 检查给付方是否有这些物品
    AMyCharacter* SenderChar = Cast<AMyCharacter>(Sender->GetPawn());
    if (!SenderChar) return;

    UInventoryComponent* Inv = SenderChar->FindComponentByClass<UInventoryComponent>();
    if (!Inv)
    {
        LogTransaction(FGiveRequest(), false, TEXT("给付方无背包"));
        return;
    }

    // 验证物品数量
    for (const FGiveItemEntry& Entry : Items)
    {
        if (Inv->GetItemCount(Entry.ItemID) < Entry.Quantity)
        {
            LogTransaction(FGiveRequest(), false, FString::Printf(TEXT("物品不足: %s"), *Entry.ItemID.ToString()));
            return;
        }
    }

    // 验证货币
    if (CreditAmount > 0.f)
    {
        UCurrencyComponent* Cur = SenderChar->FindComponentByClass<UCurrencyComponent>();
        if (!Cur || Cur->GetCredits() < CreditAmount)
        {
            LogTransaction(FGiveRequest(), false, TEXT("货币不足"));
            return;
        }
    }

    // 创建请求
    FGiveRequest Request;
    Request.RequestID = GenerateRequestID();
    Request.FromPlayerNetID = SenderNet;
    Request.ToPlayerNetID = ReceiverNet;
    Request.Items = Items;
    Request.CreditAmount = CreditAmount;
    Request.Status = EGiveRequestStatus::Pending;
    Request.CreatedAt = Now;
    Request.ExpiresAt = Now + RequestTimeout;
    Request.bSenderConfirmed = true; // 发起方自动确认
    Request.bReceiverConfirmed = false;

    ActiveRequests.Add(Request.RequestID, Request);

    // 记录频率
    PlayerGiveTimestamps.FindOrAdd(SenderNet).Add(Now);

    // 通知接收方
    OnGiveRequestReceived.Broadcast(Request.RequestID, SenderNet, Items.Num());

    UE_LOG(LogProximityGive, Log, TEXT("[Give] 请求 %s: %s → %s, %d 件, %.0f 信用点"),
           *Request.RequestID.ToString(), *SenderNet, *ReceiverNet, Items.Num(), CreditAmount);
}

bool APlayerProximityGiveManager::Server_InitiateGive_Validate(
    AController* Sender, AController* Receiver,
    const TArray<FGiveItemEntry>& Items, float CreditAmount)
{
    return Sender && Receiver && CreditAmount >= 0.f;
}

// ==================== 接受给付 ====================

void APlayerProximityGiveManager::Server_AcceptGive_Implementation(
    AController* Receiver, FName RequestID)
{
    if (!Receiver) return;

    FGiveRequest* Request = ActiveRequests.Find(RequestID);
    if (!Request)
    {
        LogTransaction(FGiveRequest(), false, TEXT("请求不存在"));
        return;
    }

    if (Request->Status != EGiveRequestStatus::Pending)
    {
        LogTransaction(*Request, false, TEXT("请求状态非Pending"));
        return;
    }

    // 验证接收方身份
    FString ReceiverNet = Receiver->GetNetConnection() ? Receiver->GetNetConnection()->PlayerId.ToString() : Receiver->GetName();
    if (Request->ToPlayerNetID != ReceiverNet)
    {
        LogTransaction(*Request, false, TEXT("接收方身份不匹配"));
        return;
    }

    // 再次校验距离(防止移动后)
    AController* SenderController = nullptr;
    // 通过 World 查找 Sender
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (PC && PC != Receiver)
        {
            FString NetID = PC->GetNetConnection() ? PC->GetNetConnection()->PlayerId.ToString() : PC->GetName();
            if (NetID == Request->FromPlayerNetID)
            {
                SenderController = PC;
                break;
            }
        }
    }

    if (!SenderController)
    {
        LogTransaction(*Request, false, TEXT("找不到给付方"));
        return;
    }

    // 距离校验
    if (!ValidateDistance(SenderController, Receiver, MaxGiveDistance))
    {
        LogTransaction(*Request, false, TEXT("接受时距离过远"));
        Request->Status = EGiveRequestStatus::Cancelled;
        OnGiveRequestUpdated.Broadcast(RequestID, EGiveRequestStatus::Cancelled, TEXT("距离过远"), false);
        return;
    }

    // 标记双方确认
    Request->bReceiverConfirmed = true;
    Request->Status = EGiveRequestStatus::Accepted;

    // 执行转移
    ExecuteTransfer(*Request);

    // 完成
    Request->Status = EGiveRequestStatus::Completed;
    OnGiveRequestUpdated.Broadcast(RequestID, EGiveRequestStatus::Completed, TEXT("成功"), true);
    OnGiveItemsTransferred.Broadcast(RequestID, Request->ToPlayerNetID, Request->Items.Num());

    // 清理
    ActiveRequests.Remove(RequestID);

    UE_LOG(LogProximityGive, Log, TEXT("[Give] 完成 %s"), *RequestID.ToString());
}

bool APlayerProximityGiveManager::Server_AcceptGive_Validate(
    AController* Receiver, FName RequestID)
{
    return Receiver != nullptr;
}

// ==================== 拒绝给付 ====================

void APlayerProximityGiveManager::Server_RejectGive_Implementation(
    AController* Receiver, FName RequestID, FString Reason)
{
    if (!Receiver) return;

    FGiveRequest* Request = ActiveRequests.Find(RequestID);
    if (!Request) return;

    Request->Status = EGiveRequestStatus::Rejected;
    OnGiveRequestUpdated.Broadcast(RequestID, EGiveRequestStatus::Rejected, Reason, false);

    ActiveRequests.Remove(RequestID);

    UE_LOG(LogProximityGive, Log, TEXT("[Give] 拒绝 %s: %s"), *RequestID.ToString(), *Reason);
}

bool APlayerProximityGiveManager::Server_RejectGive_Validate(
    AController* Receiver, FName RequestID, FString Reason)
{
    return Receiver != nullptr;
}

// ==================== 取消给付 ====================

void APlayerProximityGiveManager::Server_CancelGive_Implementation(
    AController* Sender, FName RequestID)
{
    if (!Sender) return;

    FGiveRequest* Request = ActiveRequests.Find(RequestID);
    if (!Request) return;

    FString SenderNet = Sender->GetNetConnection() ? Sender->GetNetConnection()->PlayerId.ToString() : Sender->GetName();
    if (Request->FromPlayerNetID != SenderNet)
    {
        LogTransaction(*Request, false, TEXT("取消方身份不匹配"));
        return;
    }

    Request->Status = EGiveRequestStatus::Cancelled;
    OnGiveRequestUpdated.Broadcast(RequestID, EGiveRequestStatus::Cancelled, TEXT("给付方取消"), false);

    ActiveRequests.Remove(RequestID);
}

bool APlayerProximityGiveManager::Server_CancelGive_Validate(
    AController* Sender, FName RequestID)
{
    return Sender != nullptr;
}

// ==================== 执行物品转移 ====================

void APlayerProximityGiveManager::ExecuteTransfer(FGiveRequest& Request)
{
    UWorld* World = GetWorld();
    if (!World) return;

    // 查找 Sender 和 Receiver 的 Pawn
    AMyCharacter* SenderChar = nullptr;
    AMyCharacter* ReceiverChar = nullptr;

    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (!PC) continue;
        FString NetID = PC->GetNetConnection() ? PC->GetNetConnection()->PlayerId.ToString() : PC->GetName();
        AMyCharacter* Char = Cast<AMyCharacter>(PC->GetPawn());
        if (!Char) continue;

        if (NetID == Request.FromPlayerNetID) SenderChar = Char;
        else if (NetID == Request.ToPlayerNetID) ReceiverChar = Char;
    }

    if (!SenderChar || !ReceiverChar)
    {
        LogTransaction(Request, false, TEXT("找不到角色"));
        return;
    }

    UInventoryComponent* SenderInv = SenderChar->FindComponentByClass<UInventoryComponent>();
    UInventoryComponent* ReceiverInv = ReceiverChar->FindComponentByClass<UInventoryComponent>();
    UCurrencyComponent* SenderCur = SenderChar->FindComponentByClass<UCurrencyComponent>();
    UCurrencyComponent* ReceiverCur = ReceiverChar->FindComponentByClass<UCurrencyComponent>();

    if (!SenderInv || !ReceiverInv)
    {
        LogTransaction(Request, false, TEXT("背包组件缺失"));
        return;
    }

    // 转移物品
    for (const FGiveItemEntry& Entry : Request.Items)
    {
        // 从给付方移除
        SenderInv->ServerRemoveItem(Entry.ItemID, Entry.Quantity);
        // 给接收方添加
        ReceiverInv->ServerAddItem(Entry.ItemID, Entry.Quantity);
    }

    // 转移货币
    if (Request.CreditAmount > 0.f && SenderCur && ReceiverCur)
    {
        SenderCur->ServerSpendCredits(Request.CreditAmount, TEXT("PlayerGive"));
        ReceiverCur->ServerAddCredits(Request.CreditAmount, TEXT("PlayerGive"));
    }

    LogTransaction(Request, true, TEXT("转移成功"));
}

// ==================== 过期检查 ====================

void APlayerProximityGiveManager::TickExpireRequests(float CurrentTime)
{
    TArray<FName> ToExpire;
    for (auto& Pair : ActiveRequests)
    {
        if (Pair.Value.Status == EGiveRequestStatus::Pending &&
            CurrentTime >= Pair.Value.ExpiresAt)
        {
            ToExpire.Add(Pair.Key);
        }
    }

    for (FName ID : ToExpire)
    {
        FGiveRequest& Req = ActiveRequests[ID];
        Req.Status = EGiveRequestStatus::Expired;
        OnGiveRequestExpired.Broadcast(ID);
        OnGiveRequestUpdated.Broadcast(ID, EGiveRequestStatus::Expired, TEXT("超时"), false);
        ActiveRequests.Remove(ID);
        UE_LOG(LogProximityGive, Log, TEXT("[Give] 过期 %s"), *ID.ToString());
    }
}

// ==================== 查询 ====================

TArray<FString> APlayerProximityGiveManager::GetNearbyPlayersForGive(AController* Player) const
{
    TArray<FString> Result;
    if (!Player) return Result;

    APawn* MyPawn = Player->GetPawn();
    if (!MyPawn) return Result;

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* Other = It->Get();
        if (!Other || Other == Player) continue;

        APawn* OtherPawn = Other->GetPawn();
        if (!OtherPawn) continue;

        float DistSq = FVector::DistSquared(MyPawn->GetActorLocation(), OtherPawn->GetActorLocation());
        if (DistSq <= MaxGiveDistanceSq)
        {
            FString NetID = Other->GetNetConnection() ? Other->GetNetConnection()->PlayerId.ToString() : Other->GetName();
            Result.Add(NetID);
        }
    }

    return Result;
}

TArray<FGiveRequest> APlayerProximityGiveManager::GetPendingRequestsForPlayer(AController* Player) const
{
    TArray<FGiveRequest> Result;
    if (!Player) return Result;

    FString NetID = Player->GetNetConnection() ? Player->GetNetConnection()->PlayerId.ToString() : Player->GetName();

    for (const auto& Pair : ActiveRequests)
    {
        if (Pair.Value.Status != EGiveRequestStatus::Pending) continue;
        if (Pair.Value.FromPlayerNetID == NetID || Pair.Value.ToPlayerNetID == NetID)
        {
            Result.Add(Pair.Value);
        }
    }

    return Result;
}

FGiveRequest APlayerProximityGiveManager::GetGiveRequest(FName RequestID) const
{
    if (const FGiveRequest* Req = ActiveRequests.Find(RequestID))
    {
        return *Req;
    }
    return FGiveRequest();
}

// ==================== 飞船↔飞船 给付 ====================

void APlayerProximityGiveManager::Server_TransferCargoBetweenShips_Implementation(
    AController* Sender, AController* Receiver,
    const TArray<FName>& ItemIDs, const TArray<int32>& Quantities)
{
    if (!Sender || !Receiver) return;
    if (ItemIDs.Num() != Quantities.Num()) return;

    // 距离校验(飞船用更大距离)
    if (!ValidateDistance(Sender, Receiver, ShipGiveDistance))
    {
        LogTransaction(FGiveRequest(), false, TEXT("飞船距离过远"));
        return;
    }

    APawn* SenderPawn = Sender->GetPawn();
    APawn* ReceiverPawn = Receiver->GetPawn();
    if (!SenderPawn || !ReceiverPawn) return;

    UShipCargoComponent* SenderCargo = SenderPawn->FindComponentByClass<UShipCargoComponent>();
    UShipCargoComponent* ReceiverCargo = ReceiverPawn->FindComponentByClass<UShipCargoComponent>();

    if (!SenderCargo || !ReceiverCargo)
    {
        LogTransaction(FGiveRequest(), false, TEXT("飞船货舱缺失"));
        return;
    }

    // 逐件转移
    for (int32 i = 0; i < ItemIDs.Num(); i++)
    {
        FName ItemID = ItemIDs[i];
        int32 Qty = Quantities[i];

        // 检查发送方有足够货物
        if (SenderCargo->GetItemQuantity(ItemID) < Qty) continue;

        // 检查接收方有空间
        // (简化: 假设重量体积足够, 实际应检查)
        // 从发送方移除
        SenderCargo->UnloadCargo(ItemID, Qty);
        // 给接收方添加(简化: 用默认重量体积)
        ReceiverCargo->LoadCargo(ItemID, Qty, 1.f, 1.f);
    }

    UE_LOG(LogProximityGive, Log, TEXT("[ShipGive] %s → %s: %d 种货物"),
           *Sender->GetName(), *Receiver->GetName(), ItemIDs.Num());
}

bool APlayerProximityGiveManager::Server_TransferCargoBetweenShips_Validate(
    AController* Sender, AController* Receiver,
    const TArray<FName>& ItemIDs, const TArray<int32>& Quantities)
{
    return Sender && Receiver && ItemIDs.Num() == Quantities.Num();
}

void APlayerProximityGiveManager::Server_TransferCreditsBetweenShips_Implementation(
    AController* Sender, AController* Receiver, float Amount)
{
    if (!Sender || !Receiver || Amount <= 0.f) return;

    if (!ValidateDistance(Sender, Receiver, ShipGiveDistance))
    {
        LogTransaction(FGiveRequest(), false, TEXT("飞船货币转移距离过远"));
        return;
    }

    AMyCharacter* SenderChar = Cast<AMyCharacter>(Sender->GetPawn());
    AMyCharacter* ReceiverChar = Cast<AMyCharacter>(Receiver->GetPawn());
    if (!SenderChar || !ReceiverChar) return;

    UCurrencyComponent* SenderCur = SenderChar->FindComponentByClass<UCurrencyComponent>();
    UCurrencyComponent* ReceiverCur = ReceiverChar->FindComponentByClass<UCurrencyComponent>();
    if (!SenderCur || !ReceiverCur) return;

    if (SenderCur->GetCredits() < Amount) return;

    SenderCur->ServerSpendCredits(Amount, TEXT("ShipGive"));
    ReceiverCur->ServerAddCredits(Amount, TEXT("ShipGive"));

    UE_LOG(LogProximityGive, Log, TEXT("[ShipGive] 货币 %.0f %s → %s"),
           Amount, *Sender->GetName(), *Receiver->GetName());
}

bool APlayerProximityGiveManager::Server_TransferCreditsBetweenShips_Validate(
    AController* Sender, AController* Receiver, float Amount)
{
    return Sender && Receiver && Amount > 0.f && Amount < 10000000.f;
}

// ==================== 日志 ====================

void APlayerProximityGiveManager::LogTransaction(const FGiveRequest& Request, bool bSuccess, FString Note)
{
    if (!bLogAllTransactions) return;

    FString Status = bSuccess ? TEXT("SUCCESS") : TEXT("FAIL");
    UE_LOG(LogProximityGive, Log, TEXT("[Give][%s] %s | From=%s To=%s Items=%d Credits=%.0f | %s"),
           *Status, *Request.RequestID.ToString(),
           *Request.FromPlayerNetID, *Request.ToPlayerNetID,
           Request.Items.Num(), Request.CreditAmount, *Note);
}

// ==================== 工具 ====================

FName APlayerProximityGiveManager::GenerateRequestID() const
{
    static int32 Counter = 0;
    Counter++;
    return FName(*FString::Printf(TEXT("Give_%d_%d"), FMath::Rand(), Counter));
}
