// PlanetLOD.h
// 异步多线程 LOD：用 AsyncTask + TaskGraph 在后台线程生成 mesh 数据
#pragma once

#include "CoreMinimal.h"
#include "PlanetLOD.generated.h"

class UProceduralMeshComponent;
class UWorld;

// LOD 等级
UENUM(BlueprintType)
enum class ELODLevel : uint8
{
    LOD0_High   UMETA(DisplayName = "LOD0 (High 128)"),
    LOD1_Medium UMETA(DisplayName = "LOD1 (Med 64)"),
    LOD2_Low    UMETA(DisplayName = "LOD2 (Low 32)"),
    LOD3_Cull   UMETA(DisplayName = "LOD3 (Cull)")
};

// 单个 Chunk 数据（可在后台填充）
USTRUCT(BlueprintType)
struct FLODChunkData
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

    // 是否正在异步生成
    UPROPERTY(VisibleAnywhere)
    bool bGenerating = false;

    // 是否已生成完毕等待应用
    UPROPERTY(VisibleAnywhere)
    bool bReadyToApply = false;

    // 网格引用
    UPROPERTY(VisibleAnywhere)
    UProceduralMeshComponent* Mesh = nullptr;

    // ---- 后台生成结果（主线程消费）----
    TArray<FVector> PendingVertices;
    TArray<int32> PendingTriangles;
    TArray<FVector> PendingNormals;
    TArray<FVector2D> PendingUVs;
    TArray<FColor> PendingColors;
    TArray<FProcMeshTangent> PendingTangents;

    // 异步任务句柄（用于取消）
    FGraphEventRef GenerationTask;
};

// LOD 配置
USTRUCT(BlueprintType)
struct FLODSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DistanceLOD0 = 500000.f;   // < 此距离用 128

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DistanceLOD1 = 1500000.f;  // < 此距离用 64

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DistanceLOD2 = 5000000.f;  // < 此距离用 32

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ResolutionLOD0 = 128;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ResolutionLOD1 = 64;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ResolutionLOD2 = 32;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxConcurrentTasks = 4;     // 最大并发后台任务数

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float UpdateInterval = 0.5f;     // 检查频率（秒）
};

// 异步 LOD 管理器（由 ProceduralPlanet 持有）
UCLASS()
class UPlanetLODManager : public UObject
{
    GENERATED_BODY()

public:
    UPlanetLODManager();

    // 初始化（传入行星引用）
    void Initialize(AActor* InPlanetOwner, const FLODSettings& InSettings);

    // 每帧调用：检查距离 + 应用已完成的任务
    void Tick(float DeltaTime, const FVector& ViewerLocation);

    // 强制全部重建（参数改变时）
    void RebuildAll();

    // 取消所有进行中的任务（退出/销毁时）
    void CancelAllTasks();

    // 设置噪声参数（用于后台采样）
    void SetNoiseParams(float InNoiseScale, int32 InOctaves, float InPersistence, float InLacunarity, int32 InSeed);

    // 设置行星参数
    void SetPlanetParams(float InRadius, float InAmplitude, float InOceanThreshold);

    // 是否正在生成
    UFUNCTION(BlueprintCallable, BlueprintPure)
    bool IsGenerating() const;

    UFUNCTION(BlueprintCallable, BlueprintPure)
    int32 GetActiveTaskCount() const;

    UFUNCTION(BlueprintCallable, BlueprintPure)
    float GetGenerationProgress() const;

private:
    // 所有者（行星）
    TWeakObjectPtr<AActor> PlanetOwner;

    // 设置
    FLODSettings Settings;
    float NoiseScale = 0.0005f;
    int32 Octaves = 6;
    float Persistence = 0.5f;
    float Lacunarity = 2.0f;
    int32 NoiseSeed = 42;
    float PlanetRadius = 100000.f;
    float Amplitude = 80000.f;
    float OceanThreshold = 0.3f;

    // 所有 Chunk（6 面 × 最多 4 级细分 = 24 个）
    UPROPERTY()
    TArray<FLODChunkData> Chunks;

    // 计时器
    float CheckTimer = 0.f;

    // 当前活跃任务数
    int32 ActiveTasks = 0;

    // ---- 核心逻辑 ----
    void CheckAndSchedule(const FVector& ViewerLocation);
    void ScheduleChunkGeneration(int32 ChunkIdx, ELODLevel TargetLOD);
    void ApplyCompletedChunks();

    // 后台生成函数（在 worker 线程执行）
    static void GenerateChunkData(
        FLODChunkData* Chunk,
        ELODLevel LOD,
        int32 Resolution,
        float PlanetRadius,
        float Amplitude,
        float NoiseScale,
        int32 Octaves,
        float Persistence,
        float Lacunarity,
        int32 Seed,
        float OceanThreshold);

    // 获取/创建 Chunk 的 Mesh 组件
    UProceduralMeshComponent* GetOrCreateChunkMesh(int32 ChunkIdx);

    // 计算某 Chunk 应有的 LOD
    ELODLevel CalculateTargetLOD(int32 ChunkIdx, const FVector& ViewerLocation) const;

    // 获取 Chunk 世界中心
    FVector GetChunkWorldCenter(int32 ChunkIdx) const;
};
