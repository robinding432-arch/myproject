#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "ProceduralBuildings.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;

// —— 建筑类型 ——
UENUM(BlueprintType)
enum class EBuildingType : uint8
{
    Habitation,        // 居住区
    Industrial,       // 工业区
    Research,         // 科研站
    Military,         // 军事基地
    Trade,            // 贸易站
    Farm,             // 农业穹顶
    Mining,           // 采矿设施
    Communication,    // 通信塔
    Energy,           // 能源站
    Storage           // 仓储
};

// —— 建筑实例数据 ——
USTRUCT(BlueprintType)
struct FBuildingInstance
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EBuildingType Type = EBuildingType::Habitation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector WorldPosition = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRotator Rotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Scale = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Seed = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString OwnerFaction = TEXT("Independent");

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Population = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsAbandoned = false;
};

// —— 建筑生成配置 ——
USTRUCT(BlueprintType)
struct FBuildingGenConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EBuildingType BuildingType = EBuildingType::Habitation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MinCount = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxCount = 20;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "1"))
    float Density = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MinSpacing = 2000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxHeight = 5000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bClusterAroundLandingPad = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ClusterRadius = 30000.f;
};

// —— 程序化建筑生成器 ——
UCLASS()
class STELLARSYSTEM_API AProceduralBuildings : public AActor
{
    GENERATED_BODY()

public:
    AProceduralBuildings();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // —— 生成接口 ——
    UFUNCTION(BlueprintCallable, Category = "Buildings")
    void GenerateBuildingsForPlanet(AActor* PlanetActor, int32 Seed = 0);

    UFUNCTION(BlueprintCallable, Category = "Buildings")
    void GenerateBuildingAt(const FVector& WorldPos, EBuildingType Type, int32 Seed = 0);

    // —— 查询 ——
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Buildings")
    const TArray<FBuildingInstance>& GetAllBuildings() const { return AllBuildings; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Buildings")
    FBuildingInstance GetNearestBuilding(const FVector& WorldPos) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Buildings")
    TArray<FBuildingInstance> GetBuildingsByType(EBuildingType Type) const;

    // —— 配置 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buildings|Generation")
    TArray<FBuildingGenConfig> GenerationConfigs;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buildings|Generation")
    int32 TotalBuildingBudget = 200;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buildings|Generation")
    float SurfaceOffset = 100.f; // 离地高度

    // —— 流式加载 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buildings|Streaming")
    float StreamingRadius = 50000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buildings|Streaming")
    float RebuildInterval = 3.f;

    // —— 资产覆盖 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buildings|Assets")
    bool bUseAssetRegistry = true;

    // —— 事件 ——
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBuildingSpawned, const FBuildingInstance&, Building);
    UPROPERTY(BlueprintAssignable, Category = "Buildings|Events")
    FOnBuildingSpawned OnBuildingSpawned;

protected:
    // 建筑 Mesh 组件（按类型分桶）
    UPROPERTY()
    TMap<EBuildingType, UProceduralMeshComponent*> BuildingMeshes;

    UPROPERTY()
    TMap<EBuildingType, UInstancedStaticMeshComponent*> BuildingISMs;

    // 所有建筑实例
    UPROPERTY()
    TArray<FBuildingInstance> AllBuildings;

    // 活动实例索引
    TSet<int32> ActiveIndices;

    // 生成子函数
    void GenerateBuildingMesh(EBuildingType Type, int32 InstanceIndex);
    void GenerateHabitation(const FBuildingInstance& Inst);
    void GenerateIndustrial(const FBuildingInstance& Inst);
    void GenerateResearch(const FBuildingInstance& Inst);
    void GenerateMilitary(const FBuildingInstance& Inst);
    void GenerateTrade(const FBuildingInstance& Inst);
    void GenerateFarm(const FBuildingInstance& Inst);
    void GenerateMining(const FBuildingInstance& Inst);
    void GenerateCommunication(const FBuildingInstance& Inst);
    void GenerateEnergy(const FBuildingInstance& Inst);
    void GenerateStorage(const FBuildingInstance& Inst);

    // 通用建筑工具
    void AddBoxToMesh(UProceduralMeshComponent* Mesh, const FVector& Center,
        const FVector& Size, const FColor& Color, int32& VertexOffset);
    void AddCylinderToMesh(UProceduralMeshComponent* Mesh, const FVector& Base,
        float Radius, float Height, int32 Segments, const FColor& Color, int32& VertexOffset);
    void AddPyramidToMesh(UProceduralMeshComponent* Mesh, const FVector& Base,
        float BaseSize, float Height, const FColor& Color, int32& VertexOffset);
    void AddDomeToMesh(UProceduralMeshComponent* Mesh, const FVector& Center,
        float Radius, const FColor& Color, int32& VertexOffset);

    // 建筑颜色方案
    FColor GetBuildingColor(EBuildingType Type, int32 Seed) const;

    // 流式更新
    void UpdateStreaming();
    float StreamingTimer = 0.f;

    // 【Fix 2】分帧增量生成队列
    // 避免大型聚落首次生成一次性卡主线程 50-100ms
    TArray<int32> PendingBuildingsToGenerate;
    int32 BuildingsGeneratedThisFrame = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buildings|Streaming")
    int32 MaxBuildingsPerFrame = 3; // 每帧最多生成几栋

    // 父行星引用
    UPROPERTY()
    AActor* ParentPlanet = nullptr;

    // 噪声：建筑分布
    float BuildingNoise(const FVector& Pos, int32 Octaves) const;
};
