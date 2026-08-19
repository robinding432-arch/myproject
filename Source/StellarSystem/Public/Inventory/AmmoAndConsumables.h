// AmmoAndConsumables.h
// 弹药系统 + 消耗品系统（AI 生成弹药类型 + 全消耗品管理）
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AmmoAndConsumables.generated.h"

class UInventoryComponent;

// 弹药类型
UENUM(BlueprintType)
enum class EAmmoType : uint8
{
    LightCaliber,   // 轻型：高穿透
    MediumCaliber,  // 中型：均衡
    HeavyCaliber,   // 重型：高伤害
    SniperRound,    // 狙击：极远射程
    ShotgunShell,   // 霰弹
    PistolRound,    // 手枪弹
    SMGRound,       // 冲锋枪弹
    EnergyCell,     // 能量电池
    PlasmaPack,     // 等离子燃料
    RailSlug,       // 磁轨弹丸
    Rocket,         // 火箭弹
    Grenade,        // 手雷
    HomingMissile,  // 追踪导弹
    Mine,           // 地雷
    Flare           // 信号弹
};

// 弹药参数（AI 生成）
USTRUCT(BlueprintType)
struct FAmmoParameters
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName AmmoID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EAmmoType Type = EAmmoType::MediumCaliber;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EItemRarity Rarity = EItemRarity::Common;

    // —— 物理属性 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Mass = 10.f;           // 克
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Caliber = 7.62f;      // 毫米
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MuzzleVelocity = 850.f; // m/s
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DragCoefficient = 0.3f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Stability = 0.8f;      // 飞行稳定性

    // —— 伤害属性 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BaseDamage = 35.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ArmorPierce = 0.2f;    // 穿甲率
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Range = 50000.f;       // 有效射程
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DropoffStart = 0.7f;   // 距离衰减起始（%射程）

    // —— 元素效果 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FireChance = 0.f;      // 点燃概率
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FireDuration = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ElectricChance = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FrostChance = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AcidChance = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float VoidChance = 0.f;

    // —— 特殊效果 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bExplosive = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ExplosionRadius = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bTracer = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIncendiary = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bHollowPoint = false;    // 空尖弹：高伤害低穿甲
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bArmorPiercing = false;  // 穿甲弹：低伤害高穿甲
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bSubsonic = false;       // 亚音速：消音友好

    // —— 视觉 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor TracerColor = FLinearColor(1.f, 0.8f, 0.3f, 1.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1"))
    float CasingLength = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1"))
    float CasingWidth = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor CasingColor = FLinearColor(0.7f, 0.6f, 0.3f, 1.f);

    // —— 堆叠 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 StackSize = 30;
};

// 消耗品类型
UENUM(BlueprintType)
enum class EConsumableType : uint8
{
    // 医疗
    Medkit, AdvancedMedkit, TraumaKit, StimPack, Adrenaline,
    // 食物
    Ration, MRE, FreshFood, SynthesizedMeal, AlienFruit,
    // 饮料
    Water, EnergyDrink, AlienBrew, Coffee,
    // 氧气
    OxygenTank, OxygenCanister, EVAKit,
    // 能量
    Battery, PowerCell, FusionCell,
    // 工具
    RepairKit, Multitool, Welder, Scanner,
    // 特殊
    AntiRad, Antidote, Steroid, NeuralBooster,
    // 信号
    Flare, SmokeGrenade, EMPGrenade, DistressBeacon
};

// 消耗品参数
USTRUCT(BlueprintType)
struct FConsumableParameters
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName ConsumableID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EConsumableType Type = EConsumableType::Medkit;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EItemRarity Rarity = EItemRarity::Common;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(MultiLine=true))
    FText Description;

    // —— 效果数值 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HealthRestore = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OxygenRestore = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EnergyRestore = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float StaminaRestore = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HungerRestore = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ThirstRestore = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RadiationCure = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ToxinCure = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SuitRepair = 0.f;

    // —— 临时增益（Buff）——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BuffDuration = 0.f;     // 秒
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SpeedBuff = 0.f;        // 移速%
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DamageBuff = 0.f;       // 伤害%
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DefenseBuff = 0.f;      // 防御%
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float StealthBuff = 0.f;      // 隐蔽%
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RegenBuff = 0.f;        // 回血/秒

    // —— 使用条件 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bUsableInCombat = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bUsableWhileMoving = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float UseTime = 1.f;          // 使用耗时（秒）

    // —— 堆叠 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 StackSize = 5;

    // —— 稀有度效果倍率 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Effectiveness = 1.f;   // 效果倍率

    // —— 视觉 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor ItemColor = FLinearColor(0.2f, 0.8f, 0.3f, 1.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UTexture2D* Icon = nullptr;
};

// 弹药生成器
UCLASS(BlueprintType)
class UAmmoGenerator : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="Ammo")
    static FAmmoParameters GenerateAmmo(int32 Seed, EAmmoType Type = EAmmoType::MediumCaliber);

    UFUNCTION(BlueprintCallable, Category="Ammo")
    static FAmmoParameters MutateAmmo(const FAmmoParameters& Base, int32 Seed, float Strength = 0.15f);

    UFUNCTION(BlueprintCallable, Category="Ammo")
    static void ApplyRarityToAmmo(FAmmoParameters& P, EItemRarity Rarity);

    UFUNCTION(BlueprintCallable, Category="Ammo")
    static TArray<EAmmoType> GetCompatibleAmmoTypes(EWeaponClass WeaponClass);

    // 弹药物理模拟（弹道计算）
    UFUNCTION(BlueprintCallable, Category="Ammo")
    static FVector CalculateTrajectory(const FAmmoParameters& Ammo, FVector StartPos,
                                        FVector Direction, float Gravity, float TimeStep = 0.016f);

    // 弹药效果计算
    UFUNCTION(BlueprintCallable, Category="Ammo")
    static float CalculateDamageAtRange(const FAmmoParameters& Ammo, float Distance);
};

// 消耗品生成器
UCLASS(BlueprintType)
class UConsumableGenerator : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="Consumable")
    static FConsumableParameters GenerateConsumable(int32 Seed, EConsumableType Type);

    UFUNCTION(BlueprintCallable, Category="Consumable")
    static FConsumableParameters GenerateRandomConsumable(int32 Seed);

    UFUNCTION(BlueprintCallable, Category="Consumable")
    static void ApplyRarityToConsumable(FConsumableParameters& P, EItemRarity Rarity);

    // 使用消耗品（应用到维生系统）
    UFUNCTION(BlueprintCallable, Category="Consumable")
    static bool UseConsumable(APawn* User, const FConsumableParameters& Consumable);
};

// 弹药库存组件（挂在 Character 上）
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UAmmoInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UAmmoInventoryComponent();

    // 弹药库存：AmmoID → 数量
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
    TMap<FName, int32> AmmoStock;

    // 查询
    UFUNCTION(BlueprintCallable)
    int32 GetAmmoCount(FName AmmoID) const;

    UFUNCTION(BlueprintCallable)
    int32 GetAmmoCountByType(EAmmoType Type) const;

    // 增删
    UFUNCTION(BlueprintCallable, Server, Reliable)
    void ServerAddAmmo(FName AmmoID, int32 Amount);

    UFUNCTION(BlueprintCallable, Server, Reliable)
    bool ServerConsumeAmmo(FName AmmoID, int32 Amount);

    UFUNCTION(BlueprintCallable, Server, Reliable)
    void ServerAddAmmoOfType(EAmmoType Type, int32 Amount, int32 Seed = 0);

    // 检查是否兼容武器
    UFUNCTION(BlueprintCallable)
    bool IsCompatibleWithWeapon(EAmmoType AmmoType, EWeaponClass WeaponClass) const;

    // 自动装填（从库存选最优弹药）
    UFUNCTION(BlueprintCallable)
    FName GetBestAmmoForWeapon(EWeaponClass WeaponClass) const;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

private:
    // 弹药参数缓存（从 GameMode 查询）
    FAmmoParameters GetAmmoParams(FName AmmoID) const;
};

// 消耗品库存组件
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UConsumableInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UConsumableInventoryComponent();

    // 消耗品库存：ConsumableID → 数量
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
    TMap<FName, int32> ConsumableStock;

    // 快捷栏（0-9）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
    TMap<int32, FName> HotbarSlots;

    // 查询
    UFUNCTION(BlueprintCallable)
    int32 GetCount(FName ConsumableID) const;

    UFUNCTION(BlueprintCallable)
    TArray<FName> GetAllConsumables() const;

    // 增删
    UFUNCTION(BlueprintCallable, Server, Reliable)
    void ServerAddConsumable(FName ConsumableID, int32 Amount);

    UFUNCTION(BlueprintCallable, Server, Reliable)
    bool ServerUseConsumable(FName ConsumableID, APawn* User);

    UFUNCTION(BlueprintCallable, Server, Reliable)
    bool ServerUseFromHotbar(int32 Slot, APawn* User);

    // 快捷栏管理
    UFUNCTION(BlueprintCallable, Server, Reliable)
    void ServerAssignHotbar(int32 Slot, FName ConsumableID);

    UFUNCTION(BlueprintCallable, Server, Reliable)
    void ServerClearHotbar(int32 Slot);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

private:
    // 正在使用中的 Buff 列表
    UPROPERTY(Replicated)
    TArray<struct FActiveBuff> ActiveBuffs;
};

// 活跃 Buff
USTRUCT(BlueprintType)
struct FActiveBuff
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName SourceID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RemainingTime = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SpeedModifier = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DamageModifier = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DefenseModifier = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float StealthModifier = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RegenPerSecond = 0.f;
};

// 弹药数据资产
UCLASS(BlueprintType)
class UAmmoDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FAmmoParameters AmmoParams;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UStaticMesh* CasingMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UParticleSystem* TracerEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    USoundBase* ImpactSound;
};

// 消耗品数据资产
UCLASS(BlueprintType)
class UConsumableDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FConsumableParameters ConsumableParams;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UStaticMesh* ItemMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UParticleSystem* UseEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    USoundBase* UseSound;
};
