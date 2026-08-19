// WeGameIntegration.cpp
// v6.9 — WeGame Rail SDK 核心集成实现

#include "WeGame/WeGameIntegration.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "GameFramework/GameModeBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogWeGame, Log, All);

// ============================================================
//  生命周期
// ============================================================

void UWeGameIntegration::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    bSDKInitialized = false;
    AntiAddictionState = EAntiAddictionState::Unknown;
    bPlaytimeAllowed = true;
    RemainingPlaytimeMinutes = 0;
    EventPollAccumulator = 0.0f;

    UE_LOG(LogWeGame, Log, TEXT("UWeGameIntegration::Initialize — subsystem created (SDK not yet initialized)"));
}

void UWeGameIntegration::Deinitialize()
{
    ShutdownSDK();
    Super::Deinitialize();
}

bool UWeGameIntegration::InitializeSDK(int32 AppId, const FString& AppVersion)
{
#if WITH_WEGAME
    if (bSDKInitialized)
    {
        UE_LOG(LogWeGame, Warning, TEXT("InitializeSDK called but SDK already initialized"));
        return true;
    }

    // Step 1: 加载 rail_api.dll / rail_api64.dll
    // 在 Windows 上 Rail SDK 通常通过 Delay Load 自动加载
    // 在 Linux 上需要显式 dlopen
    // Build.cs 中通过 PublicAdditionalLibraries 链接

    // Step 2: 检查环境（命令行参数由 WeGame 客户端传入）
    // RailNeedRestartAppForCheckingEnvironment 检查是否需要重启以通过环境校验
    int32 ArgC = 0;
    const char** ArgV = nullptr;

    // 从 FCommandLine 获取参数
    FString CommandLine = FCommandLine::Get();

    // Step 3: 初始化 SDK
    rail::RailResult Result = rail::RailNeedRestartAppForCheckingEnvironment(0, ArgC, ArgV);
    if (Result != rail::kSuccess)
    {
        UE_LOG(LogWeGame, Warning, TEXT("RailNeedRestartAppForCheckingEnvironment returned %d"), (int32)Result);
        // 非致命：可能在 IDE 中直接运行（调试模式）
    }

    // 真正初始化
    rail::RailInitializeArgs InitArgs;
    FMemory::Memzero(&InitArgs, sizeof(InitArgs));
    InitArgs.rail_app_id = static_cast<uint32>(AppId);
    InitArgs.rail_app_version = TCHAR_TO_ANSI(*AppVersion);
    InitArgs.rail_environment = "development"; // 发布时改为 "release"

    Result = rail::RailInitialize(&InitArgs);
    if (Result != rail::kSuccess)
    {
        UE_LOG(LogWeGame, Error, TEXT("RailInitialize FAILED, result=%d"), (int32)Result);
        return false;
    }

    bSDKInitialized = true;
    CachedAppId = AppId;

    UE_LOG(LogWeGame, Log, TEXT("Rail SDK initialized. AppID=%d Version=%s"), AppId, *AppVersion);

    // Step 4: 注册系统状态回调（必须监听）
    // 在 Rail SDK 中通过 RailRegisterEvent 注册
    // 这里注册 RailSystemStateChanged 事件
    // 注意：具体回调注册方式取决于 Rail SDK 版本
    // 以下为示意代码，实际 API 以官方 SDK 为准
    /*
    rail::RailRegisterEvent(
        rail::kRailEventSystemStateChanged,
        this,
        &UWeGameIntegration::OnSystemStateChanged
    );
    */

    // Step 5: 获取本地玩家身份
    rail::IRailPlayer* PlayerHelper = rail::RailFactory()->RailPlayer();
    if (PlayerHelper)
    {
        rail::RailID LocalRailID = PlayerHelper->GetRailID();
        LocalIdentity.RailID = FString::Printf(TEXT("%llu"), LocalRailID.get_id());
        LocalIdentity.DisplayName = FString(ANSI_TO_TCHAR(PlayerHelper->GetPlayerName().c_str()));
        LocalIdentity.bIsValid = !LocalIdentity.RailID.IsEmpty();
    }

    // 触发登录事件
    OnSessionTicketReceived.Broadcast(true);

    return true;
#else
    UE_LOG(LogWeGame, Log, TEXT("WITH_WEGAME=0 — SDK initialization skipped (stub mode)"));
    // Stub 模式：模拟一个虚拟身份，方便在 IDE 中调试
    LocalIdentity.RailID = TEXT("WeGame_Stub_0001");
    LocalIdentity.DisplayName = TEXT("WeGame Player (Stub)");
    LocalIdentity.bIsValid = true;
    bSDKInitialized = true;
    CachedAppId = AppId;
    OnSessionTicketReceived.Broadcast(true);
    return true;
#endif
}

void UWeGameIntegration::ShutdownSDK()
{
#if WITH_WEGAME
    if (bSDKInitialized)
    {
        rail::RailFinalize();
        UE_LOG(LogWeGame, Log, TEXT("Rail SDK finalized"));
    }
#endif
    bSDKInitialized = false;
    LocalIdentity.bIsValid = false;
    AntiAddictionState = EAntiAddictionState::Unknown;
    bPlaytimeAllowed = true;
    UnlockedAchievements.Empty();
}

bool UWeGameIntegration::IsWeGameRunning() const
{
#if WITH_WEGAME
    // 检查 WeGame 客户端是否正在运行
    // Rail SDK 提供 RailIsPlatformValid() 或类似接口
    return bSDKInitialized;
#else
    return false;
#endif
}

// ============================================================
//  玩家身份 / Session Ticket
// ============================================================

bool UWeGameIntegration::AcquireSessionTicket()
{
#if WITH_WEGAME
    if (!bSDKInitialized) return false;

    rail::IRailPlayer* PlayerHelper = rail::RailFactory()->RailPlayer();
    if (!PlayerHelper) return false;

    // 异步获取会话票据，结果通过 kRailEventPlayerAcquireSessionTicketResult 回调
    rail::RailResult Result = PlayerHelper->AsyncAcquireSessionTicket("WeGameAuth");
    if (Result == rail::kSuccess)
    {
        UE_LOG(LogWeGame, Log, TEXT("AsyncAcquireSessionTicket initiated"));
        return true;
    }
    UE_LOG(LogWeGame, Warning, TEXT("AsyncAcquireSessionTicket failed: %d"), (int32)Result);
    return false;
#else
    // Stub
    LocalIdentity.SessionTicket = TEXT("STUB_TICKET_") + FString::FromInt(FMath::Rand());
    UE_LOG(LogWeGame, Log, TEXT("Stub session ticket acquired"));
    return true;
#endif
}

FWeGamePlayerIdentity UWeGameIntegration::GetLocalPlayerIdentity() const
{
    return LocalIdentity;
}

// ============================================================
//  成就
// ============================================================

bool UWeGameIntegration::UnlockAchievement(EWeGameAchievement Achievement)
{
    if (!bSDKInitialized) return false;

    FString AchievementName = AchievementToString(Achievement);
    if (UnlockedAchievements.Contains((uint8)Achievement))
    {
        return true; // 已解锁，幂等
    }

#if WITH_WEGAME
    rail::IRailAchievementHelper* Helper = rail::RailFactory()->RailAchievementHelper();
    if (!Helper) return false;

    rail::IRailPlayerAchievement* PlayerAch = Helper->CreatePlayerAchievement(rail::RailID());
    if (!PlayerAch) return false;

    rail::RailResult Result = PlayerAch->MakeAchievement(TCHAR_TO_ANSI(*AchievementName));
    if (Result == rail::kSuccess)
    {
        // 持久化到云端
        PlayerAch->AsyncStoreAchievement("UnlockAchievement");
        UnlockedAchievements.Add((uint8)Achievement);
        UE_LOG(LogWeGame, Log, TEXT("Achievement unlocked: %s"), *AchievementName);
        return true;
    }
    PlayerAch->Release();
    return false;
#else
    // Stub
    UnlockedAchievements.Add((uint8)Achievement);
    UE_LOG(LogWeGame, Log, TEXT("[STUB] Achievement unlocked: %s"), *AchievementName);
    return true;
#endif
}

bool UWeGameIntegration::ClearAchievement(EWeGameAchievement Achievement)
{
    if (!bSDKInitialized) return false;

#if WITH_WEGAME
    rail::IRailAchievementHelper* Helper = rail::RailFactory()->RailAchievementHelper();
    if (!Helper) return false;

    rail::IRailPlayerAchievement* PlayerAch = Helper->CreatePlayerAchievement(rail::RailID());
    if (!PlayerAch) return false;

    FString Name = AchievementToString(Achievement);
    rail::RailResult Result = PlayerAch->CancelAchievement(TCHAR_TO_ANSI(*Name));
    if (Result == rail::kSuccess)
    {
        UnlockedAchievements.Remove((uint8)Achievement);
    }
    PlayerAch->Release();
    return Result == rail::kSuccess;
#else
    UnlockedAchievements.Remove((uint8)Achievement);
    return true;
#endif
}

bool UWeGameIntegration::HasAchievement(EWeGameAchievement Achievement) const
{
    return UnlockedAchievements.Contains((uint8)Achievement);
}

void UWeGameIntegration::ResetAllAchievements()
{
#if WITH_WEGAME
    if (!bSDKInitialized) return;
    rail::IRailAchievementHelper* Helper = rail::RailFactory()->RailAchievementHelper();
    if (!Helper) return;

    rail::IRailPlayerAchievement* PlayerAch = Helper->CreatePlayerAchievement(rail::RailID());
    if (PlayerAch)
    {
        PlayerAch->ResetAllAchievements();
        PlayerAch->Release();
    }
#endif
    UnlockedAchievements.Empty();
}

void UWeGameIntegration::SetAchievementProgress(EWeGameAchievement Achievement, int32 CurrentValue, int32 MaxValue)
{
    if (!bSDKInitialized) return;

#if WITH_WEGAME
    rail::IRailAchievementHelper* Helper = rail::RailFactory()->RailAchievementHelper();
    if (!Helper) return;

    rail::IRailPlayerAchievement* PlayerAch = Helper->CreatePlayerAchievement(rail::RailID());
    if (!PlayerAch) return;

    FString Name = AchievementToString(Achievement);
    PlayerAch->AsyncTriggerAchievementProgress(TCHAR_TO_ANSI(*Name), CurrentValue, MaxValue, "");
    PlayerAch->Release();
#else
    UE_LOG(LogWeGame, Log, TEXT("[STUB] Achievement progress: %s %d/%d"),
        *AchievementToString(Achievement), CurrentValue, MaxValue);
#endif
}

FString UWeGameIntegration::AchievementToString(EWeGameAchievement A) const
{
    switch (A)
    {
        case EWeGameAchievement::FirstLaunch:       return TEXT("game_achievement_first_launch");
        case EWeGameAchievement::FirstPlanetLand:   return TEXT("game_achievement_first_land");
        case EWeGameAchievement::FirstWarp:         return TEXT("game_achievement_first_warp");
        case EWeGameAchievement::VisitAll8Planets:  return TEXT("game_achievement_visit_all_planets");
        case EWeGameAchievement::KillEnemyShip:     return TEXT("game_achievement_first_blood");
        case EWeGameAchievement::CollectRareItem:   return TEXT("game_achievement_treasure_hunter");
        case EWeGameAchievement::MaxLevelShip:      return TEXT("game_achievement_fully_upgraded");
        case EWeGameAchievement::SurviveStorm:      return TEXT("game_achievement_storm_rider");
        case EWeGameAchievement::CreditMillion:     return TEXT("game_achievement_millionaire");
        case EWeGameAchievement::DieFirstTime:       return TEXT("game_achievement_respawn_protocol");
        case EWeGameAchievement::WeGameFirstLogin:   return TEXT("game_achievement_wegame_pioneer");
        case EWeGameAchievement::PvPVictory:        return TEXT("game_achievement_space_ace");
        case EWeGameAchievement::TradeMaster:       return TEXT("game_achievement_merchant_prince");
        case EWeGameAchievement::FactionMaxRank:    return TEXT("game_achievement_faction_leader");
        case EWeGameAchievement::SurviveEVA:        return TEXT("game_achievement_eva_survivor");
        default: return TEXT("game_achievement_unknown");
    }
}

// ============================================================
//  云存档（IRailFile 方式）
// ============================================================

bool UWeGameIntegration::WriteCloudFile(const FString& FileName, const TArray<uint8>& Data)
{
    if (!bSDKInitialized) return false;

    FString FullName = CloudPrefix + FileName;

#if WITH_WEGAME
    rail::IRailStorageHelper* Storage = rail::RailFactory()->StorageHelper();
    if (!Storage) return false;

    rail::RailResult Result;
    rail::IRailFile* File = Storage->CreateFile(TCHAR_TO_ANSI(*FullName), &Result);
    if (!File || Result != rail::kSuccess) return false;

    // IRailFile::Write 接受 const void* + size_t
    Result = File->Write(Data.GetData(), Data.Num());
    File->Close();
    File->Release();

    if (Result == rail::kSuccess)
    {
        UE_LOG(LogWeGame, Log, TEXT("Cloud write: %s (%d bytes)"), *FullName, Data.Num());
        return true;
    }
    return false;
#else
    // Stub: 写入本地文件
    FString SavePath = FPaths::ProjectSavedDir() / TEXT("WeGameCloud") / FullName;
    FFileHelper::SaveArrayToFile(Data, *SavePath);
    UE_LOG(LogWeGame, Log, TEXT("[STUB] Cloud write: %s (%d bytes)"), *FullName, Data.Num());
    return true;
#endif
}

bool UWeGameIntegration::ReadCloudFile(const FString& FileName, TArray<uint8>& OutData)
{
    if (!bSDKInitialized) return false;

    FString FullName = CloudPrefix + FileName;
    OutData.Empty();

#if WITH_WEGAME
    rail::IRailStorageHelper* Storage = rail::RailFactory()->StorageHelper();
    if (!Storage) return false;

    rail::RailResult Result;
    rail::IRailFile* File = Storage->OpenFile(TCHAR_TO_ANSI(*FullName), &Result);
    if (!File || Result != rail::kSuccess) return false;

    uint32 FileSize = File->GetFileSize();
    if (FileSize == 0)
    {
        File->Close();
        File->Release();
        return true; // 空文件不算失败
    }

    OutData.SetNumUninitialized((int32)FileSize);
    Result = File->Read(OutData.GetData(), FileSize);
    File->Close();
    File->Release();

    return Result == rail::kSuccess;
#else
    FString SavePath = FPaths::ProjectSavedDir() / TEXT("WeGameCloud") / FullName;
    bool bLoaded = FFileHelper::LoadFileToArray(OutData, *SavePath);
    UE_LOG(LogWeGame, Log, TEXT("[STUB] Cloud read: %s (%d bytes, %s)"),
        *FullName, OutData.Num(), bLoaded ? TEXT("OK") : TEXT("FAIL"));
    return bLoaded;
#endif
}

bool UWeGameIntegration::CloudFileExists(const FString& FileName) const
{
    if (!bSDKInitialized) return false;

    FString FullName = CloudPrefix + FileName;

#if WITH_WEGAME
    rail::IRailStorageHelper* Storage = rail::RailFactory()->StorageHelper();
    if (!Storage) return false;

    // 尝试打开文件，成功即存在
    rail::RailResult Result;
    rail::IRailFile* File = Storage->OpenFile(TCHAR_TO_ANSI(*FullName), &Result);
    if (File)
    {
        File->Close();
        File->Release();
    }
    return Result == rail::kSuccess;
#else
    FString SavePath = FPaths::ProjectSavedDir() / TEXT("WeGameCloud") / FullName;
    return FPaths::FileExists(SavePath);
#endif
}

bool UWeGameIntegration::DeleteCloudFile(const FString& FileName)
{
    if (!bSDKInitialized) return false;

    FString FullName = CloudPrefix + FileName;

#if WITH_WEGAME
    rail::IRailStorageHelper* Storage = rail::RailFactory()->StorageHelper();
    if (!Storage) return false;

    rail::RailResult Result = Storage->DeleteFile(TCHAR_TO_ANSI(*FullName));
    return Result == rail::kSuccess;
#else
    FString SavePath = FPaths::ProjectSavedDir() / TEXT("WeGameCloud") / FullName;
    if (FPaths::FileExists(SavePath))
    {
        IFileManager::Get().Delete(*SavePath);
    }
    return true;
#endif
}

TArray<FWeGameCloudSaveMeta> UWeGameIntegration::GetCloudFileList() const
{
    TArray<FWeGameCloudSaveMeta> Result;

#if WITH_WEGAME
    if (!bSDKInitialized) return Result;
    // Rail SDK 不直接提供文件列表枚举
    // 需要在开发者平台配置已知文件名列表，逐条查询
    // 这里返回空数组，由上层用已知文件名列表查询
#endif

    return Result;
}

// ============================================================
//  Rich Presence
// ============================================================

void UWeGameIntegration::SetRichPresence(const FString& Key, const FString& Value)
{
#if WITH_WEGAME
    if (!bSDKInitialized) return;
    // Rail SDK 通过 IRailPlayer::SetRichPresence 设置
    rail::IRailPlayer* Player = rail::RailFactory()->RailPlayer();
    if (Player)
    {
        Player->SetRichPresence(TCHAR_TO_ANSI(*Key), TCHAR_TO_ANSI(*Value));
    }
#else
    UE_LOG(LogWeGame, Verbose, TEXT("[STUB] RichPresence: %s=%s"), *Key, *Value);
#endif
}

void UWeGameIntegration::ClearRichPresence()
{
#if WITH_WEGAME
    if (!bSDKInitialized) return;
    rail::IRailPlayer* Player = rail::RailFactory()->RailPlayer();
    if (Player)
    {
        Player->ClearRichPresence();
    }
#endif
}

// ============================================================
//  统计
// ============================================================

void UWeGameIntegration::IncrementStat(const FString& StatName, int32 Amount)
{
#if WITH_WEGAME
    if (!bSDKInitialized) return;
    // Rail SDK 统计接口
    // 通过 IRailStatsHelper 或类似接口
    // 以下为示意
    /*
    rail::IRailStatsHelper* Stats = rail::RailFactory()->RailStatsHelper();
    if (Stats)
    {
        Stats->IncrementStat(TCHAR_TO_ANSI(*StatName), Amount);
    }
    */
#else
    UE_LOG(LogWeGame, Verbose, TEXT("[STUB] IncrementStat: %s +%d"), *StatName, Amount);
#endif
}

void UWeGameIntegration::SetStat(const FString& StatName, int32 Value)
{
#if WITH_WEGAME
    if (!bSDKInitialized) return;
    /*
    rail::IRailStatsHelper* Stats = rail::RailFactory()->RailStatsHelper();
    if (Stats)
    {
        Stats->SetStat(TCHAR_TO_ANSI(*StatName), Value);
    }
    */
#else
    UE_LOG(LogWeGame, Verbose, TEXT("[STUB] SetStat: %s =%d"), *StatName, Value);
#endif
}

void UWeGameIntegration::StoreStats()
{
#if WITH_WEGAME
    if (!bSDKInitialized) return;
    /*
    rail::IRailStatsHelper* Stats = rail::RailFactory()->RailStatsHelper();
    if (Stats)
    {
        Stats->StoreStats();
    }
    */
#endif
}

// ============================================================
//  排行榜
// ============================================================

void UWeGameIntegration::UpdateLeaderboard(const FString& LeaderboardName, int32 Score)
{
#if WITH_WEGAME
    if (!bSDKInitialized) return;

    rail::IRailLeaderboardHelper* LB = rail::RailFactory()->RailLeaderboardHelper();
    if (!LB) return;

    // 注册回调事件: LeaderboardUploaded
    // 然后调用 UploadLeaderboardScore
    LB->UploadLeaderboardScore(TCHAR_TO_ANSI(*LeaderboardName), Score, nullptr, 0, rail::kRailLeaderboardUploadScoreMethodForceUpdate);
#else
    UE_LOG(LogWeGame, Log, TEXT("[STUB] Leaderboard %s: %d"), *LeaderboardName, Score);
#endif
}

// ============================================================
//  事件分发（由 GameMode 调用）
// ============================================================

void UWeGameIntegration::OnPlayerLandedOnPlanet(const FString& PlanetName)
{
    UnlockAchievement(EWeGameAchievement::FirstPlanetLand);
    IncrementStat(TEXT("planets_visited"), 1);
    SetRichPresence(TEXT("status"), FString::Printf(TEXT("Exploring %s"), *PlanetName));
}

void UWeGameIntegration::OnPlayerWarped(const FString& Destination)
{
    UnlockAchievement(EWeGameAchievement::FirstWarp);
    IncrementStat(TEXT("warps_completed"), 1);
    SetRichPresence(TEXT("status"), FString::Printf(TEXT("Warping to %s"), *Destination));
}

void UWeGameIntegration::OnPlayerKilledEnemy()
{
    UnlockAchievement(EWeGameAchievement::KillEnemyShip);
    IncrementStat(TEXT("enemies_killed"), 1);
}

void UWeGameIntegration::OnPlayerCollectedRare()
{
    UnlockAchievement(EWeGameAchievement::CollectRareItem);
    IncrementStat(TEXT("rare_items_found"), 1);
}

void UWeGameIntegration::OnPlayerDied()
{
    UnlockAchievement(EWeGameAchievement::DieFirstTime);
    IncrementStat(TEXT("death_count"), 1);
}

void UWeGameIntegration::OnPlayerPvPWin()
{
    UnlockAchievement(EWeGameAchievement::PvPVictory);
    IncrementStat(TEXT("pvp_wins"), 1);
    UpdateLeaderboard(TEXT("pvp_elo"), 1500); // 初始 ELO
}

void UWeGameIntegration::OnPlayerTradeCompleted(int32 CreditAmount)
{
    IncrementStat(TEXT("credits_earned"), CreditAmount);
    if (CreditAmount >= 1000000)
    {
        UnlockAchievement(EWeGameAchievement::CreditMillion);
    }
    if (CreditAmount >= 100000)
    {
        UnlockAchievement(EWeGameAchievement::TradeMaster);
    }
}

// ============================================================
//  防沉迷
// ============================================================

void UWeGameIntegration::ProcessAntiAddictionEvent(const FAntiAddictionEvent& Event)
{
    // 根据指令类型更新状态
    switch (Event.Action)
    {
        case EAntiAddictionAction::ShowTips:
            // 显示提示对话框
            UE_LOG(LogWeGame, Log, TEXT("AntiAddiction Tips: %s — %s (duration=%ds)"),
                *Event.Title, *Event.Content, Event.DisplayDurationSeconds);
            // 广播给 UI 显示弹窗
            OnAntiAddictionAction.Broadcast(Event);
            break;

        case EAntiAddictionAction::ForceExit:
            UE_LOG(LogWeGame, Warning, TEXT("AntiAddiction: FORCE EXIT requested"));
            // 保存进度后退出
            bPlaytimeAllowed = false;
            AntiAddictionState = EAntiAddictionState::MinorStrict;
            OnAntiAddictionStateChanged.Broadcast(AntiAddictionState);
            // 请求退出游戏
            if (GEngine && GEngine->GetCurrentPlayWorld())
            {
                if (AGameModeBase* GM = GEngine->GetCurrentPlayWorld()->GetAuthGameMode())
                {
                    // 触发游戏退出流程
                    GM->ReturnToMainMenuHost();
                }
            }
            break;

        case EAntiAddictionAction::TimeWarning:
            UE_LOG(LogWeGame, Log, TEXT("AntiAddiction: Time warning — %s"), *Event.Content);
            OnAntiAddictionAction.Broadcast(Event);
            break;

        case EAntiAddictionAction::CurfewActive:
            UE_LOG(LogWeGame, Log, TEXT("AntiAddiction: Curfew active"));
            bPlaytimeAllowed = false;
            OnAntiAddictionStateChanged.Broadcast(AntiAddictionState);
            break;

        default:
            break;
    }
}

// ============================================================
//  敏感词过滤
// ============================================================

FString UWeGameIntegration::FilterDirtyWords(const FString& InputText) const
{
#if WITH_WEGAME
    if (!bSDKInitialized) return InputText;

    rail::IRailUtils* Utils = rail::RailFactory()->RailUtils();
    if (!Utils) return InputText;

    rail::RailDirtyWordsCheckResult Result;
    FString InputAnsi = InputText; // Rail SDK 接受 UTF-8
    rail::RailResult Ret = Utils->DirtyWordsFilter(TCHAR_TO_ANSI(*InputAnsi), true, &Result);

    if (Ret == rail::kSuccess)
    {
        if (Result.dirty_type != rail::kRailDirtyWordsTypeNormalAllowWords)
        {
            // 返回过滤后的字符串
            return FString(ANSI_TO_TCHAR(Result.replace_string.c_str()));
        }
    }
    return InputText;
#else
    // Stub: 简单的关键词过滤
    FString Filtered = InputText;
    TArray<FString> BadWords = { TEXT("badword1"), TEXT("badword2") };
    for (const FString& Bad : BadWords)
    {
        if (Filtered.Contains(Bad, ESearchCase::IgnoreCase))
        {
            Filtered.ReplaceInline(*Bad, TEXT("***"), ESearchCase::IgnoreCase);
        }
    }
    return Filtered;
#endif
}

// ============================================================
//  内购
// ============================================================

void UWeGameIntegration::RequestPurchasableProducts()
{
#if WITH_WEGAME
    if (!bSDKInitialized) return;

    rail::IRailInGamePurchase* Purchase = rail::RailFactory()->RailInGamePurchase();
    if (!Purchase) return;

    Purchase->AsyncRequestAllPurchasableProducts("RequestProducts");
#else
    UE_LOG(LogWeGame, Log, TEXT("[STUB] RequestPurchasableProducts"));
#endif
}

void UWeGameIntegration::ShowPaymentWindow(const FString& OrderID)
{
#if WITH_WEGAME
    if (!bSDKInitialized) return;

    rail::IRailInGameStorePurchaseHelper* Store = rail::RailFactory()->RailInGameStorePurchaseHelper();
    if (!Store) return;

    Store->AsyncShowPaymentWindow(TCHAR_TO_ANSI(*OrderID), "ShowPayment");
#else
    UE_LOG(LogWeGame, Log, TEXT("[STUB] ShowPaymentWindow OrderID=%s"), *OrderID);
    // Stub: 模拟购买成功
    OnPurchaseResult.Broadcast(true, OrderID);
#endif
}

// ============================================================
//  Tick — 驱动 Rail SDK 事件循环
// ============================================================

void UWeGameIntegration::Tick(float DeltaTime)
{
    if (!bSDKInitialized) return;

    EventPollAccumulator += DeltaTime;
    if (EventPollAccumulator >= EventPollInterval)
    {
        EventPollAccumulator = 0.0f;

#if WITH_WEGAME
        // 必须周期性调用以驱动事件分发
        rail::RailFireEvents();
#endif
    }
}
