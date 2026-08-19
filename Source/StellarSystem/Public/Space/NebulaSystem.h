// NebulaSystem.h
// 星云系统：5 种类型 + 密度场 + 脉动 + 粒子
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NebulaSystem.generated.h"

class UNiagaraComponent;
class UStaticMeshComponent;

// 星云类型
UENUM(BlueprintType)
enum class ENebulaType : uint8
{
    Emission,   // 发射星云（粉/红）
    Reflection,  // 反射星云（蓝）
    DarkNebula,  // 暗星云（遮挡背景）
    Planetary,   // 行星状星云（彩色壳）
    Supernova    // 超新星遗迹（不规则）
};

// 星云参数
USTRUCT(BlueprintType)
struct FNebulaParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ENebulaType Type = ENebulaType::Emission;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor PrimaryColor = FLinearColor(0.8f, 0.2f, 0.5f, 0.6f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor SecondaryColor = FLinearColor(0.2f, 0.4f, 0.9f, 0.4f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Radius = 1000000.f;  // cm（10km）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Density = 0.5f;       // 0~1

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Turbulence = 0.5f;    // 湍流程度

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PulseSpeed = 0.1f;     // 脉动速度

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PulseAmplitude = 0.2f;  // 脉动幅度

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Seed = 42;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ParticleCount = 5000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ParticleSize = 500.f;   // cm

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RotationSpeed = 1.0f;   // 度/秒

    // 物理效果
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RadiationLevel = 0.1f;  // 西弗/秒（穿越星云时）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SensorInterference = 0.3f; // 传感器干扰 0~1
};

UCLASS()
class ANebulaSystem : public AActor
{
    GENERATED_BODY()

public:
    ANebulaSystem();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // —— 参数 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Nebula")
    FNebulaParams NebulaParams;

    // —— 粒子组件 ——
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UNiagaraComponent* NebulaEffect = nullptr;

    // —— 密度场查询（飞船穿越时调用）——
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Nebula")
    float GetDensityAtLocation(const FVector& WorldPos) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Nebula")
    float GetRadiationAtLocation(const FVector& WorldPos) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Nebula")
    float GetSensorInterferenceAt(const FVector& WorldPos) const;

    // —— 生成 ——
    UFUNCTION(BlueprintCallable)
    void GenerateNebula();

    UFUNCTION(BlueprintCallable)
    void RegenerateWithSeed(int32 NewSeed);

    // —— 预设 ——
    UFUNCTION(BlueprintCallable, Category = "Nebula|Presets")
    static FNebulaParams MakeEmissionNebula(int32 Seed);

    UFUNCTION(BlueprintCallable, Category = "Nebula|Presets")
    static FNebulaParams MakeReflectionNebula(int32 Seed);

    UFUNCTION(BlueprintCallable, Category = "Nebula|Presets")
    static FNebulaParams MakeDarkNebula(int32 Seed);

    UFUNCTION(BlueprintCallable, Category = "Nebula|Presets")
    static FNebulaParams MakePlanetaryNebula(int32 Seed);

    UFUNCTION(BlueprintCallable, Category = "Nebula|Presets")
    static FNebulaParams MakeSupernovaRemnant(int32 Seed);

    // —— 体积碰撞（触发辐射/干扰）——
    UPROPERTY(VisibleAnywhere)
    class USphereComponent* InfluenceVolume;

    UFUNCTION()
    void OnActorEnteredVolume(UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor, UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnActorExitedVolume(UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

private:
    // 密度场（3D 噪声采样）
    float SampleDensityField(FVector LocalPos) const;
    float SampleNoise3D(FVector P) const;

    // 粒子更新
    void UpdateParticleColors(float Dt);
    void UpdatePulsation(float Dt);

    float CurrentPulsePhase = 0.f;
    float TimeAccumulator = 0.f;

    // 缓存
    TArray<FVector> ParticleOffsets;
    TArray<float> ParticleSizes;
    TArray<FLinearColor> ParticleColors;
};
