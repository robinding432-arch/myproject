// SolarSystem.h
// 太阳系生成器：1 颗恒星 + 8 颗行星（模仿太阳系 8 大行星）
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SolarSystem.generated.h"

class AStellarStar;
class AProceduralPlanet;
class UWorld;

// 单颗行星生成参数
USTRUCT(BlueprintType)
struct FSolarPlanetParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString PlanetName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DistanceFromStar = 0.f;   // cm

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PlanetRadius = 100000.f;   // cm

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OrbitalPeriod = 60.f;      // 游戏内秒

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RotationSpeed = 5.f;       // 度/秒

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float InitialAngle = 0.f;         // 度

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OrbitalInclination = 0.f;  // 度

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 BiomeSeed = 42;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bHasOcean = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OceanThreshold = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Amplitude = 80000.f;       // 地形起伏
};

UCLASS()
class ASolarSystem : public AActor
{
    GENERATED_BODY()

public:
    ASolarSystem();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // ---- 中心恒星 ----
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SolarSystem")
    TSubclassOf<AStellarStar> StarClass;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SolarSystem")
    AStellarStar* CentralStar = nullptr;

    // ---- 行星列表 ----
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SolarSystem")
    TSubclassOf<AProceduralPlanet> PlanetClass;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SolarSystem")
    TArray<AProceduralPlanet*> Planets;

    // ---- 8 颗行星参数（编辑器里可改）----
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SolarSystem|Planets")
    FSolarPlanetParams MercuryParams;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SolarSystem|Planets")
    FSolarPlanetParams VenusParams;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SolarSystem|Planets")
    FSolarPlanetParams EarthParams;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SolarSystem|Planets")
    FSolarPlanetParams MarsParams;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SolarSystem|Planets")
    FSolarPlanetParams JupiterParams;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SolarSystem|Planets")
    FSolarPlanetParams SaturnParams;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SolarSystem|Planets")
    FSolarPlanetParams UranusParams;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SolarSystem|Planets")
    FSolarPlanetParams NeptuneParams;

    // ---- API ----
    UFUNCTION(BlueprintCallable, Category = "SolarSystem")
    void GenerateSolarSystem();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SolarSystem")
    AProceduralPlanet* GetPlanetByName(const FString& Name) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SolarSystem")
    AProceduralPlanet* GetPlanetByIndex(int32 Index) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SolarSystem")
    int32 GetPlanetCount() const { return Planets.Num(); }

protected:
    // 默认太阳系参数（真实比例压缩）
    void InitDefaultParams();

    // 创建单颗行星
    AProceduralPlanet* SpawnPlanet(const FSolarPlanetParams& Params);

    // 是否初始化
    bool bGenerated = false;
};
