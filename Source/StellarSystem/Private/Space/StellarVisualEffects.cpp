// ============================================================
// StellarVisualEffects.cpp
// 宇宙光效完整实现
// 路径: Source/StellarSystem/Private/Space/StellarVisualEffects.cpp
// ============================================================

#include "Space/StellarVisualEffects.h"
#include "Core/StellarStar.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Components/PostProcessComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Math/UnrealMathUtility.h"

// ======================== 构造 ========================

AStellarVisualEffects::AStellarVisualEffects()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(RootComponent);

    // 恒星核心
    StarCoreMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StarCore"));
    StarCoreMesh->SetupAttachment(RootComponent);
    StarCoreMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 日冕层
    CoronaMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Corona"));
    CoronaMesh->SetupAttachment(RootComponent);
    CoronaMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 光晕
    HaloMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Halo"));
    HaloMesh->SetupAttachment(RootComponent);
    HaloMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 恒星光源
    StarLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StarLight"));
    StarLight->SetupAttachment(RootComponent);
    StarLight->SetIntensity(50000000.f);
    StarLight->SetAttenuationRadius(1e10f);
    StarLight->SetLightColor(FLinearColor(1.f, 0.95f, 0.8f));
    StarLight->bUseInverseSquaredFalloff = false;

    // Niagara 组件
    ProminenceEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Prominence"));
    ProminenceEffect->SetupAttachment(RootComponent);

    WarpTunnelEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("WarpTunnel"));
    WarpTunnelEffect->SetupAttachment(RootComponent);
    WarpTunnelEffect->SetAutoActivate(false);

    StarStreamEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("StarStream"));
    StarStreamEffect->SetupAttachment(RootComponent);
    StarStreamEffect->SetAutoActivate(false);

    ArrivalFlashEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ArrivalFlash"));
    ArrivalFlashEffect->SetupAttachment(RootComponent);
    ArrivalFlashEffect->SetAutoActivate(false);

    // 大气散射
    SkyAtmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("SkyAtmosphere"));
    SkyAtmosphere->SetupAttachment(RootComponent);

    // 体积云
    VolumetricClouds = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("VolumetricClouds"));
    VolumetricClouds->SetupAttachment(RootComponent);

    // 后处理（跃迁扭曲）
    WarpPostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("WarpPostProcess"));
    WarpPostProcess->SetupAttachment(RootComponent);

    // 空间雾
    SpaceFog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("SpaceFog"));
    SpaceFog->SetupAttachment(RootComponent);
    SpaceFog->SetFogDensity(0.0001f);
    SpaceFog->SetFogHeightFalloff(0.01f);

    // 默认球体
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(
        TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereFinder.Succeeded())
    {
        StarCoreMesh->SetStaticMesh(SphereFinder.Object);
        CoronaMesh->SetStaticMesh(SphereFinder.Object);
        HaloMesh->SetStaticMesh(SphereFinder.Object);
    }

    // 初始化变换
    StarCoreMesh->SetWorldScale3D(FVector(1.f));
    CoronaMesh->SetWorldScale3D(FVector(2.5f));
    CoronaMesh->SetTranslucencySortPriority(1);
    HaloMesh->SetWorldScale3D(FVector(5.f));
    HaloMesh->SetTranslucencySortPriority(0);

    // 计算镜头光晕偏移
    ComputeLensFlareOffsets();
}

// ======================== 生命周期 ========================

void AStellarVisualEffects::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        // 初始化大气参数
        if (SkyAtmosphere)
        {
            SkyAtmosphere->SetAtmosphereHeight(PlanetAtmosphereParams.AtmosphereHeight);
            SkyAtmosphere->SetRayleighScattering(PlanetAtmosphereParams.RayleighColor);
            SkyAtmosphere->SetMieScattering(PlanetAtmosphereParams.MieColor);
        }
    }

    // 创建动态材质实例
    CreateCoronaMesh();
    CreateHaloMesh();
    SetupNiagaraSystems();
    SetupPostProcess();
}

void AStellarVisualEffects::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 恒星脉动
    UpdateStarPulse(DeltaTime);

    // 日冕动画
    UpdateCorona(DeltaTime);

    // 光晕脉动
    UpdateHalo(DeltaTime);

    // 耀斑计时
    if (bFlareActive)
    {
        FlareTimer += DeltaTime;
        if (FlareTimer >= FlareDuration)
        {
            bFlareActive = false;
            // 恢复日冕
            if (CoronaMesh)
            {
                CoronaMesh->SetScalarParameterValueOnMaterials(TEXT("Intensity"), 1.f);
            }
        }
    }

    // 跃迁效果计时
    if (CurrentWarpPhase != EWarpVFXPhase::Charging)
    {
        WarpEffectTimer += DeltaTime;
    }
}

// ======================== 恒星初始化 ========================

void AStellarVisualEffects::InitializeForStar(EStarType StarType, float InStarRadius)
{
    ApplyStarTypeVisuals(StarType);

    // 缩放
    float CoreScale = InStarRadius * 0.001f * StarVisualParams.CoreRadius;
    StarCoreMesh->SetWorldScale3D(FVector(CoreScale));

    float CoronaScale = InStarRadius * 0.001f * StarVisualParams.CoronaRadius;
    CoronaMesh->SetWorldScale3D(FVector(CoronaScale));

    float HaloScale = InStarRadius * 0.001f * StarVisualParams.HaloRadius;
    HaloMesh->SetWorldScale3D(FVector(HaloScale));

    // 光源强度
    StarLight->SetIntensity(StarVisualParams.CoreIntensity * (InStarRadius / 69600000.f));
    StarLight->SetLightColor(StarVisualParams.CoreColor);
}

void AStellarVisualEffects::ApplyStarTypeVisuals(EStarType StarType)
{
    switch (StarType)
    {
    case EStarType::RedDwarf:
        StarVisualParams.CoreColor = FLinearColor(1.f, 0.3f, 0.1f, 1.f);
        StarVisualParams.CoronaColor = FLinearColor(1.f, 0.2f, 0.05f, 0.4f);
        StarVisualParams.HaloColor = FLinearColor(0.8f, 0.2f, 0.1f, 0.2f);
        StarVisualParams.CoronaRadius = 1.8f;
        StarVisualParams.PulseAmplitude = 0.1f;
        StarVisualParams.PulseFrequency = 0.05f;
        break;

    case EStarType::MainSequence:
        StarVisualParams.CoreColor = FLinearColor(1.f, 0.95f, 0.8f, 1.f);
        StarVisualParams.CoronaColor = FLinearColor(1.f, 0.6f, 0.2f, 0.5f);
        StarVisualParams.HaloColor = FLinearColor(1.f, 0.8f, 0.5f, 0.3f);
        StarVisualParams.CoronaRadius = 2.5f;
        StarVisualParams.HaloRadius = 5.f;
        StarVisualParams.PulseAmplitude = 0.05f;
        StarVisualParams.PulseFrequency = 0.1f;
        break;

    case EStarType::BlueGiant:
        StarVisualParams.CoreColor = FLinearColor(0.6f, 0.7f, 1.f, 1.f);
        StarVisualParams.CoronaColor = FLinearColor(0.4f, 0.6f, 1.f, 0.6f);
        StarVisualParams.HaloColor = FLinearColor(0.5f, 0.6f, 1.f, 0.4f);
        StarVisualParams.CoronaRadius = 3.0f;
        StarVisualParams.HaloRadius = 8.f;
        StarVisualParams.PulseAmplitude = 0.08f;
        StarVisualParams.PulseFrequency = 0.15f;
        break;

    case EStarType::RedGiant:
        StarVisualParams.CoreColor = FLinearColor(1.f, 0.4f, 0.2f, 1.f);
        StarVisualParams.CoronaColor = FLinearColor(1.f, 0.3f, 0.1f, 0.7f);
        StarVisualParams.HaloColor = FLinearColor(1.f, 0.5f, 0.2f, 0.5f);
        StarVisualParams.CoronaRadius = 4.0f;
        StarVisualParams.HaloRadius = 10.f;
        StarVisualParams.PulseAmplitude = 0.15f;
        StarVisualParams.PulseFrequency = 0.03f;
        break;

    case EStarType::WhiteDwarf:
        StarVisualParams.CoreColor = FLinearColor(0.9f, 0.9f, 1.f, 1.f);
        StarVisualParams.CoronaColor = FLinearColor(0.7f, 0.7f, 1.f, 0.3f);
        StarVisualParams.HaloColor = FLinearColor(0.8f, 0.8f, 1.f, 0.2f);
        StarVisualParams.CoronaRadius = 1.5f;
        StarVisualParams.HaloRadius = 3.f;
        StarVisualParams.PulseAmplitude = 0.02f;
        StarVisualParams.PulseFrequency = 0.5f;
        break;

    case EStarType::NeutronStar:
        StarVisualParams.CoreColor = FLinearColor(0.5f, 0.7f, 1.f, 1.f);
        StarVisualParams.CoronaColor = FLinearColor(0.3f, 0.5f, 1.f, 0.8f);
        StarVisualParams.HaloColor = FLinearColor(0.4f, 0.6f, 1.f, 0.5f);
        StarVisualParams.CoronaRadius = 2.0f;
        StarVisualParams.HaloRadius = 6.f;
        StarVisualParams.PulseAmplitude = 0.2f;
        StarVisualParams.PulseFrequency = 2.f; // 脉冲星快速闪烁
        break;

    case EStarType::BlackHole:
        StarVisualParams.CoreColor = FLinearColor(0.f, 0.f, 0.f, 1.f);
        StarVisualParams.CoronaColor = FLinearColor(0.8f, 0.6f, 0.2f, 0.9f); // 吸积盘
        StarVisualParams.HaloColor = FLinearColor(1.f, 0.7f, 0.3f, 0.6f);
        StarVisualParams.CoronaRadius = 3.5f;
        StarVisualParams.HaloRadius = 12.f;
        StarVisualParams.PulseAmplitude = 0.1f;
        StarVisualParams.PulseFrequency = 0.5f;
        break;
    }

    // 应用到光源
    StarLight->SetLightColor(StarVisualParams.CoreColor);
}

// ======================== 恒星脉动 ========================

void AStellarVisualEffects::UpdateStarPulse(float Dt)
{
    PulsePhase += Dt * StarVisualParams.PulseFrequency * 2.f * PI;

    float Pulse = FMath::Sin(PulsePhase) * 0.5f + 0.5f;
    Pulse *= StarVisualParams.PulseAmplitude;

    // 核心亮度脉动
    float BaseIntensity = StarVisualParams.CoreIntensity;
    StarLight->SetIntensity(BaseIntensity * (1.f + Pulse));

    // 核心缩放脉动（微小）
    float CoreScale = StarVisualParams.CoreRadius * (1.f + Pulse * 0.02f);
    StarCoreMesh->SetWorldScale3D(FVector(CoreScale));

    // 表面湍流：旋转核心
    StarCoreMesh->AddLocalRotation(FRotator(0.f,
        Dt * StarVisualParams.SurfaceSpeed * 10.f, 0.f));
}

// ======================== 日冕动画 ========================

void AStellarVisualEffects::UpdateCorona(float Dt)
{
    CoronaPhase += Dt * StarVisualParams.ProminenceFrequency;

    // 日珥：径向缩放脉动
    float Prominence = FMath::Sin(CoronaPhase) * StarVisualParams.ProminenceAmplitude;
    float Scale = StarVisualParams.CoronaRadius * (1.f + Prominence);

    // 非均匀缩放模拟日珥突起
    float XScale = Scale * (1.f + FMath::Sin(CoronaPhase * 1.3f) * 0.15f);
    float YScale = Scale * (1.f + FMath::Cos(CoronaPhase * 0.7f) * 0.1f);
    float ZScale = Scale * (1.f + FMath::Sin(CoronaPhase * 1.7f) * 0.12f);

    CoronaMesh->SetWorldScale3D(FVector(XScale, YScale, ZScale));

    // 日冕颜色脉动
    FLinearColor CoronaCol = StarVisualParams.CoronaColor;
    CoronaCol.A = StarVisualParams.CoronaOpacity * (1.f + Prominence * 0.5f);
    CoronaMesh->SetVectorParameterValueOnMaterials(TEXT("Color"), FVector(CoronaCol.R, CoronaCol.G, CoronaCol.B));
    CoronaMesh->SetScalarParameterValueOnMaterials(TEXT("Opacity"), CoronaCol.A);

    // 旋转日冕（不同速度）
    CoronaMesh->AddLocalRotation(FRotator(0.f, Dt * 5.f, 0.f));
}

// ======================== 光晕 ========================

void AStellarVisualEffects::UpdateHalo(float Dt)
{
    // 光晕缓慢呼吸
    float Breath = FMath::Sin(PulsePhase * 0.5f) * 0.5f + 0.5f;
    float HaloScale = StarVisualParams.HaloRadius * (1.f + Breath * 0.05f);
    HaloMesh->SetWorldScale3D(FVector(HaloScale));

    // 光晕颜色
    FLinearColor HaloCol = StarVisualParams.HaloColor;
    HaloCol.A = 0.3f * (1.f - Breath * 0.3f); // 亮时稍透明
    HaloMesh->SetVectorParameterValueOnMaterials(TEXT("Color"), FVector(HaloCol.R, HaloCol.G, HaloCol.B));
    HaloMesh->SetScalarParameterValueOnMaterials(TEXT("Opacity"), HaloCol.A);
}

// ======================== 镜头光晕 ========================

void AStellarVisualEffects::ComputeLensFlareOffsets()
{
    CachedFlareOffsets.Reset();
    for (int32 i = 0; i < StarVisualParams.LensFlareCount; ++i)
    {
        float T = (float)i / (float)StarVisualParams.LensFlareCount;
        // 沿光轴分散
        float Offset = FMath::Pow(T, 1.5f) * 0.8f;
        // 交替方向
        if (i % 2 == 1) Offset = -Offset * 0.6f;
        CachedFlareOffsets.Add(FVector(Offset, 0, 0));
    }
}

void AStellarVisualEffects::UpdateLensFlares(const FVector& CameraLocation)
{
    if (!StarVisualParams.bEnableLensFlare) return;

    // 计算屏幕空间光晕位置
    FVector ToCamera = (CameraLocation - GetActorLocation()).GetSafeNormal();
    FVector LightDir = -ToCamera; // 从相机看向光源

    // 简化：通过 StarLight 的屏幕位置驱动
    // 实际项目中应使用 Material Parameter Collection 传给全屏后处理
    float Intensity = StarVisualParams.LensFlareIntensity;
    StarLight->SetIntensity(StarLight->Intensity * (1.f + Intensity * 0.01f));
}

// ======================== 耀斑 ========================

void AStellarVisualEffects::TriggerSolarFlare(float Intensity, float Duration)
{
    bFlareActive = true;
    FlareTimer = 0.f;
    FlareIntensity = Intensity;
    FlareDuration = Duration;

    // 增强日冕亮度
    if (CoronaMesh)
    {
        CoronaMesh->SetScalarParameterValueOnMaterials(TEXT("Intensity"),
            1.f + Intensity * 3.f);
    }

    // 光源闪烁
    StarLight->SetIntensity(StarLight->Intensity * (1.f + Intensity));

    UE_LOG(LogTemp, Warning, TEXT("[VFX] Solar Flare! Intensity=%.1f Duration=%.1fs"),
        Intensity, Duration);
}

// ======================== 极光 ========================

void AStellarVisualEffects::SetAuroraIntensity(float Intensity)
{
    PlanetAtmosphereParams.AuroraIntensity = FMath::Clamp(Intensity, 0.f, 3.f);

    if (VolumetricClouds)
    {
        // 用体积云模拟极光带
        VolumetricClouds->SetRenderTargetResolutionScale(1.f + Intensity * 0.2f);
    }
}

// ======================== 跃迁光效 ========================

void AStellarVisualEffects::StartWarpCharge(const FVector& WarpDirection)
{
    CurrentWarpPhase = EWarpVFXPhase::Charging;
    WarpEffectTimer = 0.f;
    this->WarpDirection = WarpDirection.GetSafeNormal();

    // 激活跃迁隧道粒子
    if (WarpTunnelEffect)
    {
        WarpTunnelEffect->SetActive(true);
        WarpTunnelEffect->SetVariableVec3(TEXT("TunnelDirection"), WarpDirection);
        WarpTunnelEffect->SetVariableFloat(TEXT("ChargeProgress"), 0.f);
        WarpTunnelEffect->SetVariableLinearColor(TEXT("TunnelColor"),
            FLinearColor(WarpVFXParams.TunnelColor));
    }

    // 后处理：屏幕扭曲开始
    if (WarpPostProcess)
    {
        WarpPostProcess->Settings.bOverride_ChromaticAberration = true;
        WarpPostProcess->Settings.ChromaticAberration = 0.5f;
        WarpPostProcess->Settings.bOverride_MotionBlurAmount = true;
        WarpPostProcess->Settings.MotionBlurAmount = WarpVFXParams.MotionBlurAmount * 0.5f;
    }

    UE_LOG(LogTemp, Log, TEXT("[VFX] Warp charge started"));
}

void AStellarVisualEffects::UpdateWarpTunnel(float Progress, const FVector& Direction)
{
    if (CurrentWarpPhase == EWarpVFXPhase::Charging && Progress > 0.15f)
    {
        CurrentWarpPhase = EWarpVFXPhase::WarpTunnel;
    }

    WarpDirection = Direction.GetSafeNormal();

    if (WarpTunnelEffect)
    {
        WarpTunnelEffect->SetVariableFloat(TEXT("ChargeProgress"), Progress);
        WarpTunnelEffect->SetVariableFloat(TEXT("SpiralSpeed"),
            WarpVFXParams.TunnelSpiralSpeed * (1.f + Progress));
        WarpTunnelEffect->SetVariableFloat(TEXT("TunnelRadius"),
            WarpVFXParams.TunnelRadius * (1.f + Progress * 0.5f));
    }

    // 后处理随进度增强
    if (WarpPostProcess)
    {
        float Chromatic = FMath::Lerp(0.5f, 2.f, Progress);
        WarpPostProcess->Settings.ChromaticAberration = Chromatic;

        float Motion = FMath::Lerp(WarpVFXParams.MotionBlurAmount * 0.5f,
            WarpVFXParams.MotionBlurAmount, Progress);
        WarpPostProcess->Settings.MotionBlurAmount = Motion;

        // 暗角
        WarpPostProcess->Settings.bOverride_VignetteIntensity = true;
        WarpPostProcess->Settings.VignetteIntensity = Progress * 0.8f;
    }

    // 星流效果
    if (StarStreamEffect && Progress > 0.3f)
    {
        StarStreamEffect->SetActive(true);
        StarStreamEffect->SetVariableVec3(TEXT("StreamDirection"), Direction);
        StarStreamEffect->SetVariableFloat(TEXT("StreamSpeed"),
            WarpVFXParams.StarStreamSpeed * Progress);
        StarStreamEffect->SetVariableLinearColor(TEXT("StreamColor"),
            FLinearColor(WarpVFXParams.StarStreamColor));
    }
}

void AStellarVisualEffects::TriggerArrivalFlash(const FVector& Location)
{
    CurrentWarpPhase = EWarpVFXPhase::ArrivalFlash;
    WarpEffectTimer = 0.f;

    // 闪光粒子
    if (ArrivalFlashEffect)
    {
        ArrivalFlashEffect->SetWorldLocation(Location);
        ArrivalFlashEffect->SetActive(true);
        ArrivalFlashEffect->SetVariableLinearColor(TEXT("FlashColor"),
            FLinearColor(WarpVFXParams.ArrivalColor));
        ArrivalFlashEffect->SetVariableFloat(TEXT("FlashRadius"),
            WarpVFXParams.ArrivalFlashRadius);
        ArrivalFlashEffect->SetVariableFloat(TEXT("FlashDuration"),
            WarpVFXParams.ArrivalFlashDuration);
    }

    // 后处理：强闪光
    if (WarpPostProcess)
    {
        WarpPostProcess->Settings.bOverride_BloomIntensity = true;
        WarpPostProcess->Settings.BloomIntensity = 5.f;
        WarpPostProcess->Settings.bOverride_ColorSaturation = true;
        WarpPostProcess->Settings.ColorSaturation = FVector4(1.5f, 1.5f, 2.f, 1.f);
    }

    // 光源闪烁模拟冲击
    if (StarLight)
    {
        StarLight->SetIntensity(StarLight->Intensity * 1.5f);
    }

    UE_LOG(LogTemp, Log, TEXT("[VFX] Arrival flash at %s"), *Location.ToString());
}

void AStellarVisualEffects::EndWarpEffects()
{
    CurrentWarpPhase = EWarpVFXPhase::Charging; // reset
    WarpEffectTimer = 0.f;

    // 关闭所有跃迁效果
    if (WarpTunnelEffect) WarpTunnelEffect->SetActive(false);
    if (StarStreamEffect) StarStreamEffect->SetActive(false);
    if (ArrivalFlashEffect) ArrivalFlashEffect->SetActive(false);

    // 重置后处理
    if (WarpPostProcess)
    {
        WarpPostProcess->Settings.bOverride_ChromaticAberration = false;
        WarpPostProcess->Settings.bOverride_MotionBlurAmount = false;
        WarpPostProcess->Settings.bOverride_VignetteIntensity = false;
        WarpPostProcess->Settings.bOverride_BloomIntensity = false;
        WarpPostProcess->Settings.bOverride_ColorSaturation = false;
    }

    UE_LOG(LogTemp, Log, TEXT("[VFX] Warp effects ended"));
}

// ======================== 大气 ========================

void AStellarVisualEffects::ApplyAtmosphereToPlanet(AActor* Planet)
{
    if (!Planet) return;

    // 创建 SkyAtmosphere 子组件
    USkyAtmosphereComponent* PlanetSky = NewObject<USkyAtmosphereComponent>(Planet);
    if (PlanetSky)
    {
        PlanetSky->AttachToComponent(Planet->GetRootComponent(),
            FAttachmentTransformRules::KeepRelativeTransform);
        PlanetSky->SetAtmosphereHeight(PlanetAtmosphereParams.AtmosphereHeight);
        PlanetSky->SetRayleighScattering(PlanetAtmosphereParams.RayleighColor);
        PlanetSky->SetRayleighScatteringScale(PlanetAtmosphereParams.RayleighScattering);
        PlanetSky->SetMieScattering(PlanetAtmosphereParams.MieColor);
        PlanetSky->SetMieScatteringScale(PlanetAtmosphereParams.MieScattering);
        PlanetSky->RegisterComponent();
    }

    // 极光粒子
    if (PlanetAtmosphereParams.bEnableAurora)
    {
        UNiagaraComponent* Aurora = NewObject<UNiagaraComponent>(Planet);
        if (Aurora)
        {
            Aurora->AttachToComponent(Planet->GetRootComponent(),
                FAttachmentTransformRules::KeepRelativeTransform);
            Aurora->SetRelativeLocation(FVector(0, 0, PlanetAtmosphereParams.AtmosphereHeight * 0.8f));
            Aurora->SetVariableLinearColor(TEXT("AuroraColor"),
                FLinearColor(PlanetAtmosphereParams.AuroraColor));
            Aurora->SetVariableFloat(TEXT("Intensity"),
                PlanetAtmosphereParams.AuroraIntensity);
            Aurora->SetVariableFloat(TEXT("Speed"),
                PlanetAtmosphereParams.AuroraSpeed);
            Aurora->RegisterComponent();
        }
    }
}

// ======================== 初始化辅助 ========================

void AStellarVisualEffects::CreateCoronaMesh()
{
    if (!CoronaMesh) return;

    // 创建动态材质
    UMaterialInterface* BaseMat = CoronaMesh->GetMaterial(0);
    if (BaseMat)
    {
        UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMat, this);
        if (DynMat)
        {
            DynMat->SetVectorParameterValue(TEXT("Color"),
                FLinearColor(StarVisualParams.CoronaColor));
            DynMat->SetScalarParameterValue(TEXT("Opacity"),
                StarVisualParams.CoronaOpacity);
            DynMat->SetScalarParameterValue(TEXT("Intensity"), 1.f);
            CoronaMesh->SetMaterial(0, DynMat);
        }
    }

    // 半透明 + 加性混合
    CoronaMesh->SetTranslucencySortPriority(10);
}

void AStellarVisualEffects::CreateHaloMesh()
{
    if (!HaloMesh) return;

    UMaterialInterface* BaseMat = HaloMesh->GetMaterial(0);
    if (BaseMat)
    {
        UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMat, this);
        if (DynMat)
        {
            DynMat->SetVectorParameterValue(TEXT("Color"),
                FLinearColor(StarVisualParams.HaloColor));
            DynMat->SetScalarParameterValue(TEXT("Opacity"), 0.3f);
            DynMat->SetScalarParameterValue(TEXT("Falloff"),
                StarVisualParams.HaloFalloff);
            HaloMesh->SetMaterial(0, DynMat);
        }
    }

    HaloMesh->SetTranslucencySortPriority(5);
}

void AStellarVisualEffects::SetupNiagaraSystems()
{
    // 日珥粒子系统
    if (ProminenceEffect)
    {
        ProminenceEffect->SetVariableLinearColor(TEXT("BaseColor"),
            FLinearColor(StarVisualParams.CoronaColor));
        ProminenceEffect->SetVariableFloat(TEXT("SpawnRate"),
            500.f * StarVisualParams.ProminenceAmplitude);
        ProminenceEffect->SetVariableFloat(TEXT("Lifetime"), 2.f);
    }
}

void AStellarVisualEffects::SetupPostProcess()
{
    if (!WarpPostProcess) return;

    // 默认后处理设置
    WarpPostProcess->Settings.bOverride_AutoExposureBias = true;
    WarpPostProcess->Settings.AutoExposureBias = -1.f; // 太空偏暗

    // 暗角（平时轻微）
    WarpPostProcess->Settings.bOverride_VignetteIntensity = true;
    WarpPostProcess->Settings.VignetteIntensity = 0.3f;

    // 颜色分级
    WarpPostProcess->Settings.bOverride_ColorSaturation = true;
    WarpPostProcess->Settings.ColorSaturation = FVector4(1.1f, 1.1f, 1.2f, 1.f);
}

// ======================== 网络复制 ========================

void AStellarVisualEffects::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(AStellarVisualEffects, StarVisualParams);
    DOREPLIFETIME(AStellarVisualEffects, WarpVFXParams);
    DOREPLIFETIME(AStellarVisualEffects, PlanetAtmosphereParams);
}
