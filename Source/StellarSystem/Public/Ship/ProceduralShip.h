// ProceduralShip.h
// 程序化飞船生成器（4 种船型 + AI 变异 + 登船/离船）
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralShip.generated.h"

class UProceduralMeshComponent;
class UStaticMeshComponent;
class UBoxComponent;
class UFloatingPawnMovement;
class UCharacterCustomizationComponent;
class AMyCharacter;
class AShipAIController;

// 飞船类型
UENUM(BlueprintType)
enum class EShipClass : uint8
{
    Fighter,    // 战斗机
    Freighter,  // 货船
    Explorer,   // 探索船
    Capital     // 旗舰
};

// 飞船生成参数
USTRUCT(BlueprintType)
struct FShipGenParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Seed = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EShipClass ShipClass = EShipClass::Explorer;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HullLength = 1000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HullWidth = 300.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HullHeight = 200.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 EngineCount = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 WingCount = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bHasCockpit = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bHasCargoBay = false;

    // 派生
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxWarpRange = 10000000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FuelCapacity = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TopSpeed = 5000.f;
};

UCLASS()
class AProceduralShip : public AActor
{
    GENERATED_BODY()

public:
    AProceduralShip();

    // —— 生成参数（Details 面板 / 蓝图）——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Ship")
    FShipGenParams GenParams;

    // —— 组件 ——
    UPROPERTY(VisibleAnywhere)
    USceneComponent* Root;

    UPROPERTY(VisibleAnywhere)
    UProceduralMeshComponent* HullMesh;

    UPROPERTY(VisibleAnywhere)
    UProceduralMeshComponent* WingMesh;

    UPROPERTY(VisibleAnywhere)
    UProceduralMeshComponent* EngineMesh;

    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* CockpitMesh;

    UPROPERTY(VisibleAnywhere)
    UBoxComponent* InteractionVolume;

    UPROPERTY(VisibleAnywhere)
    UBoxComponent* CockpitArea;

    // —— 状态 ——
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Replicated)
    bool bPlayerInside = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Replicated)
    bool bDoorsOpen = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Replicated)
    float CurrentFuel = 100.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Replicated)
    AActor* CurrentPlanet = nullptr;

    // —— AI ——
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    AShipAIController* AIController = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    bool bAIDriven = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    float AIMinStayTime = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    float AIMaxStayTime = 60.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    float AIExplorationRadius = 50000000.f;

    // —— 生成接口 ——
    UFUNCTION(BlueprintCallable)
    void GenerateShip();

    UFUNCTION(BlueprintCallable)
    void RegenerateWithSeed(int32 NewSeed);

    // —— 登船/离船 ——
    UFUNCTION(BlueprintCallable, Server, Reliable)
    void EnterShip(APawn* Pawn);

    UFUNCTION(BlueprintCallable, Server, Reliable)
    void ExitShip();

    UFUNCTION(BlueprintCallable, BlueprintPure)
    bool IsPlayerInside() const { return bPlayerInside; }

    UFUNCTION(BlueprintCallable, BlueprintPure)
    FVector GetCockpitEntryPoint() const;

    UFUNCTION(BlueprintCallable, BlueprintPure)
    FVector GetCockpitWorldPos() const;

    // —— AI 探索循环 ——
    UFUNCTION(BlueprintCallable)
    void AI_StartExploration();

    UFUNCTION(BlueprintCallable)
    void AI_PickNextPlanet();

    UFUNCTION(BlueprintCallable)
    void AI_WarpToCurrentTarget();

    // —— 网络 ——
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

protected:
    // 生成子函数
    void GenerateHull(float L, float W, float H);
    void GenerateWings(float L, float W, float H);
    void GenerateEngines(int32 Count, float L);
    void GenerateCockpit();
    void GenerateInterior();

    // 噪声
    float ShipNoise(FVector P) const;

    // AI 状态
    float AITimeAtCurrent = 0.f;
    bool bAITraveling = false;
    AActor* AITargetPlanet = nullptr;

    // 交互回调
    UFUNCTION()
    void OnInteractionOverlap(UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor, UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
