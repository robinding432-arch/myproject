// ============================================================
// 路径: Source/StellarSystem/Private/Death/PlayerDeathSystem.cpp
// 作用: 玩家死亡 → 尸体立即消失 + 装备/背包 100% 转移 → 医院复活
// 修改于: v7.6 (立即消失/100%物品保留/无敌期/自动复活)
// ============================================================

#include "Death/PlayerDeathSystem.h"
#include "Character/MyCharacter.h"
#include "Character/InventoryComponent.h"
#include "Character/CurrencyComponent.h"
#include "Character/VitalsComponent.h"
#include "Respawn/RespawnManager.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Math/UnrealMathUtility.h"

APlayerDeathManager::APlayerDeathManager()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
}

void APlayerDeathManager::BeginPlay()
{
    Super::BeginPlay();
}

void APlayerDeathManager::Tick(float Dt)
{
    Super::Tick(Dt);
    TickRespawnTimers(Dt);
}

// ========== 核心: 玩家死亡 ==========
void APlayerDeathManager::OnPlayerDied(AMyCharacter* DeadCharacter, EDeathCause Cause,
                                      const FName& KillerID, const FString& KillerName)
{
    if (!DeadCharacter || !GetAuthority()) return;

    FString PlayerID = DeadCharacter->GetName();

    // 防止重复处理
    if (PendingDeaths.Contains(PlayerID)) return;

    // ★ 关键: 捕获完整快照(100% 物品)
    FPlayerDeathSnapshot Snap = CaptureDeathSnapshot(DeadCharacter, Cause);
    Snap.KillerID = KillerID;
    Snap.KillerName = KillerName;
    Snap.DeathLocation = DeadCharacter->GetActorLocation();
    Snap.DeathTime = FDateTime::Now();

    // 记录死亡时所在的飞船
    if (AShipPawn* Ship = Cast<AShipPawn>(DeadCharacter->GetPawnOrParentActor()))
    {
        Snap.ShipAtDeath = FName(*Ship->GetName());
        Snap.bShipDestroyed = (Ship->FlightMode == EShipFlightMode::Dead);
    }

    PendingDeaths.Add(PlayerID, Snap);

    // ★ 立即处理: 应用死亡惩罚 + 生成尸体(立即消失)
    ApplyDeathPenalty(DeadCharacter, PendingDeaths[PlayerID]);
    SpawnCorpse(DeadCharacter, PendingDeaths[PlayerID]);

    // 通知事件
    OnPlayerDiedEvent.Broadcast(Snap);

    // ★ 自动复活计时(3秒后)
    FRespawnTimer Timer;
    Timer.PlayerNetID = PlayerID;
    Timer.TimeRemaining = PenaltyRules.CorpseFadeDuration + 2.5f; // 淡出后2.5秒
    Timer.RespawnType = PenaltyRules.bPreferHospitalRespawn ? NAME("Hospital") : NAME("Wilderness");
    Timer.LocationID = NAME_None;
    PendingRespawns.Add(Timer);

    UE_LOG(LogTemp, Warning, TEXT("[Death] Player %s died (cause=%d). Auto-respawn in %.1fs"),
        *PlayerID, (int)Cause, Timer.TimeRemaining);
}

// ========== 创建尸体(关键修复: 立即消失) ==========
void APlayerDeathManager::SpawnCorpse(AMyCharacter* DeadCharacter, const FPlayerDeathSnapshot& Snapshot)
{
    if (!DeadCharacter || !GetAuthority()) return;

    FName CorpseID = GenerateCorpseID();

    if (bSpawnCorpseActor && PenaltyRules.bCorpseInstantDespawn)
    {
        // ★ 生成尸体 Actor 但立即开始淡出
        AActor* CorpseActor = GetWorld()->SpawnActor<AActor>(
            DeadCharacter->GetClass(),
            DeadCharacter->GetActorLocation(),
            DeadCharacter->GetActorRotation()
        );

        if (CorpseActor)
        {
            CorpseActors.Add(CorpseID, CorpseActor);

            // ★ 立即关闭碰撞(不可交互)
            CorpseActor->SetActorEnableCollision(false);

            // 极短淡出后销毁(默认0.5秒)
            float FadeTime = PenaltyRules.CorpseFadeDuration;
            if (PenaltyRules.bFadeOutCorpse && FadeTime > 0.f)
            {
                FadeOutAndDespawnCorpse(CorpseID, FadeTime);
            }
            else
            {
                // 完全立即(无淡出)
                InstantDespawnCorpse(CorpseID);
            }
        }
    }
    else
    {
        // 不生成尸体 Actor(完全无痕)
        UE_LOG(LogTemp, Log, TEXT("[Death] Corpse skipped (instant despawn)"));
    }

    // 通知: 尸体已处理
    OnCorpseDespawnedEvent.Broadcast(CorpseID);
}

void APlayerDeathManager::InstantDespawnCorpse(FName CorpseID)
{
    AActor** Found = CorpseActors.Find(CorpseID);
    if (Found && *Found && IsValid(*Found))
    {
        (*Found)->Destroy();
    }
    CorpseActors.Remove(CorpseID);
}

void APlayerDeathManager::FadeOutAndDespawnCorpse(FName CorpseID, float FadeTime)
{
    AActor* Corpse = nullptr;
    if (AActor** Found = CorpseActors.Find(CorpseID))
    {
        Corpse = *Found;
    }

    if (!Corpse || !IsValid(Corpse)) return;

    // 设置渲染参数(淡出)
    TArray<UPrimitiveComponent*> PrimComps;
    Corpse->GetComponents<UPrimitiveComponent>(PrimComps);
    for (UPrimitiveComponent* Prim : PrimComps)
    {
        Prim->SetCastShadow(false);
        // 实际项目中应修改材质透明度
    }

    // 定时销毁
    FTimerHandle Timer;
    GetWorld()->GetTimerManager().SetTimer(Timer, [this, CorpseID]()
    {
        InstantDespawnCorpse(CorpseID);
    }, FadeTime, false);

    UE_LOG(LogTemp, Log, TEXT("[Death] Corpse fading out in %.1fs"), FadeTime);
}

// ========== 复活(带完整物品转移) ==========
void APlayerDeathManager::Server_RespawnAtHospital_Implementation(AController* Player, FName HospitalPointID)
{
    if (!Player || !GetAuthority()) return;

    FString PlayerID = Player->GetName();
    if (!PendingDeaths.Contains(PlayerID)) return;

    FPlayerDeathSnapshot& Snap = PendingDeaths[PlayerID];
    Snap.bIsHospitalRespawn = true;
    Snap.RespawnPointID = HospitalPointID;

    // 查找/创建新 Character
    AMyCharacter* NewChar = Cast<AMyCharacter>(Player->GetPawn());
    if (!NewChar)
    {
        // 需要 Spawn 新 Pawn
        if (AStellarGameMode* GM = Cast<AStellarGameMode>(GetWorld()->GetAuthGameMode()))
        {
            FVector SpawnLoc = GM->GetHospitalSpawnLocation(HospitalPointID);
            NewChar = GM->SpawnCharacterAt(Player, SpawnLoc);
        }
    }

    if (NewChar)
    {
        // ★ 关键: 100% 恢复所有物品
        RestoreInventoryToNewPawn(NewChar, Snap);

        // 医院治疗
        ApplyHospitalHealing(NewChar, PenaltyRules.HospitalHealPercent);

        // 复活无敌
        GrantRespawnInvulnerability(NewChar, PenaltyRules.RespawnInvulnerabilityTime);

        // 清除死亡状态
        PendingDeaths.Remove(PlayerID);

        OnPlayerRespawnedWithGearEvent.Broadcast(NewChar, Snap);

        UE_LOG(LogTemp, Log, TEXT("[Death] Player %s respawned at hospital (full inventory restored)"), *PlayerID);
    }
}

bool APlayerDeathManager::Server_RespawnAtHospital_Validate(AController*, FName)
{
    return true;
}

void APlayerDeathManager::Server_RespawnAtNearest_Implementation(AController* Player)
{
    if (!Player || !GetAuthority()) return;

    FString PlayerID = Player->GetName();
    if (!PendingDeaths.Contains(PlayerID)) return;

    FPlayerDeathSnapshot& Snap = PendingDeaths[PlayerID];
    Snap.bIsHospitalRespawn = false;

    // 在最近复活点生成
    AMyCharacter* NewChar = nullptr;
    if (AStellarGameMode* GM = Cast<AStellarGameMode>(GetWorld()->GetAuthGameMode()))
    {
        FVector SpawnLoc = GM->GetNearestRespawnLocation(Snap.DeathLocation);
        NewChar = GM->SpawnCharacterAt(Player, SpawnLoc);
    }

    if (NewChar)
    {
        // 野外复活: 只保留部分物品(根据规则)
        RestoreInventoryToNewPawn(NewChar, Snap);

        // 野外治疗较少
        ApplyHospitalHealing(NewChar, PenaltyRules.WildernessHealPercent);

        GrantRespawnInvulnerability(NewChar, PenaltyRules.RespawnInvulnerabilityTime);

        PendingDeaths.Remove(PlayerID);

        OnPlayerRespawnedWithGearEvent.Broadcast(NewChar, Snap);

        UE_LOG(LogTemp, Log, TEXT("[Death] Player %s respawned in wilderness"), *PlayerID);
    }
}

bool APlayerDeathManager::Server_RespawnAtNearest_Validate(AController*)
{
    return true;
}

void APlayerDeathManager::Server_RespawnAtPlayerStructure_Implementation(AController* Player, FName StructureID)
{
    if (!Player || !GetAuthority()) return;

    FString PlayerID = Player->GetName();
    if (!PendingDeaths.Contains(PlayerID)) return;

    FPlayerDeathSnapshot& Snap = PendingDeaths[PlayerID];
    Snap.bIsHospitalRespawn = true; // 主权建筑内复活同医院待遇
    Snap.RespawnPointID = StructureID;

    AMyCharacter* NewChar = nullptr;
    if (AStellarGameMode* GM = Cast<AStellarGameMode>(GetWorld()->GetAuthGameMode()))
    {
        FVector SpawnLoc = GM->GetPlayerStructureSpawnLocation(StructureID);
        NewChar = GM->SpawnCharacterAt(Player, SpawnLoc);
    }

    if (NewChar)
    {
        RestoreInventoryToNewPawn(NewChar, Snap);
        ApplyHospitalHealing(NewChar, PenaltyRules.HospitalHealPercent);
        GrantRespawnInvulnerability(NewChar, PenaltyRules.RespawnInvulnerabilityTime);
        PendingDeaths.Remove(PlayerID);
        OnPlayerRespawnedWithGearEvent.Broadcast(NewChar, Snap);
    }
}

bool APlayerDeathManager::Server_RespawnAtPlayerStructure_Validate(AController*, FName)
{
    return true;
}

void APlayerDeathManager::AdminForceRespawn(AController* Player, FVector Location, FRotator Rotation)
{
    if (!Player || !GetAuthority()) return;

    if (AStellarGameMode* GM = Cast<AStellarGameMode>(GetWorld()->GetAuthGameMode()))
    {
        AMyCharacter* NewChar = GM->SpawnCharacterAt(Player, Location);
        if (NewChar)
        {
            NewChar->SetActorRotation(Rotation);
            GrantRespawnInvulnerability(NewChar, 10.f);

            FString PlayerID = Player->GetName();
            if (PendingDeaths.Contains(PlayerID))
            {
                RestoreInventoryToNewPawn(NewChar, PendingDeaths[PlayerID]);
                PendingDeaths.Remove(PlayerID);
            }
        }
    }
}

// ========== 物品转移(核心 - 100% 保留) ==========
FPlayerDeathSnapshot APlayerDeathManager::CaptureDeathSnapshot(AMyCharacter* Character, EDeathCause Cause)
{
    FPlayerDeathSnapshot Snap;
    if (!Character) return Snap;

    Snap.PlayerID = Character->GetName();
    Snap.PlayerName = Character->GetCharacterName();
    Snap.Cause = Cause;
    Snap.DeathLocation = Character->GetActorLocation();
    Snap.DeathTime = FDateTime::Now();

    SaveInventorySnapshot(Character, Snap);

    return Snap;
}

void APlayerDeathManager::SaveInventorySnapshot(AMyCharacter* Char, FPlayerDeathSnapshot& Out)
{
    if (!Char) return;

    // 装备
    if (UInventoryComponent* Inv = Char->FindComponentByClass<UInventoryComponent>())
    {
        Out.EquippedItems = Inv->GetAllEquippedItems();
        Out.InventorySlots = Inv->GetAllInventorySlots();
        Out.InventoryWeight = Inv->GetTotalWeight();
        Out.HotbarItems = Inv->GetHotbarItems();
        Out.AmmoInventory = Inv->GetAllAmmo();
        Out.TotalItemCountAtDeath = Inv->GetTotalItemCount();

        // 记录所有物品名(用于验证)
        for (const FInventorySlot& Slot : Out.InventorySlots)
        {
            Out.AllItemsAtDeath.Add(Slot.ItemID);
        }
        for (const auto& Pair : Out.EquippedItems)
        {
            Out.AllItemsAtDeath.Add(Pair.Value.ItemID);
        }
    }

    // 货币
    if (UCurrencyComponent* Cur = Char->FindComponentByClass<UCurrencyComponent>())
    {
        Out.CurrencyAtDeath = Cur->GetAllCurrencies();
    }

    // 维生
    if (UVitalsComponent* Vitals = Char->FindComponentByClass<UVitalsComponent>())
    {
        Out.HealthAtDeath = Vitals->GetHealth();
        Out.StaminaAtDeath = Vitals->GetStamina();
        Out.HungerAtDeath = Vitals->GetHunger();
        Out.ThirstAtDeath = Vitals->GetThirst();
        Out.OxygenAtDeath = Vitals->GetOxygen();
    }
}

void APlayerDeathManager::RestoreInventoryToNewPawn(AMyCharacter* NewCharacter, const FPlayerDeathSnapshot& Snapshot)
{
    if (!NewCharacter) return;

    int32 RestoredCount = 0;

    // ★ 恢复装备(100%)
    RestoreEquippedItems(NewCharacter, Snapshot);
    RestoredCount += Snapshot.EquippedItems.Num();

    // ★ 恢复背包(100%)
    RestoreInventorySlots(NewCharacter, Snapshot);
    RestoredCount += Snapshot.InventorySlots.Num();

    // ★ 恢复弹药(100%)
    RestoreAmmo(NewCharacter, Snapshot);
    RestoredCount += Snapshot.AmmoInventory.Num();

    // ★ 恢复货币(扣除惩罚后)
    RestoreCurrency(NewCharacter, Snapshot);

    // ★ 恢复快捷栏
    RestoreHotbar(NewCharacter, Snapshot);

    // 验证完整性
    bool bVerified = VerifyInventoryIntegrity(NewCharacter, Snapshot);

    OnInventoryTransferredEvent.Broadcast(
        FName(*Snapshot.PlayerID),
        RestoredCount,
        bVerified
    );

    UE_LOG(LogTemp, Log, TEXT("[Death] Inventory restored: %d items, verified=%s"),
        RestoredCount, bVerified ? TEXT("true") : TEXT("false"));
}

void APlayerDeathManager::RestoreEquippedItems(AMyCharacter* Char, const FPlayerDeathSnapshot& Snap)
{
    if (!Char) return;
    if (UInventoryComponent* Inv = Char->FindComponentByClass<UInventoryComponent>())
    {
        for (const auto& Pair : Snap.EquippedItems)
        {
            Inv->EquipItemFromSnapshot(Pair.Key, Pair.Value);
        }
    }
}

void APlayerDeathManager::RestoreInventorySlots(AMyCharacter* Char, const FPlayerDeathSnapshot& Snap)
{
    if (!Char) return;
    if (UInventoryComponent* Inv = Char->FindComponentByClass<UInventoryComponent>())
    {
        for (const FInventorySlot& Slot : Snap.InventorySlots)
        {
            Inv->AddItemFromSnapshot(Slot);
        }
    }
}

void APlayerDeathManager::RestoreAmmo(AMyCharacter* Char, const FPlayerDeathSnapshot& Snap)
{
    if (!Char) return;
    if (UInventoryComponent* Inv = Char->FindComponentByClass<UInventoryComponent>())
    {
        for (const auto& Pair : Snap.AmmoInventory)
        {
            Inv->SetAmmo(Pair.Key, Pair.Value);
        }
    }
}

void APlayerDeathManager::RestoreCurrency(AMyCharacter* Char, const FPlayerDeathSnapshot& Snap)
{
    if (!Char) return;
    if (UCurrencyComponent* Cur = Char->FindComponentByClass<UCurrencyComponent>())
    {
        for (const auto& Pair : Snap.CurrencyAtDeath)
        {
            float Lost = Snap.CurrencyLost.Contains(Pair.Key) ? Snap.CurrencyLost[Pair.Key] : 0.f;
            float Restored = FMath::Max(0.f, Pair.Value - Lost);
            Cur->SetCurrency(Pair.Key, Restored);
        }
    }
}

void APlayerDeathManager::RestoreHotbar(AMyCharacter* Char, const FPlayerDeathSnapshot& Snap)
{
    if (!Char) return;
    if (UInventoryComponent* Inv = Char->FindComponentByClass<UInventoryComponent>())
    {
        Inv->SetHotbarItems(Snap.HotbarItems);
    }
}

bool APlayerDeathManager::VerifyInventoryIntegrity(AMyCharacter* Char, const FPlayerDeathSnapshot& Snap) const
{
    if (!Char) return false;

    if (UInventoryComponent* Inv = Char->FindComponentByClass<UInventoryComponent>())
    {
        int32 CurrentCount = Inv->GetTotalItemCount();
        // 允许少量误差(如消耗品在死亡过程中被使用)
        int32 ExpectedMin = FMath::FloorToInt(Snap.TotalItemCountAtDeath * 0.95f); // 95% 容差
        return CurrentCount >= ExpectedMin;
    }
    return false;
}

void APlayerDeathManager::ApplyDeathPenalty(AMyCharacter* Character, FPlayerDeathSnapshot& Snapshot)
{
    if (!Character) return;

    bool bIsHospital = Snapshot.bIsHospitalRespawn;

    // 计算货币惩罚
    if (UCurrencyComponent* Cur = Character->FindComponentByClass<UCurrencyComponent>())
    {
        TMap<FName, float> AllCur = Cur->GetAllCurrencies();
        for (const auto& Pair : AllCur)
        {
            float LossPercent = bIsHospital ?
                PenaltyRules.CreditLossPercentHospital :
                PenaltyRules.CreditLossPercentWilderness;

            float Loss = Pair.Value * LossPercent;
            Snapshot.CurrencyLost.Add(Pair.Key, Loss);
            Cur->SetCurrency(Pair.Key, Pair.Value - Loss);
        }
    }

    // 医院复活: 装备/背包/弹药全保留
    if (bIsHospital)
    {
        // 不丢任何物品
        return;
    }

    // 野外复活: 根据规则可能掉落
    if (PenaltyRules.bDropInventoryItemsWilderness)
    {
        // 掉落30%随机物品
        if (UInventoryComponent* Inv = Character->FindComponentByClass<UInventoryComponent>())
        {
            Inv->DropRandomPercentage(0.3f);
        }
    }

    if (PenaltyRules.bDropAmmoWilderness)
    {
        if (UInventoryComponent* Inv = Character->FindComponentByClass<UInventoryComponent>())
        {
            Inv->DropRandomAmmo(0.5f);
        }
    }
}

void APlayerDeathManager::ApplyHospitalHealing(AMyCharacter* Char, float HealPercent)
{
    if (!Char) return;
    if (UVitalsComponent* Vitals = Char->FindComponentByClass<UVitalsComponent>())
    {
        float MaxHP = Vitals->GetMaxHealth();
        Vitals->SetHealth(MaxHP * HealPercent);
        Vitals->SetStamina(Vitals->GetMaxStamina() * 0.8f);
    }
}

void APlayerDeathManager::GrantRespawnInvulnerability(AMyCharacter* Char, float Duration)
{
    if (!Char) return;
    Char->SetInvulnerable(true);
    FTimerHandle Timer;
    GetWorld()->GetTimerManager().SetTimer(Timer, [Char]()
    {
        if (Char && IsValid(Char)) Char->SetInvulnerable(false);
    }, Duration, false);
}

// ========== 查询 ==========
FPlayerDeathSnapshot APlayerDeathManager::GetDeathSnapshot(AController* Player) const
{
    if (!Player) return FPlayerDeathSnapshot();
    FString ID = Player->GetName();
    const FPlayerDeathSnapshot* Found = PendingDeaths.Find(ID);
    return Found ? *Found : FPlayerDeathSnapshot();
}

bool APlayerDeathManager::HasPendingDeath(AController* Player) const
{
    if (!Player) return false;
    return PendingDeaths.Contains(Player->GetName());
}

TArray<FPlayerDeathSnapshot> APlayerDeathManager::GetAllPendingDeaths() const
{
    TArray<FPlayerDeathSnapshot> Out;
    for (const auto& Pair : PendingDeaths)
    {
        Out.Add(Pair.Value);
    }
    return Out;
}

// ========== 计时器 ==========
void APlayerDeathManager::TickRespawnTimers(float Dt)
{
    if (!GetAuthority()) return;

    for (int32 i = PendingRespawns.Num() - 1; i >= 0; --i)
    {
        FRespawnTimer& Timer = PendingRespawns[i];
        Timer.TimeRemaining -= Dt;

        if (Timer.TimeRemaining <= 0.f)
        {
            // 自动触发复活
            AController* Player = FindPlayerByNetID(Timer.PlayerNetID);
            if (Player)
            {
                if (Timer.RespawnType == NAME("Hospital"))
                {
                    Server_RespawnAtHospital(Player, Timer.LocationID);
                }
                else if (Timer.RespawnType == NAME("Structure"))
                {
                    Server_RespawnAtPlayerStructure(Player, Timer.LocationID);
                }
                else
                {
                    Server_RespawnAtNearest(Player);
                }
            }
            PendingRespawns.RemoveAt(i);
        }
    }
}

// ========== 辅助 ==========
AController* APlayerDeathManager::FindPlayerByNetID(const FString& NetID) const
{
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        AController* PC = It->Get();
        if (PC && PC->GetName() == NetID)
        {
            return PC;
        }
    }
    return nullptr;
}

ARespawnManager* APlayerDeathManager::GetRespawnManager() const
{
    return GetWorld() ? GetWorld()->GetGameState()->FindComponentByClass<ARespawnManager>() : nullptr;
}

FName APlayerDeathManager::GenerateCorpseID()
{
    CorpseIDCounter++;
    return FName(*FString::Printf(TEXT("CRP_%d"), CorpseIDCounter));
}

// ========== 网络 ==========
void APlayerDeathManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(APlayerDeathManager, PendingDeaths);
}
