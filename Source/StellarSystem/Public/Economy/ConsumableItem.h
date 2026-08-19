// ============================================================
// 路径: Source/StellarSystem/Public/Economy/ConsumableItem.h
// 作用: 消耗品系统 — 20+ 种 / Buff / 快捷栏
// 依赖: 无
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ConsumableItem.generated.h"

// 消耗品类别
UENUM(BlueprintType)
enum class EConsumableCategory : uint8
{
    Medical    UMETA(DisplayName = "Medical (Heal/Drug)"),
    Food       UMETA(DisplayName = "Food"),
    Drink      UMETA(DisplayName = "Drink"),
    Oxygen     UMETA(DisplayName = "Oxygen Tank"),
    Energy     UMETA(DisplayName = "Energy Cell"),
    Tool       UMETA(DisplayName = "Tool/Utility"),
    Special    UMETA(DisplayName = "Special/Quest"),
    Signal     UMETA(DisplayName = "Signal Flare/Beacon")
};

// Buff/Debuff 类型
UENUM(BlueprintType)
enum class EBuffType : uint8
{
    Speed      UMETA(DisplayName = "Movement Speed"),
    Damage     UMETA(DisplayName = "Damage Boost"),
    Defense    UMETA(DisplayName = "Defense Boost"),
    Stealth    UMETA(DisplayName = "Stealth"),
    Regen      UMETA(DisplayName = "Health Regen"),
    O2Regen    UMETA(DisplayName = "Oxygen Regen"),
    EnergyRegen UMETA(DisplayName = "Energy Regen"),
    RadProt    UMETA(DisplayName = "Radiation Protection"),
    ToxinClean UMETA(DisplayName = "Toxin Cleanse"),
    Healing    UMETA(DisplayName = "Instant Heal")
};

// Buff 实例
USTRUCT(BlueprintType)
struct FBuffInstance
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EBuffType BuffType = EBuffType::Healing;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Magnitude = 1.0f; // 效果强度

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Duration = 30.f; // 秒

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RemainingTime = 30.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsDebuff = false;

    // Tick 效果
    void Tick(float DeltaTime, class UVitalsComponent* Vitals);
};

// 消耗品数据资产
USTRUCT(BlueprintType)
struct FConsumableData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName ItemID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EConsumableCategory Category = EConsumableCategory::Medical;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 QualityLevel = 1; // 1~5

    // 即时效果
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float InstantHeal = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float InstantOxygen = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float InstantEnergy = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float InstantFood = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float InstantWater = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float InstantRadiationCleanse = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float InstantToxinCleanse = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float InstantStopBleeding = 0.f; // >0 则止血

    // Buff 效果
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FBuffInstance> AppliedBuffs;

    // 使用条件
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bRequiresInSpace = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bRequiresOnPlanet = false;

    // 重量/体积
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Weight = 0.5f; // kg

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Volume = 0.1f; // liters

    // 稀有度
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString RarityTier = TEXT("Common"); // Common/Uncommon/Rare/Epic/Legendary

    // 价格
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BasePrice = 10.f;

    // 描述
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Description;

    // 图标
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString IconPath;

    // 工具方法
    float GetQualityMultiplier() const
    {
        return 1.f + (QualityLevel - 1) * 0.25f;
    }

    FString GetQualityColor() const
    {
        switch (QualityLevel)
        {
            case 1: return TEXT("<White>");
            case 2: return TEXT("<Green>");
            case 3: return TEXT("<Blue>");
            case 4: return TEXT("<Purple>");
            case 5: return TEXT("<Orange>");
        }
        return TEXT("");
    }
};

// 消耗品组件（挂在角色上）
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UConsumableInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UConsumableInventoryComponent();

    // —— 快捷栏（0~9） ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consumables|Hotbar")
    TArray<FName> Hotbar; // 长度 10

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Consumables|Hotbar")
    int32 ActiveSlot = 0;

    // —— 激活的 Buff ——
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Consumables|Buffs")
    TArray<FBuffInstance> ActiveBuffs;

    // —— 预设消耗品库 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consumables|Library")
    TMap<FName, FConsumableData> ConsumableLibrary;

    // ========== 使用 ==========
    UFUNCTION(BlueprintCallable, Category = "Consumables")
    bool UseConsumable(FName ItemID);

    UFUNCTION(BlueprintCallable, Category = "Consumables")
    bool UseFromHotbar(int32 SlotIndex);

    UFUNCTION(BlueprintCallable, Category = "Consumables")
    void CycleHotbar(int32 Direction = 1);

    // ========== 注册 ==========
    UFUNCTION(BlueprintCallable, Category = "Consumables")
    void RegisterConsumable(const FConsumableData& Data);

    UFUNCTION(BlueprintCallable, Category = "Consumables")
    void RegisterDefaults();

    // ========== 查询 ==========
    UFUNCTION(BlueprintCallable, Category = "Consumables")
    FConsumableData GetConsumableData(FName ItemID) const;

    UFUNCTION(BlueprintCallable, Category = "Consumables")
    bool HasConsumable(FName ItemID, int32 MinQuantity = 1) const;

    UFUNCTION(BlueprintCallable, Category = "Consumables")
    FString GetBuffStatusText() const;

    // ========== Tick ==========
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    // —— 事件 ——
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnConsumableUsed, FName, ItemID);
    UPROPERTY(BlueprintAssignable, Category = "Consumables|Events")
    FOnConsumableUsed OnConsumableUsed;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBuffExpired, EBuffType, BuffType);
    UPROPERTY(BlueprintAssignable, Category = "Consumables|Events")
    FOnBuffExpired OnBuffExpired;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBuffApplied, FBuffInstance, Buff);
    UPROPERTY(BlueprintAssignable, Category = "Consumables|Events")
    FOnBuffApplied OnBuffApplied;

private:
    // 快捷栏 → 库存查询
    int32 GetHotbarQuantity(int32 SlotIndex) const;

    // 应用效果
    void ApplyInstantEffects(const FConsumableData& Data);
    void ApplyBuffEffects(const FConsumableData& Data);

    // 查找角色维生组件
    class UVitalsComponent* GetVitals() const;

    // 已激活 Buff 的 Tick
    void TickActiveBuffs(float DeltaTime);

    // 快捷栏引用（指向 InventoryComponent 的槽位）
    UPROPERTY()
    class UInventoryComponent* LinkedInventory = nullptr;
};
