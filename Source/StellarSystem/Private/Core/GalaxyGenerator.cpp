// GalaxyGenerator.cpp
#include "Core/GalaxyGenerator.h"
#include "Planet/ProceduralPlanet.h"
#include "Space/NebulaSystem.h"
#include "Space/AsteroidBelt.h"
#include "Math/UnrealMathUtility.h"

UGalaxyGenerator::UGalaxyGenerator()
{
    // 默认值
}

FGalaxyParams UGalaxyGenerator::GenerateGalaxy(int32 Seed)
{
    FRandomStream Rand(Seed);
    FGalaxyParams G;

    G.GalaxySeed = Seed;
    G.StarCount = FMath::Clamp(Rand.RandRange(1, 4), 1, 8);
    G.MinStarSeparation = 1000000000.f;  // 10 AU
    G.MaxStarSeparation = 10000000000.f; // 100 AU
    G.bUseTitiusBode = true;
    G.TitiusBodeOffset = 0.4f;
    G.TitiusBodeMultiplier = FMath::RandRange(1.3f, 1.8f);
    G.bGenerateNebulae = Rand.FRand() > 0.3f;
    G.NebulaCount = Rand.RandRange(1, 5);
    G.bGenerateAsteroidBelts = Rand.FRand() > 0.5f;
    G.AsteroidBeltCount = Rand.RandRange(1, 3);
    G.HabitableZoneBias = Rand.FRand();

    return G;
}

FStarParams UGalaxyGenerator::GenerateStar(int32 Seed, int32 StarIndex, const FGalaxyParams& Galaxy)
{
    FRandomStream Rand(Seed * 1000 + StarIndex);
    return GenerateStarFromSpectralClass(Seed, Rand);
}

TArray<FPlanetOrbitParams> UGalaxyGenerator::GeneratePlanetOrbits(int32 Seed, const FStarParams& Star)
{
    FRandomStream Rand(Seed);
    TArray<FPlanetOrbitParams> Orbits;

    for (int32 i = 0; i < Star.PlanetCount; ++i)
    {
        FPlanetOrbitParams P;
        P.PlanetID = FName(*FString::Printf(TEXT("Planet_%d_%d"), Seed, i));

        float AU = CalculateOrbitalRadius(i, FGalaxyParams(), Star);

        P.OrbitalRadius = AU;
        P.OrbitalPeriod = AU * FMath::Sqrt(AU) * 365.25f; // 开普勒第三定律近似
        P.OrbitalEccentricity = Rand.FRandRange(0.f, 0.2f);
        P.OrbitalInclination = Rand.FRandRange(-5.f, 5.f);
        P.AxialTilt = Rand.FRandRange(0.f, 45.f);
        P.RotationPeriod = Rand.FRandRange(6.f, 48.f);
        P.bRetrograde = Rand.FRand() < 0.05f; // 5% 逆行

        Orbits.Add(P);
    }

    return Orbits;
}

FName UGalaxyGenerator::DeterminePlanetType(const FPlanetOrbitParams& Orbit, const FStarParams& Star)
{
    FRandomStream Rand(FMath::FloorToInt(Orbit.OrbitalRadius * 1000.f));
    return PickPlanetClass(Orbit.OrbitalRadius, Star.Luminosity, Rand);
}

float UGalaxyGenerator::CalculateHabitability(const FPlanetOrbitParams& Orbit, const FStarParams& Star)
{
    // 宜居指数（0~1）
    float OptimalDist = FMath::Sqrt(Star.Luminosity); // AU
    float Dist = Orbit.OrbitalRadius;
    float Hab = 1.f - FMath::Abs(Dist - OptimalDist) / FMath::Max(OptimalDist, 0.1f);
    Hab = FMath::Clamp(Hab, 0.f, 1.f);

    // 偏心修正
    Hab *= (1.f - Orbit.OrbitalEccentricity * 0.5f);

    // 倾角修正
    Hab *= (1.f - FMath::Abs(Orbit.OrbitalInclination) / 90.f * 0.2f);

    return Hab;
}

FText UGalaxyGenerator::GenerateGalaxyDescription(int32 Seed, const FGalaxyParams& Galaxy)
{
    FString Desc;

    if (Galaxy.StarCount == 1)
        Desc = TEXT("一个孤立的恒星系统，周围环绕着尚未完全探索的行星。");
    else
        Desc = FString::Printf(TEXT("一个拥有 %d 颗恒星的多星系统，引力舞蹈塑造了复杂的轨道。"), Galaxy.StarCount);

    if (Galaxy.bGenerateNebulae)
        Desc += TEXT(" 远处可见多彩的星云，暗示着恒星摇篮或死亡遗迹。");

    if (Galaxy.bGenerateAsteroidBelts)
        Desc += TEXT(" 小行星带横亘在行星之间，是矿工和海盗 alike 的猎场。");

    return FText::FromString(Desc);
}

FLinearColor UGalaxyGenerator::TemperatureToColor(float Kelvin)
{
    // 黑体辐射近似颜色
    float T = Kelvin / 1000.f;

    float R, G, B;

    if (T <= 1.0f)      { R = 1.0f; G = 0.2f; B = 0.1f; }
    else if (T <= 2.0f) { R = 1.0f; G = 0.5f + (T-1.f)*0.3f; B = 0.1f; }
    else if (T <= 3.0f) { R = 1.0f; G = 0.8f + (T-2.f)*0.2f; B = 0.2f + (T-2.f)*0.3f; }
    else if (T <= 5.0f) { R = 1.0f; G = 1.0f; B = 0.5f + (T-3.f)*0.25f; }
    else if (T <= 7.0f) { R = 0.9f - (T-5.f)*0.1f; G = 0.95f; B = 1.0f; }
    else                    { R = 0.7f; G = 0.85f; B = 1.0f; }

    return FLinearColor(FMath::Clamp(R, 0.f, 1.f), FMath::Clamp(G, 0.f, 1.f), FMath::Clamp(B, 0.f, 1.f), 1.f);
}

float UGalaxyGenerator::AUToUnrealUnits(float AU)
{
    // 1 AU = 149,597,870,700 meters = 14,959,787,070,000 cm
    // 但 UE 用 cm，且 1 AU 太大，我们缩放
    // 约定：1 AU = 1,000,000,000 cm = 10,000 km（游戏缩放）
    return AU * 1000000000.f;
}

float UGalaxyGenerator::UnrealUnitsToAU(float UnrealCm)
{
    return UnrealCm / 1000000000.f;
}

// --- Private ---

float UGalaxyGenerator::CalculateOrbitalRadius(int32 PlanetIndex, const FGalaxyParams& Galaxy, const FStarParams& Star)
{
    if (Galaxy.bUseTitiusBode)
    {
        // Titius-Bode 律：a = 0.4 + 0.3 × 2^n AU
        float AU = Galaxy.TitiusBodeOffset + Galaxy.TitiusBodeMultiplier * FMath::Pow(2.f, (float)PlanetIndex);
        return AU;
    }
    else
    {
        // 随机分布
        return FMath::Pow(1.5f, (float)PlanetIndex) * 0.5f + FMath::FRandRange(0.f, 0.3f);
    }
}

FStarParams UGalaxyGenerator::GenerateStarFromSpectralClass(int32 Seed, FRandomStream& Rand)
{
    FStarParams Star;

    // 光谱类型概率（真实分布近似）
    float Roll = Rand.FRand();
    FString Prefix;
    float Mass, Temp, Lum;

    if (Roll < 0.76f)       // M 型红矮星（最常见）
    {
        Prefix = TEXT("M");
        Mass = Rand.FRandRange(0.1f, 0.5f);
        Temp = Rand.FRandRange(2400.f, 3700.f);
        Lum = FMath::Pow(Mass, 3.5f);
    }
    else if (Roll < 0.95f)  // K 型橙矮星
    {
        Prefix = TEXT("K");
        Mass = Rand.FRandRange(0.5f, 0.8f);
        Temp = Rand.FRandRange(3700.f, 5200.f);
        Lum = FMath::Pow(Mass, 3.5f);
    }
    else if (Roll < 0.99f)  // G 型黄矮星（太阳型）
    {
        Prefix = TEXT("G");
        Mass = Rand.FRandRange(0.8f, 1.05f);
        Temp = Rand.FRandRange(5200.f, 6000.f);
        Lum = FMath::Pow(Mass, 3.5f);
    }
    else                      // A/F 型亮星
    {
        Prefix = TEXT("F");
        Mass = Rand.FRandRange(1.05f, 1.5f);
        Temp = Rand.FRandRange(6000.f, 7500.f);
        Lum = FMath::Pow(Mass, 3.5f);
    }

    Star.StarMass = Mass;
    Star.SurfaceTemp = Temp;
    Star.Luminosity = Lum;
    Star.StarRadius = 69570000.f * FMath::Pow(Mass, 0.8f); // 粗略
    Star.HabitableZoneInner = FMath::Sqrt(Lum) * 0.95f;
    Star.HabitableZoneOuter = FMath::Sqrt(Lum) * 1.37f;
    Star.PlanetCount = FMath::Clamp(Rand.RandRange(3, 9), 3, 12);

    Star.StarColor = TemperatureToColor(Temp);
    Star.StarID = FName(*FString::Printf(TEXT("%s-%d"), *Prefix, Seed));
    Star.DisplayName = FText::FromString(Star.StarID.ToString());

    return Star;
}

FName UGalaxyGenerator::PickPlanetClass(float DistanceAU, float StarLuminosity, FRandomStream& Rand)
{
    float HabitableDist = FMath::Sqrt(StarLuminosity);

    if (DistanceAU < HabitableDist * 0.4f) return TEXT("Inferno");     // 火狱
    if (DistanceAU < HabitableDist * 0.8f) return TEXT("Rocky");       // 岩石
    if (DistanceAU < HabitableDist * 1.2f) return TEXT("Terrestrial");  // 宜居
    if (DistanceAU < HabitableDist * 2.0f) return TEXT("ColdRocky");   // 寒岩
    if (DistanceAU < HabitableDist * 4.0f) return TEXT("IceGiant");     // 冰巨
    return TEXT("GasGiant");                                            // 气巨
}
