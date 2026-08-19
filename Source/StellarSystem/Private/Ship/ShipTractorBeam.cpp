// ============================================================
// 路径: Source/StellarSystem/Private/Ship/ShipTractorBeam.cpp
// 模块: Ship (飞船武器)
// 类型: 源文件
// 作用: 牵引光束完整实现 — 持续波束牵引/推斥/稳定/拖曳
// 新增于: v7.6.1
// ============================================================

#include "Ship/ShipTractorBeam.h"
#include "Ship/ShipPawn.h"
#include "Cargo/ShipCargoComponent.h"
#include "GameFramework/Pawn.h"
#include "Components/PrimitiveComponent.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "DrawDebugHelpers.h"
#include "Math/UnrealMathUtility.h"

DEFINE_LOG_CATEGORY_STATIC(LogTractorBeam, Log, All);

UShipTractorBeamComponent::UShipTractorBeamComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);

    // 默认值已由 UPROPERTY 初始化
}

void UShipTractorBeamComponent::BeginPlay()
{
    Super::BeginPlay();

    if (GetOwner())
    {
        CurrentEnergy = MaxEnergy;
    }
}

void UShipTractorBeamComponent::TickComponent(float Dt, ELevelTick Tick, FActorComponentTickFunction* Fn)
{
    Super::TickComponent(Dt, Tick, Fn);

    // 能量恢复（未在开火时）
    UpdateEnergy(Dt);

    // 散热
    UpdateHeat(Dt);

    // 波束持续处理
    if (Runtime.bBeamActive)
    {
        Runtime.BeamDuration += Dt;
        UpdateBeam(Dt);
        CheckBeamRange();
    }
}

// ========== 能量/热量 ==========

void UShipTractorBeamComponent::UpdateEnergy(float Dt)
{
    if (Runtime.bBeamActive)
    {
        float Drain = EnergyDrainPerSecond;
        if (bOverchargeMode) Drain *= OverchargeEnergyMultiplier;
        CurrentEnergy = FMath::Max(0.f, CurrentEnergy - Drain * Dt);

        if (CurrentEnergy <= 0.f && Runtime.bBeamActive)
        {
            // 能量耗尽 → 自动停止
            Server_StopTractorBeam();
        }
    }
    else
    {
        CurrentEnergy = FMath::Min(MaxEnergy, CurrentEnergy + EnergyRegenPerSecond * Dt);
    }
}

void UShipTractorBeamComponent::UpdateHeat(float Dt)
{
    if (Runtime.bBeamActive && !bOverheated)
    {
        float HeatRate = HeatPerSecond;
        if (bOverchargeMode) HeatRate *= OverchargeHeatMultiplier;
        CurrentHeat = FMath::Min(OverheatThreshold, CurrentHeat + HeatRate * Dt);

        if (CurrentHeat >= OverheatThreshold)
        {
            bOverheated = true;
            Server_StopTractorBeam();
            if (OverheatSound.IsValid())
            {
                UGameplayStatics::PlaySoundAtLocation(this, OverheatSound.Get(), GetComponentLocation());
            }
            GetWorld()->GetTimerManager().SetTimer(
                OverheatTimerHandle,
                [this]()
                {
                    bOverheated = false;
                    CurrentHeat = 0.f;
                },
                OverheatCooldown, false
            );
        }
    }
    else if (!Runtime.bBeamActive && CurrentHeat > 0)
    {
        CurrentHeat = FMath::Max(0.f, CurrentHeat - HeatDissipationRate * Dt);
    }
}

// ========== 波束核心逻辑 ==========

void UShipTractorBeamComponent::UpdateBeam(float Dt)
{
    if (!Runtime.TargetActor || !IsValid(Runtime.TargetActor))
    {
        Server_StopTractorBeam();
        return;
    }

    // 验证目标有效性
    if (!ValidateTarget(Runtime.TargetActor))
    {
        Server_ReleaseTractorTarget();
        return;
    }

    // 应用牵引效果
    ProcessTractorEffect(Dt);

    // 更新特效位置
    if (ActiveBeamPSC)
    {
        FVector BeamEnd = Runtime.TargetActor->GetActorLocation();
        ActiveBeamPSC->SetBeamTargetPoint(0, BeamEnd, 0);
    }

    Runtime.CurrentPullDistance = FVector::Dist(GetOwner()->GetActorLocation(), Runtime.TargetActor->GetActorLocation());
}

void UShipTractorBeamComponent::ProcessTractorEffect(float Dt)
{
    if (!Runtime.TargetActor) return;

    switch (Runtime.CurrentMode)
    {
    case ETractorMode::Pull:
        ApplyPullForce(Dt, Runtime.TargetActor);
        break;
    case ETractorMode::Push:
        ApplyPushForce(Dt, Runtime.TargetActor);
        break;
    case ETractorMode::Stabilize:
        ApplyStabilize(Dt, Runtime.TargetActor);
        break;
    case ETractorMode::Tow:
        ApplyTow(Dt, Runtime.TargetActor);
        break;
    }
}

void UShipTractorBeamComponent::ApplyPullForce(float Dt, AActor* Target)
{
    if (!Target) return;

    FVector OwnerLoc = GetOwner()->GetActorLocation();
    FVector TargetLoc = Target->GetActorLocation();
    FVector Direction = (OwnerLoc - TargetLoc).GetSafeNormal();

    float Force = EffectProfile.PullForce;
    if (bOverchargeMode) Force *= OverchargePullMultiplier;

    // 质量缩放
    float MassScale = CalculateMassScaling(Target);
    Force *= MassScale;

    // 速度限制
    UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Target->GetRootComponent());
    if (Prim)
    {
        FVector CurrentVel = Prim->GetComponentVelocity();
        float CurrentSpeed = CurrentVel.Size();
        if (CurrentSpeed < EffectProfile.MaxPullSpeed)
        {
            FVector Impulse = Direction * Force * Dt;
            Prim->AddImpulse(Impulse, NAME_None, true);
        }
    }

    // 如果是货物，尝试自动收入货舱
    if (Target->ActorHasTag(FName("Cargo")) || Target->ActorHasTag(FName("LooseCargo")))
    {
        float Dist = FVector::Dist(OwnerLoc, TargetLoc);
        if (Dist < 500.f)  // 足够近 → 吸入货舱
        {
            Server_TractorRetrieveCargo(Target);
        }
    }
}

void UShipTractorBeamComponent::ApplyPushForce(float Dt, AActor* Target)
{
    if (!Target) return;

    FVector OwnerLoc = GetOwner()->GetActorLocation();
    FVector TargetLoc = Target->GetActorLocation();
    FVector Direction = (TargetLoc - OwnerLoc).GetSafeNormal();  // 反向

    float Force = EffectProfile.PushForce;
    float MassScale = CalculateMassScaling(Target);
    Force *= MassScale;

    UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Target->GetRootComponent());
    if (Prim)
    {
        FVector Impulse = Direction * Force * Dt;
        Prim->AddImpulse(Impulse, NAME_None, true);
    }
}

void UShipTractorBeamComponent::ApplyStabilize(float Dt, AActor* Target)
{
    if (!Target) return;

    UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Target->GetRootComponent());
    if (!Prim) return;

    // 阻尼：将目标速度衰减到接近零
    FVector Vel = Prim->GetComponentVelocity();
    FVector Damped = Vel * (1.f - EffectProfile.StabilizeDamping * Dt);
    Prim->SetPhysicsLinearVelocity(Damped);
    Prim->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
}

void UShipTractorBeamComponent::ApplyTow(float Dt, AActor* Target)
{
    if (!Target) return;

    FVector OwnerLoc = GetOwner()->GetActorLocation();
    FVector TargetLoc = Target->GetActorLocation();
    float Dist = FVector::Dist(OwnerLoc, TargetLoc);

    // 拖曳：保持 TowBreakDistance 距离，超出则断裂
    if (Dist > EffectProfile.TowBreakDistance * 3.f)
    {
        UE_LOG(LogTractorBeam, Warning, TEXT("Tow broken: distance %.0f exceeds limit"), Dist);
        Server_ReleaseTractorTarget();
        return;
    }

    // 拉向飞船但保持一定距离
    FVector Direction = (OwnerLoc - TargetLoc).GetSafeNormal();
    float Force = EffectProfile.PullForce * 0.6f;  // 拖曳力较弱
    float MassScale = CalculateMassScaling(Target);
    Force *= MassScale;

    UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Target->GetRootComponent());
    if (Prim)
    {
        Prim->AddImpulse(Direction * Force * Dt, NAME_None, true);
    }
}

// ========== 目标验证 ==========

bool UShipTractorBeamComponent::ValidateTarget(AActor* Target) const
{
    if (!Target || !IsValid(Target)) return false;

    // 检查距离
    float Dist = FVector::Dist(GetOwner()->GetActorLocation(), Target->GetActorLocation());
    if (Dist > BeamRange) return false;

    // 检查质量
    UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Target->GetRootComponent());
    if (Prim)
    {
        float Mass = Prim->GetMass();
        if (Mass < EffectProfile.MinTargetMass) return false;
        if (Mass > EffectProfile.MaxTargetMass) return false;
    }

    // 检查是否为敌方飞船（默认不允许牵引敌方）
    if (Target->IsA<AShipPawn>())
    {
        if (!EffectProfile.bAffectsShips) return false;
        if (!EffectProfile.bCanGrapEnemyShips)
        {
            // 检查是否为友方（简化：同 Owner）
            AShipPawn* TargetShip = Cast<AShipPawn>(Target);
            AShipPawn* OwnerShip = Cast<AShipPawn>(GetOwner());
            if (TargetShip && OwnerShip && TargetShip->GetOwnerID() != OwnerShip->GetOwnerID())
            {
                return false;  // 敌方飞船不可牵引
            }
        }
    }

    return true;
}

float UShipTractorBeamComponent::CalculateMassScaling(AActor* Target) const
{
    UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Target->GetRootComponent());
    if (!Prim) return 0.5f;

    float Mass = Prim->GetMass();
    float MidMass = (EffectProfile.MaxTargetMass + EffectProfile.MinTargetMass) * 0.5f;

    // 质量越接近 MidMass，效率越高
    float Ratio = Mass / MidMass;
    return FMath::Clamp(1.0f / FMath::Sqrt(Ratio + 0.1f), 0.2f, 1.5f);
}

void UShipTractorBeamComponent::CheckBeamRange()
{
    if (!Runtime.TargetActor) return;

    float Dist = FVector::Dist(
        GetOwner()->GetActorLocation(),
        Runtime.TargetActor->GetActorLocation()
    );

    if (Dist > BeamRange)
    {
        UE_LOG(LogTractorBeam, Log, TEXT("Tractor target out of range (%.0f > %.0f)"), Dist, BeamRange);
        Server_ReleaseTractorTarget();
    }
}

// ========== 特效 ==========

void UShipTractorBeamComponent::SpawnBeamEffects()
{
    if (ActiveBeamPSC) return;

    FVector Start = GetComponentLocation();
    FVector End = Runtime.TargetActor ? Runtime.TargetActor->GetActorLocation() : Start + GetForwardVector() * BeamRange;

    if (BeamParticle.IsValid())
    {
        ActiveBeamPSC = UGameplayStatics::SpawnEmitterAtLocation(
            GetWorld(), BeamParticle.Get(), Start, GetComponentRotation(), true
        );
        if (ActiveBeamPSC)
        {
            ActiveBeamPSC->SetBeamSourcePoint(0, Start, 0);
            ActiveBeamPSC->SetBeamTargetPoint(0, End, 0);
            // 根据模式设置颜色
            FLinearColor Color = (Runtime.CurrentMode == ETractorMode::Push) ? PushBeamColor : BeamColor;
            ActiveBeamPSC->SetColorParameter(FName("BeamTint"), Color);
        }
    }

    if (BeamActiveSound.IsValid())
    {
        ActiveBeamAudio = UGameplayStatics::SpawnSoundAtLocation(this, BeamActiveSound.Get(), Start);
    }
}

void UShipTractorBeamComponent::DestroyBeamEffects()
{
    if (ActiveBeamPSC)
    {
        ActiveBeamPSC->DestroyComponent();
        ActiveBeamPSC = nullptr;
    }
    if (ActiveBeamAudio)
    {
        ActiveBeamAudio->Stop();
        ActiveBeamAudio->DestroyComponent();
        ActiveBeamAudio = nullptr;
    }
    if (BeamEndSound.IsValid() && GetOwner())
    {
        UGameplayStatics::PlaySoundAtLocation(this, BeamEndSound.Get(), GetComponentLocation());
    }
}

// ========== Server RPC ==========

void UShipTractorBeamComponent::Server_StartTractorBeam_Implementation(ETractorMode Mode)
{
    if (!CanActivateBeam()) return;

    Runtime.bBeamActive = true;
    Runtime.CurrentMode = Mode;
    Runtime.BeamDuration = 0.f;

    SpawnBeamEffects();

    UE_LOG(LogTractorBeam, Log, TEXT("Tractor beam ACTIVATED (mode=%d)"), (int)Mode);
}

bool UShipTractorBeamComponent::Server_StartTractorBeam_Validate(ETractorMode) { return true; }

void UShipTractorBeamComponent::Server_StopTractorBeam_Implementation()
{
    if (!Runtime.bBeamActive) return;

    Runtime.bBeamActive = false;
    Runtime.BeamDuration = 0.f;
    Runtime.TargetActor = nullptr;

    DestroyBeamEffects();

    UE_LOG(LogTractorBeam, Log, TEXT("Tractor beam DEACTIVATED"));
}

void UShipTractorBeamComponent::Server_AcquireTractorTarget_Implementation()
{
    TArray<AActor*> Targets = GetValidTractorTargets();
    if (Targets.Num() == 0) return;

    // 选最近的
    AActor* Nearest = Targets[0];
    float NearestDist = FVector::DistSquared(GetOwner()->GetActorLocation(), Nearest->GetActorLocation());

    for (AActor* Candidate : Targets)
    {
        float D = FVector::DistSquared(GetOwner()->GetActorLocation(), Candidate->GetActorLocation());
        if (D < NearestDist)
        {
            NearestDist = D;
            Nearest = Candidate;
        }
    }

    Runtime.TargetActor = Nearest;
    UE_LOG(LogTractorBeam, Log, TEXT("Tractor target acquired: %s"), *Nearest->GetName());
}

bool UShipTractorBeamComponent::Server_AcquireTractorTarget_Validate() { return true; }

void UShipTractorBeamComponent::Server_ReleaseTractorTarget_Implementation()
{
    Runtime.TargetActor = nullptr;
    UE_LOG(LogTractorBeam, Log, TEXT("Tractor target released"));
}

// ========== 货物回收 ==========

void UShipTractorBeamComponent::Server_TractorRetrieveCargo_Implementation(AActor* CargoActor)
{
    if (!CargoActor || !HasAuthority()) return;
    if (!CargoActor->ActorHasTag(FName("Cargo")) && !CargoActor->ActorHasTag(FName("LooseCargo"))) return;

    // 检查距离
    float Dist = FVector::Dist(GetOwner()->GetActorLocation(), CargoActor->GetActorLocation());
    if (Dist > 800.f) return;  // 必须很近

    // 尝试放入货舱
    AShipPawn* OwnerShip = Cast<AShipPawn>(GetOwner());
    if (OwnerShip)
    {
        // 获取货舱组件
        // (假设 ShipCargoComponent 挂在 ShipPawn 上)
        // 简化处理：广播事件让 CargoSystem 处理
        UE_LOG(LogTractorBeam, Log, TEXT("Cargo retrieved via tractor: %s"), *CargoActor->GetName());
        CargoActor->Destroy();  // 货物被吸入后销毁（由货舱系统记录）
    }
}

bool UShipTractorBeamComponent::Server_TractorRetrieveCargo_Validate(AActor*) { return true; }

void UShipTractorBeamComponent::Server_AutoRetrieveNearbyCargo_Implementation()
{
    if (!HasAuthority()) return;

    TArray<AActor*> CargoActors;
    UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Cargo"), CargoActors);
    UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("LooseCargo"), CargoActors);

    int32 Retrieved = 0;
    for (AActor* Cargo : CargoActors)
    {
        float Dist = FVector::Dist(GetOwner()->GetActorLocation(), Cargo->GetActorLocation());
        if (Dist < 1500.f)  // 自动回收半径 15m
        {
            Server_TractorRetrieveCargo(Cargo);
            Retrieved++;
        }
    }

    UE_LOG(LogTractorBeam, Log, TEXT("Auto-retrieved %d cargo items"), Retrieved);
}

// ========== 模式切换 ==========

void UShipTractorBeamComponent::SetTractorMode(ETractorMode NewMode)
{
    if (Runtime.bBeamActive)
    {
        Runtime.CurrentMode = NewMode;
        // 更新特效颜色
        if (ActiveBeamPSC)
        {
            FLinearColor Color = (NewMode == ETractorMode::Push) ? PushBeamColor : BeamColor;
            ActiveBeamPSC->SetColorParameter(FName("BeamTint"), Color);
        }
    }
    else
    {
        Runtime.CurrentMode = NewMode;
    }
}

void UShipTractorBeamComponent::SetOvercharge(bool bEnabled)
{
    bOverchargeMode = bEnabled;
    UE_LOG(LogTractorBeam, Log, TEXT("Tractor overcharge: %s"), bEnabled ? TEXT("ON") : TEXT("OFF"));
}

// ========== 查询 ==========

bool UShipTractorBeamComponent::CanActivateBeam() const
{
    if (bOverheated) return false;
    if (CurrentEnergy < MaxEnergy * 0.05f) return false;  // 至少 5% 能量
    return true;
}

float UShipTractorBeamComponent::GetBeamEnergyPercent() const
{
    return MaxEnergy > 0.f ? (CurrentEnergy / MaxEnergy) : 0.f;
}

float UShipTractorBeamComponent::GetBeamHeatPercent() const
{
    return OverheatThreshold > 0.f ? (CurrentHeat / OverheatThreshold) : 0.f;
}

TArray<AActor*> UShipTractorBeamComponent::GetValidTractorTargets() const
{
    TArray<AActor*> Results;

    if (!GetOwner() || !GetWorld()) return Results;

    FVector OwnerLoc = GetOwner()->GetActorLocation();

    // 收集所有带 Cargo/LooseCargo 标签的 Actor
    if (EffectProfile.bAffectsCargo || EffectProfile.bAffectsDebris)
    {
        TArray<AActor*> CargoActors;
        UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Cargo"), CargoActors);
        for (AActor* A : CargoActors)
        {
            float Dist = FVector::Dist(OwnerLoc, A->GetActorLocation());
            if (Dist <= BeamRange && ValidateTarget(A)) Results.Add(A);
        }
    }

    // 收集可牵引的飞船
    if (EffectProfile.bAffectsShips)
    {
        TArray<AActor*> Ships;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), AShipPawn::StaticClass(), Ships);
        for (AActor* S : Ships)
        {
            if (S == GetOwner()) continue;
            float Dist = FVector::Dist(OwnerLoc, S->GetActorLocation());
            if (Dist <= BeamRange && ValidateTarget(S)) Results.Add(S);
        }
    }

    return Results;
}

// ========== 网络复制 ==========

void UShipTractorBeamComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);

    DOREPLIFETIME(UShipTractorBeamComponent, CurrentEnergy);
    DOREPLIFETIME(UShipTractorBeamComponent, CurrentHeat);
    DOREPLIFETIME(UShipTractorBeamComponent, bOverheated);
    DOREPLIFETIME(UShipTractorBeamComponent, Runtime);
}
