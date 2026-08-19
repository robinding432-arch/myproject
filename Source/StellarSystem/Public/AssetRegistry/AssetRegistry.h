// AssetRegistry.h
// 资产覆盖注册表：美术自建模型替换 AI 生成模型的接口层
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "AssetRegistry.generated.h"

class UStaticMesh;
class USkeletalMesh;
class UMaterialInterface;
class UAnimBlueprint;

// 单条覆盖规则
USTRUCT(BlueprintType)
struct FAssetOverrideRule
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Override")
    FName LogicalName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Override")
    int32 Priority = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Override", meta = (AllowedClasses = "StaticMesh"))
    TSoftObjectPtr<UStaticMesh> OverrideMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Override", meta = (AllowedClasses = "SkeletalMesh"))
    TSoftObjectPtr<USkeletalMesh> OverrideSkeletalMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Override", meta = (AllowedClasses = "MaterialInterface"))
    TSoftObjectPtr<UMaterialInterface> OverrideMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Override", meta = (AllowedClasses = "AnimBlueprint"))
    TSoftObjectPtr<UAnimBlueprint> OverrideAnimBlueprint;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Override")
    bool bEnabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Override")
    FGameplayTagContainer RequiredTags;
};

// 命名约定模板
USTRUCT(BlueprintType)
struct FAssetNamingConvention
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naming")
    FString RootPath = TEXT("/Game/Art");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naming")
    FString Template = TEXT("{LogicalName}_{Quality}_{LOD}");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naming")
    TArray<FString> QualityFallbackOrder = {TEXT("High"), TEXT("Medium"), TEXT("Low")};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naming")
    TArray<FString> ValidExtensions = {TEXT(".uasset"), TEXT(".fbx"), TEXT(".obj")};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naming")
    bool bEnableAutoDiscovery = true;
};

// 主注册表 DataAsset
UCLASS(BlueprintType)
class UAssetRegistry : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // 显式覆盖规则（优先级最高）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overrides")
    TArray<FAssetOverrideRule> OverrideRules;

    // 自动发现配置
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoDiscovery")
    FAssetNamingConvention NamingConvention;

    // 预加载配置
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming")
    float CacheTimeoutSeconds = 300.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming")
    int32 MaxCachedAssets = 200;

    // —— 查询接口（所有模块调这个）——
    UFUNCTION(BlueprintCallable, Category = "AssetRegistry")
    UStaticMesh* GetStaticMesh(const FName& LogicalName, const FGameplayTagContainer& Tags) const;

    UFUNCTION(BlueprintCallable, Category = "AssetRegistry")
    USkeletalMesh* GetSkeletalMesh(const FName& LogicalName, const FGameplayTagContainer& Tags) const;

    UFUNCTION(BlueprintCallable, Category = "AssetRegistry")
    UMaterialInterface* GetMaterial(const FName& LogicalName, const FGameplayTagContainer& Tags) const;

    UFUNCTION(BlueprintCallable, Category = "AssetRegistry")
    UAnimBlueprint* GetAnimBlueprint(const FName& LogicalName, const FGameplayTagContainer& Tags) const;

    // 批量预加载
    UFUNCTION(BlueprintCallable, Category = "AssetRegistry")
    void PreloadAssets(const TArray<FName>& LogicalNames);

    // 卸载
    UFUNCTION(BlueprintCallable, Category = "AssetRegistry")
    void UnloadAssets(const TArray<FName>& LogicalNames);

    // 清除缓存
    UFUNCTION(BlueprintCallable, Category = "AssetRegistry")
    void ClearCache();

    // 运行时动态添加规则（Mod 支持）
    UFUNCTION(BlueprintCallable, Category = "AssetRegistry")
    void AddOverrideRule(const FAssetOverrideRule& NewRule);

    UFUNCTION(BlueprintCallable, Category = "AssetRegistry")
    void RemoveOverrideRule(FName LogicalName, int32 Priority);

    // 诊断
    UFUNCTION(BlueprintCallable, Category = "AssetRegistry|Debug")
    FString GetDebugInfo() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AssetRegistry|Debug")
    int32 GetCacheCount() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AssetRegistry|Debug")
    TArray<FName> GetCachedAssetNames() const;

#if WITH_EDITOR
    // 编辑器热重载（开发期神器）
    UFUNCTION(BlueprintCallable, Category = "AssetRegistry|Editor")
    void OnAssetModified(FAssetData AssetData);

    UFUNCTION(BlueprintCallable, Category = "AssetRegistry|Editor")
    void ValidateAllRules() const;
#endif

private:
    // 运行时缓存
    mutable TMap<FName, TWeakObjectPtr<UObject>> LoadedCache;
    mutable TMap<FName, float> CacheTimestamps;

    // 查找最佳规则
    const FAssetOverrideRule* FindBestRule(const FName& LogicalName, const FGameplayTagContainer& Tags) const;

    // 自动发现
    UStaticMesh* AutoDiscoverStaticMesh(const FName& LogicalName) const;
    USkeletalMesh* AutoDiscoverSkeletalMesh(const FName& LogicalName) const;
    UMaterialInterface* AutoDiscoverMaterial(const FName& LogicalName) const;

    // 路径拼接
    FString BuildAutoDiscoveryPath(const FName& LogicalName, const FString& Quality, const FString& Extension) const;

    // 缓存管理
    void AddToCache(FName Name, UObject* Asset) const;
    void CleanupCache() const;
    bool IsCacheValid(FName Name) const;
};
