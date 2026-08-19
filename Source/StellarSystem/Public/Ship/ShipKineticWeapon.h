// ============================================================
// 路径: Source/StellarSystem/Public/Ship/ShipKineticWeapon.h
// 作用: 飞船实弹武器（Autocannon/Railgun/MassDriver/Gatling 四类细分）
// 依赖: ShipWeaponBase.h
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "ShipWeaponBase.h"
#include "ShipKineticWeapon.generated.h"

class UParticleSystem;
class USoundBase;

// 实弹武器细分类型
UENUM(BlueprintType)
enum class EShipKineticWeaponSubtype : uint8
{
    Autocannon  UMETA(DisplayName = "Autocannon (High RoF)"),
    Railgun     UMETA(DisplayName = "Railgun (Pierce)"),
    MassDriver  UMETA(DisplayName = "Mass Driver (Heavy)"),
    Gatling     UMETA(DisplayName = "Gatling Cannon (Spread)")
};

// 实弹专用伤害属性
USTRUCT(BlueprintType)
struct FShipKineticDamageProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ArmorPierce = 0.6f;             // 穿甲率（越高越无视护甲）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HullDamageMultiplier = 1.3f;    // 对船体伤害倍率

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ShieldDamageMultiplier = 0.5f;  // 对护盾伤害倍率

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ProjectileMass = 0.5f;          // 弹丸质量 kg

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ProjectileSpeed = 120000.f;     // 弹丸速度 cm/s

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ProjectileDrag = 0.15f;         // 空气阻力系数

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIgnoresShield = false;           // 是否穿透护盾

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float KnockbackForce = 100.f;         // 击退力
};

UCLASS(ClassGroup=(Ship|Weapons), meta=(BlueprintSpawnableComponent))
class UShipKineticWeaponComponent : public UShipWeaponBaseComponent
{
    GENERATED_BODY()

public:
    UShipKineticWeaponComponent();

    virtual void TickComponent(float Dt, ELevelTick Tick, FActorComponentTickFunction* Fn) override;
    virtual void BeginPlay() override;

    // —— 子类类型 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kinetic Weapon")
    EShipKineticWeaponSubtype KineticSubtype = EShipKineticWeaponSubtype::Autocannon;

    // —— 弹道属性 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kinetic Weapon")
    float MuzzleVelocity = 120000.f;       // cm/s

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kinetic Weapon")
    float GravityEffect = 0.f;             // 太空=0

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kinetic Weapon")
    float MaxRange = 80000.f;              // cm

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kinetic Weapon")
    float SpreadAngle = 0.5f;             // 散射角（度）

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kinetic Weapon")
    int32 PelletCount = 1;                 // 每次发射弹丸数（霰弹）

    // —— 弹药管理 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kinetic Weapon")
    int32 MagazineSize = 60;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kinetic Weapon")
    int32 CurrentAmmo = 60;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kinetic Weapon")
    float ReloadTime = 3.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kinetic Weapon")
    bool bIsReloading = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kinetic Weapon")
    FName AmmoTypeID = FName("ShipAutocannonAmmo");

    // —— 伤害配置 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kinetic Weapon")
    FShipKineticDamageProfile DamageProfile;

    // —— 穿甲模式（Railgun 专用）——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kinetic Weapon")
    bool bChargedShot = false;             // 蓄力射击

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kinetic Weapon")
    float ChargeTime = 2.f;                // 蓄力时间

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kinetic Weapon")
    float ChargedDamageMultiplier = 3.f;   // 蓄力伤害倍率

    // —— 视觉效果 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kinetic Weapon|VFX")
    TSoftObjectPtr<UParticleSystem> TracerEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kinetic Weapon|VFX")
    TSoftObjectPtr<UParticleSystem> ImpactEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kinetic Weapon|VFX")
    TSoftObjectPtr<UParticleSystem> MuzzleFlashEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kinetic Weapon|VFX")
    TSoftObjectPtr<USoundBase> FireSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kinetic Weapon|VFX")
    TSoftObjectPtr<USoundBase> ReloadSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kinetic Weapon|VFX")
    FLinearColor TracerColor = FLinearColor(1.f, 0.9f, 0.3f, 1.f);

    // —— 开火 ——
    virtual void FireWeapon(int32 SlotIndex) override;
    virtual bool CanFire() const override;

    // —— 弹药操作 ——
    UFUNCTION(BlueprintCallable, Category = "Kinetic Weapon")
    void StartReload();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Kinetic Weapon")
    float GetAmmoPercent() const;

    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Kinetic Weapon")
    void ServerReload();

    // —— 蓄力 ——
    UFUNCTION(BlueprintCallable, Category = "Kinetic Weapon")
    void StartCharging();

    UFUNCTION(BlueprintCallable, Category = "Kinetic Weapon")
    void ReleaseChargedShot();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Kinetic Weapon")
    float GetChargeProgress() const;

    // —— 网络复制 ——
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

private:
    void ProcessAutocannonFire(int32 SlotIndex);
    void ProcessRailgunFire(int32 SlotIndex);
    void ProcessMassDriverFire(int32 SlotIndex);
    void ProcessGatlingFire(int32 SlotIndex);
    void SpawnKineticProjectile(int32 SlotIndex, const FVector& Direction, float DamageMult);

    FTimerHandle ReloadTimerHandle;
    float CurrentCharge = 0.f;
    bool bIsCharging = false;
};
