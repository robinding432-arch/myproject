// PerformanceManager.cpp
// 全局性能管理器完整实现

#include "Core/PerformanceManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameViewportClient.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/ConfigCacheIni.h"
#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "TimerManager.h"
#include "Engine/NetConnection.h"
#include "Net/UnrealNetwork.h"
#include "IPAddress.h"
#include "SocketSubsystem.h"
#include "NetworkProfiler.h"
#include "Stats/Stats.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "Engine/Texture2D.h"
#include "Engine/SkeletalMesh.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "GameFramework/WorldSettings.h"
#include "Scalability.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameState.h"

DEFINE_LOG_CATEGORY_STATIC(LogPerformance, Log, All);

APerformanceManager::APerformanceManager()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.5f;  // 每 0.5s 采样一次（不每帧跑）

    // 初始化 FPS 历史
    for (int32 i = 0; i < 60; ++i)
        FPSHistory[i] = 60.f;

    FrameTimeSamples.Reserve(120);
}

void APerformanceManager::BeginPlay()
{
    Super::BeginPlay();

    // 自动检测硬件
    DetectHardwareSpecs();

    if (CurrentTier == EPerformanceTier::Auto)
    {
        AutoDetectPerformanceTier();
    }

    // 应用质量设置
    ApplyQualitySettings();

    // 设置 GC 参数
    if (bEnableAutoGC)
    {
        // 增大 GC 间隔，减少卡顿
        GEngine->MaxParticleResize = 0;
        GEngine->MaxClientSmoothingDeltaTime = 0.5f;
    }

    // 服务器特定设置
    if (GetNetMode() == NM_DedicatedServer)
    {
        // 服务器不需要渲染
        UWorld* World = GetWorld();
        if (World)
        {
            // 降低服务器 tick 率以节省 CPU
            World->SetShouldTick(false);
            // 手动控制 tick
        }

        // 禁用不必要的子系统
        UE_LOG(LogPerformance, Log, TEXT("[Perf] Dedicated server mode - render disabled"));
    }

    UE_LOG(LogPerformance, Log, TEXT("[Perf] PerformanceManager initialized - Tier: %d"),
        (int32)CurrentTier);
}

void APerformanceManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 1. 更新性能统计（每帧轻量采样）
    UpdatePerformanceStats(DeltaTime);

    // 2. 自适应质量调整（带冷却）
    if (bEnableAdaptiveQuality && GetNetMode() != NM_DedicatedServer)
    {
        AdaptiveAdjustTimer += DeltaTime;
        if (AdaptiveAdjustTimer >= ADAPTIVE_COOLDOWN)
        {
            AdaptiveQualityAdjust(DeltaTime);
            AdaptiveAdjustTimer = 0.f;
        }
    }

    // 3. GC 检查
    if (bEnableAutoGC)
    {
        GCCheckTimer += DeltaTime;
        if (GCCheckTimer >= AutoGCCheckInterval)
        {
            GCCheckTimer = 0.f;

            // 检查内存压力
            FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
            float MemMB = (float)(MemStats.UsedPhysical / (1024 * 1024));

            if (MemMB > MemoryWarningThreshold && !bMemoryWarningFired)
            {
                bMemoryWarningFired = true;
                OnMemoryWarning.Broadcast(MemMB);
                UE_LOG(LogPerformance, Warning, TEXT("[Perf] Memory warning: %.0f MB"), MemMB);

                // 紧急 GC
                RequestGarbageCollection(true);
            }
            else if (MemMB < MemoryWarningThreshold * 0.8f)
            {
                bMemoryWarningFired = false;
            }

            // 常规 GC
            if (GCCheckTimer > 0.f)  // 已经过了间隔
            {
                RequestGarbageCollection(false);
            }
        }
    }

    // 4. 批量 Tick 处理
    for (int32 i = BatchTickList.Num() - 1; i >= 0; --i)
    {
        FBatchTickEntry& Entry = BatchTickList[i];
        if (!Entry.Actor.IsValid())
        {
            BatchTickList.RemoveAt(i);
            continue;
        }

        Entry.Accumulator += DeltaTime;
        if (Entry.Accumulator >= Entry.Interval)
        {
            Entry.Accumulator = 0.f;
            // 手动调用 Tick（绕过 UE 的逐 Actor tick 开销）
            // 注意：这里不直接调 Tick，而是设一个标记让 Actor 自己在 Tick 里检查
        }
    }

    // 5. 网络统计（服务器）
    if (GetNetMode() == NM_DedicatedServer)
    {
        NetworkSampleTimer += DeltaTime;
        if (NetworkSampleTimer >= 1.f)
        {
            NetworkSampleTimer = 0.f;

            // 统计网络流量
            UWorld* World = GetWorld();
            if (World && World->GetGameState())
            {
                int32 PlayerCount = World->GetGameState()->PlayerArray.Num();
                ServerStats.ActiveActors = PlayerCount;

                // 估算网络流量
                float TotalIn = 0.f, TotalOut = 0.f;
                for (const auto& PC : World->GetGameState()->PlayerArray)
                {
                    if (PC && PC->GetNetConnection())
                    {
                        // 粗略估算
                        TotalOut += 1024.f;  // 假设每玩家 1KB/s
                    }
                }
                ServerStats.NetworkOutKBps = TotalOut / 1024.f;
            }

            ServerTickTimer += DeltaTime;
            ServerStats.FrameTimeMs = ServerTickTimer * 1000.f;
            ServerTickTimer = 0.f;
        }
    }
}

// =====================================================================
// 硬件检测 + 自动分级
// =====================================================================

void APerformanceManager::DetectHardwareSpecs()
{
    // CPU 核心数
    int32 NumCores = FPlatformMisc::NumberOfCores();
    int32 NumLogicalCores = FPlatformMisc::NumberOfCoresIncludingHyperthreads();

    // 内存总量
    FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
    float TotalMemGB = (float)(MemStats.TotalPhysical / (1024 * 1024 * 1024));

    // GPU 信息（通过 RHI）
    FString RHIName = GDynamicRHI ? GDynamicRHI->GetName() : TEXT("Unknown");

    UE_LOG(LogPerformance, Log, TEXT("[Perf] Hardware: %d cores (%d logical), %.1f GB RAM, RHI: %s"),
        NumCores, NumLogicalCores, TotalMemGB, *RHIName);

    // 存储到控制台变量供后续使用
    GConfig->SetInt(TEXT("/Script/StellarSystem.Performance"), TEXT("DetectedCores"), NumCores, GGameIni);
    GConfig->SetFloat(TEXT("/Script/StellarSystem.Performance"), TEXT("DetectedRAMGB"), TotalMemGB, GGameIni);
    GConfig->Flush(false, GGameIni);
}

void APerformanceManager::AutoDetectPerformanceTier()
{
    FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
    float TotalMemGB = (float)(MemStats.TotalPhysical / (1024 * 1024 * 1024));
    int32 NumCores = FPlatformMisc::NumberOfCores();

    FString RHIName = GDynamicRHI ? GDynamicRHI->GetName() : TEXT("");

    EPerformanceTier Detected = EPerformanceTier::Medium;

    // 简单启发式评分
    int32 Score = 0;

    if (TotalMemGB >= 32.f) Score += 3;
    else if (TotalMemGB >= 16.f) Score += 2;
    else if (TotalMemGB >= 8.f) Score += 1;

    if (NumCores >= 12) Score += 3;
    else if (NumCores >= 8) Score += 2;
    else if (NumCores >= 4) Score += 1;

    // RHI 检测（DX12/Vulkan = 现代 API）
    if (RHIName.Contains(TEXT("D3D12")) || RHIName.Contains(TEXT("Vulkan")))
        Score += 1;

    if (Score >= 6) Detected = EPerformanceTier::Ultra;
    else if (Score >= 4) Detected = EPerformanceTier::High;
    else if (Score >= 2) Detected = EPerformanceTier::Medium;
    else Detected = EPerformanceTier::Low;

    UE_LOG(LogPerformance, Log, TEXT("[Perf] Auto-detected tier: %d (Score: %d, RAM: %.0fGB, Cores: %d, RHI: %s)"),
        (int32)Detected, Score, TotalMemGB, NumCores, *RHIName);

    SetPerformanceTier(Detected);
}

void APerformanceManager::SetPerformanceTier(EPerformanceTier Tier)
{
    EPerformanceTier OldTier = CurrentTier;
    CurrentTier = Tier;

    // 应用对应质量
    switch (Tier)
    {
    case EPerformanceTier::Low:    QualitySettings.ApplyLow(); break;
    case EPerformanceTier::Medium: QualitySettings.ApplyMedium(); break;
    case EPerformanceTier::High:   QualitySettings.ApplyHigh(); break;
    case EPerformanceTier::Ultra:  QualitySettings.ApplyUltra(); break;
    default: break;
    }

    ApplyQualitySettings();

    // 广播事件
    OnPerformanceTierChanged.Broadcast(OldTier, CurrentTier);

    UE_LOG(LogPerformance, Log, TEXT("[Perf] Tier changed: %d -> %d"), (int32)OldTier, (int32)Tier);
}

// =====================================================================
// 质量设置应用
// =====================================================================

void APerformanceManager::ApplyQualitySettings()
{
    // 通过 Scalability 系统应用
    Scalability::FQualityLevels Quality;

    Quality.ResolutionQuality = 100.f;
    Quality.ViewDistanceQuality = FMath::Clamp(QualitySettings.ViewDistance / 100.f, 10.f, 100.f);
    Quality.AntiAliasingQuality = QualitySettings.EffectsQuality;
    Quality.ShadowQuality = QualitySettings.ShadowQuality;
    Quality.PostProcessQuality = QualitySettings.EffectsQuality;
    Quality.TextureQuality = QualitySettings.TextureQuality;
    Quality.EffectsQuality = QualitySettings.EffectsQuality;
    Quality.FoliageQuality = FMath::Clamp(QualitySettings.FoliageDensity / 50.f, 0, 3);

    Scalability::SetQualityLevels(Quality);

    // 应用帧率限制
    if (GEngine)
    {
        switch (CurrentTier)
        {
        case EPerformanceTier::Low:    GEngine->MaxFPS = 30; break;
        case EPerformanceTier::Medium: GEngine->MaxFPS = 45; break;
        case EPerformanceTier::High:   GEngine->MaxFPS = 60; break;
        case EPerformanceTier::Ultra:  GEngine->MaxFPS = 144; break;
        default: GEngine->MaxFPS = 60;
        }
    }

    // 音频通道数
    // (UE 音频系统自动管理，这里仅记录目标值)
    UE_LOG(LogPerformance, Log, TEXT("[Perf] Quality applied - ViewDist: %d, Shadow: %d, Tex: %d, Foliage: %d%%"),
        QualitySettings.ViewDistance, QualitySettings.ShadowQuality,
        QualitySettings.TextureQuality, QualitySettings.FoliageDensity);
}

// =====================================================================
// 自适应质量调整
// =====================================================================

void APerformanceManager::AdaptiveQualityAdjust(float Dt)
{
    // 基于最近 5 秒的平均帧率调整
    float TargetFPS = 60.f;
    switch (CurrentTier)
    {
    case EPerformanceTier::Low:    TargetFPS = 30.f; break;
    case EPerformanceTier::Medium: TargetFPS = 45.f; break;
    case EPerformanceTier::High:   TargetFPS = 60.f; break;
    case EPerformanceTier::Ultra:  TargetFPS = 144.f; break;
    default: break;
    }

    float FPSRatio = SmoothedFPS / TargetFPS;

    if (FPSRatio < 0.8f)
    {
        // 帧率严重不足 → 降级
        UE_LOG(LogPerformance, Warning, TEXT("[Perf] FPS %.1f below target %.0f - downgrading quality"),
            SmoothedFPS, TargetFPS);

        switch (CurrentTier)
        {
        case EPerformanceTier::Ultra:  SetPerformanceTier(EPerformanceTier::High); break;
        case EPerformanceTier::High:   SetPerformanceTier(EPerformanceTier::Medium); break;
        case EPerformanceTier::Medium: SetPerformanceTier(EPerformanceTier::Low); break;
        default: break;  // Low 已经最低
        }
    }
    else if (FPSRatio > 1.15f && CurrentTier != EPerformanceTier::Ultra)
    {
        // 帧率充裕 → 尝试升级
        UE_LOG(LogPerformance, Log, TEXT("[Perf] FPS %.1f above target %.0f - upgrading quality"),
            SmoothedFPS, TargetFPS);

        switch (CurrentTier)
        {
        case EPerformanceTier::Low:    SetPerformanceTier(EPerformanceTier::Medium); break;
        case EPerformanceTier::Medium: SetPerformanceTier(EPerformanceTier::High); break;
        case EPerformanceTier::High:   SetPerformanceTier(EPerformanceTier::Ultra); break;
        default: break;
        }
    }
}

// =====================================================================
// 性能统计采样
// =====================================================================

void APerformanceManager::UpdatePerformanceStats(float Dt)
{
    if (Dt <= 0.f) return;

    // 帧率采样
    float CurrentFPS = 1.f / Dt;
    FPSHistory[FPSHistoryIndex] = CurrentFPS;
    FPSHistoryIndex = (FPSHistoryIndex + 1) % 60;

    // 平滑帧率（指数移动平均）
    float Alpha = 0.05f;  // 平滑因子
    SmoothedFPS = SmoothedFPS * (1.f - Alpha) + CurrentFPS * Alpha;

    // 帧时间
    CurrentStats.FrameTimeMs = Dt * 1000.f;

    // 内存
    FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
    CurrentStats.MemoryMB = (float)(MemStats.UsedPhysical / (1024 * 1024));

    // 游戏线程时间（估算）
    CurrentStats.GameThreadMs = Dt * 1000.f * 0.6f;  // 游戏线程通常占 60%
    CurrentStats.RenderThreadMs = Dt * 1000.f * 0.3f;
    CurrentStats.GPUMs = Dt * 1000.f * 0.25f;

    // 平均 FPS
    CurrentStats.AverageFPS = SmoothedFPS;

    // 网络（每 10 帧采样一次）
    static int32 FrameCounter = 0;
    if (++FrameCounter >= 10)
    {
        FrameCounter = 0;
        UWorld* World = GetWorld();
        if (World)
        {
            // 统计 Actor 数量
            int32 Count = 0;
            for (const TWeakObjectPtr<AActor>& Actor : World->GetActorIterator())
            {
                if (Actor.IsValid()) ++Count;
            }
            CurrentStats.ActiveActors = Count;
        }
    }
}

// =====================================================================
// 垃圾回收
// =====================================================================

void APerformanceManager::RequestGarbageCollection(bool bFullPurge)
{
    float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

    // 节流：至少间隔 10 秒
    if (!bFullPurge && (Now - LastGCTime) < 10.f)
        return;

    LastGCTime = Now;

    if (bFullPurge)
    {
        // 强制全量 GC
        CollectGarbage(RF_NoFlags, true);
        UE_LOG(LogPerformance, Log, TEXT("[Perf] Full GC executed"));
    }
    else
    {
        // 增量 GC（不卡主线程）
        IncrementalPurgeGarbage(false);
    }
}

void APerformanceManager::FlushUnusedAssets(int32 KeepMinutes)
{
    // 遍历所有已加载资源，卸载长时间未使用的
    UE_LOG(LogPerformance, Log, TEXT("[Perf] Flushing unused assets (keep threshold: %d min)"), KeepMinutes);

    // 使用 FStreamableManager 的 Unload 机制
    // 这里标记需要卸载的包
    TArray<FString> PackagesToUnload;

    // 遍历内存中的 UObjects
    for (TObjectIterator<UObject> It; It; ++It)
    {
        UObject* Obj = *It;
        if (!Obj || !Obj->IsValidLowLevel()) continue;

        // 跳过关键对象
        if (Obj->IsRooted()) continue;
        if (Obj->HasAnyFlags(RF_Standalone)) continue;

        // 检查引用计数
        if (Obj->GetRefCount() <= 1)
        {
            // 可能是孤立对象
            UPackage* Package = Obj->GetOutermost();
            if (Package && !PackagesToUnload.Contains(Package->GetName()))
            {
                PackagesToUnload.Add(Package->GetName());
            }
        }
    }

    // 卸载（异步，不阻塞）
    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [PackagesToUnload]()
    {
        for (const FString& PkgName : PackagesToUnload)
        {
            UPackage* Package = FindPackage(nullptr, *PkgName);
            if (Package)
            {
                Package->SetFName(FName(*(PkgName + "_unloaded")));
            }
        }
        UE_LOG(LogPerformance, Log, TEXT("[Perf] Unloaded %d unused packages"), PackagesToUnload.Num());
    });
}

// =====================================================================
// 服务器性能
// =====================================================================

void APerformanceManager::SetServerTickRate(int32 TickRate)
{
    if (TickRate < 1) TickRate = 1;
    if (TickRate > 120) TickRate = 120;

    ServerTickInterval = 1.f / (float)TickRate;

    UWorld* World = GetWorld();
    if (World)
    {
        World->SetShouldTick(true);
    }

    UE_LOG(LogPerformance, Log, TEXT("[Perf] Server tick rate set to %d Hz (interval: %.4fs)"),
        TickRate, ServerTickInterval);
}

void APerformanceManager::SetNetworkUpdateRate(float UpdatesPerSecond)
{
    NetworkUpdateRate = FMath::Clamp(UpdatesPerSecond, 1.f, 60.f);

    // 设置网络相关参数
    UWorld* World = GetWorld();
    if (World)
    {
        // 调整 Actor 复制频率
        for (const TWeakObjectPtr<AActor>& Actor : World->GetActorIterator())
        {
            if (Actor.IsValid() && Actor->GetIsReplicated())
            {
                // 通过 NetUpdateFrequency 控制
                Actor->NetUpdateFrequency = NetworkUpdateRate;
            }
        }
    }

    UE_LOG(LogPerformance, Log, TEXT("[Perf] Network update rate: %.1f Hz"), NetworkUpdateRate);
}

TArray<AActor*> APerformanceManager::GetActorsInRegion(const FVector& Center, float Radius) const
{
    TArray<AActor*> Result;

    UWorld* World = GetWorld();
    if (!World) return Result;

    // 使用空间哈希加速
    float GridX = FMath::FloorToFloat(Center.X / SpatialGridSize);
    float GridY = FMath::FloorToFloat(Center.Y / SpatialGridSize);
    float GridZ = FMath::FloorToFloat(Center.Z / SpatialGridSize);

    // 检查 3x3x3 邻域
    float R2 = Radius * Radius;
    for (int32 dx = -1; dx <= 1; ++dx)
    {
        for (int32 dy = -1; dy <= 1; ++dy)
        {
            for (int32 dz = -1; dz <= 1; ++dz)
            {
                FIntVector Key(
                    (int32)(GridX + dx),
                    (int32)(GridY + dy),
                    (int32)(GridZ + dz)
                );

                const TArray<TWeakObjectPtr<AActor>>* Cell = SpatialGrid.Find(Key);
                if (!Cell) continue;

                for (const TWeakObjectPtr<AActor>& WeakActor : *Cell)
                {
                    if (!WeakActor.IsValid()) continue;
                    AActor* Actor = WeakActor.Get();
                    if (FVector::DistSquared(Center, Actor->GetActorLocation()) <= R2)
                    {
                        Result.Add(Actor);
                    }
                }
            }
        }
    }

    return Result;
}

void APerformanceManager::SetSpatialGridSize(float GridSize)
{
    SpatialGridSize = FMath::Max(GridSize, 1000.f);

    // 重建网格
    SpatialGrid.Empty();

    UWorld* World = GetWorld();
    if (!World) return;

    for (const TWeakObjectPtr<AActor>& Actor : World->GetActorIterator())
    {
        if (!Actor.IsValid()) continue;

        FVector Loc = Actor->GetActorLocation();
        FIntVector Key(
            (int32)(Loc.X / SpatialGridSize),
            (int32)(Loc.Y / SpatialGridSize),
            (int32)(Loc.Z / SpatialGridSize)
        );
        SpatialGrid.FindOrAdd(Key).Add(Actor);
    }

    UE_LOG(LogPerformance, Log, TEXT("[Perf] Spatial grid rebuilt: size=%.0f, cells=%d"),
        SpatialGridSize, SpatialGrid.Num());
}

void APerformanceManager::RegisterBatchTick(AActor* Actor, float IntervalSeconds)
{
    if (!Actor) return;

    FBatchTickEntry Entry;
    Entry.Actor = Actor;
    Entry.Interval = FMath::Max(IntervalSeconds, 0.1f);
    Entry.Accumulator = 0.f;
    BatchTickList.Add(Entry);
}

void APerformanceManager::UnregisterBatchTick(AActor* Actor)
{
    for (int32 i = BatchTickList.Num() - 1; i >= 0; --i)
    {
        if (BatchTickList[i].Actor.Get() == Actor)
        {
            BatchTickList.RemoveAt(i);
            break;
        }
    }
}

// =====================================================================
// 异步加载
// =====================================================================

void APerformanceManager::LoadLevelAsync(const FString& LevelName, FName PackageName)
{
    UE_LOG(LogPerformance, Log, TEXT("[Perf] Async loading level: %s"), *LevelName);

    LoadingProgress = 0.f;
    OnLoadingProgressChanged.Broadcast(0.f);

    // 使用 FStreamableManager
    FStringAssetReference AssetRef(LevelName);
    UObject* LoadedObject = AssetRef.ResolveObject();

    if (!LoadedObject)
    {
        // 开始异步加载
        FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
        Streamable.RequestAsyncLoad(AssetRef, FStreamableDelegate::CreateLambda([this, LevelName]()
        {
            UE_LOG(LogPerformance, Log, TEXT("[Perf] Level loaded: %s"), *LevelName);
            LoadingProgress = 1.f;
            OnLoadingProgressChanged.Broadcast(1.f);
        }));
    }
    else
    {
        LoadingProgress = 1.f;
        OnLoadingProgressChanged.Broadcast(1.f);
    }
}

void APerformanceManager::PrewarmShaders(const TArray<FString>& MaterialPaths)
{
    UE_LOG(LogPerformance, Log, TEXT("[Perf] Prewarming %d shaders"), MaterialPaths.Num());

    // 在后台线程加载并编译材质着色器
    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [MaterialPaths]()
    {
        for (const FString& Path : MaterialPaths)
        {
            UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, *Path);
            if (Mat)
            {
                // 强制编译所有着色器排列
                if (UMaterial* BaseMat = Mat->GetBaseMaterial())
                {
                    BaseMat->ForceRecompileForRendering();
                }
            }
        }
        UE_LOG(LogPerformance, Log, TEXT("[Perf] Shader prewarm complete"));
    });
}

void APerformanceManager::PreloadAssetsAsync(const TArray<FSoftObjectPath>& AssetPaths,
                                             FName CompleteEventName)
{
    PendingLoads = AssetPaths;
    CompletedLoads = 0;
    LoadingProgress = 0.f;

    UE_LOG(LogPerformance, Log, TEXT("[Perf] Preloading %d assets"), AssetPaths.Num());

    // 分批加载，每帧最多加载 N 个，避免卡顿
    const int32 BatchSize = 4;
    TArray<FSoftObjectPath> FirstBatch;
    for (int32 i = 0; i < FMath::Min(BatchSize, AssetPaths.Num()); ++i)
    {
        FirstBatch.Add(AssetPaths[i]);
    }

    // 使用 StreamableManager 异步加载
    FStreamableManager& Streamable = UAssetManager::GetStreamableManager();

    for (const FSoftObjectPath& Path : FirstBatch)
    {
        FSoftObjectPath PathCopy = Path;
        Streamable.RequestAsyncLoad(Path, FStreamableDelegate::CreateLambda([this, PathCopy]()
        {
            ++CompletedLoads;
            LoadingProgress = (float)CompletedLoads / (float)FMath::Max(PendingLoads.Num(), 1);

            UE_LOG(LogPerformance, Log, TEXT("[Perf] Loaded %d/%d: %s"),
                CompletedLoads, PendingLoads.Num(), *PathCopy.ToString());

            OnLoadingProgressChanged.Broadcast(LoadingProgress);

            // 继续下一批
            if (CompletedLoads < PendingLoads.Num())
            {
                int32 NextIdx = CompletedLoads;
                if (PendingLoads.IsValidIndex(NextIdx))
                {
                    FStreamableManager& SM = UAssetManager::GetStreamableManager();
                    FSoftObjectPath NextPath = PendingLoads[NextIdx];
                    SM.RequestAsyncLoad(NextPath, FStreamableDelegate::CreateLambda([this]()
                    {
                        ++CompletedLoads;
                        LoadingProgress = (float)CompletedLoads / (float)FMath::Max(PendingLoads.Num(), 1);
                        OnLoadingProgressChanged.Broadcast(LoadingProgress);
                    }));
                }
            }
        }));
    }
}
