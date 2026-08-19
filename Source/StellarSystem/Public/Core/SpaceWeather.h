// SpaceWeather.h
// 太空天气：太阳风 / 辐射风暴 / 电磁脉冲
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpaceWeather.generated.h"

class AStellarStar;

// 天气事件类型
UENUM(BlueprintType)
enum class EWeatherEvent : uint8
{
    None           UMETA(DisplayName = "Clear Space"),
    SolarWind      UMETA(DisplayName = "Solar Wind"),
    RadiationStorm UMETA(DisplayName = "Radiation Storm"),
    EMPBurst      UMETA(DisplayName = "EMP Burst"),
    MicrometeorShower UMETA(DisplayName = "Micrometeor Shower"),
    NebulaFog      UMETA(DisplayName = "Nebula Interference")
};

// 单个天气事件
USTRUCT(BlueprintType)
struct FWeatherEventData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EWeatherEvent Type = EWeatherEvent::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Intensity = 0.f;       // 0~1

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Duration = 30.f;        // 秒

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RemainingTime = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Epicenter = FVector::ZeroVector; // 事件中心

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EffectRadius = 1000000.f; // 影响半径 cm

    // 运行时
    float BuildupTimer = 0.f;
    bool bActive = false;
    bool bWarningIssued = false;
};

// 天气配置
USTRUCT(BlueprintType)
struct FWeatherSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableSolarWind = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableRadiationStorms = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableEMPBursts = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MinEventInterval = 60.f;   // 事件间隔

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxEventInterval = 300.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float WarningLeadTime = 10.f;    // 预警提前量

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxConcurrentEvents = 3;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeatherWarning, EWeatherEvent, EventType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWeatherStarted, EWeatherEvent, EventType, float, Intensity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeatherEnded, EWeatherEvent, EventType);

UCLASS()
class ASpaceWeather : public AActor
{
    GENERATED_BODY()

public:
    ASpaceWeather();

    virtual void Tick(float DeltaTime) override;
    virtual void BeginPlay() override;

    // ---- 配置 ----
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
    FWeatherSettings Settings;

    // 关联的恒星（读取风强度）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
    TSoftObjectPtr<AStellarStar> SourceStar;

    // ---- 当前事件 ----
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather|State")
    TArray<FWeatherEventData> ActiveEvents;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather|State")
    TArray<FWeatherEventData> PendingWarnings;

    // ---- API ----
    UFUNCTION(BlueprintCallable, Category = "Weather")
    void TriggerEvent(EWeatherEvent Type, float Intensity, float Duration, const FVector& Location, float Radius = 1000000.f);

    UFUNCTION(BlueprintCallable, Category = "Weather")
    void CancelAllEvents();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weather")
    float GetRadiationLevelAt(const FVector& Location) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weather")
    float GetSolarWindAt(const FVector& Location) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weather")
    bool IsInEMPField(const FVector& Location) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weather")
    TArray<EWeatherEvent> GetActiveEventTypes() const;

    // 检查某位置是否受某类事件影响
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weather")
    float GetEventIntensityAt(const FVector& Location, EWeatherEvent Type) const;

    // ---- 委托 ----
    UPROPERTY(BlueprintAssignable, Category = "Weather|Events")
    FOnWeatherWarning OnWeatherWarning;

    UPROPERTY(BlueprintAssignable, Category = "Weather|Events")
    FOnWeatherStarted OnWeatherStarted;

    UPROPERTY(BlueprintAssignable, Category = "Weather|Events")
    FOnWeatherEnded OnWeatherEnded;

protected:
    // 计时器
    float NextEventTimer = 0.f;
    float EventCheckTimer = 0.f;

    // 生成随机事件
    void ScheduleNextEvent();
    void GenerateRandomEvent();

    // 更新事件生命周期
    void UpdateEvents(float DeltaTime);

    // 应用效果到玩家/飞船
    void ApplyEffects(float DeltaTime);

    // 视觉特效
    void SpawnWeatherEffect(EWeatherEvent Type, const FVector& Location, float Intensity);
    void UpdateWeatherEffects(float DeltaTime);

    // 恒星风基础强度
    float GetBaseStellarWind() const;

private:
    // 特效组件列表
    UPROPERTY()
    TArray<UParticleSystemComponent*> ActiveEffects;
};
