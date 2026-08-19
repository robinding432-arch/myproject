// ============================================================
// 路径: Source/StellarSystem/Public/Character/PlayerEnergyWeapon.h
// 作用: 玩家能量武器（激光手枪/等离子步枪/光束/离子 4 种细分）
// 依赖: PlayerWeaponBase.h
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "PlayerWeaponBase.h"
#include "PlayerEnergyWeapon.generated.h"

class UParticleSystem;
class USoundBase;

// 能量武器细分
UENUM(BlueprintType)
enum class EEnergySubtype : uint8
{
    LaserPistol     UMETA(DisplayName = "Laser Pistol (激光手枪)"),
    PlasmaRifle     UMETA(DisplayName = "Plasma Rifle (等离子步枪)"),
    BeamRifle       UMETA(DisplayName = "Beam Rifle (光束步枪)"),
    IonBlaster      UMETA(DisplayName = "Ion Blaster (离子冲击)")
};

// 能量电池属性
USTRUCT(BlueprintType)
struct FEnergyCellParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxEnergy = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EnergyPerShot = 5.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RegenPerSecond = 8.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bOverchargeAllowed = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OverchargeDrainMultiplier = 3.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OverchargeDamageMultiplier = 2.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OverchargeHeatMultiplier = 2.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HeatPerShot = 3.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HeatDissipationRate = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OverheatThreshold = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OverheatCooldown = 2.f;
};

// 能量伤害属性
USTRUCT(BlueprintType)
struct FEnergyDamageProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ShieldDamageMultiplier = 1.5f;   // 对护盾额外伤害

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HullDamageMultiplier = 0.7f;     // 对实体伤害较低

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HeatDamage = 3.f;                 // 过热伤害（积累减速）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ShieldPenetration = 0.2f;       // 护盾穿透

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bCanDisableShields = false;       // 能否瘫痪护盾

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ShieldDisableDuration = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bCanDisableSystems = false;       // 能否瘫痪子系统

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName SystemDisableType = FName("Weapons");
};

UCLASS(ClassGroup=(Character|Weapons), meta=(BlueprintSpawnableComponent))
class UPlayerEnergyWeaponComponent : public UPlayerWeaponBaseComponent
{
    GENERATED_BODY()

public:
    UPlayerEnergyWeaponComponent();

    virtual void TickComponent(float Dt, ELevelTick Tick, FActorComponentTickFunction* Fn) override;
    virtual void BeginPlay() override;

    // —— 子类类型 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy")
    EEnergySubtype EnergyType = EEnergySubtype::LaserPistol;

    // —— 电池 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy|Cell")
    FEnergyCellParams CellParams;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Energy|Cell")
    float CurrentEnergy = 100.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Energy|Cell")
    float CurrentHeat = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Energy|Cell")
    bool bOverheated = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Energy|Cell")
    bool bOvercharging = false;

    // —— 伤害 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy|Damage")
    FEnergyDamageProfile DamageProfile;

    // —— 射弹/光束属性 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy|Beam")
    float BeamWidth = 2.f;                  // Beam 宽度 cm

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy|Beam")
    bool bIsInstantHit = true;             // 即时命中（Beam/Laser）vs 弹道（Plasma）

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy|Beam")
    float PlasmaProjectileSpeed = 60000.f; // 等离子弹速度

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy|Beam")
    float PlasmaLifetime = 2.f;            // 等离子弹寿命

    // —— 视觉效果 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy|VFX")
    FLinearColor BeamColor = FLinearColor(0.2f, 0.6f, 1.f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy|VFX")
    TSoftObjectPtr<UParticleSystem> BeamEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy|VFX")
    TSoftObjectPtr<UParticleSystem> MuzzleFlash;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy|VFX")
    TSoftObjectPtr<UParticleSystem> ImpactEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy|VFX")
    TSoftObjectPtr<UParticleSystem> PlasmaTrail;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy|VFX")
    TSoftObjectPtr<USoundBase> FireSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy|VFX")
    TSoftObjectPtr<USoundBase> OverheatSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy|VFX")
    TSoftObjectPtr<USoundBase> RechargeSound;

    // —— 过载模式 ——
    UFUNCTION(BlueprintCallable, Category = "Energy")
    void ToggleOvercharge();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Energy")
    bool IsOvercharging() const { return bOvercharging; }

    // —— 开火 ——
    virtual void FireWeapon() override;
    virtual bool CanFire() const override;

    // —— 能量管理 ——
    UFUNCTION(BlueprintCallable, Category = "Energy")
    void RechargeEnergy(float Amount);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Energy")
    float GetEnergyPercent() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Energy")
    float GetHeatPercent() const;

    // —— 电池更换 ——
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Energy")
    void ServerSwapEnergyCell(float NewMaxEnergy, float NewCurrentEnergy);

    // —— 网络复制 ——
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

private:
    void FireInstantHit();
    void FirePlasmaProjectile();
    void ProcessBeamFire();
    void ProcessLaserFire();
    void ProcessIonFire();
    void UpdateHeat(float Dt);
    void UpdateEnergy(float Dt);

    FTimerHandle OverheatTimerHandle;
};
