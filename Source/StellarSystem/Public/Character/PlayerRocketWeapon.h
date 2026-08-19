// ============================================================
// 路径: Source/StellarSystem/Public/Character/PlayerRocketWeapon.h
// 作用: 玩家火箭弹武器（RPG/微导弹/蜂群/制导/双发 5 种细分）
// 依赖: PlayerWeaponBase.h
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "PlayerWeaponBase.h"
#include "PlayerRocketWeapon.generated.h"

class UParticleSystem;
class USoundBase;

// 火箭弹细分
UENUM(BlueprintType)
enum class ERocketSubtype : uint8
{
    RPG          UMETA(DisplayName = "RPG (火箭筒)"),
    MicroMissile UMETA(DisplayName = "Micro Missile (微导弹)"),
    SwarmRocket  UMETA(DisplayName = "Swarm Rocket (蜂群)"),
    GuidedRocket UMETA(DisplayName = "Guided Rocket (制导)"),
    DualRocket   UMETA(DisplayName = "Dual Rocket (双发)")
};

// 火箭弹飞行参数
USTRUCT(BlueprintType)
struct FRocketFlightParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ThrustAcceleration = 60000.f;  // 加速度 cm/s²

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxSpeed = 40000.f;            // 最大速度 cm/s

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TurnRate = 60.f;              // 转弯速率 度/秒（制导用）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FuelDuration = 5.f;            // 燃料持续时间 秒

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ArmDistance = 200.f;          // 保险距离 cm

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DetonationDelay = 0.05f;      // 触发延迟

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bHasThrustVectoring = false;   // 推力矢量

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ThrustVectoringAngle = 15.f;  // 矢量角
};

// 火箭弹战斗部
USTRUCT(BlueprintType)
struct FRocketWarhead
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ExplosionDamage = 150.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ExplosionRadius = 500.f;       // cm

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ArmorPierce = 0.4f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ShieldDamageMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HullDamageMultiplier = 1.2f;

    // 特殊弹头
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsShapedCharge = false;       // 聚能弹头（反装甲）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ShapedChargeArmorBonus = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsEMP = false;               // 电磁弹头

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EMPRadius = 300.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EMPDuration = 4.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsIncendiary = false;        // 燃烧弹头

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FireDamagePerSec = 8.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FireDuration = 6.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsFrag = false;              // 破片弹头

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 FragCount = 16;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FragSpreadAngle = 45.f;
};

// 制导参数
USTRUCT(BlueprintType)
struct FRocketGuidance
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bHasActiveGuidance = false;     // 是否有主动制导

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LockTime = 1.0f;              // 锁定时间

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LockConeAngle = 20.f;         // 锁定锥角

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxLockRange = 25000.f;       // 最大锁定距离

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bCanLockOnHeat = true;         // 热感应

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bCanLockOnRadar = false;      // 雷达感应

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bCanLockOnOptical = false;    // 光学感应

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIgnoresFlares = false;       // 是否无视热焰弹

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float JamResistance = 0.5f;       // 抗干扰 0-1
};

// 蜂群专用参数
USTRUCT(BlueprintType)
struct FSwarmRocketParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 SwarmCount = 4;               // 蜂群数量

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SwarmSpreadAngle = 10.f;     // 散射角

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SwarmSeparationDelay = 0.2f; // 分离延迟

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEachRocketIndependent = true; // 每发独立锁定

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SwarmRetargetRange = 800.f;  // 换目标范围
};

UCLASS(ClassGroup=(Character|Weapons), meta=(BlueprintSpawnableComponent))
class UPlayerRocketWeaponComponent : public UPlayerWeaponBaseComponent
{
    GENERATED_BODY()

public:
    UPlayerRocketWeaponComponent();

    virtual void TickComponent(float Dt, ELevelTick Tick, FActorComponentTickFunction* Fn) override;
    virtual void BeginPlay() override;

    // —— 子类类型 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocket")
    ERocketSubtype RocketType = ERocketSubtype::RPG;

    // —— 飞行 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocket|Flight")
    FRocketFlightParams FlightParams;

    // —— 战斗部 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocket|Warhead")
    FRocketWarhead Warhead;

    // —— 制导 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocket|Guidance")
    FRocketGuidance Guidance;

    // —— 蜂群 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocket|Swarm")
    FSwarmRocketParams SwarmParams;

    // —— 锁定系统 ——
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rocket|Lock")
    float CurrentLockProgress = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rocket|Lock")
    bool bLocked = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rocket|Lock")
    AActor* CurrentTarget = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocket|Lock")
    bool bRequiresLock = false;         // RPG 不需要锁定，Guided 需要

    // —— 弹药管理 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocket|Ammo")
    int32 MagazineSize = 2;            // 火箭筒弹容量很小

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rocket|Ammo")
    int32 CurrentRocketAmmo = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocket|Ammo")
    float ReloadTime = 4.f;            // 火箭装填慢

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rocket|Ammo")
    bool bIsReloading = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocket|Ammo")
    FName RocketAmmoID = FName("RocketWarhead");

    // —— 后坐力/警告 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocket|Safety")
    float BackblastRadius = 300.f;     // 后喷危险区

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocket|Safety")
    float BackblastDamage = 50.f;      // 后喷伤害

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocket|Safety")
    bool bWarnNearbyAllies = true;     // 警告附近队友

    // —— 视觉效果 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocket|VFX")
    TSoftObjectPtr<UStaticMesh> RocketMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocket|VFX")
    TSoftObjectPtr<UParticleSystem> RocketTrail;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocket|VFX")
    TSoftObjectPtr<UParticleSystem> ExplosionEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocket|VFX")
    TSoftObjectPtr<UParticleSystem> BackblastEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocket|VFX")
    TSoftObjectPtr<USoundBase> LaunchSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocket|VFX")
    TSoftObjectPtr<USoundBase> ExplosionSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocket|VFX")
    TSoftObjectPtr<USoundBase> LockSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocket|VFX")
    FLinearColor RocketTrailColor = FLinearColor(1.f, 0.5f, 0.1f, 1.f);

    // —— 开火 ——
    virtual void FireWeapon() override;
    virtual bool CanFire() const override;

    // —— 锁定 ——
    UFUNCTION(BlueprintCallable, Category = "Rocket")
    void AcquireTarget(AActor* Target);

    UFUNCTION(BlueprintCallable, Category = "Rocket")
    void ReleaseTarget();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rocket")
    float GetLockProgress() const { return CurrentLockProgress; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rocket")
    bool IsLocked() const { return bLocked; }

    // —— 双发专用 ——
    UFUNCTION(BlueprintCallable, Category = "Rocket|Dual")
    void FireDualRockets();

    // —— 蜂群专用 ——
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rocket|Swarm")
    int32 GetSwarmCount() const { return SwarmParams.SwarmCount; }

    // —— 装填 ——
    UFUNCTION(BlueprintCallable, Category = "Rocket")
    void StartReload();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rocket")
    float GetAmmoPercent() const;

    // —— 反制 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocket|Counter")
    bool bCanBeJammed = true;

    // —— 网络复制 ——
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

    // —— 事件 ——
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRocketLocked, AActor*, Target);
    UPROPERTY(BlueprintAssignable, Category = "Rocket|Events")
    FOnRocketLocked OnRocketLocked;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRocketLaunched, int32, AmmoRemaining);
    UPROPERTY(BlueprintAssignable, Category = "Rocket|Events")
    FOnRocketLaunched OnRocketLaunched;

private:
    void ProcessRPGFire();
    void ProcessMicroMissileFire();
    void ProcessSwarmFire();
    void ProcessGuidedFire(AActor* Target);
    void ProcessDualFire();
    void SpawnRocketProjectile(const FVector& Origin, const FVector& Direction,
                               AActor* HomingTarget, float DamageMult);
    void UpdateLocking(float Dt);
    void CheckBackblast();

    FTimerHandle ReloadTimerHandle;
    FTimerHandle LockTimerHandle;
};
