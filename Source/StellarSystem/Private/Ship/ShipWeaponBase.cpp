// ============================================================
// 路径: Source/StellarSystem/Private/Ship/ShipWeaponBase.cpp
// 作用: 飞船武器基类实现
// ============================================================

#include "Ship/ShipWeaponBase.h"
#include "Ship/ShipPawn.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"

UShipWeaponBaseComponent::UShipWeaponBaseComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
}

void UShipWeaponBaseComponent::BeginPlay()
{
    Super::BeginPlay();
    OwnerShip = Cast<AShipPawn>(GetOwner());
    CurrentHeat = 0.f;
    CurrentAmmo = MagazineSize;
}

void UShipWeaponBaseComponent::TickComponent(float Dt, ELevelTick Tick, FActorComponentTickFunction* Fn)
{
    Super::TickComponent(Dt, Tick, Fn);
    TimeSinceLastShot += Dt;
    UpdateHeat(Dt);
    if (bWantsToFire && CanFire())
    {
        FireWeapon(0);
    }
}

void UShipWeaponBaseComponent::FireWeapon_Implementation(int32 SlotIndex)
{
    if (!CanFire()) return;
    if (OwnerShip && OwnerShip->GetLocalRole() == ROLE_Authority)
    {
        SpawnProjectile(SlotIndex);
        TimeSinceLastShot = 0.f;
        CurrentAmmo = FMath::Max(0, CurrentAmmo - 1);
        OnWeaponFired.Broadcast(SlotIndex);
    }
}

bool UShipWeaponBaseComponent::CanFire() const
{
    if (CurrentAmmo <= 0) return false;
    if (bIsReloading) return false;
    if (FireInterval > 0.f && TimeSinceLastShot < FireInterval) return false;
    if (CurrentHeat >= OverheatThreshold) return false;
    return true;
}

void UShipWeaponBaseComponent::StartReload()
{
    if (bIsReloading) return;
    bIsReloading = true;
    GetWorld()->GetTimerManager().SetTimer(ReloadTimerHandle, this,
        &UShipWeaponBaseComponent::FinishReload, ReloadTime, false);
}

void UShipWeaponBaseComponent::FinishReload()
{
    CurrentAmmo = MagazineSize;
    bIsReloading = false;
    OnReloadFinished.Broadcast(true);
}

float UShipWeaponBaseComponent::GetAmmoPercent() const
{
    return MagazineSize > 0 ? (float)CurrentAmmo / (float)MagazineSize : 0.f;
}

float UShipWeaponBaseComponent::GetHeatPercent() const
{
    return OverheatThreshold > 0 ? CurrentHeat / OverheatThreshold : 0.f;
}

void UShipWeaponBaseComponent::UpdateHeat(float Dt)
{
    if (CurrentHeat > 0.f)
    {
        float Dissipation = (OwnerShip && bIsOvercharged) ? HeatDissipationRate * 0.5f : HeatDissipationRate;
        CurrentHeat = FMath::Max(0.f, CurrentHeat - Dissipation * Dt);
    }
    if (CurrentHeat >= OverheatThreshold && !bIsOverheated)
    {
        bIsOverheated = true;
        GetWorld()->GetTimerManager().SetTimer(OverheatTimerHandle, this,
            &UShipWeaponBaseComponent::RecoverFromOverheat, OverheatCooldown, false);
    }
}

void UShipWeaponBaseComponent::RecoverFromOverheat()
{
    bIsOverheated = false;
    CurrentHeat = OverheatThreshold * 0.5f;
}

void UShipWeaponBaseComponent::SetOvercharge(bool bEnabled)
{
    bIsOvercharged = bEnabled;
    if (bEnabled)
    {
        DamageMultiplier = OverchargeDamageMultiplier;
        HeatPerShot *= OverchargeHeatMultiplier;
    }
    else
    {
        DamageMultiplier = 1.f;
        HeatPerShot /= OverchargeHeatMultiplier;
    }
}

void UShipWeaponBaseComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(UShipWeaponBaseComponent, CurrentAmmo);
    DOREPLIFETIME(UShipWeaponBaseComponent, CurrentHeat);
    DOREPLIFETIME(UShipWeaponBaseComponent, bIsReloading);
    DOREPLIFETIME(UShipWeaponBaseComponent, bIsOverheated);
}
