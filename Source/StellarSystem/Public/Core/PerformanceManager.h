// PerformanceManager.h
// 全局性能管理器：客户端+服务器统一性能调控
// v6.6 性能优化核心

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "PerformanceManager.generated.h"

// 性能等级（自动检测硬件后设定）
UENUM(BlueprintType)
enum class EPerformanceTier : uint8
{
    Low      UMETA(DisplayName = "Low (Potato)"),
    Medium   UMETA(DisplayName = "Medium"),
    High     UMETA(DisplayName = "High"),
    Ultra    UMETA(DisplayName = "Ultra"),
    Auto     UMETA(DisplayName = "Auto-Detect")
};

// 性能统计数据
USTRUCT(BlueprintType)
struct FPerformanceStats
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    float AverageFPS = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float FrameTimeMs = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float GameThreadMs = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float RenderThreadMs = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float GPUMs = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float MemoryMB = 0.f;

    UPROPERTY(BlueprintReadOnly)
    int32 DrawCalls = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 Triangles = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 ActiveActors = 0;

    UPROPERTY(BlueprintReadOnly)
    float NetworkInKBps = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float NetworkOutKBps = 0.f;

    void Reset()
    {
        AverageFPS = 0.f;
        FrameTimeMs = 0.f;
        GameThreadMs = 0.f;
        RenderThreadMs = 0.f;
        GPUMs = 0.f;
        MemoryMB = 0.f;
        DrawCalls = 0;
        Triangles = 0;
        ActiveActors = 0;
        NetworkInKBps = 0.f;
        NetworkOutKBps = 0.f;
    }
};

// 性能预算配置（每帧允许的时间预算）
USTRUCT(BlueprintType)
struct FPerformanceBudget
{
    GENERATED_BODY()

    // 游戏线程总预算（毫秒）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float GameThreadBudgetMs = 16.0f;  // 60fps

    // 渲染线程预算
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RenderThreadBudgetMs = 8.0f;

    // 网络更新预算
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float NetworkBudgetKBps = 256.0f;

    // 物理预算
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PhysicsBudgetMs = 4.0f;

    // AI 预算
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AIBudgetMs = 3.0f;

    // 垃圾回收预算
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float GCBudgetMs = 2.0f;
};

// 自适应质量设置
USTRUCT(BlueprintType)
struct FAdaptiveQualitySettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ViewDistance = 10000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ShadowQuality = 2;  // 0-3

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 TextureQuality = 2;  // 0-3

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 EffectsQuality = 2;  // 0-3

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 FoliageDensity = 100;  // 百分比

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxDrawCalls = 2000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 PlanetLODResolution = 64;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FoliageCullDistance = 5000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxConcurrentNebulae = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 AudioChannels = 32;

    void ApplyLow()
    {
        ViewDistance = 3000;
        ShadowQuality = 0;
        TextureQuality = 0;
        EffectsQuality = 0;
        FoliageDensity = 30;
        MaxDrawCalls = 500;
        PlanetLODResolution = 32;
        FoliageCullDistance = 2000.f;
        MaxConcurrentNebulae = 1;
        AudioChannels = 16;
    }

    void ApplyMedium()
    {
        ViewDistance = 6000;
        ShadowQuality = 1;
        TextureQuality = 1;
        EffectsQuality = 1;
        FoliageDensity = 60;
        MaxDrawCalls = 1000;
        PlanetLODResolution = 48;
        FoliageCullDistance = 3500.f;
        MaxConcurrentNebulae = 2;
        AudioChannels = 24;
    }

    void ApplyHigh()
    {
        ViewDistance = 10000;
        ShadowQuality = 2;
        TextureQuality = 2;
        EffectsQuality = 2;
        FoliageDensity = 100;
        MaxDrawCalls = 2000;
        PlanetLODResolution = 64;
        FoliageCullDistance = 5000.f;
        MaxConcurrentNebulae = 3;
        AudioChannels = 32;
    }

    void ApplyUltra()
    {
        ViewDistance = 20000;
        ShadowQuality = 3;
        TextureQuality = 3;
        EffectsQuality = 3;
        FoliageDensity = 150;
        MaxDrawCalls = 4000;
        PlanetLODResolution = 96;
        FoliageCullDistance = 8000.f;
        MaxConcurrentNebulae = 5;
        AudioChannels = 48;
    }
};

// 性能管理器（Actor，挂在世界中）
UCLASS(BlueprintType)
class STELLARSYSTEM_API APerformanceManager : public AActor
{
    GENERATED_BODY()

public:
    APerformanceManager();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // ---- 客户端性能 ----

    // 自动检测硬件并设定性能等级
    UFUNCTION(BlueprintCallable, Category = "Performance")
    void AutoDetectPerformanceTier();

    // 手动设置性能等级
    UFUNCTION(BlueprintCallable, Category = "Performance")
    void SetPerformanceTier(EPerformanceTier Tier);

    // 获取当前性能等级
    UFUNCTION(BlueprintCallable, Category = "Performance")
    EPerformanceTier GetPerformanceTier() const { return CurrentTier; }

    // 获取当前性能统计
    UFUNCTION(BlueprintCallable, Category = "Performance")
    FPerformanceStats GetCurrentStats() const { return CurrentStats; }

    // 获取自适应质量设置
    UFUNCTION(BlueprintCallable, Category = "Performance")
    FAdaptiveQualitySettings GetQualitySettings() const { return QualitySettings; }

    // 强制垃圾回收（安全时机）
    UFUNCTION(BlueprintCallable, Category = "Performance")
    void RequestGarbageCollection(bool bFullPurge = false);

    // 清除未使用资源
    UFUNCTION(BlueprintCallable, Category = "Performance")
    void FlushUnusedAssets(int32 KeepMinutes = 5);

    // ---- 服务器性能 ----

    // 设置服务器最大 tick 率
    UFUNCTION(BlueprintCallable, Category = "Performance|Server")
    void SetServerTickRate(int32 TickRate);

    // 获取服务器性能统计
    UFUNCTION(BlueprintCallable, Category = "Performance|Server")
    FPerformanceStats GetServerStats() const { return ServerStats; }

    // 网络更新频率控制
    UFUNCTION(BlueprintCallable, Category = "Performance|Server")
    void SetNetworkUpdateRate(float UpdatesPerSecond);

    // 获取当前网络更新率
    UFUNCTION(BlueprintCallable, Category = "Performance|Server")
    float GetNetworkUpdateRate() const { return NetworkUpdateRate; }

    // 空间分区：获取区域内的 Actor
    UFUNCTION(BlueprintCallable, Category = "Performance|Server")
    TArray<AActor*> GetActorsInRegion(const FVector& Center, float Radius) const;

    // 设置空间分区网格大小
    UFUNCTION(BlueprintCallable, Category = "Performance|Server")
    void SetSpatialGridSize(float GridSize);

    // 批量更新优化（合并多个 Actor 的 Tick）
    void RegisterBatchTick(AActor* Actor, float IntervalSeconds);
    void UnregisterBatchTick(AActor* Actor);

    // ---- 加载优化 ----

    // 异步加载关卡
    UFUNCTION(BlueprintCallable, Category = "Performance|Loading")
    void LoadLevelAsync(const FString& LevelName, FName PackageName);

    // 预热着色器
    UFUNCTION(BlueprintCallable, Category = "Performance|Loading")
    void PrewarmShaders(const TArray<FString>& MaterialPaths);

    // 资产预加载
    UFUNCTION(BlueprintCallable, Category = "Performance|Loading")
    void PreloadAssetsAsync(const TArray<FSoftObjectPath>& AssetPaths,
                            FName CompleteEventName = NAME_None);

    // 获取加载进度
    UFUNCTION(BlueprintCallable, Category = "Performance|Loading")
    float GetLoadingProgress() const { return LoadingProgress; }

    // ---- 配置 ----

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    FPerformanceBudget Budget;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    bool bEnableAdaptiveQuality = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    bool bEnableAutoGC = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    float AutoGCCheckInterval = 30.f;  // 每 30 秒检查一次

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance|Server")
    float ServerTickInterval = 1.f / 30.f;  // 30Hz 默认

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance|Server")
    int32 MaxPlayers = 32;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance|Server")
    float NetworkUpdateRate = 20.f;  // Hz

    // 委托
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPerformanceTierChanged,
        EPerformanceTier, OldTier, EPerformanceTier, NewTier);
    UPROPERTY(BlueprintAssignable, Category = "Performance")
    FOnPerformanceTierChanged OnPerformanceTierChanged;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLoadingProgressChanged,
        float, Progress);
    UPROPERTY(BlueprintAssignable, Category = "Performance")
    FOnLoadingProgressChanged OnLoadingProgressChanged;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMemoryWarning,
        float, MemoryMB);
    UPROPERTY(BlueprintAssignable, Category = "Performance")
    FOnMemoryWarning OnMemoryWarning;

private:
    // 性能检测
    void UpdatePerformanceStats(float Dt);
    void DetectHardwareSpecs();
    void ApplyQualitySettings();
    void AdaptiveQualityAdjust(float Dt);

    // 帧时间采样
    TArray<float> FrameTimeSamples;
    int32 FrameSampleIndex = 0;
    float FrameSampleAccumulator = 0.f;
    int32 FrameSampleCount = 0;

    // GC 管理
    float GCCheckTimer = 0.f;
    float LastGCTime = 0.f;
    bool bGCPending = false;

    // 网络统计
    float NetworkSampleTimer = 0.f;
    int32 NetworkBytesIn = 0;
    int32 NetworkBytesOut = 0;

    // 空间分区
    float SpatialGridSize = 100000.f;  // 1km 网格
    TMap<FIntVector, TArray<TWeakObjectPtr<AActor>>> SpatialGrid;

    // 批量 Tick
    struct FBatchTickEntry
    {
        TWeakObjectPtr<AActor> Actor;
        float Interval;
        float Accumulator;
    };
    TArray<FBatchTickEntry> BatchTickList;

    // 加载管理
    float LoadingProgress = 0.f;
    TArray<FSoftObjectPath> PendingLoads;
    int32 CompletedLoads = 0;

    // 当前状态
    EPerformanceTier CurrentTier = EPerformanceTier::Auto;
    FPerformanceStats CurrentStats;
    FPerformanceStats ServerStats;
    FAdaptiveQualitySettings QualitySettings;

    // 内存警告阈值（MB）
    float MemoryWarningThreshold = 2048.f;  // 2GB
    bool bMemoryWarningFired = false;

    // 帧率平滑
    float SmoothedFPS = 60.f;
    float FPSHistory[60];  // 1 秒历史
    int32 FPSHistoryIndex = 0;

    // 自适应调整冷却
    float AdaptiveAdjustTimer = 0.f;
    static constexpr float ADAPTIVE_COOLDOWN = 5.f;  // 5 秒冷却

    // 服务器统计
    float ServerTickTimer = 0.f;
    int32 ServerActorCount = 0;
    float ServerFrameTimeMs = 0.f;
};
