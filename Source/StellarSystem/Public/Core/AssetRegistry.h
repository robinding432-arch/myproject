// ============================================================
// 路径: Source/StellarSystem/Public/Core/AssetRegistry.h
// 作用: 资产覆盖注册表 — 美术模型零代码替换 AI 生成模型
// 依赖: 无（基础模块，所有模块通过它要资源）
// 这是给后期维护人员留的"口子"
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "AssetRegistry.generated.h"

// —— 单条资产覆盖规则 ——
USTRUCT(BlueprintType)
struct FAssetOverrideRule
{
    GENERATED_BODY()

    // 匹配键：程序化生成时的"逻辑名"
    // 例："Ship_Hull_Fighter", "Character_Head_Male", "Weapon_Laser_Rifle"
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
    FName LogicalName;

    // 优先级：数字越大越优先
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
    int32 Priority = 0;

    // 覆盖用的静态网格（SoftPtr → 不强制加载）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh",
        meta = (AllowedClasses = "StaticMesh"))
    TSoftObjectPtr<UStaticMesh> OverrideMesh;

    // 覆盖用的骨骼网格（角色/武器用）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh",
        meta = (AllowedClasses = "SkeletalMesh"))
    TSoftObjectPtr<USkeletalMesh> OverrideSkeletalMesh;

    // 覆盖材质
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material",
        meta = (AllowedClasses = "MaterialInterface"))
    TSoftObjectPtr<UMaterialInterface> OverrideMaterial;

    // 覆盖动画蓝图
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation",
        meta = (AllowedClasses = "AnimBlueprint"))
    TSoftObjectPtr<UAnimBlueprint> OverrideAnimBlueprint;

    // 是否启用此规则
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
    bool bEnabled = true;

    // 标签过滤：只有带这些标签的实体才用此规则
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
    FGameplayTagContainer RequiredTags;
};

// —— 资产注册表 DataAsset ——
// 美术在编辑器里建一个这个资产，往里填规则即可
UCLASS(BlueprintType)
class UAssetRegistry : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // ========== 显式规则（手动配置，优先级最高） ==========
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overrides")
    TArray<FAssetOverrideRule> OverrideRules;

    // ========== 自动发现（按路径约定自动查找） ==========
    UPROPERTY(EditAnywhere, Category = "AutoDiscovery")
    bool bEnableAutoDiscovery = true;

    // 根目录：/Game/Art/Characters/ 下按 "LogicalName" 建文件夹
    UPROPERTY(EditAnywhere, Category = "AutoDiscovery")
    FString AutoDiscoveryRoot = TEXT("/Game/Art");

    // 命名约定模板
    // {LogicalName}_{Quality}_{LOD} 例：Ship_Hull_Fighter_High_LOD0
    UPROPERTY(EditAnywhere, Category = "AutoDiscovery")
    FString NamingTemplate = TEXT("{LogicalName}_{Quality}_{LOD}");

    // 质量级别优先级（从高到低尝试）
    UPROPERTY(EditAnywhere, Category = "AutoDiscovery")
    TArray<FString> QualityFallbackOrder = {TEXT("High"), TEXT("Medium"), TEXT("Low")};

    // LOD 层级数
    UPROPERTY(EditAnywhere, Category = "AutoDiscovery")
    int32 LODCount = 4;

    // ========== 查询接口（所有模块调这个） ==========
    UFUNCTION(BlueprintCallable, Category = "AssetRegistry")
    UStaticMesh* GetStaticMesh(const FName& LogicalName, const FGameplayTagContainer& Tags) const;

    UFUNCTION(BlueprintCallable, Category = "AssetRegistry")
    USkeletalMesh* GetSkeletalMesh(const FName& LogicalName, const FGameplayTagContainer& Tags) const;

    UFUNCTION(BlueprintCallable, Category = "AssetRegistry")
    UMaterialInterface* GetMaterial(const FName& LogicalName, const FGameplayTagContainer& Tags) const;

    UFUNCTION(BlueprintCallable, Category = "AssetRegistry")
    UAnimBlueprint* GetAnimBlueprint(const FName& LogicalName, const FGameplayTagContainer& Tags) const;

    // 批量预加载（进区域时调用）
    UFUNCTION(BlueprintCallable, Category = "AssetRegistry")
    void PreloadAssets(const TArray<FName>& LogicalNames);

    // 卸载（离开区域时调用）
    UFUNCTION(BlueprintCallable, Category = "AssetRegistry")
    void UnloadAssets(const TArray<FName>& LogicalNames);

    // 运行时添加/移除规则（DLC / 热更新用）
    UFUNCTION(BlueprintCallable, Category = "AssetRegistry")
    void AddOverrideRule(const FAssetOverrideRule& NewRule);

    UFUNCTION(BlueprintCallable, Category = "AssetRegistry")
    bool RemoveOverrideRule(const FName& LogicalName);

    // 清除缓存（编辑器热重载用）
    UFUNCTION(BlueprintCallable, Category = "AssetRegistry")
    void ClearCache();

private:
    // 按优先级+标签匹配最佳规则
    const FAssetOverrideRule* FindBestRule(const FName& LogicalName,
        const FGameplayTagContainer& Tags) const;

    // 自动发现：按路径约定拼路径尝试加载
    UStaticMesh* AutoDiscoverStaticMesh(const FName& LogicalName) const;
    USkeletalMesh* AutoDiscoverSkeletalMesh(const FName& LogicalName) const;

    // 运行时缓存（已加载的资产，避免重复 IO）
    mutable TMap<FName, TWeakObjectPtr<UObject>> LoadedCache;
};
