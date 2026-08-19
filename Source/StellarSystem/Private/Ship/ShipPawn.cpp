// ============================================================
// 路径: Source/StellarSystem/Private/Ship/ShipPawn.cpp
// 作用: 飞船驾驶完整实现 (6DOF + 跃迁 + 护盾 + 自动驾驶)
// 修改于: v7.6 (修复跃迁速度/移动目标追踪/自动跃迁触发/配置保存加载)
// ============================================================

#include "Ship/ShipPawn.h"
#include "Ship/ShipWeapons.h"
#include "Ship/ShipLoadout.h"
#include "Ship/ShipHUD.h"
#include "Ship/ProceduralShip.h"
#include "Core/StellarGameMode.h"
#include "Planet/ProceduralPlanet.h"
#include "Character/MyCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "EnhancedInputComponent.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Math/UnrealMathUtility.h"
#include "TimerManager.h"

// ======================== 构造 ========================

AShipPawn::AShipPawn()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;

    ShipMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShipMesh"));
    ShipMesh->SetupAttachment(RootComponent);
    ShipMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
    CollisionBox->SetupAttachment(ShipMesh);
    CollisionBox->SetBoxExtent(FVector(200.f, 200.f, 100.f));

    Movement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("Movement"));
    Movement->MaxSpeed = MaxSpeed;
    Movement->Acceleration = Acceleration;
    Movement->Deceleration = Acceleration * 0.8f;

    WeaponsComp = CreateDefaultSubobject<UShipWeaponsComponent>(TEXT("Weapons"));
    LoadoutComp = CreateDefaultSubobject<UShipLoadoutComponent>(TEXT("Loadout"));
    HUDComp = CreateDefaultSubobject<UShipHUDComponent>(TEXT("HUD"));
    WarpVFXComp = CreateDefaultSubobject<UWarpVFXIntegration>(TEXT("WarpVFX"));
}

// ======================== 生命周期 ========================

void AShipPawn::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        // 注册到 GameMode
        if (AStellarGameMode* GM = Cast<AStellarGameMode>(GetWorld()->GetAuthGameMode()))
        {
            GM->RegisterShip(this);
        }

        // 初始化保存配置(从当前参数)
        SavedConfig.ShipClassID = FName(*GetName());
        SavedConfig.MaxSpeed = MaxSpeed;
        SavedConfig.Acceleration = Acceleration;
        SavedConfig.RotationSpeed = RotationSpeed;
        SavedConfig.WarpSpeed = WarpSpeed;
        SavedConfig.WarpAcceleration = WarpAcceleration;
        SavedConfig.MaxWarpRange = MaxWarpRange;
        SavedConfig.MaxFuel = MaxFuel;
        SavedConfig.ShieldMax = ShieldMax;
        SavedConfig.HullMax = HullIntegrity;
    }

    // 初始化护盾
    ShieldIntegrity = ShieldMax;
}

void AShipPawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(AShipPawn, FlightMode);
    DOREPLIFETIME(AShipPawn, WarpPhase);
    DOREPLIFETIME(AShipPawn, CurrentSpeed);
    DOREPLIFETIME(AShipPawn, CurrentFuel);
    DOREPLIFETIME(AShipPawn, MaxFuel);
    DOREPLIFETIME(AShipPawn, HullIntegrity);
    DOREPLIFETIME(AShipPawn, ShieldIntegrity);
    DOREPLIFETIME(AShipPawn, ShieldMax);
    DOREPLIFETIME(AShipPawn, WarpProgress);
    DOREPLIFETIME(AShipPawn, WarpTarget);
    DOREPLIFETIME(AShipPawn, WarpDestType);
    DOREPLIFETIME(AShipPawn, WarpTargetLocation);
    DOREPLIFETIME(AShipPawn, bWarpTargetIsMoving);
    DOREPLIFETIME(AShipPawn, SavedConfig);
}

// ======================== Tick ========================

void AShipPawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 跃迁更新
    if (WarpPhase != EWarpPhase::Idle && WarpPhase != EWarpPhase::Arrived)
    {
        UpdateWarp(DeltaTime);
        return; // 跃迁中不做常规飞行
    }

    // 自动驾驶
    if (FlightMode == EShipFlightMode::Autopilot && AutopilotTarget)
    {
        UpdateAutopilot(DeltaTime);
    }

    // 常规飞行输入
    if (FlightMode == EShipFlightMode::Manual)
    {
        ApplyFlightInput(DeltaTime);
    }

    // 护盾回复
    if (ShieldIntegrity < ShieldMax)
    {
        ShieldRegenTimer += DeltaTime;
        if (ShieldRegenTimer >= ShieldRegenDelay)
        {
            ShieldIntegrity = FMath::Min(ShieldMax,
                ShieldIntegrity + ShieldRegenRate * DeltaTime);
        }
    }

    // 燃料消耗(仅推进时)
    if (ThrustVal > 0.1f)
    {
        CurrentFuel = FMath::Max(0.f, CurrentFuel - DeltaTime * 0.5f);
    }

    // 检查离开机库后自动跃迁触发
    CheckAutoWarpTrigger();

    // 更新HUD
    if (HUDComp)
    {
        HUDComp->UpdateHUD();
    }
}

// ======================== 输入 ========================

void AShipPawn::SetupPlayerInputComponent(UInputComponent* IC)
{
    Super::SetupPlayerInputComponent(IC);

    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(IC))
    {
        // 需要在项目 InputConfig 中定义对应 Action
        // 这里保留接口兼容性
    }
}

void AShipPawn::ApplyFlightInput(float DT)
{
    // 推进
    if (FMath::Abs(ThrustVal) > 0.01f && CurrentFuel > 0.f)
    {
        FVector Fwd = GetActorForwardVector();
        Movement->AddInputVector(Fwd * ThrustVal);
        CurrentSpeed = GetVelocity().Size();
    }

    // 平移
    if (FMath::Abs(StrafeVal) > 0.01f)
    {
        FVector Right = GetActorRightVector();
        Movement->AddInputVector(Right * StrafeVal * 0.6f);
    }
    if (FMath::Abs(VerticalVal) > 0.01f)
    {
        FVector Up = GetActorUpVector();
        Movement->AddInputVector(Up * VerticalVal * 0.6f);
    }

    // 旋转
    if (FMath::Abs(YawVal) > 0.01f)
    {
        AddActorLocalRotation(FRotator(0.f, YawVal * RotationSpeed * GetWorld()->GetDeltaSeconds(), 0.f));
    }
    if (FMath::Abs(PitchVal) > 0.01f)
    {
        AddActorLocalRotation(FRotator(PitchVal * RotationSpeed * GetWorld()->GetDeltaSeconds(), 0.f, 0.f));
    }
    if (FMath::Abs(RollVal) > 0.01f)
    {
        AddActorLocalRotation(FRotator(0.f, 0.f, RollVal * RotationSpeed * 0.8f * GetWorld()->GetDeltaSeconds()));
    }
}

// ======================== 输入回调 ========================

void AShipPawn::ThrustInput(const FInputActionValue& Value)
{
    ThrustVal = Value.Get<float>();
}

void AShipPawn::StrafeInput(const FInputActionValue& Value)
{
    StrafeVal = Value.Get<float>();
}

void AShipPawn::VerticalInput(const FInputActionValue& Value)
{
    VerticalVal = Value.Get<float>();
}

void AShipPawn::PitchInput(const FInputActionValue& Value)
{
    PitchVal = Value.Get<float>();
}

void AShipPawn::YawInput(const FInputActionValue& Value)
{
    YawVal = Value.Get<float>();
}

void AShipPawn::RollInput(const FInputActionValue& Value)
{
    RollVal = Value.Get<float>();
}

void AShipPawn::StartWarp()
{
    if (WarpPhase != EWarpPhase::Idle) return;

    AActor* Target = WarpTarget;
    if (!Target) Target = AutopilotTarget;

    if (Target)
    {
        // 根据目标类型选择跃迁方式
        if (Target->IsA<AProceduralPlanet>())
        {
            WarpToPlanet(Target);
        }
        else
        {
            WarpToStation(Target);
        }
    }
}

void AShipPawn::ToggleAutopilot()
{
    if (FlightMode == EShipFlightMode::Autopilot)
    {
        FlightMode = EShipFlightMode::Manual;
    }
    else
    {
        FlightMode = EShipFlightMode::Autopilot;
    }
}

void AShipPawn::ExitShip()
{
    if (AStellarGameMode* GM = Cast<AStellarGameMode>(GetWorld()->GetAuthGameMode()))
    {
        FVector SpawnPos = GetActorLocation() + GetActorRightVector() * 500.f;
        // 实际应 Spawn AMyCharacter 并 Possess
        GM->SpawnCharacterNearShip(this, SpawnPos);
    }
}

void AShipPawn::FireWeapon()
{
    if (WeaponsComp) WeaponsComp->FirePrimary();

    if (WarpVFXComp)
    {
        WarpVFXComp->OnWeaponFired(EAudioCategory::WeaponFire_Laser);
    }
}

void AShipPawn::LockTarget()
{
    if (WeaponsComp) WeaponsComp->CycleLockTarget();

    if (WarpVFXComp)
    {
        WarpVFXComp->OnTargetLocked();
    }
}

// ======================== 跃迁(增强版) ========================

void AShipPawn::WarpToPlanet(AActor* TargetPlanet)
{
    if (!TargetPlanet || !CanWarpTo(TargetPlanet)) return;

    WarpTarget = TargetPlanet;
    WarpTargetLocation = TargetPlanet->GetActorLocation();
    WarpDestType = EWarpDestType::Planet;
    bWarpTargetIsMoving = true; // 行星在公转,需要追踪
    bTrackMovingTarget = true;

    StartWarpSequence(TargetPlanet->GetActorLocation());
}

void AShipPawn::WarpToStation(AActor* TargetStation)
{
    if (!TargetStation || !CanWarpTo(TargetStation)) return;

    WarpTarget = TargetStation;
    WarpTargetLocation = TargetStation->GetActorLocation();
    WarpDestType = EWarpDestType::Station;
    bWarpTargetIsMoving = true; // 空间站随行星公转
    bTrackMovingTarget = true;

    StartWarpSequence(TargetStation->GetActorLocation());
}

void AShipPawn::WarpToSpaceport(AActor* TargetSpaceport)
{
    if (!TargetSpaceport || !CanWarpTo(TargetSpaceport)) return;

    WarpTarget = TargetSpaceport;
    WarpTargetLocation = TargetSpaceport->GetActorLocation();
    WarpDestType = EWarpDestType::Spaceport;
    bWarpTargetIsMoving = false; // 地面建筑固定
    bTrackMovingTarget = false;

    StartWarpSequence(TargetSpaceport->GetActorLocation());
}

void AShipPawn::WarpToCoordinates(const FVector& WorldCoords)
{
    if (!CanWarpToCoordinates(WorldCoords)) return;

    WarpTarget = nullptr;
    WarpTargetLocation = WorldCoords;
    WarpDestType = EWarpDestType::FreeSpace;
    bWarpTargetIsMoving = false;
    bTrackMovingTarget = false;

    StartWarpSequence(WorldCoords);
}

void AShipPawn::WarpToPlayerStructure(FName StructureID)
{
    // 查找玩家主权建筑位置
    if (AStellarGameMode* GM = Cast<AStellarGameMode>(GetWorld()->GetAuthGameMode()))
    {
        FVector Loc = GM->GetPlayerStructureLocation(StructureID);
        if (Loc != FVector::ZeroVector)
        {
            WarpTargetLocation = Loc;
            WarpDestType = EWarpDestType::PlayerStructure;
            bWarpTargetIsMoving = false; // 主权港相对恒星固定
            bTrackMovingTarget = false;

            StartWarpSequence(Loc);
        }
    }
}

void AShipPawn::StartWarpSequence(const FVector& Destination)
{
    WarpStartPos = GetActorLocation();
    WarpEndPos = Destination;

    // 计算跃迁距离和持续时间(关键修复: 使用 WarpSpeed)
    float Dist = FVector::Dist(WarpStartPos, WarpEndPos);

    // 应用速度曲线(如果有)
    float SpeedMultiplier = 1.f;
    if (WarpSpeedCurve)
    {
        float NormalizedDist = FMath::Clamp(Dist / MaxWarpRange, 0.f, 1.f);
        SpeedMultiplier = WarpSpeedCurve->GetFloatValue(NormalizedDist);
    }

    float EffectiveWarpSpeed = WarpSpeed * FMath::Max(SpeedMultiplier, 0.1f);
    WarpDuration = FMath::Clamp(Dist / EffectiveWarpSpeed, 2.f, 15.f);

    // 加速段距离
    WarpPhase = EWarpPhase::Accelerating;
    FlightMode = EShipFlightMode::Warping;
    WarpProgress = 0.f;
    WarpTimer = 0.f;
    CurrentWarpSpeed = 0.f;

    UE_LOG(LogTemp, Log, TEXT("[Ship] Warp initiated → dist=%.0fkm dur=%.1fs speed=%.0fkm/s"),
        Dist * 0.00001f, WarpDuration, EffectiveWarpSpeed * 0.00001f);
}

bool AShipPawn::CanWarpTo(AActor* Target) const
{
    if (!Target) return false;
    float Dist = FVector::Dist(GetActorLocation(), Target->GetActorLocation());
    return Dist <= MaxWarpRange && CurrentFuel > 5.f;
}

bool AShipPawn::CanWarpToCoordinates(const FVector& Coords) const
{
    float Dist = FVector::Dist(GetActorLocation(), Coords);
    return Dist <= MaxWarpRange && CurrentFuel > 5.f;
}

float AShipPawn::GetWarpDurationTo(AActor* Target) const
{
    if (!Target) return 0.f;
    float Dist = FVector::Dist(GetActorLocation(), Target->GetActorLocation());
    return FMath::Clamp(Dist / WarpSpeed, 2.f, 15.f);
}

float AShipPawn::GetWarpDurationToCoords(const FVector& Coords) const
{
    float Dist = FVector::Dist(GetActorLocation(), Coords);
    return FMath::Clamp(Dist / WarpSpeed, 2.f, 15.f);
}

void AShipPawn::UpdateWarp(float DeltaTime)
{
    WarpTimer += DeltaTime;
    WarpProgress = FMath::Clamp(WarpTimer / WarpDuration, 0.f, 1.f);

    // 追踪移动目标(关键修复: 目的地在移动时动态调整终点)
    if (bTrackMovingTarget && WarpTarget)
    {
        TargetUpdateTimer += DeltaTime;
        if (TargetUpdateTimer >= TargetUpdateInterval)
        {
            TargetUpdateTimer = 0.f;
            UpdateMovingTarget();
        }
    }

    // 三段式: 加速 → 巡航 → 减速
    float t = WarpProgress;
    float EasedT = t;

    if (t < 0.2f)
    {
        // 加速段(平滑加速)
        float st = t / 0.2f;
        EasedT = st * st * 0.5f;
        CurrentWarpSpeed = FMath::Lerp(0.f, WarpSpeed, st);
        if (WarpPhase != EWarpPhase::Accelerating)
        {
            WarpPhase = EWarpPhase::Accelerating;
        }
    }
    else if (t < 0.8f)
    {
        // 巡航段(匀速)
        EasedT = 0.1f + (t - 0.2f) / 0.6f * 0.8f;
        CurrentWarpSpeed = WarpSpeed;
        if (WarpPhase != EWarpPhase::Cruising)
        {
            WarpPhase = EWarpPhase::Cruising;
        }
    }
    else
    {
        // 减速段(平滑减速)
        float st = (t - 0.8f) / 0.2f;
        EasedT = 0.9f + st * st * 0.1f;
        CurrentWarpSpeed = FMath::Lerp(WarpSpeed, 0.f, st);
        if (WarpPhase != EWarpPhase::Decelerating)
        {
            WarpPhase = EWarpPhase::Decelerating;
        }
    }

    // 插值位置
    FVector NewPos = FMath::Lerp(WarpStartPos, WarpEndPos, EasedT);
    SetActorLocation(NewPos);

    // 驱动跃迁光效 + 音频
    if (WarpVFXComp)
    {
        FVector WarpDir = (WarpEndPos - WarpStartPos).GetSafeNormal();
        WarpVFXComp->OnWarpProgress(WarpProgress, WarpDir);
    }

    // 消耗燃料(跃迁消耗更高)
    CurrentFuel = FMath::Max(0.f, CurrentFuel - DeltaTime * 2.f);

    if (WarpProgress >= 1.f)
    {
        CompleteWarp();
    }
}

void AShipPawn::UpdateMovingTarget()
{
    if (!WarpTarget) return;

    FVector NewTargetLoc = WarpTarget->GetActorLocation();

    // 计算目标移动速度(用于预判)
    FVector TargetVelocity = (NewTargetLoc - WarpEndPos) / TargetUpdateInterval;

    // 更新终点(加入预判: 剩余飞行时间内目标会移动的距离)
    float RemainingTime = WarpDuration - WarpTimer;
    FVector PredictedOffset = TargetVelocity * RemainingTime * 0.5f; // 50% 预判
    WarpEndPos = NewTargetLoc + PredictedOffset;

    // 更新起始点(当前位置)
    WarpStartPos = GetActorLocation();

    // 重新计算持续时间
    float RemainingDist = FVector::Dist(WarpStartPos, WarpEndPos);
    float NewRemainingDuration = FMath::Clamp(RemainingDist / WarpSpeed, 0.5f, 5.f);

    // 平滑调整 WarpDuration
    float TotalElapsed = WarpTimer;
    float NewTotalDuration = TotalElapsed + NewRemainingDuration;
    if (NewTotalDuration > 0.f)
    {
        WarpDuration = NewTotalDuration;
    }
}

void AShipPawn::CompleteWarp()
{
    WarpPhase = EWarpPhase::Arrived;
    FlightMode = EShipFlightMode::Manual;
    WarpTimer = 0.f;
    WarpProgress = 0.f;
    CurrentWarpSpeed = 0.f;
    bTrackMovingTarget = false;

    // 最终位置修正(吸附到目标)
    if (WarpTarget)
    {
        FVector SnapOffset = FVector::ZeroVector;
        switch (WarpDestType)
        {
        case EWarpDestType::Planet:
            {
                // 停在行星轨道上
                FVector ToPlanet = (WarpTarget->GetActorLocation() - GetActorLocation()).GetSafeNormal();
                FVector OrbitPos = WarpTarget->GetActorLocation() + ToPlanet * 150000.f;
                SetActorLocation(OrbitPos);
            }
            break;
        case EWarpDestType::Station:
            {
                // 停在空间站对接位
                FVector ToStation = (WarpTarget->GetActorLocation() - GetActorLocation()).GetSafeNormal();
                FVector DockPos = WarpTarget->GetActorLocation() + ToStation * 5000.f;
                SetActorLocation(DockPos);
            }
            break;
        case EWarpDestType::Spaceport:
        case EWarpDestType::PlayerStructure:
            {
                // 停在建筑附近
                FVector ToStruct = (WarpTarget->GetActorLocation() - GetActorLocation()).GetSafeNormal();
                FVector ArrivalPos = WarpTarget->GetActorLocation() + ToStruct * 10000.f;
                SetActorLocation(ArrivalPos);
            }
            break;
        case EWarpDestType::FreeSpace:
        case EWarpDestType::Asteroid:
            {
                // 精确到达坐标
                SetActorLocation(WarpTargetLocation);
            }
            break;
        }
    }
    else
    {
        // 无目标, 精确到达坐标
        SetActorLocation(WarpTargetLocation);
    }

    // 触发到达光效 + 音效
    if (WarpVFXComp)
    {
        WarpVFXComp->OnWarpCompleted(GetActorLocation());
    }

    // 到达后减速停下
    Movement->Velocity = FVector::ZeroVector;

    // 记录出发位置(用于离开检测)
    LastDockedLocation = GetActorLocation();

    UE_LOG(LogTemp, Log, TEXT("[Ship] Warp complete → type=%d"), (int)WarpDestType);
    WarpPhase = EWarpPhase::Idle;
}

void AShipPawn::CheckAutoWarpTrigger()
{
    if (WarpPhase != EWarpPhase::Idle) return;
    if (LastDockedLocation.IsZero()) return;

    float DistFromDock = FVector::Dist(GetActorLocation(), LastDockedLocation);
    DistanceFromDock = DistFromDock;

    // 离开机库超过阈值后, 允许跃迁
    if (DistFromDock > AutoWarpTriggerDistance)
    {
        // 通知 HUD 跃迁可用
        if (HUDComp)
        {
            HUDComp->SetWarpAvailable(true);
        }
    }
}

// ======================== 自动驾驶 ========================

void AShipPawn::SetAutopilotTarget(AActor* Target)
{
    AutopilotTarget = Target;
    FlightMode = EShipFlightMode::Autopilot;
}

void AShipPawn::UpdateAutopilot(float DeltaTime)
{
    if (!AutopilotTarget) return;

    FVector ToTarget = AutopilotTarget->GetActorLocation() - GetActorLocation();
    float Dist = ToTarget.Size();

    // 接近目标时减速
    if (Dist < 200000.f) // 2km
    {
        Movement->Velocity *= 0.95f;
        if (Dist < 50000.f)
        {
            FlightMode = EShipFlightMode::Manual;
            Movement->Velocity = FVector::ZeroVector;
            UE_LOG(LogTemp, Log, TEXT("[Ship] Autopilot: arrived"));
        }
        return;
    }

    // 朝向目标
    FVector Dir = ToTarget.GetSafeNormal();
    FRotator TargetRot = Dir.Rotation();
    SetActorRotation(FMath::RInterpTo(GetActorRotation(), TargetRot,
        DeltaTime, 2.f));

    // 推进
    ThrustVal = 1.f;
    ApplyFlightInput(DeltaTime);
}

// ======================== 伤害/维修 ========================

void AShipPawn::TakeDamage(float Amount, bool bIgnoreShields)
{
    if (FlightMode == EShipFlightMode::Dead) return; // 已毁不再受伤

    if (bIgnoreShields || ShieldIntegrity <= 0.f)
    {
        HullIntegrity = FMath::Max(0.f, HullIntegrity - Amount);
    }
    else
    {
        float ShieldDamage = FMath::Min(ShieldIntegrity, Amount);
        ShieldIntegrity -= ShieldDamage;
        Amount -= ShieldDamage;

        if (Amount > 0.f)
        {
            HullIntegrity = FMath::Max(0.f, HullIntegrity - Amount);
        }

        ShieldRegenTimer = 0.f; // 重置护盾回复
    }

    if (HullIntegrity <= 0.f && FlightMode != EShipFlightMode::Dead)
    {
        FlightMode = EShipFlightMode::Dead;
        UE_LOG(LogTemp, Warning, TEXT("[Ship] Destroyed!"));

        // 通知 GameMode 处理保险/残骸
        if (AStellarGameMode* GM = Cast<AStellarGameMode>(GetWorld()->GetAuthGameMode()))
        {
            GM->OnShipDestroyed(this);
        }

        if (WarpVFXComp)
        {
            WarpVFXComp->OnHullCritical();
        }
    }
}

void AShipPawn::Refuel(float Amount)
{
    CurrentFuel = FMath::Min(MaxFuel, CurrentFuel + Amount);
}

void AShipPawn::DockAtStation(AActor* Station)
{
    if (!Station) return;
    FlightMode = EShipFlightMode::Docked;
    Movement->Velocity = FVector::ZeroVector;
    LastDockedLocation = GetActorLocation();

    // 通知 GameMode
    if (AStellarGameMode* GM = Cast<AStellarGameMode>(GetWorld()->GetAuthGameMode()))
    {
        GM->OnShipDocked(this, Station);
    }

    UE_LOG(LogTemp, Log, TEXT("[Ship] Docked at %s"), *Station->GetName());
}

// ======================== 配置保存/加载(索赔核心) ========================

void AShipPawn::SaveShipConfig()
{
    if (!HasAuthority()) return;

    SavedConfig.ShipClassID = FName(*GetName());
    SavedConfig.MaxSpeed = MaxSpeed;
    SavedConfig.Acceleration = Acceleration;
    SavedConfig.RotationSpeed = RotationSpeed;
    SavedConfig.WarpSpeed = WarpSpeed;
    SavedConfig.WarpAcceleration = WarpAcceleration;
    SavedConfig.MaxWarpRange = MaxWarpRange;
    SavedConfig.MaxFuel = MaxFuel;
    SavedConfig.ShieldMax = ShieldMax;
    SavedConfig.HullMax = HullIntegrity;
    SavedConfig.SkinID = LoadoutComp ? LoadoutComp->GetCurrentSkinID() : NAME_None;

    // 保存武器配置
    if (WeaponsComp)
    {
        SavedConfig.EquippedWeaponIDs = WeaponsComp->GetEquippedWeaponIDs();
    }

    // 保存组件耐久
    // (由 DamageSystem 填充)

    UE_LOG(LogTemp, Log, TEXT("[Ship] Config saved for %s"), *GetName());
}

void AShipPawn::LoadShipConfig(const FShipSavedConfig& Config)
{
    if (!HasAuthority()) return;

    MaxSpeed = Config.MaxSpeed;
    Acceleration = Config.Acceleration;
    RotationSpeed = Config.RotationSpeed;
    WarpSpeed = Config.WarpSpeed;
    WarpAcceleration = Config.WarpAcceleration;
    MaxWarpRange = Config.MaxWarpRange;
    MaxFuel = Config.MaxFuel;
    CurrentFuel = MaxFuel; // 新船满燃料
    ShieldMax = Config.ShieldMax;
    ShieldIntegrity = ShieldMax; // 新船满护盾
    HullIntegrity = Config.HullMax; // 新船满HP

    SavedConfig = Config;

    // 恢复武器
    if (WeaponsComp && Config.EquippedWeaponIDs.Num() > 0)
    {
        WeaponsComp->RestoreWeapons(Config.EquippedWeaponIDs);
    }

    // 恢复涂装
    if (LoadoutComp && Config.SkinID != NAME_None)
    {
        LoadoutComp->ApplySkin(Config.SkinID);
    }

    // 恢复组件耐久
    for (const auto& Pair : Config.ComponentHealth)
    {
        // 通知 DamageSystem 恢复
    }

    UE_LOG(LogTemp, Log, TEXT("[Ship] Config loaded for %s (class=%s)"),
        *GetName(), *Config.ShipClassID.ToString());
}

FShipSavedConfig AShipPawn::GetCurrentConfig() const
{
    return SavedConfig;
}
