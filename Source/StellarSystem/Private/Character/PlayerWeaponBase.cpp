// ============================================================
// 路径: Source/StellarSystem/Private/Character/PlayerWeaponBase.cpp
// 作用: 玩家武器基类实现
// ============================================================

#include "Character/PlayerWeaponBase.h"
#include "Character/MyCharacter.h"
#include "Inventory/AmmoAndConsumables.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"
#include "DrawDebugHelpers.h"

UPlayerWeaponBaseComponent::UPlayerWeaponBaseComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
    CurrentState = EWeaponState::Idle;
    CurrentMagAmmo = 15;
    CurrentReserveAmmo = 90;
    CurrentSpread = 1.0f;
    CurrentCharge = 0.f;
    bIsFiring = false;
    bIsAiming = false;
    TimeSinceLastShot = 0.f;
    FireInterval = 0.2f;
    bWantsToFire = false;
    JamChance = 0.001f;
    ConditionDecayPerShot = 0.05f;
    MinConditionForUse = 10.f;
}

void UPlayerWeaponBaseComponent::BeginPlay()
{
    Super::BeginPlay();
    OwnerCharacter = Cast<AMyCharacter>(GetOwner());
    if (OwnerCharacter)
    {
        AmmoComp = OwnerCharacter->FindComponentByClass<UAmmoInventoryComponent>();
    }
    CurrentMagAmmo = WeaponData.MagazineSize;
    CurrentReserveAmmo = WeaponData.ReserveAmmo;
    CurrentSpread = WeaponData.SpreadBase;
    FireInterval = WeaponData.FireRate > 0.f ? 60.f / WeaponData.FireRate : 0.2f;
}

void UPlayerWeaponBaseComponent::TickComponent(float Dt, ELevelTick Tick, FActorComponentTickFunction* Fn)
{
    Super::TickComponent(Dt, Tick, Fn);
    TimeSinceLastShot += Dt;
    UpdateSpread(Dt);
    ProcessFireRate(Dt);

    if (bWantsToFire && CanFire())
    {
        FireWeapon();
    }
}

void UPlayerWeaponBaseComponent::StartFire()
{
    bWantsToFire = true;
    bIsFiring = true;
    if (WeaponData.FireMode == EFireMode::SemiAuto && CanFire())
    {
        FireWeapon();
        bWantsToFire = false; // Semi-auto 只发一发
    }
}

void UPlayerWeaponBaseComponent::StopFire()
{
    bWantsToFire = false;
    bIsFiring = false;
}

void UPlayerWeaponBaseComponent::FireWeapon()
{
    if (!CanFire()) return;

    // 卡壳检查
    if (FMath::RandRange(0.f, 1.f) < JamChance * (WeaponData.Condition * 0.01f))
    {
        CurrentState = EWeaponState::Jammed;
        OnWeaponJammed.Broadcast(WeaponData.WeaponID);
        return;
    }

    // 消耗弹药
    if (GetOwner() && GetOwner()->GetLocalRole() == ROLE_Authority)
    {
        CurrentMagAmmo = FMath::Max(0, CurrentMagAmmo - 1);
        WeaponData.Condition = FMath::Max(0.f, WeaponData.Condition - ConditionDecayPerShot);
    }

    // 计算散射
    float Spread = CurrentSpread;
    if (bIsAiming) Spread *= 0.4f;

    // 应用配件修正
    for (const auto& Pair : WeaponData.InstalledAttachments)
    {
        Spread += Pair.Value.AccuracyModifier;
    }
    Spread = FMath::Max(0.05f, Spread);

    FVector Origin = GetOwner()->GetActorLocation() + FVector(0, 0, 50.f);
    FVector Direction = GetOwner()->GetActorForwardVector();
    if (Spread > 0.1f)
    {
        Direction = FMath::VRandCone(Direction, FMath::DegreesToRadians(Spread * 0.5f));
    }

    float Damage = CalculateDamage(0.f); // 默认近距离
    SpawnProjectile(Origin, Direction, Damage);

    // 后坐力
    ApplyRecoil();

    // 更新散射
    CurrentSpread = FMath::Min(WeaponData.SpreadMax, CurrentSpread + WeaponData.SpreadGrowthPerShot);

    // 特效/音效
    if (WeaponData.MuzzleFlash.IsValid())
    {
        UGameplayStatics::SpawnEmitterAttached(WeaponData.MuzzleFlash.Get(), GetOwner()->GetRootComponent());
    }
    if (WeaponData.FireSound.IsValid())
    {
        UGameplayStatics::PlaySoundAtLocation(this, WeaponData.FireSound.Get(), Origin);
    }

    TimeSinceLastShot = 0.f;
    OnWeaponFired.Broadcast();

    // 检查空仓
    if (CurrentMagAmmo <= 0)
    {
        CurrentState = EWeaponState::OutOfAmmo;
        OnOutOfAmmo.Broadcast(WeaponData.WeaponID);
        if (WeaponData.DryFireSound.IsValid())
        {
            UGameplayStatics::PlaySoundAtLocation(this, WeaponData.DryFireSound.Get(), Origin);
        }
    }
}

bool UPlayerWeaponBaseComponent::CanFire() const
{
    if (CurrentState == EWeaponState::Reloading) return false;
    if (CurrentState == EWeaponState::Jammed) return false;
    if (CurrentState == EWeaponState::Overheated) return false;
    if (CurrentMagAmmo <= 0) return false;
    if (WeaponData.Condition < WeaponData.MinConditionForUse) return false;
    if (FireInterval > 0.f && TimeSinceLastShot < FireInterval) return false;
    return true;
}

void UPlayerWeaponBaseComponent::StartReload()
{
    if (CurrentState == EWeaponState::Reloading) return;
    if (CurrentMagAmmo >= WeaponData.MagazineSize) return;
    if (CurrentReserveAmmo <= 0) return;

    CurrentState = EWeaponState::Reloading;
    if (WeaponData.ReloadSound.IsValid())
    {
        UGameplayStatics::PlaySoundAtLocation(this, WeaponData.ReloadSound.Get(), GetOwner()->GetActorLocation());
    }

    if (GetOwner() && GetOwner()->GetLocalRole() == ROLE_Authority)
    {
        GetWorld()->GetTimerManager().SetTimer(ReloadTimerHandle, this,
            &UPlayerWeaponBaseComponent::FinishReload, WeaponData.ReloadTime, false);
    }
    else if (GetOwner())
    {
        ServerStartReload();
    }
}

void UPlayerWeaponBaseComponent::ServerStartReload_Implementation()
{
    StartReload();
}

bool UPlayerWeaponBaseComponent::ServerStartReload_Validate()
{
    return true;
}

void UPlayerWeaponBaseComponent::FinishReload()
{
    int32 Needed = WeaponData.MagazineSize - CurrentMagAmmo;
    int32 FromReserve = FMath::Min(Needed, CurrentReserveAmmo);
    CurrentMagAmmo += FromReserve;
    CurrentReserveAmmo -= FromReserve;
    CurrentState = EWeaponState::Idle;
    CurrentSpread = WeaponData.SpreadBase;
    OnReloadFinished.Broadcast(true);
}

void UPlayerWeaponBaseComponent::SetAiming(bool bAiming)
{
    bIsAiming = bAiming;
}

void UPlayerWeaponBaseComponent::StartCharging()
{
    if (!WeaponData.bHasChargeShot) return;
    CurrentState = EWeaponState::Charging;
    CurrentCharge = 0.f;
}

void UPlayerWeaponBaseComponent::ReleaseCharge()
{
    if (CurrentState != EWeaponState::Charging) return;
    float ChargeRatio = CurrentCharge / WeaponData.MaxChargeTime;
    float DamageMult = 1.f + (WeaponData.ChargeDamageMultiplier - 1.f) * ChargeRatio;
    FVector Origin = GetOwner()->GetActorLocation() + FVector(0, 0, 50.f);
    FVector Direction = GetOwner()->GetActorForwardVector();
    SpawnProjectile(Origin, Direction, GetEffectiveDamage() * DamageMult);
    CurrentCharge = 0.f;
    CurrentState = EWeaponState::Idle;
}

float UPlayerWeaponBaseComponent::GetChargeProgress() const
{
    return WeaponData.MaxChargeTime > 0 ? CurrentCharge / WeaponData.MaxChargeTime : 0.f;
}

bool UPlayerWeaponBaseComponent::InstallAttachment(const FWeaponAttachment& Attachment)
{
    if (WeaponData.AvailableSlots.Contains(Attachment.Slot))
    {
        WeaponData.InstalledAttachments.Add(Attachment.Slot, Attachment);
        return true;
    }
    return false;
}

bool UPlayerWeaponBaseComponent::RemoveAttachment(EWeaponAttachmentSlot Slot)
{
    return WeaponData.InstalledAttachments.Remove(Slot) > 0;
}

FWeaponAttachment UPlayerWeaponBaseComponent::GetInstalledAttachment(EWeaponAttachmentSlot Slot) const
{
    if (WeaponData.InstalledAttachments.Contains(Slot))
    {
        return WeaponData.InstalledAttachments[Slot];
    }
    return FWeaponAttachment();
}

float UPlayerWeaponBaseComponent::CalculateDamage(float Distance) const
{
    float Damage = WeaponData.BaseDamage;
    // 距离衰减
    float DropoffStart = WeaponData.EffectiveRange * 0.7f;
    if (Distance > DropoffStart)
    {
        float T = (Distance - DropoffStart) / FMath::Max(1.f, WeaponData.MaxRange - DropoffStart);
        T = FMath::Clamp(T, 0.f, 1.f);
        Damage *= (1.f - 0.5f * T); // 最远衰减 50%
    }
    // 暴击
    if (FMath::RandRange(0.f, 1.f) < WeaponData.CriticalChance)
    {
        Damage *= WeaponData.CriticalMultiplier;
    }
    // 配件
    for (const auto& Pair : WeaponData.InstalledAttachments)
    {
        Damage += Pair.Value.DamageModifier;
    }
    // 品质
    Damage *= (1.f + (WeaponData.QualityLevel - 1) * 0.1f);
    return Damage;
}

float UPlayerWeaponBaseComponent::GetEffectiveDamage() const
{
    float Dmg = WeaponData.BaseDamage;
    Dmg *= (1.f + (WeaponData.QualityLevel - 1) * 0.1f);
    if (WeaponData.bIsIncendiary) Dmg *= 1.15f;
    if (WeaponData.bIsExplosive) Dmg *= 1.2f;
    return Dmg;
}

void UPlayerWeaponBaseComponent::SpawnProjectile(const FVector& Origin, const FVector& Direction, float Damage)
{
    // 实际项目中 SpawnActor<AProjectile>()
    // 此处为框架实现
}

void UPlayerWeaponBaseComponent::ApplyRecoil()
{
    float Recoil = WeaponData.RecoilKick;
    for (const auto& Pair : WeaponData.InstalledAttachments)
    {
        Recoil += Pair.Value.RecoilModifier;
    }
    Recoil = FMath::Max(0.f, Recoil);
    // 应用到控制器旋转（实际项目中 Cast<APlayerController>()）
}

void UPlayerWeaponBaseComponent::UpdateSpread(float Dt)
{
    if (!bIsFiring && CurrentSpread > WeaponData.SpreadBase)
    {
        CurrentSpread = FMath::Max(WeaponData.SpreadBase, CurrentSpread - WeaponData.SpreadRecoveryPerSec * Dt);
    }
}

void UPlayerWeaponBaseComponent::ProcessFireRate(float Dt)
{
    if (CurrentState == EWeaponState::Charging)
    {
        CurrentCharge = FMath::Min(WeaponData.MaxChargeTime, CurrentCharge + Dt);
    }
}

void UPlayerWeaponBaseComponent::ServerFire_Implementation(FVector Origin, FVector Direction)
{
    // Server validates and applies damage
    // Raycast from Origin in Direction
}

bool UPlayerWeaponBaseComponent::ServerFire_Validate(FVector Origin, FVector Direction)
{
    return Direction.IsNormalized() && Origin != FVector::ZeroVector;
}

void UPlayerWeaponBaseComponent::ServerConsumeAmmo_Implementation(int32 Amount)
{
    CurrentMagAmmo = FMath::Max(0, CurrentMagAmmo - Amount);
}

bool UPlayerWeaponBaseComponent::ServerConsumeAmmo_Validate(int32 Amount)
{
    return Amount > 0 && Amount <= WeaponData.MagazineSize;
}

void UPlayerWeaponBaseComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(UPlayerWeaponBaseComponent, CurrentState);
    DOREPLIFETIME(UPlayerWeaponBaseComponent, CurrentMagAmmo);
    DOREPLIFETIME(UPlayerWeaponBaseComponent, CurrentReserveAmmo);
    DOREPLIFETIME(UPlayerWeaponBaseComponent, CurrentSpread);
    DOREPLIFETIME(UPlayerWeaponBaseComponent, CurrentCharge);
    DOREPLIFETIME(UPlayerWeaponBaseComponent, bIsFiring);
    DOREPLIFETIME(UPlayerWeaponBaseComponent, bIsAiming);
}
