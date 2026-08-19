// SpaceWeather.cpp
#include "Core/SpaceWeather.h"
#include "Core/StellarStar.h"
#include "Particles/ParticleSystemComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Math/UnrealMathUtility.h"

ASpaceWeather::ASpaceWeather()
{
    PrimaryActorTick.bCanEverTick = true;
    Settings.MinEventInterval = 60.f;
    Settings.MaxEventInterval = 300.f;
    Settings.WarningLeadTime = 10.f;
    Settings.MaxConcurrentEvents = 3;
    NextEventTimer = FMath::RandRange(Settings.MinEventInterval, Settings.MaxEventInterval);
}

void ASpaceWeather::BeginPlay()
{
    Super::BeginPlay();
    // 预热
    NextEventTimer = 30.f; // 30 秒后第一次事件
}

void ASpaceWeather::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 事件调度
    NextEventTimer -= DeltaTime;
    if (NextEventTimer <= 0.f && ActiveEvents.Num() < Settings.MaxConcurrentEvents)
    {
        GenerateRandomEvent();
        NextEventTimer = FMath::RandRange(Settings.MinEventInterval, Settings.MaxEventInterval);
    }

    // 更新事件
    UpdateEvents(DeltaTime);

    // 应用效果
    ApplyEffects(DeltaTime);

    // 更新视觉
    UpdateWeatherEffects(DeltaTime);
}

void ASpaceWeather::GenerateRandomEvent()
{
    // 随机选类型
    int32 TypeInt = FMath::RandRange(0, 4);
    EWeatherEvent Type = (EWeatherEvent)TypeInt;

    // 检查是否启用
    if (Type == EWeatherEvent::SolarWind && !Settings.bEnableSolarWind) return;
    if (Type == EWeatherEvent::RadiationStorm && !Settings.bEnableRadiationStorms) return;
    if (Type == EWeatherEvent::EMPBurst && !Settings.bEnableEMPBursts) return;

    // 随机参数
    float Intensity = FMath::RandRange(0.3f, 1.f);
    float Duration = FMath::RandRange(20.f, 90.f);
    FVector Center = GetActorLocation() + FVector(
        FMath::RandRange(-5000000.f, 5000000.f),
        FMath::RandRange(-5000000.f, 5000000.f),
        FMath::RandRange(-2000000.f, 2000000.f));
    float Radius = FMath::RandRange(500000.f, 2000000.f);

    TriggerEvent(Type, Intensity, Duration, Center, Radius);
}

void ASpaceWeather::TriggerEvent(EWeatherEvent Type, float Intensity, float Duration, const FVector& Location, float Radius)
{
    FWeatherEventData NewEvent;
    NewEvent.Type = Type;
    NewEvent.Intensity = FMath::Clamp(Intensity, 0.f, 1.f);
    NewEvent.Duration = Duration;
    NewEvent.RemainingTime = Duration;
    NewEvent.Epicenter = Location;
    NewEvent.EffectRadius = Radius;
    NewEvent.BuildupTimer = Settings.WarningLeadTime;
    NewEvent.bActive = false;
    NewEvent.bWarningIssued = false;

    // 发出预警
    OnWeatherWarning.Broadcast(Type);
    UE_LOG(LogTemp, Warning, TEXT("[Weather] WARNING: %d incoming in %.0fs"), (int32)Type, Settings.WarningLeadTime);

    PendingWarnings.Add(NewEvent);
}

void ASpaceWeather::UpdateEvents(float DeltaTime)
{
    // 处理预警 -> 激活
    for (int32 i = PendingWarnings.Num() - 1; i >= 0; --i)
    {
        FWeatherEventData& E = PendingWarnings[i];
        E.BuildupTimer -= DeltaTime;
        if (E.BuildupTimer <= 0.f)
        {
            E.bActive = true;
            ActiveEvents.Add(E);
            OnWeatherStarted.Broadcast(E.Type, E.Intensity);
            SpawnWeatherEffect(E.Type, E.Epicenter, E.Intensity);
            PendingWarnings.RemoveAt(i);
            UE_LOG(LogTemp, Warning, TEXT("[Weather] EVENT STARTED: %d Intensity %.2f"), (int32)E.Type, E.Intensity);
        }
    }

    // 更新激活事件
    for (int32 i = ActiveEvents.Num() - 1; i >= 0; --i)
    {
        FWeatherEventData& E = ActiveEvents[i];
        E.RemainingTime -= DeltaTime;
        if (E.RemainingTime <= 0.f)
        {
            OnWeatherEnded.Broadcast(E.Type);
            ActiveEvents.RemoveAt(i);
            UE_LOG(LogTemp, Log, TEXT("[Weather] EVENT ENDED: %d"), (int32)E.Type);
        }
    }
}

void ASpaceWeather::ApplyEffects(float DeltaTime)
{
    // 找到玩家
    APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!Player) return;

    FVector PlayerLoc = Player->GetActorLocation();

    // 累加辐射
    float TotalRadiation = 0.f;
    float TotalEMPBurst = 0.f;

    for (const FWeatherEventData& E : ActiveEvents)
    {
        float Dist = FVector::Dist(PlayerLoc, E.Epicenter);
        if (Dist > E.EffectRadius) continue;

        float Falloff = 1.f - (Dist / E.EffectRadius);
        Falloff = FMath::Clamp(Falloff, 0.f, 1.f);

        if (E.Type == EWeatherEvent::RadiationStorm)
        {
            TotalRadiation += E.Intensity * Falloff * DeltaTime * 2.f; // 单位：西弗/秒
        }
        else if (E.Type == EWeatherEvent::EMPBurst)
        {
            TotalEMPBurst += E.Intensity * Falloff;
        }
        else if (E.Type == EWeatherEvent::SolarWind)
        {
            // 太阳风：持续微小伤害/干扰
            TotalRadiation += E.Intensity * Falloff * DeltaTime * 0.3f;
        }
    }

    // 应用辐射到玩家维生系统
    if (TotalRadiation > 0.f)
    {
        // 通过 GameMode 找玩家维生组件
        // 简化：直接发伤害事件
        if (TotalRadiation > 0.5f)
        {
            UGameplayStatics::ApplyDamage(Player, TotalRadiation * 5.f, nullptr, this, nullptr);
        }
    }

    // EMP 效果：暂时禁用飞船系统
    if (TotalEMPBurst > 0.8f)
    {
        // 通知飞船 HUD 显示 EMP 警告
        // 简化实现
    }
}

void ASpaceWeather::SpawnWeatherEffect(EWeatherEvent Type, const FVector& Location, float Intensity)
{
    // 创建粒子效果（简化：用内置粒子）
    UParticleSystem* Particle = nullptr;
    switch (Type)
    {
    case EWeatherEvent::SolarWind:
        // 用灰尘粒子替代
        Particle = LoadObject<UParticleSystem>(nullptr, TEXT("/Engine/Templates/Particles/P_Fire.P_Fire"));
        break;
    case EWeatherEvent::RadiationStorm:
        Particle = LoadObject<UParticleSystem>(nullptr, TEXT("/Engine/Templates/Particles/P_Explosion.P_Explosion"));
        break;
    case EWeatherEvent::EMPBurst:
        Particle = LoadObject<UParticleSystem>(nullptr, TEXT("/Engine/Templates/Particles/P_Steam.P_Steam"));
        break;
    default:
        Particle = LoadObject<UParticleSystem>(nullptr, TEXT("/Engine/Templates/Particles/P_Smoke.P_Smoke"));
        break;
    }

    if (Particle)
    {
        UParticleSystemComponent* Comp = UGameplayStatics::SpawnEmitterAtLocation(
            GetWorld(), Particle, Location, FRotator::ZeroRotator, true);
        if (Comp)
        {
            Comp->SetWorldScale3D(FVector(Intensity * 5.f));
            ActiveEffects.Add(Comp);
        }
    }
}

void ASpaceWeather::UpdateWeatherEffects(float DeltaTime)
{
    // 清理已结束的粒子
    for (int32 i = ActiveEffects.Num() - 1; i >= 0; --i)
    {
        if (!ActiveEffects[i] || !ActiveEffects[i]->IsActive())
        {
            ActiveEffects.RemoveAt(i);
        }
    }
}

void ASpaceWeather::CancelAllEvents()
{
    ActiveEvents.Reset();
    PendingWarnings.Reset();
    for (UParticleSystemComponent* Comp : ActiveEffects)
    {
        if (Comp) Comp->DestroyComponent();
    }
    ActiveEffects.Reset();
}

float ASpaceWeather::GetRadiationLevelAt(const FVector& Location) const
{
    float Total = 0.f;
    for (const FWeatherEventData& E : ActiveEvents)
    {
        if (E.Type != EWeatherEvent::RadiationStorm) continue;
        float Dist = FVector::Dist(Location, E.Epicenter);
        if (Dist > E.EffectRadius) continue;
        float Falloff = 1.f - (Dist / E.EffectRadius);
        Total += E.Intensity * FMath::Clamp(Falloff, 0.f, 1.f);
    }
    return Total;
}

float ASpaceWeather::GetSolarWindAt(const FVector& Location) const
{
    float Total = GetBaseStellarWind();
    for (const FWeatherEventData& E : ActiveEvents)
    {
        if (E.Type != EWeatherEvent::SolarWind) continue;
        float Dist = FVector::Dist(Location, E.Epicenter);
        if (Dist > E.EffectRadius) continue;
        float Falloff = 1.f - (Dist / E.EffectRadius);
        Total += E.Intensity * FMath::Clamp(Falloff, 0.f, 1.f) * 2.f;
    }
    return Total;
}

bool ASpaceWeather::IsInEMPField(const FVector& Location) const
{
    for (const FWeatherEventData& E : ActiveEvents)
    {
        if (E.Type != EWeatherEvent::EMPBurst) continue;
        float Dist = FVector::Dist(Location, E.Epicenter);
        if (Dist < E.EffectRadius) return true;
    }
    return false;
}

TArray<EWeatherEvent> ASpaceWeather::GetActiveEventTypes() const
{
    TArray<EWeatherEvent> Out;
    for (const FWeatherEventData& E : ActiveEvents)
    {
        Out.AddUnique(E.Type);
    }
    return Out;
}

float ASpaceWeather::GetEventIntensityAt(const FVector& Location, EWeatherEvent Type) const
{
    float Best = 0.f;
    for (const FWeatherEventData& E : ActiveEvents)
    {
        if (E.Type != Type) continue;
        float Dist = FVector::Dist(Location, E.Epicenter);
        if (Dist > E.EffectRadius) continue;
        float Falloff = 1.f - (Dist / E.EffectRadius);
        float I = E.Intensity * FMath::Clamp(Falloff, 0.f, 1.f);
        if (I > Best) Best = I;
    }
    return Best;
}

float ASpaceWeather::GetBaseStellarWind() const
{
    if (SourceStar.IsValid())
    {
        AStellarStar* Star = SourceStar.Get();
        if (Star) return Star->GetStellarWindStrength();
    }
    return 1.f; // 默认值
}
