// ModLoader.h
// Mod 支持系统：JSON 数据驱动 + Lua 脚本接口
// 允许玩家/社区添加：飞船、武器、护甲、任务、NPC、星球

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ModLoader.generated.h"

class UDataTable;

// Mod 元信息
USTRUCT(BlueprintType)
struct FModInfo
{
    GENERATED_BODY()

    // Mod 唯一 ID
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ModID;

    // 显示名称
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString DisplayName;

    // 版本
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Version = TEXT("1.0.0");

    // 作者
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Author;

    // 描述
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Description;

    // Mod 文件夹路径
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ContentPath;

    // 加载优先级（数字越大越晚加载，可覆盖前面的）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 LoadPriority = 0;

    // 是否启用
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnabled = true;

    // 依赖 Mod ID 列表
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Dependencies;

    // 校验和（防止篡改）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Checksum;
};

// Mod 加载结果
UENUM(BlueprintType)
enum class EModLoadResult : uint8
{
    Success,
    FileNotFound,
    InvalidFormat,
    MissingDependency,
    VersionMismatch,
    ChecksumFailed,
    ScriptError,
    DuplicateModID
};

// 单个数据覆盖规则（JSON → 内存）
USTRUCT(BlueprintType)
struct FModDataOverride
{
    GENERATED_BODY()

    // 目标表名（如 "Weapons", "Ships", "Quests"）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TargetTable;

    // 操作类型
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Operation; // "Add" / "Modify" / "Remove" / "Replace"

    // 目标行 ID
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString RowID;

    // JSON 数据（字段键值对）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, FString> Fields;
};

// Mod 事件委托
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnModLoaded, FString, ModID);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnModLoadFailed, FString, ErrorMessage);

UCLASS(BlueprintType)
class UModLoader : public UObject
{
    GENERATED_BODY()

public:
    UModLoader();

    // —— 核心 API ——

    // 扫描 Mods/ 目录，返回发现的 Mod 列表
    UFUNCTION(BlueprintCallable, Category = "Mod")
    TArray<FModInfo> ScanForMods(const FString& ModsDirectory);

    // 加载单个 Mod
    UFUNCTION(BlueprintCallable, Category = "Mod")
    EModLoadResult LoadMod(const FString& ModID);

    // 卸载 Mod
    UFUNCTION(BlueprintCallable, Category = "Mod")
    bool UnloadMod(const FString& ModID);

    // 加载所有启用的 Mod（按优先级排序）
    UFUNCTION(BlueprintCallable, Category = "Mod")
    int32 LoadAllEnabledMods();

    // 重新加载（热更新）
    UFUNCTION(BlueprintCallable, Category = "Mod")
    EModLoadResult ReloadMod(const FString& ModID);

    // —— 数据查询（其他系统调用） ——

    // 获取 Mod 覆盖后的武器数据
    UFUNCTION(BlueprintCallable, Category = "Mod|Data")
    TMap<FString, FString> GetWeaponData(const FString& WeaponID);

    // 获取 Mod 覆盖后的飞船数据
    UFUNCTION(BlueprintCallable, Category = "Mod|Data")
    TMap<FString, FString> GetShipData(const FString& ShipID);

    // 获取 Mod 覆盖后的任务数据
    UFUNCTION(BlueprintCallable, Category = "Mod|Data")
    TMap<FString, FString> GetQuestData(const FString& QuestID);

    // 获取所有已注册 Mod 的列表
    UFUNCTION(BlueprintCallable, Category = "Mod|Info")
    TArray<FModInfo> GetLoadedMods() const;

    // —— Lua 脚本接口 ——

    // 执行 Mod 的 Lua 脚本
    UFUNCTION(BlueprintCallable, Category = "Mod|Script")
    bool ExecuteModScript(const FString& ModID, const FString& FunctionName,
        const TArray<FString>& Args);

    // 注册 C++ 函数给 Lua 调用
    UFUNCTION(BlueprintCallable, Category = "Mod|Script")
    void RegisterScriptAPI();

    // —— 事件 ——
    UPROPERTY(BlueprintAssignable, Category = "Mod|Events")
    FOnModLoaded OnModLoaded;

    UPROPERTY(BlueprintAssignable, Category = "Mod|Events")
    FOnModLoadFailed OnModLoadFailed;

    // —— 配置 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mod|Config")
    FString ModsRootDirectory = TEXT("Mods");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mod|Config")
    bool bEnableLuaScripting = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mod|Config")
    bool bEnableHotReload = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mod|Config")
    bool bVerifyChecksum = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mod|Config")
    int32 MaxModsLoaded = 64;

    // 初始化（GameMode BeginPlay 时调用）
    UFUNCTION(BlueprintCallable, Category = "Mod")
    void Initialize();

private:
    // 已加载 Mod
    UPROPERTY()
    TMap<FString, FModInfo> LoadedMods;

    // 数据覆盖表（按 TargetTable → RowID → Fields）
    TMap<FString, TMap<FString, TMap<FString, FString>>> OverrideTables;

    // 依赖解析
    bool ResolveDependencies(const FModInfo& Mod, TArray<FString>& LoadOrder);

    // 解析 mod.json
    bool ParseModManifest(const FString& ManifestPath, FModInfo& OutInfo);

    // 应用数据覆盖
    void ApplyDataOverrides(const FModInfo& Mod);

    // 校验和验证
    bool VerifyModIntegrity(const FModInfo& Mod);

    // Lua 虚拟机（简化接口，实际用 UnLua/sol2）
    void* LuaState = nullptr;
};
