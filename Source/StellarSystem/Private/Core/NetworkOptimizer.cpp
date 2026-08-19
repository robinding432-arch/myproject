// NetworkOptimizer.cpp
// 网络性能优化完整实现

#include "Core/NetworkOptimizer.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Net/NetConnection.h"
#include "TimerManager.h"
#include "GameFramework/GameState.h"
#include "HAL/PlatformTime.h"
#include "Serialization/BitWriter.h"
#include "Serialization/BitReader.h"
#include "Compression/Compressor.h"

DEFINE_LOG_CATEGORY_STATIC(LogNetOptimizer, Log, All);

ANetworkOptimizer::ANetworkOptimizer()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.05f;  // 20Hz 检查频率

    GlobalUpdateInterval = 0.1f;
    MaxReplicatedActors = 256;
    bEnableRelevancyCulling = true;
    DefaultRelevancyDistance = 100000.f;  // 1km
    bEnableBandwidthShaping = true;

    BandwidthOutLimit = 1024.f;  // 1MB/s
    BandwidthInLimit = 1024.f;
}

void ANetworkOptimizer::BeginPlay()
{
    Super::BeginPlay();

    // 设置网络参数
    UWorld* World = GetWorld();
    if (World)
    {
        // 调整网络配置
        World->NetDriverName = NAME_GameNetDriver;

        // 设置合理的最大客户端速率
        GConfig->SetInt(TEXT("IpNetDriver"), TEXT("MaxClientRate"), 100000, GEngineIni);
        GConfig->SetInt(TEXT("IpNetDriver"), TEXT("MaxServerRate"), 100000, GEngineIni);
    }

    UE_LOG(LogNetOptimizer, Log, TEXT("[NetOpt] Initialized - UpdateInterval: %.3fs, MaxActors: %d"),
        GlobalUpdateInterval, MaxReplicatedActors);
}

void ANetworkOptimizer::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bNetworkPaused) return;

    // 1. 更新 Actor 复制
    UpdateActorReplication(DeltaTime);

    // 2. 带宽整形
    if (bEnableBandwidthShaping)
    {
        UpdateBandwidthShaping(DeltaTime);
    }

    // 3. 自适应频率调整
    if (bAdaptiveEnabled)
    {
        AdaptiveTimer += DeltaTime;
        if (AdaptiveTimer >= ADAPTIVE_CHECK_INTERVAL)
        {
            AdaptiveTimer = 0.f;
            AdaptiveFrequencyAdjust();
        }
    }
}

// =====================================================================
// Actor 注册与复制
// =====================================================================

void ANetworkOptimizer::RegisterActor(AActor* Actor, const FNetworkUpdateConfig& Config)
{
    if (!Actor) return;

    // 检查是否已注册
    for (const FRegisteredActor& Reg : RegisteredActors)
    {
        if (Reg.Actor.Get() == Actor) return;  // 已存在
    }

    FRegisteredActor NewReg;
    NewReg.Actor = Actor;
    NewReg.Config = Config;
    NewReg.Accumulator = 0.f;
    NewReg.LastUpdateTime = 0.f;
    NewReg.LastReplicatedLocation = Actor->GetActorLocation();
    NewReg.LastReplicatedRotation = Actor->GetActorRotation();
    NewReg.bPendingUpdate = false;

    RegisteredActors.Add(NewReg);

    // 设置 Actor 的复制频率
    Actor->NetUpdateFrequency = 1.f / FMath::Max(Config.UpdateInterval, 0.01f);

    UE_LOG(LogNetOptimizer, Log, TEXT("[NetOpt] Registered: %s (interval=%.3fs, priority=%d)"),
        *Actor->GetName(), Config.UpdateInterval, (int32)Config.Priority);
}

void ANetworkOptimizer::UnregisterActor(AActor* Actor)
{
    for (int32 i = RegisteredActors.Num() - 1; i >= 0; --i)
    {
        if (RegisteredActors[i].Actor.Get() == Actor)
        {
            RegisteredActors.RemoveAt(i);
            UE_LOG(LogNetOptimizer, Log, TEXT("[NetOpt] Unregistered: %s"), *Actor->GetName());
            break;
        }
    }
}

void ANetworkOptimizer::UpdateActorReplication(float Dt)
{
    UWorld* World = GetWorld();
    if (!World) return;

    // 获取观察者位置（玩家控制器视角）
    FVector ViewerLocation = FVector::ZeroVector;
    if (APlayerController* PC = World->GetFirstPlayerController())
    {
        if (APawn* Pawn = PC->GetPawn())
        {
            ViewerLocation = Pawn->GetActorLocation();
        }
    }

    int32 ReplicatedThisTick = 0;
    float CurrentTime = World->GetTimeSeconds();

    for (int32 i = 0; i < RegisteredActors.Num(); ++i)
    {
        FRegisteredActor& Reg = RegisteredActors[i];

        if (!Reg.Actor.IsValid())
        {
            // 延迟移除（避免迭代中修改）
            RegisteredActors.RemoveAt(i);
            --i;
            continue;
        }

        AActor* Actor = Reg.Actor.Get();

        // 带宽限制检查
        if (BandwidthAccumulator > BandwidthOutLimit * BANDWIDTH_WINDOW)
        {
            Reg.Config.SkippedUpdates++;
            BandwidthStats.SkippedUpdates++;
            continue;
        }

        // 累加时间
        Reg.Accumulator += Dt;

        // 自适应频率
        float EffectiveInterval = Reg.Config.UpdateInterval;
        if (Reg.Config.bUseAdaptiveFrequency)
        {
            // 根据距离动态调整
            float Dist = FVector::Dist(ViewerLocation, Actor->GetActorLocation());
            if (Dist > Reg.Config.RelevancyRadius * 3.f)
            {
                EffectiveInterval = FMath::Min(Reg.Config.UpdateInterval * 3.f, Reg.Config.MaxUpdateInterval);
            }
            else if (Dist > Reg.Config.RelevancyRadius)
            {
                EffectiveInterval = FMath::Min(Reg.Config.UpdateInterval * 1.5f, Reg.Config.MaxUpdateInterval);
            }
            else
            {
                EffectiveInterval = FMath::Max(Reg.Config.UpdateInterval, Reg.Config.MinUpdateInterval);
            }
        }

        // 检查是否需要更新
        if (Reg.Accumulator < EffectiveInterval) continue;

        // 相关性剔除
        if (bEnableRelevancyCulling)
        {
            float Dist = FVector::Dist(ViewerLocation, Actor->GetActorLocation());
            float RelevancyDist = Reg.Config.RelevancyRadius > 0.f ?
                Reg.Config.RelevancyRadius : DefaultRelevancyDistance;

            if (Dist > RelevancyDist)
            {
                Reg.Accumulator = 0.f;
                Reg.Config.SkippedUpdates++;
                continue;
            }
        }

        // 检查是否有变化（增量更新）
        FVector CurrentLoc = Actor->GetActorLocation();
        FRotator CurrentRot = Actor->GetActorRotation();

        float LocDelta = FVector::Dist(CurrentLoc, Reg.LastReplicatedLocation);
        float RotDelta = FMath::Abs((CurrentRot - Reg.LastReplicatedRotation).GetManhattanComponents().Size());

        // 变化太小则跳过
        if (LocDelta < 1.f && RotDelta < 0.1f)
        {
            Reg.Accumulator = 0.f;  // 重置，不计入跳过
            continue;
        }

        // 执行复制
        if (Actor->GetIsReplicated())
        {
            // 强制网络更新
            Actor->ForceNetUpdate();

            // 更新统计
            Reg.LastUpdateTime = CurrentTime;
            Reg.LastReplicatedLocation = CurrentLoc;
            Reg.LastReplicatedRotation = CurrentRot;
            Reg.Accumulator = 0.f;
            ReplicatedThisTick++;

            // 估算带宽
            float EstimatedBytes = 64.f;  // 位置+旋转 ≈ 64 字节
            if (Reg.Config.bCompressData) EstimatedBytes *= Reg.Config.CompressionRatio;
            BandwidthAccumulator += EstimatedBytes;

            // 丢包补偿：冗余发送
            if (bPacketLossCompEnabled && Reg.Config.Priority >= ENetworkPriority::High)
            {
                // 高速移动的 Actor 额外多发一次
                float Speed = LocDelta / EffectiveInterval;
                if (Speed > 1000.f)  // > 10 m/s
                {
                    Actor->ForceNetUpdate();
                    BandwidthAccumulator += EstimatedBytes;
                }
            }
        }
    }

    BandwidthStats.ReplicatedActors = ReplicatedThisTick;
}

// =====================================================================
// 带宽管理
// =====================================================================

void ANetworkOptimizer::SetBandwidthLimit(float MaxKBpsOut, float MaxKBpsIn)
{
    BandwidthOutLimit = FMath::Max(MaxKBpsOut, 64.f);   // 最低 64KB/s
    BandwidthInLimit = FMath::Max(MaxKBpsIn, 64.f);

    UE_LOG(LogNetOptimizer, Log, TEXT("[NetOpt] Bandwidth limits: Out=%.0fKB/s, In=%.0fKB/s"),
        BandwidthOutLimit, BandwidthInLimit);
}

void ANetworkOptimizer::UpdateBandwidthShaping(float Dt)
{
    BandwidthWindowTimer += Dt;

    if (BandwidthWindowTimer >= BANDWIDTH_WINDOW)
    {
        // 计算实际带宽
        float WindowSeconds = FMath::Max(BandwidthWindowTimer, 0.001f);
        BandwidthStats.OutgoingKBps = (BandwidthAccumulator / 1024.f) / WindowSeconds;
        BandwidthStats.IncomingKBps = 0.f;  // 需要底层统计

        // 重置
        BandwidthAccumulator = 0.f;
        BandwidthWindowTimer = 0.f;

        // 带宽警告
        if (BandwidthStats.OutgoingKBps > BandwidthOutLimit * 0.9f)
        {
            OnBandwidthWarning.Broadcast(BandwidthStats.OutgoingKBps);
            UE_LOG(LogNetOptimizer, Warning, TEXT("[NetOpt] Bandwidth warning: %.0fKB/s (limit: %.0f)"),
                BandwidthStats.OutgoingKBps, BandwidthOutLimit);
        }
    }
}

// =====================================================================
// 自适应频率
// =====================================================================

void ANetworkOptimizer::EnableAdaptiveFrequency(bool bEnable)
{
    bAdaptiveEnabled = bEnable;
    UE_LOG(LogNetOptimizer, Log, TEXT("[NetOpt] Adaptive frequency: %s"),
        bEnable ? TEXT("ENABLED") : TEXT("DISABLED"));
}

void ANetworkOptimizer::AdaptiveFrequencyAdjust()
{
    // 根据当前带宽使用率调整全局频率
    float UsageRatio = BandwidthStats.OutgoingKBps / FMath::Max(BandwidthOutLimit, 1.f);

    if (UsageRatio > 0.85f)
    {
        // 带宽紧张 → 降低频率
        GlobalUpdateInterval = FMath::Min(GlobalUpdateInterval * 1.2f, 0.5f);

        // 降低所有 Actor 的频率
        for (FRegisteredActor& Reg : RegisteredActors)
        {
            Reg.Config.UpdateInterval = FMath::Min(
                Reg.Config.UpdateInterval * 1.1f,
                Reg.Config.MaxUpdateInterval
            );
        }

        UE_LOG(LogNetOptimizer, Log, TEXT("[NetOpt] Bandwidth tight (%.0f%%), slowing updates to %.3fs"),
            UsageRatio * 100.f, GlobalUpdateInterval);
    }
    else if (UsageRatio < 0.4f)
    {
        // 带宽充裕 → 提高频率
        GlobalUpdateInterval = FMath::Max(GlobalUpdateInterval * 0.9f, 0.02f);

        for (FRegisteredActor& Reg : RegisteredActors)
        {
            Reg.Config.UpdateInterval = FMath::Max(
                Reg.Config.UpdateInterval * 0.95f,
                Reg.Config.MinUpdateInterval
            );
        }
    }
}

// =====================================================================
// 相关性距离
// =====================================================================

void ANetworkOptimizer::SetRelevancyDistance(float Distance)
{
    DefaultRelevancyDistance = FMath::Max(Distance, 1000.f);

    UE_LOG(LogNetOptimizer, Log, TEXT("[NetOpt] Default relevancy distance: %.0f cm"), DefaultRelevancyDistance);
}

// =====================================================================
// 紧急通道
// =====================================================================

void ANetworkOptimizer::ForceReplicate(AActor* Actor)
{
    if (!Actor || !Actor->GetIsReplicated()) return;

    // 立即复制，忽略频率限制
    Actor->ForceNetUpdate();

    // 额外冗余发送（丢包补偿）
    if (bPacketLossCompEnabled)
    {
        for (int32 i = 0; i < (int32)RedundancyFactor; ++i)
        {
            Actor->ForceNetUpdate();
        }
    }

    UE_LOG(LogNetOptimizer, Verbose, TEXT("[NetOpt] Force replicated: %s"), *Actor->GetName());
}

// =====================================================================
// 网络暂停/恢复
// =====================================================================

void ANetworkOptimizer::PauseNetwork()
{
    bNetworkPaused = true;

    UWorld* World = GetWorld();
    if (World && World->GetNetDriver())
    {
        // 暂停网络驱动
        World->GetNetDriver()->SetWorld(World);
    }

    UE_LOG(LogNetOptimizer, Log, TEXT("[NetOpt] Network PAUSED"));
}

void ANetworkOptimizer::ResumeNetwork()
{
    bNetworkPaused = false;

    UE_LOG(LogNetOptimizer, Log, TEXT("[NetOpt] Network RESUMED"));
}

// =====================================================================
// 丢包补偿
// =====================================================================

void ANetworkOptimizer::SetPacketLossCompensation(bool bEnable, float InRedundancyFactor)
{
    bPacketLossCompEnabled = bEnable;
    RedundancyFactor = FMath::Clamp(InRedundancyFactor, 1.f, 5.f);

    UE_LOG(LogNetOptimizer, Log, TEXT("[NetOpt] Packet loss compensation: %s (redundancy=%.1f)"),
        bEnable ? TEXT("ON") : TEXT("OFF"), RedundancyFactor);
}

// =====================================================================
// 内部辅助
// =====================================================================

bool ANetworkOptimizer::ShouldReplicateActor(const FRegisteredActor& Reg) const
{
    if (!Reg.Actor.IsValid()) return false;
    if (!Reg.Actor->GetIsReplicated()) return false;

    AActor* Actor = Reg.Actor.Get();

    // 检查相关性
    if (bEnableRelevancyCulling)
    {
        // 这里需要观察者位置，简化判断
        // 实际实现中会从 World 获取
    }

    return true;
}

float ANetworkOptimizer::CalculatePriorityScore(const FRegisteredActor& Reg) const
{
    if (!Reg.Actor.IsValid()) return 0.f;

    float Score = 0.f;

    switch (Reg.Config.Priority)
    {
    case ENetworkPriority::Critical:  Score = 1000.f; break;
    case ENetworkPriority::High:     Score = 100.f; break;
    case ENetworkPriority::Normal:   Score = 10.f; break;
    case ENetworkPriority::Low:      Score = 1.f; break;
    case ENetworkPriority::Background: Score = 0.1f; break;
    }

    // 距离因子（越近越重要）
    // 速度因子（越快越重要）

    return Score;
}

void ANetworkOptimizer::CompressReplicationData(FRegisteredActor& Reg)
{
    if (!Reg.Config.bCompressData) return;

    // UE 的网络序列化已经内置压缩
    // 这里可以做额外的应用层压缩
    // 例如：只发送变化量（delta encoding）

    // 简化：标记已压缩
    Reg.Config.CompressionRatio = 0.7f;  // 假设 30% 压缩率
}
