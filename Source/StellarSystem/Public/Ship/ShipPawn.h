// ============================================================
// 路径: Source/StellarSystem/Public/Ship/ShipPawn.h
// 作用: 飞船 Pawn — 6DOF 驾驶/跃迁/自动驾驶
// 修改于: v7.6 (修复跃迁/索赔/配置保存)
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "ShipPawn.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UFloatingPawnMovement;
class UShipWeaponsComponent;
class UShipLoadoutComponent;
class UShipHUDComponent;
class UWarpVFXIntegration;
class AProceduralPlanet;
class AProceduralShip;

// —— 飞行模式 ——
UENUM(BlueprintType)
enum class EShipFlightMode : uint8
{
    Manual      UMETA(DisplayName = "Manual Control"),
    Autopilot   UMETA(DisplayName = "Autopilot"),
    Warping     UMETA(DisplayName = "Warp Engaged"),
    Docked      UMETA(DisplayName = "Docked/Station"),
    Dead        UMETA(DisplayName = "Destroyed")
};

// —— 跃迁状态 ——
UENUM(BlueprintType)
enum class EWarpPhase : uint8
{
    Idle        UMETA(DisplayName = "Idle"),
    Accelerating UMETA(DisplayName = "Accelerating"),
    Cruising    UMETA(DisplayName = "Warp Cruise"),
    Decelerating UMETA(DisplayName = "Decelerating"),
    Arrived     UMETA(DisplayName = "Arrived")
};

// —— 跃迁目的地类型 ——
UENUM(BlueprintType)
enum class EWarpDestType : uint8
{
    Planet       UMETA(DisplayName = "Planet (orbit)"),
    Station      UMETA(DisplayName = "Station (dock)"),
    Spaceport    UMETA(DisplayName = "Ground Spaceport"),
    Asteroid     UMETA(DisplayName = "Asteroid / POI"),
    FreeSpace    UMETA(DisplayName = "Free Space (coordinates)"),
    PlayerStructure UMETA(DisplayName = "Player-owned Building")
};

// 飞船保存配置(索赔时按此重建)
USTRUCT(BlueprintType)
struct FShipSavedConfig
{
    GENERATED_BODY()

    UPROPERTY()
    FName ShipClassID;       // 飞船型号逻辑名

    UPROPERTY()
    FName ShipDisplayName;

    UPROPERTY()
    float MaxSpeed = 5000.f;

    UPROPERTY()
    float Acceleration = 800.f;

    UPROPERTY()
    float RotationSpeed = 45.f;

    UPROPERTY()
    float WarpSpeed = 10000000.f;

    UPROPERTY()
    float WarpAcceleration = 2000000.f;

    UPROPERTY()
    float MaxWarpRange = 10000000.f;

    UPROPERTY()
    float MaxFuel = 100.f;

    UPROPERTY()
    float ShieldMax = 100.f;

    UPROPERTY()
    float HullMax = 100.f;

    // 武器配置
    UPROPERTY()
    TArray<FName> EquippedWeaponIDs;

    // 涂装/皮肤
    UPROPERTY()
    FName SkinID = NAME_None;

    // 组件耐久
    UPROPERTY()
    TMap<FName, float> ComponentHealth;

    // 飞船等级/经验
    UPROPERTY()
    int32 ShipLevel = 1;

    UPROPERTY()
    float ShipXP = 0.f;
};

UCLASS()
class AShipPawn : public APawn
{
    GENERATED_BODY()

public:
    AShipPawn();

    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* IC) override;

    // —— 组件 ——
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship|Components")
    UStaticMeshComponent* ShipMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship|Components")
    UBoxComponent* CollisionBox;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship|Components")
    UFloatingPawnMovement* Movement;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship|Components")
    UShipWeaponsComponent* WeaponsComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship|Components")
    UShipLoadoutComponent* LoadoutComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship|Components")
    UShipHUDComponent* HUDComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship|Components")
    UWarpVFXIntegration* WarpVFXComp;

    // —— 飞船参数(可复制) ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Ship|Params")
    float MaxSpeed = 5000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Ship|Params")
    float Acceleration = 800.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Ship|Params")
    float RotationSpeed = 45.f;

    // —— 跃迁参数(关键修复: WarpSpeed 改为可配置+可复制) ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Ship|Params|Warp")
    float WarpSpeed = 10000000.f; // 1千万 cm/s = 100km/s

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Ship|Params|Warp")
    float WarpAcceleration = 2000000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Ship|Params|Warp")
    float MaxWarpRange = 10000000.f;

    // 跃迁速度曲线(根据距离动态调整)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Params|Warp")
    UCurveFloat* WarpSpeedCurve = nullptr; // 距离→速度倍率

    // —— 状态 ——
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Ship|State")
    EShipFlightMode FlightMode = EShipFlightMode::Manual;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Ship|State")
    EWarpPhase WarpPhase = EWarpPhase::Idle;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Ship|State")
    float CurrentSpeed = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Ship|State")
    float CurrentFuel = 100.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Ship|State")
    float MaxFuel = 100.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Ship|State")
    float HullIntegrity = 100.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Ship|State")
    float ShieldIntegrity = 100.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Ship|State")
    float ShieldMax = 100.f;

    // —— 跃迁目标(增强: 支持多种目的地类型) ——
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship|Warp")
    AActor* WarpTarget = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Ship|Warp")
    EWarpDestType WarpDestType = EWarpDestType::Planet;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Ship|Warp")
    FVector WarpTargetLocation = FVector::ZeroVector; // 自由坐标跃迁用

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Ship|Warp")
    bool bWarpTargetIsMoving = false; // 目标是否在移动(行星/空间站公转)

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship|Warp")
    float WarpProgress = 0.f; // 0~1

    // —— 已保存配置(索赔时读取) ——
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Ship|Config")
    FShipSavedConfig SavedConfig;

    // —— 输入回调 ——
    void ThrustInput(const FInputActionValue& Value);
    void StrafeInput(const FInputActionValue& Value);
    void VerticalInput(const FInputActionValue& Value);
    void PitchInput(const FInputActionValue& Value);
    void YawInput(const FInputActionValue& Value);
    void RollInput(const FInputActionValue& Value);
    void StartWarp();
    void ToggleAutopilot();
    void ExitShip();
    void FireWeapon();
    void LockTarget();

    // —— 公共接口(增强) ——
    UFUNCTION(BlueprintCallable, Category = "Ship")
    void WarpToPlanet(AActor* TargetPlanet);

    UFUNCTION(BlueprintCallable, Category = "Ship")
    void WarpToStation(AActor* TargetStation);

    UFUNCTION(BlueprintCallable, Category = "Ship")
    void WarpToSpaceport(AActor* TargetSpaceport);

    UFUNCTION(BlueprintCallable, Category = "Ship")
    void WarpToCoordinates(const FVector& WorldCoords);

    UFUNCTION(BlueprintCallable, Category = "Ship")
    void WarpToPlayerStructure(FName StructureID);

    UFUNCTION(BlueprintCallable, Category = "Ship")
    void SetAutopilotTarget(AActor* Target);

    UFUNCTION(BlueprintCallable, Category = "Ship")
    void DockAtStation(AActor* Station);

    UFUNCTION(BlueprintCallable, Category = "Ship")
    void TakeDamage(float Amount, bool bIgnoreShields = false);

    UFUNCTION(BlueprintCallable, Category = "Ship")
    void Refuel(float Amount);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ship")
    bool CanWarpTo(AActor* Target) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ship|Warp")
    bool CanWarpToCoordinates(const FVector& Coords) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ship|Warp")
    float GetWarpDurationTo(AActor* Target) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ship|Warp")
    float GetWarpDurationToCoords(const FVector& Coords) const;

    // —— 配置保存/加载(索赔核心) ——
    UFUNCTION(BlueprintCallable, Category = "Ship|Config")
    void SaveShipConfig();

    UFUNCTION(BlueprintCallable, Category = "Ship|Config")
    void LoadShipConfig(const FShipSavedConfig& Config);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ship|Config")
    FShipSavedConfig GetCurrentConfig() const;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

protected:
    virtual void BeginPlay() override;

private:
    // —— 跃迁逻辑(增强) ——
    FVector WarpStartPos;
    FVector WarpEndPos;
    float WarpTimer = 0.f;
    float WarpDuration = 3.f;
    float CurrentWarpSpeed = 0.f; // 当前跃迁速度(加速中变化)

    // 目标追踪(目的地移动时动态调整终点)
    bool bTrackMovingTarget = false;
    float TargetUpdateInterval = 0.5f; // 每0.5秒更新一次目标位置
    float TargetUpdateTimer = 0.f;

    void UpdateWarp(float DeltaTime);
    void CompleteWarp();
    void UpdateMovingTarget();

    // —— 护盾回复 ——
    UPROPERTY(EditAnywhere, Category = "Ship|Shield")
    float ShieldRegenRate = 5.f;

    UPROPERTY(EditAnywhere, Category = "Ship|Shield")
    float ShieldRegenDelay = 3.f;

    float ShieldRegenTimer = 0.f;

    // —— 引擎尾焰特效位置 ——
    UPROPERTY(EditAnywhere, Category = "Ship|Effects")
    TArray<FVector> EngineEffectLocations;

    // —— 关联的程序化飞船(登船用) ——
    UPROPERTY()
    AProceduralShip* OwningShipGenerator = nullptr;

    // —— 自动驾驶 ——
    UPROPERTY()
    AActor* AutopilotTarget = nullptr;

    void UpdateAutopilot(float DeltaTime);

    // —— 输入值缓存 ——
    float ThrustVal = 0.f;
    float StrafeVal = 0.f;
    float VerticalVal = 0.f;
    float PitchVal = 0.f;
    float YawVal = 0.f;
    float RollVal = 0.f;

    // —— 出发距离记录(用于离开机库检测) ——
    UPROPERTY()
    FVector LastDockedLocation = FVector::ZeroVector;

    UPROPERTY()
    float DistanceFromDock = 0.f;

    UPROPERTY(EditAnywhere, Category = "Ship|Warp")
    float AutoWarpTriggerDistance = 50000.f; // 离开机库50km后允许跃迁

    void CheckAutoWarpTrigger();
};
