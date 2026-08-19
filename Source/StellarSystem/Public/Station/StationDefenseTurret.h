// ============================================================
// 路径: Source/StellarSystem/Public/Station/StationDefenseTurret.h
// 模块: Station (空间站)
// 类型: 头文件
// 作用: 主权港防御炮塔系统 — 可升级的自动防御武器
//       支持 Laser/Flak/Missile 三种炮塔类型
//       与 PlayerOwnedStation 的升级系统深度集成
// 新增于: v7.6.1
// 依赖: PlayerOwnedStation.h
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StationDefenseTurret.generated.h"

class AShipPawn;
class APlayerOwnedStation;
class UParticleSystem;
class USoundBase;
class UStaticMeshComponent;
class USphereComponent;

// 炮塔类型
UENUM(BlueprintType)
enum class ETurretType : uint8
{
    Laser       UMETA(DisplayName = "激光炮塔 (Laser)"),
    Flak        UMETA(DisplayName = "防空机炮 (Flak)"),
    Missile     UMETA(DisplayName = "导弹发射器 (Missile)"),
    Beam        UMETA(DisplayName = "光束炮塔 (Beam)"),
    Plasma      UMETA(DisplayName = "等离子炮塔 (Plasma)")
};

// 炮塔升级等级
UENUM(BlueprintType)
enum class ETurretLevel : uint8
{
    None        UMETA(DisplayName = "未安装"),
    Basic       UMETA(DisplayName = "基础 (1级)"),
    Improved    UMETA(DisplayName = "改进 (2级)"),
    Advanced    UMETA(DisplayName = "高级 (3级)"),
    Elite       UMETA(DisplayName = "精英 (4级)"),
    Apex        UMETA(DisplayName = "巅峰 (5级)")
};

// 炮塔单次射击数据
USTRUCT(BlueprintType)
struct FTurretShotData
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FVector FireLocation = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FVector TargetLocation = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float Damage = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float Timestamp = 0.f;
};

// 炮塔升级成本
USTRUCT(BlueprintType)
struct FTurretUpgradeCost
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Upgrade")
    int32 Credits = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Upgrade")
    int32 Titanium = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Upgrade")
    int32 QuantumCore = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Upgrade")
    int32 Electronics = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Upgrade")
    int32 WeaponComponents = 0;
};

// 炮塔战斗属性
USTRUCT(BlueprintType)
struct FTurretCombatStats
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Combat")
    float DamagePerShot = 50.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Combat")
    float FireRate = 120.f;               // 发/分钟

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Combat")
    float Range = 50000.f;                // 射程 (cm)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Combat")
    float Accuracy = 0.85f;              // 命中率 0~1

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Combat")
    float TurnSpeed = 90.f;              // 转向速度 (deg/s)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Combat")
    float ShieldDamageMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Combat")
    float HullDamageMultiplier = 0.7f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Combat")
    bool bCanTargetShips = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Combat")
    bool bCanTargetMissiles = false;      // Flak 专用

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Combat")
    int32 AmmoCapacity = -1;             // -1 = 无限

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Combat")
    float ReloadTime = 0.f;
};

/**
 * 主权港防御炮塔
 *
 * 功能：
 * - 5 种炮塔类型（Laser/Flak/Missile/Beam/Plasma）
 * - 5 级升级（Basic → Apex），每级提升伤害/射速/射程
 * - 自动索敌（飞船/导弹），服务端权威判定
 * - 与 PlayerOwnedStation 绑定（所属太空港）
 * - 炮塔被毁可维修/重建
 * - 升级消耗资源（信用点+钛+量子核心+电子元件+武器部件）
 * - 炮塔过热管理
 * - 弹药补给（非无限炮塔）
 */
UCLASS(BlueprintType, Blueprintable)
class AStationDefenseTurret : public AActor
{
    GENERATED_BODY()

public:
    AStationDefenseTurret();

    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;

    // ========== 基础属性 ==========

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Turret")
    FName TurretID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Turret")
    ETurretType TurretType = ETurretType::Laser;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Turret")
    ETurretLevel CurrentLevel = ETurretLevel::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Turret")
    bool bIsInstalled = false;

    // ========== 战斗属性 ==========

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Turret|Combat")
    FTurretCombatStats CombatStats;

    // ========== 炮塔状态 ==========

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Turret|Status")
    float CurrentHealth = 500.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Status")
    float MaxHealth = 500.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Turret|Status")
    bool bIsDestroyed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Turret|Status")
    bool bIsRepairing = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Turret|Status")
    float RepairProgress = 0.f;              // 0~1

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Status")
    float RepairTime = 60.f;                 // 修复所需时间(秒)

    // ========== 索敌 ==========

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Turret|Targeting")
    AActor* CurrentTarget = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Turret|Targeting")
    float TargetLockProgress = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Targeting")
    float LockTime = 1.5f;                  // 锁定所需时间

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Targeting")
    float ScanInterval = 0.5f;              // 扫描间隔

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Targeting")
    TArray<TEnumAsByte<ECollisionChannel>> TargetChannels;

    // ========== 过热 ==========

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Turret|Heat")
    float CurrentHeat = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Heat")
    float MaxHeat = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Heat")
    float HeatPerShot = 3.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Heat")
    float HeatDissipationRate = 8.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Turret|Heat")
    bool bOverheated = false;

    // ========== 弹药 ==========

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Turret|Ammo")
    int32 CurrentAmmo = -1;                 // -1 = 无限

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Ammo")
    bool bInfiniteAmmo = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Ammo")
    float ResupplyTime = 30.f;              // 弹药补给时间

    // ========== 视觉效果 ==========

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|VFX")
    TSoftObjectPtr<UStaticMesh> TurretMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|VFX")
    TSoftObjectPtr<UStaticMesh> BarrelMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|VFX")
    TSoftObjectPtr<UParticleSystem> MuzzleFlash;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|VFX")
    TSoftObjectPtr<UParticleSystem> TracerEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|VFX")
    TSoftObjectPtr<UParticleSystem> DestroyedEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|VFX")
    TSoftObjectPtr<USoundBase> FireSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|VFX")
    TSoftObjectPtr<USoundBase> DestroyedSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|VFX")
    TSoftObjectPtr<USoundBase> OverheatSound;

    // ========== 绑定 ==========

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Turret|Owner")
    FName OwningStationID = NAME_None;       // 所属太空港 ID

    // ========== 升级系统 ==========

    /** 获取下一级升级成本 */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Turret|Upgrade")
    FTurretUpgradeCost GetUpgradeCost() const;

    /** 检查是否可以升级 */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Turret|Upgrade")
    bool CanUpgrade() const;

    /** 执行升级（服务端） */
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Turret|Upgrade")
    void Server_UpgradeTurret(AController* RequestedBy);

    /** 安装炮塔（从无到有） */
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Turret|Upgrade")
    void Server_InstallTurret(AController* RequestedBy, ETurretType Type);

    /** 卸载炮塔（回收部分资源） */
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Turret|Upgrade")
    void Server_UninstallTurret(AController* RequestedBy);

    // ========== 战斗 ==========

    /** 手动开火（如果自动模式关闭） */
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Turret|Combat")
    void Server_FireTurret();

    /** 设置自动开火模式 */
    UFUNCTION(BlueprintCallable, BlueprintReadWrite, Category = "Turret|Combat")
    bool bAutoFire = true;

    /** 设置炮塔优先级目标类型 */
    UFUNCTION(BlueprintCallable, BlueprintReadWrite, Category = "Turret|Combat")
    TArray<FName> PriorityTargetTags;

    // ========== 维修 ==========

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Turret|Repair")
    bool CanRepair() const { return bIsDestroyed && !bIsRepairing; }

    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Turret|Repair")
    void Server_StartRepair(AController* RequestedBy);

    // ========== 查询 ==========

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Turret|Query")
    float GetHealthPercent() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Turret|Query")
    float GetHeatPercent() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Turret|Query")
    int32 GetCurrentLevelNumber() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Turret|Query")
    FString GetTurretStatusString() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Turret|Query")
    TArray<AActor*> GetHostileTargetsInRange() const;

    // ========== 事件 ==========

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTurretFired, const FTurretShotData&, ShotData);
    UPROPERTY(BlueprintAssignable, Category = "Turret|Events")
    FOnTurretFired OnTurretFired;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTurretDestroyed, AStationDefenseTurret*, Turret);
    UPROPERTY(BlueprintAssignable, Category = "Turret|Events")
    FOnTurretDestroyed OnTurretDestroyed;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTurretUpgraded, AStationDefenseTurret*, Turret, int32, NewLevel);
    UPROPERTY(BlueprintAssignable, Category = "Turret|Events")
    FOnTurretUpgraded OnTurretUpgraded;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTurretRepaired, AStationDefenseTurret*, Turret);
    UPROPERTY(BlueprintAssignable, Category = "Turret|Events")
    FOnTurretRepaired OnTurretRepaired;

    // ========== 网络 ==========

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

private:
    // 内部
    void ScanForTargets();
    void AcquireTarget(AActor* Target);
    void FireAtTarget();
    void ProcessAutoFire(float Dt);
    void UpdateHeat(float Dt);
    void UpdateRepair(float Dt);
    void ApplyDamageToTarget(AActor* Target, float Damage);
    bool IsValidTarget(AActor* Target) const;
    float CalculateDamageFalloff(float Distance) const;
    void HandleTurretDestroyed();
    void PlayFireEffects();
    void PlayDestroyedEffects();

    // 计时器
    FTimerHandle ScanTimerHandle;
    FTimerHandle FireTimerHandle;
    FTimerHandle RepairTimerHandle;

    // 运行时
    float FireCooldownRemaining = 0.f;
    float ScanTimer = 0.f;
    TArray<AActor*> RecentTargets;

    // 炮塔网格（运行时创建）
    UPROPERTY()
    UStaticMeshComponent* TurretMeshComponent = nullptr;

    UPROPERTY()
    UStaticMeshComponent* BarrelMeshComponent = nullptr;

    UPROPERTY()
    USphereComponent* DetectionSphere = nullptr;
};
