// ============================================================
// 路径: Source/StellarSystem/Public/Character/PlayerWeaponBase.h
// 作用: 玩家个人武器基类（5 大子类继承于此）
// 依赖: Character/MyCharacter.h, Inventory/AmmoAndConsumables.h
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerWeaponBase.generated.h"

class AMyCharacter;
class UAmmoInventoryComponent;
class UParticleSystem;
class USoundBase;

// 玩家武器大类（5 种）
UENUM(BlueprintType)
enum class EPlayerWeaponCategory : uint8
{
    Ballistic   UMETA(DisplayName = "Ballistic (实弹)"),
    Energy      UMETA(DisplayName = "Energy (能量)"),
    Grenade     UMETA(DisplayName = "Grenade (手雷)"),
    Bow         UMETA(DisplayName = "Bow (弓弩)"),
    Rocket      UMETA(DisplayName = "Rocket (火箭弹)")
};

// 玩家武器细分类型
UENUM(BlueprintType)
enum class EPlayerWeaponSubtype : uint8
{
    // —— 实弹 ——
    Pistol          UMETA(DisplayName = "Pistol (手枪)"),
    SMG             UMETA(DisplayName = "SMG (冲锋枪)"),
    AssaultRifle    UMETA(DisplayName = "Assault Rifle (突击步枪)"),
    SniperRifle     UMETA(DisplayName = "Sniper Rifle (狙击枪)"),
    Shotgun         UMETA(DisplayName = "Shotgun (霰弹枪)"),
    LMG             UMETA(DisplayName = "LMG (轻机枪)"),
    // —— 能量 ——
    LaserPistol     UMETA(DisplayName = "Laser Pistol (激光手枪)"),
    PlasmaRifle     UMETA(DisplayName = "Plasma Rifle (等离子步枪)"),
    BeamRifle       UMETA(DisplayName = "Beam Rifle (光束步枪)"),
    IonBlaster      UMETA(DisplayName = "Ion Blaster (离子冲击)"),
    // —— 手雷 ——
    FragGrenade     UMETA(DisplayName = "Frag Grenade (破片)"),
    EMPGrenade      UMETA(DisplayName = "EMP Grenade (电磁)"),
    SmokeGrenade    UMETA(DisplayName = "Smoke Grenade (烟雾)"),
    IncendiaryGrenade UMETA(DisplayName = "Incendiary (燃烧)"),
    CryoGrenade     UMETA(DisplayName = "Cryo Grenade (冷冻)"),
    // —— 弓弩 ——
    ShortBow        UMETA(DisplayName = "Short Bow (短弓)"),
    LongBow         UMETA(DisplayName = "Long Bow (长弓)"),
    Crossbow        UMETA(DisplayName = "Crossbow (十字弩)"),
    AutoCrossbow    UMETA(DisplayName = "Auto Crossbow (连弩)"),
    CompoundBow     UMETA(DisplayName = "Compound Bow (复合弓)"),
    // —— 火箭弹 ——
    RPG             UMETA(DisplayName = "RPG (火箭筒)"),
    MicroMissile    UMETA(DisplayName = "Micro Missile (微导弹)"),
    SwarmRocket     UMETA(DisplayName = "Swarm Rocket (蜂群)"),
    GuidedRocket    UMETA(DisplayName = "Guided Rocket (制导)"),
    DualRocket      UMETA(DisplayName = "Dual Rocket (双发)")
};

// 武器开火模式
UENUM(BlueprintType)
enum class EFireMode : uint8
{
    SemiAuto    UMETA(DisplayName = "Semi-Auto"),
    FullAuto    UMETA(DisplayName = "Full Auto"),
    Burst2      UMETA(DisplayName = "Burst (2-round)"),
    Burst3      UMETA(DisplayName = "Burst (3-round)"),
    Charged     UMETA(DisplayName = "Charged Shot"),
    Held        UMETA(DisplayName = "Hold to Fire")
};

// 武器配件槽
UENUM(BlueprintType)
enum class EWeaponAttachmentSlot : uint8
{
    Sight       UMETA(DisplayName = "Optic Sight"),
    Barrel      UMETA(DisplayName = "Barrel Mod"),
    Underbarrel UMETA(DisplayName = "Underbarrel"),
    Magazine    UMETA(DisplayName = "Magazine"),
    Stock       UMETA(DisplayName = "Stock"),
    Muzzle      UMETA(DisplayName = "Muzzle Device")
};

// 武器配件数据
USTRUCT(BlueprintType)
struct FWeaponAttachment
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName AttachmentID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EWeaponAttachmentSlot Slot = EWeaponAttachmentSlot::Sight;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DamageModifier = 0.f;          // 伤害增减

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AccuracyModifier = 0.f;        // 精度增减（度）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RecoilModifier = 0.f;         // 后坐力增减

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float WeightModifier = 0.f;          // 重量增减

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FireRateModifier = 0.f;       // 射速增减（发/分钟）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bAddsAutoFire = false;         // 是否增加全自动

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Description;
};

// 武器状态
UENUM(BlueprintType)
enum class EWeaponState : uint8
{
    Idle,
    Firing,
    Reloading,
    Charging,
    Overheated,
    OutOfAmmo,
    Jammed
};

// 武器基础数据
USTRUCT(BlueprintType)
struct FPlayerWeaponData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName WeaponID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EPlayerWeaponCategory Category = EPlayerWeaponCategory::Ballistic;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EPlayerWeaponSubtype Subtype = EPlayerWeaponSubtype::Pistol;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EFireMode FireMode = EFireMode::SemiAuto;

    // —— 伤害 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BaseDamage = 25.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ArmorPierce = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CriticalMultiplier = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CriticalChance = 0.05f;

    // —— 射速/弹道 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FireRate = 300.f;             // 发/分钟

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MuzzleVelocity = 80000.f;     // cm/s

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EffectiveRange = 30000.f;      // cm

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxRange = 60000.f;           // cm

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SpreadBase = 1.0f;           // 基础散射（度）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SpreadMax = 5.0f;            // 最大散射（度）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SpreadGrowthPerShot = 0.3f;   // 每发增长

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SpreadRecoveryPerSec = 2.0f;  // 每秒恢复

    // —— 后坐力 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RecoilKick = 0.5f;           // 后坐力强度

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RecoilPattern_Randomness = 0.3f;

    // —— 弹药 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName AmmoTypeID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MagazineSize = 15;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ReserveAmmo = 90;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ReloadTime = 2.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 PelletCount = 1;              // 霰弹/蜂群数量

    // —— 特殊 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsExplosive = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ExplosionRadius = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsIncendiary = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BurnDuration = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsEMP = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EMPDuration = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsCryo = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CryoDuration = 0.f;

    // —— 蓄力 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bHasChargeShot = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxChargeTime = 2.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ChargeDamageMultiplier = 3.f;

    // —— 重量/手感 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float WeaponWeight = 2.5f;         // kg

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EquipSpeed = 0.5f;           // 装备速度（秒）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AimDownSightSpeed = 0.3f;    // 开镜速度（秒）

    // —— 配件 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<EWeaponAttachmentSlot> AvailableSlots;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<EWeaponAttachmentSlot, FWeaponAttachment> InstalledAttachments;

    // —— 视觉 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<UStaticMesh> WeaponMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<UParticleSystem> MuzzleFlash;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<UParticleSystem> TracerEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<UParticleSystem> ImpactEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<USoundBase> FireSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<USoundBase> ReloadSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<USoundBase> DryFireSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor TracerColor = FLinearColor(1.f, 0.8f, 0.3f, 1.f);

    // —— 稀有度 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 QualityLevel = 1;             // 1-5

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Condition = 100.f;            // 0-100 耐久
};

// 武器基类组件
UCLASS(ClassGroup=(Character|Weapons), meta=(BlueprintSpawnableComponent))
class UPlayerWeaponBaseComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPlayerWeaponBaseComponent();

    virtual void TickComponent(float Dt, ELevelTick Tick, FActorComponentTickFunction* Fn) override;
    virtual void BeginPlay() override;

    // —— 武器数据 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Data")
    FPlayerWeaponData WeaponData;

    // —— 运行时状态 ——
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|State")
    EWeaponState CurrentState = EWeaponState::Idle;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|State")
    int32 CurrentMagAmmo = 15;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|State")
    int32 CurrentReserveAmmo = 90;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|State")
    float CurrentSpread = 1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|State")
    float CurrentCharge = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|State")
    bool bIsFiring = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|State")
    bool bIsAiming = false;

    // —— 开火控制 ——
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    virtual void StartFire();

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    virtual void StopFire();

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    virtual void FireWeapon();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon")
    virtual bool CanFire() const;

    // —— 装填 ——
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    virtual void StartReload();

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    virtual void FinishReload();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon")
    bool IsReloading() const { return CurrentState == EWeaponState::Reloading; }

    // —— 瞄准 ——
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void SetAiming(bool bAiming);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon")
    float GetCurrentSpread() const { return CurrentSpread; }

    // —— 蓄力 ——
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void StartCharging();

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void ReleaseCharge();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon")
    float GetChargeProgress() const;

    // —— 配件 ——
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    bool InstallAttachment(const FWeaponAttachment& Attachment);

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    bool RemoveAttachment(EWeaponAttachmentSlot Slot);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon")
    FWeaponAttachment GetInstalledAttachment(EWeaponAttachmentSlot Slot) const;

    // —— 伤害计算 ——
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon|Damage")
    float CalculateDamage(float Distance) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon|Damage")
    float GetEffectiveDamage() const;

    // —— 网络 ——
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerFire(FVector Origin, FVector Direction);

    UFUNCTION(Server, Reliable, WithValidation)
    void ServerStartReload();

    UFUNCTION(Server, Reliable, WithValidation)
    void ServerConsumeAmmo(int32 Amount);

    // —— 事件 ——
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWeaponFired);
    UPROPERTY(BlueprintAssignable, Category = "Weapon|Events")
    FOnWeaponFired OnWeaponFired;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnReloadFinished, bool, bSuccess);
    UPROPERTY(BlueprintAssignable, Category = "Weapon|Events")
    FOnReloadFinished OnReloadFinished;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOutOfAmmo, FName, WeaponID);
    UPROPERTY(BlueprintAssignable, Category = "Weapon|Events")
    FOnOutOfAmmo OnOutOfAmmo;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeaponJammed, FName, WeaponID);
    UPROPERTY(BlueprintAssignable, Category = "Weapon|Events")
    FOnWeaponJammed OnWeaponJammed;

    // —— 网络复制 ——
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

protected:
    // 内部方法（子类重写）
    virtual void SpawnProjectile(const FVector& Origin, const FVector& Direction, float Damage);
    virtual void ApplyRecoil();
    virtual void UpdateSpread(float Dt);
    virtual void ProcessFireRate(float Dt);

    // 引用
    UPROPERTY()
    AMyCharacter* OwnerCharacter = nullptr;

    UPROPERTY()
    UAmmoInventoryComponent* AmmoComp = nullptr;

    // 内部状态
    float TimeSinceLastShot = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Internal")
    float FireInterval = 0.2f; // 60 RPM 默认
    bool bWantsToFire = false;

    // 卡壳
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Malfunction")
    float JamChance = 0.001f;            // 每发卡壳概率

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Malfunction")
    float ConditionDecayPerShot = 0.05f; // 每发耐久损耗

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Malfunction")
    float MinConditionForUse = 10.f;     // 最低可用耐久

    FTimerHandle ReloadTimerHandle;
    FTimerHandle ChargeTimerHandle;
};
