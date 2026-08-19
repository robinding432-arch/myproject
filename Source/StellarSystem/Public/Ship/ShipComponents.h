// ShipComponents.h
// AI 程序化生成飞船组件（引擎/武器/护盾/传感器/货舱/跃迁核心）
#pragma once

#include "CoreMinimal.h"
#include "ShipComponents.generated.h"

class UProceduralMeshComponent;

// 组件槽位
UENUM(BlueprintType)
enum class EShipComponentSlot : uint8
{
    Engine, Weapon, Shield, Sensor, Cargo,
    WarpCore, Reactor, Cooler, Armor, Utility
};

// 组件稀有度（继承 EItemRarity 语义，此处独立定义便于扩展）
UENUM(BlueprintType)
enum class EComponentRarity : uint8
{
    Standard,  // 标准
    Improved,  // 改良
    Advanced,  // 先进
    Prototype, // 原型
    Alien      // 外星科技
};

// 单个飞船组件参数
USTRUCT(BlueprintType)
struct FShipComponentParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName ComponentID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EShipComponentSlot Slot = EShipComponentSlot::Engine;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EComponentRarity Rarity = EComponentRarity::Standard;

    // —— 几何 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1"))
    float Size = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1"))
    float Complexity = 0.5f;     // 部件细节复杂度
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1"))
    float Symmetry = 0.8f;       // 对称性
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1"))
    float Angularity = 0.5f;     // 棱角度
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1"))
    float VentCount = 0.3f;      // 散热口
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1"))
    float GlowIntensity = 0.5f;  // 能量发光

    // —— 材质 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor BaseColor = FLinearColor(0.25f,0.27f,0.3f,1.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor EnergyColor = FLinearColor(0.f,0.7f,1.f,1.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1"))
    float Metallic = 0.8f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1"))
    float Roughness = 0.4f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1"))
    float Wear = 0.2f;

    // —— 性能属性 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PerformanceValue = 100.f;  // 综合性能
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PowerDraw = 50.f;          // 功率消耗
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HeatOutput = 30.f;        // 散热需求
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Mass = 100.f;             // 质量

    // —— 槽位专属属性 ——
    // Engine
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="Slot==EShipComponentSlot::Engine"))
    float Thrust = 1000.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="Slot==EShipComponentSlot::Engine"))
    float WarpCapable = 0.f; // 0=否, 1=是

    // Weapon
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="Slot==EShipComponentSlot::Weapon"))
    float Damage = 50.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="Slot==EShipComponentSlot::Weapon"))
    float FireRate = 200.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="Slot==EShipComponentSlot::Weapon"))
    float Range = 50000.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="Slot==EShipComponentSlot::Weapon"))
    float EnergyPerShot = 10.f;

    // Shield
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="Slot==EShipComponentSlot::Shield"))
    float ShieldHP = 200.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="Slot==EShipComponentSlot::Shield"))
    float RegenRate = 5.f;

    // Sensor
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="Slot==EShipComponentSlot::Sensor"))
    float ScanRange = 30000.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="Slot==EShipComponentSlot::Sensor"))
    float TargetLockSpeed = 1.f;

    // Cargo
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="Slot==EShipComponentSlot::Cargo"))
    float Capacity = 100.f;

    // WarpCore
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="Slot==EShipComponentSlot::WarpCore"))
    float WarpRange = 5000000.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="Slot==EShipComponentSlot::WarpCore"))
    float WarpChargeTime = 5.f;

    // Reactor
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="Slot==EShipComponentSlot::Reactor"))
    float PowerOutput = 200.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="Slot==EShipComponentSlot::Reactor"))
    float Efficiency = 0.8f;

    // Cooler
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="Slot==EShipComponentSlot::Cooler"))
    float CoolingRate = 50.f;

    // Armor
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="Slot==EShipComponentSlot::Armor"))
    float ArmorHP = 300.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="Slot==EShipComponentSlot::Armor"))
    float DamageReduction = 0.15f;

    // Utility
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="Slot==EShipComponentSlot::Utility"))
    FName UtilityEffect = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="Slot==EShipComponentSlot::Utility"))
    float UtilityStrength = 1.f;
};

// 飞船组件生成器
UCLASS(BlueprintType)
class UShipComponentGenerator : public UObject
{
    GENERATED_BODY()

public:
    // 生成单个组件
    UFUNCTION(BlueprintCallable, Category="Ship|Components")
    static FShipComponentParams GenerateComponent(int32 Seed, EShipComponentSlot Slot);

    // 生成一整套飞船组件（用于新飞船）
    UFUNCTION(BlueprintCallable, Category="Ship|Components")
    static TArray<FShipComponentParams> GenerateFullSet(int32 Seed, int32 EngineCount, int32 WeaponCount);

    // 变异
    UFUNCTION(BlueprintCallable, Category="Ship|Components")
    static FShipComponentParams MutateComponent(const FShipComponentParams& Base, int32 Seed, float Strength=0.15f);

    // 稀有度缩放
    UFUNCTION(BlueprintCallable, Category="Ship|Components")
    static void ApplyRarity(FShipComponentParams& P, EComponentRarity Rarity);

    // 构建组件 Mesh（挂载到指定 ProceduralMesh）
    UFUNCTION(BlueprintCallable, Category="Ship|Components")
    static void BuildComponentMesh(UProceduralMeshComponent* Target, const FShipComponentParams& P);

    // 组件兼容性检查
    UFUNCTION(BlueprintCallable, Category="Ship|Components")
    static bool IsCompatible(const FShipComponentParams& A, const FShipComponentParams& B);

    // 组件升级路径
    UFUNCTION(BlueprintCallable, Category="Ship|Components")
    static FShipComponentParams UpgradeComponent(const FShipComponentParams& Base, int32 Seed);

private:
    static void GenEngineMesh(TArray<FVector>& V,TArray<int32>& T,const FShipComponentParams& P);
    static void GenWeaponMesh(TArray<FVector>& V,TArray<int32>& T,const FShipComponentParams& P);
    static void GenShieldMesh(TArray<FVector>& V,TArray<int32>& T,const FShipComponentParams& P);
    static void GenSensorMesh(TArray<FVector>& V,TArray<int32>& T,const FShipComponentParams& P);
    static void GenCargoMesh(TArray<FVector>& V,TArray<int32>& T,const FShipComponentParams& P);
    static void GenWarpCoreMesh(TArray<FVector>& V,TArray<int32>& T,const FShipComponentParams& P);
    static void GenReactorMesh(TArray<FVector>& V,TArray<int32>& T,const FShipComponentParams& P);
    static void GenCoolerMesh(TArray<FVector>& V,TArray<int32>& T,const FShipComponentParams& P);
    static void GenArmorPlate(TArray<FVector>& V,TArray<int32>& T,const FShipComponentParams& P);
    static void GenUtilityMesh(TArray<FVector>& V,TArray<int32>& T,const FShipComponentParams& P);
    static void AddVents(TArray<FVector>& V,const FShipComponentParams& P,FRandomStream& R);
    static void AddGlowStrips(TArray<FVector>& V,const FShipComponentParams& P,FRandomStream& R);
};

// 飞船组件数据资产（可售卖/装备/存档）
UCLASS(BlueprintType)
class UShipComponentData : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FShipComponentParams Params;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 RequiredShipClass = 0; // 0=通用

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MarketPrice = 500;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ECurrencyType PriceCurrency = ECurrencyType::Credits;
};

// 飞船装配管理器（挂在 ShipPawn 上）
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UShipLoadoutComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UShipLoadoutComponent();

    // 当前装配
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
    TArray<FShipComponentParams> InstalledComponents;

    // 安装/卸载
    UFUNCTION(BlueprintCallable, Server, Reliable)
    void ServerInstallComponent(const FShipComponentParams& Comp);

    UFUNCTION(BlueprintCallable, Server, Reliable)
    void ServerRemoveComponent(EShipComponentSlot Slot, int32 Index=0);

    // 查询
    UFUNCTION(BlueprintCallable)
    float GetTotalThrust() const;
    UFUNCTION(BlueprintCallable)
    float GetTotalShieldHP() const;
    UFUNCTION(BlueprintCallable)
    float GetTotalPowerOutput() const;
    UFUNCTION(BlueprintCallable)
    float GetTotalMass() const;
    UFUNCTION(BlueprintCallable)
    float GetMaxWarpRange() const;

    // 热管理
    UFUNCTION(BlueprintCallable)
    float GetHeatGeneration(float Throttle) const;
    UFUNCTION(BlueprintCallable)
    float GetHeatDissipation() const;

    // 兼容性检查
    UFUNCTION(BlueprintCallable)
    bool CanInstall(const FShipComponentParams& Comp) const;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

private:
    UPROPERTY(Replicated)
    float CurrentTotalMass = 0.f;

    void RecalculateStats();
};
