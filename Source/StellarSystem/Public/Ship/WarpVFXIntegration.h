// ============================================================
// WarpVFXIntegration.h
// 跃迁光效 + 音频集成：把 StellarVisualEffects 和 AudioManager 接入 ShipPawn
// 路径: Source/StellarSystem/Public/Ship/WarpVFXIntegration.h
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "WarpVFXIntegration.generated.h"

class AStellarVisualEffects;
class UAudioManager;
class AShipPawn;

// 集成器：挂在 ShipPawn 上，统一驱动跃迁光效+音频
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UWarpVFXIntegration : public UActorComponent
{
    GENERATED_BODY()

public:
    UWarpVFXIntegration();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;
    virtual void BeginPlay() override;

    // —— 配置 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WarpVFX")
    float MinEnginePitch = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WarpVFX")
    float MaxEnginePitch = 2.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WarpVFX")
    float MinEngineVolume = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WarpVFX")
    float MaxEngineVolume = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WarpVFX")
    float ChargeRumbleIntensity = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WarpVFX")
    float ArrivalScreenShake = 1.5f;

    // —— 外部引用 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WarpVFX|References")
    AStellarVisualEffects* VisualEffectsActor = nullptr;

    // —— 公共接口（由 ShipPawn 调用）——
    UFUNCTION(BlueprintCallable, Category = "WarpVFX")
    void OnWarpStarted(const FVector& WarpTargetLocation);

    UFUNCTION(BlueprintCallable, Category = "WarpVFX")
    void OnWarpProgress(float Progress, const FVector& CurrentDirection);

    UFUNCTION(BlueprintCallable, Category = "WarpVFX")
    void OnWarpCompleted(const FVector& ArrivalLocation);

    UFUNCTION(BlueprintCallable, Category = "WarpVFX")
    void OnWarpAborted();

    // —— 引擎音频更新（每帧由 ShipPawn 调用）——
    UFUNCTION(BlueprintCallable, Category = "WarpVFX|Engine")
    void UpdateEngineAudio(float ThrustInput, float CurrentSpeed, float MaxSpeed);

    // —— 伤害音效 ——
    UFUNCTION(BlueprintCallable, Category = "WarpVFX|Combat")
    void OnShieldHit(float DamageAmount);

    UFUNCTION(BlueprintCallable, Category = "WarpVFX|Combat")
    void OnHullHit(float DamageAmount);

    UFUNCTION(BlueprintCallable, Category = "WarpVFX|Combat")
    void OnHullCritical();

    // —— 武器音效 ——
    UFUNCTION(BlueprintCallable, Category = "WarpVFX|Combat")
    void OnWeaponFired(EAudioCategory WeaponType);

    UFUNCTION(BlueprintCallable, Category = "WarpVFX|Combat")
    void OnWeaponReload();

    UFUNCTION(BlueprintCallable, Category = "WarpVFX|Combat")
    void OnTargetLocked();

    UFUNCTION(BlueprintCallable, Category = "WarpVFX|Combat")
    void OnTargetLost();

    // —— 环境音更新 ——
    UFUNCTION(BlueprintCallable, Category = "WarpVFX|Ambient")
    void OnBiomeChanged(EBiomeType NewBiome);

    UFUNCTION(BlueprintCallable, Category = "WarpVFX|Ambient")
    void OnSpaceWeatherChanged(EAudioCategory WeatherType, float Intensity);

    // —— 生命周期 ——
    UFUNCTION(BlueprintCallable, Category = "WarpVFX")
    void InitializeForShip(AShipPawn* OwningShip);

private:
    // 缓存
    UPROPERTY()
    UAudioManager* AudioMgr = nullptr;

    UPROPERTY()
    AShipPawn* OwningShip = nullptr;

    // 状态
    bool bWarpActive = false;
    float LastThrustInput = 0.f;
    float EngineRampSpeed = 2.f;

    // 查找辅助
    UAudioManager* GetAudioManager() const;
    AStellarVisualEffects* FindVisualEffects() const;

    // 屏幕震动
    void TriggerScreenShake(float Intensity);

    // 音频淡变
    void RampEngineVolume(float TargetVol, float Dt);
    float CurrentEngineVol = 0.f;
};
