// ============================================================
// 路径: Source/StellarSystem/Public/Economy/AmmoItem.h
// 作用: 弹药系统 — 15 种 / 弹道物理 / 特种弹药
// 依赖: 无
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AmmoItem.generated.h"

class UParticleSystem;

// 弹药大类
UENUM(BlueprintType)
enum class EAmmoCategory : uint8
{
    Bullet      UMETA(DisplayName = "Bullet (Kinetic)"),
    Energy      UMETA(DisplayName = "Energy Cell"),
    Plasma      UMETA(DisplayName = "Plasma Container"),
    Rail        UMETA(DisplayName = "Rail Slug"),
    Rocket      UMETA(DisplayName = "Rocket/Missile"),
    Mine        UMETA(DisplayName = "Space Mine"),
    Flare       UMETA(DisplayName = "Flare/Signal"),
    Special     UMETA(DisplayName = "Specialized")
};

// 特种弹药类型
UENUM(BlueprintType)
enum class ESpecialAmmoType : uint8
{
    ArmorPiercing  UMETA(DisplayName = "Armor Piercing (AP)"),
    HollowPoint    UMETA(DisplayName = "Hollow Point (HP)"),
    Subsonic      UMETA(DisplayName = "Subsonic (Quiet)"),
    Tracer        UMETA(DisplayName = "Tracer (Visible)"),
    Incendiary    UMETA(DisplayName = "Incendiary"),
    Explosive     UMETA(DisplayName = "Explosive Tip"),
    Poison        UMETA(DisplayName = "Poison Tipped"),
    Cryo          UMETA(DisplayName = "Cryo-Freeze"),
    Electromagnetic UMETA(DisplayName = "EMP Round"),
    Quantum       UMETA(DisplayName = "Quantum Phase")
};

// 弹药数据
USTRUCT(BlueprintType)
struct FAmmoData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName AmmoID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EAmmoCategory Category = EAmmoCategory::Bullet;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ESpecialAmmoType SpecialType = ESpecialAmmoType::ArmorPiercing;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 QualityLevel = 1;

    // 弹道
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MuzzleVelocity = 50000.f; // cm/s = 500m/s

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Mass = 0.05f; // kg

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Caliber = 10.f; // mm

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DragCoefficient = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float GravityEffect = 0.f; // 太空=0, 行星表面>0

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxRange = 30000.f; // cm

    // 伤害
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BaseDamage = 20.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ArmorPenetration = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SplashRadius = 0.f;

    // 效果
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsTracer = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsIncendiary = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsExplosive = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsPoison = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsCryo = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsEMP = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsStealth = false; // 不被雷达发现

    // 经济
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PricePerUnit = 5.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 StackSize = 30;

    // 特效
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<UParticleSystem> TracerEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<UParticleSystem> ImpactEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<UParticleSystem> TrailEffect;

    // 工具方法
    float GetEffectiveDamage() const
    {
        float Mult = 1.f + (QualityLevel - 1) * 0.15f;
        if (bIsIncendiary) Mult *= 1.2f;
        if (bIsExplosive) Mult *= 1.3f;
        if (bIsPoison) Mult *= 1.1f;
        return BaseDamage * Mult;
    }

    FString GetCategoryText() const
    {
        switch (Category)
        {
            case EAmmoCategory::Bullet:  return TEXT("Kinetic");
            case EAmmoCategory::Energy:  return TEXT("Energy");
            case EAmmoCategory::Plasma:  return TEXT("Plasma");
            case EAmmoCategory::Rail:    return TEXT("Rail");
            case EAmmoCategory::Rocket:  return TEXT("Rocket");
            case EAmmoCategory::Mine:    return TEXT("Mine");
            case EAmmoCategory::Flare:   return TEXT("Flare");
            case EAmmoCategory::Special: return TEXT("Special");
        }
        return TEXT("");
    }
};

// 弹药库组件
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UAmmoInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UAmmoInventoryComponent();

    // —— 弹药库存 ——
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ammo|Inventory")
    TMap<FName, int32> AmmoStock; // AmmoID → Count

    // —— 预设弹药库 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo|Library")
    TMap<FName, FAmmoData> AmmoLibrary;

    // —— 快捷选择 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo|Loadout")
    FName PrimaryAmmoType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo|Loadout")
    FName SecondaryAmmoType;

    // ========== 库存操作 ==========
    UFUNCTION(BlueprintCallable, Category = "Ammo")
    bool AddAmmo(FName AmmoID, int32 Count);

    UFUNCTION(BlueprintCallable, Category = "Ammo")
    bool ConsumeAmmo(FName AmmoID, int32 Count);

    UFUNCTION(BlueprintCallable, Category = "Ammo")
    int32 GetAmmoCount(FName AmmoID) const;

    UFUNCTION(BlueprintCallable, Category = "Ammo")
    bool HasAmmo(FName AmmoID, int32 MinCount = 1) const;

    // ========== 注册 ==========
    UFUNCTION(BlueprintCallable, Category = "Ammo")
    void RegisterAmmoType(const FAmmoData& Data);

    UFUNCTION(BlueprintCallable, Category = "Ammo")
    void RegisterDefaultAmmoTypes();

    // ========== 查询 ==========
    UFUNCTION(BlueprintCallable, Category = "Ammo")
    FAmmoData GetAmmoData(FName AmmoID) const;

    UFUNCTION(BlueprintCallable, Category = "Ammo")
    TArray<FName> GetAvailableAmmoTypes() const;

    UFUNCTION(BlueprintCallable, Category = "Ammo")
    FString GetAmmoDescription(FName AmmoID) const;

    // ========== 弹道模拟 ==========
    UFUNCTION(BlueprintCallable, Category = "Ammo|Ballistics")
    FVector CalculateTrajectory(const FVector& Start, const FVector& Direction,
        FName AmmoID, float TimeStep = 0.016f, int32 MaxSteps = 1000) const;

    UFUNCTION(BlueprintCallable, Category = "Ammo|Ballistics")
    float CalculateDrop(const FVector& Start, FName AmmoID, float FlightTime) const;

    // ========== 生成 ==========
    UFUNCTION(BlueprintCallable, Category = "Ammo|Generate")
    FAmmoData GenerateRandomAmmo(EAmmoCategory Category, int32 QualityLevel, int32 Seed);

    // —— 事件 ——
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAmmoChanged, FName, AmmoID);
    UPROPERTY(BlueprintAssignable, Category = "Ammo|Events")
    FOnAmmoChanged OnAmmoAdded;

    UPROPERTY(BlueprintAssignable, Category = "Ammo|Events")
    FOnAmmoChanged OnAmmoConsumed;

    UPROPERTY(BlueprintAssignable, Category = "Ammo|Events")
    FOnAmmoChanged OnAmmoDepleted;

private:
    void ApplySpecialProperties(FAmmoData& Ammo, ESpecialAmmoType Special, int32 QualityLevel);
    float GetSpecialDamageMultiplier(ESpecialAmmoType Type) const;
};
