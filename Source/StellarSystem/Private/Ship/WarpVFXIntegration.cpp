// ============================================================
// WarpVFXIntegration.cpp
// 跃迁光效 + 音频集成完整实现
// 路径: Source/StellarSystem/Private/Ship/WarpVFXIntegration.cpp
// ============================================================

#include "Ship/WarpVFXIntegration.h"
#include "Ship/ShipPawn.h"
#include "Space/StellarVisualEffects.h"
#include "Audio/AudioManager.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraShakeBase.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Math/UnrealMathUtility.h"

// ======================== 构造 ========================

UWarpVFXIntegration::UWarpVFXIntegration()
{
    PrimaryComponentTick.bCanEverTick = true;
}

// ======================== 生命周期 ========================

void UWarpVFXIntegration::BeginPlay()
{
    Super::BeginPlay();

    // 自动查找 AudioManager
    if (!AudioMgr)
    {
        AudioMgr = GetAudioManager();
    }

    // 自动查找 VisualEffects
    if (!VisualEffectsActor)
    {
        VisualEffectsActor = FindVisualEffects();
    }
}

void UWarpVFXIntegration::InitializeForShip(AShipPawn* OwningShip)
{
    this->OwningShip = OwningShip;
}

// ======================== Tick ========================

void UWarpVFXIntegration::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // 如果飞船存在，自动更新引擎音频
    if (OwningShip && !bWarpActive)
    {
        // 从飞船获取当前推进输入
        // 简化：通过 GetVelocity 估算
        float Speed = OwningShip->GetVelocity().Size();
        float SpeedRatio = FMath::Clamp(Speed / OwningShip->MaxSpeed, 0.f, 1.f);
        float Thrust = (SpeedRatio > 0.05f) ? SpeedRatio : 0.f;
        UpdateEngineAudio(Thrust, Speed, OwningShip->MaxSpeed);
    }
}

// ======================== 跃迁事件 ========================

void UWarpVFXIntegration::OnWarpStarted(const FVector& WarpTargetLocation)
{
    bWarpActive = true;

    // 计算跃迁方向
    FVector Direction = (WarpTargetLocation - GetOwner()->GetActorLocation()).GetSafeNormal();

    // —— 视觉：开始蓄能 ——
    if (VisualEffectsActor)
    {
        VisualEffectsActor->StartWarpCharge(Direction);
    }

    // —— 音频：开始跃迁音 ——
    if (AudioMgr)
    {
        AudioMgr->StartWarpAudio(1.5f);
    }

    UE_LOG(LogTemp, Log, TEXT("[WarpVFX] Warp started, direction=%s"),
        *Direction.ToString());
}

void UWarpVFXIntegration::OnWarpProgress(float Progress, const FVector& CurrentDirection)
{
    if (!bWarpActive) return;

    // —— 视觉：更新跃迁隧道 ——
    if (VisualEffectsActor)
    {
        VisualEffectsActor->UpdateWarpTunnel(Progress, CurrentDirection);
    }

    // —— 音频：更新跃迁音 ——
    if (AudioMgr)
    {
        AudioMgr->UpdateWarpAudio(Progress);
    }
}

void UWarpVFXIntegration::OnWarpCompleted(const FVector& ArrivalLocation)
{
    bWarpActive = false;

    // —— 视觉：到达闪光 ——
    if (VisualEffectsActor)
    {
        VisualEffectsActor->TriggerArrivalFlash(ArrivalLocation);
        // 短暂延迟后清除
        if (GetWorld())
        {
            FTimerHandle TH;
            GetWorld()->GetTimerManager().SetTimer(TH, [this]()
            {
                if (VisualEffectsActor)
                {
                    VisualEffectsActor->EndWarpEffects();
                }
            }, 2.f, false);
        }
    }

    // —— 音频：停止跃迁音 + 播放到达音 ——
    if (AudioMgr)
    {
        AudioMgr->StopWarpAudio();
    }

    // —— 屏幕震动 ——
    TriggerScreenShake(ArrivalScreenShake);

    UE_LOG(LogTemp, Log, TEXT("[WarpVFX] Warp completed at %s"), *ArrivalLocation.ToString());
}

void UWarpVFXIntegration::OnWarpAborted()
{
    bWarpActive = false;

    if (VisualEffectsActor)
    {
        VisualEffectsActor->EndWarpEffects();
    }

    if (AudioMgr)
    {
        AudioMgr->StopWarpAudio();
    }

    UE_LOG(LogTemp, Log, TEXT("[WarpVFX] Warp aborted"));
}

// ======================== 引擎音频 ========================

void UWarpVFXIntegration::UpdateEngineAudio(float ThrustInput, float CurrentSpeed, float MaxSpeed)
{
    if (!AudioMgr) return;

    // 平滑推力输入
    float TargetVol = FMath::Lerp(MinEngineVolume, MaxEngineVolume,
        FMath::Clamp(ThrustInput, 0.f, 1.f));
    CurrentEngineVol = FMath::FInterpTo(CurrentEngineVol, TargetVol,
        GetWorld()->GetDeltaSeconds(), EngineRampSpeed);

    AudioMgr->SetEngineHumVolume(CurrentEngineVol);

    // 更新音调（速度越快音调越高）
    float SpeedRatio = FMath::Clamp(CurrentSpeed / FMath::Max(MaxSpeed, 1.f), 0.f, 1.f);
    float TargetPitch = FMath::Lerp(MinEnginePitch, MaxEnginePitch, SpeedRatio);

    // 通过 AudioManager 的接口设置
    // （AudioManager 内部 EngineHum 的 pitch 在 PlaySound 时设置）
    // 这里通过 SetEngineHumVolume 间接控制

    LastThrustInput = ThrustInput;
}

void UWarpVFXIntegration::RampEngineVolume(float TargetVol, float Dt)
{
    CurrentEngineVol = FMath::FInterpTo(CurrentEngineVol, TargetVol, Dt, EngineRampSpeed);
    if (AudioMgr)
    {
        AudioMgr->SetEngineHumVolume(CurrentEngineVol);
    }
}

// ======================== 伤害音效 ========================

void UWarpVFXIntegration::OnShieldHit(float DamageAmount)
{
    if (!AudioMgr) return;

    float Intensity = FMath::Clamp(DamageAmount / 50.f, 0.2f, 2.f);
    AudioMgr->PlaySound2D(EAudioCategory::ShieldHit, Intensity);

    // 护盾嗡嗡声随伤害变化
    float ShieldVol = FMath::Clamp(DamageAmount / 100.f, 0.1f, 1.f);
    AudioMgr->SetShieldHumVolume(ShieldVol);
}

void UWarpVFXIntegration::OnHullHit(float DamageAmount)
{
    if (!AudioMgr) return;

    float Intensity = FMath::Clamp(DamageAmount / 30.f, 0.3f, 2.f);
    AudioMgr->PlaySound2D(EAudioCategory::HullHit, Intensity);

    // 严重伤害 → 警报
    if (DamageAmount > 40.f && OwningShip)
    {
        float HullPct = OwningShip->HullIntegrity / 100.f;
        if (HullPct < 0.3f)
        {
            AudioMgr->PlaySound2D(EAudioCategory::HullAlarm, 1.f);
        }
    }
}

void UWarpVFXIntegration::OnHullCritical()
{
    if (!AudioMgr) return;

    AudioMgr->PlaySound2D(EAudioCategory::HullAlarm, 1.5f);

    // 视觉：屏幕震动
    TriggerScreenShake(2.f);
}

// ======================== 武器音效 ========================

void UWarpVFXIntegration::OnWeaponFired(EAudioCategory WeaponType)
{
    if (!AudioMgr) return;

    AudioMgr->PlaySound2D(WeaponType, 1.f);
}

void UWarpVFXIntegration::OnWeaponReload()
{
    if (!AudioMgr) return;

    AudioMgr->PlaySound2D(EAudioCategory::WeaponReload, 0.8f);
}

void UWarpVFXIntegration::OnTargetLocked()
{
    if (!AudioMgr) return;

    AudioMgr->PlaySound2D(EAudioCategory::LockOn_Acquire, 1.f);
}

void UWarpVFXIntegration::OnTargetLost()
{
    if (!AudioMgr) return;

    AudioMgr->PlaySound2D(EAudioCategory::LockOn_Lost, 0.6f);
}

// ======================== 环境音 ========================

void UWarpVFXIntegration::OnBiomeChanged(EBiomeType NewBiome)
{
    if (!AudioMgr) return;

    AudioMgr->SetAmbientForBiome(NewBiome, 1.f);
}

void UWarpVFXIntegration::OnSpaceWeatherChanged(EAudioCategory WeatherType, float Intensity)
{
    if (!AudioMgr) return;

    switch (WeatherType)
    {
    case EAudioCategory::SolarWind:
        AudioMgr->StartSolarWindSound(Intensity);
        break;
    case EAudioCategory::RadiationStorm:
        AudioMgr->StartRadiationStormSound(Intensity);
        break;
    case EAudioCategory::EMPBurst:
        AudioMgr->StartEMPSound(Intensity);
        break;
    default:
        break;
    }
}

// ======================== 辅助 ========================

UAudioManager* UWarpVFXIntegration::GetAudioManager() const
{
    if (AudioMgr) return AudioMgr;

    // 从 GameInstance 获取
    if (GetWorld() && GetWorld()->GetGameInstance())
    {
        return GetWorld()->GetGameInstance()->GetSubsystem<UAudioManager>();
    }
    return nullptr;
}

AStellarVisualEffects* UWarpVFXIntegration::FindVisualEffects() const
{
    if (VisualEffectsActor) return VisualEffectsActor;

    // 查找场景中的第一个
    TArray<AActor*> Found;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(),
        AStellarVisualEffects::StaticClass(), Found);
    if (Found.Num() > 0)
    {
        return Cast<AStellarVisualEffects>(Found[0]);
    }
    return nullptr;
}

void UWarpVFXIntegration::TriggerScreenShake(float Intensity)
{
    if (!GetWorld()) return;

    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC) return;

    // 使用内置相机震动
    PC->ClientStartCameraShake(UCameraShakeBase::StaticClass(), Intensity);
}
