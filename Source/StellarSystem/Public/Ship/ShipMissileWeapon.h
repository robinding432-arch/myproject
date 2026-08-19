// ============================================================
// 路径: Source/StellarSystem/Public/Ship/ShipMissileWeapon.h
// 作用: 飞船导弹武器（Heatseeker/Swarm/Cluster/Dumbfire 四类细分）
// 依赖: ShipWeaponBase.h
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "ShipWeaponBase.h"
#include "ShipMissileWeapon.generated.h"

class UParticleSystem;
class USoundBase;

// 导弹细分类型
UENUM(BlueprintType)
enum class EShipMissileSubtype : uint8
{
    Heatseeker  UMETA(DisplayName = "Heatseeker (IR Lock)"),
    Swarm       UMETA(DisplayName = "Swarm Missile (Multi)"),
    Cluster     UMETA(DisplayName = "Cluster Missile (Burst)"),
    Dumbfire    UMETA(DisplayName = "Dumbfire (Line)"),
    Flak        UMETA(DisplayName = "Flak Missile (AoE)")
};

// 导弹飞行参数
USTRUCT(BlueprintType)
struct FShipMissileFlightParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ThrustAcceleration = 80000.f;   // 加速度 cm/s²

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxSpeed = 60000.f;             // 最大速度 cm/s

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TurnRate = 45.f;                // 转弯速率 度/秒

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FuelDuration = 8.f;             // 燃料持续时间 秒

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ArmDistance = 300.f;            // 保险距离 cm

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DetonationDelay = 0.1f;         // 触发延迟

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bCanRetarget = false;            // 能否中途换目标

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RetargetRange = 0.f;            // 换目标范围
};

// 导弹战斗部参数
USTRUCT(BlueprintType)
struct FShipMissileWarhead
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ExplosionDamage = 80.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ExplosionRadius = 500.f;        // cm

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ArmorPierce = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ShieldDamageMultiplier = 1.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HullDamageMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsEMP = false;                  // 电磁脉冲弹头

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EMPDuration = 3.f;              // EMP 持续时间

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsFrag = false;                // 破片弹

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 FragCount = 12;                // 破片数量

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FragSpreadAngle = 30.f;        // 破片散射角
};

UCLASS(ClassGroup=(Ship|Weapons), meta=(BlueprintSpawnableComponent))
class UShipMissileWeaponComponent : public UShipWeaponBaseComponent
{
    GENERATED_BODY()

public:
    UShipMissileWeaponComponent();

    virtual void TickComponent(float Dt, ELevelTick Tick, FActorComponentTickFunction* Fn) override;
    virtual void BeginPlay() override;

    // —— 子类类型 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile Weapon")
    EShipMissileSubtype MissileSubtype = EShipMissileSubtype::Heatseeker;

    // —— 飞行参数 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile Weapon")
    FShipMissileFlightParams FlightParams;

    // —— 战斗部 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile Weapon")
    FShipMissileWarhead Warhead;

    // —— 锁定系统 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile Weapon|Lock")
    bool bRequiresLock = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile Weapon|Lock")
    float LockTime = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile Weapon|Lock")
    float LockConeAngle = 15.f;          // 锁定锥角 度

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile Weapon|Lock")
    float MaxLockRange = 30000.f;        // 最大锁定距离 cm

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile Weapon|Lock")
    bool bCanLockOnDecoy = false;        // 能否锁定诱饵

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Missile Weapon|Lock")
    float CurrentLockProgress = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Missile Weapon|Lock")
    bool bLocked = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Missile Weapon|Lock")
    AActor* CurrentTarget = nullptr;

    // —— 弹药管理 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile Weapon")
    int32 MagazineSize = 12;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Missile Weapon")
    int32 CurrentAmmo = 12;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile Weapon")
    float ReloadTime = 5.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Missile Weapon")
    bool bIsReloading = false;

    // —— Swarm 专用 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile Weapon|Swarm")
    int32 SwarmCount = 4;                // 一次发射数量

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile Weapon|Swarm")
    float SwarmSpreadAngle = 8.f;        // 散射角

    // —— Cluster 专用 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile Weapon|Cluster")
    int32 ClusterBurstCount = 6;         // 分裂数量

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile Weapon|Cluster")
    float ClusterBurstDistance = 2000.f;  // 分裂距离

    // —— Flak 专用 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile Weapon|Flak")
    float FlakAirBurstRadius = 800.f;    // 空爆半径

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile Weapon|Flak")
    float FlakProximityFuzeRange = 300.f; // 近炸引信范围

    // —— 视觉效果 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile Weapon|VFX")
    TSoftObjectPtr<UParticleSystem> MissileTrailEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile Weapon|VFX")
    TSoftObjectPtr<UParticleSystem> ExplosionEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile Weapon|VFX")
    TSoftObjectPtr<USoundBase> LaunchSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile Weapon|VFX")
    TSoftObjectPtr<USoundBase> ExplosionSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile Weapon|VFX")
    TSoftObjectPtr<USoundBase> LockSound;

    // —— 开火 ——
    virtual void FireWeapon(int32 SlotIndex) override;
    virtual bool CanFire() const override;

    // —— 锁定 ——
    UFUNCTION(BlueprintCallable, Category = "Missile Weapon")
    void AcquireTarget(AActor* Target);

    UFUNCTION(BlueprintCallable, Category = "Missile Weapon")
    void ReleaseTarget();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Missile Weapon")
    float GetLockProgress() const { return CurrentLockProgress; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Missile Weapon")
    bool IsLocked() const { return bLocked; }

    // —— 弹药操作 ——
    UFUNCTION(BlueprintCallable, Category = "Missile Weapon")
    void StartReload();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Missile Weapon")
    float GetAmmoPercent() const;

    // —— 反制 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile Weapon|Counter")
    bool bCanBeJammed = true;             // 能否被干扰

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile Weapon|Counter")
    float JamResistance = 0.5f;          // 抗干扰（0-1）

    // —— 网络复制 ——
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

private:
    void ProcessHeatseekerFire(int32 SlotIndex);
    void ProcessSwarmFire(int32 SlotIndex);
    void ProcessClusterFire(int32 SlotIndex);
    void ProcessDumbfireFire(int32 SlotIndex);
    void ProcessFlakFire(int32 SlotIndex);
    void UpdateLocking(float Dt);

    FTimerHandle ReloadTimerHandle;
    FTimerHandle LockTimerHandle;
};
