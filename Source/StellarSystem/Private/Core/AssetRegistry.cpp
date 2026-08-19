// ============================================================
// 路径: Source/StellarSystem/Private/Core/AssetRegistry.cpp
// 作用: 资产覆盖注册表实现
// 依赖: Core/AssetRegistry.h
// ============================================================

#include "Core/AssetRegistry.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "Animation/AnimBlueprint.h"
#include "Misc/PackageName.h"
#include "UObject/SoftObjectPath.h"
#include "HAL/FileManager.h"

const FAssetOverrideRule* UAssetRegistry::FindBestRule(const FName& LogicalName,
    const FGameplayTagContainer& Tags) const
{
    const FAssetOverrideRule* BestRule = nullptr;
    int32 BestPriority = MIN_int32;

    for (const FAssetOverrideRule& Rule : OverrideRules)
    {
        if (!Rule.bEnabled) continue;
        if (Rule.LogicalName != LogicalName) continue;

        // 标签过滤
        if (Rule.RequiredTags.Num() > 0)
        {
            if (!Tags.HasAll(Rule.RequiredTags)) continue;
        }

        if (Rule.Priority > BestPriority)
        {
            BestPriority = Rule.Priority;
            BestRule = &Rule;
        }
    }

    return BestRule;
}

UStaticMesh* UAssetRegistry::GetStaticMesh(const FName& LogicalName,
    const FGameplayTagContainer& Tags) const
{
    // 1. 查缓存
    if (TWeakObjectPtr<UObject>* Cached = LoadedCache.Find(LogicalName))
    {
        if (Cached->IsValid())
            return Cast<UStaticMesh>(Cached->Get());
    }

    // 2. 查显式规则
    if (const FAssetOverrideRule* Rule = FindBestRule(LogicalName, Tags))
    {
        if (Rule->OverrideMesh.IsValid())
        {
            UStaticMesh* Mesh = Rule->OverrideMesh.LoadSynchronous();
            if (Mesh)
            {
                LoadedCache.Add(LogicalName, Mesh);
                UE_LOG(LogTemp, Log, TEXT("[AssetReg] Rule hit: %s"), *LogicalName.ToString());
                return Mesh;
            }
        }
    }

    // 3. 自动发现
    UStaticMesh* AutoMesh = AutoDiscoverStaticMesh(LogicalName);
    if (AutoMesh)
    {
        LoadedCache.Add(LogicalName, AutoMesh);
        UE_LOG(LogTemp, Log, TEXT("[AssetReg] Auto-discovered: %s"), *LogicalName.ToString());
        return AutoMesh;
    }

    // 4. 返回 nullptr → 调用方 fallback 到程序化生成
    UE_LOG(LogTemp, Verbose, TEXT("[AssetReg] No override for: %s → procedural fallback"),
        *LogicalName.ToString());
    return nullptr;
}

USkeletalMesh* UAssetRegistry::GetSkeletalMesh(const FName& LogicalName,
    const FGameplayTagContainer& Tags) const
{
    // 1. 查缓存
    if (TWeakObjectPtr<UObject>* Cached = LoadedCache.Find(LogicalName))
    {
        if (Cached->IsValid())
            return Cast<USkeletalMesh>(Cached->Get());
    }

    // 2. 查显式规则
    if (const FAssetOverrideRule* Rule = FindBestRule(LogicalName, Tags))
    {
        if (Rule->OverrideSkeletalMesh.IsValid())
        {
            USkeletalMesh* Mesh = Rule->OverrideSkeletalMesh.LoadSynchronous();
            if (Mesh)
            {
                LoadedCache.Add(LogicalName, Mesh);
                return Mesh;
            }
        }
    }

    // 3. 自动发现
    USkeletalMesh* AutoMesh = AutoDiscoverSkeletalMesh(LogicalName);
    if (AutoMesh)
    {
        LoadedCache.Add(LogicalName, AutoMesh);
        return AutoMesh;
    }

    return nullptr;
}

UMaterialInterface* UAssetRegistry::GetMaterial(const FName& LogicalName,
    const FGameplayTagContainer& Tags) const
{
    if (const FAssetOverrideRule* Rule = FindBestRule(LogicalName, Tags))
    {
        if (Rule->OverrideMaterial.IsValid())
            return Rule->OverrideMaterial.LoadSynchronous();
    }
    return nullptr;
}

UAnimBlueprint* UAssetRegistry::GetAnimBlueprint(const FName& LogicalName,
    const FGameplayTagContainer& Tags) const
{
    if (const FAssetOverrideRule* Rule = FindBestRule(LogicalName, Tags))
    {
        if (Rule->OverrideAnimBlueprint.IsValid())
            return Rule->OverrideAnimBlueprint.LoadSynchronous();
    }
    return nullptr;
}

UStaticMesh* UAssetRegistry::AutoDiscoverStaticMesh(const FName& LogicalName) const
{
    if (!bEnableAutoDiscovery) return nullptr;

    FString NameStr = LogicalName.ToString();

    for (const FString& Quality : QualityFallbackOrder)
    {
        FString Path = FString::Printf(TEXT("%s/StaticMeshes/%s/%s_%s.uasset"),
            *AutoDiscoveryRoot, *NameStr, *NameStr, *Quality);

        // 去掉 .uasset 后缀，用对象路径加载
        FString ObjectPath = FString::Printf(TEXT("%s.StaticMeshes.%s.%s_%s"),
            *AutoDiscoveryRoot, *NameStr, *NameStr, *Quality);

        // 尝试直接路径
        FString FullPath = FString::Printf(TEXT("%s/StaticMeshes/%s/%s_%s"),
            *AutoDiscoveryRoot, *NameStr, *NameStr, *Quality);

        if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *FullPath))
        {
            return Mesh;
        }
    }

    return nullptr;
}

USkeletalMesh* UAssetRegistry::AutoDiscoverSkeletalMesh(const FName& LogicalName) const
{
    if (!bEnableAutoDiscovery) return nullptr;

    FString NameStr = LogicalName.ToString();

    for (const FString& Quality : QualityFallbackOrder)
    {
        FString FullPath = FString::Printf(TEXT("%s/SkeletalMeshes/%s/%s_%s"),
            *AutoDiscoveryRoot, *NameStr, *NameStr, *Quality);

        if (USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *FullPath))
        {
            return Mesh;
        }
    }

    return nullptr;
}

void UAssetRegistry::PreloadAssets(const TArray<FName>& LogicalNames)
{
    FGameplayTagContainer EmptyTags;
    for (const FName& Name : LogicalNames)
    {
        GetStaticMesh(Name, EmptyTags);
        GetSkeletalMesh(Name, EmptyTags);
    }
    UE_LOG(LogTemp, Log, TEXT("[AssetReg] Preloaded %d assets"), LogicalNames.Num());
}

void UAssetRegistry::UnloadAssets(const TArray<FName>& LogicalNames)
{
    for (const FName& Name : LogicalNames)
    {
        LoadedCache.Remove(Name);
    }
}

void UAssetRegistry::AddOverrideRule(const FAssetOverrideRule& NewRule)
{
    OverrideRules.Add(NewRule);
}

bool UAssetRegistry::RemoveOverrideRule(const FName& LogicalName)
{
    for (int32 i = OverrideRules.Num() - 1; i >= 0; --i)
    {
        if (OverrideRules[i].LogicalName == LogicalName)
        {
            OverrideRules.RemoveAt(i);
            LoadedCache.Remove(LogicalName);
            return true;
        }
    }
    return false;
}

void UAssetRegistry::ClearCache()
{
    LoadedCache.Reset();
    UE_LOG(LogTemp, Log, TEXT("[AssetReg] Cache cleared"));
}
