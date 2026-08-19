// ============================================================
// 路径: Source/StellarSystem/Private/Ship/ShipEnergyWeapon.cpp
// 作用: 飞船能量武器实现
// ============================================================

#include "Ship/ShipEnergyWeapon.h"
#include "Ship/ShipPawn.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"

UShipEnergyWeaponComponent::UShipEnergyWeaponComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
    EnergySubtype = EShipEnergyWeaponSubtype::Laser;
    EnergyPerShot = 15.f;
    MaxEnergy = 200.f;
    CurrentEnergy = 200.f;
    EnergyRegenPerSecond = 25.f;
    OverheatThreshold = 100.f;
    CurrentHeat = 0.f;
    HeatPerShot = 8.f;
    HeatDissipationRate = 12.f;
    OverheatCooldown = 3.f;
    bOverheated = false;
    BeamWidth = 5.f;
    PulseRadius = 300.f;
    PulseCount = 3;
    bOverchargeMode = false;
    OverchargeDamageMultiplier = 1.5f;
    OverchargeHeatMultiplier = 2.f;
}

void UShipEnergyWeaponComponent::BeginPlay()
{
    Super::BeginPlay();
    CurrentEnergy = MaxEnergy;
}

void UShipEnergyWeaponComponent::TickComponent(float Dt, ELevelTick Tick, FActorComponentTickFunction* Fn)
{
    Super::TickComponent(Dt, Tick, Fn);
    UpdateEnergy(Dt);
    UpdateHeat(Dt);
    if (bWantsToFire && CanFire())
    {
        FireWeapon(0);
    }
}

void UShipEnergyWeaponComponent::FireWeapon_Implementation(int32 SlotIndex)
{
    if (!CanFire()) return;
    if (CurrentEnergy < EnergyPerShot)
    {
        // 能量不足
        return;
    }

    CurrentEnergy -= EnergyPerShot;
    CurrentHeat += HeatPerShot;

    switch (EnergySubtype)
    {
        case EShipEnergyWeaponSubtype::Laser:
            ProcessProjectileFire(SlotIndex);
            break;
        case EShipEnergyWeaponSubtype::Plasma:
            ProcessProjectileFire(SlotIndex); // Plasma 也是弹道
            break;
        case EShipEnergyWeaponSubtype::Beam:
            ProcessBeamFire(SlotIndex);
            break;
        case EShipEnergyWeaponSubtype::Pulse:
            ProcessPulseFire(SlotIndex);
            break;
    }

    TimeSinceLastShot = 0.f;
    OnWeaponFired.Broadcast(SlotIndex);
}

bool UShipEnergyWeaponComponent::CanFire() const
{
    if (bOverheated) return false;
    if (CurrentEnergy < EnergyPerShot) return false;
    if (FireInterval > 0.f && TimeSinceLastShot < FireInterval) return false;
    return true;
}

void UShipEnergyWeaponComponent::ProcessBeamFire(int32 SlotIndex)
{
    if (!OwnerShip) return;
    FVector Origin = OwnerShip->GetActorLocation();
    FVector Forward = OwnerShip->GetActorForwardVector();
    FVector End = Origin + Forward * EffectiveRange;

    // 射线检测
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(OwnerShip);
    bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Origin, End, ECC_Pawn, Params);

    if (bHit && Hit.GetActor())
    {
        float Damage = BaseDamage * DamageMultiplier;
        if (bIsOvercharged) Damage *= OverchargeDamageMultiplier;

        // 对护盾额外伤害
        Damage *= DamageProfile.ShieldDamageMultiplier;

        UGameplayStatics::ApplyDamage(Hit.GetActor(), Damage, OwnerShip->GetController(), OwnerShip, UDamageType::StaticClass());

        // 过热效果
        if (DamageProfile.bCanDisableSystems && FMath::RandRange(0.f, 1.f) < 0.1f)
        {
            // 10% 概率瘫痪目标子系统
        }
    }

    // 生成 Beam 特效
    if (BeamEffect.IsValid())
    {
        FVector BeamEnd = bHit ? Hit.Location : End;
        // Spawn beam particle between Origin and BeamEnd
    }
}

void UShipEnergyWeaponComponent::ProcessProjectileFire(int32 SlotIndex)
{
    if (!OwnerShip) return;
    FVector Origin = OwnerShip->GetActorLocation() + OwnerShip->GetActorForwardVector() * 200.f;
    FVector Direction = OwnerShip->GetActorForwardVector();
    float Damage = BaseDamage * DamageMultiplier;
    if (bIsOvercharged) Damage *= OverchargeDamageMultiplier;

    // Spawn energy projectile actor
    // （实际项目中这里 SpawnActor<AShipProjectile>，此处省略具体类）
}

void UShipEnergyWeaponComponent::ProcessPulseFire(int32 SlotIndex)
{
    if (!OwnerShip) return;
    FVector Origin = OwnerShip->GetActorLocation();
    float Damage = BaseDamage * DamageMultiplier * 0.6f; // 每次脉冲伤害较低
    if (bIsOvercharged) Damage *= OverchargeDamageMultiplier;

    // 范围伤害
    TArray<AActor*> OverlappingActors;
    // 用 SphereOverlapActors 检测 PulseRadius 内敌人
    // 对每个造成伤害
}

void UShipEnergyWeaponComponent::UpdateHeat(float Dt)
{
    if (CurrentHeat > 0.f)
    {
        float Dissipation = HeatDissipationRate;
        if (bIsOvercharged) Dissipation *= 0.5f;
        CurrentHeat = FMath::Max(0.f, CurrentHeat - Dissipation * Dt);
    }
    if (CurrentHeat >= OverheatThreshold && !bOverheated)
    {
        bOverheated = true;
        if (OverheatSound.IsValid())
        {
            UGameplayStatics::PlaySoundAtLocation(this, OverheatSound.Get(), GetOwner()->GetActorLocation());
        }
        GetWorld()->GetTimerManager().SetTimer(OverheatTimerHandle, this,
            &UShipEnergyWeaponComponent::RecoverFromOverheat, OverheatCooldown, false);
    }
}

void UShipEnergyWeaponComponent::RecoverFromOverheat()
{
    bOverheated = false;
    CurrentHeat = OverheatThreshold * 0.3f;
}

void UShipEnergyWeaponComponent::UpdateEnergy(float Dt)
{
    if (CurrentEnergy < MaxEnergy)
    {
        float Regen = EnergyRegenPerSecond;
        if (bIsOvercharged) Regen *= 0.5f;
        CurrentEnergy = FMath::Min(MaxEnergy, CurrentEnergy + Regen * Dt);
    }
}

void UShipEnergyWeaponComponent::RechargeEnergy(float Amount)
{
    CurrentEnergy = FMath::Min(MaxEnergy, CurrentEnergy + Amount);
}

float UShipEnergyWeaponComponent::GetEnergyPercent() const
{
    return MaxEnergy > 0 ? CurrentEnergy / MaxEnergy : 0.f;
}

float UShipEnergyWeaponComponent::GetHeatPercent() const
{
    return OverheatThreshold > 0 ? CurrentHeat / OverheatThreshold : 0.f;
}

void UShipEnergyWeaponComponent::SetOvercharge(bool bEnabled)
{
    bIsOvercharged = bEnabled;
    if (bEnabled)
    {
        DamageMultiplier *= OverchargeDamageMultiplier;
        HeatPerShot *= OverchargeHeatMultiplier;
    }
    else
    {
        DamageMultiplier /= OverchargeDamageMultiplier;
        HeatPerShot /= OverchargeHeatMultiplier;
    }
}

void UShipEnergyWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(UShipEnergyWeaponComponent, CurrentEnergy);
    DOREPLIFETIME(UShipEnergyWeaponComponent, CurrentHeat);
    DOREPLIFETIME(UShipEnergyWeaponComponent, bOverheated);
    DOREPLIFETIME(UShipEnergyWeaponComponent, bIsOvercharged);
}
