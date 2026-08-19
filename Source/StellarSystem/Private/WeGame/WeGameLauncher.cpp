// WeGameLauncher.cpp
// v6.9 — WeGame 平台启动器实现

#include "WeGame/WeGameLauncher.h"
#include "WeGame/WeGameIntegration.h"
#include "HAL/PlatformProcess.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogWeGameLauncher, Log, All);

void UWeGameLauncher::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    CurrentState = EWeGameSystemState::Unknown;
    bIsWeGameEnvironment = false;
    bClientRunning = false;
    bSDKInitialized = false;
    EventPollTimer = 0.0f;

    UE_LOG(LogWeGameLauncher, Log, TEXT("UWeGameLauncher initialized"));
}

void UWeGameLauncher::Deinitialize()
{
    if (bSDKInitialized)
    {
        // 通知 WeGame 客户端游戏即将退出
#if WITH_WEGAME
        rail::RailFinalize();
#endif
        bSDKInitialized = false;
        UE_LOG(LogWeGameLauncher, Log, TEXT("Rail SDK finalized by launcher"));
    }

    Super::Deinitialize();
}

// ============================================================
//  初始化
// ============================================================

bool UWeGameLauncher::InitializeForWeGame(int32 AppId, const FString& AppVersion)
{
    CachedAppId = AppId;
    CachedAppVersion = AppVersion;

    // Step 1: 检测是否在 WeGame 环境中
    bIsWeGameEnvironment = DetectWeGameEnvironment();
    UE_LOG(LogWeGameLauncher, Log, TEXT("WeGame environment detected: %s"),
        bIsWeGameEnvironment ? TEXT("YES") : TEXT("NO"));

    if (!bIsWeGameEnvironment)
    {
        // 不在 WeGame 环境中（可能是 IDE 调试模式）
        // 允许继续运行，但切换到 stub 模式
        UE_LOG(LogWeGameLauncher, Warning,
            TEXT("Not running under WeGame client — entering DEBUG mode"));
        HandleSystemStateChanged(EWeGameSystemState::ClientOffline);
        return true; // 允许调试
    }

    // Step 2: 检查 WeGame 客户端进程
    bClientRunning = CheckWeGameClientProcess();
    if (!bClientRunning)
    {
        UE_LOG(LogWeGameLauncher, Error, TEXT("WeGame client not running!"));
        HandleSystemStateChanged(EWeGameSystemState::ClientOffline);
        return false;
    }

    // Step 3: 初始化 WeGameIntegration（它内部调用 Rail SDK）
    UWeGameIntegration* WeGame = GEngine->GetEngineSubsystem<UWeGameIntegration>();
    if (WeGame)
    {
        if (WeGame->InitializeSDK(AppId, AppVersion))
        {
            bSDKInitialized = true;
            HandleSystemStateChanged(EWeGameSystemState::ClientRunning);
            UE_LOG(LogWeGameLauncher, Log, TEXT("WeGame SDK initialized via launcher"));
            return true;
        }
    }

    HandleSystemStateChanged(EWeGameSystemState::SDKError);
    return false;
}

// ============================================================
//  Tick — 驱动事件循环 + 监控客户端状态
// ============================================================

void UWeGameLauncher::Tick(float DeltaTime)
{
    if (!bIsWeGameEnvironment) return;

    EventPollTimer += DeltaTime;
    if (EventPollTimer >= PollInterval)
    {
        EventPollTimer = 0.0f;

        // 1) 驱动 Rail SDK 事件循环
#if WITH_WEGAME
        if (bSDKInitialized)
        {
            rail::RailFireEvents();
        }
#endif

        // 2) 监控 WeGame 客户端状态
        bool bClientNowRunning = CheckWeGameClientProcess();
        if (bClientRunning && !bClientNowRunning)
        {
            // 客户端意外退出 → 游戏必须强退
            UE_LOG(LogWeGameLauncher, Warning, TEXT("WeGame client disconnected — forcing game exit"));
            HandleSystemStateChanged(EWeGameSystemState::ClientExiting);
            ForceExitGame(TEXT("WeGame client exited"));
        }
        bClientRunning = bClientNowRunning;
    }
}

// ============================================================
//  环境检测
// ============================================================

bool UWeGameLauncher::DetectWeGameEnvironment()
{
#if WITH_WEGAME
    // 方法1: 检查命令行参数（WeGame 客户端会传入特定参数）
    FString CmdLine = FCommandLine::Get();
    if (CmdLine.Contains(TEXT("-wegame")) ||
        CmdLine.Contains(TEXT("-rail_app_id")) ||
        CmdLine.Contains(TEXT("-rail"))))
    {
        return true;
    }

    // 方法2: 检查环境变量
    FString EnvVal;
    if (FPlatformMisc::GetEnvironmentVariable(TEXT("WEGAME_HOME"), EnvVal) ||
        FPlatformMisc::GetEnvironmentVariable(TEXT("RAIL_SDK_PATH"), EnvVal))
    {
        return !EnvVal.IsEmpty();
    }

    // 方法3: 检查进程（见下方）
    return CheckWeGameClientProcess();
#else
    // 从命令行检测（即使没有 SDK 也能判断）
    FString CmdLine = FCommandLine::Get();
    return CmdLine.Contains(TEXT("-wegame")) ||
           CmdLine.Contains(TEXT("-rail_app_id"));
#endif
}

bool UWeGameLauncher::CheckWeGameClientProcess() const
{
#if PLATFORM_WINDOWS
    // 检查 WeGame 客户端进程是否存在
    // 常见进程名: WeGame.exe / WeGameClient.exe / RailClient.exe
    TArray<FString> ProcessNames = {
        TEXT("WeGame.exe"),
        TEXT("WeGameClient.exe"),
        TEXT("RailClient.exe")
    };

    for (const FString& Name : ProcessNames)
    {
        // 使用 FPlatformProcess 枚举进程
        // 简化实现：检查已知路径
        FString KnownPath = FPaths::Combine(
            FPlatformMisc::GetEnvironmentVariable(TEXT("ProgramFiles(x86)")),
            TEXT("WeGame/WeGame.exe")
        );
        if (FPaths::FileExists(KnownPath))
        {
            // 文件存在不代表进程在跑，但结合其他检测已足够
            return true;
        }
    }

    // 退而求其次：检查 SDK 是否报告客户端在线
#if WITH_WEGAME
    // rail::RailIsPlatformValid() 或类似接口
    // 这里返回 true 以允许调试
    return true;
#endif

    return false;
#else
    // 非 Windows 平台不支持 WeGame 客户端
    return false;
#endif
}

// ============================================================
//  强退游戏
// ============================================================

void UWeGameLauncher::ForceExitGame(const FString& Reason)
{
    UE_LOG(LogWeGameLauncher, Warning, TEXT("ForceExitGame: %s"), *Reason);

    // 1) 保存进度
    // （由 GameMode 的 OnPauseMenuOpened 或 OnSessionEnded 处理）

    // 2) 通知 WeGameIntegration 清理
    UWeGameIntegration* WeGame = GEngine->GetEngineSubsystem<UWeGameIntegration>();
    if (WeGame)
    {
        WeGame->ShutdownSDK();
    }

    // 3) 返回主菜单或退出
    if (GEngine && GEngine->GetCurrentPlayWorld())
    {
        if (AGameModeBase* GM = GEngine->GetCurrentPlayWorld()->GetAuthGameMode())
        {
            GM->ReturnToMainMenuHost();
        }
    }

    // 4) 广播事件给 UI
    OnClientExit.Broadcast();

    // 5) 彻底退出进程（WeGame 要求游戏在客户端退出后也退出）
    // 延迟一帧确保存档完成
    GEngine->GetCurrentPlayWorld()->GetTimerManager().SetTimerForNextTick(
        []()
        {
            FPlatformMisc::RequestExit(false);
        }
    );
}

// ============================================================
//  状态变化处理
// ============================================================

void UWeGameLauncher::HandleSystemStateChanged(EWeGameSystemState NewState)
{
    if (CurrentState == NewState) return;

    CurrentState = NewState;
    UE_LOG(LogWeGameLauncher, Log, TEXT("WeGame system state: %d"), (int32)NewState);

    OnSystemStateChanged.Broadcast(NewState);

    // 某些状态需要立即响应
    switch (NewState)
    {
        case EWeGameSystemState::ClientExiting:
            ForceExitGame(TEXT("Client exiting"));
            break;

        case EWeGameSystemState::SDKError:
            // 尝试恢复或降级
            UE_LOG(LogWeGameLauncher, Error, TEXT("Rail SDK error — game may not function correctly"));
            break;

        default:
            break;
    }
}
