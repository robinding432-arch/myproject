// StellarDataAssets.h
// 所有 DataAsset 父类 + 共享枚举汇总
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StellarDataAssets.generated.h"

// 全局稀有度（所有模块共用）
UENUM(BlueprintType)
enum class EItemRarity : uint8
{
    Common      UMETA(DisplayName = "普通"),
    Uncommon    UMETA(DisplayName = "优秀"),
    Rare        UMETA(DisplayName = "稀有"),
    Epic        UMETA(DisplayName = "史诗"),
    Legendary   UMETA(DisplayName = "传说"),
    Mythic      UMETA(DisplayName = "神话")
};

// 货币类型（全局）
UENUM(BlueprintType)
enum class ECurrencyType : uint8
{
    Credits,    // 通用货币
    Premium,    // 付费货币
    Faction,    // 阵营声望
    Salvage,    // 拆解废料
    Science,    // 科研点
    Reputation  // 声望值
};

// 物品类型
UENUM(BlueprintType)
enum class EShopItemType : uint8
{
    Weapon, Armor, Helmet, Boots, Gloves,
    Consumable, Ammo, ShipComponent, Cosmetic, Upgrade
};

// 环境危险类型
UENUM(BlueprintType)
enum class EHazardType : uint8
{
    None, Vacuum, ToxicAtmosphere, ExtremeHeat, ExtremeCold,
    Radiation, Corrosive, BiolHazard, EMP
};

// 武器火控模式
UENUM(BlueprintType)
enum class EFireMode : uint8
{
    Semi,       // 半自动
    Burst,      // 点射
    Auto,       // 全自动
    Charge,     // 蓄力
    Beam        // 光束
};

// 元素伤害类型
UENUM(BlueprintType)
enum class EElementType : uint8
{
    None, Fire, Electric, Frost, Acid, Void
};

// 弹药类型
UENUM(BlueprintType)
enum class EAmmoType : uint8
{
    LightCaliber, MediumCaliber, HeavyCaliber,
    SniperRound, ShotgunShell, PistolRound, SMGRound,
    EnergyCell, PlasmaPack, RailSlug,
    Rocket, Grenade, HomingMissile, Mine, Flare
};

// 装备槽位
UENUM(BlueprintType)
enum class EEquipSlot : uint8
{
    Head, Chest, Arms, Legs, Feet,
    Weapon1, Weapon2, Weapon3,
    ShipComponent1, ShipComponent2,
    Utility1, Utility2
};

// 飞船组件槽位
UENUM(BlueprintType)
enum class EShipComponentSlot : uint8
{
    Engine, Weapon, Shield, Sensor, Cargo,
    WarpCore, Reactor, Cooler, Armor, Utility
};

// 飞船组件稀有度
UENUM(BlueprintType)
enum class EComponentRarity : uint8
{
    Standard, Improved, Advanced, Prototype, Alien
};

// 全局数据资产基类
UCLASS(BlueprintType)
class UStellarDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stellar")
    FName AssetID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stellar")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stellar", meta = (MultiLine = true))
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stellar")
    EItemRarity Rarity = EItemRarity::Common;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stellar")
    int32 RequiredLevel = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stellar")
    UTexture2D* Icon = nullptr;

    // 数据资产类型标识
    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
