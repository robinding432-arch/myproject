// ============================================================
// 路径: Source/StellarSystem/Private/Ship/ShipKineticWeapon.cpp
// 作用: 飞船实弹武器实现
// ============================================================

#include "Ship/ShipKineticWeapon.h"
#include "Ship/ShipPawn.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"

UShipKineticWeaponComponent::UShipKineticWeaponComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
    KineticSubtype = EShipKineticWeaponSubtype::Autocannon;
    MagazineSize = 60;
    CurrentAmmo = 60;
    ReloadTime = 3.f;
    bIsReloading = false;
    MuzzleVelocity = 120000.f;
    GravityEffect = 0.f;
    MaxRange = 80000.f;
    SpreadAngle = 0.5f;
    PelletCount = 1;
    bChargedShot = false;
    ChargeTime = 2.f;
    ChargedDamageMultiplier = 3.f;
    CurrentCharge = 0.f;
    bIsCharging = false;
    AmmoTypeID = FName("ShipAutocannonAmmo");
}

void UShipKineticWeaponComponent::BeginPlay()
{
    Super::BeginPlay();
    CurrentAmmo = MagazineSize;
}

void UShipKineticWeaponComponent::TickComponent(float Dt, ELevelTick Tick, FActorComponentTickFunction* Fn)
{
    Super::TickComponent(Dt, Tick, Fn);
    UpdateHeat(Dt);

    if (bIsCharging)
    {
        CurrentCharge = FMath::Min(1.f, CurrentCharge + Dt / ChargeTime);
    }

    if (bWantsToFire && CanFire())
    {
        FireWeapon(0);
    }
}

void UShipKineticWeaponComponent::FireWeapon_Implementation(int32 SlotIndex)
{
    if (!CanFire()) return;

    float DamageMult = DamageMultiplier;
    if (bIsCharging && CurrentCharge >= 0.95f)
    {
        DamageMult *= ChargedDamageMultiplier;
    }

    FVector Direction = FVector::ForwardVector;
    if (OwnerShip)
    {
        Direction = OwnerShip->GetActorForwardVector();
    }

    switch (KineticSubtype)
    {
        case EShipKineticWeaponSubtype::Autocannon:
            ProcessAutocannonFire(SlotIndex);
            break;
        case EShipKineticWeaponSubtype::Railgun:
            ProcessRailgunFire(SlotIndex);
            break;
        case EShipKineticWeaponSubtype::MassDriver:
            ProcessMassDriverFire(SlotIndex);
            break;
        case EShipKineticWeaponSubtype::Gatling:
            ProcessGatlingFire(SlotIndex);
            break;
    }

    CurrentAmmo = FMath::Max(0, CurrentAmmo - 1);
    TimeSinceLastShot = 0.f;
    CurrentCharge = 0.f;
    bIsCharging = false;
    OnWeaponFired.Broadcast(SlotIndex);
}

bool UShipKineticWeaponComponent::CanFire() const
{
    if (CurrentAmmo <= 0) return false;
    if (bIsReloading) return false;
    if (FireInterval > 0.f && TimeSinceLastShot < FireInterval) return false;
    if (CurrentHeat >= OverheatThreshold) return false;
    return true;
}

void UShipKineticWeaponComponent::ProcessAutocannonFire(int32 SlotIndex)
{
    // 高速连发，中等伤害
    SpawnKineticProjectile(SlotIndex, GetOwner()->GetActorForwardVector(), 1.f);
}

void UShipKineticWeaponComponent::ProcessRailgunFire(int32 SlotIndex)
{
    // 蓄力射击，极高穿甲，单发高伤
    FVector Dir = GetOwner()->GetActorForwardVector();
    SpawnKineticProjectile(SlotIndex, Dir, ChargedDamageMultiplier);
    CurrentHeat += HeatPerShot * 2.f; // 轨道炮发热大
}

void UShipKineticWeaponComponent::ProcessMassDriverFire(int32 SlotIndex)
{
    // 重型弹丸，极大伤害，极低射速
    FVector Dir = GetOwner()->GetActorForwardVector();
    SpawnKineticProjectile(SlotIndex, Dir, 2.5f);
    CurrentHeat += HeatPerShot * 3.f;
}

void UShipKineticWeaponComponent::ProcessGatlingFire(int32 SlotIndex)
{
    // 超高射速，散射，低单发伤害
    FVector BaseDir = GetOwner()->GetActorForwardVector();
    for (int32 i = 0; i < PelletCount; i++)
    {
        FVector Spread = FMath::VRandCone(BaseDir, FMath::DegreesToRadians(SpreadAngle));
        SpawnKineticProjectile(SlotIndex, Spread, 0.6f);
    }
}

void UShipKineticWeaponComponent::SpawnKineticProjectile(int32 SlotIndex, const FVector& Direction, float DamageMult)
{
    if (!OwnerShip) return;

    float Damage = BaseDamage * DamageMult;
    // 应用穿甲
    Damage *= (1.f + DamageProfile.ArmorPierce * 0.5f);

    // Spawn projectile actor
    FVector Origin = OwnerShip->GetActorLocation() + Direction * 300.f;
    // （实际项目中 SpawnActor<AShipProjectile>）
}

void UShipKineticWeaponComponent::StartReload()
{
    if (bIsReloading) return;
    bIsReloading = true;
    if (ReloadSound.IsValid())
    {
        UGameplayStatics::PlaySoundAtLocation(this, ReloadSound.Get(), GetOwner()->GetActorLocation());
    }
    GetWorld()->GetTimerManager().SetTimer(ReloadTimerHandle, this,
        &UShipKineticWeaponComponent::ServerReload, ReloadTime, false);
}

void UShipKineticWeaponComponent::ServerReload_Implementation()
{
    CurrentAmmo = MagazineSize;
    bIsReloading = false;
    OnReloadFinished.Broadcast(true);
}

float UShipKineticWeaponComponent::GetAmmoPercent() const
{
    return MagazineSize > 0 ? (float)CurrentAmmo / (float)MagazineSize : 0.f;
}

void UShipKineticWeaponComponent::StartCharging()
{
    bIsCharging = true;
    CurrentCharge = 0.f;
}

void UShipKineticWeaponComponent::ReleaseChargedShot()
{
    if (CurrentCharge >= 0.5f)
    {
        FireWeapon(0);
    }
    bIsCharging = false;
    CurrentCharge = 0.f;
}

float UShipKineticWeaponComponent::GetChargeProgress() const
{
    return CurrentCharge;
}

void UShipKineticWeaponComponent::UpdateHeat(float Dt)
{
    if (CurrentHeat > 0.f)
    {
        CurrentHeat = FMath::Max(0.f, CurrentHeat - HeatDissipationRate * Dt);
    }
    if (CurrentHeat >= OverheatThreshold && !bIsOverheated)
    {
        bIsOverheated = true;
        GetWorld()->GetTimerManager().SetTimer(OverheatTimerHandle, this,
            &UShipKineticWeaponComponent::RecoverFromOverheat, OverheatCooldown, false);
    }
}

void UShipKineticWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(UShipKineticWeaponComponent, CurrentAmmo);
    DOREPLIFETIME(UShipKineticWeaponComponent, bIsReloading);
    DOREPLIFETIME(UShipKineticWeaponComponent, CurrentCharge);
    DOREPLIFETIME(UShipKineticWeaponComponent, bIsCharging);
}
