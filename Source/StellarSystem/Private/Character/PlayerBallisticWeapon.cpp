// ============================================================
// 路径: Source/StellarSystem/Private/Character/PlayerBallisticWeapon.cpp
// 作用: 玩家实弹武器实现（手枪/冲锋枪/步枪/狙击/霰弹/机枪）
// ============================================================

#include "Character/PlayerBallisticWeapon.h"
#include "Character/MyCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"
#include "DrawDebugHelpers.h"

UPlayerBallisticWeaponComponent::UPlayerBallisticWeaponComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
    BallisticType = EBallisticSubtype::Pistol;
    CurrentHeat = 0.f;
    BurstRemaining = 0;

    // 默认弹道
    ProjectileParams.Caliber = 9.f;
    ProjectileParams.ProjectileMass = 0.008f;
    ProjectileParams.DragCoefficient = 0.3f;
    ProjectileParams.GravityEffect = 981.f;
    ProjectileParams.StabilityFactor = 0.85f;
    ProjectileParams.bIsSubsonic = false;
    ProjectileParams.SoundSuppression = 0.f;

    // 默认特殊弹药
    SpecialAmmo.bHollowPoint = false;
    SpecialAmmo.bArmorPiercing = false;
    SpecialAmmo.bTracer = false;
    SpecialAmmo.bIncendiary = false;
    SpecialAmmo.bExplosiveTip = false;

    // 霰弹
    ShotgunPelletCount = 8;
    ShotgunSpread = 12.f;
    ShotgunRange = 1500.f;

    // 狙击
    bHasSuppressor = false;
    SuppressorSoundReduction = 0.7f;
    bHasBipod = false;
    BipodAccuracyBonus = 0.8f;

    // 机枪
    bHasBeltFeed = false;
    BeltCapacity = 100;
    OverheatThreshold = 60.f;
}

void UPlayerBallisticWeaponComponent::BeginPlay()
{
    Super::BeginPlay();
    // 根据子类类型设置默认值
    switch (BallisticType)
    {
        case EBallisticSubtype::Pistol:
            WeaponData.FireRate = 400.f; WeaponData.MagazineSize = 15; WeaponData.ReserveAmmo = 90;
            WeaponData.BaseDamage = 22.f; WeaponData.SpreadBase = 1.5f; WeaponData.MuzzleVelocity = 70000.f;
            break;
        case EBallisticSubtype::SMG:
            WeaponData.FireRate = 800.f; WeaponData.MagazineSize = 30; WeaponData.ReserveAmmo = 180;
            WeaponData.BaseDamage = 18.f; WeaponData.SpreadBase = 2.5f; WeaponData.MuzzleVelocity = 65000.f;
            break;
        case EBallisticSubtype::AssaultRifle:
            WeaponData.FireRate = 600.f; WeaponData.MagazineSize = 30; WeaponData.ReserveAmmo = 180;
            WeaponData.BaseDamage = 28.f; WeaponData.SpreadBase = 1.8f; WeaponData.MuzzleVelocity = 85000.f;
            break;
        case EBallisticSubtype::SniperRifle:
            WeaponData.FireRate = 40.f; WeaponData.MagazineSize = 5; WeaponData.ReserveAmmo = 30;
            WeaponData.BaseDamage = 90.f; WeaponData.SpreadBase = 0.15f; WeaponData.MuzzleVelocity = 120000.f;
            WeaponData.EffectiveRange = 60000.f; WeaponData.MaxRange = 100000.f;
            break;
        case EBallisticSubtype::Shotgun:
            WeaponData.FireRate = 80.f; WeaponData.MagazineSize = 8; WeaponData.ReserveAmmo = 48;
            WeaponData.BaseDamage = 12.f; WeaponData.SpreadBase = 8.f; WeaponData.MuzzleVelocity = 40000.f;
            WeaponData.EffectiveRange = 1500.f;
            break;
        case EBallisticSubtype::LMG:
            WeaponData.FireRate = 750.f; WeaponData.MagazineSize = 100; WeaponData.ReserveAmmo = 300;
            WeaponData.BaseDamage = 25.f; WeaponData.SpreadBase = 3.0f; WeaponData.MuzzleVelocity = 80000.f;
            break;
    }
    CurrentMagAmmo = WeaponData.MagazineSize;
    CurrentReserveAmmo = WeaponData.ReserveAmmo;
    FireInterval = WeaponData.FireRate > 0.f ? 60.f / WeaponData.FireRate : 0.2f;
}

void UPlayerBallisticWeaponComponent::TickComponent(float Dt, ELevelTick Tick, FActorComponentTickFunction* Fn)
{
    Super::TickComponent(Dt, Tick, Fn);
    UpdateHeat(Dt);
}

void UPlayerBallisticWeaponComponent::FireWeapon()
{
    if (!CanFire()) return;

    float DamageMult = 1.f;

    // 特殊弹药修正
    if (SpecialAmmo.bHollowPoint)
    {
        DamageMult *= (1.f + SpecialAmmo.HollowPointDamageBonus);
        WeaponData.ArmorPierce *= (1.f - SpecialAmmo.HollowPointArmorPenalty);
    }
    if (SpecialAmmo.bArmorPiercing)
    {
        DamageMult *= (1.f - SpecialAmmo.APPenaltyDamage);
        WeaponData.ArmorPierce *= (1.f + SpecialAmmo.APPenaltyArmorBonus);
    }

    switch (BallisticType)
    {
        case EBallisticSubtype::Pistol:       FirePistolShot(); break;
        case EBallisticSubtype::SMG:          FireSMGBurst(); break;
        case EBallisticSubtype::AssaultRifle: FireRifleShot(); break;
        case EBallisticSubtype::SniperRifle:  FireSniperShot(); break;
        case EBallisticSubtype::Shotgun:      FireShotgunShell(); break;
        case EBallisticSubtype::LMG:          FireLMGBurst(); break;
    }

    // 燃烧弹效果
    if (SpecialAmmo.bIncendiary)
    {
        // 点燃目标（在 SpawnBallisticProjectile 内处理）
    }

    CurrentMagAmmo = FMath::Max(0, CurrentMagAmmo - 1);
    TimeSinceLastShot = 0.f;
    OnWeaponFired.Broadcast();
}

bool UPlayerBallisticWeaponComponent::CanFire() const
{
    if (!Super::CanFire()) return false;
    if (CurrentHeat >= OverheatThreshold) return false;
    return true;
}

void UPlayerBallisticWeaponComponent::FirePistolShot()
{
    FVector Dir = GetOwner()->GetActorForwardVector();
    SpawnBallisticProjectile(Dir, 1.f);
    CurrentHeat += 1.f;
}

void UPlayerBallisticWeaponComponent::FireSMGBurst()
{
    FVector BaseDir = GetOwner()->GetActorForwardVector();
    int32 Pellets = WeaponData.PelletCount > 1 ? WeaponData.PelletCount : 3;
    for (int32 i = 0; i < Pellets; i++)
    {
        FVector Spread = FMath::VRandCone(BaseDir, FMath::DegreesToRadians(WeaponData.SpreadBase * 0.5f));
        SpawnBallisticProjectile(Spread, 0.8f);
    }
    CurrentHeat += 3.f;
}

void UPlayerBallisticWeaponComponent::FireRifleShot()
{
    FVector Dir = GetOwner()->GetActorForwardVector();
    float Accuracy = bIsAiming ? WeaponData.SpreadBase * 0.3f : WeaponData.SpreadBase;
    Dir = FMath::VRandCone(Dir, FMath::DegreesToRadians(Accuracy * 0.5f));
    SpawnBallisticProjectile(Dir, 1.f);
    CurrentHeat += 2.f;
}

void UPlayerBallisticWeaponComponent::FireSniperShot()
{
    FVector Dir = GetOwner()->GetActorForwardVector();
    float Accuracy = bIsAiming ? WeaponData.SpreadBase * 0.2f : WeaponData.SpreadBase * 0.8f;
    if (bHasBipod && bIsAiming) Accuracy *= BipodAccuracyBonus;
    Dir = FMath::VRandCone(Dir, FMath::DegreesToRadians(Accuracy * 0.5f));
    SpawnBallisticProjectile(Dir, 1.5f); // 狙击伤害倍率
    CurrentHeat += 5.f;
}

void UPlayerBallisticWeaponComponent::FireShotgunShell()
{
    FVector BaseDir = GetOwner()->GetActorForwardVector();
    for (int32 i = 0; i < ShotgunPelletCount; i++)
    {
        FVector Spread = FMath::VRandCone(BaseDir, FMath::DegreesToRadians(ShotgunSpread * 0.5f));
        SpawnBallisticProjectile(Spread, 1.f);
    }
    CurrentHeat += 4.f;
}

void UPlayerBallisticWeaponComponent::FireLMGBurst()
{
    FVector BaseDir = GetOwner()->GetActorForwardVector();
    FVector Spread = FMath::VRandCone(BaseDir, FMath::DegreesToRadians(WeaponData.SpreadBase * 0.5f));
    SpawnBallisticProjectile(Spread, 0.9f);
    CurrentHeat += 3.5f;
    if (CurrentHeat >= OverheatThreshold * 0.8f)
    {
        // 过热警告
    }
}

void UPlayerBallisticWeaponComponent::SpawnBallisticProjectile(const FVector& Direction, float DamageMult)
{
    if (!GetOwner()) return;
    FVector Origin = GetOwner()->GetActorLocation() + Direction * 100.f + FVector(0, 0, 50.f);
    float Damage = WeaponData.BaseDamage * DamageMult;

    // 射线检测（即时命中，高速弹丸）
    FVector End = Origin + Direction * WeaponData.MaxRange;
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(GetOwner());

    if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, End, ECC_Pawn, Params))
    {
        if (Hit.GetActor())
        {
            float Distance = FVector::Dist(Origin, Hit.Location);
            Damage = CalculateDamage(Distance) * DamageMult;
            UGameplayStatics::ApplyPointDamage(Hit.GetActor(), Damage, Direction, Hit,
                GetOwner()->GetInstigatorController(), GetOwner(), UDamageType::StaticClass());
        }
    }

    // 曳光弹特效
    if (SpecialAmmo.bTracer && WeaponData.TracerEffect.IsValid())
    {
        // Spawn tracer particle
    }
}

FVector UPlayerBallisticWeaponComponent::CalculateBulletDrop(const FVector& StartPos, const FVector& Direction, float Distance) const
{
    // 简化的弹道下坠
    float Time = Distance / FMath::Max(1.f, WeaponData.MuzzleVelocity * 0.01f); // 秒
    float Drop = 0.5f * ProjectileParams.GravityEffect * Time * Time; // cm
    return StartPos + Direction * Distance - FVector(0, 0, Drop);
}

float UPlayerBallisticWeaponComponent::GetTimeToTarget(float Distance) const
{
    return Distance / FMath::Max(1.f, WeaponData.MuzzleVelocity * 0.01f);
}

float UPlayerBallisticWeaponComponent::GetNoiseLevel() const
{
    float Noise = 1.f;
    if (bHasSuppressor) Noise *= (1.f - SuppressorSoundReduction);
    if (SpecialAmmo.bIsSubsonic) Noise *= 0.5f;
    return Noise;
}

void UPlayerBallisticWeaponComponent::UpdateHeat(float Dt)
{
    if (CurrentHeat > 0.f)
    {
        float Dissipation = bIsAiming ? 8.f : 5.f;
        CurrentHeat = FMath::Max(0.f, CurrentHeat - Dissipation * Dt);
    }
    if (CurrentHeat >= OverheatThreshold && CurrentState != EWeaponState::Overheated)
    {
        CurrentState = EWeaponState::Overheated;
        GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
        {
            // 3秒后恢复
            GetWorld()->GetTimerManager().SetTimer(OverheatTimerHandle, [this]()
            {
                CurrentHeat = OverheatThreshold * 0.3f;
                CurrentState = EWeaponState::Idle;
            }, 3.f, false);
        });
    }
}

float UPlayerBallisticWeaponComponent::GetHeatPercent() const
{
    return OverheatThreshold > 0 ? CurrentHeat / OverheatThreshold : 0.f;
}

void UPlayerBallisticWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(UPlayerBallisticWeaponComponent, CurrentHeat);
}

void UPlayerBallisticWeaponComponent::ServerSwapAmmoType_Implementation(EAmmoType NewType)
{
    // 切换弹药类型（服务端验证）
}
