// ProceduralEquipment.h
// AI 程序化生成护甲 + 个人武器（参数化几何 + 噪声细节 + 材质变异）
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ProceduralEquipment.generated.h"

class UProceduralMeshComponent;
class UStaticMesh;
class UMaterialInterface;

// ============ 护甲 ============

UENUM(BlueprintType)
enum class EArmorSlot : uint8
{
    Head, Chest, Arms, Legs, Feet, Shield
};

UENUM(BlueprintType)
enum class EArmorType : uint8
{
    Light,      // 轻甲：高机动
    Medium,     // 中甲：均衡
    Heavy,      // 重甲：高防御
    Powered,    // 动力甲：技能加成
    Stealth,    // 潜行甲：隐身加成
    Hazard      // 危险环境甲：抗辐射/毒/热
};

// 护甲参数（AI 生成 + 玩家可微调）
USTRUCT(BlueprintType)
struct FArmorParameters
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName ArmorID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EArmorSlot Slot = EArmorSlot::Chest;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EArmorType Type = EArmorType::Medium;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EItemRarity Rarity = EItemRarity::Common;

    // —— 几何参数 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1"))
    float PlateThickness = 0.5f;   // 装甲板厚度
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1"))
    float SegmentCount = 0.5f;     // 分段数（整体→分块）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1"))
    float Angularity = 0.5f;       // 棱角 vs 圆润
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1"))
    float SurfaceDetail = 0.5f;     // 表面细节密度
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1"))
    float Vents = 0.3f;            // 散热口大小
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1"))
    float Pauldrons = 0.5f;        // 肩甲突出度
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1"))
    float TrimWidth = 0.3f;        // 边缘饰条宽度

    // —— 材质参数 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor PrimaryColor = FLinearColor(0.3f, 0.35f, 0.4f, 1.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor SecondaryColor = FLinearColor(0.15f, 0.15f, 0.2f, 1.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor AccentColor = FLinearColor(0.8f, 0.6f, 0.1f, 1.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1"))
    float Metallic = 0.7f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1"))
    float Roughness = 0.4f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1"))
    float WearAndTear = 0.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1"))
    float EnergyGlow = 0.f;        // 能量发光条纹

    // —— 属性 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DefenseRating = 50.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MobilityPenalty = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EnergyResistance = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ThermalResistance = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RadiationResistance = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ToxinResistance = 0.f;

    // —— 外观变体 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName FactionTheme;             // 阵营外观主题
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FName> AttachmentPoints; // 可挂附件位置
};

// 护甲生成器
UCLASS(BlueprintType)
class UArmorGenerator : public UObject
{
    GENERATED_BODY()

public:
    // Seed → 完整护甲参数
    UFUNCTION(BlueprintCallable, Category="Equipment|Armor")
    static FArmorParameters GenerateArmor(int32 Seed, EArmorSlot Slot, EArmorType Type = EArmorType::Medium);

    // 基于基础护甲做变异（随机词条）
    UFUNCTION(BlueprintCallable, Category="Equipment|Armor")
    static FArmorParameters MutateArmor(const FArmorParameters& Base, int32 Seed, float Strength = 0.15f);

    // 按稀有度提权（高稀有度 → 更高属性和视觉特效）
    UFUNCTION(BlueprintCallable, Category="Equipment|Armor")
    static void ApplyRarityScaling(FArmorParameters& Params, EItemRarity Rarity);

    // 生成护甲 Mesh（程序化几何）
    UFUNCTION(BlueprintCallable, Category="Equipment|Armor")
    static void BuildArmorMesh(UProceduralMeshComponent* TargetMesh, const FArmorParameters& Params);

private:
    static void GenerateChestPlate(TArray<FVector>& V, TArray<int32>& T, const FArmorParameters& P);
    static void GeneratePauldron(TArray<FVector>& V, TArray<int32>& T, const FArmorParameters& P, bool bLeft);
    static void GenerateHelmet(TArray<FVector>& V, TArray<int32>& T, const FArmorParameters& P);
    static void GenerateGreaves(TArray<FVector>& V, TArray<int32>& T, const FArmorParameters& P);
    static void GenerateBoots(TArray<FVector>& V, TArray<int32>& T, const FArmorParameters& P);
    static void GenerateGauntlets(TArray<FVector>& V, TArray<int32>& T, const FArmorParameters& P);
    static void AddSurfaceDetail(TArray<FVector>& V, const FArmorParameters& P, FRandomStream& Rand);
    static void ApplyFactionTheme(FArmorParameters& P, FRandomStream& Rand);
};

// ============ 个人武器 ============

UENUM(BlueprintType)
enum class EWeaponClass : uint8
{
    Pistol, Rifle, SMG, Shotgun, Sniper,
    EnergyPistol, EnergyRifle, PlasmaCaster,
    Melee, Throwable
};

UENUM(BlueprintType)
enum class EFireMode : uint8
{
    Semi, Burst, Auto, Charge, Beam
};

// 武器参数
USTRUCT(BlueprintType)
struct FWeaponParameters
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName WeaponID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EWeaponClass Class = EWeaponClass::Rifle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EFireMode FireMode = EFireMode::Auto;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EItemRarity Rarity = EItemRarity::Common;

    // —— 几何 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1"))
    float BarrelLength = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1"))
    float BarrelThickness = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1"))
    float ReceiverSize = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1"))
    float StockStyle = 0.5f;     // 无托/折叠/固定
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1"))
    float GripAngle = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1"))
    float MagazineSize = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1"))
    float MagazineShape = 0.5f;  // 直/弯/鼓
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1"))
    float ScopeSize = 0.3f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1"))
    float RailCount = 0.3f;      // 皮卡汀尼导轨数
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1"))
    float MuzzleDevice = 0.3f;   // 消焰器/制退器/消音器

    // —— 材质 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor BodyColor = FLinearColor(0.2f, 0.2f, 0.22f, 1.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor TrimColor = FLinearColor(0.6f, 0.6f, 0.65f, 1.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor GlowColor = FLinearColor(0.f, 0.8f, 1.f, 1.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1"))
    float BodyMetallic = 0.8f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1"))
    float BodyRoughness = 0.35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1"))
    float GripTexture = 0.5f;

    // —— 战斗属性 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Damage = 25.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FireRate = 600.f;       // 发/分钟
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MagazineCapacity = 30.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ReloadTime = 2.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Accuracy = 0.8f;        // 0-1
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Stability = 0.7f;       // 后坐力控制
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Range = 50000.f;        // cm
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CriticalChance = 0.05f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CriticalMultiplier = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HeatPerShot = 5.f;      // 能量武器
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ChargeTime = 0.f;       // 蓄力武器

    // —— 元素伤害 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FireDamage = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ElectricDamage = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FrostDamage = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AcidDamage = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float VoidDamage = 0.f;

    // —— 特殊词条 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FName> Affixes;         // 随机词条
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float WeaponMass = 2.f;        // 影响后坐力
};

// 武器生成器
UCLASS(BlueprintType)
class UWeaponGenerator : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="Equipment|Weapon")
    static FWeaponParameters GenerateWeapon(int32 Seed, EWeaponClass Class = EWeaponClass::Rifle);

    UFUNCTION(BlueprintCallable, Category="Equipment|Weapon")
    static FWeaponParameters MutateWeapon(const FWeaponParameters& Base, int32 Seed, float Strength = 0.15f);

    UFUNCTION(BlueprintCallable, Category="Equipment|Weapon")
    static void ApplyRarityScaling(FWeaponParameters& Params, EItemRarity Rarity);

    UFUNCTION(BlueprintCallable, Category="Equipment|Weapon")
    static void BuildWeaponMesh(UProceduralMeshComponent* TargetMesh, const FWeaponParameters& Params);

    // 生成随机词条（基于稀有度）
    UFUNCTION(BlueprintCallable, Category="Equipment|Weapon")
    static TArray<FName> RollAffixes(int32 Seed, EItemRarity Rarity, EWeaponClass Class);

private:
    static void GenerateBarrel(TArray<FVector>& V, TArray<int32>& T, const FWeaponParameters& P);
    static void GenerateReceiver(TArray<FVector>& V, TArray<int32>& T, const FWeaponParameters& P);
    static void GenerateStock(TArray<FVector>& V, TArray<int32>& T, const FWeaponParameters& P);
    static void GenerateGrip(TArray<FVector>& V, TArray<int32>& T, const FWeaponParameters& P);
    static void GenerateMagazine(TArray<FVector>& V, TArray<int32>& T, const FWeaponParameters& P);
    static void GenerateScope(TArray<FVector>& V, TArray<int32>& T, const FWeaponParameters& P);
    static void GenerateMuzzle(TArray<FVector>& V, TArray<int32>& T, const FWeaponParameters& P);
    static void AddWeaponDetail(TArray<FVector>& V, const FWeaponParameters& P, FRandomStream& Rand);
};

// 武器数据资产（可售卖/存档）
UCLASS(BlueprintType)
class UWeaponDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FWeaponParameters WeaponParams;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSubclassOf<AActor> ProjectileClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    USoundBase* FireSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UParticleSystem* MuzzleFlash;
};

// 护甲数据资产
UCLASS(BlueprintType)
class UArmorDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FArmorParameters ArmorParams;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName SetBonusID; // 套装效果
};
