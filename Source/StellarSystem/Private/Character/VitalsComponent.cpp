// VitalsComponent.cpp
// 维生系统完整实现（8 项指标全模拟）
// v5.0：集成 SpaceWeather + 太阳风/辐射风暴
#include "Character/VitalsComponent.h"
#include "Character/MyCharacter.h"
#include "Planet/ProceduralPlanet.h"
#include "Core/SpaceWeather.h"
#include "GameFramework/Character.h"
#include "Math/UnrealMathUtility.h"

UVitalsComponent::UVitalsComponent()
{
    PrimaryComponentTick.bCanEverTick = false; // 由 Owner 手动 Tick

    Oxygen.DrainRate = 0.5f;
    Energy.DrainRate = 0.3f;
    Hunger.DrainRate = 0.1f;
    Thirst.DrainRate = 0.15f;
    Radiation.DrainRate = 0.f;
    Toxin.DrainRate = 0.f;
    Health.DrainRate = 0.f;
    BleedRate = 0.f;

    // v5.0 默认值
    CurrentRadiationLevel = 0.f;
    CurrentSolarWind = 0.f;
}

void UVitalsComponent::Init(AMyCharacter* OwnerChar)
{
    OwnerCharacter = OwnerChar;
}

// ======================= 主 Tick =======================
void UVitalsComponent::TickVitals(float DeltaTime, AProceduralPlanet* CurrentPlanet)
{
    CheckEnvironment(CurrentPlanet);
    QueryWeather(WeatherSource.Get());

    UpdateOxygen(DeltaTime);
    UpdateEnergy(DeltaTime);
    UpdateHunger(DeltaTime);
    UpdateThirst(DeltaTime);
    UpdateTemperature(DeltaTime);
    UpdateRadiation(DeltaTime);
    UpdateToxin(DeltaTime);
    UpdateBleeding(DeltaTime);
    ApplyEnvironmentalDamage(DeltaTime);
}

// ======================= 各指标更新 =======================
void UVitalsComponent::UpdateOxygen(float DT)
{
    if (bInVacuum && !bHasSpaceSuit)
    {
        Oxygen.Current = FMath::Max(0.f, Oxygen.Current - DT * 5.f);
    }
    else if (bInVacuum && bHasSpaceSuit)
    {
        if (SuitIntegrity > 0.f)
        {
            float SuitEff = SuitIntegrity / 100.f;
            Oxygen.Current = FMath::Max(0.f,
                Oxygen.Current - DT * Oxygen.DrainRate * (2.f - SuitEff));
        }
        else
        {
            Oxygen.Current = FMath::Max(0.f, Oxygen.Current - DT * 8.f);
            OnSuitBreach.Broadcast(EVitalStatus::Critical);
        }
    }
    else
    {
        // 有大气：缓慢消耗
        Oxygen.Current = FMath::Max(0.f,
            Oxygen.Current - DT * Oxygen.DrainRate * 0.3f);
    }

    if (Oxygen.GetStatus() == EVitalStatus::Critical && OwnerCharacter)
    {
        OnOxygenCritical.Broadcast(EVitalStatus::Critical);
    }
}

void UVitalsComponent::UpdateEnergy(float DT)
{
    Energy.Current = FMath::Max(0.f, Energy.Current - DT * Energy.DrainRate);
}

void UVitalsComponent::UpdateHunger(float DT)
{
    Hunger.Current = FMath::Min(Hunger.Max, Hunger.Current + DT * Hunger.DrainRate);
    if (Hunger.Current > 80.f && Health.Current > 0.f)
    {
        Health.Current = FMath::Max(0.f, Health.Current - DT * 0.5f);
    }
}

void UVitalsComponent::UpdateThirst(float DT)
{
    Thirst.Current = FMath::Min(Thirst.Max, Thirst.Current + DT * Thirst.DrainRate);
    if (Thirst.Current > 80.f && Health.Current > 0.f)
    {
        Health.Current = FMath::Max(0.f, Health.Current - DT * 1.0f);
    }
}

void UVitalsComponent::UpdateTemperature(float DT)
{
    float SuitEff = (SuitIntegrity / 100.f) * SuitInsulation;
    float Diff = CurrentEnvironmentTemp - BodyTemperature;
    BodyTemperature += Diff * TempChangeRate * DT * (1.f - SuitEff);

    if (BodyTemperature < MinSafeTemp || BodyTemperature > MaxSafeTemp)
    {
        float Excess = FMath::Max(
            MinSafeTemp - BodyTemperature,
            BodyTemperature - MaxSafeTemp);
        float Resist = (BodyTemperature < MinSafeTemp) ? ColdResistance : HeatResistance;
        float Damage = Excess * DT * 0.1f * (1.f - Resist * 0.5f);
        Health.Current = FMath::Max(0.f, Health.Current - Damage);
    }
}

void UVitalsComponent::UpdateRadiation(float DT)
{
    if (CurrentRadiationLevel <= 0.f) return;

    float SuitEff = (SuitIntegrity / 100.f) * SuitRadiationShield;
    float AdjustedRad = CurrentRadiationLevel * (1.f - SuitEff) * (1.f - RadiationResistance);
    Radiation.Current = FMath::Min(Radiation.Max, Radiation.Current + AdjustedRad * DT);

    if (Radiation.Current > 50.f)
    {
        Health.Current = FMath::Max(0.f,
            Health.Current - DT * (Radiation.Current - 50.f) * 0.05f);
    }

    if (Radiation.Current > 30.f && OwnerCharacter)
    {
        OnRadiationCritical.Broadcast(EVitalStatus::Warning);
    }
}

void UVitalsComponent::UpdateToxin(float DT)
{
    if (Toxin.Current <= 0.f) return;
    float Adjusted = Toxin.DrainRate * (1.f - ToxinResistance);
    Toxin.Current = FMath::Max(0.f, Toxin.Current - DT * 0.05f);
    Health.Current = FMath::Max(0.f, Health.Current - DT * Adjusted * 0.3f);
}

void UVitalsComponent::UpdateBleeding(float DT)
{
    if (BleedRate <= 0.f) return;
    Health.Current = FMath::Max(0.f, Health.Current - BleedRate * DT);
}

// ======================= 环境检测 =======================
void UVitalsComponent::CheckEnvironment(AProceduralPlanet* Planet)
{
    if (!Planet || !OwnerCharacter) return;

    FVector CharPos = OwnerCharacter->GetActorLocation();
    FVector PlanetCenter = Planet->GetActorLocation();
    float Dist = FVector::Dist(CharPos, PlanetCenter);
    float SurfaceR = Planet->PlanetRadius;

    bInVacuum = (Dist > SurfaceR * 1.05f);

    // 温度估算
    float HeightRatio = (Dist - SurfaceR) / FMath::Max(SurfaceR, 1.f);
    CurrentEnvironmentTemp = 20.f - HeightRatio * 40.f;

    // 基础辐射（太空中）
    CurrentRadiationLevel = bInVacuum ? 0.1f : 0.0f;
}

// v5.0：查询天气系统
void UVitalsComponent::QueryWeather(ASpaceWeather* Weather)
{
    if (!Weather || !OwnerCharacter) return;

    FVector Loc = OwnerCharacter->GetActorLocation();

    float Rad = Weather->GetRadiationLevelAt(Loc);
    float Wind = Weather->GetSolarWindAt(Loc);

    // 叠加到当前辐射
    CurrentRadiationLevel += Rad;
    CurrentSolarWind = Wind;

    // EMP 检测
    if (Weather->IsInEMPField(Loc))
    {
        // EMP：暂时禁用能量恢复
        Energy.Current = FMath::Max(0.f, Energy.Current - 2.f * GetWorld()->GetDeltaSeconds());
    }
}

// ======================= 环境伤害 =======================
void UVitalsComponent::ApplyEnvironmentalDamage(float DT)
{
    if (bInVacuum && !bHasSpaceSuit)
    {
        Health.Current = FMath::Max(0.f, Health.Current - DT * 3.f);
    }
    else if (bInVacuum && bHasSpaceSuit && SuitIntegrity <= 0.f)
    {
        Health.Current = FMath::Max(0.f, Health.Current - DT * 5.f);
        OnSuitBreach.Broadcast(EVitalStatus::Critical);
    }

    if (Health.Current <= 0.f && OwnerCharacter)
    {
        OnHealthCritical.Broadcast(EVitalStatus::Depleted);
        // 通知 Owner 处理死亡
        if (ACharacter* C = Cast<ACharacter>(OwnerCharacter))
        {
            C->TakeDamage(0.f, FDamageEvent(), nullptr, nullptr);
        }
    }
}

// ======================= 公共接口 =======================
void UVitalsComponent::ConsumeOxygen(float Amount) { Oxygen.Current = FMath::Max(0.f, Oxygen.Current - Amount); }
void UVitalsComponent::ConsumeEnergy(float Amount)  { Energy.Current = FMath::Max(0.f, Energy.Current - Amount); }
void UVitalsComponent::RestoreOxygen(float Amount)  { Oxygen.Current = FMath::Min(Oxygen.Max, Oxygen.Current + Amount); }
void UVitalsComponent::RestoreEnergy(float Amount)   { Energy.Current = FMath::Min(Energy.Max, Energy.Current + Amount); }
void UVitalsComponent::Feed(float Amount)           { Hunger.Current = FMath::Max(0.f, Hunger.Current - Amount); }
void UVitalsComponent::Hydrate(float Amount)        { Thirst.Current = FMath::Max(0.f, Thirst.Current - Amount); }
void UVitalsComponent::ApplyRadiation(float Sv)     { Radiation.Current = FMath::Min(Radiation.Max, Radiation.Current + Sv); }
void UVitalsComponent::ApplyToxin(float Amount)     { Toxin.Current = FMath::Min(Toxin.Max, Toxin.Current + Amount); }
void UVitalsComponent::StartBleeding(float Rate)   { BleedRate = Rate; }
void UVitalsComponent::StopBleeding()              { BleedRate = 0.f; }
void UVitalsComponent::DamageSuit(float Amount)    { SuitIntegrity = FMath::Max(0.f, SuitIntegrity - Amount); }

void UVitalsComponent::HealHealth(float Amount)
{
    Health.Current = FMath::Min(Health.Max, Health.Current + Amount);
}

bool UVitalsComponent::CanSprint() const
{
    return Energy.Current > 20.f;
}

EVitalStatus UVitalsComponent::GetOverallStatus() const
{
    EVitalStatus Worst = EVitalStatus::Normal;
    auto CheckWorst = [&](EVitalStatus S)
    {
        if ((int32)S < (int32)Worst) Worst = S;
    };

    CheckWorst(Oxygen.GetStatus());
    CheckWorst(Energy.GetStatus());
    CheckWorst(Hunger.GetStatus());
    CheckWorst(Thirst.GetStatus());
    CheckWorst(Radiation.GetStatus());
    CheckWorst(Toxin.GetStatus());
    CheckWorst(Health.GetStatus());
    return Worst;
}

FString UVitalsComponent::GetStatusText() const
{
    EVitalStatus S = GetOverallStatus();
    switch (S)
    {
        case EVitalStatus::Normal:    return TEXT("All systems nominal");
        case EVitalStatus::Warning:   return TEXT("Warning: vital signs dropping");
        case EVitalStatus::Critical:  return TEXT("CRITICAL: immediate action required");
        case EVitalStatus::Depleted:  return TEXT("FATAL: vital depleted");
    }
    return TEXT("");
}
