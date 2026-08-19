// GalaxyGenerator.h
// 星系生成器：多恒星 + Titius-Bode 律 + 程序化布局
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GalaxyGenerator.generated.h"

class AProceduralPlanet;
class ANebulaSystem;
class AAsteroidBelt;

// 单颗恒星参数
USTRUCT(BlueprintType)
struct FStarParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName StarID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor StarColor = FLinearColor(1.f, 0.9f, 0.7f, 1.f); // 恒星色温

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float StarMass = 1.0f;        // 太阳质量倍数

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float StarRadius = 69570000.f; // cm（太阳半径）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Luminosity = 1.0f;      // 太阳光度倍数

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SurfaceTemp = 5778.f;    // 开尔文

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HabitableZoneInner = 0.95f; // AU

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HabitableZoneOuter = 1.37f; // AU

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 PlanetCount = 6;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bHasAsteroidBelt = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bHasNebula = false;
};

// 行星轨道参数
USTRUCT(BlueprintType)
struct FPlanetOrbitParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName PlanetID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OrbitalRadius = 1.0f;   // AU

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OrbitalPeriod = 365.25f;  // 天

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OrbitalEccentricity = 0.05f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OrbitalInclination = 0.f;  // 度

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AxialTilt = 23.5f;       // 度

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RotationPeriod = 24.f;     // 小时

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bRetrograde = false;
};

// 星系生成参数
USTRUCT(BlueprintType)
struct FGalaxyParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 GalaxySeed = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 StarCount = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MinStarSeparation = 1000000000.f; // cm（10 天文单位）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxStarSeparation = 10000000000.f; // 100 AU

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bUseTitiusBode = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TitiusBodeOffset = 0.4f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TitiusBodeMultiplier = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bGenerateNebulae = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 NebulaCount = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bGenerateAsteroidBelts = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 AsteroidBeltCount = 2;

    // 宜居性
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HabitableZoneBias = 0.5f;  // 0=冷, 1=热
};

// 星系生成器（GameMode 持有）
UCLASS(BlueprintType)
class UGalaxyGenerator : public UObject
{
    GENERATED_BODY()

public:
    UGalaxyGenerator();

    // 生成完整星系
    UFUNCTION(BlueprintCallable, Category = "Galaxy")
    static FGalaxyParams GenerateGalaxy(int32 Seed);

    // 生成单颗恒星
    UFUNCTION(BlueprintCallable, Category = "Galaxy")
    static FStarParams GenerateStar(int32 Seed, int32 StarIndex, const FGalaxyParams& Galaxy);

    // 生成行星轨道（Titius-Bode 律）
    UFUNCTION(BlueprintCallable, Category = "Galaxy")
    static TArray<FPlanetOrbitParams> GeneratePlanetOrbits(int32 Seed, const FStarParams& Star);

    // 根据轨道参数 + 恒星参数决定行星类型
    UFUNCTION(BlueprintCallable, Category = "Galaxy")
    static FName DeterminePlanetType(const FPlanetOrbitParams& Orbit, const FStarParams& Star);

    // 生成宜居性评分
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Galaxy")
    static float CalculateHabitability(const FPlanetOrbitParams& Orbit, const FStarParams& Star);

    // 生成星系描述（给星图用）
    UFUNCTION(BlueprintCallable, Category = "Galaxy")
    static FText GenerateGalaxyDescription(int32 Seed, const FGalaxyParams& Galaxy);

    // 计算恒星色温（黑体辐射近似）
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Galaxy")
    static FLinearColor TemperatureToColor(float Kelvin);

    // 将 AU 转为 UE 厘米单位
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Galaxy")
    static float AUToUnrealUnits(float AU);

    // 从 UE 厘米转 AU
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Galaxy")
    static float UnrealUnitsToAU(float UnrealCm);

private:
    static float CalculateOrbitalRadius(int32 PlanetIndex, const FGalaxyParams& Galaxy, const FStarParams& Star);
    static FStarParams GenerateStarFromSpectralClass(int32 Seed, FRandomStream& Rand);
    static FName PickPlanetClass(float DistanceAU, float StarLuminosity, FRandomStream& Rand);
};
