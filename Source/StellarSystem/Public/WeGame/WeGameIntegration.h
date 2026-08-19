// WeGameIntegration.h
// v6.9 — WeGame Rail SDK 核心集成（成就/云存档/统计/Rich Presence）
// 通过 Rail SDK C++ API 接入 WeGame 平台服务
// 所有 Rail SDK 调用通过条件编译隔离，未安装 SDK 时可编译通过

#pragma once

#include "CoreMinimal.h"
#include "WeGameIntegration.generated.h"

// ============================================================
//  编译开关
//  - WITH_WEGAME=1  → 启用 WeGame Rail SDK 调用
//  - WITH_WEGAME=0  → 全部接口退化为空实现
//  在 Build.cs 中根据 Target.Platform == Win64 自动开启
// ============================================================

#ifndef WITH_WEGAME
#define WITH_WEGAME 0
#endif

#if WITH_WEGAME
// Rail SDK 头文件（从 WeGame 开发者平台下载的 SDK 包中获取）
// 路径示例：ThirdParty/RailSDK/include/rail/sdk/rail_api.h
// 在 Build.cs 中通过 PublicIncludePaths 引入
#include "rail/sdk/rail_api.h"
#endif

// —— 成就 ID 枚举（与 WeGame 开发者平台配置的 API 名称一致）——
UENUM(BlueprintType)
enum class EWeGameAchievement : uint8
{
    FirstLaunch       UMETA(DisplayName = "Welcome to the Stars"),
    FirstPlanetLand   UMETA(DisplayName = "First Steps"),
    FirstWarp         UMETA(DisplayName = "Warp Drive Engaged"),
    VisitAll8Planets  UMETA(DisplayName = "Solar Tourist"),
    KillEnemyShip     UMETA(DisplayName = "First Blood"),
    CollectRareItem   UMETA(DisplayName = "Treasure Hunter"),
    MaxLevelShip      UMETA(DisplayName = "Fully Upgraded"),
    SurviveStorm      UMETA(DisplayName = "Storm Rider"),
    CreditMillion     UMETA(DisplayName = "Millionaire"),
    DieFirstTime      UMETA(DisplayName = "Respawn Protocol"),
    WeGameFirstLogin  UMETA(DisplayName = "WeGame Pioneer"),
    PvPVictory        UMETA(DisplayName = "Space Ace"),
    TradeMaster       UMETA(DisplayName = "Merchant Prince"),
    FactionMaxRank    UMETA(DisplayName = "Faction Leader"),
    SurviveEVA        UMETA(DisplayName = "EVA Survivor")
};

// —— 云存档元数据 ——
USTRUCT(BlueprintType)
struct FWeGameCloudSaveMeta
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FString FileName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 FileSize = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FDateTime LastModified;
};

// —— 防沉迷状态 ——
UENUM(BlueprintType)
enum class EAntiAddictionState : uint8
{
    Unknown        UMETA(DisplayName = "Unknown"),
    Adult          UMETA(DisplayName = "Adult (No Limit)"),
    MinorStrict    UMETA(DisplayName = "Minor (Strict Limits)"),
    MinorRelaxed   UMETA(DisplayName = "Minor (Relaxed Limits)"),
    Unverified     UMETA(DisplayName = "Unverified")
};

// —— 防沉迷指令类型 ——
UENUM(BlueprintType)
enum class EAntiAddictionAction : uint8
{
    None           UMETA(DisplayName = "None"),
    ShowTips       UMETA(DisplayName = "Show Tips Dialog"),
    ForceExit      UMETA(DisplayName = "Force Exit Game"),
    TimeWarning    UMETA(DisplayName = "Time Warning"),
    CurfewActive   UMETA(DisplayName = "Curfew Active")
};

// —— 防沉迷事件数据 ——
USTRUCT(BlueprintType)
struct FAntiAddictionEvent
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    EAntiAddictionAction Action = EAntiAddictionAction::None;

    UPROPERTY(BlueprintReadOnly)
    FString Title;

    UPROPERTY(BlueprintReadOnly)
    FString Content;

    UPROPERTY(BlueprintReadOnly)
    int32 DisplayDurationSeconds = 60;
};

// —— 玩家身份信息 ——
USTRUCT(BlueprintType)
struct FWeGamePlayerIdentity
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString RailID;          // WeGame 用户唯一标识

    UPROPERTY(BlueprintReadOnly)
    FString DisplayName;      // WeGame 昵称

    UPROPERTY(BlueprintReadOnly)
    FString SessionTicket;    // 会话票据（用于服务端验证）

    UPROPERTY(BlueprintReadOnly)
    bool bIsValid = false;
};

// ============================================================
//  UWeGameIntegration
//  对标 USteamIntegration，提供成就/云存档/统计/Rich Presence
//  单例通过 GameInstanceSubsystem 管理生命周期
//  通过 FTickableGameObject 接口实现每帧 Tick
// ============================================================
UCLASS()
class STELLARSYSTEM_API UWeGameIntegration : public UGameInstanceSubsystem, public FTickableGameObject
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ---- 生命周期 ----
    UFUNCTION(BlueprintCallable, Category = "WeGame")
    bool InitializeSDK(int32 AppId, const FString& AppVersion);

    UFUNCTION(BlueprintCallable, Category = "WeGame")
    void ShutdownSDK();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WeGame")
    bool IsSDKInitialized() const { return bSDKInitialized; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WeGame")
    bool IsWeGameRunning() const;

    // ---- 玩家身份 ----
    UFUNCTION(BlueprintCallable, Category = "WeGame|Identity")
    bool AcquireSessionTicket();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WeGame|Identity")
    FWeGamePlayerIdentity GetLocalPlayerIdentity() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WeGame|Identity")
    FString GetRailID() const { return LocalIdentity.RailID; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WeGame|Identity")
    FString GetDisplayName() const { return LocalIdentity.DisplayName; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WeGame|Identity")
    FString GetSessionTicket() const { return LocalIdentity.SessionTicket; }

    // ---- 成就 ----
    UFUNCTION(BlueprintCallable, Category = "WeGame|Achievement")
    bool UnlockAchievement(EWeGameAchievement Achievement);

    UFUNCTION(BlueprintCallable, Category = "WeGame|Achievement")
    bool ClearAchievement(EWeGameAchievement Achievement);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WeGame|Achievement")
    bool HasAchievement(EWeGameAchievement Achievement) const;

    UFUNCTION(BlueprintCallable, Category = "WeGame|Achievement")
    void ResetAllAchievements();

    UFUNCTION(BlueprintCallable, Category = "WeGame|Achievement")
    void SetAchievementProgress(EWeGameAchievement Achievement, int32 CurrentValue, int32 MaxValue);

    // ---- 云存档（IRailFile 方式，本地路径由平台自动同步）----
    UFUNCTION(BlueprintCallable, Category = "WeGame|Cloud")
    bool WriteCloudFile(const FString& FileName, const TArray<uint8>& Data);

    UFUNCTION(BlueprintCallable, Category = "WeGame|Cloud")
    bool ReadCloudFile(const FString& FileName, TArray<uint8>& OutData);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WeGame|Cloud")
    bool CloudFileExists(const FString& FileName) const;

    UFUNCTION(BlueprintCallable, Category = "WeGame|Cloud")
    bool DeleteCloudFile(const FString& FileName);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WeGame|Cloud")
    TArray<FWeGameCloudSaveMeta> GetCloudFileList() const;

    // ---- Rich Presence ----
    UFUNCTION(BlueprintCallable, Category = "WeGame|RichPresence")
    void SetRichPresence(const FString& Key, const FString& Value);

    UFUNCTION(BlueprintCallable, Category = "WeGame|RichPresence")
    void ClearRichPresence();

    // ---- 统计 ----
    UFUNCTION(BlueprintCallable, Category = "WeGame|Stats")
    void IncrementStat(const FString& StatName, int32 Amount = 1);

    UFUNCTION(BlueprintCallable, Category = "WeGame|Stats")
    void SetStat(const FString& StatName, int32 Value);

    UFUNCTION(BlueprintCallable, Category = "WeGame|Stats")
    void StoreStats();

    // ---- 排行榜 ----
    UFUNCTION(BlueprintCallable, Category = "WeGame|Leaderboard")
    void UpdateLeaderboard(const FString& LeaderboardName, int32 Score);

    // ---- 事件分发（由 GameMode 调用）----
    UFUNCTION(BlueprintCallable, Category = "WeGame|Events")
    void OnPlayerLandedOnPlanet(const FString& PlanetName);

    UFUNCTION(BlueprintCallable, Category = "WeGame|Events")
    void OnPlayerWarped(const FString& Destination);

    UFUNCTION(BlueprintCallable, Category = "WeGame|Events")
    void OnPlayerKilledEnemy();

    UFUNCTION(BlueprintCallable, Category = "WeGame|Events")
    void OnPlayerCollectedRare();

    UFUNCTION(BlueprintCallable, Category = "WeGame|Events")
    void OnPlayerDied();

    UFUNCTION(BlueprintCallable, Category = "WeGame|Events")
    void OnPlayerPvPWin();

    UFUNCTION(BlueprintCallable, Category = "WeGame|Events")
    void OnPlayerTradeCompleted(int32 CreditAmount);

    // ---- 防沉迷 ----
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WeGame|AntiAddiction")
    EAntiAddictionState GetAntiAddictionState() const { return AntiAddictionState; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WeGame|AntiAddiction")
    bool IsPlaytimeAllowed() const { return bPlaytimeAllowed; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WeGame|AntiAddiction")
    int32 GetRemainingPlaytimeMinutes() const { return RemainingPlaytimeMinutes; }

    // 处理防沉迷事件（由 SDK 回调触发）
    void ProcessAntiAddictionEvent(const FAntiAddictionEvent& Event);

    // 委托：防沉迷状态变化
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAntiAddictionStateChanged, EAntiAddictionState, NewState);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAntiAddictionAction, FAntiAddictionEvent, Event);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSessionTicketReceived, bool, bSuccess);

    UPROPERTY(BlueprintAssignable, Category = "WeGame|AntiAddiction")
    FOnAntiAddictionStateChanged OnAntiAddictionStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "WeGame|AntiAddiction")
    FOnAntiAddictionAction OnAntiAddictionAction;

    UPROPERTY(BlueprintAssignable, Category = "WeGame|Events")
    FOnSessionTicketReceived OnSessionTicketReceived;

    // ---- 敏感词过滤 ----
    UFUNCTION(BlueprintCallable, Category = "WeGame|Content")
    FString FilterDirtyWords(const FString& InputText) const;

    // ---- 内购 ----
    UFUNCTION(BlueprintCallable, Category = "WeGame|Store")
    void RequestPurchasableProducts();

    UFUNCTION(BlueprintCallable, Category = "WeGame|Store")
    void ShowPaymentWindow(const FString& OrderID);

    // 委托：内购结果
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPurchaseResult, bool, bSuccess, const FString&, OrderID);
    UPROPERTY(BlueprintAssignable, Category = "WeGame|Store")
    FOnPurchaseResult OnPurchaseResult;

    // ---- 心跳（每帧调用，驱动 Rail SDK 事件循环）----
    void Tick(float DeltaTime);

protected:
    // 将 EWeGameAchievement 转为 Rail SDK 字符串
    FString AchievementToString(EWeGameAchievement A) const;

    // SDK 初始化状态
    bool bSDKInitialized = false;

    // 本地玩家身份
    FWeGamePlayerIdentity LocalIdentity;

    // 防沉迷状态
    EAntiAddictionState AntiAddictionState = EAntiAddictionState::Unknown;
    bool bPlaytimeAllowed = true;
    int32 RemainingPlaytimeMinutes = 0;

    // 已解锁成就缓存
    UPROPERTY()
    TSet<uint8> UnlockedAchievements;

    // 云存档前缀
    FString CloudPrefix = TEXT("StellarSave_");

    // 内部计时器（用于 Tick 节流）
    float EventPollAccumulator = 0.0f;
    static constexpr float EventPollInterval = 0.1f; // 100ms 轮询一次 RailFireEvents

    // ---- FTickableGameObject 接口 ----
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override { return bSDKInitialized; }
    virtual TStatId GetStatId() const override { return TStatId(); }

    // 应用信息
    int32 CachedAppId = 0;
};
