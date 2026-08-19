// ============================================================
// 路径: Source/StellarSystem/Public/Ship/ShipTorpedoWeapon.h
// 作用: 飞船鱼雷武器（HeavyTorpedo/Devastator/Nuclear/GuidedTorpedo 四类细分）
// 依赖: ShipWeaponBase.h
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "ShipWeaponBase.h"
#include "ShipTorpedoWeapon.generated.h"

class UParticleSystem;
class USoundBase;

// 鱼雷细分类型
UENUM(BlueprintType)
enum class EShipTorpedoSubtype : uint8
{
    HeavyTorpedo   UMETA(DisplayName = "Heavy Torpedo (Massive Damage)"),
    Devastator     UMETA(DisplayName = "Devastator (Ship Killer)"),
    NuclearTorpedo UMETA(DisplayName = "Nuclear Torpedo (AoE)"),
    GuidedTorpedo  UMETA(DisplayName = "Guided Torpedo (Smart)"),
    ShatterTorpedo UMETA(DisplayName = "Shatter Torpedo (Armor Break)")
};

// 鱼雷飞行参数
USTRUCT(BlueprintType)
struct FShipTorpedoFlightParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ThrustAcceleration = 40000.f;   // 加速度 cm/s²（比导弹慢，更重）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxSpeed = 35000.f;             // 最大速度 cm/s

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TurnRate = 20.f;                // 转弯速率 度/秒（缓慢）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FuelDuration = 20.f;            // 燃料持续时间 秒

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ArmDistance = 800.f;           // 保险距离 cm（比导弹远）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float IgnitionDelay = 0.5f;          // 点火延迟

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bUsesWarpDrive = false;          // 是否使用微型跃迁引擎

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float WarpRange = 5000.f;            // 跃迁距离 cm

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float WarpChargeTime = 3.f;          // 跃迁充能时间
};

// 鱼雷战斗部参数
USTRUCT(BlueprintType)
struct FShipTorpedoWarhead
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BaseDamage = 300.f;            // 基础伤害（极高）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ExplosionRadius = 2000.f;     // 爆炸半径 cm（巨大）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ArmorPierce = 0.8f;           // 穿甲率（极高）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ShieldDamageMultiplier = 0.6f; // 对护盾伤害倍率（鱼雷偏重船体）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HullDamageMultiplier = 2.0f;  // 对船体伤害倍率

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RadiationDamage = 0.f;         // 辐射伤害

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RadiationRadius = 0.f;        // 辐射范围

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RadiationDuration = 0.f;      // 辐射持续时间

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsNuclear = false;             // 是否为核弹头

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float NuclearYield = 0.f;            // 核当量（千吨 TNT）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float NuclearFalloutDuration = 0.f;  // 核辐射持续时间

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsEMP = false;                // 是否为电磁脉冲弹头

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EMPRadius = 0.f;              // EMP 范围

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EMPDuration = 0.f;            // EMP 持续时间

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bShatterEffect = false;         // 是否破甲

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ShatterArmorReduction = 0.f;  // 破甲后目标护甲降低值
};

// 鱼雷制导参数
USTRUCT(BlueprintType)
struct FShipTorpedoGuidance
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bHasActiveGuidance = true;      // 是否有主动制导

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LockTime = 3.f;               // 锁定时间（比导弹长）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LockConeAngle = 8.f;          // 锁定锥角（比导弹窄，更精准）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxLockRange = 50000.f;       // 最大锁定距离

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bCanRetarget = true;           // 能否中途换目标

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RetargetInterval = 2.f;       // 换目标间隔

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIgnoresFlares = false;        // 是否无视热焰弹

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bCanTargetShields = false;     // 能否锁定护盾发生器
};

UCLASS(ClassGroup=(Ship|Weapons), meta=(BlueprintSpawnableComponent))
class UShipTorpedoWeaponComponent : public UShipWeaponBaseComponent
{
    GENERATED_BODY()

public:
    UShipTorpedoWeaponComponent();

    virtual void TickComponent(float Dt, ELevelTick Tick, FActorComponentTickFunction* Fn) override;
    virtual void BeginPlay() override;

    // —— 子类类型 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torpedo Weapon")
    EShipTorpedoSubtype TorpedoSubtype = EShipTorpedoSubtype::HeavyTorpedo;

    // —— 飞行参数 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torpedo Weapon")
    FShipTorpedoFlightParams FlightParams;

    // —— 战斗部 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torpedo Weapon")
    FShipTorpedoWarhead Warhead;

    // —— 制导 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torpedo Weapon")
    FShipTorpedoGuidance Guidance;

    // —— 锁定系统 ——
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Torpedo Weapon|Lock")
    float CurrentLockProgress = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Torpedo Weapon|Lock")
    bool bLocked = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Torpedo Weapon|Lock")
    AActor* CurrentTarget = nullptr;

    // —— 弹药管理 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torpedo Weapon")
    int32 MagazineSize = 4;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Torpedo Weapon")
    int32 CurrentAmmo = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torpedo Weapon")
    float ReloadTime = 12.f;            // 鱼雷装填很慢

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Torpedo Weapon")
    bool bIsReloading = false;

    // —— 发射序列（鱼雷需要预热）——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torpedo Weapon|Launch")
    float LaunchWarmupTime = 2.f;       // 发射预热

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Torpedo Weapon|Launch")
    float CurrentWarmup = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Torpedo Weapon|Launch")
    bool bIsWarmingUp = false;

    // —— Devastator 专用 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torpedo Weapon|Devastator")
    float DevastatorChargeTime = 5.f;   // 蓄力时间

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torpedo Weapon|Devastator")
    float DevastatorDamageMultiplier = 2.5f;

    // —— Nuclear 专用 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torpedo Weapon|Nuclear")
    bool bShowFalloutWarning = true;    // 是否显示辐射警告

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torpedo Weapon|Nuclear")
    FLinearColor NuclearBlastColor = FLinearColor(1.f, 0.8f, 0.3f, 1.f);

    // —— 视觉效果 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torpedo Weapon|VFX")
    TSoftObjectPtr<UParticleSystem> TorpedoTrailEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torpedo Weapon|VFX")
    TSoftObjectPtr<UParticleSystem> ExplosionEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torpedo Weapon|VFX")
    TSoftObjectPtr<UParticleSystem> NuclearExplosionEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torpedo Weapon|VFX")
    TSoftObjectPtr<USoundBase> LaunchSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torpedo Weapon|VFX")
    TSoftObjectPtr<USoundBase> ExplosionSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torpedo Weapon|VFX")
    TSoftObjectPtr<USoundBase> LockSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torpedo Weapon|VFX")
    TSoftObjectPtr<USoundBase> NuclearExplosionSound;

    // —— 开火 ——
    virtual void FireWeapon(int32 SlotIndex) override;
    virtual bool CanFire() const override;

    // —— 锁定 ——
    UFUNCTION(BlueprintCallable, Category = "Torpedo Weapon")
    void AcquireTarget(AActor* Target);

    UFUNCTION(BlueprintCallable, Category = "Torpedo Weapon")
    void ReleaseTarget();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Torpedo Weapon")
    float GetLockProgress() const { return CurrentLockProgress; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Torpedo Weapon")
    bool IsLocked() const { return bLocked; }

    // —— 预热 ——
    UFUNCTION(BlueprintCallable, Category = "Torpedo Weapon")
    void StartWarmup();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Torpedo Weapon")
    float GetWarmupProgress() const;

    // —— 跃迁发射 ——
    UFUNCTION(BlueprintCallable, Category = "Torpedo Weapon")
    void FireWarpTorpedo(AActor* Target);

    // —— 弹药操作 ——
    UFUNCTION(BlueprintCallable, Category = "Torpedo Weapon")
    void StartReload();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Torpedo Weapon")
    float GetAmmoPercent() const;

    // —— 事件 ——
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTorpedoWarmupComplete, int32, SlotIndex);
    UPROPERTY(BlueprintAssignable, Category = "Torpedo Weapon|Events")
    FOnTorpedoWarmupComplete OnWarmupComplete;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTorpedoDetonated, int32, SlotIndex, FVector, Location);
    UPROPERTY(BlueprintAssignable, Category = "Torpedo Weapon|Events")
    FOnTorpedoDetonated OnTorpedoDetonated;

    // —— 网络复制 ——
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

private:
    void ProcessHeavyTorpedoFire(int32 SlotIndex, AActor* Target);
    void ProcessDevastatorFire(int32 SlotIndex, AActor* Target);
    void ProcessNuclearFire(int32 SlotIndex, AActor* Target);
    void ProcessGuidedFire(int32 SlotIndex, AActor* Target);
    void ProcessShatterFire(int32 SlotIndex, AActor* Target);
    void UpdateLocking(float Dt);
    void UpdateWarmup(float Dt);
    void SpawnTorpedoProjectile(int32 SlotIndex, AActor* Target, float DamageMult);

    FTimerHandle ReloadTimerHandle;
    FTimerHandle WarmupTimerHandle;
    FTimerHandle LockTimerHandle;
};
