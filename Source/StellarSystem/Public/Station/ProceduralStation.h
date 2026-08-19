// ProceduralStation.h
// 程序化空间站：环形/球形/柱状/集群 4 种 + 服务区
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralStation.generated.h"

class UProceduralMeshComponent;
class UBoxComponent;
class AMyCharacter;

// 空间站类型
UENUM(BlueprintType)
enum class EStationType : uint8
{
    Ring,       // 环形（大型空间站）
    Spherical,  // 球形（小型前哨）
    Cylindrical,// 柱状（工业站）
    Cluster     // 集群（多模块拼接）
};

// 空间站模块
USTRUCT(BlueprintType)
struct FStationModule
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName ModuleID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector RelativePos = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRotator RelativeRot = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Size = 0.5f; // 0~1

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bHasDock = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bHasShop = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bHasRepair = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bHasFuel = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor ModuleColor = FLinearColor(0.6f, 0.6f, 0.65f, 1.f);
};

// 服务区类型
UENUM(BlueprintType)
enum class EStationService : uint8
{
    None, Shop, Repair, Refuel, Medical, Mission, Storage, Upgrade
};

UCLASS()
class AProceduralStation : public AActor
{
    GENERATED_BODY()

public:
    AProceduralStation();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // —— 参数 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Station")
    EStationType StationType = EStationType::Ring;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Station")
    int32 StationSeed = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Station")
    float StationRadius = 5000.f; // cm

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Station")
    int32 ModuleCount = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Station")
    FName FactionID = NAME_None;

    // —— Mesh 组件 ——
    UPROPERTY(VisibleAnywhere)
    TArray<UProceduralMeshComponent*> ModuleMeshes;

    UPROPERTY(VisibleAnywhere)
    UProceduralMeshComponent* CoreMesh;

    // —— 服务区 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Services")
    TMap<EStationService, UBoxComponent*> ServiceVolumes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Services")
    TArray<EStationService> AvailableServices;

    // —— 生成 ——
    UFUNCTION(BlueprintCallable)
    void GenerateStation();

    UFUNCTION(BlueprintCallable)
    void RegenerateWithSeed(int32 NewSeed);

    // —— 交互 ——
    UFUNCTION(BlueprintCallable)
    void EnterStation(APawn* Pawn);

    UFUNCTION(BlueprintCallable)
    void ExitStation(APawn* Pawn);

    UFUNCTION(BlueprintCallable)
    bool HasService(EStationService Service) const;

    UFUNCTION(BlueprintCallable)
    FVector GetDockPosition(int32 DockIndex = 0) const;

    // —— 网络 ——
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

private:
    // 生成子函数
    void GenerateRingStation();
    void GenerateSphericalStation();
    void GenerateCylindricalStation();
    void GenerateClusterStation();
    void BuildModuleMesh(UProceduralMeshComponent* Mesh, const FStationModule& Mod);
    void PlaceServiceVolumes();
    void GenerateInterior();

    // 模块列表
    UPROPERTY()
    TArray<FStationModule> Modules;

    // 停靠位
    UPROPERTY()
    TArray<FVector> DockPositions;

    // 旋转（缓慢自转）
    UPROPERTY(EditAnywhere, Category = "Station")
    float RotationRate = 2.f; // 度/秒
};
