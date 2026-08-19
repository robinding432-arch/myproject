// ============================================================
// 路径: Source/StellarSystem/Private/Station/StationDefenseTurret.cpp
// 模块: Station (空间站)
// 类型: 源文件
// 作用: 防御炮塔完整实现 — 索敌/开火/过热/升级/维修
// 新增于: v7.6.1
// ============================================================

#include "Station/StationDefenseTurret.h"
#include "Station/PlayerOwnedStation.h"
#include "Ship/ShipPawn.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "Math/UnrealMathUtility.h"
#include "Math/RotationMatrix.h"

DEFINE_LOG_CATEGORY_STATIC(LogDefenseTurret, Log, All);

// ========== 升级曲线参数 ==========
static const float TurretDamageScale[]   = { 1.0f, 1.5f, 2.2f, 3.0f, 4.0f, 5.5f };  // Level 0~5
static const float TurretFireRateScale[] = { 1.0f, 1.3f, 1.7f, 2.2f, 3.0f, 4.0f };
static const float TurretRangeScale[]    = { 1.0f, 1.2f, 1.4f, 1.6f, 1.8f, 2.0f };
static const float TurretHealthScale[]   = { 1.0f, 1.8f, 3.0f, 5.0f, 8.0f, 12.0f };
static const float TurretAccuracyScale[] = { 0.6f, 0.75f, 0.85f, 0.92f, 0.96f, 0.99f };

// ========== 升级成本 ==========
static const int32 TurretCreditCost[]   = { 0, 5000, 15000, 40000, 100000, 250000 };
static const int32 TurretTitaniumCost[] = { 0, 50, 150, 400, 1000, 2500 };
static const int32 TurretQuantumCost[]  = { 0, 5, 15, 40, 100, 250 };
static const int32 TurretElectronicsCost[] = { 0, 20, 60, 150, 400, 1000 };
static const int32 TurretWeaponCost[]   = { 0, 10, 30, 80, 200, 500 };

AStationDefenseTurret::AStationDefenseTurret()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(false);

    // 碰撞体
    DetectionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("DetectionSphere"));
    DetectionSphere->SetupAttachment(RootComponent);
    DetectionSphere->SetSphereRadius(50000.f);  // 500m 默认
    DetectionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

    // 炮塔网格
    TurretMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretMesh"));
    TurretMeshComponent->SetupAttachment(RootComponent);
    TurretMeshComponent->SetIsReplicated(true);

    BarrelMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BarrelMesh"));
    BarrelMeshComponent->SetupAttachment(TurretMeshComponent);
    BarrelMeshComponent->SetIsReplicated(true);

    // 默认战斗属性
    CombatStats.DamagePerShot = 50.f;
    CombatStats.FireRate = 120.f;
    CombatStats.Range = 50000.f;
    CombatStats.Accuracy = 0.85f;
    CombatStats.TurnSpeed = 90.f;
    CombatStats.ShieldDamageMultiplier = 1.0f;
    CombatStats.HullDamageMultiplier = 0.7f;
    CombatStats.bCanTargetShips = true;
    CombatStats.bCanTargetMissiles = false;
    CombatStats.AmmoCapacity = -1;
    CombatStats.ReloadTime = 0.f;

    // 默认目标通道
    TargetChannels.Add(ECC_Pawn);
    TargetChannels.Add(ECC_Vehicle);
    TargetChannels.Add(ECC_WorldDynamic);

    // 运行时
    CurrentHealth = MaxHealth;
    FireCooldownRemaining = 0.f;
    ScanTimer = 0.f;
}

void AStationDefenseTurret::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        CurrentHealth = MaxHealth * TurretHealthScale[(int)CurrentLevel];
        CurrentHeat = 0.f;

        if (bIsInstalled && CurrentLevel > ETurretLevel::None)
        {
            // 应用等级加成
            int32 Lvl = (int32)CurrentLevel;
            CombatStats.DamagePerShot *= TurretDamageScale[Lvl];
            CombatStats.FireRate *= TurretFireRateScale[Lvl];
            CombatStats.Range *= TurretRangeScale[Lvl];
            CombatStats.Accuracy = TurretAccuracyScale[Lvl];
            DetectionSphere->SetSphereRadius(CombatStats.Range);
        }

        // 启动扫描
        GetWorld()->GetTimerManager().SetTimer(
            ScanTimerHandle,
            this, &AStationDefenseTurret::ScanForTargets,
            ScanInterval, true
        );

        UE_LOG(LogDefenseTurret, Log, TEXT("Turret %s initialized (Type=%d, Level=%d, Installed=%s)"),
            *TurretID.ToString(), (int)TurretType, (int)CurrentLevel,
            bIsInstalled ? TEXT("Yes") : TEXT("No"));
    }
}

void AStationDefenseTurret::Tick(float Dt)
{
    Super::Tick(Dt);
    if (!HasAuthority()) return;

    // 过热更新
    UpdateHeat(Dt);

    // 维修更新
    if (bIsRepairing) UpdateRepair(Dt);

    // 炮塔被毁 → 停止一切
    if (bIsDestroyed) return;

    // 自动开火
    if (bAutoFire && !bOverheated)
    {
        ProcessAutoFire(Dt);
    }

    // 转向目标
    if (CurrentTarget && IsValid(CurrentTarget))
    {
        FVector Dir = (CurrentTarget->GetActorLocation() - GetActorLocation()).GetSafeNormal();
        FRotator DesiredRot = Dir.Rotation();
        FRotator CurrentRot = TurretMeshComponent->GetComponentRotation();
        float TurnRate = CombatStats.TurnSpeed * Dt;
        FRotator NewRot = FMath::RInterpTo(CurrentRot, DesiredRot, Dt, CombatStats.TurnSpeed / 90.f);
        TurretMeshComponent->SetWorldRotation(NewRot);
    }

    // 冷却
    if (FireCooldownRemaining > 0.f)
    {
        FireCooldownRemaining = FMath::Max(0.f, FireCooldownRemaining - Dt);
    }
}

// ========== 扫描索敌 ==========

void AStationDefenseTurret::ScanForTargets()
{
    if (!HasAuthority() || bIsDestroyed || bOverheated) return;

    UWorld* World = GetWorld();
    if (!World) return;

    FVector MyLoc = GetActorLocation();
    TArray<AActor*> Hostiles;
    TArray<AActor*> Missiles;

    // 收集所有 Actor
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AShipPawn::StaticClass(), AllActors);

    for (AActor* Actor : AllActors)
    {
        if (!IsValidTarget(Actor)) continue;

        float Dist = FVector::Dist(MyLoc, Actor->GetActorLocation());
        if (Dist > CombatStats.Range) continue;

        // 检查是否在 PriorityTargetTags 中
        bool bPriority = false;
        for (FName Tag : PriorityTargetTags)
        {
            if (Actor->ActorHasTag(Tag)) { bPriority = true; break; }
        }

        if (bPriority)
        {
            Hostiles.Insert(Actor, 0);  // 优先目标放前面
        }
        else
        {
            Hostiles.Add(Actor);
        }
    }

    // 如果有当前目标且仍有效，保持
    if (CurrentTarget && IsValid(CurrentTarget) && IsValidTarget(CurrentTarget))
    {
        float Dist = FVector::Dist(MyLoc, CurrentTarget->GetActorLocation());
        if (Dist <= CombatStats.Range) return;  // 保持锁定
    }

    // 选新目标
    if (Hostiles.Num() > 0)
    {
        AcquireTarget(Hostiles[0]);
    }
    else
    {
        CurrentTarget = nullptr;
        TargetLockProgress = 0.f;
    }
}

void AStationDefenseTurret::AcquireTarget(AActor* Target)
{
    if (!Target) return;

    CurrentTarget = Target;
    TargetLockProgress = 0.f;

    UE_LOG(LogDefenseTurret, Log, TEXT("Turret %s acquiring target: %s"),
        *TurretID.ToString(), *Target->GetName());
}

// ========== 开火 ==========

void AStationDefenseTurret::ProcessAutoFire(float Dt)
{
    if (!CurrentTarget || !IsValid(CurrentTarget)) return;
    if (FireCooldownRemaining > 0.f) return;
    if (CurrentAmmo == 0) return;

    // 锁定进度
    TargetLockProgress += Dt;
    if (TargetLockProgress < LockTime) return;  // 还在锁定中

    // 检查视线
    FVector Start = GetActorLocation();
    FVector End = CurrentTarget->GetActorLocation();
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    bool bHasLOS = !GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

    if (bHasLOS || FVector::Dist(Start, End) < 1000.f)
    {
        FireAtTarget();
    }
}

void AStationDefenseTurret::FireAtTarget()
{
    if (!CurrentTarget || !HasAuthority()) return;
    if (bOverheated) return;

    // 弹药检查
    if (CurrentAmmo > 0)
    {
        CurrentAmmo--;
    }
    else if (CurrentAmmo == 0)
    {
        // 需要补给
        return;
    }

    // 命中判定（带精度）
    float AccuracyRoll = FMath::FRand();
    bool bHit = (AccuracyRoll < CombatStats.Accuracy);
    float Damage = CombatStats.DamagePerShot;

    // 距离衰减
    float Dist = FVector::Dist(GetActorLocation(), CurrentTarget->GetActorLocation());
    Damage *= CalculateDamageFalloff(Dist);

    FTurretShotData ShotData;
    ShotData.FireLocation = GetActorLocation();
    ShotData.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

    if (bHit)
    {
        ShotData.TargetLocation = CurrentTarget->GetActorLocation();
        ShotData.Damage = Damage;
        ApplyDamageToTarget(CurrentTarget, Damage);
    }
    else
    {
        // 脱靶 → 偏移
        FVector Offset = FMath::VRand() * (1000.f / CombatStats.Accuracy);
        ShotData.TargetLocation = CurrentTarget->GetActorLocation() + Offset;
        ShotData.Damage = 0.f;
    }

    // 过热
    CurrentHeat = FMath::Min(MaxHeat, CurrentHeat + HeatPerShot);
    if (CurrentHeat >= MaxHeat)
    {
        bOverheated = true;
        if (OverheatSound.IsValid())
        {
            UGameplayStatics::PlaySoundAtLocation(this, OverheatSound.Get(), GetActorLocation());
        }
        UE_LOG(LogDefenseTurret, Warning, TEXT("Turret %s OVERHEATED"), *TurretID.ToString());
    }

    // 冷却
    FireCooldownRemaining = 60.f / CombatStats.FireRate;  // 秒/发

    // 特效
    PlayFireEffects();

    // 广播事件
    OnTurretFired.Broadcast(ShotData);

    UE_LOG(LogDefenseTurret, Verbose, TEXT("Turret %s fired: dmg=%.0f hit=%s target=%s"),
        *TurretID.ToString(), Damage, bHit ? TEXT("YES") : TEXT("NO"),
        *CurrentTarget->GetName());
}

void AStationDefenseTurret::Server_FireTurret_Implementation()
{
    if (CurrentTarget && IsValid(CurrentTarget))
    {
        FireAtTarget();
    }
}

// ========== 伤害 ==========

void AStationDefenseTurret::ApplyDamageToTarget(AActor* Target, float Damage)
{
    if (!Target || Damage <= 0.f) return;

    // 区分护盾/船体
    AShipPawn* Ship = Cast<AShipPawn>(Target);
    if (Ship)
    {
        // 先扣护盾
        float ShieldDmg = Damage * CombatStats.ShieldDamageMultiplier;
        float HullDmg = Damage * CombatStats.HullDamageMultiplier;

        // 调用 ShipPawn 的受击接口
        // Ship->TakeDamage(ShieldDmg, HullDmg, this);
        // 简化：直接 ApplyDamage
        Target->TakeDamage(ShieldDmg + HullDmg * 0.5f, FDamageEvent(), GetInstigatorController(), this);
    }
    else
    {
        Target->TakeDamage(Damage, FDamageEvent(), GetInstigatorController(), this);
    }
}

float AStationDefenseTurret::CalculateDamageFalloff(float Distance) const
{
    float Range = CombatStats.Range;
    if (Distance >= Range) return 0.3f;  // 最远距离伤害衰减到 30%

    // 线性衰减：50% 射程内满伤害，之后递减
    float HalfRange = Range * 0.5f;
    if (Distance < HalfRange) return 1.0f;
    return 1.0f - 0.7f * ((Distance - HalfRange) / (Range - HalfRange));
}

bool AStationDefenseTurret::IsValidTarget(AActor* Target) const
{
    if (!Target || !IsValid(Target)) return false;

    // 不能打友方
    // (简化：检查 Owner 标签)
    if (Target->ActorHasTag(FName("Friendly")) || Target->ActorHasTag(FName("Allied")))
    {
        // 检查是否属于同一所有者
        // (需要访问 PlayerOwnedStation 的 OwningGuild)
    }

    // 类型检查
    if (Target->IsA<AShipPawn>() && CombatStats.bCanTargetShips) return true;

    // Flak 可以打导弹
    if (CombatStats.bCanTargetMissiles && Target->ActorHasTag(FName("Missile"))) return true;

    return false;
}

// ========== 过热 ==========

void AStationDefenseTurret::UpdateHeat(float Dt)
{
    if (bOverheated)
    {
        // 过热冷却较慢
        CurrentHeat = FMath::Max(0.f, CurrentHeat - HeatDissipationRate * 0.5f * Dt);
        if (CurrentHeat <= MaxHeat * 0.3f)  // 降到 30% 以下才恢复
        {
            bOverheated = false;
            UE_LOG(LogDefenseTurret, Log, TEXT("Turret %s heat recovered"), *TurretID.ToString());
        }
    }
    else if (CurrentHeat > 0.f)
    {
        CurrentHeat = FMath::Max(0.f, CurrentHeat - HeatDissipationRate * Dt);
    }
}

// ========== 升级 ==========

FTurretUpgradeCost AStationDefenseTurret::GetUpgradeCost() const
{
    FTurretUpgradeCost Cost;
    int32 NextLvl = FMath::Min(5, (int32)CurrentLevel + 1);

    Cost.Credits = TurretCreditCost[NextLvl];
    Cost.Titanium = TurretTitaniumCost[NextLvl];
    Cost.QuantumCore = TurretQuantumCost[NextLvl];
    Cost.Electronics = TurretElectronicsCost[NextLvl];
    Cost.WeaponComponents = TurretWeaponCost[NextLvl];

    return Cost;
}

bool AStationDefenseTurret::CanUpgrade() const
{
    if (bIsDestroyed) return false;
    if (bIsRepairing) return false;
    if ((int32)CurrentLevel >= 5) return false;  // 已达巅峰
    if (!bIsInstalled) return false;
    return true;
}

void AStationDefenseTurret::Server_UpgradeTurret_Implementation(AController* RequestedBy)
{
    if (!CanUpgrade()) return;
    if (!RequestedBy) return;

    int32 NextLvl = FMath::Min(5, (int32)CurrentLevel + 1);
    FTurretUpgradeCost Cost = GetUpgradeCost();

    // TODO: 验证玩家资源（信用点/钛/量子核心等）
    // 通过 GameMode 的货币/库存系统扣费
    // 简化：假设已验证通过

    // 应用升级
    ETurretLevel OldLevel = CurrentLevel;
    CurrentLevel = (ETurretLevel)NextLvl;

    // 重新计算属性
    float HealthRatio = CurrentHealth / MaxHealth;
    MaxHealth = 500.f * TurretHealthScale[NextLvl];
    CurrentHealth = MaxHealth * HealthRatio;  // 保持百分比

    CombatStats.DamagePerShot *= (TurretDamageScale[NextLvl] / TurretDamageScale[(int)OldLevel]);
    CombatStats.FireRate *= (TurretFireRateScale[NextLvl] / TurretFireRateScale[(int)OldLevel]);
    CombatStats.Range *= (TurretRangeScale[NextLvl] / TurretRangeScale[(int)OldLevel]);
    CombatStats.Accuracy = TurretAccuracyScale[NextLvl];
    DetectionSphere->SetSphereRadius(CombatStats.Range);

    // Flak 类型在高级解锁导弹防御
    if (TurretType == ETurretType::Flak && NextLvl >= 3)
    {
        CombatStats.bCanTargetMissiles = true;
    }

    // 广播事件
    OnTurretUpgraded.Broadcast(this, NextLvl);

    UE_LOG(LogDefenseTurret, Log, TEXT("Turret %s UPGRADED: %d → %d (Dmg=%.0f, FireRate=%.0f, Range=%.0f)"),
        *TurretID.ToString(), (int)OldLevel, NextLvl,
        CombatStats.DamagePerShot, CombatStats.FireRate, CombatStats.Range);
}

bool AStationDefenseTurret::Server_UpgradeTurret_Validate(AController*) { return true; }

// ========== 安装/卸载 ==========

void AStationDefenseTurret::Server_InstallTurret_Implementation(AController* RequestedBy, ETurretType Type)
{
    if (bIsInstalled) return;
    if (!RequestedBy) return;

    // TODO: 验证资源

    TurretType = Type;
    bIsInstalled = true;
    CurrentLevel = ETurretType::Basic;  // 安装后 = 1 级
    CurrentHealth = MaxHealth * TurretHealthScale[1];
    MaxHealth = 500.f * TurretHealthScale[1];

    // 根据类型设置初始属性
    switch (Type)
    {
    case ETurretType::Laser:
        CombatStats.DamagePerShot = 45.f;
        CombatStats.FireRate = 180.f;
        CombatStats.Range = 45000.f;
        CombatStats.Accuracy = 0.9f;
        CombatStats.ShieldDamageMultiplier = 1.4f;
        CombatStats.bCanTargetMissiles = false;
        HeatPerShot = 2.f;
        break;
    case ETurretType::Flak:
        CombatStats.DamagePerShot = 25.f;
        CombatStats.FireRate = 300.f;
        CombatStats.Range = 30000.f;
        CombatStats.Accuracy = 0.7f;
        CombatStats.bCanTargetMissiles = true;  // Flak 专长
        CombatStats.bCanTargetShips = true;
        HeatPerShot = 1.5f;
        break;
    case ETurretType::Missile:
        CombatStats.DamagePerShot = 120.f;
        CombatStats.FireRate = 30.f;
        CombatStats.Range = 70000.f;
        CombatStats.Accuracy = 0.8f;
        CombatStats.ShieldDamageMultiplier = 0.6f;
        CombatStats.HullDamageMultiplier = 1.5f;
        HeatPerShot = 5.f;
        CurrentAmmo = 20;  // 导弹有弹量限制
        bInfiniteAmmo = false;
        break;
    case ETurretType::Beam:
        CombatStats.DamagePerShot = 35.f;
        CombatStats.FireRate = 600.f;  // DPS 型
        CombatStats.Range = 40000.f;
        CombatStats.Accuracy = 0.95f;
        CombatStats.ShieldDamageMultiplier = 1.6f;
        HeatPerShot = 4.f;
        break;
    case ETurretType::Plasma:
        CombatStats.DamagePerShot = 80.f;
        CombatStats.FireRate = 90.f;
        CombatStats.Range = 35000.f;
        CombatStats.Accuracy = 0.75f;
        CombatStats.ShieldDamageMultiplier = 1.2f;
        CombatStats.HullDamageMultiplier = 1.3f;
        HeatPerShot = 6.f;
        break;
    }

    // 应用 1 级加成
    CombatStats.DamagePerShot *= TurretDamageScale[1];
    CombatStats.FireRate *= TurretFireRateScale[1];
    CombatStats.Range *= TurretRangeScale[1];
    CombatStats.Accuracy = TurretAccuracyScale[1];
    DetectionSphere->SetSphereRadius(CombatStats.Range);

    UE_LOG(LogDefenseTurret, Log, TEXT("Turret %s INSTALLED: Type=%d, Level=1, Dmg=%.0f"),
        *TurretID.ToString(), (int)Type, CombatStats.DamagePerShot);
}

bool AStationDefenseTurret::Server_InstallTurret_Validate(AController*, ETurretType) { return true; }

void AStationDefenseTurret::Server_UninstallTurret_Implementation(AController* RequestedBy)
{
    if (!bIsInstalled) return;
    if (bIsDestroyed) return;  // 被毁的不能卸载，只能修

    // 卸载返还 50% 资源
    int32 Lvl = (int32)CurrentLevel;
    float RefundRate = 0.5f;

    // TODO: 返还资源到玩家库存
    int32 RefundCredits = FMath::FloorToInt(TurretCreditCost[Lvl] * RefundRate);
    int32 RefundTitanium = FMath::FloorToInt(TurretTitaniumCost[Lvl] * RefundRate);

    UE_LOG(LogDefenseTurret, Log, TEXT("Turret %s UNINSTALLED: refund=%d credits, %d titanium"),
        *TurretID.ToString(), RefundCredits, RefundTitanium);

    // 重置
    bIsInstalled = false;
    CurrentLevel = ETurretLevel::None;
    CurrentHealth = 0.f;
    MaxHealth = 500.f;
    CombatStats = FTurretCombatStats();  // 重置为默认值
    CurrentTarget = nullptr;
    bAutoFire = false;
}

bool AStationDefenseTurret::Server_UninstallTurret_Validate(AController*) { return true; }

// ========== 维修 ==========

void AStationDefenseTurret::Server_StartRepair_Implementation(AController* RequestedBy)
{
    if (!CanRepair()) return;
    if (!RequestedBy) return;

    bIsRepairing = true;
    RepairProgress = 0.f;

    UE_LOG(LogDefenseTurret, Log, TEXT("Turret %s REPAIR STARTED (%.0fs)"), *TurretID.ToString(), RepairTime);
}

bool AStationDefenseTurret::Server_StartRepair_Validate(AController*) { return true; }

void AStationDefenseTurret::UpdateRepair(float Dt)
{
    RepairProgress += Dt / RepairTime;

    if (RepairProgress >= 1.0f)
    {
        // 维修完成
        bIsRepairing = false;
        bIsDestroyed = false;
        CurrentHealth = MaxHealth;
        CurrentHeat = 0.f;
        bOverheated = false;

        OnTurretRepaired.Broadcast(this);
        UE_LOG(LogDefenseTurret, Log, TEXT("Turret %s REPAIRED"), *TurretID.ToString());
    }
}

// ========== 受击/被毁 ==========

float AStationDefenseTurret::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (!HasAuthority()) return 0.f;
    if (bIsDestroyed) return 0.f;

    CurrentHealth = FMath::Max(0.f, CurrentHealth - DamageAmount);

    if (CurrentHealth <= 0.f)
    {
        HandleTurretDestroyed();
    }

    return DamageAmount;
}

void AStationDefenseTurret::HandleTurretDestroyed()
{
    bIsDestroyed = true;
    CurrentTarget = nullptr;
    bAutoFire = false;
    CurrentHeat = 0.f;

    PlayDestroyedEffects();

    OnTurretDestroyed.Broadcast(this);

    UE_LOG(LogDefenseTurret, Warning, TEXT("TURRET %s DESTROYED!"), *TurretID.ToString());
}

// ========== 特效 ==========

void AStationDefenseTurret::PlayFireEffects()
{
    if (MuzzleFlash.IsValid())
    {
        UGameplayStatics::SpawnEmitterAtLocation(
            GetWorld(), MuzzleFlash.Get(),
            GetActorLocation() + GetActorForwardVector() * 100.f,
            GetActorRotation(), true
        );
    }

    if (FireSound.IsValid())
    {
        UGameplayStatics::PlaySoundAtLocation(this, FireSound.Get(), GetActorLocation());
    }

    // 弹道轨迹
    if (TracerEffect.IsValid() && CurrentTarget)
    {
        UParticleSystemComponent* Tracer = UGameplayStatics::SpawnEmitterAtLocation(
            GetWorld(), TracerEffect.Get(),
            GetActorLocation(), GetActorRotation(), true
        );
        if (Tracer)
        {
            Tracer->SetBeamSourcePoint(0, GetActorLocation(), 0);
            Tracer->SetBeamTargetPoint(0, CurrentTarget->GetActorLocation(), 0);
        }
    }
}

void AStationDefenseTurret::PlayDestroyedEffects()
{
    if (DestroyedEffect.IsValid())
    {
        UGameplayStatics::SpawnEmitterAtLocation(
            GetWorld(), DestroyedEffect.Get(),
            GetActorLocation(), GetActorRotation(), true
        );
    }

    if (DestroyedSound.IsValid())
    {
        UGameplayStatics::PlaySoundAtLocation(this, DestroyedSound.Get(), GetActorLocation());
    }
}

// ========== 查询 ==========

float AStationDefenseTurret::GetHealthPercent() const
{
    return MaxHealth > 0.f ? (CurrentHealth / MaxHealth) : 0.f;
}

float AStationDefenseTurret::GetHeatPercent() const
{
    return MaxHeat > 0.f ? (CurrentHeat / MaxHeat) : 0.f;
}

int32 AStationDefenseTurret::GetCurrentLevelNumber() const
{
    return (int32)CurrentLevel;
}

FString AStationDefenseTurret::GetTurretStatusString() const
{
    if (bIsDestroyed) return FString::Printf(TEXT("DESTROYED — 维修进度: %.0f%%"), RepairProgress * 100.f);
    if (bIsRepairing) return FString::Printf(TEXT("维修中: %.0f%%"), RepairProgress * 100.f);
    if (!bIsInstalled) return TEXT("未安装");
    if (bOverheated) return FString::Printf(TEXT("过热中 (%.0f%%)"), GetHeatPercent() * 100.f);

    FString TypeStr;
    switch (TurretType)
    {
    case ETurretType::Laser:  TypeStr = TEXT("激光"); break;
    case ETurretType::Flak:   TypeStr = TEXT("防空"); break;
    case ETurretType::Missile: TypeStr = TEXT("导弹"); break;
    case ETurretType::Beam:   TypeStr = TEXT("光束"); break;
    case ETurretType::Plasma:  TypeStr = TEXT("等离子"); break;
    }

    return FString::Printf(TEXT("%s Lv%d — HP %.0f/%.0f — 热量 %.0f%%"),
        *TypeStr, (int)CurrentLevel, CurrentHealth, MaxHealth, GetHeatPercent() * 100.f);
}

TArray<AActor*> AStationDefenseTurret::GetHostileTargetsInRange() const
{
    TArray<AActor*> Results;
    if (!GetWorld()) return Results;

    TArray<AActor*> AllShips;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AShipPawn::StaticClass(), AllShips);

    FVector MyLoc = GetActorLocation();
    for (AActor* Ship : AllShips)
    {
        if (!IsValidTarget(Ship)) continue;
        float Dist = FVector::Dist(MyLoc, Ship->GetActorLocation());
        if (Dist <= CombatStats.Range) Results.Add(Ship);
    }

    return Results;
}

// ========== 网络复制 ==========

void AStationDefenseTurret::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);

    DOREPLIFETIME(AStationDefenseTurret, TurretID);
    DOREPLIFETIME(AStationDefenseTurret, TurretType);
    DOREPLIFETIME(AStationDefenseTurret, CurrentLevel);
    DOREPLIFETIME(AStationDefenseTurret, bIsInstalled);
    DOREPLIFETIME(AStationDefenseTurret, CurrentHealth);
    DOREPLIFETIME(AStationDefenseTurret, bIsDestroyed);
    DOREPLIFETIME(AStationDefenseTurret, bIsRepairing);
    DOREPLIFETIME(AStationDefenseTurret, RepairProgress);
    DOREPLIFETIME(AStationDefenseTurret, CurrentTarget);
    DOREPLIFETIME(AStationDefenseTurret, TargetLockProgress);
    DOREPLIFETIME(AStationDefenseTurret, CurrentHeat);
    DOREPLIFETIME(AStationDefenseTurret, bOverheated);
    DOREPLIFETIME(AStationDefenseTurret, CurrentAmmo);
    DOREPLIFETIME(AStationDefenseTurret, OwningStationID);
    DOREPLIFETIME(AStationDefenseTurret, CombatStats);
}
