// ============================================================
// 路径: Source/StellarSystem/Private/Ship/ShipTorpedoWeapon.cpp
// 作用: 飞船鱼雷武器实现
// ============================================================

#include "Ship/ShipTorpedoWeapon.h"
#include "Ship/ShipPawn.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"

UShipTorpedoWeaponComponent::UShipTorpedoWeaponComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
    TorpedoSubtype = EShipTorpedoSubtype::HeavyTorpedo;
    MagazineSize = 4;
    CurrentAmmo = 4;
    ReloadTime = 12.f;
    bIsReloading = false;
    LaunchWarmupTime = 2.f;
    CurrentWarmup = 0.f;
    bIsWarmingUp = false;
    bRequiresLock = true;
    LockTime = 3.f;
    LockConeAngle = 8.f;
    MaxLockRange = 50000.f;
    bCanRetarget = true;
    RetargetInterval = 2.f;
    bIgnoresFlares = false;
    bCanTargetShields = false;
    CurrentLockProgress = 0.f;
    bLocked = false;
    CurrentTarget = nullptr;
    DevastatorChargeTime = 5.f;
    bShowFalloutWarning = true;
    NuclearBlastColor = FLinearColor(1.f, 0.8f, 0.3f, 1.f);
}

void UShipTorpedoWeaponComponent::BeginPlay()
{
    Super::BeginPlay();
    CurrentAmmo = MagazineSize;
}

void UShipTorpedoWeaponComponent::TickComponent(float Dt, ELevelTick Tick, FActorComponentTickFunction* Fn)
{
    Super::TickComponent(Dt, Tick, Fn);
    UpdateWarmup(Dt);
    UpdateLocking(Dt);

    if (bWantsToFire && CanFire())
    {
        FireWeapon(0);
    }
}

void UShipTorpedoWeaponComponent::FireWeapon_Implementation(int32 SlotIndex)
{
    if (!CanFire()) return;
    if (bRequiresLock && !bLocked)
    {
        return;
    }

    float DamageMult = DamageMultiplier;
    if (TorpedoSubtype == EShipTorpedoSubtype::Devastator && CurrentWarmup >= LaunchWarmupTime)
    {
        DamageMult *= DevastatorDamageMultiplier;
    }

    if (CurrentTarget)
    {
        switch (TorpedoSubtype)
        {
            case EShipTorpedoSubtype::HeavyTorpedo:
                ProcessHeavyTorpedoFire(SlotIndex, CurrentTarget);
                break;
            case EShipTorpedoSubtype::Devastator:
                ProcessDevastatorFire(SlotIndex, CurrentTarget);
                break;
            case EShipTorpedoSubtype::NuclearTorpedo:
                ProcessNuclearFire(SlotIndex, CurrentTarget);
                break;
            case EShipTorpedoSubtype::GuidedTorpedo:
                ProcessGuidedFire(SlotIndex, CurrentTarget);
                break;
            case EShipTorpedoSubtype::ShatterTorpedo:
                ProcessShatterFire(SlotIndex, CurrentTarget);
                break;
        }
    }

    CurrentAmmo = FMath::Max(0, CurrentAmmo - 1);
    TimeSinceLastShot = 0.f;
    bLocked = false;
    CurrentLockProgress = 0.f;
    CurrentWarmup = 0.f;
    bIsWarmingUp = false;
    OnWeaponFired.Broadcast(SlotIndex);
}

bool UShipTorpedoWeaponComponent::CanFire() const
{
    if (CurrentAmmo <= 0) return false;
    if (bIsReloading) return false;
    if (FireInterval > 0.f && TimeSinceLastShot < FireInterval) return false;
    if (bRequiresLock && !bLocked) return false;
    if (bIsWarmingUp && CurrentWarmup < LaunchWarmupTime) return false;
    return true;
}

void UShipTorpedoWeaponComponent::ProcessHeavyTorpedoFire(int32 SlotIndex, AActor* Target)
{
    if (!OwnerShip) return;
    FVector Origin = OwnerShip->GetActorLocation() + OwnerShip->GetActorForwardVector() * 600.f;
    float Damage = BaseDamage * Warhead.BaseDamage * 0.01f;
    // Spawn heavy torpedo projectile with homing to Target
    // SpawnActor<AShipTorpedoProjectile>(Origin, Target)
    if (LaunchSound.IsValid())
    {
        UGameplayStatics::PlaySoundAtLocation(this, LaunchSound.Get(), Origin);
    }
}

void UShipTorpedoWeaponComponent::ProcessDevastatorFire(int32 SlotIndex, AActor* Target)
{
    if (!OwnerShip) return;
    FVector Origin = OwnerShip->GetActorLocation() + OwnerShip->GetActorForwardVector() * 600.f;
    float Damage = BaseDamage * Warhead.BaseDamage * 0.01f * DevastatorDamageMultiplier;
    // Spawn devastator torpedo (massive damage, slow)
}

void UShipTorpedoWeaponComponent::ProcessNuclearFire(int32 SlotIndex, AActor* Target)
{
    if (!OwnerShip) return;
    FVector Origin = OwnerShip->GetActorLocation() + OwnerShip->GetActorForwardVector() * 600.f;
    // Spawn nuclear torpedo with massive AoE + radiation
    // On detonation: apply radiation damage over RadiationDuration within RadiationRadius
}

void UShipTorpedoWeaponComponent::ProcessGuidedFire(int32 SlotIndex, AActor* Target)
{
    if (!OwnerShip) return;
    FVector Origin = OwnerShip->GetActorLocation() + OwnerShip->GetActorForwardVector() * 600.f;
    // Spawn smart torpedo with active guidance + retarget capability
}

void UShipTorpedoWeaponComponent::ProcessShatterFire(int32 SlotIndex, AActor* Target)
{
    if (!OwnerShip) return;
    FVector Origin = OwnerShip->GetActorLocation() + OwnerShip->GetActorForwardVector() * 600.f;
    // Spawn shatter torpedo: on hit, reduces target armor by ShatterArmorReduction
    // Then explodes for full damage
}

void UShipTorpedoWeaponComponent::UpdateLocking(float Dt)
{
    if (!bRequiresLock || !CurrentTarget || !OwnerShip) return;

    FVector ToTarget = (CurrentTarget->GetActorLocation() - OwnerShip->GetActorLocation()).GetSafeNormal();
    FVector Forward = OwnerShip->GetActorForwardVector();
    float Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(ToTarget, Forward)));
    float Distance = FVector::Dist(OwnerShip->GetActorLocation(), CurrentTarget->GetActorLocation());

    if (Angle <= LockConeAngle && Distance <= MaxLockRange)
    {
        CurrentLockProgress += Dt / FMath::Max(0.1f, LockTime);
        if (CurrentLockProgress >= 1.f && !bLocked)
        {
            bLocked = true;
            if (LockSound.IsValid())
            {
                UGameplayStatics::PlaySoundAtLocation(this, LockSound.Get(), GetOwner()->GetActorLocation());
            }
        }
    }
    else
    {
        CurrentLockProgress = FMath::Max(0.f, CurrentLockProgress - Dt * 0.3f);
        bLocked = false;
    }
}

void UShipTorpedoWeaponComponent::UpdateWarmup(float Dt)
{
    if (bIsWarmingUp)
    {
        CurrentWarmup = FMath::Min(LaunchWarmupTime, CurrentWarmup + Dt);
        if (CurrentWarmup >= LaunchWarmupTime)
        {
            bIsWarmingUp = false;
            OnWarmupComplete.Broadcast(0);
        }
    }
}

void UShipTorpedoWeaponComponent::StartWarmup()
{
    bIsWarmingUp = true;
    CurrentWarmup = 0.f;
}

float UShipTorpedoWeaponComponent::GetWarmupProgress() const
{
    return LaunchWarmupTime > 0 ? CurrentWarmup / LaunchWarmupTime : 0.f;
}

void UShipTorpedoWeaponComponent::AcquireTarget(AActor* Target)
{
    if (!Target) return;
    CurrentTarget = Target;
    if (GetOwner() && GetOwner()->GetLocalRole() == ROLE_AutonomousProxy)
    {
        ServerAcquireTarget(Target);
    }
}

void UShipTorpedoWeaponComponent::ServerAcquireTarget_Implementation(AActor* Target)
{
    CurrentTarget = Target;
}

bool UShipTorpedoWeaponComponent::ServerAcquireTarget_Validate(AActor* Target)
{
    return IsValid(Target);
}

void UShipTorpedoWeaponComponent::ReleaseTarget()
{
    CurrentTarget = nullptr;
    CurrentLockProgress = 0.f;
    bLocked = false;
}

void UShipTorpedoWeaponComponent::FireWarpTorpedo(AActor* Target)
{
    if (!OwnerShip || !Target) return;
    // Spawn torpedo with micro-warp drive
    // Teleports WarpRange cm toward target after WarpChargeTime
}

void UShipTorpedoWeaponComponent::StartReload()
{
    if (bIsReloading) return;
    bIsReloading = true;
    GetWorld()->GetTimerManager().SetTimer(ReloadTimerHandle, this,
        &UShipTorpedoWeaponComponent::FinishReload, ReloadTime, false);
}

void UShipTorpedoWeaponComponent::FinishReload()
{
    CurrentAmmo = MagazineSize;
    bIsReloading = false;
    OnReloadFinished.Broadcast(true);
}

float UShipTorpedoWeaponComponent::GetAmmoPercent() const
{
    return MagazineSize > 0 ? (float)CurrentAmmo / (float)MagazineSize : 0.f;
}

void UShipTorpedoWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(UShipTorpedoWeaponComponent, CurrentAmmo);
    DOREPLIFETIME(UShipTorpedoWeaponComponent, bIsReloading);
    DOREPLIFETIME(UShipTorpedoWeaponComponent, CurrentLockProgress);
    DOREPLIFETIME(UShipTorpedoWeaponComponent, bLocked);
    DOREPLIFETIME(UShipTorpedoWeaponComponent, CurrentTarget);
    DOREPLIFETIME(UShipTorpedoWeaponComponent, CurrentWarmup);
    DOREPLIFETIME(UShipTorpedoWeaponComponent, bIsWarmingUp);
}
