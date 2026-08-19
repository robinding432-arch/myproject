// StellarClientGameMode.cpp
// ============================================================
//  客户端 GameMode 实现
//  连接 Dedicated Server，管理本地表现
// ============================================================

#include "Client/StellarClientGameMode.h"
#include "Audio/AudioManager.h"
#include "Core/PerformanceManager.h"
#include "Core/ObjectPool.h"
#include "Online/AntiCheatManager.h"
#include "Core/SolarSystem.h"
#include "Planet/ProceduralPlanet.h"
#include "Ship/ShipPawn.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogStellarClient, Log, All);

AStellarClientGameMode::AStellarClientGameMode()
{
    // ---- 客户端只创建表现层子系统 ----
    AudioMgr = CreateDefaultSubobject<UAudioManager>(TEXT("AudioMgr"));
    PerfManager = CreateDefaultSubobject<APerformanceManager>(TEXT("PerfManager"));
    PoolManager = CreateDefaultSubobject<AObjectPoolManager>(TEXT("PoolManager"));
    LocalAntiCheat = CreateDefaultSubobject<AAntiCheatManager>(TEXT("LocalAntiCheat"));

    // 客户端不创建：
    // - SaveManager（服务器权威存档）
    // - NetworkOptimizer（服务器管理网络）
    // - SteamIntegration（由 OnlineSubsystem 处理）

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.f;  // 客户端全帧 Tick
}

void AStellarClientGameMode::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogStellarClient, Log, TEXT("╔════════════════════════════════════╗"));
    UE_LOG(LogStellarClient, Log, TEXT("║   StellarSystem Client v6.7          ║"));
    UE_LOG(LogStellarClient, Log, TEXT("╚════════════════════════════════════╝"));

    // ---- 初始化本地子系统 ----
    InitLocalSubsystems();

    // ---- 连接状态 ----
    ConnectionState = EClientConnectionState::Disconnected;
    ConnectionStatusMessage = TEXT("Ready to connect");

    UE_LOG(LogStellarClient, Log, TEXT("[Client] Local subsystems ready"));
    UE_LOG(LogStellarClient, Log, TEXT("[Client] Call ConnectToServer() to join a game"));
}

void AStellarClientGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
    UE_LOG(LogStellarClient, Log, TEXT("[Client] EndPlay. Reason: %d"), (int32)Reason);

    // ---- 断开服务器 ----
    if (ConnectionState == EClientConnectionState::Connected ||
        ConnectionState == EClientConnectionState::Connecting)
    {
        DisconnectFromServer(TEXT("Client shutting down"));
    }

    // ---- 清理本地资源 ----
    CleanupLocalResources();

    // ---- 关闭音频 ----
    if (AudioMgr) AudioMgr->Shutdown();

    Super::EndPlay(Reason);
}

void AStellarClientGameMode::Tick(float Dt)
{
    Super::Tick(Dt);

    // ---- 连接超时检测 ----
    CheckConnectionTimeout(Dt);

    // ---- 更新服务器状态（如果已连接）----
    if (ConnectionState == EClientConnectionState::Connected)
    {
        TimeSinceLastServerUpdate += Dt;

        // 超过 10 秒无服务器更新 → 认为断开
        if (TimeSinceLastServerUpdate > 10.f)
        {
            UE_LOG(LogStellarClient, Warning,
                TEXT("[Client] No server update for %.0fs. Reconnecting..."),
                TimeSinceLastServerUpdate);
            AttemptReconnect();
        }
    }
}

// ============================================================
//  连接管理
// ============================================================

void AStellarClientGameMode::ConnectToServer(const FString& IPAddress, int32 Port)
{
    if (ConnectionState == EClientConnectionState::Connecting)
    {
        UE_LOG(LogStellarClient, Warning, TEXT("[Client] Already connecting..."));
        return;
    }

    UE_LOG(LogStellarClient, Log, TEXT("[Client] Connecting to %s:%d..."),
        *IPAddress, Port);

    ConnectionState = EClientConnectionState::Connecting;
    ConnectionStatusMessage = FString::Printf(TEXT("Connecting to %s:%d..."),
        *IPAddress, Port);
    ReconnectAttempts = 0;
    TimeSinceLastServerUpdate = 0.f;

    // ---- 通过 OnlineSubsystem 连接 ----
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    if (OSS)
    {
        IOnlineSessionPtr SessionInt = OSS->GetSessionInterface();
        if (SessionInt.IsValid())
        {
            // 创建临时会话 → 连接
            FOnlineSessionSettings Settings;
            Settings.bIsLANMatch = false;
            Settings.bUsesPresence = false;
            Settings.NumPublicConnections = 1;

            FString TravelURL = FString::Printf(TEXT("%s:%d"), *IPAddress, Port);

            UE_LOG(LogStellarClient, Log, TEXT("[Client] Traveling to %s"), *TravelURL);
            GetWorld()->ServerTravel(TravelURL, false);

            ConnectionState = EClientConnectionState::Authenticating;
            ConnectionStatusMessage = TEXT("Authenticating...");
            return;
        }
    }

    // ---- 备选：直接控制台连接 ----
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (PC)
    {
        FString ConsoleCmd = FString::Printf(TEXT("open %s:%d"), *IPAddress, Port);
        PC->ConsoleCommand(ConsoleCmd, true);

        ConnectionState = EClientConnectionState::Authenticating;
        ConnectionStatusMessage = TEXT("Connecting (console fallback)...");
    }
    else
    {
        ConnectionState = EClientConnectionState::Disconnected;
        ConnectionStatusMessage = TEXT("Failed to connect: no player controller");
        UE_LOG(LogStellarClient, Error, TEXT("[Client] No PlayerController!"));
    }
}

void AStellarClientGameMode::DisconnectFromServer(const FString& Reason)
{
    UE_LOG(LogStellarClient, Log, TEXT("[Client] Disconnecting. Reason: %s"), *Reason);

    // ---- 通知服务器 ----
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (PC && PC->GetNetConnection())
    {
        // 服务器会触发 OnPlayerDisconnected
        PC->ClientReturnToMainMenu(Reason);
    }

    // ---- 重置状态 ----
    ConnectionState = EClientConnectionState::Disconnected;
    ConnectionStatusMessage = FString::Printf(TEXT("Disconnected: %s"), *Reason);

    // ---- 清理本地 ----
    LocalPlanets.Empty();
    LocalShips.Empty();
    LocalChatLog.Empty();

    // ---- 停止音频 ----
    if (AudioMgr) AudioMgr->StopAllSounds();
}

void AStellarClientGameMode::AttemptReconnect()
{
    if (!bAutoReconnect)
    {
        UE_LOG(LogStellarClient, Log, TEXT("[Client] Auto-reconnect disabled"));
        return;
    }

    ReconnectAttempts++;
    if (ReconnectAttempts > MaxReconnectAttempts)
    {
        UE_LOG(LogStellarClient, Warning,
            TEXT("[Client] Max reconnect attempts reached (%d)"), MaxReconnectAttempts);
        ConnectionState = EClientConnectionState::Disconnected;
        ConnectionStatusMessage = TEXT("Connection lost. Please reconnect manually.");
        return;
    }

    UE_LOG(LogStellarClient, Log, TEXT("[Client] Reconnect attempt %d/%d in %.0fs"),
        ReconnectAttempts, MaxReconnectAttempts, ReconnectDelay);

    ConnectionState = EClientConnectionState::Reconnecting;
    ConnectionStatusMessage = FString::Printf(TEXT("Reconnecting... (%d/%d)"),
        ReconnectAttempts, MaxReconnectAttempts);

    // 延迟后重连
    FTimerHandle TimerHandle;
    GetWorld()->GetTimerManager().SetTimer(TimerHandle,
        [this]()
        {
            // 需要知道上次连接的 IP:Port
            // 实际实现应从持久存储读取
            // ConnectToServer(LastIP, LastPort);
            UE_LOG(LogStellarClient, Warning,
                TEXT("[Client] Reconnect: need to store last server address"));
        },
        ReconnectDelay,
        false
    );
}

void AStellarClientGameMode::CheckConnectionTimeout(float Dt)
{
    if (ConnectionState != EClientConnectionState::Connecting &&
        ConnectionState != EClientConnectionState::Authenticating)
        return;

    TimeSinceLastServerUpdate += Dt;

    if (TimeSinceLastServerUpdate > ConnectionTimeout)
    {
        UE_LOG(LogStellarClient, Warning,
            TEXT("[Client] Connection timeout after %.0fs"), ConnectionTimeout);

        if (bAutoReconnect && ReconnectAttempts < MaxReconnectAttempts)
        {
            AttemptReconnect();
        }
        else
        {
            ConnectionState = EClientConnectionState::Disconnected;
            ConnectionStatusMessage = TEXT("Connection timed out");
        }
    }
}

// ============================================================
//  服务器列表
// ============================================================

TArray<FServerInfo> AStellarClientGameMode::GetServerList() const
{
    TArray<FServerInfo> Result;

    // ---- 从 Steam 服务器浏览器获取 ----
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    if (OSS)
    {
        // 实际实现需要注册 OnFindSessionsComplete 回调
        // 这里返回缓存的示例数据
    }

    // ---- 从本地配置读取收藏服务器 ----
    FString FavPath = FPaths::ProjectSavedDir() + TEXT("FavoriteServers.txt");
    if (FFileHelper::LoadFileToStringArray())
    {
        // 解析每行 "Name\tIP\tPort"
    }

    // 示例（实际应从网络查询）
    FServerInfo Example;
    Example.ServerName = TEXT("Official StellarSystem Server");
    Example.IPAddress = TEXT("127.0.0.1");
    Example.Port = 7777;
    Example.PlayerCount = 12;
    Example.MaxPlayers = 32;
    Example.Ping = 25.f;
    Example.GameVersion = TEXT("6.7.0");
    Example.bPvPEnabled = true;
    Example.MOTD = TEXT("Welcome to the official server!");
    Result.Add(Example);

    return Result;
}

// ============================================================
//  聊天/消息
// ============================================================

void AStellarClientGameMode::SendChatMessage(const FString& Message)
{
    if (Message.IsEmpty()) return;

    // 通过 RPC 发送到服务器 → 服务器广播
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (PC && PC->GetNetConnection())
    {
        // ServerSendChat(PC, Message);  // 需要在 PlayerController 实现
        LocalChatLog.Add(FString::Printf(TEXT("[You] %s"), *Message));

        if (LocalChatLog.Num() > MaxChatLogSize)
            LocalChatLog.RemoveAt(0);
    }
}

void AStellarClientGameMode::OnServerBroadcast_Implementation(
    const FString& Message, const FLinearColor& Color)
{
    // 显示到聊天框
    LocalChatLog.Add(Message);
    if (LocalChatLog.Num() > MaxChatLogSize)
        LocalChatLog.RemoveAt(0);

    UE_LOG(LogStellarClient, Log, TEXT("[Chat] %s"), *Message);

    // 更新最后收到服务器消息的时间
    TimeSinceLastServerUpdate = 0.f;
}

void AStellarClientGameMode::OnKickedFromServer_Implementation(const FString& Reason)
{
    UE_LOG(LogStellarClient, Warning, TEXT("[Client] Kicked from server: %s"), *Reason);
    ConnectionState = EClientConnectionState::Kicked;
    ConnectionStatusMessage = FString::Printf(TEXT("Kicked: %s"), *Reason);

    // 播放音效
    if (AudioMgr) AudioMgr->PlayUISound(TEXT("Kicked"));

    // 返回主菜单
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (PC) PC->ClientReturnToMainMenu(FString::Printf(TEXT("Kicked: %s"), *Reason));
}

void AStellarClientGameMode::OnBannedFromServer_Implementation(
    const FString& Reason, bool bHardwareBan)
{
    UE_LOG(LogStellarClient, Error, TEXT("[Client] BANNED from server: %s (HWID=%s)"),
        *Reason, bHardwareBan ? TEXT("Yes") : TEXT("No"));

    ConnectionState = EClientConnectionState::Banned;
    ConnectionStatusMessage = FString::Printf(TEXT("Banned: %s"), *Reason);

    // 播放音效
    if (AudioMgr) AudioMgr->PlayUISound(TEXT("Banned"));

    // 返回主菜单
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (PC) PC->ClientReturnToMainMenu(FString::Printf(TEXT("Banned: %s"), *Reason));
}

// ============================================================
//  本地子系统
// ============================================================

void AStellarClientGameMode::InitLocalSubsystems()
{
    // ---- 音频 ----
    if (AudioMgr)
    {
        AudioMgr->Init(this);
        AudioMgr->PreloadAllSounds();
        AudioMgr->PlaySpaceAmbient();
        UE_LOG(LogStellarClient, Log, TEXT("[Client] AudioManager initialized"));
    }

    // ---- 性能管理器 ----
    if (PerfManager)
    {
        // PerfManager 的 BeginPlay 会自动检测硬件
        UE_LOG(LogStellarClient, Log, TEXT("[Client] PerformanceManager initialized"));
    }

    // ---- 对象池 ----
    if (PoolManager)
    {
        // 预注册常用池化类（子弹/粒子/伤害数字）
        UE_LOG(LogStellarClient, Log, TEXT("[Client] ObjectPoolManager initialized"));
    }

    // ---- 本地反作弊（轻量）----
    if (LocalAntiCheat)
    {
        LocalAntiCheat->bServerSide = false;
        UE_LOG(LogStellarClient, Log, TEXT("[Client] LocalAntiCheat initialized (client-side)"));
    }
}

void AStellarClientGameMode::CleanupLocalResources()
{
    // ---- 清空对象池 ----
    if (PoolManager) PoolManager->Shutdown();

    // ---- 清理本地行星引用 ----
    for (AProceduralPlanet* Planet : LocalPlanets)
    {
        if (Planet && Planet->GetWorld()) Planet->Destroy();
    }
    LocalPlanets.Empty();

    // ---- 清理本地飞船引用 ----
    for (AShipPawn* Ship : LocalShips)
    {
        if (Ship && Ship->GetWorld()) Ship->Destroy();
    }
    LocalShips.Empty();
}

// ============================================================
//  本地诊断
// ============================================================

FString AStellarClientGameMode::RunLocalDiagnostic() const
{
    FString Report;
    Report += TEXT("===== StellarSystem Client Diagnostic =====\n");

    // 连接状态
    Report += FString::Printf(TEXT("  Connection: %d (%s)\n"),
        (int32)ConnectionState, *ConnectionStatusMessage);
    Report += FString::Printf(TEXT("  ReconnectAttempts: %.0f\n"), ReconnectAttempts);
    Report += FString::Printf(TEXT("  TimeSinceServerUpdate: %.1fs\n"),
        TimeSinceLastServerUpdate);

    // 性能
    if (PerfManager)
    {
        // Report += PerfManager->GetDiagnosticString();
    }

    // 音频
    if (AudioMgr)
    {
        Report += TEXT("  Audio: Running\n");
    }

    // 聊天日志
    Report += FString::Printf(TEXT("  ChatLog: %d messages\n"), LocalChatLog.Num());

    // 本地对象
    Report += FString::Printf(TEXT("  LocalPlanets: %d\n"), LocalPlanets.Num());
    Report += FString::Printf(TEXT("  LocalShips: %d\n"), LocalShips.Num());

    Report += TEXT("===== End Diagnostic =====\n");
    return Report;
}

void AStellarClientGameMode::SetManualQuality(int32 Tier)
{
    if (!PerfManager) return;

    Tier = FMath::Clamp(Tier, 0, 3);
    // PerfManager->SetQualityTier((EQualityTier)Tier);
    UE_LOG(LogStellarClient, Log, TEXT("[Client] Manual quality set to %d"), Tier);
}

// ============================================================
//  网络复制
// ============================================================

void AStellarClientGameMode::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);

    DOREPLIFETIME(AStellarClientGameMode, ServerName);
    DOREPLIFETIME(AStellarClientGameMode, ServerMaxPlayers);
    DOREPLIFETIME(AStellarClientGameMode, ServerCurrentPlayers);
    DOREPLIFETIME(AStellarClientGameMode, ServerUptime);
    DOREPLIFETIME(AStellarClientGameMode, ServerMOTD);
}
