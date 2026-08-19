#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MiningSystem.generated.h"

class AProceduralPlanet;
class UStaticMeshComponent;
class UParticleSystemComponent;

// —— 矿石类型 ——
UENUM(BlueprintType)
enum class EOreType : uint8
{
    Iron,           // 铁：最常见，基础建材
    Copper,         // 铜：导电，电子设备
    Aluminum,       // 铝：轻量合金
    Titanium,       // 钛：高强度合金
    Gold,           // 金：电子+货币
    Platinum,       // 铂：催化剂+奢侈品
    Diamond,        // 钻石：切割工具+奢侈品
    Uranium,        // 铀：核燃料
    Helium3,        // 氦3：聚变燃料（气体巨星专属）
    RareEarth,      // 稀土：高级电子
    Silicon,        // 硅：芯片
    Carbon,         // 碳：石墨烯/纳米管
    Ice,            // 冰：水+氧气来源
    Sulphur,        // 硫：化工原料
    Biomass         // 生物质：食物+药品
};

// —— 矿石数据资产 ——
USTRUCT(BlueprintType)
struct FOreData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EOreType OreType = EOreType::Iron;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString DisplayName = TEXT("Iron Ore");

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Description = TEXT("Common iron ore used in basic manufacturing.");

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BaseValue = 10.f;          // 基础单价（Credits）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Rarity = 0.5f;            // 0~1，越大越稀有

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Hardness = 1.f;           // 开采难度倍率

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MassPerUnit = 2.f;        // 每单位质量（kg）

    // 哪些 Biome 更容易出
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FName> PreferredBiomes;

    // 哪些行星类型更容易出
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FName> PreferredPlanetTypes;

    // 矿石颜色（用于程序化生成的小石块外观）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor OreColor = FLinearColor(0.6f, 0.55f, 0.5f);

    // 精炼后产物
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName RefinedProduct = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RefineYield = 0.5f;       // 精炼产出率
};

// —— 单个矿脉（行星表面的可采集点） ——
USTRUCT(BlueprintType)
struct FOreVein
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FVector WorldLocation;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    EOreType OreType = EOreType::Iron;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float RemainingAmount = 100.f;   // 剩余储量

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float TotalAmount = 100.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float Quality = 1.f;            // 品质倍率（影响售价）

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    bool bDepleted = false;

    // 程序化生成的矿石 Mesh 参数
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float Scale = 1.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FRotator Rotation;
};

// —— 精炼产物 ——
USTRUCT(BlueprintType)
struct FRefinedMaterial
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName MaterialName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BaseValue = 50.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<EOreType> RequiredOres;  // 精炼所需矿石

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<int32> RequiredAmounts;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RefineTime = 5.f;          // 精炼耗时（秒）
};

// —— 采矿光束组件（装在飞船或角色上） ——
UCLASS(BlueprintType)
class UMiningLaserComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMiningLaserComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    // 开始采矿（对目标矿脉）
    UFUNCTION(BlueprintCallable, Category = "Mining")
    void StartMining(AActor* TargetVein);

    UFUNCTION(BlueprintCallable, Category = "Mining")
    void StopMining();

    // 当前是否正在采矿
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    bool bIsMining = false;

    // 采矿速率（单位/秒）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining")
    float MiningRate = 5.f;

    // 采矿范围
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining")
    float MiningRange = 5000.f;       // 50m

    // 光束伤害（对矿脉）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining")
    float BeamDamage = 10.f;

    // 当前目标
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    AActor* CurrentTarget = nullptr;

    // 已采集的矿石（本次采矿会话）
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TMap<EOreType, float> SessionYield;

    // 采矿效率倍率（受装备/技能影响）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining")
    float EfficiencyMultiplier = 1.f;

    // 事件
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnOreExtracted, EOreType, OreType, float, Amount, float, TotalQuality);
    UPROPERTY(BlueprintAssignable, Category = "Mining")
    FOnOreExtracted OnOreExtracted;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVeinDepleted, AActor*, VeinActor);
    UPROPERTY(BlueprintAssignable, Category = "Mining")
    FOnVeinDepleted OnVeinDepleted;

private:
    float MiningTimer = 0.f;
    float ExtractInterval = 0.5f;    // 每 0.5s 结算一次
};

// —— 行星矿脉管理器（挂在 ProceduralPlanet 上） ——
UCLASS(BlueprintType)
class APlanetMiningManager : public AActor
{
    GENERATED_BODY()

public:
    APlanetMiningManager();

    virtual void BeginPlay() override;

    // 在行星表面生成矿脉
    UFUNCTION(BlueprintCallable, Category = "Mining")
    void GenerateOreVeins(int32 Seed, int32 VeinCount = 200);

    // 获取行星上所有矿脉
    UFUNCTION(BlueprintCallable, Category = "Mining")
    TArray<FOreVein> GetAllVeins() const { return OreVeins; }

    // 获取指定类型的矿脉
    UFUNCTION(BlueprintCallable, Category = "Mining")
    TArray<FOreVein> GetVeinsByType(EOreType OreType) const;

    // 采集矿脉（减少储量）
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Mining")
    void Server_ExtractOre(int32 VeinIndex, float Amount, AActor* Extractor);

    // 注册矿脉 Actor 到索引映射
    void RegisterVeinActor(AActor* VeinActor, int32 VeinIndex);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    AProceduralPlanet* OwnerPlanet = nullptr;

    // 矿石数据表（DataAsset 引用）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining")
    TArray<FOreData> OreDatabase;

    // 精炼配方
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining")
    TArray<FRefinedMaterial> RefineRecipes;

    // 行星矿脉密度倍率
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining")
    float VeinDensityMultiplier = 1.f;

    // 品质范围
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining")
    FVector2D QualityRange = FVector2D(0.5f, 1.5f);

private:
    UPROPERTY(VisibleAnywhere)
    TArray<FOreVein> OreVeins;

    // Vein Actor → Index 映射
    UPROPERTY()
    TMap<AActor*, int32> VeinActorToIndex;

    // 按 Biome 加权选矿石类型
    EOreType PickOreTypeForBiome(FName Biome, FRandomStream& Rand) const;

    // 程序化生成矿脉 Mesh
    void SpawnVeinMesh(const FOreVein& Vein, int32 Index);
};
