// ShipDamageSystem.cpp
// 飞船物理破坏系统实现

#include "Combat/ShipDamageSystem.h"
#include "Ship/ShipPawn.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"

UShipDamageSystem::UShipDamageSystem()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
}

void UShipDamageSystem::BeginPlay()
{
    Super::BeginPlay();
    BuildAdjacencyMap();
}

void UShipDamageSystem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UShipDamageSystem, PartStates);
    DOREPLIFETIME(UShipDamageSystem, bShipDestroyed);
    DOREPLIFETIME(UShipDamageSystem, ShipIntegrity);
}

void UShipDamageSystem::InitializeParts(int32 EngineCount, int32 WingCount)
{
    PartStates.Empty();

    // 根据飞船配置初始化各部件
    auto InitPart = [&](EShipPart Part, float HP)
    {
        FShipPartState State;
        State.MaxHP = HP;
        State.Reset();
        PartStates.Add(Part, State);
    };

    // 引擎（数量由飞船类决定）
    if (EngineCount >= 1) InitPart(EShipPart::Engine_Center, BasePartHP * 1.2f);
    if (EngineCount >= 2)
    {
        InitPart(EShipPart::Engine_Left, BasePartHP);
        InitPart(EShipPart::Engine_Right, BasePartHP);
    }

    // 机翼
    if (WingCount >= 2)
    {
        InitPart(EShipPart::Wing_Left, BasePartHP * 0.8f);
        InitPart(EShipPart::Wing_Right, BasePartHP * 0.8f);
    }

    // 船体
    InitPart(EShipPart::Hull_Front, BasePartHP * 1.5f);
    InitPart(EShipPart::Hull_Rear, BasePartHP * 1.5f);

    // 关键系统
    InitPart(EShipPart::Shield_Generator, BasePartHP * 0.6f);
    InitPart(EShipPart::Weapon_Port, BasePartHP * 0.5f);
    InitPart(EShipPart::Weapon_Starboard, BasePartHP * 0.5f);
    InitPart(EShipPart::Sensor_Array, BasePartHP * 0.4f);
    InitPart(EShipPart::Reactor_Core, BasePartHP * 2.f);
    InitPart(EShipPart::Cargo_Hold, BasePartHP * 0.7f);
    InitPart(EShipPart::Cockpit, BasePartHP * 1.f);

    ShipIntegrity = 1.f;
    bShipDestroyed = false;
}

void UShipDamageSystem::BuildAdjacencyMap()
{
    AdjacencyMap.Empty();

    // 定义相邻关系（爆炸波及）
    AdjacencyMap.Add(EShipPart::Engine_Left,   { EShipPart::Wing_Left, EShipPart::Hull_Rear });
    AdjacencyMap.Add(EShipPart::Engine_Right,  { EShipPart::Wing_Right, EShipPart::Hull_Rear });
    AdjacencyMap.Add(EShipPart::Engine_Center, { EShipPart::Hull_Rear, EShipPart::Reactor_Core });
    AdjacencyMap.Add(EShipPart::Wing_Left,    { EShipPart::Engine_Left, EShipPart::Hull_Front });
    AdjacencyMap.Add(EShipPart::Wing_Right,   { EShipPart::Engine_Right, EShipPart::Hull_Front });
    AdjacencyMap.Add(EShipPart::Hull_Front,   { EShipPart::Cockpit, EShipPart::Wing_Left, EShipPart::Wing_Right });
    AdjacencyMap.Add(EShipPart::Hull_Rear,    { EShipPart::Engine_Center, EShipPart::Cargo_Hold });
    AdjacencyMap.Add(EShipPart::Reactor_Core, { EShipPart::Engine_Center, EShipPart::Hull_Rear });
    AdjacencyMap.Add(EShipPart::Shield_Generator, { EShipPart::Hull_Rear });
    AdjacencyMap.Add(EShipPart::Weapon_Port,  { EShipPart::Wing_Left });
    AdjacencyMap.Add(EShipPart::Weapon_Starboard, { EShipPart::Wing_Right });
    AdjacencyMap.Add(EShipPart::Sensor_Array, { EShipPart::Hull_Front });
    AdjacencyMap.Add(EShipPart::Cargo_Hold,  { EShipPart::Hull_Rear });
    AdjacencyMap.Add(EShipPart::Cockpit,      { EShipPart::Hull_Front });
}

void UShipDamageSystem::ApplyDamageToPart(EShipPart Part, float Damage)
{
    if (bShipDestroyed) return;

    FShipPartState* State = PartStates.Find(Part);
    if (!State) return;

    bool WasDestroyed = State->bDestroyed;
    State->ApplyDamage(Damage);

    // 广播受损事件
    OnPartDamaged.Broadcast(Part, Damage);

    // 损毁事件
    if (!WasDestroyed && State->bDestroyed)
    {
        OnPartDestroyed.Broadcast(Part);

        // 反应堆熔毁
        if (Part == EShipPart::Reactor_Core)
        {
            bMeltdownStarted = true;
            ReactorMeltdownTimer = 0.f;
        }

        // 连锁伤害
        TriggerChainDamage(Part, Damage);
    }

    // 更新整体完整度
    float TotalHP = 0.f, CurrentHP = 0.f;
    for (const auto& Pair : PartStates)
    {
        TotalHP += Pair.Value.MaxHP;
        CurrentHP += Pair.Value.CurrentHP;
    }
    ShipIntegrity = (TotalHP > 0.f) ? (CurrentHP / TotalHP) : 0.f;

    CheckShipDestruction();
}

void UShipDamageSystem::ApplyDamageToShip(float Damage, EShipPart HitPart)
{
    // 护盾先吸收（如果护盾发生器完好）
    if (!IsPartDestroyed(EShipPart::Shield_Generator))
    {
        float ShieldAbsorb = Damage * 0.6f;
        Damage -= ShieldAbsorb;
    }

    ApplyDamageToPart(HitPart, Damage);
}

void UShipDamageSystem::TriggerChainDamage(EShipPart SourcePart, float OriginalDamage)
{
    if (FMath::FRand() > ChainDamageChance) return;

    const TArray<EShipPart>* Neighbors = AdjacencyMap.Find(SourcePart);
    if (!Neighbors || Neighbors->Num() == 0) return;

    // 随机选一个相邻部件
    int32 Idx = FMath::RandRange(0, Neighbors->Num() - 1);
    EShipPart Target = (*Neighbors)[Idx];

    float ChainDmg = OriginalDamage * ChainDamageMultiplier;
    ApplyDamageToPart(Target, ChainDmg);
}

void UShipDamageSystem::CheckShipDestruction()
{
    if (bShipDestroyed) return;

    // 条件1：反应堆炸了
    bool ReactorDead = IsPartDestroyed(EShipPart::Reactor_Core);

    // 条件2：所有引擎都炸了
    int32 TotalEngines = 0, DeadEngines = 0;
    for (const auto& Pair : PartStates)
    {
        if (Pair.Key == EShipPart::Engine_Left ||
            Pair.Key == EShipPart::Engine_Right ||
            Pair.Key == EShipPart::Engine_Center)
        {
            TotalEngines++;
            if (Pair.Value.bDestroyed) DeadEngines++;
        }
    }
    bool AllEnginesDead = (TotalEngines > 0) && (DeadEngines == TotalEngines);

    // 条件3：完整度低于 10%
    bool IntegrityCritical = ShipIntegrity < 0.1f;

    if (ReactorDead || AllEnginesDead || IntegrityCritical)
    {
        bShipDestroyed = true;
        OnShipDestroyed.Broadcast();

        // 触发爆炸特效（由外部监听）
        // 通知 ShipPawn 播放爆炸
        AShipPawn* Ship = Cast<AShipPawn>(GetOwner());
        if (Ship)
        {
            // 触发爆炸
        }
    }
}

void UShipDamageSystem::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UpdatePartEffects(DeltaTime);
    TickReactorMeltdown(DeltaTime);
}

void UShipDamageSystem::UpdatePartEffects(float DeltaTime)
{
    // 更新冒烟/火焰强度（随时间变化，让烟忽大忽小）
    for (auto& Pair : PartStates)
    {
        FShipPartState& State = Pair.Value;

        if (State.bDamaged && !State.bDestroyed)
        {
            // 冒烟忽大忽小
            float T = GetWorld()->GetTimeSeconds();
            State.SmokeIntensity = FMath::Clamp(
                0.3f + 0.4f * FMath::Sin(T * 3.f + (int32)Pair.Key),
                0.1f, 0.8f);
        }
        else if (State.bDestroyed)
        {
            // 着火持续
            State.FireIntensity = FMath::FRandRange(0.6f, 1.f);
        }
    }
}

void UShipDamageSystem::TickReactorMeltdown(float DeltaTime)
{
    if (!bMeltdownStarted) return;

    ReactorMeltdownTimer += DeltaTime;
    if (ReactorMeltdownTimer >= ReactorMeltdownDelay)
    {
        // 反应堆熔毁 → 大爆炸
        bMeltdownStarted = false;

        // 对所有部件造成毁灭性伤害
        for (auto& Pair : PartStates)
        {
            Pair.Value.ApplyDamage(9999.f);
        }
        ShipIntegrity = 0.f;
        bShipDestroyed = true;
        OnShipDestroyed.Broadcast();
    }
}

// —— 查询接口 ——

float UShipDamageSystem::GetThrustMultiplier() const
{
    float Mult = 1.f;

    // 中心引擎
    if (const FShipPartState* C = PartStates.Find(EShipPart::Engine_Center))
    {
        if (C->bDestroyed) Mult *= DestroyedEngineThrustFactor;
        else if (C->bDamaged) Mult *= DamagedEngineThrustFactor;
    }

    // 左右引擎
    int32 AliveEngines = 0;
    int32 TotalSideEngines = 0;
    for (const auto& Pair : PartStates)
    {
        if (Pair.Key == EShipPart::Engine_Left || Pair.Key == EShipPart::Engine_Right)
        {
            TotalSideEngines++;
            if (!Pair.Value.bDestroyed) AliveEngines++;
        }
    }

    if (TotalSideEngines > 0)
    {
        float SideRatio = (float)AliveEngines / (float)TotalSideEngines;
        Mult *= FMath::Lerp(DestroyedEngineThrustFactor, 1.f, SideRatio);
    }

    return Mult;
}

float UShipDamageSystem::GetRollMultiplier() const
{
    float Mult = 1.f;

    if (const FShipPartState* L = PartStates.Find(EShipPart::Wing_Left))
        if (L->bDestroyed) Mult *= DestroyedWingRollPenalty;
    if (const FShipPartState* R = PartStates.Find(EShipPart::Wing_Right))
        if (R->bDestroyed) Mult *= DestroyedWingRollPenalty;

    return Mult;
}

float UShipDamageSystem::GetPitchMultiplier() const
{
    // 机翼损毁影响俯仰
    float Mult = 1.f;
    if (const FShipPartState* L = PartStates.Find(EShipPart::Wing_Left))
        if (L->bDestroyed) Mult *= 0.5f;
    if (const FShipPartState* R = PartStates.Find(EShipPart::Wing_Right))
        if (R->bDestroyed) Mult *= 0.5f;
    return Mult;
}

float UShipDamageSystem::GetYawMultiplier() const
{
    // 尾部受损影响偏航
    if (const FShipPartState* H = PartStates.Find(EShipPart::Hull_Rear))
        if (H->bDestroyed) return 0.3f;
    return 1.f;
}

float UShipDamageSystem::GetShieldMultiplier() const
{
    if (bShieldGenLossDisablesShield)
    {
        if (IsPartDestroyed(EShipPart::Shield_Generator)) return 0.f;
    }
    // 护盾发生器受损 → 护盾效率降低
    if (const FShipPartState* S = PartStates.Find(EShipPart::Shield_Generator))
    {
        return S->Functionality;
    }
    return 1.f;
}

float UShipDamageSystem::GetSensorRangeMultiplier() const
{
    if (const FShipPartState* S = PartStates.Find(EShipPart::Sensor_Array))
    {
        if (S->bDestroyed && bSensorLossAffectsLock) return 0.2f;
        return S->Functionality;
    }
    return 1.f;
}

float UShipDamageSystem::GetWeaponFireRateMultiplier() const
{
    float Mult = 1.f;
    int32 AliveWeapons = 0;
    int32 TotalWeapons = 0;

    if (const FShipPartState* WP = PartStates.Find(EShipPart::Weapon_Port))
    {
        TotalWeapons++;
        if (!WP->bDestroyed) AliveWeapons++;
    }
    if (const FShipPartState* WS = PartStates.Find(EShipPart::Weapon_Starboard))
    {
        TotalWeapons++;
        if (!WS->bDestroyed) AliveWeapons++;
    }

    if (TotalWeapons > 0)
        Mult = (float)AliveWeapons / (float)TotalWeapons;

    return Mult;
}

bool UShipDamageSystem::IsPartDestroyed(EShipPart Part) const
{
    if (const FShipPartState* S = PartStates.Find(Part))
        return S->bDestroyed;
    return false;
}

FString UShipDamageSystem::GetDamageReport() const
{
    FString Report = FString::Printf(TEXT("=== Ship Integrity: %.0f%% ===\n"), ShipIntegrity * 100.f);

    for (const auto& Pair : PartStates)
    {
        FString PartName = StaticEnum<EShipPart>()->GetNameStringByValue((int64)Pair.Key);
        FString Status = Pair.Value.bDestroyed ? TEXT("DESTROYED") :
                        Pair.Value.bDamaged ? TEXT("DAMAGED") : TEXT("OK");
        float Pct = (Pair.Value.MaxHP > 0.f) ? (Pair.Value.CurrentHP / Pair.Value.MaxHP * 100.f) : 0.f;
        Report += FString::Printf(TEXT("  %s: %s (%.0f%%)\n"), *PartName, *Status, Pct);
    }

    return Report;
}

void UShipDamageSystem::RepairPart(EShipPart Part, float Amount)
{
    if (FShipPartState* S = PartStates.Find(Part))
    {
        S->CurrentHP = FMath::Min(S->MaxHP, S->CurrentHP + Amount);
        S->bDestroyed = (S->CurrentHP <= 0.f);
        S->bDamaged = (S->CurrentHP < S->MaxHP * 0.5f) && !S->bDestroyed;
        S->Functionality = S->CurrentHP / S->MaxHP;
        S->SmokeIntensity = S->bDamaged ? 0.3f : 0.f;
        S->FireIntensity = 0.f;
    }
}

void UShipDamageSystem::RepairAll(float Amount)
{
    for (auto& Pair : PartStates)
    {
        RepairPart(Pair.Key, Amount);
    }
    bShipDestroyed = false;
    ReactorMeltdownTimer = 0.f;
    bMeltdownStarted = false;
}
