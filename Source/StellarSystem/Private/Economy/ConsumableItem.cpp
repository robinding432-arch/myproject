// ============================================================
// 路径: Source/StellarSystem/Private/Economy/ConsumableItem.cpp
// 作用: 消耗品系统实现（20+ 种 / Buff / 快捷栏）
// 依赖: Economy/ConsumableItem.h, Character/VitalsComponent.h
// ============================================================

#include "Economy/ConsumableItem.h"
#include "Character/VitalsComponent.h"
#include "Character/InventoryComponent.h"
#include "Math/UnrealMathUtility.h"

// ======================== 构造 ========================

UConsumableInventoryComponent::UConsumableInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    Hotbar.SetNum(10);
}

// ======================== Tick ========================

void UConsumableInventoryComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    TickActiveBuffs(DeltaTime);
}

// ======================== 使用 ========================

bool UConsumableInventoryComponent::UseConsumable(FName ItemID)
{
    if (!HasConsumable(ItemID)) return false;

    const FConsumableData* Data = ConsumableLibrary.Find(ItemID);
    if (!Data) return false;

    // 检查使用条件
    if (Data->bRequiresInSpace)
    {
        // 简化：总是允许
    }
    if (Data->bRequiresOnPlanet)
    {
        // 简化：总是允许
    }

    // 应用效果
    ApplyInstantEffects(*Data);
    ApplyBuffEffects(*Data);

    // 从库存扣减
    if (LinkedInventory)
    {
        LinkedInventory->RemoveItem(ItemID, 1);
    }

    OnConsumableUsed.Broadcast(ItemID);
    UE_LOG(LogTemp, Log, TEXT("[Consumable] Used: %s"), *Data->DisplayName);
    return true;
}

bool UConsumableInventoryComponent::UseFromHotbar(int32 SlotIndex)
{
    if (!Hotbar.IsValidIndex(SlotIndex)) return false;

    FName ItemID = Hotbar[SlotIndex];
    if (ItemID == NAME_None) return false;

    ActiveSlot = SlotIndex;
    return UseConsumable(ItemID);
}

void UConsumableInventoryComponent::CycleHotbar(int32 Direction)
{
    if (Hotbar.Num() == 0) return;
    ActiveSlot = (ActiveSlot + Direction + Hotbar.Num()) % Hotbar.Num();
}

// ======================== 注册 ========================

void UConsumableInventoryComponent::RegisterConsumable(const FConsumableData& Data)
{
    ConsumableLibrary.Add(Data.ItemID, Data);
}

void UConsumableInventoryComponent::RegisterDefaults()
{
    FRandomStream Rand(42);

    // ========== 医疗类 ==========
    auto MakeMedical = [&](FName ID, FString Name, float Heal, int32 QL)
    {
        FConsumableData D;
        D.ItemID = ID;
        D.DisplayName = Name;
        D.Category = EConsumableCategory::Medical;
        D.QualityLevel = QL;
        D.InstantHeal = Heal * D.GetQualityMultiplier();
        D.BasePrice = 50.f * QL;
        D.Description = FString::Printf(TEXT("Restores %.0f HP"), D.InstantHeal);
        D.RarityTier = (QL >= 4) ? TEXT("Epic") : (QL >= 3) ? TEXT("Rare") : TEXT("Common");
        RegisterConsumable(D);
    };

    MakeMedical(FName("Med_Bandage"),     TEXT("Basic Bandage"),     25.f, 1);
    MakeMedical(FName("Med_Medkit"),      TEXT("Medkit"),            60.f, 2);
    MakeMedical(FName("Med_AdvMedkit"),   TEXT("Advanced Medkit"),   120.f, 3);
    MakeMedical(FName("Med_Stimpak"),    TEXT("Stimpak"),           40.f, 2);
    MakeMedical(FName("Med_CombatStim"), TEXT("Combat Stim"),        80.f, 4);
    MakeMedical(FName("Med_NanoMed"),    TEXT("Nano Medkit"),       200.f, 5);

    // ========== 食物类 ==========
    auto MakeFood = [&](FName ID, FString Name, float FoodVal, int32 QL)
    {
        FConsumableData D;
        D.ItemID = ID;
        D.DisplayName = Name;
        D.Category = EConsumableCategory::Food;
        D.QualityLevel = QL;
        D.InstantFood = FoodVal * D.GetQualityMultiplier();
        D.BasePrice = 15.f * QL;
        D.Description = FString::Printf(TEXT("Satisfies hunger (%.0f)"), D.InstantFood);
        RegisterConsumable(D);
    };

    MakeFood(FName("Food_Ration"),    TEXT("Nutri-Ration"),   30.f, 1);
    MakeFood(FName("Food_MealPack"), TEXT("Meal Pack"),      50.f, 2);
    MakeFood(FName("Food_Gourmet"),  TEXT("Gourmet Pack"),   80.f, 3);
    MakeFood(FName("Food_Synth"),    TEXT("Synth-Steak"),    40.f, 2);

    // ========== 饮料类 ==========
    auto MakeDrink = [&](FName ID, FString Name, float WaterVal, int32 QL)
    {
        FConsumableData D;
        D.ItemID = ID;
        D.DisplayName = Name;
        D.Category = EConsumableCategory::Drink;
        D.QualityLevel = QL;
        D.InstantWater = WaterVal * D.GetQualityMultiplier();
        D.BasePrice = 12.f * QL;
        D.Description = FString::Printf(TEXT("Hydrates (%.0f)"), D.InstantWater);
        RegisterConsumable(D);
    };

    MakeDrink(FName("Drink_Water"),    TEXT("Purified Water"), 40.f, 1);
    MakeDrink(FName("Drink_Juice"),   TEXT("Synth-Juice"),    35.f, 2);
    MakeDrink(FName("Drink_Energy"),  TEXT("Energy Drink"),   25.f, 2);
    MakeDrink(FName("Drink_Ambrosia"),TEXT("Ambrosia"),      60.f, 4);

    // ========== 氧气类 ==========
    auto MakeO2 = [&](FName ID, FString Name, float O2, int32 QL)
    {
        FConsumableData D;
        D.ItemID = ID;
        D.DisplayName = Name;
        D.Category = EConsumableCategory::Oxygen;
        D.QualityLevel = QL;
        D.InstantOxygen = O2 * D.GetQualityMultiplier();
        D.BasePrice = 30.f * QL;
        D.bRequiresInSpace = true;
        D.Description = FString::Printf(TEXT("Refills O2 (%.0f)"), D.InstantOxygen);
        RegisterConsumable(D);
    };

    MakeO2(FName("O2_SmallTank"),  TEXT("Small O2 Tank"),  30.f, 1);
    MakeO2(FName("O2_MedTank"),   TEXT("Medium O2 Tank"), 60.f, 2);
    MakeO2(FName("O2_LargeTank"), TEXT("Large O2 Tank"), 120.f, 3);
    MakeO2(FName("O2_Regen"),     TEXT("Regen O2 Canister"), 50.f, 4);

    // ========== 能量类 ==========
    auto MakeEnergy = [&](FName ID, FString Name, float Energy, int32 QL)
    {
        FConsumableData D;
        D.ItemID = ID;
        D.DisplayName = Name;
        D.Category = EConsumableCategory::Energy;
        D.QualityLevel = QL;
        D.InstantEnergy = Energy * D.GetQualityMultiplier();
        D.BasePrice = 25.f * QL;
        D.Description = FString::Printf(TEXT("Restores %.0f Energy"), D.InstantEnergy);
        RegisterConsumable(D);
    };

    MakeEnergy(FName("Pwr_Cell"),     TEXT("Power Cell"),     40.f, 1);
    MakeEnergy(FName("Pwr_Battery"),  TEXT("Battery Pack"),   80.f, 2);
    MakeEnergy(FName("Pwr_Fusion"),   TEXT("Fusion Cell"),   150.f, 4);
    MakeEnergy(FName("Pwr_Plasma"),  TEXT("Plasma Battery"),120.f, 3);

    // ========== 工具类 ==========
    FConsumableData RepairKit;
    RepairKit.ItemID = FName("Tool_Repair");
    RepairKit.DisplayName = TEXT("Multi-Tool Repair Kit");
    RepairKit.Category = EConsumableCategory::Tool;
    RepairKit.QualityLevel = 2;
    RepairKit.InstantHeal = 30.f; // 修船体
    RepairKit.BasePrice = 80.f;
    RepairKit.Description = TEXT("Repairs ship hull (30 HP)");
    RegisterConsumable(RepairKit);

    FConsumableData Welder;
    Welder.ItemID = FName("Tool_Welder");
    Welder.DisplayName = TEXT("Plasma Welder");
    Welder.Category = EConsumableCategory::Tool;
    Welder.QualityLevel = 3;
    Welder.InstantHeal = 60.f;
    Welder.BasePrice = 150.f;
    Welder.Description = TEXT("Heavy repair tool (60 HP)");
    RegisterConsumable(Welder);

    // ========== 特殊类 ==========
    FConsumableData RadAway;
    RadAway.ItemID = FName("Spec_RadAway");
    RadAway.DisplayName = TEXT("RadAway");
    RadAway.Category = EConsumableCategory::Special;
    RadAway.QualityLevel = 3;
    RadAway.InstantRadiationCleanse = 50.f;
    RadAway.BasePrice = 120.f;
    RadAway.Description = TEXT("Removes 50 radiation");
    RegisterConsumable(RadAway);

    FConsumableData ToxinFilter;
    ToxinFilter.ItemID = FName("Spec_ToxinFilter");
    ToxinFilter.DisplayName = TEXT("Toxin Filter");
    ToxinFilter.Category = EConsumableCategory::Special;
    ToxinFilter.QualityLevel = 2;
    ToxinFilter.InstantToxinCleanse = 40.f;
    ToxinFilter.BasePrice = 90.f;
    ToxinFilter.Description = TEXT("Cleanses 40 toxin");
    RegisterConsumable(ToxinFilter);

    FConsumableData StopBleed;
    StopBleed.ItemID = FName("Spec_StopBleed");
    StopBleed.DisplayName = TEXT("Coagulant Patch");
    StopBleed.Category = EConsumableCategory::Special;
    StopBleed.QualityLevel = 2;
    StopBleed.InstantStopBleeding = 1.f; // >0 = 止血
    StopBleed.BasePrice = 60.f;
    StopBleed.Description = TEXT("Stops bleeding instantly");
    RegisterConsumable(StopBleed);

    // ========== 信号类 ==========
    FConsumableData Flare;
    Flare.ItemID = FName("Sig_Flare");
    Flare.DisplayName = TEXT("Distress Flare");
    Flare.Category = EConsumableCategory::Signal;
    Flare.QualityLevel = 1;
    Flare.BasePrice = 20.f;
    Flare.Description = TEXT("Attracts nearby ships");
    RegisterConsumable(Flare);

    FConsumableData Beacon;
    Beacon.ItemID = FName("Sig_Beacon");
    Beacon.DisplayName = TEXT("Navigation Beacon");
    Beacon.Category = EConsumableCategory::Signal;
    Beacon.QualityLevel = 2;
    Beacon.BasePrice = 50.f;
    Beacon.Description = TEXT("Marks location on starmap");
    RegisterConsumable(Beacon);

    // ========== Buff 物品 ==========
    FConsumableData SpeedPill;
    SpeedPill.ItemID = FName("Buff_Speed");
    SpeedPill.DisplayName = TEXT("Speed Enhancement");
    SpeedPill.Category = EConsumableCategory::Medical;
    SpeedPill.QualityLevel = 3;
    SpeedPill.BasePrice = 100.f;
    FBuffInstance SpeedBuff;
    SpeedBuff.BuffType = EBuffType::Speed;
    SpeedBuff.Magnitude = 1.3f; // +30% 速度
    SpeedBuff.Duration = 45.f;
    SpeedBuff.RemainingTime = 45.f;
    SpeedPill.AppliedBuffs.Add(SpeedBuff);
    SpeedPill.Description = TEXT("+30% movement speed for 45s");
    RegisterConsumable(SpeedPill);

    FConsumableData DefShield;
    DefShield.ItemID = FName("Buff_Defense");
    DefShield.DisplayName = TEXT("Defense Matrix");
    DefShield.Category = EConsumableCategory::Medical;
    DefShield.QualityLevel = 3;
    DefShield.BasePrice = 120.f;
    FBuffInstance DefBuff;
    DefBuff.BuffType = EBuffType::Defense;
    DefBuff.Magnitude = 1.25f;
    DefBuff.Duration = 30.f;
    DefBuff.RemainingTime = 30.f;
    DefShield.AppliedBuffs.Add(DefBuff);
    DefShield.Description = TEXT("+25% defense for 30s");
    RegisterConsumable(DefShield);

    FConsumableData StealthCloak;
    StealthCloak.ItemID = FName("Buff_Stealth");
    StealthCloak.DisplayName = TEXT("Stealth Cloak");
    StealthCloak.Category = EConsumableCategory::Special;
    StealthCloak.QualityLevel = 4;
    StealthCloak.BasePrice = 300.f;
    FBuffInstance StealthBuff;
    StealthBuff.BuffType = EBuffType::Stealth;
    StealthBuff.Magnitude = 0.3f; // 70% 隐身
    StealthBuff.Duration = 20.f;
    StealthBuff.RemainingTime = 20.f;
    StealthCloak.AppliedBuffs.Add(StealthBuff);
    StealthCloak.Description = TEXT("Stealth mode for 20s");
    RegisterConsumable(StealthCloak);

    FConsumableData RegenField;
    RegenField.ItemID = FName("Buff_Regen");
    RegenField.DisplayName = TEXT("Regen Field");
    RegenField.Category = EConsumableCategory::Medical;
    RegenField.QualityLevel = 4;
    RegenField.BasePrice = 200.f;
    FBuffInstance RegenBuff;
    RegenBuff.BuffType = EBuffType::Regen;
    RegenBuff.Magnitude = 5.f; // 5 HP/s
    RegenBuff.Duration = 30.f;
    RegenBuff.RemainingTime = 30.f;
    RegenField.AppliedBuffs.Add(RegenBuff);
    RegenField.Description = TEXT("+5 HP/s regen for 30s");
    RegisterConsumable(RegenField);

    UE_LOG(LogTemp, Log, TEXT("[Consumable] Registered %d default items"),
        ConsumableLibrary.Num());
}

// ======================== 查询 ========================

FConsumableData UConsumableInventoryComponent::GetConsumableData(FName ItemID) const
{
    if (const FConsumableData* Ptr = ConsumableLibrary.Find(ItemID))
        return *Ptr;
    return FConsumableData();
}

bool UConsumableInventoryComponent::HasConsumable(FName ItemID, int32 MinQuantity) const
{
    if (!LinkedInventory) return false;
    return LinkedInventory->HasItem(ItemID, MinQuantity);
}

FString UConsumableInventoryComponent::GetBuffStatusText() const
{
    FString Text;
    for (const FBuffInstance& Buff : ActiveBuffs)
    {
        Text += FString::Printf(TEXT("%s: %.0fs | "),
            *UEnum::GetValueAsString(Buff.BuffType),
            Buff.RemainingTime);
    }
    return Text;
}

// ======================== Tick Buff ========================

void UConsumableInventoryComponent::TickActiveBuffs(float DeltaTime)
{
    UVitalsComponent* Vitals = GetVitals();
    if (!Vitals) return;

    for (int32 i = ActiveBuffs.Num() - 1; i >= 0; --i)
    {
        FBuffInstance& Buff = ActiveBuffs[i];
        Buff.RemainingTime -= DeltaTime;

        // Tick 持续效果
        Buff.Tick(DeltaTime, Vitals);

        if (Buff.RemainingTime <= 0.f)
        {
            OnBuffExpired.Broadcast(Buff.BuffType);
            ActiveBuffs.RemoveAt(i);
        }
    }
}

// ======================== 私有 ========================

void UConsumableInventoryComponent::ApplyInstantEffects(const FConsumableData& Data)
{
    UVitalsComponent* Vitals = GetVitals();
    if (!Vitals) return;

    if (Data.InstantHeal > 0.f)            Vitals->HealHealth(Data.InstantHeal);
    if (Data.InstantOxygen > 0.f)        Vitals->RestoreOxygen(Data.InstantOxygen);
    if (Data.InstantEnergy > 0.f)        Vitals->RestoreEnergy(Data.InstantEnergy);
    if (Data.InstantFood > 0.f)          Vitals->Feed(-Data.InstantFood); // Feed 是减饥饿
    if (Data.InstantWater > 0.f)        Vitals->Hydrate(-Data.InstantWater);
    if (Data.InstantRadiationCleanse > 0.f) Vitals->ApplyRadiation(-Data.InstantRadiationCleanse);
    if (Data.InstantToxinCleanse > 0.f)  Vitals->ApplyToxin(-Data.InstantToxinCleanse);
    if (Data.InstantStopBleeding > 0.f)  Vitals->StopBleeding();
}

void UConsumableInventoryComponent::ApplyBuffEffects(const FConsumableData& Data)
{
    for (const FBuffInstance& Buff : Data.AppliedBuffs)
    {
        ActiveBuffs.Add(Buff);
        OnBuffApplied.Broadcast(Buff);
    }
}

UVitalsComponent* UConsumableInventoryComponent::GetVitals() const
{
    AActor* Owner = GetOwner();
    if (!Owner) return nullptr;

    // 从角色获取维生组件
    // 简化：遍历组件查找
    TArray<UVitalsComponent*> Vitals;
    Owner->GetComponents<UVitalsComponent>(Vitals);
    if (Vitals.Num() > 0) return Vitals[0];

    return nullptr;
}

int32 UConsumableInventoryComponent::GetHotbarQuantity(int32 SlotIndex) const
{
    if (!Hotbar.IsValidIndex(SlotIndex)) return 0;
    if (!LinkedInventory) return 0;
    return LinkedInventory->GetItemCount(Hotbar[SlotIndex]);
}

// ======================== FBuffInstance 实现 ========================

void FBuffInstance::Tick(float DeltaTime, UVitalsComponent* Vitals)
{
    if (!Vitals) return;

    switch (BuffType)
    {
        case EBuffType::Regen:
            Vitals->HealHealth(Magnitude * DeltaTime);
            break;
        case EBuffType::O2Regen:
            Vitals->RestoreOxygen(Magnitude * DeltaTime);
            break;
        case EBuffType::EnergyRegen:
            Vitals->RestoreEnergy(Magnitude * DeltaTime);
            break;
        // Speed/Defense/Stealth 由其他系统查询 Magnitude
        default: break;
    }
}
