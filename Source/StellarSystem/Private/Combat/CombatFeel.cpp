// CombatFeel.cpp
// 战斗手感调优实现

#include "Combat/CombatFeel.h"
#include "Math/UnrealMathUtility.h"

float UCombatFeelProfile::GetDynamicFOV(float CurrentSpeed, float MaxPossibleSpeed) const
{
    float Ratio = (MaxPossibleSpeed > 0.f) ? (CurrentSpeed / MaxPossibleSpeed) : 0.f;
    Ratio = FMath::Clamp(Ratio, 0.f, 1.f);

    // 低于阈值时保持基础 FOV
    float BoostThreshold = FOV.FOVBoostSpeedRatio;
    float BoostAmount = 0.f;

    if (Ratio > BoostThreshold)
    {
        float T = (Ratio - BoostThreshold) / (1.f - BoostThreshold);
        T = FMath::Clamp(T, 0.f, 1.f);
        // Smoothstep 缓动
        T = T * T * (3.f - 2.f * T);
        BoostAmount = FOV.MaxFOVBoost * T;
    }

    return FOV.BaseFOV + BoostAmount;
}

float UCombatFeelProfile::EvaluateThrustCurve(EThrustCurve Curve, float Input) const
{
    Input = FMath::Clamp(Input, 0.f, 1.f);

    switch (Curve)
    {
    case EThrustCurve::Linear:
        return Input;

    case EThrustCurve::EaseIn:
        // 慢启动快加速
        return Input * Input;

    case EThrustCurve::EaseOut:
        // 快启动慢减速
        return 1.f - (1.f - Input) * (1.f - Input);

    case EThrustCurve::Exponential:
    {
        // 需要预热：前 20% 几乎没推力
        float Bias = 0.2f;
        if (Input < Bias)
            return 0.f;
        float T = (Input - Bias) / (1.f - Bias);
        return T * T * (3.f - 2.f * T);
    }

    case EThrustCurve::Custom:
        // 留给蓝图/曲线资产覆盖
        return Input;

    default:
        return Input;
    }
}

float UCombatFeelProfile::GetGForceIntensity(float CurrentAcceleration) const
{
    if (!GForce.bEnableGForceEffect) return 0.f;
    if (GForce.BlurAccelerationThreshold <= 0.f) return 0.f;

    float Ratio = CurrentAcceleration / GForce.BlurAccelerationThreshold;
    Ratio = FMath::Clamp(Ratio, 0.f, 1.f);

    // 平滑曲线
    return Ratio * Ratio;
}
