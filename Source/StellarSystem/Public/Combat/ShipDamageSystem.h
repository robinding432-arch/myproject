// ShipDamageSystem.h
// 飞船物理破坏系统：部件可损毁、引擎冒烟、迫降

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShipDamageSystem.generated.h"

class AShipPawn;

// 部件类型
UENUM(BlueprintType)
enum class EShipPart : uint8
{
    Engine_Left,
    Engine_Right,
    Engine_Center,
    Wing_Left,
    Wing_Right,
    Hull_Front,
    Hull_Rear,
    Shield_Generator,
    Weapon_Port,
    Weapon_Starboard,
    Sensor_Array,
    Reactor_Core,
    Cargo_Hold,
    Cockpit,
    COUNT
};

// 单个部件状态
USTRUCT(BlueprintType)
struct FShipPartState
{
    GENERATED_BODY()

    // 当前 HP（0=损毁）
    UPROPERTY(BlueprintReadOnly)
    float CurrentHP = 100.f;

    // 最大 HP
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxHP = 100.f;

    // 是否损毁
    UPROPERTY(BlueprintReadOnly)
    bool bDestroyed = false;

    // 是否受损（HP < 50%）
    UPROPERTY(BlueprintReadOnly)
    bool bDamaged = false;

    // 此部件提供的功能倍率（损毁时=0）
    UPROPERTY(BlueprintReadOnly)
    float Functionality = 1.f;

    // 冒烟特效强度（0~1）
    UPROPERTY(BlueprintReadOnly)
    float SmokeIntensity = 0.f;

    // 火焰特效强度（0~1）
    UPROPERTY(BlueprintReadOnly)
    float FireIntensity = 0.f;

    // 重置
    void Reset()
    {
        CurrentHP = MaxHP;
        bDestroyed = false;
        bDamaged = false;
        Functionality = 1.f;
        SmokeIntensity = 0.f;
        FireIntensity = 0.f;
    }

    // 应用伤害
    void ApplyDamage(float Dmg)
    {
        CurrentHP = FMath::Max(0.f, CurrentHP - Dmg);
        bDestroyed = (CurrentHP <= 0.f);
        bDamaged = (CurrentHP < MaxHP * 0.5f) && !bDestroyed;

        Functionality = CurrentHP / MaxHP;
        Functionality = FMath::Clamp(Functionality, 0.f, 1.f);

        SmokeIntensity = bDamaged ? FMath::Clamp(1.f - Functionality * 2.f, 0.f, 1.f) : 0.f;
        FireIntensity = bDestroyed ? FMath::FRandRange(0.5f, 1.f) : 0.f;
    }
};

// 飞船损毁事件委托
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPartDestroyed, EShipPart, Part);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnShipDestroyed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPartDamaged, EShipPart, Part, float, Dmg);

UCLASS(ClassGroup = "Combat", meta = (BlueprintSpawnableComponent))
class UShipDamageSystem : public UActorComponent
{
    GENERATED_BODY()

public:
    UShipDamageSystem();

    // —— 配置 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
    float BasePartHP = 100.f;

    // 部件间连锁伤害概率（爆炸波及相邻部件）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
    float ChainDamageChance = 0.15f;

    // 连锁伤害倍率
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
    float ChainDamageMultiplier = 0.4f;

    // 引擎损毁后推力衰减
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    float DestroyedEngineThrustFactor = 0.0f;

    // 单引擎损毁推力倍率
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    float DamagedEngineThrustFactor = 0.5f;

    // 机翼损毁滚转惩罚
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    float DestroyedWingRollPenalty = 0.3f;

    // 传感器损毁锁定范围减半
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    bool bSensorLossAffectsLock = true;

    // 护盾发生器损毁护盾归零
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    bool bShieldGenLossDisablesShield = true;

    // 反应堆损毁自动爆炸倒计时
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    float ReactorMeltdownDelay = 5.f;

    // —— 运行时状态 ——
    UPROPERTY(BlueprintReadOnly, Category = "State")
    TMap<EShipPart, FShipPartState> PartStates;

    UPROPERTY(BlueprintReadOnly, Category = "State")
    bool bShipDestroyed = false;

    UPROPERTY(BlueprintReadOnly, Category = "State")
    float ShipIntegrity = 1.f; // 0~1 整体完整度

    // —— 事件 ——
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnPartDestroyed OnPartDestroyed;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnShipDestroyed OnShipDestroyed;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnPartDamaged OnPartDamaged;

    // —— 接口 ——
    UFUNCTION(BlueprintCallable, Category = "Damage")
    void InitializeParts(int32 EngineCount, int32 WingCount);

    UFUNCTION(BlueprintCallable, Category = "Damage")
    void ApplyDamageToPart(EShipPart Part, float Damage);

    UFUNCTION(BlueprintCallable, Category = "Damage")
    void ApplyDamageToShip(float Damage, EShipPart HitPart);

    UFUNCTION(BlueprintCallable, Category = "Damage")
    float GetThrustMultiplier() const;

    UFUNCTION(BlueprintCallable, Category = "Damage")
    float GetRollMultiplier() const;

    UFUNCTION(BlueprintCallable, Category = "Damage")
    float GetPitchMultiplier() const;

    UFUNCTION(BlueprintCallable, Category = "Damage")
    float GetYawMultiplier() const;

    UFUNCTION(BlueprintCallable, Category = "Damage")
    float GetShieldMultiplier() const;

    UFUNCTION(BlueprintCallable, Category = "Damage")
    float GetSensorRangeMultiplier() const;

    UFUNCTION(BlueprintCallable, Category = "Damage")
    float GetWeaponFireRateMultiplier() const;

    UFUNCTION(BlueprintCallable, Category = "Damage")
    bool IsPartDestroyed(EShipPart Part) const;

    UFUNCTION(BlueprintCallable, Category = "Damage")
    FString GetDamageReport() const;

    UFUNCTION(BlueprintCallable, Category = "Repair")
    void RepairPart(EShipPart Part, float Amount);

    UFUNCTION(BlueprintCallable, Category = "Repair")
    void RepairAll(float Amount);

    // 网络同步
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

private:
    // 相邻部件映射（用于连锁伤害）
    TMap<EShipPart, TArray<EShipPart>> AdjacencyMap;

    // 反应堆熔毁计时
    float ReactorMeltdownTimer = 0.f;
    bool bMeltdownStarted = false;

    // 初始化相邻关系
    void BuildAdjacencyMap();

    // 连锁伤害
    void TriggerChainDamage(EShipPart SourcePart, float OriginalDamage);

    // 检查飞船是否彻底损毁
    void CheckShipDestruction();

    // 更新特效参数（冒烟/火焰）
    void UpdatePartEffects(float DeltaTime);

    // 反应堆熔毁
    void TickReactorMeltdown(float DeltaTime);
};
