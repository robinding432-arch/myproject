// ============================================================
// 路径: Source/StellarSystem/Private/Ship/ShipHUD.cpp
// 作用: 飞船 HUD 数据采集实现
// 依赖: Ship/ShipHUD.h, Ship/ShipPawn.h
// ============================================================

#include "Ship/ShipHUD.h"
#include "Ship/ShipPawn.h"
#include "Ship/ShipWeapons.h"
#include "Ship/ShipLoadout.h"
#include "Kismet/GameplayStatics.h"
#include "Math/UnrealMathUtility.h"

UShipHUDComponent::UShipHUDComponent()
{
    PrimaryComponentTick.bCanEverTick = false; // 由 ShipPawn Tick 调用
}

// ======================== 主更新 ========================

void UShipHUDComponent::UpdateHUD()
{
    AShipPawn* Ship = GetOwnerShip();
    if (!Ship) return;

    // 速度
    CurrentHUDData.Speed = Ship->CurrentSpeed;
    CurrentHUDData.MaxSpeed = Ship->MaxSpeed;

    // 燃料
    CurrentHUDData.FuelPercent = (Ship->MaxFuel > 0.f) ?
        (Ship->CurrentFuel / Ship->MaxFuel) * 100.f : 0.f;

    // 护盾
    CurrentHUDData.ShieldPercent = (Ship->ShieldMax > 0.f) ?
        (Ship->ShieldIntegrity / Ship->ShieldMax) * 100.f : 0.f;

    // 船体
    CurrentHUDData.HullPercent = Ship->HullIntegrity;

    // 跃迁
    CurrentHUDData.bIsWarping = (Ship->WarpPhase != EWarpPhase::Idle &&
                                   Ship->WarpPhase != EWarpPhase::Arrived);
    CurrentHUDData.WarpProgress = Ship->WarpProgress;

    if (Ship->WarpTarget)
        CurrentHUDData.WarpTargetName = Ship->WarpTarget->GetName();
    else
        CurrentHUDData.WarpTargetName = TEXT("--");

    // 锁定
    if (UShipWeaponsComponent* W = Ship->WeaponsComp)
    {
        CurrentHUDData.bHasLock = W->CurrentLock.bLocked;
        CurrentHUDData.LockProgress = W->CurrentLock.LockProgress;

        if (W->CurrentLock.Target)
            CurrentHUDData.LockTargetName = W->CurrentLock.Target->GetName();
        else
            CurrentHUDData.LockTargetName = TEXT("--");

        // 武器
        CurrentHUDData.ActiveWeaponIndex = W->ActiveWeaponIndex;
        if (W->WeaponSlots.IsValidIndex(W->ActiveWeaponIndex))
        {
            CurrentHUDData.ActiveWeaponName =
                UEnum::GetValueAsString(W->WeaponSlots[W->ActiveWeaponIndex].Type);
        }
        CurrentHUDData.WeaponCooldownPercent = W->GetCooldownPercent(W->ActiveWeaponIndex);
    }

    // 雷达
    UpdateRadarContacts();

    // 飞行模式
    CurrentHUDData.FlightModeText = GetFlightModeText();

    // 通知 UI 刷新
    OnHUDUpdated.Broadcast();
}

// ======================== 工具 ========================

AShipPawn* UShipHUDComponent::GetOwnerShip() const
{
    return Cast<AShipPawn>(GetOwner());
}

FLinearColor UShipHUDComponent::GetStatusColor(float Percent) const
{
    if (Percent < 20.f) return CriticalColor;
    if (Percent < 50.f) return WarningColor;
    return NormalColor;
}

void UShipHUDComponent::UpdateRadarContacts()
{
    CurrentHUDData.RadarContacts.Reset();

    AShipPawn* Ship = GetOwnerShip();
    if (!Ship) return;

    TArray<AActor*> Contacts;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AShipPawn::StaticClass(), Contacts);

    FVector ShipPos = Ship->GetActorLocation();
    int32 Count = 0;

    for (AActor* A : Contacts)
    {
        if (A == Ship || Count >= MaxRadarContacts) continue;

        float Dist = FVector::Dist(ShipPos, A->GetActorLocation());
        if (Dist > RadarRange) continue;

        FString Contact = FString::Printf(TEXT("%s (%.0fkm)"),
            *A->GetName(), Dist * 0.00001f);
        CurrentHUDData.RadarContacts.Add(Contact);
        Count++;
    }

    // 也加行星
    TArray<AActor*> Planets;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(),
        AProceduralPlanet::StaticClass(), Planets);
    for (AActor* P : Planets)
    {
        if (Count >= MaxRadarContacts) break;
        float Dist = FVector::Dist(ShipPos, P->GetActorLocation());
        if (Dist > RadarRange) continue;

        FString Contact = FString::Printf(TEXT("PLANET %s (%.0fkm)"),
            *P->GetName(), Dist * 0.00001f);
        CurrentHUDData.RadarContacts.Add(Contact);
        Count++;
    }
}

void UShipHUDComponent::DrawRadar(TArray<FVector2D>& OutBlips) const
{
    OutBlips.Reset();
    AShipPawn* Ship = GetOwnerShip();
    if (!Ship) return;

    FVector ShipPos = Ship->GetActorLocation();
    FRotator ShipRot = Ship->GetActorRotation();

    TArray<AActor*> Contacts;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), Contacts);

    for (AActor* A : Contacts)
    {
        if (A == Ship) continue;

        FVector ToActor = A->GetActorLocation() - ShipPos;
        float Dist = ToActor.Size();
        if (Dist > RadarRange) continue;

        // 转换到飞船本地空间
        FVector Local = ShipRot.UnrotateVector(ToActor);
        float Angle = FMath::Atan2(Local.Y, Local.X); // -PI~PI
        float NormalizedDist = FMath::Clamp(Dist / RadarRange, 0.f, 1.f);

        FVector2D Blip(FMath::Cos(Angle) * NormalizedDist,
                       FMath::Sin(Angle) * NormalizedDist);
        OutBlips.Add(Blip);
    }
}

FString UShipHUDComponent::GetWarpStatusText() const
{
    AShipPawn* Ship = GetOwnerShip();
    if (!Ship) return TEXT("");

    switch (Ship->WarpPhase)
    {
        case EWarpPhase::Idle:      return TEXT("Warp Drive: STANDBY");
        case EWarpPhase::Accelerating: return FString::Printf(TEXT("Warp: Accelerating (%.0f%%)"),
                                                    Ship->WarpProgress * 100.f);
        case EWarpPhase::Cruising:   return FString::Printf(TEXT("WARP CRUISE → %s"),
                                                    *CurrentHUDData.WarpTargetName);
        case EWarpPhase::Decelerating: return FString::Printf(TEXT("Warp: Decelerating (%.0f%%)"),
                                                    Ship->WarpProgress * 100.f);
        case EWarpPhase::Arrived:    return TEXT("Warp: ARRIVED");
    }
    return TEXT("");
}

FString UShipHUDComponent::GetLockStatusText() const
{
    if (CurrentHUDData.bHasLock)
        return FString::Printf(TEXT("LOCKED: %s"), *CurrentHUDData.LockTargetName);
    else if (CurrentHUDData.LockProgress > 0.f)
        return FString::Printf(TEXT("LOCKING... %.0f%%"), CurrentHUDData.LockProgress * 100.f);
    return TEXT("NO TARGET");
}

FString UShipHUDComponent::GetFlightModeText() const
{
    AShipPawn* Ship = GetOwnerShip();
    if (!Ship) return TEXT("");

    switch (Ship->FlightMode)
    {
        case EShipFlightMode::Manual:    return TEXT("MANUAL");
        case EShipFlightMode::Autopilot: return TEXT("AUTOPILOT");
        case EShipFlightMode::Warping:   return TEXT("WARPING");
        case EShipFlightMode::Docked:    return TEXT("DOCKED");
        case EShipFlightMode::Dead:      return TEXT("DESTROYED");
    }
    return TEXT("");
}
