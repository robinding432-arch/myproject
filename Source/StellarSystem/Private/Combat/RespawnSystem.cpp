// RespawnSystem.cpp
// 复活点系统实现

#include "Combat/RespawnSystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"

ARespawnManager::ARespawnManager()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
}

void ARespawnManager::BeginPlay()
{
    Super::BeginPlay();

    // 确保所有复活点数据已初始化
    for (FRespawnPointData& Point : RespawnPoints)
    {
        if (Point.PointID.IsEmpty())
        {
            Point.PointID = FString::Printf(TEXT("respawn_%d"),
                FMath::RandRange(10000, 99999));
        }
        if (Point.DisplayName.IsEmpty())
        {
            Point.DisplayName = Point.PointID;
        }
    }
}

void ARespawnManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ARespawnManager, PlayerRespawnCounts);
    DOREPLIFETIME(ARespawnManager, CustomRespawnPoints);
}

void ARespawnManager::RegisterRespawnPoint(const FRespawnPointData& PointData)
{
    if (HasAuthority())
    {
        RespawnPoints.Add(PointData);
        UE_LOG(LogTemp, Log, TEXT("[Respawn] Registered: %s at %s"),
            *PointData.PointID, *PointData.Location.ToString());
    }
}

bool ARespawnManager::UnlockRespawnPoint(const FString& PointID, const FString& UnlockerID)
{
    if (!HasAuthority()) return false;

    for (FRespawnPointData& Point : RespawnPoints)
    {
        if (Point.PointID == PointID && !Point.bUnlocked)
        {
            Point.bUnlocked = true;
            OnRespawnPointUnlocked.Broadcast(PointID);

            // 通知解锁者
            if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
            {
                // 可以在这里发送通知
            }

            UE_LOG(LogTemp, Log, TEXT("[Respawn] Unlocked: %s by %s"),
                *PointID, *UnlockerID);
            return true;
        }
    }
    return false;
}

void ARespawnManager::LockRespawnPoint(const FString& PointID)
{
    if (!HasAuthority()) return;

    for (FRespawnPointData& Point : RespawnPoints)
    {
        if (Point.PointID == PointID)
        {
            Point.bUnlocked = false;
            UE_LOG(LogTemp, Log, TEXT("[Respawn] Locked: %s"), *PointID);
            break;
        }
    }
}

TArray<FRespawnPointData> ARespawnManager::GetAvailableRespawnPoints(const FString& PlayerID) const
{
    TArray<FRespawnPointData> Result;

    for (const FRespawnPointData& Point : RespawnPoints)
    {
        if (Point.bUnlocked)
        {
            // 检查派系归属
            if (!Point.OwningFaction.IsEmpty())
            {
                // 检查玩家派系声望（简化）
                // 实际应查询 FactionSystem
            }
            Result.Add(Point);
        }
    }

    // 加入自定义复活点
    if (const FRespawnPointData* Custom = CustomRespawnPoints.Find(PlayerID))
    {
        Result.Add(*Custom);
    }

    return Result;
}

FRespawnPointData ARespawnManager::GetNearestRespawnPoint(const FVector& Location) const
{
    FRespawnPointData Nearest;
    Nearest.bUnlocked = false;
    float BestDist = TNumericLimits<float>::Max();

    for (const FRespawnPointData& Point : RespawnPoints)
    {
        if (!Point.bUnlocked) continue;

        float Dist = FVector::DistSquared(Location, Point.Location);
        if (Dist < BestDist)
        {
            BestDist = Dist;
            Nearest = Point;
        }
    }

    // 检查自定义复活点
    for (const auto& Pair : CustomRespawnPoints)
    {
        float Dist = FVector::DistSquared(Location, Pair.Value.Location);
        if (Dist < BestDist)
        {
            BestDist = Dist;
            Nearest = Pair.Value;
        }
    }

    return Nearest;
}

APawn* ARespawnManager::ExecuteRespawn(const FString& PlayerID, const FString& PointID)
{
    if (!HasAuthority()) return nullptr;

    FRespawnPointData* TargetPoint = nullptr;

    // 查找预设复活点
    for (FRespawnPointData& Point : RespawnPoints)
    {
        if (Point.PointID == PointID)
        {
            TargetPoint = &Point;
            break;
        }
    }

    // 查找自定义复活点
    if (!TargetPoint)
    {
        if (FRespawnPointData* Custom = CustomRespawnPoints.Find(PlayerID))
        {
            if (Custom->PointID == PointID)
                TargetPoint = Custom;
        }
    }

    if (!TargetPoint || !TargetPoint->bUnlocked)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Respawn] Point not found or locked: %s"), *PointID);
        return nullptr;
    }

    // 检查复活次数
    if (MaxRespawns > 0)
    {
        int32& Count = PlayerRespawnCounts.FindOrAdd(PlayerID);
        if (Count >= MaxRespawns)
        {
            UE_LOG(LogTemp, Warning, TEXT("[Respawn] Max respawns reached for %s"), *PlayerID);
            OnAllRespawnPointsLocked.Broadcast();
            return nullptr;
        }
        Count++;
    }

    // 生成新 Pawn
    UWorld* World = GetWorld();
    if (!World) return nullptr;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    // 这里需要知道要生成什么 Pawn 类
    // 简化：从 GameMode 获取默认 Pawn 类
    AGameModeBase* GM = World->GetAuthGameMode();
    if (!GM) return nullptr;

    APawn* NewPawn = World->SpawnActor<APawn>(
        GM->DefaultPawnClass, TargetPoint->Location, TargetPoint->Rotation, SpawnParams);

    if (NewPawn)
    {
        // 恢复状态
        RestorePlayerState(NewPawn, *TargetPoint);

        // 生成特效
        SpawnRespawnEffects(TargetPoint->Location);

        // 应用安全时间
        if (TargetPoint->bSafeZone)
        {
            ApplySafeTime(NewPawn, TargetPoint->SafeTime);
        }

        OnPlayerRespawned.Broadcast(PointID, NewPawn);

        UE_LOG(LogTemp, Log, TEXT("[Respawn] Player %s respawned at %s"),
            *PlayerID, *PointID);
    }

    return NewPawn;
}

APawn* ARespawnManager::QuickRespawn(const FString& PlayerID, const FVector& CurrentLocation)
{
    FRespawnPointData Nearest = GetNearestRespawnPoint(CurrentLocation);
    if (!Nearest.bUnlocked)
    {
        // 尝试解锁最近的
        if (!Nearest.PointID.IsEmpty())
        {
            UnlockRespawnPoint(Nearest.PointID, PlayerID);
        }
    }

    return ExecuteRespawn(PlayerID, Nearest.PointID);
}

bool ARespawnManager::SetCustomRespawnPoint(const FString& PlayerID, const FVector& Location,
    const FRotator& Rotation)
{
    if (!HasAuthority()) return false;

    FRespawnPointData Custom;
    Custom.PointID = FString::Printf(TEXT("custom_%s"), *PlayerID);
    Custom.DisplayName = TEXT("Custom Respawn");
    Custom.Location = Location;
    Custom.Rotation = Rotation;
    Custom.bUnlocked = true;
    Custom.Type = ERespawnType::Outpost;
    Custom.HealOnRespawn = 0.3f;
    Custom.ShieldRestoreOnRespawn = 0.2f;
    Custom.bSafeZone = false;

    CustomRespawnPoints.Add(PlayerID, Custom);

    UE_LOG(LogTemp, Log, TEXT("[Respawn] Custom point set for %s"), *PlayerID);
    return true;
}

int32 ARespawnManager::GetRespawnCount(const FString& PlayerID) const
{
    if (const int32* Count = PlayerRespawnCounts.Find(PlayerID))
        return *Count;
    return 0;
}

void ARespawnManager::ResetRespawnCount(const FString& PlayerID)
{
    if (HasAuthority())
    {
        PlayerRespawnCounts.Remove(PlayerID);
    }
}

// —— 私有方法 ——

void ARespawnManager::SpawnRespawnEffects(const FVector& Location)
{
    // 生成复活粒子特效
    // 简化：用引擎内置粒子
    // 实际应在蓝图中实现炫酷的传送/光束效果
    UE_LOG(LogTemp, Log, TEXT("[Respawn] Effects at %s"), *Location.ToString());
}

void ARespawnManager::RestorePlayerState(APawn* NewPawn, const FRespawnPointData& Point)
{
    if (!NewPawn) return;

    // 恢复 HP（通过 VitalsComponent）
    // 实际应查询角色的 VitalsComponent
    float HP = Point.HealOnRespawn; // 0~1
    float Shield = Point.ShieldRestoreOnRespawn;

    UE_LOG(LogTemp, Log, TEXT("[Respawn] Restored HP: %.0f%% Shield: %.0f%%"),
        HP * 100.f, Shield * 100.f);

    // 恢复维生指标
    if (bRestoreVitalsOnRespawn)
    {
        // 氧气/能量/饥饿等恢复到 80%
        // 实际应调用 VitalsComponent->RestoreAll(0.8f)
    }
}

void ARespawnManager::ApplySafeTime(APawn* NewPawn, float Duration)
{
    if (!NewPawn) return;

    // 设置无敌时间
    // 实际应调用 NewPawn->SetInvulnerable(true)
    // 然后用 Timer 在 Duration 秒后关闭

    UE_LOG(LogTemp, Log, TEXT("[Respawn] Safe time: %.1fs"), Duration);
}
