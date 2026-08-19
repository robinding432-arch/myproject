// ShipWeapons.cpp
#include "Ship/ShipWeapons.h"
#include "Ship/ShipPawn.h"
#include "Inventory/AmmoAndConsumables.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"

UShipWeaponsComponent::UShipWeaponsComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = true;
}

void UShipWeaponsComponent::BeginPlay()
{
    Super::BeginPlay();
    FiringState.SetNum(WeaponSlots.Num());
}

void UShipWeaponsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    DOREPLIFETIME(UShipWeaponsComponent, WeaponSlots);
    DOREPLIFETIME(UShipWeaponsComponent, FiringState);
}

void UShipWeaponsComponent::TickComponent(float Dt, ELevelTick Tick, FActorComponentTickFunction* Fn)
{
    Super::TickComponent(Dt, Tick, Fn);
    if (!HasAuthority()) return;

    ProcessFiring(Dt);
    ProcessLocking(Dt);

    // 冷却
    for (FShipWeaponSlot& W : WeaponSlots)
    {
        W.CooldownRemaining = FMath::Max(W.CooldownRemaining - Dt, 0.f);
    }
}

void UShipWeaponsComponent::ProcessFiring(float Dt)
{
    AShipPawn* Ship = Cast<AShipPawn>(GetOwner());
    if (!Ship) return;

    for (int32 i = 0; i < WeaponSlots.Num(); ++i)
    {
        if (!FiringState.IsValidIndex(i) || !FiringState[i]) continue;

        FShipWeaponSlot& W = WeaponSlots[i];
        if (W.CooldownRemaining > 0.f) continue;
        if (W.bRequiresLock && !W.bLocked) continue;

        // 检查弹药
        UAmmoInventoryComponent* AmmoInv = Ship->FindComponentByClass<UAmmoInventoryComponent>();
        if (AmmoInv)
        {
            // 简化：用武器类型映射弹药
            EAmmoType AmmoType = EAmmoType::EnergyCell;
            switch (W.Type)
            {
            case EShipWeaponType::Laser:    AmmoType = EAmmoType::EnergyCell; break;
            case EShipWeaponType::Plasma:   AmmoType = EAmmoType::PlasmaPack; break;
            case EShipWeaponType::RailGun:  AmmoType = EAmmoType::RailSlug; break;
            case EShipWeaponType::Missile:  AmmoType = EAmmoType::HomingMissile; break;
            case EShipWeaponType::Torpedo:  AmmoType = EAmmoType::Rocket; break;
            default: break;
            }
            // 检查并消耗
            // AmmoInv->ServerConsumeAmmoOfType(AmmoType, 1); // 需实现
        }

        // 开火
        SpawnProjectile(i);

        // 冷却
        W.CooldownRemaining = 60.f / FMath::Max(W.FireRate, 1.f);

        // 过热
        Ship->CurrentHeat = FMath::Min(Ship->CurrentHeat + W.HeatPerShot, 999.f);

        OnWeaponFired.Broadcast(i);
    }
}

void UShipWeaponsComponent::ProcessLocking(float Dt)
{
    for (int32 i = 0; i < WeaponSlots.Num(); ++i)
    {
        FShipWeaponSlot& W = WeaponSlots[i];
        if (!W.bRequiresLock || !W.CurrentTarget) continue;

        if (!CheckLineOfSight(W.CurrentTarget))
        {
            W.CurrentLockProgress = FMath::Max(W.CurrentLockProgress - Dt * 0.5f, 0.f);
            if (W.CurrentLockProgress <= 0.f && W.bLocked)
            {
                W.bLocked = false;
                OnLockBroken.Broadcast(i);
            }
            continue;
        }

        W.CurrentLockProgress = FMath::Min(W.CurrentLockProgress + Dt / FMath::Max(W.LockTime, 0.1f), 1.f);
        if (W.CurrentLockProgress >= 1.f && !W.bLocked)
        {
            W.bLocked = true;
            OnTargetLocked.Broadcast(i, W.CurrentTarget);
        }
    }
}

void UShipWeaponsComponent::SpawnProjectile(int32 SlotIndex)
{
    if (!WeaponSlots.IsValidIndex(SlotIndex)) return;
    FShipWeaponSlot& W = WeaponSlots[SlotIndex];

    // 简化：直接对目标/前方造成伤害
    if (W.CurrentTarget)
    {
        // 伤害事件（由外部系统监听）
        // Target->TakeDamage(W.Damage, ...);
    }
    else
    {
        // 沿飞船前向发射
        // SpawnActor<AShipProjectile>(...)
    }
}

bool UShipWeaponsComponent::CheckLineOfSight(AActor* Target) const
{
    if (!Target || !GetOwner()) return false;
    FVector Origin = GetOwner()->GetActorLocation();
    FVector Dest = Target->GetActorLocation();
    FVector Dir = (Dest - Origin).GetSafeNormal();
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(GetOwner());
    GetWorld()->LineTraceSingleByChannel(Hit, Origin, Dest, ECC_Visibility, Params);
    return !Hit.bBlockingHit || Hit.GetActor() == Target;
}

float UShipWeaponsComponent::CalculateDamageFalloff(int32 SlotIndex, float Distance) const
{
    if (!WeaponSlots.IsValidIndex(SlotIndex)) return 0.f;
    const FShipWeaponSlot& W = WeaponSlots[SlotIndex];
    float Falloff = 1.f - FMath::Clamp(Distance / W.Range, 0.f, 1.f) * 0.5f;
    return W.Damage * Falloff;
}

// ========== RPCs ==========
void UShipWeaponsComponent::ServerStartFiring_Implementation(int32 SlotIndex)
{
    if (!FiringState.IsValidIndex(SlotIndex)) return;
    FiringState[SlotIndex] = true;
}

void UShipWeaponsComponent::ServerStopFiring_Implementation(int32 SlotIndex)
{
    if (!FiringState.IsValidIndex(SlotIndex)) return;
    FiringState[SlotIndex] = false;
}

void UShipWeaponsComponent::ServerFireSlot_Implementation(int32 SlotIndex)
{
    if (!WeaponSlots.IsValidIndex(SlotIndex)) return;
    FShipWeaponSlot& W = WeaponSlots[SlotIndex];
    W.CooldownRemaining = 60.f / FMath::Max(W.FireRate, 1.f);
    SpawnProjectile(SlotIndex);
    OnWeaponFired.Broadcast(SlotIndex);
}

void UShipWeaponsComponent::ServerAcquireTarget_Implementation(int32 SlotIndex, AActor* Target)
{
    if (!WeaponSlots.IsValidIndex(SlotIndex)) return;
    WeaponSlots[SlotIndex].CurrentTarget = Target;
    WeaponSlots[SlotIndex].CurrentLockProgress = 0.f;
    WeaponSlots[SlotIndex].bLocked = false;
}

void UShipWeaponsComponent::ServerReleaseTarget_Implementation(int32 SlotIndex)
{
    if (!WeaponSlots.IsValidIndex(SlotIndex)) return;
    WeaponSlots[SlotIndex].CurrentTarget = nullptr;
    WeaponSlots[SlotIndex].CurrentLockProgress = 0.f;
    WeaponSlots[SlotIndex].bLocked = false;
}

void UShipWeaponsComponent::ServerInstallWeapon_Implementation(const FShipWeaponSlot& Weapon)
{
    if (WeaponSlots.Num() >= MaxWeaponSlots) return;
    WeaponSlots.Add(Weapon);
    FiringState.Add(false);
}

void UShipWeaponsComponent::ServerRemoveWeapon_Implementation(int32 SlotIndex)
{
    if (!WeaponSlots.IsValidIndex(SlotIndex)) return;
    WeaponSlots.RemoveAt(SlotIndex);
    if (FiringState.IsValidIndex(SlotIndex)) FiringState.RemoveAt(SlotIndex);
}

// ========== 查询 ==========
bool UShipWeaponsComponent::CanFire(int32 SlotIndex) const
{
    if (!WeaponSlots.IsValidIndex(SlotIndex)) return false;
    const FShipWeaponSlot& W = WeaponSlots[SlotIndex];
    if (W.CooldownRemaining > 0.f) return false;
    if (W.bRequiresLock && !W.bLocked) return false;
    return true;
}

float UShipWeaponsComponent::GetDPS(int32 SlotIndex) const
{
    if (!WeaponSlots.IsValidIndex(SlotIndex)) return 0.f;
    const FShipWeaponSlot& W = WeaponSlots[SlotIndex];
    return W.Damage * (W.FireRate / 60.f);
}

AActor* UShipWeaponsComponent::GetCurrentTarget(int32 SlotIndex) const
{
    if (!WeaponSlots.IsValidIndex(SlotIndex)) return nullptr;
    return WeaponSlots[SlotIndex].CurrentTarget;
}

float UShipWeaponsComponent::GetLockProgress(int32 SlotIndex) const
{
    if (!WeaponSlots.IsValidIndex(SlotIndex)) return 0.f;
    return WeaponSlots[SlotIndex].CurrentLockProgress;
}

TArray<AActor*> UShipWeaponsComponent::GetEnemiesInRange(int32 SlotIndex) const
{
    TArray<AActor*> Out;
    if (!WeaponSlots.IsValidIndex(SlotIndex) || !GetOwner()) return Out;

    // 简化：返回 World 中所有 Pawn
    for (TActorIterator<APawn> It(GetWorld()); It; ++It)
    {
        if (*It == GetOwner()) continue;
        float Dist = FVector::Dist(GetOwner()->GetActorLocation(), It->GetActorLocation());
        if (Dist <= WeaponSlots[SlotIndex].Range) Out.Add(*It);
    }
    return Out;
}
