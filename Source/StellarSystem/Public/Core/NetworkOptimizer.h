// NetworkOptimizer.h
// 网络性能优化：频率控制/优先级/带宽管理/丢包补偿
// v6.6 性能优化

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NetworkOptimizer.generated.h"

// 网络优先级
UENUM(BlueprintType)
enum class ENetworkPriority : uint8
{
    Critical  UMETA(DisplayName = "Critical (Always)"),
    High      UMETA(DisplayName = "High (20Hz)"),
    Normal    UMETA(DisplayName = "Normal (10Hz)"),
    Low       UMETA(DisplayName = "Low (5Hz)"),
    Background UMETA(DisplayName = "Background (2Hz)")
};

// 网络更新配置
USTRUCT(BlueprintType)
struct FNetworkUpdateConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ENetworkPriority Priority = ENetworkPriority::Normal;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float UpdateInterval = 0.1f;  // 默认 10Hz

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RelevancyRadius = 50000.f;  // 50m 默认相关

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bUseAdaptiveFrequency = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MinUpdateInterval = 0.05f;  // 最快 20Hz

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxUpdateInterval = 0.5f;  // 最慢 2Hz

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bCompressData = true;
};

// 带宽统计
USTRUCT(BlueprintType)
struct FBandwidthStats
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    float IncomingKBps = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float OutgoingKBps = 0.f;

    UPROPERTY(BlueprintReadOnly)
    int32 ReplicatedActors = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 SkippedUpdates = 0;  // 被跳过的更新

    UPROPERTY(BlueprintReadOnly)
    float CompressionRatio = 1.f;  // 1.0 = 无压缩

    UPROPERTY(BlueprintReadOnly)
    float PacketLossRate = 0.f;
};

// 网络优化器
UCLASS(BlueprintType)
class STELLARSYSTEM_API ANetworkOptimizer : public AActor
{
    GENERATED_BODY()

public:
    ANetworkOptimizer();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // 注册 Actor 的网络更新配置
    UFUNCTION(BlueprintCallable, Category = "Network|Optimization")
    void RegisterActor(AActor* Actor, const FNetworkUpdateConfig& Config);

    // 注销
    UFUNCTION(BlueprintCallable, Category = "Network|Optimization")
    void UnregisterActor(AActor* Actor);

    // 设置全局带宽限制
    UFUNCTION(BlueprintCallable, Category = "Network|Optimization")
    void SetBandwidthLimit(float MaxKBpsOut, float MaxKBpsIn);

    // 获取带宽统计
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Network|Optimization")
    FBandwidthStats GetBandwidthStats() const { return BandwidthStats; }

    // 自适应频率：根据带宽压力自动调整
    UFUNCTION(BlueprintCallable, Category = "Network|Optimization")
    void EnableAdaptiveFrequency(bool bEnable);

    // 设置相关距离（相关性剔除）
    UFUNCTION(BlueprintCallable, Category = "Network|Optimization")
    void SetRelevancyDistance(float Distance);

    // 紧急通道：强制立即复制（用于关键事件）
    UFUNCTION(BlueprintCallable, Category = "Network|Optimization")
    void ForceReplicate(AActor* Actor);

    // 网络暂停（断线重连时）
    UFUNCTION(BlueprintCallable, Category = "Network|Optimization")
    void PauseNetwork();

    UFUNCTION(BlueprintCallable, Category = "Network|Optimization")
    void ResumeNetwork();

    // 丢包补偿
    UFUNCTION(BlueprintCallable, Category = "Network|Optimization")
    void SetPacketLossCompensation(bool bEnable, float RedundancyFactor = 1.5f);

    // 配置
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network")
    float GlobalUpdateInterval = 0.1f;  // 全局默认 10Hz

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network")
    int32 MaxReplicatedActors = 256;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network")
    bool bEnableRelevancyCulling = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network")
    float DefaultRelevancyDistance = 100000.f;  // 1km

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network")
    bool bEnableBandwidthShaping = true;

    // 委托
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBandwidthWarning,
        float, CurrentKBps);
    UPROPERTY(BlueprintAssignable, Category = "Network")
    FOnBandwidthWarning OnBandwidthWarning;

private:
    // Actor 注册表
    struct FRegisteredActor
    {
        TWeakObjectPtr<AActor> Actor;
        FNetworkUpdateConfig Config;
        float Accumulator = 0.f;
        float LastUpdateTime = 0.f;
        FVector LastReplicatedLocation;
        FRotator LastReplicatedRotation;
        bool bPendingUpdate = false;
    };

    TArray<FRegisteredActor> RegisteredActors;

    // 带宽管理
    float BandwidthOutLimit = 1024.f;  // 1MB/s 默认
    float BandwidthInLimit = 1024.f;
    float BandwidthAccumulator = 0.f;
    float BandwidthWindowTimer = 0.f;
    static constexpr float BANDWIDTH_WINDOW = 1.f;  // 1 秒窗口

    FBandwidthStats BandwidthStats;

    // 自适应
    bool bAdaptiveEnabled = true;
    float AdaptiveTimer = 0.f;
    static constexpr float ADAPTIVE_CHECK_INTERVAL = 2.f;

    // 网络状态
    bool bNetworkPaused = false;

    // 丢包补偿
    bool bPacketLossCompEnabled = false;
    float RedundancyFactor = 1.5f;

    // 内部方法
    void UpdateActorReplication(float Dt);
    void UpdateBandwidthShaping(float Dt);
    void AdaptiveFrequencyAdjust();
    bool ShouldReplicateActor(const FRegisteredActor& Reg) const;
    float CalculatePriorityScore(const FRegisteredActor& Reg) const;
    void CompressReplicationData(FRegisteredActor& Reg);
};
