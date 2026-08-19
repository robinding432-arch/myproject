// ============================================================
// 路径: Source/StellarSystem/Private/Character/PlayerRocketWeapon.cpp
// 作用: 玩家火箭弹实现（RPG/微导弹/蜂群/制导/双发）
// ============================================================

#include "Character/PlayerRocketWeapon.h"
#include "Character/MyCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"
#include "DrawDebugHelpers.h"

UPlayerRocketWeaponComponent::UPlayerRocketWeaponComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
    RocketType = ERocketSubtype::RPG;
    CurrentRocketAmmo = 2;
    bIsReloading = false;
    bRequiresLock = false;
    bLocked = false;
    CurrentLockProgress = 0.f;
    CurrentTarget = nullptr;

    FlightParams.ThrustAcceleration = 60000.f;
    FlightParams.MaxSpeed = 40000.f;
    FlightParams.TurnRate = 60.f;
    FlightParams.FuelDuration = 5.f;
    FlightParams.ArmDistance = 200.f;
    FlightParams.DetonationDelay = 0.05f;
    FlightParams.bHasThrustVectoring = false;
    FlightParams.ThrustVectoringAngle = 15.f;

    Warhead.ExplosionDamage = 150.f;
    Warhead.ExplosionRadius = 500.f;
    Warhead.ArmorPierce = 0.4f;
    Warhead.ShieldDamageMultiplier = 1.0f;
    Warhead.HullDamageMultiplier = 1.2f;
    Warhead.bIsShapedCharge = false;
    Warhead.ShapedChargeArmorBonus = 0.5f;
    Warhead.bIsEMP = false;
    Warhead.EMPRadius = 300.f;
    Warhead.EMPDuration = 4.f;
    Warhead.bIsIncendiary = false;
    Warhead.FireDamagePerSec = 8.f;
    Warhead.FireDuration = 6.f;
    Warhead.bIsFrag = false;
    Warhead.FragCount = 16;
    Warhead.FragSpreadAngle = 45.f;

    Guidance.bHasActiveGuidance = false;
    Guidance.LockTime = 1.0f;
    Guidance.LockConeAngle = 20.f;
    Guidance.MaxLockRange = 25000.f;
    Guidance.bCanLockOnHeat = true;
    Guidance.bCanLockOnRadar = false;
    Guidance.bCanLockOnOptical = false;
    Guidance.bIgnoresFlares = false;
    Guidance.JamResistance = 0.5f;

    SwarmParams.SwarmCount = 4;
    SwarmParams.SwarmSpreadAngle = 10.f;
    SwarmParams.SwarmSeparationDelay = 0.2f;
    SwarmParams.bEachRocketIndependent = true;
    SwarmParams.SwarmRetargetRange = 800.f;

    MagazineSize = 2;
    ReloadTime = 4.f;
    BackblastRadius = 300.f;
    BackblastDamage = 50.f;
    bWarnNearbyAllies = true;
    RocketAmmoID = FName("RocketWarhead");
    RocketTrailColor = FLinearColor(1.f, 0.5f, 0.1f, 1.f);
}

void UPlayerRocketWeaponComponent::BeginPlay()
{
    Super::BeginPlay();
    CurrentRocketAmmo = MagazineSize;
    // RPG 和 DualRocket 不需要锁定
    if (RocketType == ERocketSubtype::GuidedRocket || RocketType == ERocketSubtype::MicroMissile)
    {
        bRequiresLock = true;
        Guidance.bHasActiveGuidance = true;
    }
}

void UPlayerRocketWeaponComponent::TickComponent(float Dt, ELevelTick Tick, FActorComponentTickFunction* Fn)
{
    Super::TickComponent(Dt, Tick, Fn);
    UpdateLocking(Dt);

    if (bWantsToFire && CanFire())
    {
        FireWeapon();
    }
}

void UPlayerRocketWeaponComponent::FireWeapon()
{
    if (!CanFire()) return;

    // 后喷伤害检查
    CheckBackblast();

    FVector Origin = GetOwner()->GetActorLocation() + GetOwner()->GetActorForwardVector() * 80.f;
    FVector Direction = GetOwner()->GetActorForwardVector();

    switch (RocketType)
    {
        case ERocketSubtype::RPG:          ProcessRPGFire(); break;
        case ERocketSubtype::MicroMissile: ProcessMicroMissileFire(); break;
        case ERocketSubtype::SwarmRocket:  ProcessSwarmFire(); break;
        case ERocketSubtype::GuidedRocket: ProcessGuidedFire(CurrentTarget); break;
        case ERocketSubtype::DualRocket:   ProcessDualFire(); break;
    }

    CurrentRocketAmmo = FMath::Max(0, CurrentRocketAmmo - (RocketType == ERocketSubtype::DualRocket ? 2 : 1));
    TimeSinceLastShot = 0.f;
    bLocked = false;
    CurrentLockProgress = 0.f;
    OnWeaponFired.Broadcast();
    OnRocketLaunched.Broadcast(CurrentRocketAmmo);
}

bool UPlayerRocketWeaponComponent::CanFire() const
{
    if (CurrentState == EWeaponState::Reloading) return false;
    if (CurrentRocketAmmo <= 0) return false;
    if (bRequiresLock && !bLocked) return false;
    if (FireInterval > 0.f && TimeSinceLastShot < FireInterval) return false;
    // 双发需要至少 2 发
    if (RocketType == ERocketSubtype::DualRocket && CurrentRocketAmmo < 2) return false;
    return true;
}

void UPlayerRocketWeaponComponent::ProcessRPGFire()
{
    FVector Origin = GetOwner()->GetActorLocation() + GetOwner()->GetActorForwardVector() * 80.f;
    FVector Dir = GetOwner()->GetActorForwardVector();
    SpawnRocketProjectile(Origin, Dir, nullptr, 1.f);
    if (LaunchSound.IsValid())
    {
        UGameplayStatics::PlaySoundAtLocation(this, LaunchSound.Get(), Origin);
    }
}

void UPlayerRocketWeaponComponent::ProcessMicroMissileFire()
{
    if (!CurrentTarget) return;
    FVector Origin = GetOwner()->GetActorLocation() + GetOwner()->GetActorForwardVector() * 80.f;
    SpawnRocketProjectile(Origin, FVector::ZeroVector, CurrentTarget, 0.7f);
}

void UPlayerRocketWeaponComponent::ProcessSwarmFire()
{
    if (!CurrentTarget) return;
    FVector BaseDir = (CurrentTarget->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal();
    for (int32 i = 0; i < SwarmParams.SwarmCount; i++)
    {
        FVector Spread = FMath::VRandCone(BaseDir, FMath::DegreesToRadians(SwarmParams.SwarmSpreadAngle));
        FVector Origin = GetOwner()->GetActorLocation() + Spread * 100.f;
        // 每发独立目标选择
        AActor* IndividualTarget = SwarmParams.bEachRocketIndependent ? nullptr : CurrentTarget;
        SpawnRocketProjectile(Origin, Spread, IndividualTarget, 0.5f);
    }
}

void UPlayerRocketWeaponComponent::ProcessGuidedFire(AActor* Target)
{
    if (!Target) return;
    FVector Origin = GetOwner()->GetActorLocation() + GetOwner()->GetActorForwardVector() * 80.f;
    SpawnRocketProjectile(Origin, FVector::ZeroVector, Target, 1.2f);
}

void UPlayerRocketWeaponComponent::ProcessDualFire()
{
    FVector Forward = GetOwner()->GetActorForwardVector();
    FVector Right = GetOwner()->GetActorRightVector();
    // 左右各一发
    FVector OriginL = GetOwner()->GetActorLocation() + Forward * 80.f - Right * 30.f;
    FVector OriginR = GetOwner()->GetActorLocation() + Forward * 80.f + Right * 30.f;
    SpawnRocketProjectile(OriginL, Forward, nullptr, 1.f);
    SpawnRocketProjectile(OriginR, Forward, nullptr, 1.f);
}

void UPlayerRocketWeaponComponent::SpawnRocketProjectile(const FVector& Origin, const FVector& Direction,
                                                       AActor* HomingTarget, float DamageMult)
{
    float Damage = Warhead.ExplosionDamage * 0.01f * DamageMult;

    // 聚能弹头加成
    if (Warhead.bIsShapedCharge)
    {
        Damage *= (1.f + Warhead.ShapedChargeArmorBonus);
    }

    // 实际项目: SpawnActor<ARocketProjectile>()
    // 设置: HomingTarget, Damage, ExplosionRadius, Warhead params

    if (RocketTrail.IsValid())
    {
        // Attach trail to projectile
    }
}

void UPlayerRocketWeaponComponent::UpdateLocking(float Dt)
{
    if (!bRequiresLock || !CurrentTarget || !GetOwner()) return;

    FVector ToTarget = (CurrentTarget->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal();
    FVector Forward = GetOwner()->GetActorForwardVector();
    float Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(ToTarget, Forward)));
    float Distance = FVector::Dist(GetOwner()->GetActorLocation(), CurrentTarget->GetActorLocation());

    if (Angle <= Guidance.LockConeAngle && Distance <= Guidance.MaxLockRange)
    {
        float JamFactor = bCanBeJammed ? (1.f - Guidance.JamResistance) : 0.f;
        CurrentLockProgress += Dt / FMath::Max(0.1f, Guidance.LockTime * (1.f + JamFactor));
        if (CurrentLockProgress >= 1.f && !bLocked)
        {
            bLocked = true;
            if (LockSound.IsValid())
            {
                UGameplayStatics::PlaySoundAtLocation(this, LockSound.Get(), GetOwner()->GetActorLocation());
            }
            OnRocketLocked.Broadcast(CurrentTarget);
        }
    }
    else
    {
        CurrentLockProgress = FMath::Max(0.f, CurrentLockProgress - Dt * 0.5f);
        bLocked = false;
    }
}

void UPlayerRocketWeaponComponent::AcquireTarget(AActor* Target)
{
    if (!Target) return;
    CurrentTarget = Target;
    if (GetOwner() && GetOwner()->GetLocalRole() == ROLE_AutonomousProxy)
    {
        ServerAcquireTarget(Target);
    }
}

void UPlayerRocketWeaponComponent::ServerAcquireTarget_Implementation(AActor* Target)
{
    CurrentTarget = Target;
}

bool UPlayerRocketWeaponComponent::ServerAcquireTarget_Validate(AActor* Target)
{
    return IsValid(Target);
}

void UPlayerRocketWeaponComponent::ReleaseTarget()
{
    CurrentTarget = nullptr;
    CurrentLockProgress = 0.f;
    bLocked = false;
}

void UPlayerRocketWeaponComponent::FireDualRockets()
{
    if (CurrentRocketAmmo < 2) return;
    ProcessDualFire();
    CurrentRocketAmmo = FMath::Max(0, CurrentRocketAmmo - 2);
    OnRocketLaunched.Broadcast(CurrentRocketAmmo);
}

void UPlayerRocketWeaponComponent::StartReload()
{
    if (bIsReloading) return;
    bIsReloading = true;
    CurrentState = EWeaponState::Reloading;
    GetWorld()->GetTimerManager().SetTimer(ReloadTimerHandle, this,
        &UPlayerRocketWeaponComponent::FinishReload, ReloadTime, false);
}

void UPlayerRocketWeaponComponent::FinishReload()
{
    CurrentRocketAmmo = MagazineSize;
    bIsReloading = false;
    CurrentState = EWeaponState::Idle;
    OnReloadFinished.Broadcast(true);
}

float UPlayerRocketWeaponComponent::GetAmmoPercent() const
{
    return MagazineSize > 0 ? (float)CurrentRocketAmmo / (float)MagazineSize : 0.f;
}

void UPlayerRocketWeaponComponent::CheckBackblast()
{
    if (!GetOwner()) return;
    FVector Backward = -GetOwner()->GetActorForwardVector();
    FVector BlastOrigin = GetOwner()->GetActorLocation() + Backward * 150.f;

    // 检测后喷范围内是否有友军/NPC
    TArray<AActor*> OverlappingActors;
    // SphereOverlapActors with BackblastRadius
    // For each: ApplyDamage(BackblastDamage * 0.5f)

    if (BackblastEffect.IsValid())
    {
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), BackblastEffect.Get(), BlastOrigin);
    }
}

void UPlayerRocketWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(UPlayerRocketWeaponComponent, CurrentRocketAmmo);
    DOREPLIFETIME(UPlayerRocketWeaponComponent, bIsReloading);
    DOREPLIFETIME(UPlayerRocketWeaponComponent, CurrentLockProgress);
    DOREPLIFETIME(UPlayerRocketWeaponComponent, bLocked);
    DOREPLIFETIME(UPlayerRocketWeaponComponent, CurrentTarget);
}
