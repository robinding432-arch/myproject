// ClientPredictionComponent.cpp
// StellarSystem v6.8 — 客户端预测/插值/回滚实现

#include "Network/ClientPredictionComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"

// ═══════════════════════════════════════
//  生命周期
// ═══════════════════════════════════════

UClientPredictionComponent::UClientPredictionComponent()
    : NextInputSeq(0)
    , LastError(0.f)
    , AvgError(0.f)
    , TotalRollbacks(0)
    , LastCorrectionTime(0.f)
{
    PrimaryComponentTick.bCanEverTick = true;
    bEnablePrediction = true;
    bEnableInterpolation = true;
    bEnableRollback = true;
}

void UClientPredictionComponent::BeginPlay()
{
    Super::BeginPlay();

    // 初始化当前位置/旋转为 Pawn 的初始值
    if (AActor* Owner = GetOwner())
    {
        CurrentPosition = Owner->GetActorLocation();
        CurrentRotation = Owner->GetActorRotation();
    }

    ResetBuffers();
}

void UClientPredictionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ResetBuffers();
    Super::EndPlay(EndPlayReason);
}

// ═══════════════════════════════════════
//  Tick
// ═══════════════════════════════════════

void UClientPredictionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                              FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!GetOwner()) return;

    // 客户端预测：每帧应用本地输入
    if (bEnablePrediction && GetOwner()->GetLocalRole() == ROLE_AutonomousProxy)
    {
        ApplyLocalPrediction(DeltaTime);
    }

    // 插值渲染（其他玩家）
    if (bEnableInterpolation && GetOwner()->GetLocalRole() == ROLE_SimulatedProxy)
    {
        float RenderTime = GetRenderTime();
        FVector InterpPos = GetInterpolatedPosition();
        FRotator InterpRot = GetInterpolatedRotation();

        if (AActor* Owner = GetOwner())
        {
            Owner->SetActorLocation(InterpPos);
            Owner->SetActorRotation(InterpRot);
        }
    }
}

// ═══════════════════════════════════════
//  输入采集与预测
// ═══════════════════════════════════════

void UClientPredictionComponent::CaptureInput(float Forward, float Right, float Up,
                                               float Pitch, float Yaw, float Roll,
                                               uint8 InputFlags)
{
    FInputCommand Cmd;
    Cmd.Sequence   = NextInputSeq++;
    Cmd.Forward    = Forward;
    Cmd.Right      = Right;
    Cmd.Up         = Up;
    Cmd.Pitch      = Pitch;
    Cmd.Yaw        = Yaw;
    Cmd.Roll       = Roll;
    Cmd.Flags      = InputFlags;
    Cmd.Timestamp  = FPlatformTime::Seconds();
    Cmd.PredictedPosition = CurrentPosition;
    Cmd.PredictedRotation = CurrentRotation;

    InputHistory.Add(Cmd);
    UnackedInputs.Add(Cmd);

    // 限制缓冲大小
    if (InputHistory.Num() > MaxBufferedInputs)
    {
        InputHistory.RemoveAt(0);
    }
    if (UnackedInputs.Num() > MaxBufferedInputs)
    {
        UnackedInputs.RemoveAt(0);
    }
}

void UClientPredictionComponent::ApplyLocalPrediction(float DeltaTime)
{
    if (UnackedInputs.Num() == 0) return;

    // 应用最新的输入命令
    FInputCommand& Cmd = UnackedInputs.Last();

    // 简单运动模型（实际应与 Pawn 移动组件一致）
    FVector MoveDir = FVector(Cmd.Forward, Cmd.Right, Cmd.Up);
    if (!MoveDir.IsNearlyZero())
    {
        MoveDir.Normalize();
    }

    float Speed = 600.f;  // cm/s，应与 Pawn 一致
    FVector Delta = MoveDir * Speed * DeltaTime;

    // 旋转
    FRotator DeltaRot(Cmd.Pitch * 45.f * DeltaTime,
                       Cmd.Yaw * 90.f * DeltaTime,
                       Cmd.Roll * 45.f * DeltaTime);

    CurrentPosition += Delta;
    CurrentRotation += DeltaRot;

    // 更新命令的预测位置
    Cmd.PredictedPosition = CurrentPosition;
    Cmd.PredictedRotation = CurrentRotation;

    // 应用到 Pawn
    if (AActor* Owner = GetOwner())
    {
        Owner->SetActorLocation(CurrentPosition);
        Owner->SetActorRotation(CurrentRotation);
    }
}

// ═══════════════════════════════════════
//  服务器校正 + 回滚
// ═══════════════════════════════════════

void UClientPredictionComponent::OnServerCorrection(uint16 ServerSeq,
                                                       const FVector& ServerPosition,
                                                       const FRotator& ServerRotation,
                                                       float ServerTime)
{
    // 计算误差
    float Error = FVector::Dist(CurrentPosition, ServerPosition);
    LastError = Error;
    AvgError = AvgError * 0.9f + Error * 0.1f;

    // 找到对应的输入命令
    int32 AckIdx = INDEX_NONE;
    for (int32 i = 0; i < UnackedInputs.Num(); i++)
    {
        if (UnackedInputs[i].Sequence == ServerSeq)
        {
            AckIdx = i;
            break;
        }
    }

    if (AckIdx != INDEX_NONE)
    {
        // 移除已确认的输入
        UnackedInputs.RemoveAt(0, AckIdx + 1);
    }

    // 检查是否需要回滚
    if (bEnableRollback && Error > MaxPredictionError)
    {
        PerformRollback(ServerPosition, ServerRotation);
    }
    else
    {
        // 小误差 → 平滑纠正
        CurrentPosition = FMath::Lerp(CurrentPosition, ServerPosition, 0.5f);
        CurrentRotation = FMath::Lerp(CurrentRotation, ServerRotation, 0.5f);
    }

    LastCorrectionTime = FPlatformTime::Seconds();

    // 通知
    if (Error > MaxPredictionError * 2.f)
    {
        OnPredictionError.Broadcast(Error);
    }
}

void UClientPredictionComponent::PerformRollback(const FVector& ServerPos,
                                                    const FRotator& ServerRot)
{
    // 1. 回退到服务器确认的位置
    CurrentPosition = ServerPos;
    CurrentRotation = ServerRot;

    // 2. 重放所有未确认输入
    for (const FInputCommand& Cmd : UnackedInputs)
    {
        FVector MoveDir = FVector(Cmd.Forward, Cmd.Right, Cmd.Up);
        if (!MoveDir.IsNearlyZero()) MoveDir.Normalize();

        float Speed = 600.f;
        float Dt = 1.f / 60.f;  // 假设固定步长
        CurrentPosition += MoveDir * Speed * Dt;

        FRotator DeltaRot(Cmd.Pitch * 45.f * Dt,
                           Cmd.Yaw * 90.f * Dt,
                           Cmd.Roll * 45.f * Dt);
        CurrentRotation += DeltaRot;
    }

    TotalRollbacks++;
    OnRollbackApplied.Broadcast();

    UE_LOG(LogTemp, Log, TEXT("[ClientPrediction] Rollback applied (error=%.1fcm, replayed %d inputs)"),
           LastError, UnackedInputs.Num());
}

void UClientPredictionComponent::ReplayUnackedInputs()
{
    // 与 PerformRollback 中的重放逻辑相同
    for (const FInputCommand& Cmd : UnackedInputs)
    {
        FVector MoveDir = FVector(Cmd.Forward, Cmd.Right, Cmd.Up);
        if (!MoveDir.IsNearlyZero()) MoveDir.Normalize();

        float Speed = 600.f;
        float Dt = 1.f / 60.f;
        CurrentPosition += MoveDir * Speed * Dt;
    }
}

// ═══════════════════════════════════════
//  插值（其他玩家）
// ═══════════════════════════════════════

void UClientPredictionComponent::OnRemoteSnapshot(uint16 Seq, const FVector& Position,
                                                     const FRotator& Rotation, float Timestamp)
{
    FRemoteState State;
    State.Seq       = Seq;
    State.Position  = Position;
    State.Rotation  = Rotation;
    State.Timestamp = Timestamp;

    RemoteBuffer.Add(State);

    // 排序
    RemoteBuffer.Sort([](const FRemoteState& A, const FRemoteState& B) {
        return A.Timestamp < B.Timestamp;
    });

    // 限制缓冲
    if (RemoteBuffer.Num() > 32)
    {
        RemoteBuffer.RemoveAt(0);
    }
}

FVector UClientPredictionComponent::InterpolateRemotePosition(float RenderTime) const
{
    if (RemoteBuffer.Num() < 2) return CurrentPosition;

    // 找到两个相邻状态
    for (int32 i = 0; i < RemoteBuffer.Num() - 1; i++)
    {
        if (RemoteBuffer[i].Timestamp <= RenderTime &&
            RemoteBuffer[i + 1].Timestamp >= RenderTime)
        {
            float Span = RemoteBuffer[i + 1].Timestamp - RemoteBuffer[i].Timestamp;
            float Alpha = (Span > 0) ? (RenderTime - RemoteBuffer[i].Timestamp) / Span : 0.f;
            Alpha = FMath::Clamp(Alpha, 0.f, 1.f);
            return FMath::Lerp(RemoteBuffer[i].Position, RemoteBuffer[i + 1].Position, Alpha);
        }
    }

    return RemoteBuffer.Last().Position;
}

FRotator UClientPredictionComponent::InterpolateRemoteRotation(float RenderTime) const
{
    if (RemoteBuffer.Num() < 2) return CurrentRotation;

    for (int32 i = 0; i < RemoteBuffer.Num() - 1; i++)
    {
        if (RemoteBuffer[i].Timestamp <= RenderTime &&
            RemoteBuffer[i + 1].Timestamp >= RenderTime)
        {
            float Span = RemoteBuffer[i + 1].Timestamp - RemoteBuffer[i].Timestamp;
            float Alpha = (Span > 0) ? (RenderTime - RemoteBuffer[i].Timestamp) / Span : 0.f;
            Alpha = FMath::Clamp(Alpha, 0.f, 1.f);
            return FMath::Lerp(RemoteBuffer[i].Rotation, RemoteBuffer[i + 1].Rotation, Alpha);
        }
    }

    return RemoteBuffer.Last().Rotation;
}

FVector UClientPredictionComponent::GetInterpolatedPosition() const
{
    float RenderTime = GetRenderTime();
    return InterpolateRemotePosition(RenderTime);
}

FRotator UClientPredictionComponent::GetInterpolatedRotation() const
{
    float RenderTime = GetRenderTime();
    return InterpolateRemoteRotation(RenderTime);
}

float UClientPredictionComponent::GetRenderTime() const
{
    // 渲染时间 = 当前时间 - 插值回退
    return FPlatformTime::Seconds() - InterpBackTime;
}

// ═══════════════════════════════════════
//  辅助
// ═══════════════════════════════════════

void UClientPredictionComponent::ResetBuffers()
{
    InputHistory.Empty();
    UnackedInputs.Empty();
    RemoteBuffer.Empty();
    NextInputSeq = 0;
    LastError = 0.f;
    AvgError = 0.f;
}

void UClientPredictionComponent::PruneOldInputs(uint16 AckedSeq)
{
    for (int32 i = InputHistory.Num() - 1; i >= 0; i--)
    {
        // 序列号回绕安全比较
        int32 Diff = (int32)AckedSeq - (int32)InputHistory[i].Sequence;
        if (Diff > 32768) Diff -= 65536;
        if (Diff < -32768) Diff += 65536;
        if (Diff >= 0)
        {
            InputHistory.RemoveAt(i);
        }
    }
}

float UClientPredictionComponent::GetPredictionAccuracy() const
{
    if (TotalRollbacks == 0) return 100.f;
    // 准确率 = 1 - (回滚次数 / 总校正次数)
    // 简化：用平均误差估算
    return FMath::Clamp(100.f - AvgError * 2.f, 0.f, 100.f);
}
