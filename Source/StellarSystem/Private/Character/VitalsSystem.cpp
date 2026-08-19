// VitalsSystem.cpp
#include "Character/VitalsSystem.h"
#include "Character/InventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"

UVitalsComponent::UVitalsComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
}

void UVitalsComponent::BeginPlay()
{
    Super::BeginPlay();
    Vitals.Health = MaxHealth;
    Vitals.Oxygen = MaxOxygen;
    Vitals.Energy = MaxEnergy;
    Vitals.Stamina = MaxStamina;
    Vitals.BodyTemp = 37.f;
    bIsDead = false;
}

void UVitalsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    DOREPLIFETIME(UVitalsComponent, Vitals);
    DOREPLIFETIME(UVitalsComponent, bIsDead);
}

void UVitalsComponent::TickComponent(float Dt, ELevelTick Tick, FActorComponentTickFunction* Fn)
{
    Super::TickComponent(Dt, Tick, Fn);
    if (!HasAuthority() || bIsDead) return;

    UpdateOxygen(Dt);
    UpdateEnergy(Dt);
    UpdateStamina(Dt);
    UpdateHungerThirst(Dt);
    UpdateTemperature(Dt);
    UpdateRadiation(Dt);
    UpdateToxin(Dt);
    UpdateBleeding(Dt);
    ApplyEnvironmentalDamage(Dt);
    CheckWarnings();
    CheckDeath();

    // 通知客户端
    OnVitalsChanged.Broadcast(Vitals);
}

// ========== 氧气 ==========
void UVitalsComponent::UpdateOxygen(float Dt)
{
    if (bInSafeEnvironment || CurrentOxygenLevel > 0.5f)
    {
        // 安全环境：缓慢恢复
        Vitals.Oxygen = FMath::Min(Vitals.Oxygen + Dt * 5.f, MaxOxygen);
        return;
    }

    // 真空/低氧
    float VacResist = FMath::Max(ArmorVacuumResist, 0.f);
    float Consume = OxygenConsumeRate * Dt * (1.f - VacResist * 0.01f);

    // 奔跑消耗增加
    ACharacter* C = Cast<ACharacter>(GetOwner());
    if (C && C->GetCharacterMovement()->MaxWalkSpeed > 600.f)
        Consume *= SprintOxygenMult;

    Vitals.Oxygen = FMath::Max(Vitals.Oxygen - Consume, 0.f);

    // 缺氧伤害
    if (Vitals.Oxygen <= 0.f)
    {
        Vitals.Health -= Dt * 3.f; // 每秒 3 点窒息伤害
        OnWarning.Broadcast(TEXT("Suffocating"));
    }
}

// ========== 能量 ==========
void UVitalsComponent::UpdateEnergy(float Dt)
{
    ACharacter* C = Cast<ACharacter>(GetOwner());
    bool bSprinting = C && C->GetCharacterMovement()->MaxWalkSpeed > 600.f;

    if (bSprinting)
    {
        Vitals.Energy = FMath::Max(Vitals.Energy - EnergyConsumeRate * SprintEnergyMult * Dt, 0.f);
        if (Vitals.Energy <= 0.f) C->GetCharacterMovement()->MaxWalkSpeed = 400.f; // 力竭
    }
    else
    {
        Vitals.Energy = FMath::Min(Vitals.Energy + Dt * 4.f, MaxEnergy);
    }
}

// ========== 耐力 ==========
void UVitalsComponent::UpdateStamina(float Dt)
{
    // 耐力 = 能量的镜像，做动作消耗
    if (Vitals.Stamina < MaxStamina * 0.3f)
        Vitals.Stamina = FMath::Min(Vitals.Stamina + Dt * 8.f, MaxStamina);
}

// ========== 饥饿口渴 ==========
void UVitalsComponent::UpdateHungerThirst(float Dt)
{
    AccumulatedHunger += Dt * 0.5f;  // 200 秒从 0→100
    AccumulatedThirst += Dt * 0.7f;  // ~143 秒

    Vitals.Hunger = FMath::Min(AccumulatedHunger, 100.f);
    Vitals.Thirst = FMath::Min(AccumulatedThirst, 100.f);

    // 饥饿伤害
    if (Vitals.Hunger >= 100.f)
        Vitals.Health -= Dt * 1.5f;
    if (Vitals.Thirst >= 100.f)
        Vitals.Health -= Dt * 2.5f; // 脱水更快
}

// ========== 温度 ==========
void UVitalsComponent::UpdateTemperature(float Dt)
{
    float Target = CurrentTemperature;
    float ThermalR = ArmorThermalResist * 0.01f;

    // 护甲减缓温度变化
    float LerpSpeed = (1.f - FMath::Min(ThermalR, 0.9f)) * Dt * 0.5f;
    Vitals.BodyTemp = FMath::Lerp(Vitals.BodyTemp, Target, LerpSpeed);

    // 极端温度伤害
    float Deviation = FMath::Abs(Vitals.BodyTemp - 37.f);
    if (Deviation > 20.f)
    {
        float Dmg = (Deviation - 20.f) * Dt * 0.5f;
        Vitals.Health -= Dmg;
        OnWarning.Broadcast(Vitals.BodyTemp > 37.f ? TEXT("Overheating") : TEXT("Freezing"));
    }
}

// ========== 辐射 ==========
void UVitalsComponent::UpdateRadiation(float Dt)
{
    if (CurrentRadiation <= 0.f) return;

    float RadR = ArmorRadiationResist * 0.01f;
    float ActualDose = CurrentRadiation * Dt * (1.f - FMath::Min(RadR, 0.95f));
    Vitals.Radiation += ActualDose;

    // 辐射伤害（累积型）
    if (Vitals.Radiation > 1.f)  // 1 西弗开始掉血
    {
        Vitals.Health -= Vitals.Radiation * Dt * 0.3f;
        OnWarning.Broadcast(TEXT("RadiationPoisoning"));
    }
}

// ========== 毒素 ==========
void UVitalsComponent::UpdateToxin(float Dt)
{
    if (CurrentToxinLevel <= 0.f) return;

    float ToxR = ArmorToxinResist * 0.01f;
    float ActualTox = CurrentToxinLevel * Dt * (1.f - FMath::Min(ToxR, 0.9f));
    Vitals.Toxin += ActualTox;

    if (Vitals.Toxin > 10.f)
    {
        Vitals.Health -= Dt * 2.f;
        OnWarning.Broadcast(TEXT("ToxinBuildup"));
    }
}

// ========== 流血 ==========
void UVitalsComponent::UpdateBleeding(float Dt)
{
    if (Vitals.BleedingRate <= 0.f) return;
    Vitals.Health -= Vitals.BleedingRate * Dt;
    Vitals.BleedingRate = FMath::Max(Vitals.BleedingRate - Dt * 0.1f, 0.f); // 缓慢自愈
}

// ========== 环境伤害 ==========
void UVitalsComponent::ApplyEnvironmentalDamage(float Dt)
{
    switch (CurrentHazard)
    {
    case EHazardType::Vacuum:
        if (!bInSafeEnvironment && Vitals.SuitIntegrity > 0.f)
        {
            Vitals.SuitIntegrity = FMath::Max(Vitals.SuitIntegrity - Dt * 0.5f, 0.f);
            if (Vitals.SuitIntegrity <= 0.f)
                Vitals.Health -= Dt * 5.f; // 太空服破裂，快速冻伤/失压
        }
        break;
    case EHazardType::Corrosive:
        Vitals.Health -= Dt * 3.f;
        Vitals.SuitIntegrity = FMath::Max(Vitals.SuitIntegrity - Dt * 2.f, 0.f);
        break;
    case EHazardType::EMP:
        // 暂时禁用能量武器/护盾（由外部系统处理）
        break;
    default: break;
    }
}

// ========== 警告 ==========
void UVitalsComponent::CheckWarnings()
{
    bool bNowDanger = IsInDanger();
    if (bNowDanger && !bWasInDanger)
    {
        if (Vitals.Health < 30.f) OnWarning.Broadcast(TEXT("CriticalHealth"));
        if (Vitals.Oxygen < 20.f) OnWarning.Broadcast(TEXT("LowOxygen"));
        if (Vitals.Energy < 15.f) OnWarning.Broadcast(TEXT("LowEnergy"));
    }
    bWasInDanger = bNowDanger;
}

bool UVitalsComponent::IsInDanger() const
{
    return Vitals.Health < 30.f || Vitals.Oxygen < 20.f || Vitals.Energy < 15.f
        || Vitals.Hunger > 80.f || Vitals.Thirst > 80.f
        || Vitals.Radiation > 2.f || Vitals.Toxin > 20.f;
}

float UVitalsComponent::GetOverallCondition() const
{
    float H = Vitals.Health / MaxHealth;
    float O = Vitals.Oxygen / MaxOxygen;
    float E = Vitals.Energy / MaxEnergy;
    float S = Vitals.SuitIntegrity / 100.f;
    return (H * 0.4f + O * 0.25f + E * 0.15f + S * 0.2f);
}

// ========== 死亡 ==========
void UVitalsComponent::CheckDeath()
{
    if (Vitals.Health <= 0.f && !bIsDead)
    {
        bIsDead = true;
        Vitals.Health = 0.f;
        OnDeath.Broadcast();
        // 通知 GameMode 处理重生
    }
}

// ========== 消耗品接口 ==========
void UVitalsComponent::ServerConsumeOxygen_Implementation(float Amount)
{
    Vitals.Oxygen = FMath::Min(Vitals.Oxygen + Amount, MaxOxygen);
}

void UVitalsComponent::ServerConsumeEnergy_Implementation(float Amount)
{
    Vitals.Energy = FMath::Min(Vitals.Energy + Amount, MaxEnergy);
    Vitals.Stamina = FMath::Min(Vitals.Stamina + Amount * 0.5f, MaxStamina);
}

void UVitalsComponent::ServerHeal_Implementation(float Amount)
{
    Vitals.Health = FMath::Min(Vitals.Health + Amount, MaxHealth);
    if (Vitals.Health > 0.f) bIsDead = false;
}

void UVitalsComponent::ServerCureRadiation_Implementation(float Amount)
{
    Vitals.Radiation = FMath::Max(Vitals.Radiation - Amount, 0.f);
}

void UVitalsComponent::ServerCureToxin_Implementation(float Amount)
{
    Vitals.Toxin = FMath::Max(Vitals.Toxin - Amount, 0.f);
}

void UVitalsComponent::ServerFeed_Implementation(float FoodAmount, float WaterAmount)
{
    AccumulatedHunger = FMath::Max(AccumulatedHunger - FoodAmount, 0.f);
    AccumulatedThirst = FMath::Max(AccumulatedThirst - WaterAmount, 0.f);
    Vitals.Hunger = AccumulatedHunger;
    Vitals.Thirst = AccumulatedThirst;
}

void UVitalsComponent::ServerApplyDamage_Implementation(float Amount, bool bIgnoreArmor)
{
    float Final = Amount;
    if (!bIgnoreArmor)
    {
        // 护甲减伤（引用护甲组件的抗性）
        float DR = 0.f; // 由外部设置 Armor*Resist
        Final *= (1.f - FMath::Min(DR, 0.8f));
    }
    Vitals.Health = FMath::Max(Vitals.Health - Final, 0.f);
    // 30% 概率流血
    if (FMath::FRand() < 0.3f) Vitals.BleedingRate += Final * 0.05f;
}

void UVitalsComponent::ServerRepairSuit_Implementation(float Amount)
{
    Vitals.SuitIntegrity = FMath::Min(Vitals.SuitIntegrity + Amount, 100.f);
}

// ========== 环境设置 ==========
void UVitalsComponent::SetEnvironment(EHazardType Hazard, float OxygenLevel,
                                       float Temperature, float RadiationRate, float ToxinLevel)
{
    CurrentHazard = Hazard;
    CurrentOxygenLevel = OxygenLevel;
    CurrentTemperature = Temperature;
    CurrentRadiation = RadiationRate;
    CurrentToxinLevel = ToxinLevel;
}

void UVitalsComponent::SetSafeEnvironment(bool bSafe)
{
    bInSafeEnvironment = bSafe;
    if (bSafe)
    {
        CurrentHazard = EHazardType::None;
        CurrentOxygenLevel = 1.f;
        CurrentRadiation = 0.f;
        CurrentToxinLevel = 0.f;
    }
}
