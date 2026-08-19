// StellarStar.cpp
#include "Core/StellarStar.h"
#include "Planet/ProceduralPlanet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Math/UnrealMathUtility.h"

AStellarStar::AStellarStar()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(RootComponent);

    StarMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StarMesh"));
    StarMesh->SetupAttachment(RootComponent);
    StarMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    StarLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StarLight"));
    StarLight->SetupAttachment(RootComponent);
    StarLight->SetIntensity(10000000.f);
    StarLight->SetAttenuationRadius(1e10f);
    StarLight->SetLightColor(FLinearColor(1.f, 0.95f, 0.8f));

    CoronaEffect = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("Corona"));
    CoronaEffect->SetupAttachment(RootComponent);

    // 默认球体
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(
        TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (Sphere.Succeeded())
        StarMesh->SetStaticMesh(Sphere.Object);

    StarRadius = 69600000.f; // 太阳半径 cm
}

void AStellarStar::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority() && !bInitialized)
    {
        UpdateStarAppearance();
        bInitialized = true;
    }
}

void AStellarStar::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (HasAuthority())
    {
        UpdateOrbits(DeltaTime);
    }
}

void AStellarStar::UpdateOrbits(float DeltaTime)
{
    for (FPlanetOrbit& Orbit : PlanetOrbits)
    {
        if (!Orbit.Planet) continue;

        // 角度推进
        float AngularSpeed = (Orbit.OrbitalPeriod > 0.f)
            ? (360.f / Orbit.OrbitalPeriod) : 0.f;
        Orbit.CurrentAngle += AngularSpeed * DeltaTime;
        if (Orbit.CurrentAngle > 360.f) Orbit.CurrentAngle -= 360.f;

        // 计算世界坐标
        float Rad = FMath::DegreesToRadians(Orbit.CurrentAngle);
        float X = FMath::Cos(Rad) * Orbit.SemiMajorAxis;
        float Y = FMath::Sin(Rad) * Orbit.SemiMajorAxis;
        // 倾角
        float CosI = FMath::Cos(FMath::DegreesToRadians(Orbit.OrbitalInclination));
        float SinI = FMath::Sin(FMath::DegreesToRadians(Orbit.OrbitalInclination));
        float YTilted = Y * CosI;
        float ZTilted = Y * SinI;

        FVector NewPos = GetActorLocation() + FVector(X, YTilted, ZTilted);
        Orbit.Planet->SetActorLocation(NewPos);

        // 行星自转由行星自身 Tick 处理
    }
}

void AStellarStar::UpdateStarAppearance()
{
    // 由 StarType 决定颜色/温度/半径
    switch (StarType)
    {
    case EStarType::RedDwarf:
        CachedColor = FLinearColor(1.f, 0.3f, 0.1f);
        CachedTemperature = 3000.f;
        StarRadius = 20000000.f;
        break;
    case EStarType::MainSequence:
        CachedColor = FLinearColor(1.f, 0.95f, 0.8f);
        CachedTemperature = 5778.f;
        StarRadius = 69600000.f;
        break;
    case EStarType::BlueGiant:
        CachedColor = FLinearColor(0.6f, 0.7f, 1.f);
        CachedTemperature = 20000.f;
        StarRadius = 300000000.f;
        break;
    case EStarType::RedGiant:
        CachedColor = FLinearColor(1.f, 0.4f, 0.2f);
        CachedTemperature = 4000.f;
        StarRadius = 500000000.f;
        break;
    case EStarType::WhiteDwarf:
        CachedColor = FLinearColor(0.8f, 0.8f, 1.f);
        CachedTemperature = 10000.f;
        StarRadius = 7000000.f;
        break;
    case EStarType::NeutronStar:
        CachedColor = FLinearColor(0.5f, 0.7f, 1.f);
        CachedTemperature = 600000.f;
        StarRadius = 1000000.f;
        break;
    case EStarType::BlackHole:
        CachedColor = FLinearColor(0.f, 0.f, 0.f);
        CachedTemperature = 0.f;
        StarRadius = 10000000.f;
        break;
    }

    // 应用到视觉
    StarMesh->SetWorldScale3D(FVector(StarRadius * 0.001f)); // 近似
    StarLight->SetLightColor(CachedColor);
    // 强度按温度缩放
    float Intensity = CachedTemperature * 1000.f;
    StarLight->SetIntensity(Intensity);

    // —— 驱动 StellarVisualEffects（如果同场景存在）——
    TArray<AActor*> VFXActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(),
        AStellarVisualEffects::StaticClass(), VFXActors);
    for (AActor* A : VFXActors)
    {
        AStellarVisualEffects* VFX = Cast<AStellarVisualEffects>(A);
        if (VFX)
        {
            VFX->InitializeForStar(StarType, StarRadius);
        }
    }
}

FVector AStellarStar::GetStarColor() const
{
    return FVector(CachedColor.R, CachedColor.G, CachedColor.B);
}

float AStellarStar::GetStarTemperature() const
{
    return CachedTemperature;
}

float AStellarStar::GetHabitableZoneInner() const
{
    // 简化：恒星温度越高宜居带越远
    return StarRadius * 5.f * (CachedTemperature / 5778.f);
}

float AStellarStar::GetHabitableZoneOuter() const
{
    return StarRadius * 20.f * (CachedTemperature / 5778.f);
}

float AStellarStar::GetStellarWindStrength() const
{
    // 温度越高风越强
    return CachedTemperature / 5778.f;
}

void AStellarStar::RegisterPlanet(AProceduralPlanet* Planet, float Distance, float Period, float Inclination, float StartAngle)
{
    if (!HasAuthority() || !Planet) return;

    FPlanetOrbit NewOrbit;
    NewOrbit.Planet = Planet;
    NewOrbit.SemiMajorAxis = Distance;
    NewOrbit.OrbitalPeriod = Period;
    NewOrbit.OrbitalInclination = Inclination;
    NewOrbit.InitialAngle = StartAngle;
    NewOrbit.CurrentAngle = StartAngle;
    PlanetOrbits.Add(NewOrbit);

    // 初始位置
    float Rad = FMath::DegreesToRadians(StartAngle);
    FVector Pos = GetActorLocation() + FVector(
        FMath::Cos(Rad) * Distance,
        FMath::Sin(Rad) * Distance, 0.f);
    Planet->SetActorLocation(Pos);
}

void AStellarStar::UnregisterPlanet(AProceduralPlanet* Planet)
{
    PlanetOrbits.RemoveAll([Planet](const FPlanetOrbit& O) { return O.Planet == Planet; });
}

void AStellarStar::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(AStellarStar, StarType);
    DOREPLIFETIME(AStellarStar, StarRadius);
    DOREPLIFETIME(AStellarStar, StarMass);
    DOREPLIFETIME(AStellarStar, PlanetOrbits);
}
