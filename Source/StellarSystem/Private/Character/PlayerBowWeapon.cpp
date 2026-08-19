// ============================================================
// 路径: Source/StellarSystem/Private/Character/PlayerBowWeapon.cpp
// 作用: 玩家弓弩实现（短弓/长弓/十字弩/连弩/复合弓）
// ============================================================

#include "Character/PlayerBowWeapon.h"
#include "Character/MyCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"
#include "DrawDebugHelpers.h"

UPlayerBowWeaponComponent::UPlayerBowWeaponComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
    BowType = EBowSubtype::ShortBow;
    CurrentDraw = 0.f;
    bIsDrawing = false;
    CurrentFatigue = 0.f;
    bIsScoped = false;

    DrawParams.MaxDrawTime = 1.5f;
    DrawParams.MinDrawDamageMult = 0.3f;
    DrawParams.MaxDrawDamageMult = 1.0f;
    DrawParams.DrawSpeed = 1.0f;
    DrawParams.bCanHoldFullDraw = true;
    DrawParams.HoldFullDrawTime = 5.f;
    DrawParams.DrawFatiguePerSec = 2.f;
    DrawParams.MaxFatigue = 100.f;

    BowParams.BowDrawWeight = 40.f;
    BowParams.BowstringTension = 1.0f;
    BowParams.ArrowSpeed_Min = 50000.f;
    BowParams.ArrowSpeed_Max = 120000.f;
    BowParams.AccuracyBonus = 0.2f;
    BowParams.HeadshotMultiplier = 2.0f;
    BowParams.bHasScope = false;
    BowParams.ScopeMagnification = 4.f;
    BowParams.bIsCrossbow = false;
    BowParams.ReloadTime = 1.5f;
    BowParams.bAutoReload = false;
    BowParams.MagazineSize = 1;
    BowParams.bIsCompound = false;
    BowParams.LetOffPercent = 0.8f;
    BowParams.CamSystemEfficiency = 1.3f;

    CurrentArrowType = EArrowType::Standard;
    CurrentArrowCount = 20;
    BowstringGlow = FLinearColor(0.5f, 0.8f, 1.f, 1.f);
}

void UPlayerBowWeaponComponent::BeginPlay()
{
    Super::BeginPlay();

    // 根据子类设置默认值
    switch (BowType)
    {
        case EBowSubtype::ShortBow:
            WeaponData.FireRate = 120.f; WeaponData.BaseDamage = 30.f;
            WeaponData.MagazineSize = 1; WeaponData.ReloadTime = 1.0f;
            DrawParams.MaxDrawTime = 1.0f; BowParams.BowDrawWeight = 30.f;
            BowParams.ArrowSpeed_Min = 40000.f; BowParams.ArrowSpeed_Max = 90000.f;
            break;
        case EBowSubtype::LongBow:
            WeaponData.FireRate = 60.f; WeaponData.BaseDamage = 55.f;
            WeaponData.MagazineSize = 1; WeaponData.ReloadTime = 2.0f;
            DrawParams.MaxDrawTime = 2.0f; BowParams.BowDrawWeight = 60.f;
            BowParams.ArrowSpeed_Min = 60000.f; BowParams.ArrowSpeed_Max = 150000.f;
            WeaponData.EffectiveRange = 50000.f;
            break;
        case EBowSubtype::Crossbow:
            WeaponData.FireRate = 90.f; WeaponData.BaseDamage = 65.f;
            WeaponData.MagazineSize = 1; WeaponData.ReloadTime = 2.5f;
            BowParams.bIsCrossbow = true; BowParams.BowDrawWeight = 80.f;
            BowParams.ArrowSpeed_Min = 70000.f; BowParams.ArrowSpeed_Max = 130000.f;
            DrawParams.MaxDrawTime = 0.8f; // 弩不需要拉很久
            break;
        case EBowSubtype::AutoCrossbow:
            WeaponData.FireRate = 300.f; WeaponData.BaseDamage = 28.f;
            WeaponData.MagazineSize = 6; WeaponData.ReloadTime = 3.0f;
            BowParams.bIsCrossbow = true; BowParams.bAutoReload = true;
            BowParams.BowDrawWeight = 35.f;
            BowParams.ArrowSpeed_Min = 50000.f; BowParams.ArrowSpeed_Max = 100000.f;
            break;
        case EBowSubtype::CompoundBow:
            WeaponData.FireRate = 100.f; WeaponData.BaseDamage = 50.f;
            WeaponData.MagazineSize = 1; WeaponData.ReloadTime = 1.5f;
            BowParams.bIsCompound = true; BowParams.BowDrawWeight = 50.f;
            BowParams.LetOffPercent = 0.75f; BowParams.CamSystemEfficiency = 1.4f;
            BowParams.ArrowSpeed_Min = 65000.f; BowParams.ArrowSpeed_Max = 160000.f;
            DrawParams.MaxDrawTime = 1.2f;
            WeaponData.bHasChargeShot = true;
            WeaponData.MaxChargeTime = 1.2f;
            break;
    }

    // 初始化箭矢库
    FArrowParams Standard;
    Standard.ArrowID = FName("ArrowStandard");
    Standard.Type = EArrowType::Standard;
    Standard.ArrowMass = 0.02f; Standard.BaseDamage = 35.f;
    Standard.ArmorPierce = 0.15f; Standard.DragCoefficient = 0.08f;
    ArrowLibrary.Add(EArrowType::Standard, Standard);

    FArrowParams Broadhead;
    Broadhead.ArrowID = FName("ArrowBroadhead");
    Broadhead.Type = EArrowType::Broadhead;
    Broadhead.ArrowMass = 0.025f; Broadhead.BaseDamage = 55.f;
    Broadhead.ArmorPierce = 0.1f; Broadhead.DragCoefficient = 0.12f;
    ArrowLibrary.Add(EArrowType::Broadhead, Broadhead);

    FArrowParams Bodkin;
    Bodkin.ArrowID = FName("ArrowBodkin");
    Bodkin.Type = EArrowType::Bodkin;
    Bodkin.ArrowMass = 0.022f; Bodkin.BaseDamage = 40.f;
    Bodkin.ArmorPierce = 0.5f; Bodkin.DragCoefficient = 0.06f;
    ArrowLibrary.Add(EArrowType::Bodkin, Bodkin);

    FArrowParams Fire;
    Fire.ArrowID = FName("ArrowFire");
    Fire.Type = EArrowType::FireArrow;
    Fire.ArrowMass = 0.02f; Fire.BaseDamage = 30.f;
    Fire.FireDamagePerSec = 8.f; Fire.FireDuration = 5.f;
    ArrowLibrary.Add(EArrowType::FireArrow, Fire);

    FArrowParams Poison;
    Poison.ArrowID = FName("ArrowPoison");
    Poison.Type = EArrowType::PoisonArrow;
    Poison.ArrowMass = 0.02f; Poison.BaseDamage = 20.f;
    Poison.PoisonDamagePerSec = 5.f; Poison.PoisonDuration = 8.f;
    ArrowLibrary.Add(EArrowType::PoisonArrow, Poison);

    FArrowParams Cryo;
    Cryo.ArrowID = FName("ArrowCryo");
    Cryo.Type = EArrowType::CryoArrow;
    Cryo.ArrowMass = 0.02f; Cryo.BaseDamage = 25.f;
    Cryo.CryoSlowAmount = 0.4f; Cryo.CryoDuration = 4.f;
    ArrowLibrary.Add(EArrowType::CryoArrow, Cryo);

    FArrowParams Explosive;
    Explosive.ArrowID = FName("ArrowExplosive");
    Explosive.Type = EArrowType::ExplosiveArrow;
    Explosive.ArrowMass = 0.03f; Explosive.BaseDamage = 45.f;
    Explosive.ExplosionRadius = 150.f;
    ArrowLibrary.Add(EArrowType::ExplosiveArrow, Explosive);

    FArrowParams Grapple;
    Grapple.ArrowID = FName("ArrowGrapple");
    Grapple.Type = EArrowType::GrapplingHook;
    Grapple.ArrowMass = 0.04f; Grapple.BaseDamage = 5.f;
    Grapple.GrapplingPullForce = 80000.f;
    ArrowLibrary.Add(EArrowType::GrapplingHook, Grapple);

    FArrowParams Signal;
    Signal.ArrowID = FName("ArrowSignal");
    Signal.Type = EArrowType::SignalArrow;
    Signal.ArrowMass = 0.015f; Signal.BaseDamage = 1.f;
    Signal.SignalFlareDuration = 30.f;
    ArrowLibrary.Add(EArrowType::SignalArrow, Signal);

    // 初始箭矢库存
    ArrowInventory.Add(EArrowType::Standard, 20);
    ArrowInventory.Add(EArrowType::Broadhead, 10);
    ArrowInventory.Add(EArrowType::Bodkin, 10);
    ArrowInventory.Add(EArrowType::FireArrow, 5);
    ArrowInventory.Add(EArrowType::PoisonArrow, 5);
    ArrowInventory.Add(EArrowType::CryoArrow, 5);
    ArrowInventory.Add(EArrowType::ExplosiveArrow, 3);
    ArrowInventory.Add(EArrowType::GrapplingHook, 3);
    ArrowInventory.Add(EArrowType::SignalArrow, 3);

    CurrentArrowCount = GetArrowCount(CurrentArrowType);
    FireInterval = 60.f / FMath::Max(1.f, WeaponData.FireRate);
}

void UPlayerBowWeaponComponent::TickComponent(float Dt, ELevelTick Tick, FActorComponentTickFunction* Fn)
{
    Super::TickComponent(Dt, Tick, Fn);
    UpdateDraw(Dt);
    UpdateFatigue(Dt);
}

void UPlayerBowWeaponComponent::FireWeapon()
{
    if (!CanFire()) return;

    float DamageMult = GetDamageMultiplier();
    FVector Direction = GetOwner()->GetActorForwardVector();
    float Spread = bIsAiming ? WeaponData.SpreadBase * 0.2f : WeaponData.SpreadBase * 0.5f;
    Spread *= (1.f - BowParams.AccuracyBonus);
    Direction = FMath::VRandCone(Direction, FMath::DegreesToRadians(Spread));

    SpawnArrow(GetOwner()->GetActorLocation() + FVector(0,0,50.f), Direction, DamageMult);

    // 消耗箭矢
    if (ArrowInventory.Contains(CurrentArrowType))
    {
        ArrowInventory[CurrentArrowType] = FMath::Max(0, ArrowInventory[CurrentArrowType] - 1);
    }
    CurrentArrowCount = GetArrowCount(CurrentArrowType);

    // 十字弩自动装填
    if (BowParams.bAutoReload && BowParams.bIsCrossbow)
    {
        GetWorld()->GetTimerManager().SetTimer(ReloadTimerHandle, this,
            &UPlayerBowWeaponComponent::FinishReload, BowParams.ReloadTime, false);
    }

    CurrentDraw = 0.f;
    bIsDrawing = false;
    TimeSinceLastShot = 0.f;
    OnWeaponFired.Broadcast();
}

bool UPlayerBowWeaponComponent::CanFire() const
{
    if (CurrentState == EWeaponState::Reloading) return false;
    if (CurrentState == EWeaponState::Jammed) return false;
    if (CurrentDraw < 0.3f) return false; // 至少要拉 30%
    if (GetArrowCount(CurrentArrowType) <= 0) return false;
    if (CurrentFatigue >= DrawParams.MaxFatigue) return false;
    if (FireInterval > 0.f && TimeSinceLastShot < FireInterval) return false;
    return true;
}

void UPlayerBowWeaponComponent::SpawnArrow(const FVector& Origin, const FVector& Direction, float DamageMult)
{
    if (!ArrowLibrary.Contains(CurrentArrowType)) return;

    const FArrowParams& Arrow = ArrowLibrary[CurrentArrowType];
    float ArrowSpeed = GetArrowSpeed() * (0.5f + 0.5f * CurrentDraw);
    float Damage = Arrow.BaseDamage * DamageMult;

    // 特殊效果
    switch (Arrow.Type)
    {
        case EArrowType::FireArrow:
            Damage += Arrow.FireDamagePerSec * 0.5f;
            break;
        case EArrowType::PoisonArrow:
            Damage += Arrow.PoisonDamagePerSec * 0.5f;
            break;
        case EArrowType::CryoArrow:
            Damage += 5.f; // 冷冻基础额外伤害
            break;
        case EArrowType::ExplosiveArrow:
            // 命中后爆炸
            break;
        case EArrowType::GrapplingHook:
            // 拉向目标
            break;
        case EArrowType::SignalArrow:
            // 发射信号弹效果
            break;
    }

    // 射线检测（箭矢高速，用射线近似）
    FVector End = Origin + Direction * WeaponData.MaxRange;
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(GetOwner());

    if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, End, ECC_Pawn, Params))
    {
        if (Hit.GetActor())
        {
            // 爆头检测
            bool bHeadshot = Hit.BoneName == FName("head") || Hit.BoneName == FName("Head");
            if (bHeadshot) Damage *= BowParams.HeadshotMultiplier;

            UGameplayStatics::ApplyDamage(Hit.GetActor(), Damage, nullptr, GetOwner(), UDamageType::StaticClass());

            // 特殊效果应用
            if (Arrow.Type == EArrowType::FireArrow)
            {
                // 点燃目标
            }
            else if (Arrow.Type == EArrowType::PoisonArrow)
            {
                // 中毒 DOT
            }
            else if (Arrow.Type == EArrowType::CryoArrow)
            {
                // 减速
            }
        }
    }

    // 特效
    if (ReleaseEffect.IsValid())
    {
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ReleaseEffect.Get(), Origin);
    }
    if (ReleaseSound.IsValid())
    {
        UGameplayStatics::PlaySoundAtLocation(this, ReleaseSound.Get(), Origin);
    }
}

void UPlayerBowWeaponComponent::StartDrawing()
{
    if (GetArrowCount(CurrentArrowType) <= 0) return;
    if (CurrentFatigue >= DrawParams.MaxFatigue) return;
    bIsDrawing = true;
    CurrentDraw = 0.f;
    if (DrawSound.IsValid())
    {
        UGameplayStatics::PlaySoundAtLocation(this, DrawSound.Get(), GetOwner()->GetActorLocation());
    }
}

void UPlayerBowWeaponComponent::ReleaseArrow()
{
    if (!bIsDrawing) return;
    if (CurrentDraw >= 0.3f)
    {
        FireWeapon();
    }
    bIsDrawing = false;
    CurrentDraw = 0.f;
}

float UPlayerBowWeaponComponent::GetDamageMultiplier() const
{
    float T = FMath::Clamp(CurrentDraw, 0.f, 1.f);
    return FMath::Lerp(DrawParams.MinDrawDamageMult, DrawParams.MaxDrawDamageMult, T);
}

float UPlayerBowWeaponComponent::GetArrowSpeed() const
{
    float T = FMath::Clamp(CurrentDraw, 0.f, 1.f);
    float Speed = FMath::Lerp(BowParams.ArrowSpeed_Min, BowParams.ArrowSpeed_Max, T);

    // 复合弓凸轮系统加成
    if (BowParams.bIsCompound)
    {
        Speed *= BowParams.CamSystemEfficiency;
    }

    // 疲劳惩罚
    float FatiguePenalty = CurrentFatigue / DrawParams.MaxFatigue * 0.3f;
    Speed *= (1.f - FatiguePenalty);

    return Speed;
}

void UPlayerBowWeaponComponent::UpdateDraw(float Dt)
{
    if (bIsDrawing)
    {
        float DrawSpeed = DrawParams.DrawSpeed;
        if (BowParams.bIsCompound)
        {
            // 复合弓：拉到满弓后省力
            if (CurrentDraw > 0.7f) DrawSpeed *= (1.f + (1.f - BowParams.LetOffPercent));
        }
        CurrentDraw = FMath::Min(1.f, CurrentDraw + Dt / FMath::Max(0.1f, DrawParams.MaxDrawTime) * DrawSpeed);
    }
}

void UPlayerBowWeaponComponent::UpdateFatigue(float Dt)
{
    if (bIsDrawing)
    {
        CurrentFatigue = FMath::Min(DrawParams.MaxFatigue, CurrentFatigue + DrawParams.DrawFatiguePerSec * Dt);
    }
    else if (CurrentFatigue > 0.f)
    {
        CurrentFatigue = FMath::Max(0.f, CurrentFatigue - DrawParams.DrawFatiguePerSec * 0.5f * Dt);
    }
}

void UPlayerBowWeaponComponent::SwitchArrowType(EArrowType NewType)
{
    if (ArrowLibrary.Contains(NewType) && GetArrowCount(NewType) > 0)
    {
        CurrentArrowType = NewType;
        CurrentArrowCount = GetArrowCount(NewType);
    }
}

void UPlayerBowWeaponComponent::AddArrows(EArrowType Type, int32 Amount)
{
    if (ArrowInventory.Contains(Type))
    {
        ArrowInventory[Type] += Amount;
    }
    else
    {
        ArrowInventory.Add(Type, Amount);
    }
    CurrentArrowCount = GetArrowCount(CurrentArrowType);
}

int32 UPlayerBowWeaponComponent::GetArrowCount(EArrowType Type) const
{
    if (ArrowInventory.Contains(Type)) return ArrowInventory[Type];
    return 0;
}

TArray<EArrowType> UPlayerBowWeaponComponent::GetAvailableArrowTypes() const
{
    TArray<EArrowType> Types;
    for (const auto& Pair : ArrowInventory)
    {
        if (Pair.Value > 0) Types.Add(Pair.Key);
    }
    return Types;
}

void UPlayerBowWeaponComponent::FireGrapplingHook()
{
    if (!ArrowLibrary.Contains(EArrowType::GrapplingHook)) return;
    if (GetArrowCount(EArrowType::GrapplingHook) <= 0) return;

    FVector Origin = GetOwner()->GetActorLocation() + FVector(0,0,50.f);
    FVector Direction = GetOwner()->GetActorForwardVector();
    SpawnArrow(Origin, Direction, 0.1f); // 低伤害

    ArrowInventory[EArrowType::GrapplingHook]--;
    CurrentArrowCount = GetArrowCount(EArrowType::GrapplingHook);
}

bool UPlayerBowWeaponComponent::IsGrappling() const
{
    // 检查是否有钩索连接中
    return false; // 简化
}

void UPlayerBowWeaponComponent::ReleaseGrapplingHook()
{
    // 断开钩索
}

void UPlayerBowWeaponComponent::ToggleScope()
{
    if (!BowParams.bHasScope) return;
    bIsScoped = !bIsScoped;
    if (bIsScoped)
    {
        // 放大视野
    }
    else
    {
        // 恢复视野
    }
}

void UPlayerBowWeaponComponent::ProcessStandardArrow()
{
    // 标准箭：平衡伤害
}

void UPlayerBowWeaponComponent::ProcessBodkinArrow()
{
    // 穿甲箭：高穿甲，对重甲目标额外伤害
}

void UPlayerBowWeaponComponent::ProcessFireArrow()
{
    // 火箭：点燃目标 + 持续燃烧
}

void UPlayerBowWeaponComponent::ProcessPoisonArrow()
{
    // 毒箭：DOT 伤害
}

void UPlayerBowWeaponComponent::ProcessCryoArrow()
{
    // 冰箭：减速 + 短暂冻结
}

void UPlayerBowWeaponComponent::ProcessExplosiveArrow()
{
    // 爆箭：命中后小范围爆炸
}

void UPlayerBowWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(UPlayerBowWeaponComponent, CurrentDraw);
    DOREPLIFETIME(UPlayerBowWeaponComponent, bIsDrawing);
    DOREPLIFETIME(UPlayerBowWeaponComponent, CurrentFatigue);
    DOREPLIFETIME(UPlayerBowWeaponComponent, bIsScoped);
    DOREPLIFETIME(UPlayerBowWeaponComponent, CurrentArrowType);
}
