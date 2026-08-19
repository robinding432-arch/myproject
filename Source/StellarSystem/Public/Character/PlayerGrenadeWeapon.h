// ============================================================
// 路径: Source/StellarSystem/Public/Character/PlayerGrenadeWeapon.h
// 作用: 玩家手雷武器（破片/电磁/烟雾/燃烧/冷冻 5 种细分）
// 依赖: PlayerWeaponBase.h
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "PlayerWeaponBase.h"
#include "PlayerGrenadeWeapon.generated.h"

class UParticleSystem;
class USoundBase;

// 手雷细分
UENUM(BlueprintType)
enum class EGrenadeSubtype : uint8
{
    Frag        UMETA(DisplayName = "Frag Grenade (破片)"),
    EMP         UMETA(DisplayName = "EMP Grenade (电磁)"),
    Smoke       UMETA(DisplayName = "Smoke Grenade (烟雾)"),
    Incendiary  UMETA(DisplayName = "Incendiary (燃烧)"),
    Cryo        UMETA(DisplayName = "Cryo Grenade (冷冻)")
};

// 投掷参数
USTRUCT(BlueprintType)
struct FGrenadeThrowParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ThrowForce = 60000.f;           // 投掷力 cm/s

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxThrowForce = 100000.f;       // 最大蓄力投掷力

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ChargeTime = 1.5f;              // 蓄力时间

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FuseTime = 3.f;                // 引信时间 秒

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bCanCook = true;                // 能否拔销预爆

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CookWarningTime = 0.5f;       // 即将爆炸警告

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BounceDamping = 0.3f;        // 弹跳衰减

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bSticky = false;               // 是否黏附表面
};

// 手雷效果参数
USTRUCT(BlueprintType)
struct FGrenadeEffectParams
{
    GENERATED_BODY()

    // —— 破片 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ExplosionDamage = 80.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ExplosionRadius = 400.f;       // cm

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 FragmentCount = 24;            // 破片数

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FragmentSpreadAngle = 360.f;  // 散射角

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FragmentRange = 600.f;        // 破片飞行距离

    // —— EMP ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EMPRadius = 500.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EMPDuration = 5.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bDisableShields = true;        // 瘫痪护盾

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bDisableElectronics = false;    // 瘫痪电子设备

    // —— 烟雾 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SmokeRadius = 600.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SmokeDuration = 15.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ConcealmentLevel = 0.8f;     // 隐蔽等级 0-1

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bBlocksInfrared = false;       // 阻挡红外

    // —— 燃烧 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FireDamagePerSec = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FireDuration = 8.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FireSpreadRadius = 200.f;     // 火焰蔓延

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bCreatesFireZone = true;       // 创造持续燃烧区

    // —— 冷冻 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CryoDamage = 30.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CryoRadius = 350.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CryoDuration = 6.f;          // 冻结持续时间

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CryoMoveSlow = 0.5f;        // 移速降低 50%

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CryoFireRateSlow = 0.3f;     // 射速降低
};

UCLASS(ClassGroup=(Character|Weapons), meta=(BlueprintSpawnableComponent))
class UPlayerGrenadeWeaponComponent : public UPlayerWeaponBaseComponent
{
    GENERATED_BODY()

public:
    UPlayerGrenadeWeaponComponent();

    virtual void TickComponent(float Dt, ELevelTick Tick, FActorComponentTickFunction* Fn) override;
    virtual void BeginPlay() override;

    // —— 子类类型 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grenade")
    EGrenadeSubtype GrenadeType = EGrenadeSubtype::Frag;

    // —— 投掷参数 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grenade|Throw")
    FGrenadeThrowParams ThrowParams;

    // —— 效果 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grenade|Effect")
    FGrenadeEffectParams EffectParams;

    // —— 库存 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grenade|Inventory")
    int32 MaxCarryCount = 6;            // 最大携带数量

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grenade|Inventory")
    int32 CurrentCount = 6;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grenade|Inventory")
    FName GrenadeAmmoID = FName("GrenadeFrag");

    // —— 视觉效果 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grenade|VFX")
    TSoftObjectPtr<UStaticMesh> GrenadeMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grenade|VFX")
    TSoftObjectPtr<UParticleSystem> ExplosionEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grenade|VFX")
    TSoftObjectPtr<UParticleSystem> SmokeEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grenade|VFX")
    TSoftObjectPtr<UParticleSystem> FireEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grenade|VFX")
    TSoftObjectPtr<UParticleSystem> CryoEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grenade|VFX")
    TSoftObjectPtr<UParticleSystem> EMPEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grenade|VFX")
    TSoftObjectPtr<USoundBase> ThrowSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grenade|VFX")
    TSoftObjectPtr<USoundBase> ExplosionSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grenade|VFX")
    TSoftObjectPtr<USoundBase> FuseTickSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grenade|VFX")
    FLinearColor GrenadeGlowColor = FLinearColor(1.f, 0.3f, 0.1f, 1.f);

    // —— 开火（投掷）——
    virtual void FireWeapon() override;
    virtual bool CanFire() const override;

    // —— 蓄力投掷 ——
    UFUNCTION(BlueprintCallable, Category = "Grenade")
    void StartChargedThrow();

    UFUNCTION(BlueprintCallable, Category = "Grenade")
    void ReleaseChargedThrow();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Grenade")
    float GetChargeProgress() const;

    // —— 拔销预爆 ——
    UFUNCTION(BlueprintCallable, Category = "Grenade")
    void CookGrenade();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Grenade")
    float GetCookTimeRemaining() const;

    // —— 库存管理 ——
    UFUNCTION(BlueprintCallable, Category = "Grenade")
    void AddGrenades(int32 Amount);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Grenade")
    int32 GetGrenadeCount() const { return CurrentCount; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Grenade")
    float GetGrenadePercent() const;

    // —— 切换类型 ——
    UFUNCTION(BlueprintCallable, Category = "Grenade")
    void SwitchGrenadeType(EGrenadeSubtype NewType);

    // —— 网络复制 ——
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

private:
    void SpawnGrenadeActor(const FVector& Origin, const FVector& Velocity, float FuseTime);
    void ProcessFragGrenade();
    void ProcessEMPGrenade();
    void ProcessSmokeGrenade();
    void ProcessIncendiaryGrenade();
    void ProcessCryoGrenade();
    void UpdateCooking(float Dt);

    FTimerHandle CookTimerHandle;
    float CurrentCookTime = 0.f;
    bool bIsCooking = false;
    float CurrentCharge = 0.f;
    bool bIsCharging = false;
};
