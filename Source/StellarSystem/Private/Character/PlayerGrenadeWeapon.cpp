// ============================================================
// 路径: Source/StellarSystem/Private/Character/PlayerGrenadeWeapon.cpp
// 作用: 玩家手雷实现（破片/电磁/烟雾/燃烧/冷冻）
// ============================================================

#include "Character/PlayerGrenadeWeapon.h"
#include "Character/MyCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"
#include "DrawDebugHelpers.h"

UPlayerGrenadeWeaponComponent::UPlayerGrenadeWeaponComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
    GrenadeType = EGrenadeSubtype::Frag;
    CurrentCount = 6;
    bIsCooking = false;
    CurrentCookTime = 0.f;
    bIsCharging = false;
    CurrentCharge = 0.f;

    ThrowParams.ThrowForce = 60000.f;
    ThrowParams.MaxThrowForce = 100000.f;
    ThrowParams.ChargeTime = 1.5f;
    ThrowParams.FuseTime = 3.f;
    ThrowParams.bCanCook = true;
    ThrowParams.CookWarningTime = 0.5f;
    ThrowParams.BounceDamping = 0.3f;
    ThrowParams.bSticky = false;

    EffectParams.ExplosionDamage = 80.f;
    EffectParams.ExplosionRadius = 400.f;
    EffectParams.FragmentCount = 24;
    EffectParams.FragmentSpreadAngle = 360.f;
    EffectParams.FragmentRange = 600.f;
    EffectParams.EMPRadius = 500.f;
    EffectParams.EMPDuration = 5.f;
    EffectParams.bDisableShields = true;
    EffectParams.SmokeRadius = 600.f;
    EffectParams.SmokeDuration = 15.f;
    EffectParams.ConcealmentLevel = 0.8f;
    EffectParams.FireDamagePerSec = 10.f;
    EffectParams.FireDuration = 8.f;
    EffectParams.FireSpreadRadius = 200.f;
    EffectParams.bCreatesFireZone = true;
    EffectParams.CryoDamage = 30.f;
    EffectParams.CryoRadius = 350.f;
    EffectParams.CryoDuration = 6.f;
    EffectParams.CryoMoveSlow = 0.5f;
    EffectParams.CryoFireRateSlow = 0.3f;

    GrenadeAmmoID = FName("GrenadeFrag");
    MaxCarryCount = 6;
    GrenadeGlowColor = FLinearColor(1.f, 0.3f, 0.1f, 1.f);
}

void UPlayerGrenadeWeaponComponent::BeginPlay()
{
    Super::BeginPlay();
    CurrentCount = MaxCarryCount;
}

void UPlayerGrenadeWeaponComponent::TickComponent(float Dt, ELevelTick Tick, FActorComponentTickFunction* Fn)
{
    Super::TickComponent(Dt, Tick, Fn);
    UpdateCooking(Dt);
}

void UPlayerGrenadeWeaponComponent::FireWeapon()
{
    if (!CanFire()) return;

    float ChargeRatio = bIsCharging ? FMath::Min(1.f, CurrentCharge / ThrowParams.ChargeTime) : 0.5f;
    float Force = ThrowParams.ThrowForce * (0.5f + 0.5f * ChargeRatio);

    FVector Origin = GetOwner()->GetActorLocation() + GetOwner()->GetActorForwardVector() * 50.f;
    FVector Velocity = GetOwner()->GetActorForwardVector() * Force;

    SpawnGrenadeActor(Origin, Velocity, ThrowParams.FuseTime);

    CurrentCount = FMath::Max(0, CurrentCount - 1);
    bIsCharging = false;
    CurrentCharge = 0.f;
    OnWeaponFired.Broadcast();
}

bool UPlayerGrenadeWeaponComponent::CanFire() const
{
    if (CurrentState == EWeaponState::Reloading) return false;
    if (CurrentCount <= 0) return false;
    return true;
}

void UPlayerGrenadeWeaponComponent::SpawnGrenadeActor(const FVector& Origin, const FVector& Velocity, float FuseTime)
{
    // 实际项目中: SpawnActor<AGrenadeProjectile>()
    // 此处为框架，根据 GrenadeType 设置不同参数

    switch (GrenadeType)
    {
        case EGrenadeSubtype::Frag:       ProcessFragGrenade(); break;
        case EGrenadeSubtype::EMP:        ProcessEMPGrenade(); break;
        case EGrenadeSubtype::Smoke:      ProcessSmokeGrenade(); break;
        case EGrenadeSubtype::Incendiary: ProcessIncendiaryGrenade(); break;
        case EGrenadeSubtype::Cryo:       ProcessCryoGrenade(); break;
    }

    if (ThrowSound.IsValid())
    {
        UGameplayStatics::PlaySoundAtLocation(this, ThrowSound.Get(), Origin);
    }
}

void UPlayerGrenadeWeaponComponent::ProcessFragGrenade()
{
    // 破片飞溅 + 爆炸伤害
    // On detonation: SphereOverlapActors → ApplyDamage to each
}

void UPlayerGrenadeWeaponComponent::ProcessEMPGrenade()
{
    // 电磁脉冲：瘫痪护盾 + 电子设备
    // On detonation: SphereOverlapActors within EMPRadius
    // → Disable shields for EMPDuration
}

void UPlayerGrenadeWeaponComponent::ProcessSmokeGrenade()
{
    // 生成烟雾区域
    // SpawnActor<ASmokeVolume>() with SmokeRadius and SmokeDuration
}

void UPlayerGrenadeWeaponComponent::ProcessIncendiaryGrenade()
{
    // 燃烧区域：持续火焰伤害
    // SpawnActor<AFireZone>() with FireSpreadRadius
    // → ApplyDamage every 0.5s for FireDuration
}

void UPlayerGrenadeWeaponComponent::ProcessCryoGrenade()
{
    // 冷冻：减速 + 持续伤害
    // On detonation: → Apply CryoDamage + SetMoveSpeedModifier(1.f - CryoMoveSlow)
}

void UPlayerGrenadeWeaponComponent::StartChargedThrow()
{
    bIsCharging = true;
    CurrentCharge = 0.f;
}

void UPlayerGrenadeWeaponComponent::ReleaseChargedThrow()
{
    if (CurrentCharge > 0.1f) FireWeapon();
    bIsCharging = false;
    CurrentCharge = 0.f;
}

float UPlayerGrenadeWeaponComponent::GetChargeProgress() const
{
    return ThrowParams.ChargeTime > 0 ? FMath::Min(1.f, CurrentCharge / ThrowParams.ChargeTime) : 0.f;
}

void UPlayerGrenadeWeaponComponent::CookGrenade()
{
    if (!ThrowParams.bCanCook) return;
    bIsCooking = true;
    CurrentCookTime = 0.f;
    GetWorld()->GetTimerManager().SetTimer(CookTimerHandle, this,
        &UPlayerGrenadeWeaponComponent::OnCookDetonate, ThrowParams.FuseTime, false);
}

void UPlayerGrenadeWeaponComponent::OnCookDetonate()
{
    // 在角色脚底爆炸（自伤）
    if (GetOwner())
    {
        FVector Loc = GetOwner()->GetActorLocation();
        // Apply damage to owner
        UGameplayStatics::ApplyDamage(GetOwner(), EffectParams.ExplosionDamage * 0.5f,
            nullptr, GetOwner(), UDamageType::StaticClass());
    }
    bIsCooking = false;
    CurrentCookTime = 0.f;
    CurrentCount = FMath::Max(0, CurrentCount - 1);
}

float UPlayerGrenadeWeaponComponent::GetCookTimeRemaining() const
{
    return FMath::Max(0.f, ThrowParams.FuseTime - CurrentCookTime);
}

void UPlayerGrenadeWeaponComponent::UpdateCooking(float Dt)
{
    if (bIsCooking)
    {
        CurrentCookTime += Dt;
        if (CurrentCookTime >= ThrowParams.FuseTime - ThrowParams.CookWarningTime)
        {
            // 播放警告音
            if (FuseTickSound.IsValid() && !GetWorld()->GetTimerManager().IsTimerActive(CookTimerHandle))
            {
                UGameplayStatics::PlaySoundAtLocation(this, FuseTickSound.Get(),
                    GetOwner()->GetActorLocation());
            }
        }
    }
    if (bIsCharging)
    {
        CurrentCharge = FMath::Min(ThrowParams.ChargeTime, CurrentCharge + Dt);
    }
}

void UPlayerGrenadeWeaponComponent::AddGrenades(int32 Amount)
{
    CurrentCount = FMath::Min(MaxCarryCount, CurrentCount + Amount);
}

float UPlayerGrenadeWeaponComponent::GetGrenadePercent() const
{
    return MaxCarryCount > 0 ? (float)CurrentCount / (float)MaxCarryCount : 0.f;
}

void UPlayerGrenadeWeaponComponent::SwitchGrenadeType(EGrenadeSubtype NewType)
{
    GrenadeType = NewType;
    // 更新对应 AmmoID
    switch (NewType)
    {
        case EGrenadeSubtype::Frag:       GrenadeAmmoID = FName("GrenadeFrag"); break;
        case EGrenadeSubtype::EMP:        GrenadeAmmoID = FName("GrenadeEMP"); break;
        case EGrenadeSubtype::Smoke:      GrenadeAmmoID = FName("GrenadeSmoke"); break;
        case EGrenadeSubtype::Incendiary: GrenadeAmmoID = FName("GrenadeIncendiary"); break;
        case EGrenadeSubtype::Cryo:       GrenadeAmmoID = FName("GrenadeCryo"); break;
    }
}

void UPlayerGrenadeWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(UPlayerGrenadeWeaponComponent, CurrentCount);
    DOREPLIFETIME(UPlayerGrenadeWeaponComponent, bIsCooking);
    DOREPLIFETIME(UPlayerGrenadeWeaponComponent, CurrentCookTime);
}
