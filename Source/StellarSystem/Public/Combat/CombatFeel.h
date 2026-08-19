// CombatFeel.h
// 飞船战斗手感调优系统
// 通过数据驱动的方式调整：惯性、推力曲线、FOV变化、屏幕震动、命中反馈

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CombatFeel.generated.h"

// 推力响应曲线类型
UENUM(BlueprintType)
enum class EThrustCurve : uint8
{
    Linear,         // 线性，直接响应
    EaseIn,         // 缓入，启动慢加速快
    EaseOut,        // 缓出，启动快减速慢
    Exponential,    // 指数，需要预热
    Custom          // 使用曲线资产
};

// 单个轴向的飞行参数
USTRUCT(BlueprintType)
struct FFlightAxisParams
{
    GENERATED_BODY()

    // 最大加速度 (cm/s^2)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "100"))
    float MaxAcceleration = 3000.f;

    // 最大速度 (cm/s)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "100"))
    float MaxSpeed = 5000.f;

    // 反向推力倍率（刹车效率）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "3"))
    float ReverseThrustMultiplier = 0.6f;

    // 惯性阻尼（0=无阻尼像太空，1=立刻停下）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "1"))
    float InertiaDamping = 0.05f;

    // 角速度上限 (deg/s)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "10"))
    float MaxAngularSpeed = 90.f;

    // 角加速度 (deg/s^2)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "10"))
    float AngularAcceleration = 180.f;

    // 响应曲线
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EThrustCurve ResponseCurve = EThrustCurve::EaseIn;
};

// FOV 动态变化参数
USTRUCT(BlueprintType)
struct FFOVParams
{
    GENERATED_BODY()

    // 基础 FOV
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "50", ClampMax = "120"))
    float BaseFOV = 90.f;

    // 最大额外 FOV（高速时增加）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "40"))
    float MaxFOVBoost = 15.f;

    // 触发最大 FOV 的速度比例（0~1）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.1", ClampMax = "1"))
    float FOVBoostSpeedRatio = 0.8f;

    // FOV 变化速度（越大越快）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.1", ClampMax = "10"))
    float FOVTransitionSpeed = 3.f;

    // 跃迁时 FOV 变化
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "60", ClampMax = "150"))
    float WarpFOV = 120.f;
};

// 屏幕震动参数
USTRUCT(BlueprintType)
struct FScreenShakeParams
{
    GENERATED_BODY()

    // 引擎震动（持续，与推力相关）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EngineVibrationAmplitude = 0.05f;

    // 引擎震动频率
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1", ClampMax = "100"))
    float EngineVibrationFrequency = 20.f;

    // 被击中震动幅度
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HitImpactAmplitude = 0.5f;

    // 被击中震动时长
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.05", ClampMax = "2"))
    float HitImpactDuration = 0.3f;

    // 爆炸震动幅度
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ExplosionAmplitude = 2.f;

    // 跃迁进入震动
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float WarpEnterAmplitude = 1.f;

    // 跃迁到达震动
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float WarpArriveAmplitude = 1.5f;
};

// G-Force 模糊参数
USTRUCT(BlueprintType)
struct FGForceParams
{
    GENERATED_BODY()

    // 启用 G-Force 视觉效果
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableGForceEffect = true;

    // 最大运动模糊强度
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "5"))
    float MaxMotionBlur = 2.f;

    // 触发最大模糊的加速度阈值 (cm/s^2)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1000"))
    float BlurAccelerationThreshold = 5000.f;

    // 暗角最大强度
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "2"))
    float MaxVignette = 0.8f;

    // 色差最大强度（高速时边缘色散）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "5"))
    float MaxChromaticAberration = 1.5f;
};

// 命中反馈参数
USTRUCT(BlueprintType)
struct FHitFeedbackParams
{
    GENERATED_BODY()

    // 命中标记显示时长
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.1", ClampMax = "3"))
    float HitMarkerDuration = 0.5f;

    // 伤害数字飘字时长
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.5", ClampMax = "5"))
    float DamageNumberDuration = 2.f;

    // 护盾击穿特效时长
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.1", ClampMax = "2"))
    float ShieldBreakDuration = 0.8f;

    // 击杀慢动作时长
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "2"))
    float KillSlowMoDuration = 0.4f;

    // 慢动作时间缩放
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.1", ClampMax = "1"))
    float SlowMoTimeScale = 0.3f;
};

// ★ 主数据资产：一张表调所有手感
UCLASS(BlueprintType)
class UCombatFeelProfile : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // 前进/后退
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thrust")
    FFlightAxisParams ForwardThrust;

    // 左右平移
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thrust")
    FFlightAxisParams StrafeThrust;

    // 升降
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thrust")
    FFlightAxisParams VerticalThrust;

    // 偏航（Yaw）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation")
    FFlightAxisParams YawRotation;

    // 俯仰（Pitch）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation")
    FFlightAxisParams PitchRotation;

    // 滚转（Roll）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation")
    FFlightAxisParams RollRotation;

    // FOV
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    FFOVParams FOV;

    // 屏幕震动
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    FScreenShakeParams ScreenShake;

    // G-Force
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    FGForceParams GForce;

    // 命中反馈
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback")
    FHitFeedbackParams HitFeedback;

    // 飞船类型预设（轻/中/重）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset")
    FName ShipClassTag; // "Fighter"/"Freighter"/"Explorer"/"Capital"

    // 获取当前 FOV（根据速度插值）
    UFUNCTION(BlueprintCallable, Category = "CombatFeel")
    float GetDynamicFOV(float CurrentSpeed, float MaxPossibleSpeed) const;

    // 获取推力曲线值
    UFUNCTION(BlueprintCallable, Category = "CombatFeel")
    float EvaluateThrustCurve(EThrustCurve Curve, float Input) const;

    // 获取 G-Force 强度
    UFUNCTION(BlueprintCallable, Category = "CombatFeel")
    float GetGForceIntensity(float CurrentAcceleration) const;
};
