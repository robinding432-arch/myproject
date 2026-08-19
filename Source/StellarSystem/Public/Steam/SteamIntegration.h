// SteamIntegration.h
// Steam 成就 + 云存档 + 好友/Rich Presence
#pragma once

#include "CoreMinimal.h"
#include "SteamIntegration.generated.h"

#if WITH_STEAMWORKS
#include "steam/steam_api.h"
#endif

// 成就 ID 枚举（与 Steamworks 后台一致）
UENUM(BlueprintType)
enum class EAchievement : uint8
{
    FirstLaunch      UMETA(DisplayName = "Welcome to the Stars"),
    FirstPlanetLand UMETA(DisplayName = "First Steps"),
    FirstWarp       UMETA(DisplayName = "Warp Drive Engaged"),
    VisitAll8Planets UMETA(DisplayName = "Solar Tourist"),
    KillEnemyShip    UMETA(DisplayName = "First Blood"),
    CollectRareItem  UMETA(DisplayName = "Treasure Hunter"),
    MaxLevelShip     UMETA(DisplayName = "Fully Upgraded"),
    SurviveStorm     UMETA(DisplayName = "Storm Rider"),
    CreditMillion    UMETA(DisplayName = "Millionaire"),
    DieFirstTime     UMETA(DisplayName = "Respawn Protocol")
};

// 云存档元数据
USTRUCT(BlueprintType)
struct FCloudSaveMeta
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere)
    FString FileName;

    UPROPERTY(VisibleAnywhere)
    int32 FileSize = 0;

    UPROPERTY(VisibleAnywhere)
    FDateTime LastModified;
};

UCLASS()
class USteamIntegration : public UObject
{
    GENERATED_BODY()

public:
    USteamIntegration();

    // ---- 生命周期 ----
    UFUNCTION(BlueprintCallable, Category = "Steam")
    bool Initialize();

    UFUNCTION(BlueprintCallable, Category = "Steam")
    void Shutdown();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Steam")
    bool IsSteamRunning() const;

    // ---- 成就 ----
    UFUNCTION(BlueprintCallable, Category = "Steam|Achievement")
    bool UnlockAchievement(EAchievement Achievement);

    UFUNCTION(BlueprintCallable, Category = "Steam|Achievement")
    bool ClearAchievement(EAchievement Achievement);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Steam|Achievement")
    bool HasAchievement(EAchievement Achievement) const;

    UFUNCTION(BlueprintCallable, Category = "Steam|Achievement")
    void ResetAllAchievements();

    // ---- 云存档 ----
    UFUNCTION(BlueprintCallable, Category = "Steam|Cloud")
    bool WriteCloudFile(const FString& FileName, const TArray<uint8>& Data);

    UFUNCTION(BlueprintCallable, Category = "Steam|Cloud")
    bool ReadCloudFile(const FString& FileName, TArray<uint8>& OutData);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Steam|Cloud")
    bool CloudFileExists(const FString& FileName) const;

    UFUNCTION(BlueprintCallable, Category = "Steam|Cloud")
    bool DeleteCloudFile(const FString& FileName);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Steam|Cloud")
    TArray<FCloudSaveMeta> GetCloudFileList() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Steam|Cloud")
    int32 GetCloudQuotaUsed() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Steam|Cloud")
    int32 GetCloudQuotaTotal() const;

    // ---- Rich Presence ----
    UFUNCTION(BlueprintCallable, Category = "Steam|RichPresence")
    void SetRichPresence(const FString& Key, const FString& Value);

    UFUNCTION(BlueprintCallable, Category = "Steam|RichPresence")
    void ClearRichPresence();

    // ---- 统计 ----
    UFUNCTION(BlueprintCallable, Category = "Steam|Stats")
    void IncrementStat(const FString& StatName, int32 Amount = 1);

    UFUNCTION(BlueprintCallable, Category = "Steam|Stats")
    void SetStat(const FString& StatName, int32 Value);

    UFUNCTION(BlueprintCallable, Category = "Steam|Stats")
    void StoreStats();

    // ---- 工具 ----
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Steam")
    FString GetSteamID() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Steam")
    FString GetPersonaName() const;

    // 网络事件回调（由 GameMode 调用）
    UFUNCTION(BlueprintCallable, Category = "Steam")
    void OnPlayerLandedOnPlanet(const FString& PlanetName);

    UFUNCTION(BlueprintCallable, Category = "Steam")
    void OnPlayerWarped(const FString& Destination);

    UFUNCTION(BlueprintCallable, Category = "Steam")
    void OnPlayerKilledEnemy();

    UFUNCTION(BlueprintCallable, Category = "Steam")
    void OnPlayerCollectedRare();

    UFUNCTION(BlueprintCallable, Category = "Steam")
    void OnPlayerDied();

protected:
    // 将 EAchievement 转为 Steam API 字符串
    FString AchievementToString(EAchievement A) const;

    // 已解锁缓存
    UPROPERTY()
    TSet<uint8> UnlockedAchievements;

    // 初始化状态
    bool bInitialized = false;

    // Steam 云存档前缀
    FString CloudPrefix = TEXT("StellarSave_");
};
