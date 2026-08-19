// ============================================================
// 路径: Source/StellarSystem/Public/Ship/ShipEnergyWeapon.h
// 作用: 飞船能量武器（Laser/Plasma/Beam/Pulse 四类细分）
// 依赖: ShipWeaponBase.h
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "ShipWeaponBase.h"
#include "ShipEnergyWeapon.generated.h"

class UParticleSystem;
class USoundBase;

// 能量武器细分类型
UENUM(BlueprintType)
enum class EShipEnergyWeaponSubtype : uint8
{
    Laser       UMETA(DisplayName = "Laser Cannon"),
    Plasma      UMETA(DisplayName = "Plasma Lance"),
    Beam        UMETA(DisplayName = "Beam Emitter"),
    Pulse       UMETA(DisplayName = "Pulse Array")
};

// 能量武器伤害属性
USTRUCT(BlueprintType)
struct FShipEnergyDamageProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ShieldDamageMultiplier = 1.4f;    // 对护盾伤害倍率

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HullDamageMultiplier = 0.8f;      // 对船体伤害倍率

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HeatDamage = 5.f;                  // 过热伤害（积累后减速）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EnergyDrain = 2.f;                // 目标能量消耗

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ShieldPenetration = 0.15f;        // 护盾穿透率

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bCanDisableSystems = false;         // 能否瘫痪子系统
};

UCLASS(ClassGroup=(Ship|Weapons), meta=(BlueprintSpawnableComponent))
class UShipEnergyWeaponComponent : public UShipWeaponBaseComponent
{
    GENERATED_BODY()

public:
    UShipEnergyWeaponComponent();

    virtual void TickComponent(float Dt, ELevelTick Tick, FActorComponentTickFunction* Fn) override;
    virtual void BeginPlay() override;

    // —— 子类类型 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Weapon")
    EShipEnergyWeaponSubtype EnergySubtype = EShipEnergyWeaponSubtype::Laser;

    // —— 能量专用属性 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Weapon")
    float EnergyPerShot = 15.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Weapon")
    float MaxEnergy = 200.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Energy Weapon")
    float CurrentEnergy = 200.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Weapon")
    float EnergyRegenPerSecond = 25.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Weapon")
    float OverheatThreshold = 100.f;        // 过热百分比

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Energy Weapon")
    float CurrentHeat = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Weapon")
    float HeatPerShot = 8.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Weapon")
    float HeatDissipationRate = 12.f;        // 每秒散热

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Weapon")
    bool bOverheated = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Weapon")
    float OverheatCooldown = 3.f;            // 过热后冷却时间

    // —— 伤害配置 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Weapon")
    FShipEnergyDamageProfile DamageProfile;

    // —— 射弹属性（Laser/Plasma 用弹道，Beam 用射线，Pulse 用范围）——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Weapon")
    float BeamWidth = 5.f;                   // Beam 宽度（cm）

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Weapon")
    float PulseRadius = 300.f;               // Pulse 范围伤害半径

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Weapon")
    int32 PulseCount = 3;                    // Pulse 每次发射的弹丸数

    // —— 视觉效果 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Weapon|VFX")
    TSoftObjectPtr<UParticleSystem> MuzzleFlashEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Weapon|VFX")
    TSoftObjectPtr<UParticleSystem> BeamEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Weapon|VFX")
    TSoftObjectPtr<UParticleSystem> ImpactEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Weapon|VFX")
    FLinearColor BeamColor = FLinearColor(0.2f, 0.6f, 1.f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Weapon|VFX")
    TSoftObjectPtr<USoundBase> FireSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Weapon|VFX")
    TSoftObjectPtr<USoundBase> OverheatSound;

    // —— 开火 ——
    virtual void FireWeapon(int32 SlotIndex) override;
    virtual bool CanFire() const override;

    // —— 能量管理 ——
    UFUNCTION(BlueprintCallable, Category = "Energy Weapon")
    void RechargeEnergy(float Amount);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Energy Weapon")
    float GetEnergyPercent() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Energy Weapon")
    float GetHeatPercent() const;

    // —— 过载模式 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Weapon")
    bool bOverchargeMode = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Weapon")
    float OverchargeDamageMultiplier = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Weapon")
    float OverchargeHeatMultiplier = 2.f;

    UFUNCTION(BlueprintCallable, Category = "Energy Weapon")
    void SetOvercharge(bool bEnabled);

    // —— 网络复制 ——
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

private:
    void ProcessBeamFire(int32 SlotIndex);
    void ProcessProjectileFire(int32 SlotIndex);
    void ProcessPulseFire(int32 SlotIndex);
    void UpdateHeat(float Dt);
    void UpdateEnergy(float Dt);

    FTimerHandle OverheatTimerHandle;
};
