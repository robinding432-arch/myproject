// AssetRegistry.cpp
#include "AssetRegistry/AssetRegistry.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "Animation/AnimBlueprint.h"
#include "Misc/PackageName.h"
#include "UObject/SoftObjectPath.h"
#include "Math/UnrealMathUtility.h"

const FAssetOverrideRule* UAssetRegistry::FindBestRule(const FName& LogicalName,
    const FGameplayTagContainer& Tags) const
{
    const FAssetOverrideRule* Best = nullptr;
    int32 BestPriority = MIN_int32;

    for (const FAssetOverrideRule& Rule : OverrideRules)
    {
        if (!Rule.bEnabled) continue;
        if (Rule.LogicalName != LogicalName) continue;

        if (Rule.RequiredTags.Num() > 0)
        {
            if (!Tags.HasAll(Rule.RequiredTags)) continue;
        }

        if (Rule.Priority > BestPriority)
        {
            BestPriority = Rule.Priority;
            Best = &Rule;
        }
    }
    return Best;
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
                AddToCache(LogicalName, Mesh);
                return Mesh;
            }
        }
    }

    // 3. 自动发现
    UStaticMesh* Auto = AutoDiscoverStaticMesh(LogicalName);
    if (Auto)
    {
        AddToCache(LogicalName, Auto);
        return Auto;
    }

    return nullptr;
}

USkeletalMesh* UAssetRegistry::GetSkeletalMesh(const FName& LogicalName,
    const FGameplayTagContainer& Tags) const
{
    if (TWeakObjectPtr<UObject>* Cached = LoadedCache.Find(LogicalName))
    {
        if (Cached->IsValid())
            return Cast<USkeletalMesh>(Cached->Get());
    }

    if (const FAssetOverrideRule* Rule = FindBestRule(LogicalName, Tags))
    {
        if (Rule->OverrideSkeletalMesh.IsValid())
        {
            USkeletalMesh* Mesh = Rule->OverrideSkeletalMesh.LoadSynchronous();
            if (Mesh)
            {
                AddToCache(LogicalName, Mesh);
                return Mesh;
            }
        }
    }

    return AutoDiscoverSkeletalMesh(LogicalName);
}

UMaterialInterface* UAssetRegistry::GetMaterial(const FName& LogicalName,
    const FGameplayTagContainer& Tags) const
{
    if (const FAssetOverrideRule* Rule = FindBestRule(LogicalName, Tags))
    {
        if (Rule->OverrideMaterial.IsValid())
            return Rule->OverrideMaterial.LoadSynchronous();
    }
    return AutoDiscoverMaterial(LogicalName);
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

void UAssetRegistry::PreloadAssets(const TArray<FName>& LogicalNames)
{
    CleanupCache();
    for (const FName& Name : LogicalNames)
    {
        FGameplayTagContainer Empty;
        GetStaticMesh(Name, Empty);
    }
}

void UAssetRegistry::UnloadAssets(const TArray<FName>& LogicalNames)
{
    for (const FName& Name : LogicalNames)
    {
        LoadedCache.Remove(Name);
    }
}

void UAssetRegistry::ClearCache()
{
    LoadedCache.Reset();
    CacheTimestamps.Reset();
}

void UAssetRegistry::AddOverrideRule(const FAssetOverrideRule& NewRule)
{
    OverrideRules.Add(NewRule);
}

void UAssetRegistry::RemoveOverrideRule(FName LogicalName, int32 Priority)
{
    for (int32 i = OverrideRules.Num() - 1; i >= 0; --i)
    {
        if (OverrideRules[i].LogicalName == LogicalName
            && OverrideRules[i].Priority == Priority)
        {
            OverrideRules.RemoveAt(i);
        }
    }
}

FString UAssetRegistry::GetDebugInfo() const
{
    FString Info = FString::Printf(TEXT("AssetRegistry Debug:\n"));
    Info += FString::Printf(TEXT("  Rules: %d\n"), OverrideRules.Num());
    Info += FString::Printf(TEXT("  Cached: %d\n"), LoadedCache.Num());
    Info += FString::Printf(TEXT("  AutoDiscovery: %s\n"),
        NamingConvention.bEnableAutoDiscovery ? TEXT("ON") : TEXT("OFF"));
    Info += FString::Printf(TEXT("  RootPath: %s\n"), *NamingConvention.RootPath);
    return Info;
}

int32 UAssetRegistry::GetCacheCount() const
{
    return LoadedCache.Num();
}

TArray<FName> UAssetRegistry::GetCachedAssetNames() const
{
    TArray<FName> Names;
    LoadedCache.GetKeys(Names);
    return Names;
}

#if WITH_EDITOR
void UAssetRegistry::OnAssetModified(FAssetData AssetData)
{
    LoadedCache.Reset();
}

void UAssetRegistry::ValidateAllRules() const
{
    for (const FAssetOverrideRule& Rule : OverrideRules)
    {
        if (Rule.LogicalName == NAME_None)
        {
            UE_LOG(LogTemp, Warning, TEXT("AssetRegistry: Rule with empty LogicalName!"));
        }
    }
}
#endif

// --- Private ---

UStaticMesh* UAssetRegistry::AutoDiscoverStaticMesh(const FName& LogicalName) const
{
    if (!NamingConvention.bEnableAutoDiscovery) return nullptr;

    for (const FString& Quality : NamingConvention.QualityFallbackOrder)
    {
        for (const FString& Ext : NamingConvention.ValidExtensions)
        {
            FString Path = BuildAutoDiscoveryPath(LogicalName, Quality, Ext);
            if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path))
            {
                UE_LOG(LogTemp, Log, TEXT("[AssetRegistry] Auto-discovered: %s"), *Path);
                return Mesh;
            }
        }
    }
    return nullptr;
}

USkeletalMesh* UAssetRegistry::AutoDiscoverSkeletalMesh(const FName& LogicalName) const
{
    if (!NamingConvention.bEnableAutoDiscovery) return nullptr;

    for (const FString& Quality : NamingConvention.QualityFallbackOrder)
    {
        FString Path = BuildAutoDiscoveryPath(LogicalName, Quality, TEXT(".uasset"));
        if (USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *Path))
        {
            return Mesh;
        }
    }
    return nullptr;
}

UMaterialInterface* UAssetRegistry::AutoDiscoverMaterial(const FName& LogicalName) const
{
    if (!NamingConvention.bEnableAutoDiscovery) return nullptr;

    FString Path = BuildAutoDiscoveryPath(LogicalName, TEXT("Default"), TEXT(".uasset"));
    return LoadObject<UMaterialInterface>(nullptr, *Path);
}

FString UAssetRegistry::BuildAutoDiscoveryPath(const FName& LogicalName,
    const FString& Quality, const FString& Extension) const
{
    // Template: {RootPath}/StaticMeshes/{LogicalName}/{LogicalName}_{Quality}{Ext}
    return FString::Printf(TEXT("%s/StaticMeshes/%s/%s_%s%s"),
        *NamingConvention.RootPath,
        *LogicalName.ToString(),
        *LogicalName.ToString(),
        *Quality,
        *Extension);
}

void UAssetRegistry::AddToCache(FName Name, UObject* Asset) const
{
    if (!Asset) return;
    LoadedCache.Add(Name, TWeakObjectPtr<UObject>(Asset));
    CacheTimestamps.Add(Name, GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f);
    CleanupCache();
}

void UAssetRegistry::CleanupCache() const
{
    if (LoadedCache.Num() <= NamingConvention.MaxCachedAssets) return;

    // 移除最旧的
    float OldestTime = MAX_FLT;
    FName OldestKey = NAME_None;

    for (auto& Pair : CacheTimestamps)
    {
        if (Pair.Value < OldestTime)
        {
            OldestTime = Pair.Value;
            OldestKey = Pair.Key;
        }
    }

    if (OldestKey != NAME_None)
    {
        LoadedCache.Remove(OldestKey);
        CacheTimestamps.Remove(OldestKey);
    }
}

bool UAssetRegistry::IsCacheValid(FName Name) const
{
    if (TWeakObjectPtr<UObject>* Cached = LoadedCache.Find(Name))
    {
        return Cached->IsValid();
    }
    return false;
}
