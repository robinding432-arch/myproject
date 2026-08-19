// ============================================================
// 路径: Source/StellarSystem/Public/Ship/ShipLoadout.h
// 作用: 飞船组件系统 — 10 槽位 × 5 稀有度 × 热平衡
// 依赖: 无
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShipLoadout.generated.h"

class AShipPawn;

// 组件槽位
UENUM(BlueprintType)
enum class EShipComponentSlot : uint8
{
    Engine      UMETA(DisplayName = "Engine"),
    Weapon      UMETA(DisplayName = "Weapon Slot"),
    Shield      UMETA(DisplayName = "Shield Generator"),
    Sensor      UMETA(DisplayName = "Sensor Array"),
    Cargo       UMETA(DisplayName = "Cargo Hold"),
    WarpCore    UMETA(DisplayName = "Warp Core"),
    Reactor     UMETA(DisplayName = "Reactor"),
    Cooler      UMETA(DisplayName = "Cooling System"),
    Armor       UMETA(DisplayName = "Armor Plating"),
    Utility     UMETA(DisplayName = "Utility Slot")
};

// 稀有度
UENUM(BlueprintType)
enum class EComponentRarity : uint8
{
    Standard    UMETA(DisplayName = "Standard (White)"),
    Improved    UMETA(DisplayName = "Improved (Green)"),
    Advanced    UMETA(DisplayName = "Advanced (Blue)"),
    Prototype   UMETA(DisplayName = "Prototype (Purple)"),
    Alien       UMETA(DisplayName = "Alien (Orange)")
};

// 组件数据
USTRUCT(BlueprintType)
struct FShipComponentData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName ComponentID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EShipComponentSlot Slot = EShipComponentSlot::Engine;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EComponentRarity Rarity = EComponentRarity::Standard;

    // 性能参数
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Thrust = 100.f;          // 引擎推力

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ShieldStrength = 50.f;    // 护盾强度

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ShieldRegen = 5.f;       // 护盾回复

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SensorRange = 50000.f;    // 传感器范围

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CargoCapacity = 100.f;    // 货舱容量

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float WarpRange = 5000000.f;   // 跃迁范围

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PowerOutput = 100.f;     // 能量输出

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HeatDissipation = 50.f;  // 散热

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ArmorValue = 50.f;       // 装甲值

    // 热/功率平衡
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PowerConsumption = 10.f;  // 能耗

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HeatGeneration = 5.f;     // 发热

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Mass = 100.f;            // 质量

    // 过载
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bCanOverload = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OverloadMultiplier = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OverloadHeatCost = 20.f;

    // 兼容性标签
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGameplayTagContainer CompatibilityTags;
};

// 飞船总负载状态
USTRUCT(BlueprintType)
struct FShipLoadoutStatus
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float TotalThrust = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float TotalShield = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float TotalPowerOutput = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float TotalPowerConsumed = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float TotalHeatGen = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float TotalHeatDissipation = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float TotalMass = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float PowerBalance = 0.f; // >0 盈余, <0 赤字

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float HeatBalance = 0.f; // >0 过热风险

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    bool bOverloaded = false;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UShipLoadoutComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UShipLoadoutComponent();

    // —— 已安装组件（按槽位） ——
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Loadout")
    TMap<EShipComponentSlot, FShipComponentData> InstalledComponents;

    // —— 库存组件 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Inventory")
    TArray<FShipComponentData> ComponentInventory;

    // —— 状态 ——
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Loadout|Status")
    FShipLoadoutStatus CurrentStatus;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Loadout|Status")
    float OverloadTimer = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Status")
    float MaxSafeOverloadTime = 10.f;

    // ========== 安装/卸载 ==========
    UFUNCTION(BlueprintCallable, Category = "Loadout")
    bool InstallComponent(const FShipComponentData& Component);

    UFUNCTION(BlueprintCallable, Category = "Loadout")
    bool UninstallComponent(EShipComponentSlot Slot);

    UFUNCTION(BlueprintCallable, Category = "Loadout")
    bool SwapComponent(EShipComponentSlot Slot, const FShipComponentData& NewComponent);

    UFUNCTION(BlueprintCallable, Category = "Loadout")
    bool UpgradeComponent(EShipComponentSlot Slot, EComponentRarity NewRarity);

    // ========== 查询 ==========
    UFUNCTION(BlueprintCallable, Category = "Loadout")
    FShipComponentData GetComponent(EShipComponentSlot Slot) const;

    UFUNCTION(BlueprintCallable, Category = "Loadout")
    bool HasComponent(EShipComponentSlot Slot) const;

    UFUNCTION(BlueprintCallable, Category = "Loadout")
    float GetEffectiveTopSpeed() const;

    UFUNCTION(BlueprintCallable, Category = "Loadout")
    float GetEffectiveWarpRange() const;

    UFUNCTION(BlueprintCallable, Category = "Loadout")
    float GetHeatPercentage() const;

    UFUNCTION(BlueprintCallable, Category = "Loadout")
    float GetPowerPercentage() const;

    // ========== AI 生成 ==========
    UFUNCTION(BlueprintCallable, Category = "Loadout|Generate")
    FShipComponentData GenerateRandomComponent(EShipComponentSlot Slot, EComponentRarity Rarity, int32 Seed);

    UFUNCTION(BlueprintCallable, Category = "Loadout|Generate")
    void GenerateFullLoadout(int32 Seed, EComponentRarity MinRarity, EComponentRarity MaxRarity);

    UFUNCTION(BlueprintCallable, Category = "Loadout|Generate")
    FString GetComponentDescription(const FShipComponentData& Comp) const;

    // ========== Tick ==========
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    // —— 事件 ——
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnComponentInstalled, EShipComponentSlot, Slot);
    UPROPERTY(BlueprintAssignable, Category = "Loadout|Events")
    FOnComponentInstalled OnComponentInstalled;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOverloadChanged, bool, bIsOverloaded);
    UPROPERTY(BlueprintAssignable, Category = "Loadout|Events")
    FOnOverloadChanged OnOverloadChanged;

private:
    void RecalculateStatus();
    float GetRarityMultiplier(EComponentRarity Rarity) const;
    FString GetRarityColor(EComponentRarity Rarity) const;
    bool CheckCompatibility(const FShipComponentData& Component) const;

    UPROPERTY()
    AShipPawn* OwnerShip = nullptr;
};
