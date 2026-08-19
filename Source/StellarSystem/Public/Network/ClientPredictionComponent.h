// ClientPredictionComponent.h
// StellarSystem v6.8 — 客户端预测/插值/回滚组件

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Network/NetworkTransportOptimizer.h"
#include "ClientPredictionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPredictionError, float, ErrorAmount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRollbackApplied);

/**
 * UClientPredictionComponent
 *
 * 挂在玩家 Pawn 上，负责：
 * 1. 采集输入 → 立即本地应用（预测）
 * 2. 发送输入命令到服务器
 * 3. 接收服务器校正 → 检测偏差
 * 4. 偏差过大 → 回滚 + 重放未确认输入
 * 5. 插值渲染（平滑其他玩家）
 */
UCLASS(ClassGroup=(Network), meta=(BlueprintSpawnableComponent))
class STELLARSYSTEM_API UClientPredictionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UClientPredictionComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                                FActorComponentTickFunction* ThisTickFunction) override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // ═══════════════════════════════════════
    //  配置
    // ═══════════════════════════════════════

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Prediction")
    int32  MaxBufferedInputs = 64;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Prediction")
    float  MaxPredictionError = 10.f;  // 厘米，超过则回滚

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Prediction")
    float  InterpBackTime = 0.1f;  // 插值回退时间

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Prediction")
    bool   bEnablePrediction = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Prediction")
    bool   bEnableInterpolation = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Prediction")
    bool   bEnableRollback = true;

    // ═══════════════════════════════════════
    //  API（蓝图/代码调用）
    // ═══════════════════════════════════════

    /** 采集当前输入状态（每帧调用） */
    UFUNCTION(BlueprintCallable, Category="Prediction|Input")
    void CaptureInput(float Forward, float Right, float Up,
                      float Pitch, float Yaw, float Roll,
                      uint8 InputFlags);

    /** 应用本地预测（立即移动） */
    UFUNCTION(BlueprintCallable, Category="Prediction|Input")
    void ApplyLocalPrediction(float DeltaTime);

    /** 接收服务器校正 */
    UFUNCTION(BlueprintCallable, Category="Prediction|Correction")
    void OnServerCorrection(uint16 ServerSeq, const FVector& ServerPosition,
                            const FRotator& ServerRotation, float ServerTime);

    /** 接收其他玩家快照（插值用） */
    UFUNCTION(BlueprintCallable, Category="Prediction|Interpolation")
    void OnRemoteSnapshot(uint16 Seq, const FVector& Position,
                           const FRotator& Rotation, float Timestamp);

    /** 获取插值后的位置（用于渲染） */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="Prediction|Interpolation")
    FVector GetInterpolatedPosition() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="Prediction|Interpolation")
    FRotator GetInterpolatedRotation() const;

    /** 清除所有缓冲（如发生传送） */
    UFUNCTION(BlueprintCallable, Category="Prediction")
    void ResetBuffers();

    // ═══════════════════════════════════════
    //  统计
    // ═══════════════════════════════════════

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="Prediction|Stats")
    int32 GetUnacknowledgedCount() const { return UnackedInputs.Num(); }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="Prediction|Stats")
    float GetAverageError() const { return AvgError; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="Prediction|Stats")
    int32 GetTotalRollbacks() const { return TotalRollbacks; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="Prediction|Stats")
    float GetPredictionAccuracy() const;

    // ═══════════════════════════════════════
    //  事件
    // ═══════════════════════════════════════

    UPROPERTY(BlueprintAssignable, Category="Prediction|Events")
    FOnPredictionError OnPredictionError;

    UPROPERTY(BlueprintAssignable, Category="Prediction|Events")
    FOnRollbackApplied OnRollbackApplied;

private:
    // 输入历史（用于重放）
    struct FInputCommand
    {
        uint16  Sequence;
        float   Forward, Right, Up;
        float   Pitch, Yaw, Roll;
        uint8   Flags;
        float   Timestamp;
        FVector  PredictedPosition;
        FRotator PredictedRotation;
    };

    TArray<FInputCommand> InputHistory;
    TArray<FInputCommand> UnackedInputs;

    // 插值缓冲（其他玩家）
    struct FRemoteState
    {
        uint16  Seq;
        FVector Position;
        FRotator Rotation;
        float   Timestamp;
    };
    TArray<FRemoteState> RemoteBuffer;

    // 当前状态
    FVector  CurrentPosition;
    FRotator CurrentRotation;
    uint16   NextInputSeq = 0;
    float    LastError = 0.f;
    float    AvgError = 0.f;
    int32    TotalRollbacks = 0;
    float    LastCorrectionTime = 0.f;

    // 内部方法
    void    PerformRollback(const FVector& ServerPos, const FRotator& ServerRot);
    void    ReplayUnackedInputs();
    FVector InterpolateRemotePosition(float RenderTime) const;
    FRotator InterpolateRemoteRotation(float RenderTime) const;
    float   GetRenderTime() const;
    void    PruneOldInputs(uint16 AckedSeq);
};
