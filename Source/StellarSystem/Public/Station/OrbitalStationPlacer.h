// ============================================================
// 路径: Source/StellarSystem/Public/Station/OrbitalStationPlacer.h
// 作用: 行星轨道空间站定位系统（模仿星际公民）
//       在行星轨道的拉格朗日点/同步轨道/转移轨道放置空间站
// 依赖: Station/ProceduralStation.h, Core/SolarSystem.h
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OrbitalStationPlacer.generated.h"

class AProceduralStation;
class AProceduralPlanet;
class AShipPawn;

// 轨道站位类型（星际公民风格）
UENUM(BlueprintType)
enum class EOrbitalStationSlot : uint8
{
    LagrangeL1,     // L1 拉格朗日点（行星-恒星之间）
    LagrangeL2,     // L2 拉格朗日点（行星背面）
    LagrangeL3,     // L3 拉格朗日点（恒星背面）
    LagrangeL4,     // L4 拉格朗日点（前60°三角）
    LagrangeL5,     // L5 拉格朗日点（后60°三角）
    Geosynchronous, // 同步轨道（正上方固定）
    LowOrbit,       // 低轨道（货运/穿梭）
    PolarOrbit,     // 极地轨道（侦查/通信）
    TransferOrbit,  // 转移轨道（跃迁入口）
    PlanetaryGate,  // 行星门（量子旅行入口）
    MAX
};

// 单个轨道站位定义
USTRUCT(BlueprintType)
struct FOrbitalStationSlotDef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EOrbitalStationSlot SlotType = EOrbitalStationSlot::LowOrbit;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName SlotName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OrbitalRadius = 50000.f; // 距行星中心 cm

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OrbitalInclination = 0.f; // 轨道倾角 度

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OrbitalPhase = 0.f; // 初始相位 度

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OrbitalPeriod = 3600.f; // 轨道周期 秒

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bHasSpaceport = true; // 是否有太空港

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bHasShipyard = false; // 是否有造船厂

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bHasQuantumGate = false; // 是否有量子门

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxDockingPorts = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName FactionOwner = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SecurityLevel = 0.5f; // 0=无治安 ~ 1=重兵把守
};

// 行星轨道空间站管理器
UCLASS(BlueprintType)
class AOrbitalStationPlacer : public AActor
{
    GENERATED_BODY()

public:
    AOrbitalStationPlacer();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // —— 绑定行星 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbital")
    AProceduralPlanet* ParentPlanet = nullptr;

    // —— 该行星的所有轨道站位 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbital")
    TArray<FOrbitalStationSlotDef> StationSlots;

    // —— 已生成的轨道空间站 ——
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Orbital")
    TMap<EOrbitalStationSlot, AProceduralStation*> ActiveStations;

    // —— 生成所有轨道空间站 ——
    UFUNCTION(BlueprintCallable, Category = "Orbital")
    void GenerateOrbitalStations();

    // —— 获取指定站位的世界坐标（实时，含轨道运动） ——
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Orbital")
    FVector GetStationWorldLocation(EOrbitalStationSlot Slot) const;

    // —— 获取指定站位的旋转 ——
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Orbital")
    FRotator GetStationWorldRotation(EOrbitalStationSlot Slot) const;

    // —— 查询最近的空闲空间站 ——
    UFUNCTION(BlueprintCallable, Category = "Orbital")
    AProceduralStation* FindNearestStation(const FVector& FromLocation) const;

    // —— 查询有造船厂的站 ——
    UFUNCTION(BlueprintCallable, Category = "Orbital")
    AProceduralStation* FindNearestShipyard(const FVector& FromLocation) const;

    // —— 查询有量子门的站 ——
    UFUNCTION(BlueprintCallable, Category = "Orbital")
    AProceduralStation* FindNearestQuantumGate(const FVector& FromLocation) const;

    // —— 自动配置（根据行星类型智能选择站位） ——
    UFUNCTION(BlueprintCallable, Category = "Orbital")
    void AutoConfigureForPlanet(AProceduralPlanet* Planet);

    // —— 预设模板 ——
    UFUNCTION(BlueprintCallable, Category = "Orbital")
    void ApplyTemplate_LushParadise();   // 宜居星球：多太空港+商场

    UFUNCTION(BlueprintCallable, Category = "Orbital")
    void ApplyTemplate_Industrial();      // 工业星球：造船厂+货运

    UFUNCTION(BlueprintCallable, Category = "Orbital")
    void ApplyTemplate_Military();       // 军事星球：重兵+船坞

    UFUNCTION(BlueprintCallable, Category = "Orbital")
    void ApplyTemplate_MiningOutpost();  // 采矿前哨：低轨+仓储

    UFUNCTION(BlueprintCallable, Category = "Orbital")
    void ApplyTemplate_QuantumHub();     // 量子枢纽：多量子门

    // —— 轨道运动更新 ——
    void UpdateOrbitalMotion(float DeltaTime);

    // —— 网络同步 ——
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

private:
    // 根据 Slot 类型计算轨道参数
    void CalculateOrbitalParams(FOrbitalStationSlotDef& Slot, float PlanetRadius, float PlanetMass);

    // 生成单个空间站
    AProceduralStation* SpawnStationAtSlot(const FOrbitalStationSlotDef& Slot);

    // 轨道时间累加器
    float OrbitalTimeAccumulator = 0.f;

    // 上次轨道更新时间
    float LastOrbitalUpdate = 0.f;

    // 轨道更新频率（秒）
    UPROPERTY(EditAnywhere, Category = "Orbital")
    float OrbitalUpdateInterval = 0.5f; // 每0.5秒更新一次位置
};
