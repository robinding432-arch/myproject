// ============================================================
// 路径: Source/StellarSystem/Private/Ship/ShipLoadout.cpp
// 作用: 飞船组件系统实现（10 槽位 × 5 稀有度 × 热平衡）
// 依赖: Ship/ShipLoadout.h, Ship/ShipPawn.h
// ============================================================

#include "Ship/ShipLoadout.h"
#include "Ship/ShipPawn.h"
#include "Math/UnrealMathUtility.h"

UShipLoadoutComponent::UShipLoadoutComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

// ======================== 安装/卸载 ========================

bool UShipLoadoutComponent::InstallComponent(const FShipComponentData& Component)
{
    // 兼容性检查
    if (!CheckCompatibility(Component)) return false;

    // 如果槽位已有组件 → 先卸载
    if (InstalledComponents.Contains(Component.Slot))
    {
        UninstallComponent(Component.Slot);
    }

    InstalledComponents.Add(Component.Slot, Component);
    RecalculateStatus();
    OnComponentInstalled.Broadcast(Component.Slot);

    UE_LOG(LogTemp, Log, TEXT("[Loadout] Installed: %s in slot %d"),
        *Component.DisplayName, (int32)Component.Slot);
    return true;
}

bool UShipLoadoutComponent::UninstallComponent(EShipComponentSlot Slot)
{
    if (!InstalledComponents.Contains(Slot)) return false;

    FShipComponentData Comp = InstalledComponents[Slot];
    InstalledComponents.Remove(Slot);
    RecalculateStatus();

    // 归还到库存
    ComponentInventory.Add(Comp);

    UE_LOG(LogTemp, Log, TEXT("[Loadout] Uninstalled from slot %d"), (int32)Slot);
    return true;
}

bool UShipLoadoutComponent::SwapComponent(EShipComponentSlot Slot, const FShipComponentData& NewComponent)
{
    if (!CheckCompatibility(NewComponent)) return false;

    // 把旧的存起来
    if (FShipComponentData* Old = InstalledComponents.Find(Slot))
    {
        ComponentInventory.Add(*Old);
    }

    InstalledComponents.Add(Slot, NewComponent);
    RecalculateStatus();
    OnComponentInstalled.Broadcast(Slot);
    return true;
}

bool UShipLoadoutComponent::UpgradeComponent(EShipComponentSlot Slot, EComponentRarity NewRarity)
{
    FShipComponentData* Comp = InstalledComponents.Find(Slot);
    if (!Comp) return false;

    // 保留原数据，提升稀有度
    int32 OldRarity = (int32)Comp->Rarity;
    int32 NewRar = (int32)NewRarity;
    float Mult = 1.f + (NewRar - OldRarity) * 0.2f;

    Comp->Rarity = NewRarity;
    Comp->Thrust *= Mult;
    Comp->ShieldStrength *= Mult;
    Comp->SensorRange *= Mult;
    Comp->CargoCapacity *= Mult;
    Comp->WarpRange *= Mult;
    Comp->PowerOutput *= Mult;
    Comp->HeatDissipation *= Mult;
    Comp->ArmorValue *= Mult;

    RecalculateStatus();
    return true;
}

// ======================== 查询 ========================

FShipComponentData UShipLoadoutComponent::GetComponent(EShipComponentSlot Slot) const
{
    if (const FShipComponentData* Ptr = InstalledComponents.Find(Slot))
        return *Ptr;
    return FShipComponentData();
}

bool UShipLoadoutComponent::HasComponent(EShipComponentSlot Slot) const
{
    return InstalledComponents.Contains(Slot);
}

float UShipLoadoutComponent::GetEffectiveTopSpeed() const
{
    float BaseSpeed = 5000.f;
    float ThrustBonus = CurrentStatus.TotalThrust * 10.f;
    float MassPenalty = CurrentStatus.TotalMass * 2.f;
    return FMath::Max(1000.f, BaseSpeed + ThrustBonus - MassPenalty);
}

float UShipLoadoutComponent::GetEffectiveWarpRange() const
{
    float Base = 5000000.f;
    for (auto& Pair : InstalledComponents)
    {
        Base = FMath::Max(Base, Pair.Value.WarpRange);
    }
    return Base;
}

float UShipLoadoutComponent::GetHeatPercentage() const
{
    if (CurrentStatus.TotalHeatDissipation <= 0.f) return 1.f;
    return FMath::Clamp(CurrentStatus.TotalHeatGen / CurrentStatus.TotalHeatDissipation, 0.f, 2.f);
}

float UShipLoadoutComponent::GetPowerPercentage() const
{
    if (CurrentStatus.TotalPowerOutput <= 0.f) return 0.f;
    return FMath::Clamp(CurrentStatus.TotalPowerConsumed / CurrentStatus.TotalPowerOutput, 0.f, 2.f);
}

// ======================== AI 生成 ========================

FShipComponentData UShipLoadoutComponent::GenerateRandomComponent(
    EShipComponentSlot Slot, EComponentRarity Rarity, int32 Seed)
{
    FRandomStream Rand(Seed);
    FShipComponentData Comp;
    Comp.Slot = Slot;
    Comp.Rarity = Rarity;

    float RarityMult = GetRarityMultiplier(Rarity);
    Comp.ComponentID = FName(*FString::Printf(TEXT("Comp_%d_%d"), (int32)Slot, Seed));
    Comp.DisplayName = GetRarityColor(Rarity) + TEXT(" ") + UEnum::GetValueAsString(Slot);

    switch (Slot)
    {
        case EShipComponentSlot::Engine:
            Comp.Thrust = Rand.RandRange(80.f, 150.f) * RarityMult;
            Comp.HeatGeneration = Rand.RandRange(5.f, 15.f) * RarityMult;
            Comp.Mass = Rand.RandRange(50.f, 150.f);
            Comp.PowerConsumption = Rand.RandRange(5.f, 15.f);
            break;
        case EShipComponentSlot::Weapon:
            // 武器组件由 ShipWeapons 管理，这里只存基础值
            Comp.Thrust = 0.f;
            Comp.Mass = Rand.RandRange(20.f, 80.f);
            break;
        case EShipComponentSlot::Shield:
            Comp.ShieldStrength = Rand.RandRange(50.f, 150.f) * RarityMult;
            Comp.ShieldRegen = Rand.RandRange(3.f, 12.f) * RarityMult;
            Comp.PowerConsumption = Rand.RandRange(8.f, 20.f);
            Comp.HeatGeneration = Rand.RandRange(2.f, 8.f);
            Comp.Mass = Rand.RandRange(30.f, 100.f);
            break;
        case EShipComponentSlot::Sensor:
            Comp.SensorRange = Rand.RandRange(30000.f, 100000.f) * RarityMult;
            Comp.PowerConsumption = Rand.RandRange(2.f, 8.f);
            Comp.Mass = Rand.RandRange(10.f, 40.f);
            break;
        case EShipComponentSlot::Cargo:
            Comp.CargoCapacity = Rand.RandRange(50.f, 300.f) * RarityMult;
            Comp.Mass = Rand.RandRange(20.f, 80.f) * RarityMult;
            break;
        case EShipComponentSlot::WarpCore:
            Comp.WarpRange = Rand.RandRange(5000000.f, 20000000.f) * RarityMult;
            Comp.PowerOutput = Rand.RandRange(50.f, 200.f) * RarityMult;
            Comp.HeatGeneration = Rand.RandRange(10.f, 30.f) * RarityMult;
            Comp.Mass = Rand.RandRange(50.f, 200.f);
            Comp.bCanOverload = (Rarity >= EComponentRarity::Advanced);
            break;
        case EShipComponentSlot::Reactor:
            Comp.PowerOutput = Rand.RandRange(80.f, 300.f) * RarityMult;
            Comp.HeatGeneration = Rand.RandRange(5.f, 20.f);
            Comp.Mass = Rand.RandRange(40.f, 150.f);
            Comp.bCanOverload = true;
            break;
        case EShipComponentSlot::Cooler:
            Comp.HeatDissipation = Rand.RandRange(50.f, 200.f) * RarityMult;
            Comp.PowerConsumption = Rand.RandRange(5.f, 15.f);
            Comp.Mass = Rand.RandRange(20.f, 80.f);
            break;
        case EShipComponentSlot::Armor:
            Comp.ArmorValue = Rand.RandRange(50.f, 200.f) * RarityMult;
            Comp.Mass = Rand.RandRange(50.f, 200.f) * RarityMult;
            break;
        case EShipComponentSlot::Utility:
            // 随机特殊效果
            Comp.PowerOutput = Rand.RandRange(-10.f, 30.f);
            Comp.HeatDissipation = Rand.RandRange(-10.f, 50.f);
            Comp.Mass = Rand.RandRange(5.f, 30.f);
            break;
    }

    // 过载参数
    if (Comp.bCanOverload)
    {
        Comp.OverloadMultiplier = 1.2f + Rand.GetFraction() * 0.5f;
        Comp.OverloadHeatCost = Rand.RandRange(10.f, 30.f);
    }

    return Comp;
}

void UShipLoadoutComponent::GenerateFullLoadout(int32 Seed, EComponentRarity MinRarity, EComponentRarity MaxRarity)
{
    FRandomStream Rand(Seed);

    for (int32 s = 0; s < 10; ++s)
    {
        EShipComponentSlot Slot = (EShipComponentSlot)s;
        int32 RarIdx = Rand.RandRange((int32)MinRarity, (int32)MaxRarity);
        EComponentRarity Rarity = (EComponentRarity)RarIdx;

        FShipComponentData Comp = GenerateRandomComponent(Slot, Rarity, Seed + s * 7);
        InstallComponent(Comp);
    }

    UE_LOG(LogTemp, Log, TEXT("[Loadout] Full loadout generated (Seed=%d)"), Seed);
}

FString UShipLoadoutComponent::GetComponentDescription(const FShipComponentData& Comp) const
{
    FString Desc = GetRarityColor(Comp.Rarity) + TEXT(" ") + UEnum::GetValueAsString(Comp.Slot) + TEXT("\n");
    Desc += FString::Printf(TEXT("Mass: %.0f kg\n"), Comp.Mass);

    if (Comp.Thrust > 0)        Desc += FString::Printf(TEXT("Thrust: %.0f\n"), Comp.Thrust);
    if (Comp.ShieldStrength > 0) Desc += FString::Printf(TEXT("Shield: %.0f (+%.1f/s)\n"), Comp.ShieldStrength, Comp.ShieldRegen);
    if (Comp.SensorRange > 0)   Desc += FString::Printf(TEXT("Sensor: %.0f m\n"), Comp.SensorRange * 0.01f);
    if (Comp.CargoCapacity > 0)  Desc += FString::Printf(TEXT("Cargo: %.0f units\n"), Comp.CargoCapacity);
    if (Comp.WarpRange > 0)      Desc += FString::Printf(TEXT("Warp: %.0f km\n"), Comp.WarpRange * 0.00001f);
    if (Comp.PowerOutput > 0)    Desc += FString::Printf(TEXT("Power Out: %.0f\n"), Comp.PowerOutput);
    if (Comp.PowerConsumption > 0) Desc += FString::Printf(TEXT("Power In: %.0f\n"), Comp.PowerConsumption);
    if (Comp.HeatDissipation > 0) Desc += FString::Printf(TEXT("Cooling: %.0f\n"), Comp.HeatDissipation);
    if (Comp.HeatGeneration > 0) Desc += FString::Printf(TEXT("Heat Gen: %.0f\n"), Comp.HeatGeneration);
    if (Comp.ArmorValue > 0)     Desc += FString::Printf(TEXT("Armor: %.0f\n"), Comp.ArmorValue);

    if (Comp.bCanOverload) Desc += FString::Printf(TEXT("⚠ Overload: x%.2f\n"), Comp.OverloadMultiplier);

    return Desc;
}

// ======================== Tick ========================

void UShipLoadoutComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // 过载检测
    bool bNewOverload = (GetHeatPercentage() > 1.0f) || (GetPowerPercentage() > 1.0f);

    if (bNewOverload)
    {
        OverloadTimer += DeltaTime;
        if (OverloadTimer >= MaxSafeOverloadTime && !CurrentStatus.bOverloaded)
        {
            CurrentStatus.bOverloaded = true;
            OnOverloadChanged.Broadcast(true);
            UE_LOG(LogTemp, Warning, TEXT("[Loadout] CRITICAL OVERLOAD!")));
        }
    }
    else
    {
        OverloadTimer = FMath::Max(0.f, OverloadTimer - DeltaTime * 0.5f);
        if (OverloadTimer <= 0.f && CurrentStatus.bOverloaded)
        {
            CurrentStatus.bOverloaded = false;
            OnOverloadChanged.Broadcast(false);
        }
    }
}

// ======================== 私有 ========================

void UShipLoadoutComponent::RecalculateStatus()
{
    FShipLoadoutStatus NewStatus;
    NewStatus.TotalMass = 0.f;

    for (auto& Pair : InstalledComponents)
    {
        const FShipComponentData& C = Pair.Value;
        NewStatus.TotalThrust += C.Thrust;
        NewStatus.TotalShield += C.ShieldStrength;
        NewStatus.TotalPowerOutput += C.PowerOutput;
        NewStatus.TotalPowerConsumed += C.PowerConsumption;
        NewStatus.TotalHeatGen += C.HeatGeneration;
        NewStatus.TotalHeatDissipation += C.HeatDissipation;
        NewStatus.TotalMass += C.Mass;
    }

    NewStatus.PowerBalance = NewStatus.TotalPowerOutput - NewStatus.TotalPowerConsumed;
    NewStatus.HeatBalance = NewStatus.TotalHeatGen - NewStatus.TotalHeatDissipation;

    CurrentStatus = NewStatus;

    // 同步到 ShipPawn
    if (AShipPawn* Ship = Cast<AShipPawn>(GetOwner()))
    {
        Ship->MaxSpeed = GetEffectiveTopSpeed();
        Ship->MaxWarpRange = GetEffectiveWarpRange();
        Ship->ShieldMax = CurrentStatus.TotalShield;
    }
}

float UShipLoadoutComponent::GetRarityMultiplier(EComponentRarity Rarity) const
{
    switch (Rarity)
    {
        case EComponentRarity::Standard:  return 1.0f;
        case EComponentRarity::Improved:  return 1.2f;
        case EComponentRarity::Advanced:  return 1.5f;
        case EComponentRarity::Prototype: return 2.0f;
        case EComponentRarity::Alien:     return 2.5f;
    }
    return 1.0f;
}

FString UShipLoadoutComponent::GetRarityColor(EComponentRarity Rarity) const
{
    switch (Rarity)
    {
        case EComponentRarity::Standard:  return TEXT("<White>");
        case EComponentRarity::Improved:  return TEXT("<Green>");
        case EComponentRarity::Advanced:  return TEXT("<Blue>");
        case EComponentRarity::Prototype: return TEXT("<Purple>");
        case EComponentRarity::Alien:     return TEXT("<Orange>");
    }
    return TEXT("");
}

bool UShipLoadoutComponent::CheckCompatibility(const FShipComponentData& Component) const
{
    // 检查兼容性标签
    for (auto& Pair : InstalledComponents)
    {
        const FShipComponentData& Existing = Pair.Value;
        // 简化：同类标签不兼容
        if (Existing.CompatibilityTags.HasAny(Component.CompatibilityTags))
        {
            return false;
        }
    }
    return true;
}
