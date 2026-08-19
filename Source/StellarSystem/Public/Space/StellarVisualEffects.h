// ============================================================
// StellarVisualEffects.h
// 宇宙光效系统：恒星日冕/光晕/镜头光晕/行星大气散射/跃迁光效
// 路径: Source/StellarSystem/Public/Space/StellarVisualEffects.h
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "StellarVisualEffects.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class UPostProcessComponent;
class USkyAtmosphereComponent;
class UVolumetricCloudComponent;

// 日冕层类型
UENUM(BlueprintType)
enum class ECoronaLayer : uint8
{
    InnerCorona  UMETA(DisplayName = "Inner Corona"),
    OuterCorona  UMETA(DisplayName = "Outer Corona"),
    Prominence   UMETA(DisplayName = "Solar Prominence"),
    SolarFlare   UMETA(DisplayName = "Solar Flare")
};

// 跃迁光效阶段
UENUM(BlueprintType)
enum class EWarpVFXPhase : uint8
{
    Charging     UMETA(DisplayName = "Charging Up"),
    WarpTunnel   UMETA(DisplayName = "Warp Tunnel"),
    Cruising     UMETA(DisplayName = "Cruising"),
    ArrivalFlash UMETA(DisplayName = "Arrival Flash")
};

// 恒星视觉参数
USTRUCT(BlueprintType)
struct FStarVisualParams
{
    GENERATED_BODY()

    // —— 核心发光 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star|Core")
    FLinearColor CoreColor = FLinearColor(1.f, 0.95f, 0.8f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star|Core")
    float CoreIntensity = 50000000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star|Core")
    float CoreRadius = 1.f; // 乘数

    // —— 日冕层 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star|Corona")
    FLinearColor CoronaColor = FLinearColor(1.f, 0.6f, 0.2f, 0.5f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star|Corona")
    float CoronaRadius = 2.5f; // 乘数

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star|Corona")
    float CoronaOpacity = 0.6f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star|Corona")
    float ProminenceFrequency = 3.f; // 日珥频率

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star|Corona")
    float ProminenceAmplitude = 0.3f; // 日珥幅度

    // —— 光晕/散射 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star|Halo")
    FLinearColor HaloColor = FLinearColor(1.f, 0.8f, 0.5f, 0.3f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star|Halo")
    float HaloRadius = 5.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star|Halo")
    float HaloFalloff = 2.f;

    // —— 镜头光晕 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star|LensFlare")
    bool bEnableLensFlare = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star|LensFlare")
    float LensFlareIntensity = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star|LensFlare")
    int32 LensFlareCount = 6;

    // —— 脉动 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star|Pulse")
    float PulseFrequency = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star|Pulse")
    float PulseAmplitude = 0.05f;

    // —— 色散/光球细节 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star|Surface")
    float SurfaceTurbulence = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star|Surface")
    float SurfaceSpeed = 0.3f;
};

// 跃迁视觉参数
USTRUCT(BlueprintType)
struct FWarpVFXParams
{
    GENERATED_BODY()

    // —— 蓄能阶段 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp|Charge")
    FLinearColor ChargeColor = FLinearColor(0.2f, 0.6f, 1.f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp|Charge")
    float ChargeDuration = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp|Charge")
    float ChargeGlowRadius = 3.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp|Charge")
    float ChargeIntensity = 2.f;

    // —— 跃迁隧道 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp|Tunnel")
    FLinearColor TunnelColor = FLinearColor(0.1f, 0.4f, 1.f, 0.8f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp|Tunnel")
    FLinearColor TunnelCoreColor = FLinearColor(0.8f, 0.9f, 1.f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp|Tunnel")
    float TunnelLength = 50000.f; // cm

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp|Tunnel")
    float TunnelRadius = 800.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp|Tunnel")
    float TunnelSpiralSpeed = 3.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp|Tunnel")
    int32 TunnelParticleCount = 2000;

    // —— 星流效应（经过星星时的拖尾） ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp|StarStream")
    FLinearColor StarStreamColor = FLinearColor(1.f, 1.f, 1.f, 0.9f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp|StarStream")
    float StarStreamDensity = 0.7f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp|StarStream")
    float StarStreamSpeed = 5.f;

    // —— 到达闪光 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp|Arrival")
    FLinearColor ArrivalColor = FLinearColor(0.6f, 0.8f, 1.f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp|Arrival")
    float ArrivalFlashRadius = 5000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp|Arrival")
    float ArrivalFlashDuration = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp|Arrival")
    float ArrivalShockwaveRadius = 10000.f;

    // —— 屏幕特效 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp|Screen")
    float ScreenDistortion = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp|Screen")
    float MotionBlurAmount = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp|Screen")
    FLinearColor VignetteColor = FLinearColor(0.f, 0.05f, 0.2f, 0.5f);
};

// 行星大气光效参数
USTRUCT(BlueprintType)
struct FPlanetAtmosphereParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere")
    FLinearColor RayleighColor = FLinearColor(0.2f, 0.4f, 1.f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere")
    float RayleighScattering = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere")
    float MieScattering = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere")
    FLinearColor MieColor = FLinearColor(1.f, 0.9f, 0.7f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere")
    float AtmosphereHeight = 50000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere")
    float EdgeGlowIntensity = 1.5f;

    // 极光
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora")
    bool bEnableAurora = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora")
    FLinearColor AuroraColor = FLinearColor(0.2f, 1.f, 0.5f, 0.8f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora")
    float AuroraIntensity = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora")
    float AuroraSpeed = 0.2f;
};

// ============================================================
// AStellarVisualEffects — 挂载到恒星上，管理所有宇宙光效
// ============================================================
UCLASS()
class AStellarVisualEffects : public AActor
{
    GENERATED_BODY()

public:
    AStellarVisualEffects();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // —— 恒星视觉参数 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Visual|Star")
    FStarVisualParams StarVisualParams;

    // —— 跃迁视觉参数 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual|Warp")
    FWarpVFXParams WarpVFXParams;

    // —— 行星大气参数（应用到行星） ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual|Atmosphere")
    FPlanetAtmosphereParams PlanetAtmosphereParams;

    // —— 组件 ——
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* StarCoreMesh = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* CoronaMesh = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* HaloMesh = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UPointLightComponent* StarLight = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UNiagaraComponent* ProminenceEffect = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UNiagaraComponent* WarpTunnelEffect = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UNiagaraComponent* StarStreamEffect = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UNiagaraComponent* ArrivalFlashEffect = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USkyAtmosphereComponent* SkyAtmosphere = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UVolumetricCloudComponent* VolumetricClouds = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UPostProcessComponent* WarpPostProcess = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UExponentialHeightFogComponent* SpaceFog = nullptr;

    // —— API ——

    // 初始化恒星光效（由 StellarStar 调用）
    UFUNCTION(BlueprintCallable, Category = "Visual|Star")
    void InitializeForStar(EStarType StarType, float StarRadius);

    // 跃迁光效控制
    UFUNCTION(BlueprintCallable, Category = "Visual|Warp")
    void StartWarpCharge(const FVector& WarpDirection);

    UFUNCTION(BlueprintCallable, Category = "Visual|Warp")
    void UpdateWarpTunnel(float Progress, const FVector& Direction);

    UFUNCTION(BlueprintCallable, Category = "Visual|Warp")
    void TriggerArrivalFlash(const FVector& Location);

    UFUNCTION(BlueprintCallable, Category = "Visual|Warp")
    void EndWarpEffects();

    // 日冕爆发（太阳耀斑事件）
    UFUNCTION(BlueprintCallable, Category = "Visual|Star")
    void TriggerSolarFlare(float Intensity = 1.f, float Duration = 3.f);

    // 极光控制
    UFUNCTION(BlueprintCallable, Category = "Visual|Aurora")
    void SetAuroraIntensity(float Intensity);

    // 镜头光晕更新
    UFUNCTION(BlueprintCallable, Category = "Visual|LensFlare")
    void UpdateLensFlares(const FVector& CameraLocation);

    // 应用大气参数到行星
    UFUNCTION(BlueprintCallable, Category = "Visual|Atmosphere")
    void ApplyAtmosphereToPlanet(AActor* Planet);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

private:
    // 恒星脉动
    void UpdateStarPulse(float Dt);
    float PulsePhase = 0.f;

    // 日冕动画
    void UpdateCorona(float Dt);
    float CoronaPhase = 0.f;

    // 光晕脉动
    void UpdateHalo(float Dt);

    // 镜头光晕计算
    TArray<FVector> CachedFlareOffsets;
    void ComputeLensFlareOffsets();

    // 跃迁状态
    EWarpVFXPhase CurrentWarpPhase = EWarpVFXPhase::Charging;
    float WarpEffectTimer = 0.f;
    FVector WarpDirection = FVector::ForwardVector;

    // 太阳耀斑
    bool bFlareActive = false;
    float FlareTimer = 0.f;
    float FlareIntensity = 0.f;
    float FlareDuration = 0.f;

    // 初始化辅助
    void CreateCoronaMesh();
    void CreateHaloMesh();
    void SetupNiagaraSystems();
    void SetupPostProcess();
    void ApplyStarTypeVisuals(EStarType StarType);
};
