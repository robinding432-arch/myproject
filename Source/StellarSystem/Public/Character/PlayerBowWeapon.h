// ============================================================
// 路径: Source/StellarSystem/Public/Character/PlayerBowWeapon.h
// 作用: 玩家弓弩武器（短弓/长弓/十字弩/连弩/复合弓 5 种细分）
// 依赖: PlayerWeaponBase.h
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "PlayerWeaponBase.h"
#include "PlayerBowWeapon.generated.h"

class UParticleSystem;
class USoundBase;

// 弓弩细分
UENUM(BlueprintType)
enum class EBowSubtype : uint8
{
    ShortBow      UMETA(DisplayName = "Short Bow (短弓)"),
    LongBow       UMETA(DisplayName = "Long Bow (长弓)"),
    Crossbow      UMETA(DisplayName = "Crossbow (十字弩)"),
    AutoCrossbow  UMETA(DisplayName = "Auto Crossbow (连弩)"),
    CompoundBow   UMETA(DisplayName = "Compound Bow (复合弓)")
};

// 箭矢类型
UENUM(BlueprintType)
enum class EArrowType : uint8
{
    Standard      UMETA(DisplayName = "Standard Arrow (普通箭)"),
    Broadhead     UMETA(DisplayName = "Broadhead (宽刃)"),
    Bodkin        UMETA(DisplayName = "Bodkin (穿甲)"),
    FireArrow     UMETA(DisplayName = "Fire Arrow (火箭)"),
    PoisonArrow   UMETA(DisplayName = "Poison Arrow (毒箭)"),
    CryoArrow     UMETA(DisplayName = "Cryo Arrow (冰箭)"),
    ExplosiveArrow UMETA(DisplayName = "Explosive Arrow (爆箭)"),
    GrapplingHook UMETA(DisplayName = "Grappling Hook (钩索)"),
    SignalArrow   UMETA(DisplayName = "Signal Arrow (信号箭)")
};

// 箭矢参数
USTRUCT(BlueprintType)
struct FArrowParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName ArrowID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EArrowType Type = EArrowType::Standard;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ArrowMass = 0.02f;           // kg

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ArrowLength = 0.75f;          // 米

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BaseDamage = 35.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ArmorPierce = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DragCoefficient = 0.1f;      // 箭矢阻力很小

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float StabilityBonus = 0.95f;      // 箭矢飞行稳定

    // 特殊效果
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FireDamagePerSec = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FireDuration = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PoisonDamagePerSec = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PoisonDuration = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CryoSlowAmount = 0.f;        // 减速幅度

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CryoDuration = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ExplosionRadius = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float GrapplingPullForce = 0.f;    // 钩索拉力

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SignalFlareDuration = 0.f;

    // 视觉
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor FletchingColor = FLinearColor(0.8f, 0.7f, 0.3f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<UStaticMesh> ArrowMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<UParticleSystem> ArrowTrail;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<UParticleSystem> ImpactEffect;
};

// 拉弓参数
USTRUCT(BlueprintType)
struct FBowDrawParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxDrawTime = 1.5f;          // 最大拉弓时间

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MinDrawDamageMult = 0.3f;    // 最低伤害倍率（未拉满）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxDrawDamageMult = 1.0f;    // 满弓伤害倍率

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DrawSpeed = 1.0f;            // 拉弓速度倍率

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bCanHoldFullDraw = true;       // 能否保持满弓

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HoldFullDrawTime = 5.f;       // 满弓保持时间

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DrawFatiguePerSec = 2.f;     // 每秒疲劳值

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxFatigue = 100.f;          // 最大疲劳
};

// 弓弩专属属性
USTRUCT(BlueprintType)
struct FBowWeaponParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BowDrawWeight = 40.f;        // 磅数

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BowstringTension = 1.0f;     // 弓弦张力

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ArrowSpeed_Min = 50000.f;    // 最低箭速 cm/s（未拉满）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ArrowSpeed_Max = 120000.f;   // 最高箭速 cm/s（满弓）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AccuracyBonus = 0.2f;        // 精度加成（弓弩天生精准）

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HeadshotMultiplier = 2.0f;   // 爆头倍率

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bHasScope = false;            // 是否有瞄具

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ScopeMagnification = 4.f;    // 倍率

    // 十字弩专用
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsCrossbow = false;          // 是否十字弩

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ReloadTime = 1.5f;          // 装填时间

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bAutoReload = false;          // 连弩自动装填

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MagazineSize = 1;          // 弹匣（连弩可 >1）

    // 复合弓专用
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsCompound = false;         // 是否复合弓

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LetOffPercent = 0.8f;       // 省力比

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CamSystemEfficiency = 1.3f; // 凸轮系统效率
};

UCLASS(ClassGroup=(Character|Weapons), meta=(BlueprintSpawnableComponent))
class UPlayerBowWeaponComponent : public UPlayerWeaponBaseComponent
{
    GENERATED_BODY()

public:
    UPlayerBowWeaponComponent();

    virtual void TickComponent(float Dt, ELevelTick Tick, FActorComponentTickFunction* Fn) override;
    virtual void BeginPlay() override;

    // —— 子类类型 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bow")
    EBowSubtype BowType = EBowSubtype::ShortBow;

    // —— 弓参数 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bow|Params")
    FBowWeaponParams BowParams;

    // —— 拉弓 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bow|Draw")
    FBowDrawParams DrawParams;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bow|Draw")
    float CurrentDraw = 0.f;          // 0-1 拉弓进度

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bow|Draw")
    bool bIsDrawing = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bow|Draw")
    float CurrentFatigue = 0.f;

    // —— 箭矢管理 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bow|Arrows")
    TMap<EArrowType, FArrowParams> ArrowLibrary;  // 箭矢类型库

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bow|Arrows")
    EArrowType CurrentArrowType = EArrowType::Standard;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bow|Arrows")
    TMap<EArrowType, int32> ArrowInventory;          // 各类箭矢数量

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bow|Arrows")
    int32 CurrentArrowCount = 20;

    // —— 视觉效果 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bow|VFX")
    TSoftObjectPtr<UStaticMesh> BowMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bow|VFX")
    TSoftObjectPtr<UStaticMesh> BowstringMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bow|VFX")
    TSoftObjectPtr<UParticleSystem> DrawEffect;       // 拉弓特效

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bow|VFX")
    TSoftObjectPtr<UParticleSystem> ReleaseEffect;    // 放箭特效

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bow|VFX")
    TSoftObjectPtr<UParticleSystem> ArrowTrail;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bow|VFX")
    TSoftObjectPtr<USoundBase> DrawSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bow|VFX")
    TSoftObjectPtr<USoundBase> ReleaseSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bow|VFX")
    TSoftObjectPtr<USoundBase> ImpactSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bow|VFX")
    FLinearColor BowstringGlow = FLinearColor(0.5f, 0.8f, 1.f, 1.f);

    // —— 开火（射箭）——
    virtual void FireWeapon() override;
    virtual bool CanFire() const override;

    // —— 拉弓控制 ——
    UFUNCTION(BlueprintCallable, Category = "Bow")
    void StartDrawing();

    UFUNCTION(BlueprintCallable, Category = "Bow")
    void ReleaseArrow();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Bow")
    float GetDrawProgress() const { return CurrentDraw; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Bow")
    float GetDamageMultiplier() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Bow")
    float GetArrowSpeed() const;

    // —— 箭矢切换 ——
    UFUNCTION(BlueprintCallable, Category = "Bow")
    void SwitchArrowType(EArrowType NewType);

    UFUNCTION(BlueprintCallable, Category = "Bow")
    void AddArrows(EArrowType Type, int32 Amount);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Bow")
    int32 GetArrowCount(EArrowType Type) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Bow")
    TArray<EArrowType> GetAvailableArrowTypes() const;

    // —— 钩索 ——
    UFUNCTION(BlueprintCallable, Category = "Bow")
    void FireGrapplingHook();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Bow")
    bool IsGrappling() const;

    UFUNCTION(BlueprintCallable, Category = "Bow")
    void ReleaseGrapplingHook();

    // —— 瞄具 ——
    UFUNCTION(BlueprintCallable, Category = "Bow")
    void ToggleScope();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Bow")
    bool IsScoped() const { return bIsScoped; }

    // —— 网络复制 ——
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

private:
    void SpawnArrow(const FVector& Origin, const FVector& Direction, float DamageMult);
    void ProcessStandardArrow();
    void ProcessBodkinArrow();
    void ProcessFireArrow();
    void ProcessPoisonArrow();
    void ProcessCryoArrow();
    void ProcessExplosiveArrow();
    void UpdateDraw(float Dt);
    void UpdateFatigue(float Dt);

    bool bIsScoped = false;
    FTimerHandle ReloadTimerHandle;
    FTimerHandle FatigueTimerHandle;
};
