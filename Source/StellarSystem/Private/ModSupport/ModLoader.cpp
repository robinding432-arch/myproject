// ModLoader.cpp
// Mod 支持系统实现（骨架版）

#include "ModSupport/ModLoader.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonTypes.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManager.h"
#include "Algo/Sort.h"

UModLoader::UModLoader()
{
    LuaState = nullptr;
}

void UModLoader::Initialize()
{
    // 确保 Mods 目录存在
    FString FullPath = FPaths::Combine(FPaths::ProjectDir(), ModsRootDirectory);
    IFileManager::Get().MakeDirectory(*FullPath, true);

    // 扫描并加载
    TArray<FModInfo> Found = ScanForMods(ModsRootDirectory);
    UE_LOG(LogTemp, Log, TEXT("[ModLoader] Found %d mods"), Found.Num());

    if (bEnableLuaScripting)
    {
        RegisterScriptAPI();
    }

    LoadAllEnabledMods();
}

TArray<FModInfo> UModLoader::ScanForMods(const FString& ModsDirectory)
{
    TArray<FModInfo> Results;
    FString FullPath = FPaths::Combine(FPaths::ProjectDir(), ModsDirectory);

    TArray<FString> SubDirs;
    IFileManager::Get().FindFiles(SubDirs, *(FullPath / TEXT("*")), false, true);

    for (const FString& Dir : SubDirs)
    {
        FString ManifestPath = FPaths::Combine(FullPath, Dir, TEXT("mod.json"));
        if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*ManifestPath))
        {
            FModInfo Info;
            if (ParseModManifest(ManifestPath, Info))
            {
                Info.ContentPath = FPaths::Combine(FullPath, Dir);
                Results.Add(Info);
            }
        }
    }

    // 按优先级排序
    Algo::Sort(Results, [](const FModInfo& A, const FModInfo& B)
    {
        return A.LoadPriority < B.LoadPriority;
    });

    return Results;
}

bool UModLoader::ParseModManifest(const FString& ManifestPath, FModInfo& OutInfo)
{
    FString FileContent;
    if (!FFileHelper::LoadFileToString(FileContent, *ManifestPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("[ModLoader] Cannot read: %s"), *ManifestPath);
        return false;
    }

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileContent);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[ModLoader] Invalid JSON: %s"), *ManifestPath);
        return false;
    }

    OutInfo.ModID = JsonObject->GetStringField(TEXT("id"));
    OutInfo.DisplayName = JsonObject->GetStringField(TEXT("name"));
    OutInfo.Version = JsonObject->GetStringField(TEXT("version"));
    OutInfo.Author = JsonObject->GetStringField(TEXT("author"));
    OutInfo.Description = JsonObject->GetStringField(TEXT("description"));
    OutInfo.LoadPriority = JsonObject->GetIntegerField(TEXT("priority"));
    OutInfo.bEnabled = JsonObject->GetBoolField(TEXT("enabled"));

    const TArray<TSharedPtr<FJsonValue>>* Deps;
    if (JsonObject->TryGetArrayField(TEXT("dependencies"), Deps))
    {
        for (const TSharedPtr<FJsonValue>& V : *Deps)
            OutInfo.Dependencies.Add(V->AsString());
    }

    if (JsonObject->HasField(TEXT("checksum")))
        OutInfo.Checksum = JsonObject->GetStringField(TEXT("checksum"));

    return true;
}

EModLoadResult UModLoader::LoadMod(const FString& ModID)
{
    // 检查是否已加载
    if (LoadedMods.Contains(ModID))
        return EModLoadResult::DuplicateModID;

    // 查找 Mod
    FString FullPath = FPaths::Combine(FPaths::ProjectDir(), ModsRootDirectory);
    TArray<FModInfo> All = ScanForMods(ModsRootDirectory);

    FModInfo* Target = nullptr;
    for (FModInfo& M : All)
    {
        if (M.ModID == ModID)
        {
            Target = &M;
            break;
        }
    }

    if (!Target) return EModLoadResult::FileNotFound;

    // 校验
    if (bVerifyChecksum && !VerifyModIntegrity(*Target))
        return EModLoadResult::ChecksumFailed;

    // 依赖
    TArray<FString> LoadOrder;
    if (!ResolveDependencies(*Target, LoadOrder))
        return EModLoadResult::MissingDependency;

    // 先加载依赖
    for (const FString& DepID : LoadOrder)
    {
        if (!LoadedMods.Contains(DepID))
        {
            EModLoadResult DepResult = LoadMod(DepID);
            if (DepResult != EModLoadResult::Success) return DepResult;
        }
    }

    // 应用数据覆盖
    ApplyDataOverrides(*Target);

    // 执行入口脚本
    if (bEnableLuaScripting)
    {
        ExecuteModScript(ModID, TEXT("OnLoad"), {});
    }

    LoadedMods.Add(ModID, *Target);
    OnModLoaded.Broadcast(ModID);
    UE_LOG(LogTemp, Log, TEXT("[ModLoader] Loaded: %s v%s"), *ModID, *Target->Version);

    return EModLoadResult::Success;
}

bool UModLoader::UnloadMod(const FString& ModID)
{
    if (!LoadedMods.Contains(ModID)) return false;

    if (bEnableLuaScripting)
    {
        ExecuteModScript(ModID, TEXT("OnUnload"), {});
    }

    // 移除数据覆盖（简化：重启后生效）
    LoadedMods.Remove(ModID);
    UE_LOG(LogTemp, Log, TEXT("[ModLoader] Unloaded: %s"), *ModID);
    return true;
}

int32 UModLoader::LoadAllEnabledMods()
{
    TArray<FModInfo> All = ScanForMods(ModsRootDirectory);
    int32 LoadedCount = 0;

    for (const FModInfo& Mod : All)
    {
        if (Mod.bEnabled && !LoadedMods.Contains(Mod.ModID))
        {
            if (LoadMod(Mod.ModID) == EModLoadResult::Success)
                LoadedCount++;
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[ModLoader] Total loaded: %d"), LoadedCount);
    return LoadedCount;
}

EModLoadResult UModLoader::ReloadMod(const FString& ModID)
{
    UnloadMod(ModID);
    return LoadMod(ModID);
}

bool UModLoader::ResolveDependencies(const FModInfo& Mod, TArray<FString>& LoadOrder)
{
    for (const FString& Dep : Mod.Dependencies)
    {
        if (!LoadedMods.Contains(Dep))
        {
            // 递归检查（简化）
            LoadOrder.Add(Dep);
        }
    }
    return true; // 实际应检查循环依赖
}

void UModLoader::ApplyDataOverrides(const FModInfo& Mod)
{
    FString OverrideFile = FPaths::Combine(Mod.ContentPath, TEXT("overrides.json"));
    if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*OverrideFile))
        return;

    FString Content;
    FFileHelper::LoadFileToString(Content, *OverrideFile);

    TSharedPtr<FJsonObject> Json;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
    if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid()) return;

    const TArray<TSharedPtr<FJsonValue>>* Overrides;
    if (Json->TryGetArrayField(TEXT("overrides"), Overrides))
    {
        for (const TSharedPtr<FJsonValue>& V : *Overrides)
        {
            TSharedPtr<FJsonObject> Obj = V->AsObject();
            if (!Obj.IsValid()) continue;

            FString Table = Obj->GetStringField(TEXT("table"));
            FString Op = Obj->GetStringField(TEXT("operation"));
            FString RowID = Obj->GetStringField(TEXT("rowId"));

            TMap<FString, FString> Fields;
            const TSharedPtr<FJsonObject>* FieldsObj;
            if (Obj->TryGetObjectField(TEXT("fields"), FieldsObj))
            {
                for (const auto& Pair : (*FieldsObj)->Values)
                {
                    Fields.Add(Pair.Key, Pair.Value->AsString());
                }
            }

            // 存入覆盖表
            OverrideTables.FindOrAdd(Table).Add(RowID, Fields);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[ModLoader] Applied overrides from: %s"), *Mod.ModID);
}

bool UModLoader::VerifyModIntegrity(const FModInfo& Mod)
{
    if (Mod.Checksum.IsEmpty()) return true; // 无校验和则跳过

    // 简化：实际应计算文件夹 SHA-256
    FString FullPath = Mod.ContentPath;
    // ... 实际实现：遍历文件 → SHA-256 → 比较
    return true;
}

// —— 数据查询 ——

TMap<FString, FString> UModLoader::GetWeaponData(const FString& WeaponID)
{
    TMap<FString, FString> Result;

    // 基础数据（从默认表获取，这里简化）
    // ... 

    // 应用 Mod 覆盖
    if (TMap<FString, TMap<FString, FString>>* Table = OverrideTables.Find(TEXT("Weapons")))
    {
        if (TMap<FString, FString>* Row = Table->Find(WeaponID))
        {
            for (const auto& Pair : *Row)
                Result.Add(Pair.Key, Pair.Value);
        }
    }

    return Result;
}

TMap<FString, FString> UModLoader::GetShipData(const FString& ShipID)
{
    TMap<FString, FString> Result;
    if (TMap<FString, TMap<FString, FString>>* Table = OverrideTables.Find(TEXT("Ships")))
    {
        if (TMap<FString, FString>* Row = Table->Find(ShipID))
        {
            for (const auto& Pair : *Row)
                Result.Add(Pair.Key, Pair.Value);
        }
    }
    return Result;
}

TMap<FString, FString> UModLoader::GetQuestData(const FString& QuestID)
{
    TMap<FString, FString> Result;
    if (TMap<FString, TMap<FString, FString>>* Table = OverrideTables.Find(TEXT("Quests")))
    {
        if (TMap<FString, FString>* Row = Table->Find(QuestID))
        {
            for (const auto& Pair : *Row)
                Result.Add(Pair.Key, Pair.Value);
        }
    }
    return Result;
}

TArray<FModInfo> UModLoader::GetLoadedMods() const
{
    TArray<FModInfo> Result;
    for (const auto& Pair : LoadedMods)
        Result.Add(Pair.Value);
    return Result;
}

// —— Lua 脚本接口（骨架） ——

bool UModLoader::ExecuteModScript(const FString& ModID, const FString& FunctionName,
    const TArray<FString>& Args)
{
    if (!bEnableLuaScripting) return false;

    // 实际实现需要集成 Lua VM（推荐 UnLua 插件）
    // 这里只是骨架
    UE_LOG(LogTemp, Log, TEXT("[ModLoader] ExecuteScript: %s.%s"), *ModID, *FunctionName);
    return true;
}

void UModLoader::RegisterScriptAPI()
{
    // 注册 C++ 函数给 Lua 调用
    // 例：GiveItem / SpawnNPC / SetWeather / WarpToPlanet
    UE_LOG(LogTemp, Log, TEXT("[ModLoader] Script API registered"));
}
