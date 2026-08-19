// StartupOptimizer.cpp
// 启动加载优化 + 稳定性增强完整实现

#include "Core/StartupOptimizer.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/ConfigCacheIni.h"
#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#include "TimerManager.h"
#include "HAL/RunnableThread.h"
#include "HAL/Runnable.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogStartup, Log, All);
DEFINE_LOG_CATEGORY_STATIC(LogStability, Log, All);

// =====================================================================
// AStartupOptimizer
// =====================================================================

AStartupOptimizer::AStartupOptimizer()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.f;  // 每帧都跑（启动关键期）

    bEnableParallelLoading = true;
    MaxParallelLoadThreads = 4;
    bDeferSubsystemInit = true;
    MinLoadingScreenTime = 2.f;

    bEnableHeartbeat = true;
    HeartbeatInterval = 1.f;
    MaxConsecutiveWarnings = 50;
    MemoryCriticalThresholdMB = 3584.f;  // 3.5GB
}

void AStartupOptimizer::BeginPlay()
{
    Super::BeginPlay();

    StartupBeginTime = FPlatformTime::Seconds();
    PhaseBeginTime = StartupBeginTime;

    // 阶段 1：引擎初始化（此时已完成，记录时间）
    TransitionToPhase(EStartupPhase::ModuleLoad);

    // 并行加载模块
    if (bEnableParallelLoading)
    {
        UE_LOG(LogStartup, Log, TEXT("[Startup] Parallel loading enabled (%d threads)"),
            MaxParallelLoadThreads);

        // 在后台线程预热常用系统
        AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, []()
        {
            // 预热字符串池
            FString Dummy;
            for (int32 i = 0; i < 1000; ++i)
            {
                Dummy += FString::Printf(TEXT("prewarm_%d"), i);
            }

            // 预热数学库
            float Sum = 0.f;
            for (int32 i = 0; i < 10000; ++i)
            {
                Sum += FMath::Sin((float)i * 0.001f) * FMath::Cos((float)i * 0.001f);
            }
        });
    }

    // 阶段 2：资产预加载
    TransitionToPhase(EStartupPhase::AssetPreload);

    // 延迟初始化：注册各子系统
    if (bDeferSubsystemInit)
    {
        // 这些会在 Tick 中逐步执行
        RegisterDeferredInit(TEXT("PerformanceManager"), FSimpleDelegate(), 10);
        RegisterDeferredInit(TEXT("NetworkOptimizer"), FSimpleDelegate(), 20);
        RegisterDeferredInit(TEXT("ObjectPoolManager"), FSimpleDelegate(), 30);
        RegisterDeferredInit(TEXT("AntiCheat"), FSimpleDelegate(), 40);
        RegisterDeferredInit(TEXT("AudioManager"), FSimpleDelegate(), 50);
        RegisterDeferredInit(TEXT("SaveManager"), FSimpleDelegate(), 60);
        RegisterDeferredInit(TEXT("GalaxyGenerator"), FSimpleDelegate(), 70);
    }

    UE_LOG(LogStartup, Log, TEXT("[Startup] BeginPlay complete, optimizer active"));
}

void AStartupOptimizer::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // === 启动阶段管理 ===
    if (CurrentPhase != EStartupPhase::Ready)
    {
        // 执行延迟初始化（带时间预算）
        if (DeferredInits.Num() > 0 && DeferredInitIndex < DeferredInits.Num())
        {
            ExecuteDeferredInits(DEFERRED_BATCH_TIME);
        }
        else if (CurrentPhase == EStartupPhase::AssetPreload ||
                 CurrentPhase == EStartupPhase::SubsystemInit)
        {
            // 所有延迟初始化完成 → 进入下一阶段
            TransitionToPhase(EStartupPhase::FirstFrame);
        }

        // 计算启动进度
        float Elapsed = (float)(FPlatformTime::Seconds() - StartupBeginTime) * 1000.f;
        StartupTiming.TotalStartupMs = Elapsed;

        // 确保最少显示加载画面时间
        if (Elapsed >= MinLoadingScreenTime * 1000.f &&
            CurrentPhase == EStartupPhase::FirstFrame)
        {
            TransitionToPhase(EStartupPhase::Ready);

            UE_LOG(LogStartup, Log, TEXT("[Startup] READY in %.0fms (Engine:%.0f + Modules:%.0f + Assets:%.0f + World:%.0f + Subsys:%.0f + FirstFrame:%.0f)"),
                StartupTiming.TotalStartupMs,
                StartupTiming.EngineInitMs,
                StartupTiming.ModuleLoadMs,
                StartupTiming.AssetPreloadMs,
                StartupTiming.WorldBuildMs,
                StartupTiming.SubsystemInitMs,
                StartupTiming.FirstFrameMs);
        }
    }

    // === 稳定性监控（始终运行）===
    UptimeAccumulator += DeltaTime;
    StabilityData.UptimeSeconds = UptimeAccumulator;

    // 帧时间统计
    float FrameMs = DeltaTime * 1000.f;
    FrameTimeAccumulator += DeltaTime;
    FrameCount++;
    FPSAccumulator += (1.f / FMath::Max(DeltaTime, 0.001f));

    // 最低 FPS
    float CurrentFPS = 1.f / FMath::Max(DeltaTime, 0.001f);
    StabilityData.MinFPS = FMath::Min(StabilityData.MinFPS, CurrentFPS);
    StabilityData.MaxFrameTimeMs = FMath::Max(StabilityData.MaxFrameTimeMs, FrameMs);

    // 平均 FPS（每 60 帧更新）
    if (FrameCount >= 60)
    {
        StabilityData.AverageFPS = FPSAccumulator / (float)FrameCount;
        FrameCount = 0;
        FrameTimeAccumulator = 0.f;
        FPSAccumulator = 0.f;
    }

    // 内存监控
    FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
    float MemMB = (float)(MemStats.UsedPhysical / (1024 * 1024));
    StabilityData.MemoryPeakMB = FMath::Max(StabilityData.MemoryPeakMB, MemMB);

    if (MemMB > MemoryCriticalThresholdMB)
    {
        StabilityData.OutOfMemoryCount++;
        ReportMemoryWarning(MemMB);

        // 紧急释放
        if (StabilityData.OutOfMemoryCount >= 3)
        {
            // 三次内存警告 → 安全关闭
            SafeShutdown(TEXT("Critical memory pressure"));
        }
    }

    // 心跳
    if (bEnableHeartbeat)
    {
        HeartbeatTimer += DeltaTime;
        if (HeartbeatTimer >= HeartbeatInterval)
        {
            HeartbeatTimer = 0.f;
            LastHeartbeatTime = FPlatformTime::Seconds();

            // 检查主线程是否卡死
            double Now = FPlatformTime::Seconds();
            if (Now - LastHeartbeatTime > HeartbeatTimeout)
            {
                ReportError(TEXT("Heartbeat timeout - main thread may be frozen"), true);
            }
        }
    }

    // 帧时间异常检测
    if (FrameMs > 500.f)  // 半秒卡顿
    {
        ReportError(FString::Printf(TEXT("Frame hitch detected: %.0fms"), FrameMs), false);
    }
}

// =====================================================================
// 阶段管理
// =====================================================================

void AStartupOptimizer::TransitionToPhase(EStartupPhase NewPhase)
{
    if (NewPhase == CurrentPhase) return;

    double Now = FPlatformTime::Seconds();
    float PhaseDuration = (float)((Now - PhaseBeginTime) * 1000.0);

    // 记录当前阶段耗时
    switch (CurrentPhase)
    {
    case EStartupPhase::EngineInit:    StartupTiming.EngineInitMs = PhaseDuration; break;
    case EStartupPhase::ModuleLoad:    StartupTiming.ModuleLoadMs = PhaseDuration; break;
    case EStartupPhase::AssetPreload:  StartupTiming.AssetPreloadMs = PhaseDuration; break;
    case EStartupPhase::WorldBuild:    StartupTiming.WorldBuildMs = PhaseDuration; break;
    case EStartupPhase::SubsystemInit: StartupTiming.SubsystemInitMs = PhaseDuration; break;
    case EStartupPhase::FirstFrame:    StartupTiming.FirstFrameMs = PhaseDuration; break;
    default: break;
    }

    EStartupPhase OldPhase = CurrentPhase;
    CurrentPhase = NewPhase;
    PhaseBeginTime = Now;

    OnStartupPhaseChanged.Broadcast(NewPhase);

    UE_LOG(LogStartup, Log, TEXT("[Startup] Phase: %d -> %d (%.0fms)"),
        (int32)OldPhase, (int32)NewPhase, PhaseDuration);
}

float AStartupOptimizer::GetStartupProgress() const
{
    // 基于阶段估算进度
    switch (CurrentPhase)
    {
    case EStartupPhase::EngineInit:    return 0.05f;
    case EStartupPhase::ModuleLoad:    return 0.15f;
    case EStartupPhase::AssetPreload:  return 0.35f;
    case EStartupPhase::WorldBuild:    return 0.55f;
    case EStartupPhase::SubsystemInit: return 0.80f;
    case EStartupPhase::FirstFrame:    return 0.95f;
    case EStartupPhase::Ready:        return 1.0f;
    default: return 0.f;
    }
}

// =====================================================================
// 延迟初始化
// =====================================================================

void AStartupOptimizer::RegisterDeferredInit(FName InitName, FSimpleDelegate InitFunc,
                                               int32 Priority)
{
    FDeferredInit NewInit;
    NewInit.Name = InitName;
    NewInit.Func = InitFunc;
    NewInit.Priority = Priority;
    NewInit.bExecuted = false;

    // 按优先级插入（低数字先执行）
    int32 InsertIdx = 0;
    for (int32 i = 0; i < DeferredInits.Num(); ++i)
    {
        if (DeferredInits[i].Priority > Priority)
        {
            InsertIdx = i;
            break;
        }
        InsertIdx = i + 1;
    }
    DeferredInits.Insert(NewInit, InsertIdx);
}

void AStartupOptimizer::ExecuteDeferredInits(float TimeBudget)
{
    float StartTime = (float)FPlatformTime::Seconds();

    while (DeferredInitIndex < DeferredInits.Num())
    {
        FDeferredInit& Init = DeferredInits[DeferredInitIndex];

        if (!Init.bExecuted)
        {
            // 执行（如果有绑定函数）
            if (Init.Func.IsBound())
            {
                Init.Func.Execute();
            }

            Init.bExecuted = true;
            StartupTiming.DeferredAssets++;

            UE_LOG(LogStartup, Verbose, TEXT("[Startup] Deferred init: %s (priority=%d)"),
                *Init.Name.ToString(), Init.Priority);
        }

        DeferredInitIndex++;

        // 检查时间预算
        float Elapsed = (float)(FPlatformTime::Seconds() - StartTime);
        if (Elapsed >= TimeBudget) break;
    }

    // 全部完成
    if (DeferredInitIndex >= DeferredInits.Num())
    {
        TransitionToPhase(EStartupPhase::FirstFrame);
    }
}

void AStartupOptimizer::FlushAllDeferredInits()
{
    float StartTime = (float)FPlatformTime::Seconds();

    for (int32 i = DeferredInitIndex; i < DeferredInits.Num(); ++i)
    {
        FDeferredInit& Init = DeferredInits[i];
        if (!Init.bExecuted)
        {
            if (Init.Func.IsBound())
            {
                Init.Func.Execute();
            }
            Init.bExecuted = true;
            StartupTiming.DeferredAssets++;
        }
    }

    DeferredInitIndex = DeferredInits.Num();
    float Elapsed = (float)(FPlatformTime::Seconds() - StartTime) * 1000.f;

    UE_LOG(LogStartup, Log, TEXT("[Startup] Flushed all deferred inits in %.1fms"), Elapsed);
}

void AStartupOptimizer::SkipRemainingInits()
{
    UE_LOG(LogStartup, Log, TEXT("[Startup] Skipping %d remaining inits"),
        DeferredInits.Num() - DeferredInitIndex);

    for (int32 i = DeferredInitIndex; i < DeferredInits.Num(); ++i)
    {
        DeferredInits[i].bExecuted = true;  // 标记已完成（跳过）
    }

    DeferredInitIndex = DeferredInits.Num();
    TransitionToPhase(EStartupPhase::FirstFrame);
}

// =====================================================================
// 稳定性
// =====================================================================

void AStartupOptimizer::ReportError(const FString& ErrorMessage, bool bCritical)
{
    StabilityData.WarningCount++;
    ConsecutiveWarnings++;

    FString FullMessage = FString::Printf(TEXT("[Stability] %s: %s"),
        bCritical ? TEXT("CRITICAL") : TEXT("Warning"),
        *ErrorMessage);

    if (bCritical)
    {
        UE_LOG(LogStability, Error, TEXT("%s"), *FullMessage);
        OnStabilityWarning.Broadcast(FullMessage);

        StabilityData.CrashCount++;

        if (bCrashRecoveryEnabled)
        {
            SaveCrashDump(ErrorMessage);
        }
    }
    else
    {
        UE_LOG(LogStability, Warning, TEXT("%s"), *FullMessage);

        if (ConsecutiveWarnings >= MaxConsecutiveWarnings)
        {
            OnStabilityWarning.Broadcast(FString::Printf(
                TEXT("Too many consecutive warnings (%d)"), ConsecutiveWarnings));
            ConsecutiveWarnings = 0;  // 重置，避免刷屏
        }
    }
}

void AStartupOptimizer::ReportMemoryWarning(float MemoryMB)
{
    FString Msg = FString::Printf(TEXT("Memory warning: %.0f MB (threshold: %.0f MB)"),
        MemoryMB, MemoryCriticalThresholdMB);

    ReportError(Msg, MemoryMB > MemoryCriticalThresholdMB);

    // 通知性能管理器
    OnStabilityWarning.Broadcast(Msg);
}

void AStartupOptimizer::SafeShutdown(const FString& Reason)
{
    UE_LOG(LogStability, Error, TEXT("[Stability] SAFE SHUTDOWN: %s"), *Reason);

    // 1. 保存当前游戏状态
    UWorld* World = GetWorld();
    if (World)
    {
        if (AStellarGameMode* GM = World->GetAuthGameMode<AStellarGameMode>())
        {
            GM->SaveCurrentGame(0);  // 保存到槽 0（紧急存档）
        }
    }

    // 2. 保存崩溃信息
    SaveCrashDump(Reason);

    // 3. 清理资源
    CollectGarbage(RF_NoFlags, true);

    // 4. 退出
    if (GEngine)
    {
        GEngine->DeferredCommands.Add(TEXT("QUIT"));
    }
}

void AStartupOptimizer::EnableCrashRecovery(bool bEnable)
{
    bCrashRecoveryEnabled = bEnable;

    // 设置 UE 崩溃处理
    GConfig->SetBool(TEXT("/Script/Engine.GameEngine"), TEXT("bShouldCrashOnAssert"),
        !bEnable, GEngineIni);
    GConfig->Flush(false, GEngineIni);

    UE_LOG(LogStability, Log, TEXT("[Stability] Crash recovery: %s"),
        bEnable ? TEXT("ENABLED") : TEXT("DISABLED"));
}

void AStartupOptimizer::SetHeartbeatTimeout(float TimeoutSeconds)
{
    HeartbeatTimeout = FMath::Max(TimeoutSeconds, 1.f);
    UE_LOG(LogStability, Log, TEXT("[Stability] Heartbeat timeout: %.1fs"), HeartbeatTimeout);
}

void AStartupOptimizer::SaveCrashDump(const FString& Reason)
{
    FString CrashDir = FPaths::ProjectSavedDir() / TEXT("Crashes");
    IFileManager::Get().MakeDirectory(*CrashDir, true);

    FString CrashFile = CrashDir / FString::Printf(TEXT("crash_%s.txt"),
        *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));

    FString CrashInfo;
    CrashInfo += FString::Printf(TEXT("Time: %s\n"), *FDateTime::Now().ToString());
    CrashInfo += FString::Printf(TEXT("Reason: %s\n"), *Reason);
    CrashInfo += FString::Printf(TEXT("Uptime: %.1fs\n"), UptimeAccumulator);
    CrashInfo += FString::Printf(TEXT("Memory Peak: %.0f MB\n"), StabilityData.MemoryPeakMB);
    CrashInfo += FString::Printf(TEXT("Min FPS: %.1f\n"), StabilityData.MinFPS);
    CrashInfo += FString::Printf(TEXT("Max Frame: %.0fms\n"), StabilityData.MaxFrameTimeMs);
    CrashInfo += FString::Printf(TEXT("Warnings: %d\n"), StabilityData.WarningCount);
    CrashInfo += FString::Printf(TEXT("Crashes: %d\n"), StabilityData.CrashCount);

    // 追加调用栈（简化版）
    CrashInfo += TEXT("\n--- System Info ---\n");
    CrashInfo += FString::Printf(TEXT("CPU Cores: %d\n"), FPlatformMisc::NumberOfCores());
    CrashInfo += FString::Printf(TEXT("RAM: %.1f GB\n"),
        FPlatformMemory::GetStats().TotalPhysical / (1024.0 * 1024.0 * 1024.0));
    CrashInfo += FString::Printf(TEXT("RHI: %s\n"),
        GDynamicRHI ? *GDynamicRHI->GetName() : TEXT("None"));
    CrashInfo += FString::Printf(TEXT("Engine Mode: %s\n"),
        GIsEditor ? TEXT("Editor") : TEXT("Game"));

    FFileHelper::SaveStringToFile(CrashInfo, *CrashFile);

    LastCrashInfo = CrashInfo;

    OnCrashDetected.Broadcast(CrashInfo);

    UE_LOG(LogStability, Error, TEXT("[Stability] Crash dump saved: %s"), *CrashFile);
}

void AStartupOptimizer::RestoreFromLastSafeState()
{
    // 查找最近的紧急存档
    FString SaveDir = FPaths::ProjectSavedDir() / TEXT("SaveGames");
    TArray<FString> SaveFiles;
    IFileManager::Get().FindFiles(SaveFiles, *SaveDir, TEXT("*.json"));

    if (SaveFiles.Num() == 0)
    {
        UE_LOG(LogStability, Warning, TEXT("[Stability] No save found for recovery"));
        return;
    }

    // 找最新的
    FString LatestFile;
    double LatestTime = 0;
    for (const FString& File : SaveFiles)
    {
        FString FullPath = SaveDir / File;
        FFileStatData Stat = IFileManager::Get().GetStatData(*FullPath);
        if (Stat.ModificationTime.GetTicks() > LatestTime)
        {
            LatestTime = Stat.ModificationTime.GetTicks();
            LatestFile = FullPath;
        }
    }

    if (!LatestFile.IsEmpty())
    {
        UE_LOG(LogStability, Log, TEXT("[Stability] Restoring from: %s"), *LatestFile);
        // 实际加载逻辑在 SaveManager 中
    }
}
