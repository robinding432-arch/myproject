// VitalsComponent.h
// 维生系统：氧气/能量/饥饿/体温/辐射/毒素/流血/太空服
// v5.0：新增辐射环境来源接口 + 太空天气集成
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VitalsComponent.generated.h"

class AProceduralPlanet;
class AMyCharacter;
class ASpaceWeather;

UENUM(BlueprintType)
enum class EVitalStatus : uint8
{
    Normal      UMETA(DisplayName = "Normal"),
    Warning     UMETA(DisplayName = "Warning"),
    Critical    UMETA(DisplayName = "Critical"),
    Depleted    UMETA(DisplayName = "Depleted")
};

USTRUCT(BlueprintType)
struct FVitalStat
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Current = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Max = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DrainRate = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float WarningThreshold = 30.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CriticalThreshold = 15.f;

    EVitalStatus GetStatus() const
    {
        if (Current <= 0.f) return EVitalStatus::Depleted;
        if (Current <= CriticalThreshold) return EVitalStatus::Critical;
        if (Current <= WarningThreshold) return EVitalStatus::Warning;
        return EVitalStatus::Normal;
    }
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UVitalsComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UVitalsComponent();

    UFUNCTION(BlueprintCallable, Category = "Vitals")
    void Init(AMyCharacter* OwnerChar);

    UFUNCTION(BlueprintCallable, Category = "Vitals")
    void TickVitals(float DeltaTime, AProceduralPlanet* CurrentPlanet);

    // ---- 各维生指标 ----
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|Oxygen")
    FVitalStat Oxygen;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|Energy")
    FVitalStat Energy;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|Hunger")
    FVitalStat Hunger; // 0=饱, 100=饿死

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|Thirst")
    FVitalStat Thirst;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|BodyTemp")
    float BodyTemperature = 37.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|BodyTemp")
    float MinSafeTemp = -20.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|BodyTemp")
    float MaxSafeTemp = 50.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|BodyTemp")
    float TempChangeRate = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|Radiation")
    FVitalStat Radiation; // 西弗累积

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|Toxin")
    FVitalStat Toxin;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|Bleeding")
    float BleedRate = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|Health")
    FVitalStat Health;

    // ---- 太空服 ----
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|Suit")
    bool bHasSpaceSuit = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|Suit")
    float SuitIntegrity = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|Suit")
    float SuitInsulation = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|Suit")
    float SuitRadiationShield = 0.9f;

    // ---- 护甲抗性 ----
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|Resistance")
    float HeatResistance = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|Resistance")
    float ColdResistance = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|Resistance")
    float RadiationResistance = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|Resistance")
    float ToxinResistance = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|Resistance")
    float VacuumResistance = 0.f;

    // ---- v5.0：天气引用 ----
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|Weather")
    TSoftObjectPtr<ASpaceWeather> WeatherSource;

    // ---- 公共接口 ----
    UFUNCTION(BlueprintCallable, Category = "Vitals")
    void ConsumeOxygen(float Amount);

    UFUNCTION(BlueprintCallable, Category = "Vitals")
    void ConsumeEnergy(float Amount);

    UFUNCTION(BlueprintCallable, Category = "Vitals")
    void RestoreOxygen(float Amount);

    UFUNCTION(BlueprintCallable, Category = "Vitals")
    void RestoreEnergy(float Amount);

    UFUNCTION(BlueprintCallable, Category = "Vitals")
    void Feed(float Amount);

    UFUNCTION(BlueprintCallable, Category = "Vitals")
    void Hydrate(float Amount);

    UFUNCTION(BlueprintCallable, Category = "Vitals")
    void ApplyRadiation(float Sieverts);

    UFUNCTION(BlueprintCallable, Category = "Vitals")
    void ApplyToxin(float Amount);

    UFUNCTION(BlueprintCallable, Category = "Vitals")
    void StartBleeding(float Rate);

    UFUNCTION(BlueprintCallable, Category = "Vitals")
    void StopBleeding();

    UFUNCTION(BlueprintCallable, Category = "Vitals")
    void DamageSuit(float Amount);

    UFUNCTION(BlueprintCallable, Category = "Vitals")
    void HealHealth(float Amount);

    UFUNCTION(BlueprintCallable, Category = "Vitals")
    bool CanSprint() const;

    UFUNCTION(BlueprintCallable, Category = "Vitals")
    EVitalStatus GetOverallStatus() const;

    UFUNCTION(BlueprintCallable, Category = "Vitals")
    FString GetStatusText() const;

    // v5.0：查询当前环境辐射水平
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Vitals|Weather")
    float GetCurrentRadiationLevel() const { return CurrentRadiationLevel; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Vitals|Weather")
    float GetCurrentSolarWind() const { return CurrentSolarWind; }

    // ---- 事件 ----
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVitalCritical, EVitalStatus, Status);
    UPROPERTY(BlueprintAssignable, Category = "Vitals|Events")
    FOnVitalCritical OnOxygenCritical;

    UPROPERTY(BlueprintAssignable, Category = "Vitals|Events")
    FOnVitalCritical OnHealthCritical;

    UPROPERTY(BlueprintAssignable, Category = "Vitals|Events")
    FOnVitalCritical OnSuitBreach;

    UPROPERTY(BlueprintAssignable, Category = "Vitals|Events")
    FOnVitalCritical OnRadiationCritical;

private:
    UPROPERTY()
    AMyCharacter* OwnerCharacter = nullptr;

    // 环境状态
    bool bInVacuum = false;
    float CurrentEnvironmentTemp = 20.f;
    float CurrentRadiationLevel = 0.f;
    float CurrentSolarWind = 0.f;

    // 更新子函数
    void UpdateOxygen(float DT);
    void UpdateEnergy(float DT);
    void UpdateHunger(float DT);
    void UpdateThirst(float DT);
    void UpdateTemperature(float DT);
    void UpdateRadiation(float DT);
    void UpdateToxin(float DT);
    void UpdateBleeding(float DT);
    void CheckEnvironment(AProceduralPlanet* Planet);
    void ApplyEnvironmentalDamage(float DT);
    void QueryWeather(ASpaceWeather* Weather);
};
