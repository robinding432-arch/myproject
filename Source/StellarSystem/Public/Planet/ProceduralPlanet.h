// ProceduralPlanet.h
// 程序化行星：立方球 + fBm 噪声地形 + 8 种 Biome + LOD 四叉树 + 海洋接口
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralPlanet.generated.h"

class UProceduralMeshComponent;
class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UMaterialInterface;

// Biome 类型
UENUM(BlueprintType)
enum class EBiomeType : uint8
{
    Ocean, Beach, Grassland, Forest, Desert, Tundra, Snow, Rock
};

// 噪声参数
USTRUCT(BlueprintType)
struct FNoiseParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float NoiseScale = 0.0005f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Octaves = 6;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Persistence = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Lacunarity = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Seed = 42;
};

// LOD 配置
USTRUCT(BlueprintType)
struct FLODConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LodDistance0 = 500000.f;  // 近距离高模

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LodDistance1 = 1500000.f; // 中距离中模

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LodDistance2 = 5000000.f;  // 远距离低模

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 HighRes = 128;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MidRes = 64;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 LowRes = 32;
};

// 单个 Chunk（四叉树节点）
USTRUCT(BlueprintType)
struct FPlanetChunk
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere)
    int32 FaceIndex = 0;

    UPROPERTY(VisibleAnywhere)
    int32 LodLevel = 0;

    UPROPERTY(VisibleAnywhere)
    FVector2D UVOrigin = FVector2D::ZeroVector;

    UPROPERTY(VisibleAnywhere)
    FVector2D UVSize = FVector2D::UnitVector;

    UPROPERTY(VisibleAnywhere)
    bool bDirty = true;

    UPROPERTY(VisibleAnywhere)
    UProceduralMeshComponent* Mesh = nullptr;
};

UCLASS()
class AProceduralPlanet : public AActor
{
    GENERATED_BODY()

public:
    AProceduralPlanet();

    virtual void Tick(float DeltaTime) override;
    virtual void BeginPlay() override;

    // —— 核心参数（Details 面板编辑）——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Planet")
    float PlanetRadius = 100000.f;  // cm（1000m = 1km 小行星）

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Planet")
    float Amplitude = 80000.f;     // 地形起伏幅度

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Planet")
    int32 CubeFaceResolution = 64;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Planet")
    int32 RandomSeed = 42;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Noise")
    FNoiseParams NoiseParams;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|LOD")
    FLODConfig LODSettings;

    // 海洋
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Ocean")
    bool bHasOcean = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Ocean")
    float OceanThreshold = 0.3f;  // 高度 < 此值 = 海洋

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Ocean")
    UMaterialInterface* OceanMaterial = nullptr;

    // 自转
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Rotation")
    float RotationSpeed = 5.0f;  // 度/秒

    // —— 植被 ISM ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage")
    TMap<EBiomeType, UStaticMesh*> FoliageMeshes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage")
    TMap<EBiomeType, float> FoliageDensities;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage")
    int32 FoliageSeedCount = 20000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage|Streaming")
    float StreamingRadius = 300000.f;

    // —— 查询接口（其他模块调用）——
    UFUNCTION(BlueprintCallable, Category = "Planet")
    EBiomeType GetBiomeAtWorldPos(const FVector& WorldPos) const;

    UFUNCTION(BlueprintCallable, Category = "Planet")
    float GetTerrainHeightAtWorldPos(const FVector& WorldPos) const;

    UFUNCTION(BlueprintCallable, Category = "Planet")
    FVector GetSurfaceNormal(const FVector& WorldPos) const;

    UFUNCTION(BlueprintCallable, Category = "Planet")
    float GetOceanThreshold() const { return OceanThreshold; }

    // 获取当前行星旋转（供角色跟随）
    UFUNCTION(BlueprintCallable, Category = "Planet")
    FQuat GetCurrentRotation() const { return GetActorQuat(); }

    // —— 网络 ——
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

protected:
    // 6 面立方球
    UPROPERTY(VisibleAnywhere)
    TArray<UProceduralMeshComponent*> FaceMeshes;

    // 海洋 Mesh
    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* OceanMesh = nullptr;

    // 植被 ISM 组件
    UPROPERTY(VisibleAnywhere)
    TMap<EBiomeType, UInstancedStaticMeshComponent*> FoliageISMs;

    // LOD Chunk 列表
    UPROPERTY(VisibleAnywhere)
    TArray<FPlanetChunk> Chunks;

    // 已生成的实例缓存
    TArray<struct FFoliageInstance> AllFoliageInstances;
    TSet<int32> ActiveFoliageIndices;

    // 计时器
    float LODTimer = 0.f;
    float FoliageStreamTimer = 0.f;

    // —— 生成函数 ——
    void GenerateCubeSphere();
    void GenerateFace(int32 FaceIndex, const FVector& Right, const FVector& Up, const FVector& Forward);
    void GenerateOcean();
    void GenerateFoliage();
    void RebuildFoliageInstances();
    void UpdateLOD();
    void UpdateFoliageStreaming();

    // —— 噪声 ——
    float SampleTerrainHeight(const FVector& UnitDir) const;
    float FBM_3D(FVector Pos) const;
    float SimpleNoise3D(FVector P) const;
    float GetMoistureAt(const FVector& UnitDir) const;

    // 顶点色 Biome 着色
    FColor GetBiomeColor(EBiomeType Biome) const;
    EBiomeType DetermineBiome(float Height, float Latitude, float Moisture) const;

    // 初始化标记
    bool bGenerated = false;
};
