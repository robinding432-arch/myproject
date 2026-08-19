// StellarStar.h
// 恒星：发光/引力中心/行星公转轨道/恒星风
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StellarStar.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;
class UParticleSystemComponent;
class AProceduralPlanet;
class AStellarSystemActor; // forward

// 恒星类型（决定颜色/温度/质量）
UENUM(BlueprintType)
enum class EStarType : uint8
{
    RedDwarf     UMETA(DisplayName = "Red Dwarf (M)"),
    MainSequence UMETA(DisplayName = "Sun-like (G)"),
    BlueGiant    UMETA(DisplayName = "Blue Giant (O/B)"),
    RedGiant     UMETA(DisplayName = "Red Giant"),
    WhiteDwarf   UMETA(DisplayName = "White Dwarf"),
    NeutronStar  UMETA(DisplayName = "Neutron Star"),
    BlackHole    UMETA(DisplayName = "Black Hole")
};

// 单颗行星的轨道参数
USTRUCT(BlueprintType)
struct FPlanetOrbit
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    AProceduralPlanet* Planet = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SemiMajorAxis = 1000000.f;   // 轨道半长轴 (cm)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OrbitalPeriod = 60.f;         // 公转周期 (秒, 游戏内压缩)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OrbitalInclination = 0.f;     // 轨道倾角 (度)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float InitialAngle = 0.f;           // 初始相位角 (度)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AxialTilt = 23.5f;           // 自转轴倾角

    // 运行时
    float CurrentAngle = 0.f;
};

UCLASS()
class AStellarStar : public AActor
{
    GENERATED_BODY()

public:
    AStellarStar();

    virtual void Tick(float DeltaTime) override;
    virtual void BeginPlay() override;

    // ---- Properties ----
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Star")
    EStarType StarType = EStarType::MainSequence;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Star")
    float StarRadius = 69600000.f; // 太阳半径 cm

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Star")
    float StarMass = 1.989e33f;   // 太阳质量 g (仅用于游戏内引力)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star")
    float GameGravityStrength = 500000.f; // 游戏内引力常数缩放

    // 视觉
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Star|Visual")
    UStaticMeshComponent* StarMesh = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Star|Visual")
    UPointLightComponent* StarLight = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Star|Visual")
    UParticleSystemComponent* CoronaEffect = nullptr;

    // 行星轨道
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Star|System")
    TArray<FPlanetOrbit> PlanetOrbits;

    // ---- API ----
    UFUNCTION(BlueprintCallable, Category = "Star")
    void RegisterPlanet(AProceduralPlanet* Planet, float Distance, float Period, float Inclination = 0.f, float StartAngle = 0.f);

    UFUNCTION(BlueprintCallable, Category = "Star")
    void UnregisterPlanet(AProceduralPlanet* Planet);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Star")
    FVector GetStarColor() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Star")
    float GetStarTemperature() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Star")
    float GetHabitableZoneInner() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Star")
    float GetHabitableZoneOuter() const;

    // 恒星风强度（被 SpaceWeather 读取）
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Star")
    float GetStellarWindStrength() const;

    // 网络
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

protected:
    // 公转积分
    void UpdateOrbits(float DeltaTime);

    // 恒星参数计算
    void UpdateStarAppearance();

    // 颜色缓存
    FLinearColor CachedColor = FLinearColor::White;
    float CachedTemperature = 5778.f;

    // 是否初始化
    bool bInitialized = false;
};
