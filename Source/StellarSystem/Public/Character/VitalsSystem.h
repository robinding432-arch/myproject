// VitalsSystem.h
// 维生系统：氧气/能量/辐射/温度/饥饿/毒素/HP 全模拟
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VitalsSystem.generated.h"

class UCharacterCustomizationComponent;
class UInventoryComponent;

// 环境危险类型
UENUM(BlueprintType)
enum class EHazardType : uint8
{
    None, Vacuum, ToxicAtmosphere, ExtremeHeat, ExtremeCold,
    Radiation, Corrosive, BiolHazard, EMP
};

// 维生状态快照（用于 HUD/存档）
USTRUCT(BlueprintType)
struct FVitalsSnapshot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Health = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Oxygen = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Energy = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Stamina = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Hunger = 0.f;       // 0=饱, 100=饿死
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Thirst = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BodyTemp = 37.f;    // 摄氏度
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Radiation = 0.f;    // 西弗累积
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Toxin = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BleedingRate = 0.f; // 每秒失血
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SuitIntegrity = 100.f; // 太空服完整度
};

// 维生组件（挂在 Character 上）
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UVitalsComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UVitalsComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;
    virtual void BeginPlay() override;

    // —— 当前状态 ——
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated)
    FVitalsSnapshot Vitals;

    // —— 配置 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxHealth = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxOxygen = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxEnergy = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxStamina = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OxygenConsumeRate = 1.f;       // 正常消耗/秒
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EnergyConsumeRate = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SprintOxygenMult = 3.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SprintEnergyMult = 2.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float JumpOxygenCost = 2.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float JumpEnergyCost = 5.f;

    // —— 环境 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CurrentOxygenLevel = 1.f;   // 1=正常空气, 0=真空
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CurrentTemperature = 20.f;  // 摄氏度
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CurrentRadiation = 0.f;     // 西弗/秒
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CurrentToxinLevel = 0.f;    // 毒素浓度
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EHazardType CurrentHazard = EHazardType::None;

    // 是否在飞船/空间站内（安全环境）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bInSafeEnvironment = false;

    // —— 装甲防护（引用护甲抗性）——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ArmorThermalResist = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ArmorRadiationResist = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ArmorToxinResist = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ArmorVacuumResist = 0.f;   // 太空服等级

    // —— 状态查询 ——
    UFUNCTION(BlueprintCallable, Category="Vitals")
    bool IsAlive() const { return Vitals.Health > 0.f; }

    UFUNCTION(BlueprintCallable, Category="Vitals")
    bool IsInDanger() const;

    UFUNCTION(BlueprintCallable, Category="Vitals")
    float GetOverallCondition() const; // 0~1 综合状态

    // —— 消耗品接口 ——
    UFUNCTION(BlueprintCallable, Server, Reliable, Category="Vitals")
    void ServerConsumeOxygen(float Amount);

    UFUNCTION(BlueprintCallable, Server, Reliable, Category="Vitals")
    void ServerConsumeEnergy(float Amount);

    UFUNCTION(BlueprintCallable, Server, Reliable, Category="Vitals")
    void ServerHeal(float Amount);

    UFUNCTION(BlueprintCallable, Server, Reliable, Category="Vitals")
    void ServerCureRadiation(float Amount);

    UFUNCTION(BlueprintCallable, Server, Reliable, Category="Vitals")
    void ServerCureToxin(float Amount);

    UFUNCTION(BlueprintCallable, Server, Reliable, Category="Vitals")
    void ServerFeed(float FoodAmount, float WaterAmount);

    UFUNCTION(BlueprintCallable, Server, Reliable, Category="Vitals")
    void ServerApplyDamage(float Amount, bool bIgnoreArmor = false);

    UFUNCTION(BlueprintCallable, Server, Reliable, Category="Vitals")
    void ServerRepairSuit(float Amount);

    // —— 设置环境（由行星/飞船/太空 Volume 调用）——
    UFUNCTION(BlueprintCallable, Category="Vitals")
    void SetEnvironment(EHazardType Hazard, float OxygenLevel, float Temperature,
                        float RadiationRate, float ToxinLevel);

    UFUNCTION(BlueprintCallable, Category="Vitals")
    void SetSafeEnvironment(bool bSafe);

    // —— 事件 ——
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeath);
    UPROPERTY(BlueprintAssignable)
    FOnDeath OnDeath;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVitalsChanged, const FVitalsSnapshot&, Snapshot);
    UPROPERTY(BlueprintAssignable)
    FOnVitalsChanged OnVitalsChanged;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWarning, FName, WarningType);
    UPROPERTY(BlueprintAssignable)
    FOnWarning OnWarning;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

private:
    float AccumulatedHunger = 0.f;
    float AccumulatedThirst = 0.f;
    bool bWasInDanger = false;

    void UpdateOxygen(float Dt);
    void UpdateEnergy(float Dt);
    void UpdateStamina(float Dt);
    void UpdateHungerThirst(float Dt);
    void UpdateTemperature(float Dt);
    void UpdateRadiation(float Dt);
    void UpdateToxin(float Dt);
    void UpdateBleeding(float Dt);
    void CheckDeath();
    void CheckWarnings();
    void ApplyEnvironmentalDamage(float Dt);
    bool bIsDead = false;
};
