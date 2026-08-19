// ============================================================
// 路径: Source/StellarSystem/Private/Ship/ShipMissileWeapon.cpp
// 作用: 飞船导弹武器实现
// ============================================================

#include "Ship/ShipMissileWeapon.h"
#include "Ship/ShipPawn.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"

UShipMissileWeaponComponent::UShipMissileWeaponComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
    MissileSubtype = EShipMissileSubtype::Heatseeker;
    MagazineSize = 12;
    CurrentAmmo = 12;
    ReloadTime = 5.f;
    bIsReloading = false;
    bRequiresLock = true;
    LockTime = 1.5f;
    LockConeAngle = 15.f;
    MaxLockRange = 30000.f;
    bCanLockOnDecoy = false;
    CurrentLockProgress = 0.f;
    bLocked = false;
    CurrentTarget = nullptr;
    SwarmCount = 4;
    SwarmSpreadAngle = 8.f;
    ClusterBurstCount = 6;
    ClusterBurstDistance = 2000.f;
    FlakAirBurstRadius = 800.f;
    FlakProximityFuzeRange = 300.f;
    bCanBeJammed = true;
    JamResistance = 0.5f;
}

void UShipMissileWeaponComponent::BeginPlay()
{
    Super::BeginPlay();
    CurrentAmmo = MagazineSize;
}

void UShipMissileWeaponComponent::TickComponent(float Dt, ELevelTick Tick, FActorComponentTickFunction* Fn)
{
    Super::TickComponent(Dt, Tick, Fn);
    UpdateLocking(Dt);
    if (bWantsToFire && CanFire())
    {
        FireWeapon(0);
    }
}

void UShipMissileWeaponComponent::FireWeapon_Implementation(int32 SlotIndex)
{
    if (!CanFire()) return;
    if (bRequiresLock && !bLocked)
    {
        return; // 需要锁定但未锁定
    }

    switch (MissileSubtype)
    {
        case EShipMissileSubtype::Heatseeker:
            ProcessHeatseekerFire(SlotIndex);
            break;
        case EShipMissileSubtype::Swarm:
            ProcessSwarmFire(SlotIndex);
            break;
        case EShipMissileSubtype::Cluster:
            ProcessClusterFire(SlotIndex);
            break;
        case EShipMissileSubtype::Dumbfire:
            ProcessDumbfireFire(SlotIndex);
            break;
        case EShipMissileSubtype::Flak:
            ProcessFlakFire(SlotIndex);
            break;
    }

    CurrentAmmo = FMath::Max(0, CurrentAmmo - 1);
    TimeSinceLastShot = 0.f;
    bLocked = false;
    CurrentLockProgress = 0.f;
    OnWeaponFired.Broadcast(SlotIndex);
}

bool UShipMissileWeaponComponent::CanFire() const
{
    if (CurrentAmmo <= 0) return false;
    if (bIsReloading) return false;
    if (FireInterval > 0.f && TimeSinceLastShot < FireInterval) return false;
    if (bRequiresLock && !bLocked) return false;
    return true;
}

void UShipMissileWeaponComponent::ProcessHeatseekerFire(int32 SlotIndex)
{
    if (!OwnerShip || !CurrentTarget) return;
    FVector Origin = OwnerShip->GetActorLocation() + OwnerShip->GetActorForwardVector() * 400.f;
    // Spawn heatseeker missile with homing target
    // SpawnActor<AShipMissile>(Origin, CurrentTarget)
}

void UShipMissileWeaponComponent::ProcessSwarmFire(int32 SlotIndex)
{
    if (!OwnerShip || !CurrentTarget) return;
    FVector BaseDir = (CurrentTarget->GetActorLocation() - OwnerShip->GetActorLocation()).GetSafeNormal();
    for (int32 i = 0; i < SwarmCount; i++)
    {
        FVector Spread = FMath::VRandCone(BaseDir, FMath::DegreesToRadians(SwarmSpreadAngle));
        // Spawn individual swarm missile with slight direction variation
    }
}

void UShipMissileWeaponComponent::ProcessClusterFire(int32 SlotIndex)
{
    if (!OwnerShip || !CurrentTarget) return;
    FVector Origin = OwnerShip->GetActorLocation() + OwnerShip->GetActorForwardVector() * 400.f;
    // Spawn cluster missile that bursts into ClusterBurstCount fragments at ClusterBurstDistance
}

void UShipMissileWeaponComponent::ProcessDumbfireFire(int32 SlotIndex)
{
    if (!OwnerShip) return;
    FVector Origin = OwnerShip->GetActorLocation() + OwnerShip->GetActorForwardVector() * 400.f;
    FVector Dir = OwnerShip->GetActorForwardVector();
    // Spawn dumb-fire missile (straight line, no guidance)
}

void UShipMissileWeaponComponent::ProcessFlakFire(int32 SlotIndex)
{
    if (!OwnerShip) return;
    // Flak: timed fuse, air-burst at FlakAirBurstRadius
    // Proximity fuze triggers at FlakProximityFuzeRange from target
}

void UShipMissileWeaponComponent::UpdateLocking(float Dt)
{
    if (!bRequiresLock || !CurrentTarget || !OwnerShip) return;

    // Check if target is in lock cone
    FVector ToTarget = (CurrentTarget->GetActorLocation() - OwnerShip->GetActorLocation()).GetSafeNormal();
    FVector Forward = OwnerShip->GetActorForwardVector();
    float Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(ToTarget, Forward)));

    float Distance = FVector::Dist(OwnerShip->GetActorLocation(), CurrentTarget->GetActorLocation());

    if (Angle <= LockConeAngle && Distance <= MaxLockRange)
    {
        float JamFactor = bCanBeJammed ? (1.f - JamResistance) : 0.f;
        CurrentLockProgress += Dt / FMath::Max(0.1f, LockTime * (1.f + JamFactor));
        if (CurrentLockProgress >= 1.f)
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
        CurrentLockProgress = FMath::Max(0.f, CurrentLockProgress - Dt * 0.5f);
        bLocked = false;
    }
}

void UShipMissileWeaponComponent::AcquireTarget(AActor* Target)
{
    if (!Target) return;
    CurrentTarget = Target;
    if (GetOwner() && GetOwner()->GetLocalRole() == ROLE_AutonomousProxy)
    {
        ServerAcquireTarget(Target);
    }
}

void UShipMissileWeaponComponent::ServerAcquireTarget_Implementation(AActor* Target)
{
    CurrentTarget = Target;
}

bool UShipMissileWeaponComponent::ServerAcquireTarget_Validate(AActor* Target)
{
    return IsValid(Target);
}

void UShipMissileWeaponComponent::ReleaseTarget()
{
    CurrentTarget = nullptr;
    CurrentLockProgress = 0.f;
    bLocked = false;
}

void UShipMissileWeaponComponent::StartReload()
{
    if (bIsReloading) return;
    bIsReloading = true;
    GetWorld()->GetTimerManager().SetTimer(ReloadTimerHandle, this,
        &UShipMissileWeaponComponent::FinishReload, ReloadTime, false);
}

void UShipMissileWeaponComponent::FinishReload()
{
    CurrentAmmo = MagazineSize;
    bIsReloading = false;
    OnReloadFinished.Broadcast(true);
}

float UShipMissileWeaponComponent::GetAmmoPercent() const
{
    return MagazineSize > 0 ? (float)CurrentAmmo / (float)MagazineSize : 0.f;
}

void UShipMissileWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(UShipMissileWeaponComponent, CurrentAmmo);
    DOREPLIFETIME(UShipMissileWeaponComponent, bIsReloading);
    DOREPLIFETIME(UShipMissileWeaponComponent, CurrentLockProgress);
    DOREPLIFETIME(UShipMissileWeaponComponent, bLocked);
    DOREPLIFETIME(UShipMissileWeaponComponent, CurrentTarget);
}
