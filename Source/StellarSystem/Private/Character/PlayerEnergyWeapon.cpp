// ============================================================
// 路径: Source/StellarSystem/Private/Character/PlayerEnergyWeapon.cpp
// 作用: 玩家能量武器实现（激光手枪/等离子/光束/离子）
// ============================================================

#include "Character/PlayerEnergyWeapon.h"
#include "Character/MyCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"
#include "DrawDebugHelpers.h"

UPlayerEnergyWeaponComponent::UPlayerEnergyWeaponComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
    EnergyType = EEnergySubtype::LaserPistol;
    CurrentEnergy = 100.f;
    CurrentHeat = 0.f;
    bOverheated = false;
    bOvercharging = false;

    CellParams.MaxEnergy = 100.f;
    CellParams.EnergyPerShot = 5.f;
    CellParams.RegenPerSecond = 8.f;
    CellParams.bOverchargeAllowed = true;
    CellParams.OverchargeDrainMultiplier = 3.f;
    CellParams.OverchargeDamageMultiplier = 2.f;
    CellParams.OverchargeHeatMultiplier = 2.5f;
    CellParams.HeatPerShot = 3.f;
    CellParams.HeatDissipationRate = 10.f;
    CellParams.OverheatThreshold = 100.f;
    CellParams.OverheatCooldown = 2.f;

    DamageProfile.ShieldDamageMultiplier = 1.5f;
    DamageProfile.HullDamageMultiplier = 0.7f;
    DamageProfile.HeatDamage = 3.f;
    DamageProfile.ShieldPenetration = 0.2f;
    DamageProfile.bCanDisableShields = false;
    DamageProfile.bCanDisableSystems = false;

    BeamWidth = 2.f;
    bIsInstantHit = true;
    PlasmaProjectileSpeed = 60000.f;
    PlasmaLifetime = 2.f;
    BeamColor = FLinearColor(0.2f, 0.6f, 1.f, 1.f);
}

void UPlayerEnergyWeaponComponent::BeginPlay()
{
    Super::BeginPlay();
    CurrentEnergy = CellParams.MaxEnergy;
}

void UPlayerEnergyWeaponComponent::TickComponent(float Dt, ELevelTick Tick, FActorComponentTickFunction* Fn)
{
    Super::TickComponent(Dt, Tick, Fn);
    UpdateEnergy(Dt);
    UpdateHeat(Dt);
}

void UPlayerEnergyWeaponComponent::FireWeapon()
{
    if (!CanFire()) return;

    float EnergyCost = CellParams.EnergyPerShot;
    if (bOvercharging) EnergyCost *= CellParams.OverchargeDrainMultiplier;

    if (CurrentEnergy < EnergyCost) return;

    CurrentEnergy -= EnergyCost;
    CurrentHeat += CellParams.HeatPerShot;
    if (bOvercharging) CurrentHeat += CellParams.HeatPerShot * (CellParams.OverchargeHeatMultiplier - 1.f);

    switch (EnergyType)
    {
        case EEnergySubtype::LaserPistol:  ProcessLaserFire(); break;
        case EEnergySubtype::PlasmaRifle:  ProcessPlasmaFire(); break;
        case EEnergySubtype::BeamRifle:    ProcessBeamFire(); break;
        case EEnergySubtype::IonBlaster:   ProcessIonFire(); break;
    }

    CurrentMagAmmo = FMath::Max(0, CurrentMagAmmo - 1);
    TimeSinceLastShot = 0.f;
    OnWeaponFired.Broadcast();
}

bool UPlayerEnergyWeaponComponent::CanFire() const
{
    if (!Super::CanFire()) return false;
    float EnergyCost = CellParams.EnergyPerShot;
    if (bOvercharging) EnergyCost *= CellParams.OverchargeDrainMultiplier;
    if (CurrentEnergy < EnergyCost) return false;
    if (CurrentHeat >= CellParams.OverheatThreshold) return false;
    return true;
}

void UPlayerEnergyWeaponComponent::ProcessBeamFire()
{
    if (!GetOwner()) return;
    FVector Origin = GetOwner()->GetActorLocation() + FVector(0, 0, 50.f);
    FVector Direction = GetOwner()->GetActorForwardVector();
    FVector End = Origin + Direction * WeaponData.MaxRange;

    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(GetOwner());

    if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, End, ECC_Pawn, Params))
    {
        if (Hit.GetActor())
        {
            float Damage = WeaponData.BaseDamage * DamageProfile.ShieldDamageMultiplier;
            if (bOvercharging) Damage *= CellParams.OverchargeDamageMultiplier;
            UGameplayStatics::ApplyDamage(Hit.GetActor(), Damage, nullptr, GetOwner(), UDamageType::StaticClass());

            // 过热效果
            if (DamageProfile.bCanDisableSystems && FMath::RandRange(0.f, 1.f) < 0.05f)
            {
                // 瘫痪目标随机子系统
            }
        }
    }

    // Beam 特效
    if (BeamEffect.IsValid())
    {
        FVector BeamEnd = Hit.bBlockingHit ? Hit.Location : End;
        // Spawn beam particle
    }

    // 持续光束：每帧扣能量（Beam 类型）
    if (EnergyType == EEnergySubtype::BeamRifle)
    {
        // Beam 持续消耗
    }
}

void UPlayerEnergyWeaponComponent::ProcessLaserFire()
{
    // 即时命中，高精度
    FVector Origin = GetOwner()->GetActorLocation() + FVector(0, 0, 50.f);
    FVector Direction = GetOwner()->GetActorForwardVector();
    float Spread = bIsAiming ? WeaponData.SpreadBase * 0.3f : WeaponData.SpreadBase;
    Direction = FMath::VRandCone(Direction, FMath::DegreesToRadians(Spread * 0.5f));

    FVector End = Origin + Direction * WeaponData.MaxRange;
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(GetOwner());

    if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, End, ECC_Pawn, Params))
    {
        if (Hit.GetActor())
        {
            float Damage = WeaponData.BaseDamage * 1.1f; // 激光略高伤害
            if (bOvercharging) Damage *= CellParams.OverchargeDamageMultiplier;
            UGameplayStatics::ApplyDamage(Hit.GetActor(), Damage, nullptr, GetOwner(), UDamageType::StaticClass());
        }
    }

    if (MuzzleFlash.IsValid())
    {
        UGameplayStatics::SpawnEmitterAttached(MuzzleFlash.Get(), GetOwner()->GetRootComponent());
    }
}

void UPlayerEnergyWeaponComponent::ProcessPlasmaFire()
{
    // 弹道投射物（非即时命中）
    FVector Origin = GetOwner()->GetActorLocation() + GetOwner()->GetActorForwardVector() * 80.f;
    FVector Direction = GetOwner()->GetActorForwardVector();
    float Spread = bIsAiming ? WeaponData.SpreadBase * 0.4f : WeaponData.SpreadBase * 0.7f;
    Direction = FMath::VRandCone(Direction, FMath::DegreesToRadians(Spread * 0.5f));

    float Damage = WeaponData.BaseDamage * 1.3f; // 等离子高伤害
    if (bOvercharging) Damage *= CellParams.OverchargeDamageMultiplier;

    // Spawn plasma projectile (slow, arcing, explosive on impact)
    // SpawnActor<AProjectile>(Origin, Direction.Rotation())

    if (PlasmaTrail.IsValid())
    {
        // Trail effect on projectile
    }
}

void UPlayerEnergyWeaponComponent::ProcessIonFire()
{
    // 离子冲击：低直接伤害，高系统瘫痪
    FVector Origin = GetOwner()->GetActorLocation() + FVector(0, 0, 50.f);
    FVector Direction = GetOwner()->GetActorForwardVector();
    FVector End = Origin + Direction * WeaponData.MaxRange;

    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(GetOwner());

    if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, End, ECC_Pawn, Params))
    {
        if (Hit.GetActor())
        {
            float Damage = WeaponData.BaseDamage * 0.6f; // 低直接伤害
            UGameplayStatics::ApplyDamage(Hit.GetActor(), Damage, nullptr, GetOwner(), UDamageType::StaticClass());

            // 离子特效：瘫痪护盾/电子设备
            if (DamageProfile.bCanDisableShields)
            {
                // 禁用目标护盾 3 秒
            }
        }
    }
}

void UPlayerEnergyWeaponComponent::UpdateHeat(float Dt)
{
    if (CurrentHeat > 0.f)
    {
        float Dissipation = CellParams.HeatDissipationRate;
        if (bOvercharging) Dissipation *= 0.5f;
        CurrentHeat = FMath::Max(0.f, CurrentHeat - Dissipation * Dt);
    }
    if (CurrentHeat >= CellParams.OverheatThreshold && !bOverheated)
    {
        bOverheated = true;
        CurrentState = EWeaponState::Overheated;
        if (OverheatSound.IsValid())
        {
            UGameplayStatics::PlaySoundAtLocation(this, OverheatSound.Get(), GetOwner()->GetActorLocation());
        }
        GetWorld()->GetTimerManager().SetTimer(OverheatTimerHandle, [this]()
        {
            bOverheated = false;
            CurrentHeat = CellParams.OverheatThreshold * 0.3f;
            CurrentState = EWeaponState::Idle;
        }, CellParams.OverheatCooldown, false);
    }
}

void UPlayerEnergyWeaponComponent::UpdateEnergy(float Dt)
{
    if (CurrentEnergy < CellParams.MaxEnergy)
    {
        float Regen = CellParams.RegenPerSecond;
        if (bOvercharging) Regen *= 0.4f;
        if (bIsFiring) Regen *= 0.5f;
        CurrentEnergy = FMath::Min(CellParams.MaxEnergy, CurrentEnergy + Regen * Dt);
    }
}

void UPlayerEnergyWeaponComponent::RechargeEnergy(float Amount)
{
    CurrentEnergy = FMath::Min(CellParams.MaxEnergy, CurrentEnergy + Amount);
}

float UPlayerEnergyWeaponComponent::GetEnergyPercent() const
{
    return CellParams.MaxEnergy > 0 ? CurrentEnergy / CellParams.MaxEnergy : 0.f;
}

float UPlayerEnergyWeaponComponent::GetHeatPercent() const
{
    return CellParams.OverheatThreshold > 0 ? CurrentHeat / CellParams.OverheatThreshold : 0.f;
}

void UPlayerEnergyWeaponComponent::ToggleOvercharge()
{
    bOvercharging = !bOvercharging;
    if (bOvercharging && !CellParams.bOverchargeAllowed)
    {
        bOvercharging = false; // 不允许过载
    }
    if (RechargeSound.IsValid())
    {
        UGameplayStatics::PlaySoundAtLocation(this, RechargeSound.Get(), GetOwner()->GetActorLocation());
    }
}

void UPlayerEnergyWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(UPlayerEnergyWeaponComponent, CurrentEnergy);
    DOREPLIFETIME(UPlayerEnergyWeaponComponent, CurrentHeat);
    DOREPLIFETIME(UPlayerEnergyWeaponComponent, bOverheated);
    DOREPLIFETIME(UPlayerEnergyWeaponComponent, bOvercharging);
}

void UPlayerEnergyWeaponComponent::ServerSwapEnergyCell_Implementation(float NewMaxEnergy, float NewCurrentEnergy)
{
    CellParams.MaxEnergy = NewMaxEnergy;
    CurrentEnergy = FMath::Min(NewMaxEnergy, NewCurrentEnergy);
}
