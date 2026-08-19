// ============================================================
// 路径: Source/StellarSystem/Private/Death/ShipInvalidationSystem.cpp
// 作用: 飞船索赔后失效 + 爆炸/残骸 N 秒后自动消失
// 修改于: v7.6 (原船货物处理/定时销毁/索赔后禁止登船/货物转移)
// ============================================================

#include "Death/ShipInvalidationSystem.h"
#include "Ship/ShipPawn.h"
#include "Ship/ShipCargoComponent.h"
#include "Ship/InsuranceSystem.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Math/UnrealMathUtility.h"

AShipInvalidationManager::AShipInvalidationManager()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
}

void AShipInvalidationManager::BeginPlay()
{
    Super::BeginPlay();
}

void AShipInvalidationManager::Tick(float Dt)
{
    Super::Tick(Dt);
    TickDespawnTimers(Dt);
}

// ========== 核心: 飞船被毁(关键修复: 货物处理+计时) ==========
void AShipInvalidationManager::OnShipDestroyed(AShipPawn* Ship, const FName& DestroyerID)
{
    if (!Ship || !GetAuthority()) return;

    const FName ShipID = *Ship->GetName();

    // 检查是否已经失效(防止重复)
    if (InvalidatedShips.Contains(ShipID)) return;

    FInvalidatedShipState State;
    State.ShipID = ShipID;
    State.Reason = EShipInvalidationReason::Destroyed;
    State.TimeOfDeath = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    State.MaxDespawnTime = DestroyedDespawnTime;
    State.DespawnTimer = DestroyedDespawnTime; // 爆炸残骸2分钟消失
    State.bIsWreck = true;
    State.bCanBeClaimed = true;

    // ★ 货物处理: 30%概率保留货物可搜刮
    if (UCargoComponent* Cargo = GetShipCargo(OldShip))
    {
        float CargoValue = Cargo->GetCargoValue();
        State.CargoValueAtDeath = CargoValue;

        if (FMath::FRand() < CargoPreserveChance && CargoValue > 0.f)
        {
            State.bCargoPreserved = true;
        }
        else
        {
            // 货物随船毁灭
            State.bCargoPreserved = false;
            Cargo->UnloadAll(); // 清空货舱
        }
    }

    State.LastKnownLocation = Ship->GetActorLocation();
    State.HullAtDeath = Ship->HullIntegrity;
    State.OriginalOwnerID = Ship->GetOwner() ? FName(*Ship->GetOwner()->GetName()) : NAME_None;
    State.bNotificationSent = false;

    InvalidatedShips.Add(ShipID, State);

    // 加入销毁队列
    FDespawnEntry Entry;
    Entry.ShipID = ShipID;
    Entry.TimeRemaining = State.DespawnTimer;
    Entry.bIsActorValid = IsValid(Ship);
    DespawnQueue.Add(Entry);

    // 禁止再登船
    Ship->FlightMode = EShipFlightMode::Dead;

    // 播放坍塌/冒烟
    if (bCollapseWreckMesh) PlayWreckEffects(Ship);

    // 通知机主
    if (bNotifyOwnerOnInvalid && !State.bNotificationSent)
    {
        NotifyOwner(ShipID, TEXT("你的飞船已被摧毁,残骸漂浮在太空中..."));
        State.bNotificationSent = true;
    }

    OnShipInvalidatedEvent.Broadcast(ShipID);
}

// ========== 核心: 飞船被索赔(关键修复: 立即失效+货物转移+快速消失) ==========
void AShipInvalidationManager::OnShipClaimed(AShipPawn* OldShip, AShipPawn* NewShip, FName NewShipID)
{
    if (!OldShip || !GetAuthority()) return;

    const FName OldID = *OldShip->GetName();

    // 防止重复处理
    if (InvalidatedShips.Contains(OldID))
    {
        // 已存在(如先被毁再索赔), 更新为索赔状态
        FInvalidatedShipState& Existing = InvalidatedShips[OldID];
        Existing.Reason = EShipInvalidationReason::Claimed;
        Existing.DespawnTimer = ClaimedDespawnTime; // 30秒快速消失
        Existing.bIsWreck = false;
        Existing.bCanBeClaimed = false;
        return;
    }

    FInvalidatedShipState State;
    State.ShipID = OldID;
    State.Reason = EShipInvalidationReason::Claimed;
    State.TimeOfDeath = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    State.MaxDespawnTime = ClaimedDespawnTime;
    State.DespawnTimer = ClaimedDespawnTime; // 30秒
    State.bIsWreck = false;
    State.bCanBeClaimed = false; // 已索赔,不可再索赔
    State.LastKnownLocation = OldShip->GetActorLocation();
    State.OriginalOwnerID = OldShip->GetOwner() ? FName(*OldShip->GetOwner()->GetName()) : NAME_None;
    State.bNotificationSent = false;

    // ★ 货物转移: 从旧船转移到新船
    if (bTransferCargoOnClaim && NewShip)
    {
        TransferCargo(OldShip, NewShip);
        State.bCargoPreserved = true;
    }
    else
    {
        // 不转移则清空
        if (UCargoComponent* OldCargo = GetShipCargo(OldShip))
        {
            State.CargoValueAtDeath = OldCargo->GetCargoValue();
            OldCargo->UnloadAll();
        }
        State.bCargoPreserved = false;
    }

    InvalidatedShips.Add(OldID, State);

    // 加入销毁队列
    FDespawnEntry Entry;
    Entry.ShipID = OldID;
    Entry.TimeRemaining = ClaimedDespawnTime;
    Entry.bIsActorValid = IsValid(OldShip);
    DespawnQueue.Add(Entry);

    // ★ 立即失效: 禁用移动/武器/交互/登船
    OldShip->FlightMode = EShipFlightMode::Dead;
    if (OldShip->Movement) OldShip->Movement->SetActive(false);
    OldShip->SetActorEnableCollision(false); // 禁止碰撞/登船

    // 通知
    if (bNotifyOwnerOnInvalid && !State.bNotificationSent)
    {
        NotifyOwner(OldID, TEXT("飞船已成功索赔,旧船即将回收。"));
        State.bNotificationSent = true;
    }

    OnShipInvalidatedEvent.Broadcast(OldID);

    // 启动淡出
    if (bFadeOutBeforeDespawn)
    {
        FadeOutActor(OldShip, FadeOutDuration);
    }
}

void AShipInvalidationManager::OnShipAbandoned(AShipPawn* Ship)
{
    if (!Ship || !GetAuthority()) return;
    FName ID = *Ship->GetName();

    if (InvalidatedShips.Contains(ID)) return;

    FInvalidatedShipState State;
    State.ShipID = ID;
    State.Reason = EShipInvalidationReason::Abandoned;
    State.MaxDespawnTime = DefaultDespawnTime;
    State.DespawnTimer = DefaultDespawnTime;
    State.LastKnownLocation = Ship->GetActorLocation();
    State.bCanBeClaimed = false;

    // 清空货物
    if (UCargoComponent* Cargo = GetShipCargo(OldShip))
    {
        Cargo->UnloadAll();
    }

    InvalidatedShips.Add(ID, State);
    OldShip->FlightMode = EShipFlightMode::Dead;

    FDespawnEntry Entry;
    Entry.ShipID = ID;
    Entry.TimeRemaining = DefaultDespawnTime;
    Entry.bIsActorValid = IsValid(OldShip);
    DespawnQueue.Add(Entry);

    OnShipInvalidatedEvent.Broadcast(ID);
}

void AShipInvalidationManager::ImpoundShip(AShipPawn* Ship, FString Reason)
{
    if (!Ship || !GetAuthority()) return;
    FName ID = *Ship->GetName();

    FInvalidatedShipState State;
    State.ShipID = ID;
    State.Reason = EShipInvalidationReason::Impounded;
    State.DespawnTimer = 0.f; // 无限期扣押
    State.MaxDespawnTime = 0.f;
    State.LastKnownLocation = Ship->GetActorLocation();
    State.bCanBeClaimed = false;

    InvalidatedShips.Add(ID, State);
    Ship->FlightMode = EShipFlightMode::Dead;
    Ship->SetActorEnableCollision(false);

    NotifyOwner(ID, FString::Printf(TEXT("飞船被扣押: %s"), *Reason));
    OnShipInvalidatedEvent.Broadcast(ID);
}

// ========== 查询 ==========
bool AShipInvalidationManager::IsShipValid(AShipPawn* Ship) const
{
    if (!Ship) return false;
    if (Ship->FlightMode == EShipFlightMode::Dead) return false;
    const FName ID = *Ship->GetName();
    return !InvalidatedShips.Contains(ID);
}

bool AShipInvalidationManager::CanPlayerBoard(AShipPawn* Ship, const FName& PlayerID) const
{
    if (!IsShipValid(Ship)) return false;

    // 检查是否是机主或有机库权限
    if (Ship->GetOwner() && Ship->GetOwner()->GetName() == PlayerID.ToString())
    {
        return true;
    }

    // 检查共享权限(派系/军团)
    return false; // 简化: 只有机主可登
}

FInvalidatedShipState AShipInvalidationManager::GetShipState(FName ShipID) const
{
    const FInvalidatedShipState* Found = InvalidatedShips.Find(ShipID);
    return Found ? *Found : FInvalidatedShipState();
}

TArray<FInvalidatedShipState> AShipInvalidationManager::GetAllWrecks() const
{
    TArray<FInvalidatedShipState> Out;
    for (const auto& Pair : InvalidatedShips)
        if (Pair.Value.bIsWreck) Out.Add(Pair.Value);
    return Out;
}

float AShipInvalidationManager::GetRemainingDespawnTime(FName ShipID) const
{
    // 优先从队列查(更精确)
    for (const FDespawnEntry& Entry : DespawnQueue)
    {
        if (Entry.ShipID == ShipID) return FMath::Max(0.f, Entry.TimeRemaining);
    }

    const FInvalidatedShipState* S = InvalidatedShips.Find(ShipID);
    return S ? S->DespawnTimer : 0.f;
}

// ========== 搜刮 ==========
void AShipInvalidationManager::Server_LootWreck_Implementation(
    AController* Player, FName WreckShipID)
{
    if (!Player || !GetAuthority()) return;

    const FInvalidatedShipState* S = InvalidatedShips.Find(WreckShipID);
    if (!S || !S->bIsWreck || !S->bCargoPreserved) return;

    // 搜刮逻辑: 给玩家随机资源
    // 实际应从原船货舱读取并转移
    OnWreckLootedEvent.Broadcast(WreckShipID, Player ? Player->GetPawn() : nullptr);
}

bool AShipInvalidationManager::Server_LootWreck_Validate(AController*, FName)
{
    return true;
}

// ========== 重生 ==========
void AShipInvalidationManager::Server_SpawnReplacementShip_Implementation(
    FName OwnerID, FName ShipClassID, FVector SpawnLocation, const FShipSavedConfig& Config)
{
    if (!GetAuthority()) return;

    // 通知 GameMode 生成新船(带配置)
    if (AStellarGameMode* GM = Cast<AStellarGameMode>(GetWorld()->GetAuthGameMode()))
    {
        GM->SpawnShipWithConfig(OwnerID, ShipClassID, SpawnLocation, Config);
    }
}

bool AShipInvalidationManager::Server_SpawnReplacementShip_Validate(FName, FName, FVector, const FShipSavedConfig&)
{
    return true;
}

// ========== 货物转移(核心) ==========
void AShipInvalidationManager::Server_TransferCargoToNewShip_Implementation(
    AController* Player, FName OldShipID, FName NewShipID)
{
    if (!Player || !GetAuthority()) return;

    // 查找旧船和新船
    AShipPawn* OldShip = nullptr;
    AShipPawn* NewShip = nullptr;

    // 通过 GameMode 的 Ship 注册表查找
    if (AStellarGameMode* GM = Cast<AStellarGameMode>(GetWorld()->GetAuthGameMode()))
    {
        OldShip = GM->FindShipByID(OldShipID);
        NewShip = GM->FindShipByID(NewShipID);
    }

    if (OldShip && NewShip)
    {
        TransferCargo(OldShip, NewShip);
    }
}

bool AShipInvalidationManager::Server_TransferCargoToNewShip_Validate(AController*, FName, FName)
{
    return true;
}

void AShipInvalidationManager::TransferCargo(AShipPawn* FromShip, AShipPawn* ToShip)
{
    if (!FromShip || !ToShip) return;

    UCargoComponent* FromCargo = GetShipCargo(FromShip);
    UCargoComponent* ToCargo = GetShipCargo(ToShip);

    if (!FromCargo || !ToCargo) return;

    // 转移所有货物
    float TransferredValue = 0.f;
    TArray<FCargoEntry> AllCargo = FromCargo->GetAllCargo();
    int32 TransferredCount = 0;

    for (const FCargoEntry& Entry : AllCargo)
    {
        if (ToCargo->CanFit(Entry.ItemID, Entry.Quantity, Entry.UnitWeight, Entry.UnitVolume))
        {
            if (ToCargo->LoadCargo(Entry.ItemID, Entry.Quantity, Entry.UnitWeight, Entry.UnitVolume,
                                    Entry.QuestBinding, Entry.bIsPerishable, Entry.PerishTimer))
            {
                TransferredCount++;
                TransferredValue += Entry.Quantity * Entry.UnitWeight;
            }
        }
    }

    // 清空旧船货舱
    FromCargo->UnloadAll();

    OnCargoTransferredEvent.Broadcast(
        *FromShip->GetName(),
        *ToShip->GetName(),
        TransferredValue
    );

    UE_LOG(LogTemp, Log, TEXT("[Invalidation] Cargo transferred: %d items, %.0f kg from %s to %s"),
        TransferredCount, TransferredValue, *FromShip->GetName(), *ToShip->GetName());
}

// ========== 计时器(精确) ==========
void AShipInvalidationManager::TickDespawnTimers(float Dt)
{
    if (!GetAuthority()) return;

    for (int32 i = DespawnQueue.Num() - 1; i >= 0; --i)
    {
        FDespawnEntry& Entry = DespawnQueue[i];

        if (Entry.TimeRemaining <= 0.f) continue; // 无限期(扣押)

        Entry.TimeRemaining -= Dt;

        // 同步到 InvalidatedShips
        if (FInvalidatedShipState* S = InvalidatedShips.Find(Entry.ShipID))
        {
            S->DespawnTimer = Entry.TimeRemaining;
        }

        if (Entry.TimeRemaining <= 0.f)
        {
            DespawnShip(Entry.ShipID);
            DespawnQueue.RemoveAt(i);
        }
    }
}

void AShipInvalidationManager::DespawnShip(FName ShipID)
{
    // 找到对应 Actor 并销毁
    for (TActorIterator<AShipPawn> It(GetWorld()); It; ++It)
    {
        if (*It && *It->GetName() == ShipID.ToString())
        {
            if (bFadeOutBeforeDespawn)
            {
                FadeOutActor(*It, FadeOutDuration);
            }
            else if (IsValid(*It))
            {
                It->Destroy();
            }
            break;
        }
    }

    InvalidatedShips.Remove(ShipID);
    OnShipDespawnedEvent.Broadcast(ShipID);

    if (bNotifyOwnerOnDespawn)
        NotifyOwner(ShipID, TEXT("飞船残骸已消失在深空中。"));
}

void AShipInvalidationManager::PlayWreckEffects(AShipPawn* Ship)
{
    if (!Ship) return;

    if (Ship->ShipMesh)
    {
        Ship->ShipMesh->SetSimulatePhysics(true);
        Ship->ShipMesh->SetLinearDamping(0.5f);
        Ship->ShipMesh->AddImpulse(FVector(0, 0, -100.f));
    }

    // 禁用所有组件
    if (Ship->WeaponsComp) Ship->WeaponsComp->Deactivate();
    if (Ship->LoadoutComp) Ship->LoadoutComp->Deactivate();
}

void AShipInvalidationManager::FadeOutActor(AActor* Actor, float Duration)
{
    if (!Actor || !IsValid(Actor)) return;

    // 设置渲染透明度(需要材质支持)
    TArray<UPrimitiveComponent*> PrimComps;
    Actor->GetComponents<UPrimitiveComponent>(PrimComps);
    for (UPrimitiveComponent* Prim : PrimComps)
    {
        Prim->SetRenderInMainPass(true);
        // 实际应修改材质透明度, 这里简化为定时销毁
    }

    FTimerHandle Timer;
    GetWorld()->GetTimerManager().SetTimer(Timer, [Actor]()
    {
        if (Actor && IsValid(Actor) && !Actor->IsPendingKillPending())
        {
            Actor->Destroy();
        }
    }, Duration, false);
}

void AShipInvalidationManager::NotifyOwner(FName ShipID, FString Message)
{
    UE_LOG(LogTemp, Log, TEXT("[ShipInvalidation] Ship=%s | %s"), *ShipID.ToString(), *Message);
}

UCargoComponent* AShipInvalidationManager::GetShipCargo(AShipPawn* Ship) const
{
    if (!Ship) return nullptr;
    return Ship->FindComponentByClass<UCargoComponent>();
}

// ========== 网络 ==========
void AShipInvalidationManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(AShipInvalidationManager, InvalidatedShips);
}
