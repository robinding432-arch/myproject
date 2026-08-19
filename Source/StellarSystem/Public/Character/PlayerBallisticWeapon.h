// ============================================================
// 路径: Source/StellarSystem/Public/Character/PlayerBallisticWeapon.h
// 作用: 玩家实弹武器（手枪/冲锋枪/步枪/狙击/霰弹/机枪 6 种细分）
// 依赖: PlayerWeaponBase.h
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "PlayerWeaponBase.h"
#include "PlayerBallisticWeapon.generated.h"

// 实弹细分
UENUM(BlueprintType)
enum class EBallisticSubtype : uint8
{
    Pistol          UMETA(DisplayName = "Pistol (手枪)"),
    SMG             UMETA(DisplayName = "SMG (冲锋枪)"),
    AssaultRifle    UMETA(DisplayName = "Assault Rifle (突击步枪)"),
    SniperRifle     UMETA(DisplayName = "Sniper Rifle (狙击枪)"),
    Shotgun         UMETA(DisplayName = "Shotgun (霰弹枪)"),
    LMG             UMETA(DisplayName = "LMG (轻机枪)")
};

// 实弹专用弹道属性
USTRUCT(BlueprintType)
struct FBallisticProjectileParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Caliber = 7.62f;              // 口径 mm

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ProjectileMass = 0.01f;       // 弹丸质量 kg

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DragCoefficient = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float GravityEffect = 981.f;         // cm/s²（行星表面）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float StabilityFactor = 0.85f;       // 飞行稳定性

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsSubsonic = false;           // 亚音速（消音友好）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SoundSuppression = 0.f;       // 消音等级 0-1
};

// 实弹特殊弹药效果
USTRUCT(BlueprintType)
struct FBallisticSpecialAmmo
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bHollowPoint = false;           // 空尖弹：高伤害低穿甲

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HollowPointDamageBonus = 0.4f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HollowPointArmorPenalty = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bArmorPiercing = false;        // 穿甲弹：低伤害高穿甲

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float APPenaltyDamage = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float APPenaltyArmorBonus = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bTracer = false;               // 曳光弹

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor TracerColor = FLinearColor(1.f, 0.8f, 0.2f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIncendiary = false;           // 燃烧弹

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BurnDamagePerSec = 5.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BurnDuration = 3.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bExplosiveTip = false;         // 爆裂弹头

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ExplosiveTipRadius = 50.f;
};

UCLASS(ClassGroup=(Character|Weapons), meta=(BlueprintSpawnableComponent))
class UPlayerBallisticWeaponComponent : public UPlayerWeaponBaseComponent
{
    GENERATED_BODY()

public:
    UPlayerBallisticWeaponComponent();

    virtual void TickComponent(float Dt, ELevelTick Tick, FActorComponentTickFunction* Fn) override;
    virtual void BeginPlay() override;

    // —— 子类类型 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ballistic")
    EBallisticSubtype BallisticType = EBallisticSubtype::Pistol;

    // —— 弹道 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ballistic|Projectile")
    FBallisticProjectileParams ProjectileParams;

    // —— 特殊弹药 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ballistic|Ammo")
    FBallisticSpecialAmmo SpecialAmmo;

    // —— 弹道下坠补偿（狙击用）——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ballistic|Compensation")
    bool bHasRangefinder = false;       // 是否有测距仪

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ballistic|Compensation")
    bool bHasBulletDropCompensation = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ballistic|Compensation")
    float BulletDropIndicator = 0.f;    // 下坠指示（密位）

    // —— 霰弹专用 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ballistic|Shotgun")
    int32 ShotgunPelletCount = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ballistic|Shotgun")
    float ShotgunSpread = 12.f;         // 霰弹散射角

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ballistic|Shotgun")
    float ShotgunRange = 1500.f;        // 霰弹有效射程（很短）

    // —— 狙击专用 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ballistic|Sniper")
    bool bHasSuppressor = false;        // 消音器

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ballistic|Sniper")
    float SuppressorSoundReduction = 0.7f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ballistic|Sniper")
    bool bHasBipod = false;            // 双脚架

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ballistic|Sniper")
    float BipodAccuracyBonus = 0.8f;    // 架枪精度提升

    // —— 机枪专用 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ballistic|LMG")
    bool bHasBeltFeed = false;          // 弹链供弹

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ballistic|LMG")
    int32 BeltCapacity = 100;           // 弹链容量

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ballistic|LMG")
    float OverheatThreshold = 60.f;     // 过热阈值（秒）

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ballistic|LMG")
    float CurrentHeat = 0.f;

    // —— 开火 ——
    virtual void FireWeapon() override;
    virtual bool CanFire() const override;

    // —— 弹道计算 ——
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ballistic")
    FVector CalculateBulletDrop(const FVector& StartPos, const FVector& Direction, float Distance) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ballistic")
    float GetTimeToTarget(float Distance) const;

    // —— 消音 ——
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ballistic")
    float GetNoiseLevel() const;

    // —— 过热 ——
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ballistic|LMG")
    float GetHeatPercent() const;

    // —— 网络复制 ——
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

private:
    void FirePistolShot();
    void FireSMGBurst();
    void FireRifleShot();
    void FireSniperShot();
    void FireShotgunShell();
    void FireLMGBurst();
    void SpawnBallisticProjectile(const FVector& Origin, const FVector& Direction, float DamageMult);
    void UpdateHeat(float Dt);

    FTimerHandle BurstTimerHandle;
    int32 BurstRemaining = 0;
};
