// SolarSystem.cpp
// 模仿太阳系 8 大行星的程序化生成
#include "Core/SolarSystem.h"
#include "Core/StellarStar.h"
#include "Planet/ProceduralPlanet.h"
#include "Engine/World.h"
#include "Math/UnrealMathUtility.h"

ASolarSystem::ASolarSystem()
{
    PrimaryActorTick.bCanEverTick = true;
    bGenerated = false;

    // 默认参数（真实比例压缩到游戏尺度）
    InitDefaultParams();
}

void ASolarSystem::InitDefaultParams()
{
    // 距离单位：cm，压缩比例约 1AU = 1e9 cm (1万公里)
    MercuryParams.PlanetName = TEXT("Mercury");
    MercuryParams.DistanceFromStar = 57900000000.f; // 0.39 AU
    MercuryParams.PlanetRadius = 2440000.f;          // 真实半径 cm
    MercuryParams.OrbitalPeriod = 88.f * 2.f;       // 压缩：88天 -> 176秒
    MercuryParams.RotationSpeed = 0.5f;              // 慢自转
    MercuryParams.InitialAngle = 0.f;
    MercuryParams.OrbitalInclination = 7.f;
    MercuryParams.BiomeSeed = 11;
    MercuryParams.bHasOcean = false;                   // 无海洋
    MercuryParams.OceanThreshold = 0.1f;
    MercuryParams.Amplitude = 50000.f;                // 多山

    VenusParams.PlanetName = TEXT("Venus");
    VenusParams.DistanceFromStar = 108200000000.f;   // 0.72 AU
    VenusParams.PlanetRadius = 6052000.f;
    VenusParams.OrbitalPeriod = 225.f * 2.f;
    VenusParams.RotationSpeed = -0.2f;               // 逆向自转
    VenusParams.InitialAngle = 45.f;
    VenusParams.OrbitalInclination = 3.4f;
    VenusParams.BiomeSeed = 22;
    VenusParams.bHasOcean = false;                    // 实际有但太热
    VenusParams.OceanThreshold = 0.2f;
    VenusParams.Amplitude = 30000.f;

    EarthParams.PlanetName = TEXT("Earth");
    EarthParams.DistanceFromStar = 149600000000.f;   // 1.0 AU
    EarthParams.PlanetRadius = 6371000.f;
    EarthParams.OrbitalPeriod = 365.f * 2.f;
    EarthParams.RotationSpeed = 15.f;                 // 24h -> 加速
    EarthParams.InitialAngle = 90.f;
    EarthParams.OrbitalInclination = 0.f;
    EarthParams.BiomeSeed = 42;
    EarthParams.bHasOcean = true;
    EarthParams.OceanThreshold = 0.3f;
    EarthParams.Amplitude = 80000.f;

    MarsParams.PlanetName = TEXT("Mars");
    MarsParams.DistanceFromStar = 227900000000.f;    // 1.52 AU
    MarsParams.PlanetRadius = 3390000.f;
    MarsParams.OrbitalPeriod = 687.f * 2.f;
    MarsParams.RotationSpeed = 14.f;
    MarsParams.InitialAngle = 135.f;
    MarsParams.OrbitalInclination = 1.8f;
    MarsParams.BiomeSeed = 56;
    MarsParams.bHasOcean = false;                     // 极冠水冰
    MarsParams.OceanThreshold = 0.15f;
    MarsParams.Amplitude = 60000.f;

    JupiterParams.PlanetName = TEXT("Jupiter");
    JupiterParams.DistanceFromStar = 778600000000.f;  // 5.2 AU
    JupiterParams.PlanetRadius = 69911000.f;           // 巨行星
    JupiterParams.OrbitalPeriod = 4333.f * 0.5f;     // 压缩
    JupiterParams.RotationSpeed = 80.f;               // 快速自转
    JupiterParams.InitialAngle = 180.f;
    JupiterParams.OrbitalInclination = 1.3f;
    JupiterParams.BiomeSeed = 78;
    JupiterParams.bHasOcean = true;                    // 气体巨星用特殊 ocean
    JupiterParams.OceanThreshold = 0.4f;
    JupiterParams.Amplitude = 200000.f;                // 大气带

    SaturnParams.PlanetName = TEXT("Saturn");
    SaturnParams.DistanceFromStar = 1433500000000.f;  // 9.5 AU
    SaturnParams.PlanetRadius = 58232000.f;
    SaturnParams.OrbitalPeriod = 10759.f * 0.3f;
    SaturnParams.RotationSpeed = 70.f;
    SaturnParams.InitialAngle = 225.f;
    SaturnParams.OrbitalInclination = 2.5f;
    SaturnParams.BiomeSeed = 91;
    SaturnParams.bHasOcean = true;
    SaturnParams.OceanThreshold = 0.4f;
    SaturnParams.Amplitude = 180000.f;

    UranusParams.PlanetName = TEXT("Uranus");
    UranusParams.DistanceFromStar = 2872500000000.f;  // 19.2 AU
    UranusParams.PlanetRadius = 25362000.f;
    UranusParams.OrbitalPeriod = 30687.f * 0.15f;
    UranusParams.RotationSpeed = 40.f;
    UranusParams.InitialAngle = 270.f;
    UranusParams.OrbitalInclination = 0.8f;
    UranusParams.BiomeSeed = 103;
    UranusParams.bHasOcean = true;
    UranusParams.OceanThreshold = 0.4f;
    UranusParams.Amplitude = 120000.f;

    NeptuneParams.PlanetName = TEXT("Neptune");
    NeptuneParams.DistanceFromStar = 4495100000000.f;  // 30 AU
    NeptuneParams.PlanetRadius = 24622000.f;
    NeptuneParams.OrbitalPeriod = 60190.f * 0.1f;
    NeptuneParams.RotationSpeed = 45.f;
    NeptuneParams.InitialAngle = 315.f;
    NeptuneParams.OrbitalInclination = 1.8f;
    NeptuneParams.BiomeSeed = 117;
    NeptuneParams.bHasOcean = true;
    NeptuneParams.OceanThreshold = 0.4f;
    NeptuneParams.Amplitude = 130000.f;
}

void ASolarSystem::BeginPlay()
{
    Super::BeginPlay();
    if (!bGenerated)
    {
        GenerateSolarSystem();
        bGenerated = true;
    }
}

void ASolarSystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    // 恒星自转（可选）
}

void ASolarSystem::GenerateSolarSystem()
{
    UWorld* World = GetWorld();
    if (!World) return;

    // 1. 生成恒星
    if (StarClass)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        CentralStar = World->SpawnActor<AStellarStar>(StarClass, GetActorLocation(), FRotator::ZeroRotator, SpawnParams);
        if (CentralStar)
        {
            CentralStar->StarType = EStarType::MainSequence;
            CentralStar->UpdateStarAppearance();
        }
    }

    // 2. 生成 8 颗行星
    Planets.Empty();
    Planets.Add(SpawnPlanet(MercuryParams));
    Planets.Add(SpawnPlanet(VenusParams));
    Planets.Add(SpawnPlanet(EarthParams));
    Planets.Add(SpawnPlanet(MarsParams));
    Planets.Add(SpawnPlanet(JupiterParams));
    Planets.Add(SpawnPlanet(SaturnParams));
    Planets.Add(SpawnPlanet(UranusParams));
    Planets.Add(SpawnPlanet(NeptuneParams));

    // 3. 注册到恒星轨道
    if (CentralStar)
    {
        for (AProceduralPlanet* P : Planets)
        {
            if (!P) continue;
            int32 Idx = Planets.IndexOfByKey(P);
            float Dist = 0.f;
            float Period = 60.f;
            float Inc = 0.f;
            float Ang = 0.f;
            switch (Idx)
            {
            case 0: Dist = MercuryParams.DistanceFromStar; Period = MercuryParams.OrbitalPeriod; Inc = MercuryParams.OrbitalInclination; Ang = MercuryParams.InitialAngle; break;
            case 1: Dist = VenusParams.DistanceFromStar;   Period = VenusParams.OrbitalPeriod;   Inc = VenusParams.OrbitalInclination;   Ang = VenusParams.InitialAngle;   break;
            case 2: Dist = EarthParams.DistanceFromStar;   Period = EarthParams.OrbitalPeriod;   Inc = EarthParams.OrbitalInclination;   Ang = EarthParams.InitialAngle;   break;
            case 3: Dist = MarsParams.DistanceFromStar;   Period = MarsParams.OrbitalPeriod;   Inc = MarsParams.OrbitalInclination;   Ang = MarsParams.InitialAngle;   break;
            case 4: Dist = JupiterParams.DistanceFromStar; Period = JupiterParams.OrbitalPeriod; Inc = JupiterParams.OrbitalInclination; Ang = JupiterParams.InitialAngle; break;
            case 5: Dist = SaturnParams.DistanceFromStar;  Period = SaturnParams.OrbitalPeriod; Inc = SaturnParams.OrbitalInclination;  Ang = SaturnParams.InitialAngle;  break;
            case 6: Dist = UranusParams.DistanceFromStar; Period = UranusParams.OrbitalPeriod; Inc = UranusParams.OrbitalInclination; Ang = UranusParams.InitialAngle; break;
            case 7: Dist = NeptuneParams.DistanceFromStar; Period = NeptuneParams.OrbitalPeriod; Inc = NeptuneParams.OrbitalInclination; Ang = NeptuneParams.InitialAngle; break;
            }
            CentralStar->RegisterPlanet(P, Dist, Period, Inc, Ang);
        }
    }
}

AProceduralPlanet* ASolarSystem::SpawnPlanet(const FSolarPlanetParams& Params)
{
    UWorld* World = GetWorld();
    if (!World) return nullptr;

    FVector SpawnLoc = GetActorLocation() + FVector(Params.DistanceFromStar, 0.f, 0.f);

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AProceduralPlanet* Planet = nullptr;
    if (PlanetClass)
    {
        Planet = World->SpawnActor<AProceduralPlanet>(PlanetClass, SpawnLoc, FRotator::ZeroRotator, SpawnParams);
    }
    else
    {
        Planet = World->SpawnActor<AProceduralPlanet>(AProceduralPlanet::StaticClass(), SpawnLoc, FRotator::ZeroRotator, SpawnParams);
    }

    if (Planet)
    {
        Planet->PlanetRadius = Params.PlanetRadius;
        Planet->Amplitude = Params.Amplitude;
        Planet->RandomSeed = Params.BiomeSeed;
        Planet->RotationSpeed = Params.RotationSpeed;
        Planet->bHasOcean = Params.bHasOcean;
        Planet->OceanThreshold = Params.OceanThreshold;
        Planet->SetActorLabel(Params.PlanetName);
    }

    return Planet;
}

AProceduralPlanet* ASolarSystem::GetPlanetByName(const FString& Name) const
{
    for (AProceduralPlanet* P : Planets)
    {
        if (P && P->GetActorLabel() == Name) return P;
    }
    return nullptr;
}

AProceduralPlanet* ASolarSystem::GetPlanetByIndex(int32 Index) const
{
    if (Planets.IsValidIndex(Index)) return Planets[Index];
    return nullptr;
}
