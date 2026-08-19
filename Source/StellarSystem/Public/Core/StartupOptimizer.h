// StartupOptimizer.h
// 启动/加载速度优化 + 稳定性增强
// v6.6 性能优化

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "StartupOptimizer.generated.h"

// 启动阶段
UENUM(BlueprintType)
enum class EStartupPhase : uint8
{
    EngineInit    UMETA(DisplayName = "Engine Init"),
    ModuleLoad    UMETA(DisplayName = "Module Load"),
    AssetPreload  UMETA(DisplayName = "Asset Preload"),
    WorldBuild    UMETA(DisplayName = "World Build"),
    SubsystemInit UMETA(DisplayName = "Subsystem Init"),
    FirstFrame    UMETA(DisplayName = "First Frame"),
    Ready         UMETA(DisplayName = "Ready")
};

// 启动性能数据
USTRUCT(BlueprintType)
struct FStartupTiming
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    float EngineInitMs = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float ModuleLoadMs = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float AssetPreloadMs = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float WorldBuildMs = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float SubsystemInitMs = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float FirstFrameMs = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float TotalStartupMs = 0.f;

    UPROPERTY(BlueprintReadOnly)
    int32 PreloadedAssets = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 DeferredAssets = 0;
};

// 稳定性监控
USTRUCT(BlueprintType)
struct FStabilityMetrics
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    float UptimeSeconds = 0.f;

    UPROPERTY(BlueprintReadOnly)
    int32 CrashCount = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 WarningCount = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 OutOfMemoryCount = 0;

    UPROPERTY(BlueprintReadOnly)
    float MinFPS = 999.f;

    UPROPERTY(BlueprintReadOnly)
    float MaxFrameTimeMs = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float AverageFPS = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float MemoryPeakMB = 0.f;

    void Reset()
    {
        UptimeSeconds = 0.f;
        CrashCount = 0;
        WarningCount = 0;
        OutOfMemoryCount = 0;
        MinFPS = 999.f;
        MaxFrameTimeMs = 0.f;
        AverageFPS = 0.f;
        MemoryPeakMB = 0.f;
    }
};

// 启动优化器
UCLASS(BlueprintType)
class STELLARSYSTEM_API AStartupOptimizer : public AActor
{
    GENERATED_BODY()

public:
    AStartupOptimizer();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // 获取启动计时
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Startup")
    FStartupTiming GetStartupTiming() const { return StartupTiming; }

    // 获取当前阶段
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Startup")
    EStartupPhase GetCurrentPhase() const { return CurrentPhase; }

    // 获取启动进度（0~1）
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Startup")
    float GetStartupProgress() const;

    // 注册延迟初始化项
    UFUNCTION(BlueprintCallable, Category = "Startup")
    void RegisterDeferredInit(FName InitName, FSimpleDelegate InitFunc,
                               int32 Priority = 100);

    // 强制完成所有延迟初始化
    UFUNCTION(BlueprintCallable, Category = "Startup")
    void FlushAllDeferredInits();

    // 跳过剩余延迟初始化（进游戏后再补）
    UFUNCTION(BlueprintCallable, Category = "Startup")
    void SkipRemainingInits();

    // ---- 稳定性 ----

    // 获取稳定性指标
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Stability")
    FStabilityMetrics GetStabilityMetrics() const { return StabilityData; }

    // 报告错误
    UFUNCTION(BlueprintCallable, Category = "Stability")
    void ReportError(const FString& ErrorMessage, bool bCritical = false);

    // 报告内存警告
    UFUNCTION(BlueprintCallable, Category = "Stability")
    void ReportMemoryWarning(float MemoryMB);

    // 安全退出（保存+清理）
    UFUNCTION(BlueprintCallable, Category = "Stability")
    void SafeShutdown(const FString& Reason);

    // 设置自动崩溃恢复
    UFUNCTION(BlueprintCallable, Category = "Stability")
    void EnableCrashRecovery(bool bEnable);

    // 获取上次崩溃信息
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Stability")
    FString GetLastCrashInfo() const { return LastCrashInfo; }

    // 设置心跳超时
    UFUNCTION(BlueprintCallable, Category = "Stability")
    void SetHeartbeatTimeout(float TimeoutSeconds);

    // 委托
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStartupPhaseChanged,
        EStartupPhase, NewPhase);
    UPROPERTY(BlueprintAssignable, Category = "Startup")
    FOnStartupPhaseChanged OnStartupPhaseChanged;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStabilityWarning,
        FString, WarningMessage);
    UPROPERTY(BlueprintAssignable, Category = "Stability")
    FOnStabilityWarning OnStabilityWarning;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCrashDetected,
        FString, CrashInfo);
    UPROPERTY(BlueprintAssignable, Category = "Stability")
    FOnCrashDetected OnCrashDetected;

    // 配置
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Startup")
    bool bEnableParallelLoading = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Startup")
    int32 MaxParallelLoadThreads = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Startup")
    bool bDeferSubsystemInit = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Startup")
    float MinLoadingScreenTime = 2.f;  // 最少显示 2 秒

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stability")
    bool bEnableHeartbeat = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stability")
    float HeartbeatInterval = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stability")
    int32 MaxConsecutiveWarnings = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stability")
    float MemoryCriticalThresholdMB = 3584.f;  // 3.5GB

private:
    // 启动计时
    double StartupBeginTime = 0.0;
    double PhaseBeginTime = 0.0;
    FStartupTiming StartupTiming;
    EStartupPhase CurrentPhase = EStartupPhase::EngineInit;
    float StartupProgress = 0.f;

    // 延迟初始化队列
    struct FDeferredInit
    {
        FName Name;
        FSimpleDelegate Func;
        int32 Priority;
        bool bExecuted = false;
    };
    TArray<FDeferredInit> DeferredInits;
    int32 DeferredInitIndex = 0;
    float DeferredTimer = 0.f;
    static constexpr float DEFERRED_BATCH_TIME = 0.008f;  // 每帧 8ms 预算

    // 稳定性
    FStabilityMetrics StabilityData;
    float UptimeAccumulator = 0.f;
    float FrameTimeAccumulator = 0.f;
    int32 FrameCount = 0;
    float FPSAccumulator = 0.f;
    FString LastCrashInfo;
    bool bCrashRecoveryEnabled = true;
    int32 ConsecutiveWarnings = 0;

    // 心跳
    float HeartbeatTimer = 0.f;
    float HeartbeatTimeout = 10.f;
    double LastHeartbeatTime = 0.0;

    // 阶段转换
    void TransitionToPhase(EStartupPhase NewPhase);
    void ExecuteDeferredInits(float TimeBudget);

    // 崩溃恢复
    void SaveCrashDump(const FString& Reason);
    void RestoreFromLastSafeState();
};
