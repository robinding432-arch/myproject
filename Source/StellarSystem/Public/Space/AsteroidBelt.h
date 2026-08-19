// AsteroidBelt.h
// 小行星带：开普勒轨道 + ISM + 碰撞伤害 + 轨道力学
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AsteroidBelt.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class AMyCharacter;

// 单颗小行星参数
USTRUCT(BlueprintType)
struct FAsteroidParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName AsteroidID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SemiMajorAxis = 3000000.f;  // 半长轴（cm）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Eccentricity = 0.1f;      // 偏心率 0~0.9

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Inclination = 0.f;         // 轨道倾角（度）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LongitudeAscending = 0.f;   // 升交点经度

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ArgumentPeriapsis = 0.f;   // 近心点幅角

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MeanAnomaly = 0.f;         // 平近点角（当前位置）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OrbitalPeriod = 3600.f;    // 轨道周期（秒）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Size = 0.5f;              // 0~1 归一化大小

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RotationSpeed = 5.f;       // 自转速度（度/秒）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor AsteroidColor = FLinearColor(0.5f, 0.45f, 0.4f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DamageOnImpact = 10.f;     // 碰撞伤害

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ResourceValue = 5.f;       // 拆解资源值
};

// 小行星带 Actor
UCLASS()
class AAsteroidBelt : public AActor
{
    GENERATED_BODY()

public:
    AAsteroidBelt();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // —— 参数 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Belt")
    int32 BeltSeed = 42;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Belt")
    float BeltInnerRadius = 2500000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Belt")
    float BeltOuterRadius = 5000000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Belt")
    float BeltThickness = 500000.f;   // 带厚度

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Belt")
    int32 AsteroidCount = 500;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Belt")
    float BeltInclination = 5.f;     // 带整体倾角

    // —— ISM 组件 ——
    UPROPERTY(VisibleAnywhere, Category = "Belt")
    TMap<int32, UInstancedStaticMeshComponent*> AsteroidISMs;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Belt|Meshes")
    TArray<UStaticMesh*> AsteroidMeshVariants;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Belt|Meshes")
    int32 MeshVariantsCount = 5;

    // —— 碰撞 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Belt|Collision")
    bool bAsteroidsCauseDamage = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Belt|Collision")
    float CollisionDamageScale = 1.f;

    // —— 密度场（用于传感器）——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Belt|Density")
    float DensityFalloff = 2.f;      // 边缘密度衰减

    // —— 生成 ——
    UFUNCTION(BlueprintCallable)
    void GenerateBelt();

    UFUNCTION(BlueprintCallable)
    void RegenerateWithSeed(int32 NewSeed);

    // —— 查询 ——
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Belt")
    float GetDensityAtLocation(const FVector& WorldPos) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Belt")
    TArray<FAsteroidParams> GetAsteroidsInRange(const FVector& WorldPos, float Range) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Belt")
    bool IsInBelt(const FVector& WorldPos) const;

    // —— 挖矿接口 ——
    UFUNCTION(BlueprintCallable, Server, Reliable)
    void ServerMineAsteroid(FName AsteroidID, float YieldMultiplier = 1.f);

    // —— 网络 ——
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

private:
    // 所有小行星数据
    UPROPERTY(Replicated)
    TArray<FAsteroidParams> AllAsteroids;

    // 碰撞体积
    UPROPERTY(VisibleAnywhere)
    TArray<class USphereComponent*> AsteroidColliders;

    // 更新 ISM 变换
    void UpdateISMTransforms(float Dt);

    // 轨道位置计算（开普勒近似）
    FVector CalculateOrbitalPosition(const FAsteroidParams& Asteroid, float Time) const;

    // 碰撞回调
    UFUNCTION()
    void OnAsteroidHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

    // 按大小分桶的 ISM
    void DistributeToISM(const FAsteroidParams& Asteroid, int32 Index);
};
