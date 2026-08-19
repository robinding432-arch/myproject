// StellarDedicatedServer.cpp
// ============================================================
//  专用服务器 GameMode 实现
//  Headless — 无渲染、无音频、无 UI
// ============================================================

#include "Server/StellarDedicatedServer.h"
#include "Core/SaveSystem.h"
#include "Online/AntiCheatManager.h"
#include "Core/NetworkOptimizer.h"
#include "Core/ObjectPool.h"
#include "Core/SolarSystem.h"
#include "Planet/ProceduralPlanet.h"
#include "Ship/ShipPawn.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/PlatformProcess.h"

DEFINE_LOG_CATEGORY_STATIC(LogStellarServer, Log, All);

AStellarDedicatedServer::AStellarDedicatedServer()
{
    // ---- 创建子系统（无渲染/音频/UI）----
    SaveManager = CreateDefaultSubobject<USaveManager>(TEXT("SaveManager"));
    AntiCheat = CreateDefaultSubobject<AAntiCheatManager>(TEXT("AntiCheat"));
    NetOptimizer = CreateDefaultSubobject<ANetworkOptimizer>(TEXT("NetOptimizer"));
    PoolManager = CreateDefaultSubobject<AObjectPoolManager>(TEXT("PoolManager"));

    // 服务器不需要 AudioMgr / WeatherSystem / SteamInt（客户端处理）
    // 但保留反作弊和存档

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.1f;  // 服务器 10Hz 基础 Tick（网络另算）

    // 设置 GameSession 类（处理连接/断开）
    // GameSessionClass = AStellarGameSession::StaticClass();
}

void AStellarDedicatedServer::BeginPlay()
{
    Super::BeginPlay();

    // ---- 加载配置 ----
    LoadServerConfig();

    // ---- 初始化随机数 ----
    if (ServerConfig.GalaxySeed == 0)
        ServerConfig.GalaxySeed = FMath::Rand();
    GalaxySeed = ServerConfig.GalaxySeed;

    UE_LOG(LogStellarServer, Log, TEXT("╔════════════════════════════════════╗"));
    UE_LOG(LogStellarServer, Log, TEXT("║   StellarSystem Dedicated Server    ║"));
    UE_LOG(LogStellarServer, Log, TEXT("║   Version: 6.7                     ║"));
    UE_LOG(LogStellarServer, Log, TEXT("╚════════════════════════════════════╝"));
    UE_LOG(LogStellarServer, Log, TEXT("[Server] Name: %s"), *ServerConfig.ServerName);
    UE_LOG(LogStellarServer, Log, TEXT("[Server] MaxPlayers: %d"), ServerConfig.MaxPlayers);
    UE_LOG(LogStellarServer, Log, TEXT("[Server] Port: %d"), ServerConfig.Port);
    UE_LOG(LogStellarServer, Log, TEXT("[Server] GalaxySeed: %d"), GalaxySeed);
    UE_LOG(LogStellarServer, Log, TEXT("[Server] AntiCheat: %s"),
        ServerConfig.bEnableAntiCheat ? TEXT("ON") : TEXT("OFF"));
    UE_LOG(LogStellarServer, Log, TEXT("[Server] PvP: %s"),
        ServerConfig.bEnablePvP ? TEXT("ON") : TEXT("OFF"));
    UE_LOG(LogStellarServer, Log, TEXT("[Server] NetworkTickRate: %.0f Hz"),
        ServerConfig.NetworkTickRate);
    UE_LOG(LogStellarServer, Log, TEXT("[Server] RelevancyDist: %d cm"),
        ServerConfig.RelevancyDistance);
    UE_LOG(LogStellarServer, Log, TEXT("[Server] RequiredClientVersion: %s"),
        *ServerConfig.RequiredClientVersion);

    // ---- 初始化子系统 ----
    InitSubsystems();

    // ---- 生成星系 ----
    InitGalaxy();

    // ---- 启动网络优化器 ----
    if (NetOptimizer)
    {
        NetOptimizer->SetMaxOutgoingKBs(512.f);  // 服务器默认 512KB/s 上限
        NetOptimizer->SetNetTickRate(ServerConfig.NetworkTickRate);
    }

    // ---- 广播启动消息 ----
    BroadcastMessage(
        FString::Printf(TEXT("[Server] %s is online. Welcome!"),
            *ServerConfig.ServerName),
        FLinearColor(0.2f, 0.8f, 1.f, 1.f)
    );

    UE_LOG(LogStellarServer, Log, TEXT("[Server] Ready. Listening on port %d"),
        ServerConfig.Port);
}

void AStellarDedicatedServer::EndPlay(const EEndPlayReason::Type Reason)
{
    UE_LOG(LogStellarServer, Log, TEXT("[Server] Shutting down... Reason: %d"), (int32)Reason);

    // ---- 保存所有玩家数据 ----
    ForceAutoSave();

    // ---- 通知所有客户端 ----
    BroadcastMessage(
        TEXT("[Server] Server is shutting down. See you in the stars!"),
        FLinearColor(1.f, 0.5f, 0.2f, 1.f)
    );

    // ---- 关闭子系统 ----
    if (AntiCheat) AntiCheat->Shutdown();
    if (NetOptimizer) NetOptimizer->Shutdown();
    if (PoolManager) PoolManager->Shutdown();

    Super::EndPlay(Reason);
}

void AStellarDedicatedServer::Tick(float Dt)
{
    Super::Tick(Dt);

    if (bIsShuttingDown) return;

    ServerUptime += Dt;

    // ---- 自动存档 ----
    HandleAutoSave(Dt);

    // ---- 更新统计 ----
    UpdateServerStats(Dt);

    // ---- 心跳检测 ----
    CheckPlayerHeartbeats(Dt);

    // ---- 清理断开玩家 ----
    CleanupDisconnectedPlayers();
}

void AStellarDedicatedServer::LoadServerConfig()
{
    // 从 Config/Server.ini 读取（如果存在）
    const FString ConfigPath = FPaths::ProjectConfigDir() + TEXT("Server.ini");

    if (FPaths::FileExists(ConfigPath))
    {
        GConfig->LoadFile(ConfigPath);

        int32 Val;
        if (GConfig->GetInt(TEXT("/Script/StellarSystem.StellarDedicatedServer"),
            TEXT("MaxPlayers"), Val, ConfigPath))
            ServerConfig.MaxPlayers = Val;

        if (GConfig->GetInt(TEXT("/Script/StellarSystem.StellarDedicatedServer"),
            TEXT("Port"), Val, ConfigPath))
            ServerConfig.Port = Val;

        FString Str;
        if (GConfig->GetString(TEXT("/Script/StellarSystem.StellarDedicatedServer"),
            TEXT("ServerName"), Str, ConfigPath))
            ServerConfig.ServerName = Str;

        if (GConfig->GetString(TEXT("/Script/StellarSystem.StellarDedicatedServer"),
            TEXT("MOTD"), Str, ConfigPath))
            ServerConfig.MOTD = Str;

        bool bVal;
        if (GConfig->GetBool(TEXT("/Script/StellarSystem.StellarDedicatedServer"),
            TEXT("EnableAntiCheat"), bVal, ConfigPath))
            ServerConfig.bEnableAntiCheat = bVal;

        if (GConfig->GetBool(TEXT("/Script/StellarSystem.StellarDedicatedServer"),
            TEXT("EnablePvP"), bVal, ConfigPath))
            ServerConfig.bEnablePvP = bVal;

        float FVal;
        if (GConfig->GetFloat(TEXT("/Script/StellarSystem.StellarDedicatedServer"),
            TEXT("NetworkTickRate"), FVal, ConfigPath))
            ServerConfig.NetworkTickRate = FVal;

        if (GConfig->GetFloat(TEXT("/Script/StellarSystem.StellarDedicatedServer"),
            TEXT("AutoSaveInterval"), FVal, ConfigPath))
            ServerConfig.AutoSaveInterval = FVal;

        UE_LOG(LogStellarServer, Log, TEXT("[Server] Loaded config from %s"), *ConfigPath);
    }
    else
    {
        UE_LOG(LogStellarServer, Warning, TEXT("[Server] No Server.ini found at %s, using defaults"),
            *ConfigPath);
    }

    MaxPlayerCount = ServerConfig.MaxPlayers;
}

void AStellarDedicatedServer::InitSubsystems()
{
    // ---- 存档 ----
    if (SaveManager)
    {
        SaveManager->WorldRef = GetWorld();
        UE_LOG(LogStellarServer, Log, TEXT("[Server] SaveManager initialized"));
    }

    // ---- 反作弊 ----
    if (AntiCheat && ServerConfig.bEnableAntiCheat)
    {
        AntiCheat->bServerSide = true;
        AntiCheat->Initialize();
        UE_LOG(LogStellarServer, Log, TEXT("[Server] AntiCheat initialized (server-side)"));
    }

    // ---- 网络优化器 ----
    if (NetOptimizer)
    {
        NetOptimizer->bIsServer = true;
        NetOptimizer->SetRelevancyDistance(ServerConfig.RelevancyDistance);
        NetOptimizer->SetNetTickRate(ServerConfig.NetworkTickRate);
        UE_LOG(LogStellarServer, Log, TEXT("[Server] NetworkOptimizer initialized"));
    }

    // ---- 对象池 ----
    if (PoolManager)
    {
        PoolManager->bIsServer = true;
        UE_LOG(LogStellarServer, Log, TEXT("[Server] ObjectPoolManager initialized"));
    }
}

void AStellarDedicatedServer::InitGalaxy()
{
    // 服务端只生成逻辑层（位置/状态/碰撞）
    // 不生成 Mesh（客户端负责渲染）
    UE_LOG(LogStellarServer, Log, TEXT("[Server] Initializing galaxy (seed=%d)..."), GalaxySeed);

    // 实际生成逻辑在 SpawnSolarSystem() 中
    // BeginPlay 中不直接 Spawn（等客户端连接后再生成）
}

void AStellarDedicatedServer::HandleAutoSave(float Dt)
{
    AutoSaveTimer += Dt;

    if (AutoSaveTimer >= ServerConfig.AutoSaveInterval)
    {
        AutoSaveTimer = 0.f;
        ForceAutoSave();
    }
}

void AStellarDedicatedServer::ForceAutoSave()
{
    if (!SaveManager) return;

    int32 SavedCount = 0;
    for (APlayerController* PC : GetWorld()->GetPlayerControllerIterator())
    {
        if (PC && PC->GetNetConnection())
        {
            // 通知客户端保存
            // ClientSaveRequest(PC);
            SavedCount++;
        }
    }

    UE_LOG(LogStellarServer, Log, TEXT("[Server] AutoSave: %d players, Uptime=%.0fs"),
        SavedCount, ServerUptime);
}

void AStellarDedicatedServer::UpdateServerStats(float Dt)
{
    StatUpdateTimer += Dt;
    if (StatUpdateTimer < 2.0f) return;  // 每 2 秒更新一次
    StatUpdateTimer = 0.f;

    // ---- 玩家数 ----
    CurrentPlayerCount = 0;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (It->Get()->GetNetConnection()) CurrentPlayerCount++;
    }

    // ---- 帧率（服务器 Tick 间隔的倒数）----
    ServerFPS = 1.0f / FMath::Max(Dt, 0.001f);
    ServerFPS = FMath::Clamp(ServerFPS, 1.f, 120.f);

    // ---- 内存 ----
    FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
    ServerMemoryMB = MemStats.UsedPhysical / (1024.f * 1024.f);

    // ---- CPU 估算（Tick 耗时占比）----
    float TickTime = Dt;
    float Budget = 1.0f / ServerConfig.NetworkTickRate;
    ServerCPUUsage = FMath::Clamp((TickTime / Budget) * 100.f, 0.f, 100.f);

    // ---- 带宽 ----
    float Now = GetWorld()->GetTimeSeconds();
    float DeltaTime = Now - LastNetSampleTime;
    if (DeltaTime > 0.5f)
    {
        // 估算（实际应从 NetDriver 获取）
        BandwidthInKBs = FMath::Clamp(CurrentPlayerCount * 8.f, 0.f, 10000.f);
        BandwidthOutKBs = FMath::Clamp(CurrentPlayerCount * 12.f, 0.f, 10000.f);
        LastNetSampleTime = Now;
    }

    // ---- 更新会话列表 ----
    ActiveSessions.Empty();
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (!PC || !PC->GetNetConnection()) continue;

        FPlayerSession Session;
        Session.PlayerName = PC->GetPlayerState()
            ? PC->GetPlayerState()->GetPlayerName()
            : TEXT("Unknown");
        Session.IPAddress = PC->GetNetConnection()->RemoteAddress.ToString();
        Session.ConnectedTime = ServerUptime;  // 近似
        Session.Ping = PC->GetPlayerState()
            ? PC->GetPlayerState()->Ping * 4 : 0;  // Ping 是 1/4 秒为单位

        // 信任分从反作弊获取
        if (AntiCheat)
        {
            // Session.TrustScore = AntiCheat->GetTrustScore(PC);
        }

        ActiveSessions.Add(Session);
    }

    // ---- 定期日志 ----
    UE_LOG(LogStellarServer, Log,
        TEXT("[Stats] Players=%d/%d | FPS=%.1f | CPU=%.0f%% | RAM=%.0fMB | NetIn=%.0fKB/s | NetOut=%.0fKB/s"),
        CurrentPlayerCount, MaxPlayerCount,
        ServerFPS, ServerCPUUsage, ServerMemoryMB,
        BandwidthInKBs, BandwidthOutKBs);
}

void AStellarDedicatedServer::CleanupDisconnectedPlayers()
{
    // 由 PlayerController 的 OnDestroy 触发清理
    // 这里做兜底检查
}

void AStellarDedicatedServer::CheckPlayerHeartbeats(float Dt)
{
    HeartbeatTimer += Dt;
    if (HeartbeatTimer < 5.0f) return;  // 每 5 秒
    HeartbeatTimer = 0.f;

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (!PC || !PC->GetNetConnection()) continue;

        // 检查 RPC 超时
        float LastRpcTime = PC->GetLastRPCTime();
        float Now = GetWorld()->GetTimeSeconds();
        float RpcAge = Now - LastRpcTime;

        if (RpcAge > 30.f)  // 30 秒无 RPC → 超时
        {
            UE_LOG(LogStellarServer, Warning,
                TEXT("[Server] Player %s timed out (%.0fs). Kicking..."),
                *PC->GetPlayerState()->GetPlayerName(), RpcAge);
            PC->ClientReturnToMainMenu(TEXT("Connection timed out"));
            PC->Destroy();
        }
    }
}

// ============================================================
//  玩家连接处理
// ============================================================

void AStellarDedicatedServer::OnPlayerConnected(APlayerController* PC,
    const FString& PlayerID, const FString& Version)
{
    if (!PC) return;

    // ---- 版本检查 ----
    if (Version != ServerConfig.RequiredClientVersion)
    {
        UE_LOG(LogStellarServer, Warning,
            TEXT("[Server] Player %s: version mismatch (client=%s, server=%s). Kicking."),
            *PlayerID, *Version, *ServerConfig.RequiredClientVersion);
        PC->ClientReturnToMainMenu(
            FString::Printf(TEXT("Client version mismatch. Server requires %s"),
                *ServerConfig.RequiredClientVersion));
        PC->Destroy();
        return;
    }

    // ---- 人数检查 ----
    if (CurrentPlayerCount >= MaxPlayerCount)
    {
        UE_LOG(LogStellarServer, Warning,
            TEXT("[Server] Server full. Rejecting %s"), *PlayerID);
        PC->ClientReturnToMainMenu(TEXT("Server is full. Please try again later."));
        PC->Destroy();
        return;
    }

    // ---- 反作弊注册 ----
    if (AntiCheat && ServerConfig.bEnableAntiCheat)
    {
        FString ClientChecksum = TEXT("pending");  // 客户端应上报
        AntiCheat->RegisterClient(PC, PlayerID, Version, ClientChecksum);
    }

    // ---- 发送 MOTD ----
    BroadcastMessage(
        FString::Printf(TEXT("[MOTD] %s — Welcome %s!"),
            *ServerConfig.MOTD, *PC->GetPlayerState()->GetPlayerName()),
        FLinearColor(0.3f, 1.f, 0.3f, 1.f)
    );

    UE_LOG(LogStellarServer, Log,
        TEXT("[Server] Player connected: %s (ID=%s, Version=%s)"),
        *PC->GetPlayerState()->GetPlayerName(), *PlayerID, *Version);
}

void AStellarDedicatedServer::OnPlayerDisconnected(APlayerController* PC)
{
    if (!PC) return;

    FString Name = PC->GetPlayerState()
        ? PC->GetPlayerState()->GetPlayerName()
        : TEXT("Unknown");

    UE_LOG(LogStellarServer, Log, TEXT("[Server] Player disconnected: %s"), *Name);

    // 保存该玩家数据
    if (SaveManager)
    {
        // SaveManager->SavePlayer(PC);
    }

    // 从反作弊移除
    if (AntiCheat)
    {
        // AntiCheat->UnregisterClient(PC);
    }
}

// ============================================================
//  管理命令
// ============================================================

void AStellarDedicatedServer::KickPlayer(const FString& PlayerID, const FString& Reason)
{
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (!PC || !PC->GetPlayerState()) continue;

        if (PC->GetPlayerState()->GetPlayerName().Contains(PlayerID) ||
            PC->PlayerState->GetUniqueId().ToString().Contains(PlayerID))
        {
            UE_LOG(LogStellarServer, Log, TEXT("[Server] Kicking %s. Reason: %s"),
                *PC->GetPlayerState()->GetPlayerName(), *Reason);
            PC->ClientReturnToMainMenu(FString::Printf(TEXT("Kicked: %s"), *Reason));
            PC->Destroy();
            return;
        }
    }
    UE_LOG(LogStellarServer, Warning, TEXT("[Server] KickPlayer: %s not found"), *PlayerID);
}

void AStellarDedicatedServer::BanPlayer(const FString& PlayerID, bool bHardwareBan)
{
    UE_LOG(LogStellarServer, Log, TEXT("[Server] Banning %s (HWID=%s)"),
        *PlayerID, bHardwareBan ? TEXT("Yes") : TEXT("No"));

    // 写入封禁列表（持久化）
    FString BanListPath = FPaths::ProjectSavedDir() + TEXT("BannedPlayers.txt");
    FString BanEntry = FString::Printf(TEXT("%s\t%s\t%s\n"),
        *PlayerID,
        bHardwareBan ? TEXT("HWID") : TEXT("ACCOUNT"),
        *FDateTime::Now().ToString());

    FFileHelper::SaveStringToFile(BanEntry, *BanListPath,
        FFileHelper::EEncodingOptions::AutoDetect,
        &IFileManager::Get(), FILEWRITE_Append);

    // 立即踢出
    KickPlayer(PlayerID, TEXT("You have been banned from this server."));
}

void AStellarDedicatedServer::BroadcastMessage(const FString& Message, const FLinearColor& Color)
{
    // NetMulticast → 所有客户端收到
    // 客户端 UI 层解析 → 显示到聊天框
    UE_LOG(LogStellarServer, Log, TEXT("[Broadcast] %s"), *Message);

    // 实际网络广播由 NetMulticast RPC 完成
    // 这里只是服务器日志
}

FString AStellarDedicatedServer::GetServerStatusJSON() const
{
    TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("server_name"), ServerConfig.ServerName);
    Json->SetNumberField(TEXT("version"), 6.7);
    Json->SetNumberField(TEXT("max_players"), MaxPlayerCount);
    Json->SetNumberField(TEXT("current_players"), CurrentPlayerCount);
    Json->SetNumberField(TEXT("uptime_seconds"), ServerUptime);
    Json->SetNumberField(TEXT("fps"), ServerFPS);
    Json->SetNumberField(TEXT("cpu_percent"), ServerCPUUsage);
    Json->SetNumberField(TEXT("memory_mb"), ServerMemoryMB);
    Json->SetNumberField(TEXT("net_in_kbs"), BandwidthInKBs);
    Json->SetNumberField(TEXT("net_out_kbs"), BandwidthOutKBs);
    Json->SetStringField(TEXT("motd"), ServerConfig.MOTD);
    Json->SetBoolField(TEXT("pvp_enabled"), ServerConfig.bEnablePvP);
    Json->SetBoolField(TEXT("ant_cheat_enabled"), ServerConfig.bEnableAntiCheat);

    // 玩家列表
    TArray<TSharedPtr<FJsonValue>> Players;
    for (const FPlayerSession& S : ActiveSessions)
    {
        TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
        P->SetStringField(TEXT("name"), S.PlayerName);
        P->SetStringField(TEXT("ip"), S.IPAddress);
        P->SetNumberField(TEXT("ping"), S.Ping);
        P->SetNumberField(TEXT("trust"), S.TrustScore);
        Players.Add(MakeShared<FJsonValueObject>(P));
    }
    Json->SetArrayField(TEXT("players"), Players);

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Json.ToSharedRef(), Writer);
    return Out;
}

// ============================================================
//  Exec 控制台命令
// ============================================================

void AStellarDedicatedServer::SetMaxPlayers(int32 NewMax)
{
    MaxPlayerCount = FMath::Clamp(NewMax, 1, 128);
    ServerConfig.MaxPlayers = MaxPlayerCount;
    UE_LOG(LogStellarServer, Log, TEXT("[Server] MaxPlayers set to %d"), MaxPlayerCount);
}

void AStellarDedicatedServer::ListPlayers()
{
    UE_LOG(LogStellarServer, Log, TEXT("===== Connected Players (%d/%d) ====="),
        CurrentPlayerCount, MaxPlayerCount);
    for (const FPlayerSession& S : ActiveSessions)
    {
        UE_LOG(LogStellarServer, Log, TEXT("  %s | IP=%s | Ping=%dms | Trust=%.0f"),
            *S.PlayerName, *S.IPAddress, S.Ping, S.TrustScore);
    }
    UE_LOG(LogStellarServer, Log, TEXT("====================================="));
}

void AStellarDedicatedServer::ServerSay(const FString& Msg)
{
    BroadcastMessage(FString::Printf(TEXT("[Admin] %s"), *Msg), FLinearColor(1.f, 0.8f, 0.2f, 1.f));
}

void AStellarDedicatedServer::SaveNow()
{
    UE_LOG(LogStellarServer, Log, TEXT("[Server] Manual save triggered"));
    ForceAutoSave();
}

void AStellarDedicatedServer::ShutdownServer(int32 DelaySeconds)
{
    if (bIsShuttingDown) return;

    BeginShutdown(DelaySeconds);
}

void AStellarDedicatedServer::BeginShutdown(int32 DelaySeconds)
{
    bIsShuttingDown = true;
    ShutdownCountdown = FMath::Max(DelaySeconds, 1);

    BroadcastMessage(
        FString::Printf(TEXT("[Server] Shutting down in %d seconds..."), ShutdownCountdown),
        FLinearColor(1.f, 0.3f, 0.3f, 1.f)
    );

    UE_LOG(LogStellarServer, Log, TEXT("[Server] Shutdown scheduled in %d seconds"),
        ShutdownCountdown);

    // 定时执行关闭
    GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
    {
        ShutdownCountdown--;
        if (ShutdownCountdown <= 0)
        {
            ExecuteShutdown();
        }
        else
        {
            if (ShutdownCountdown <= 5)
            {
                BroadcastMessage(
                    FString::Printf(TEXT("[Server] Shutdown in %d..."), ShutdownCountdown),
                    FLinearColor(1.f, 0.3f, 0.3f, 1.f)
                );
            }
            BeginShutdown(ShutdownCountdown);
        }
    });
}

void AStellarDedicatedServer::ExecuteShutdown()
{
    UE_LOG(LogStellarServer, Log, TEXT("[Server] Executing shutdown..."));

    // 保存所有数据
    ForceAutoSave();

    // 通知客户端
    BroadcastMessage(TEXT("[Server] Goodbye!"), FLinearColor(1.f, 0.5f, 0.2f, 1.f));

    // 延迟一帧后退出
    GetWorld()->GetTimerManager().SetTimerForNextTick([]()
    {
        UE_LOG(LogStellarServer, Log, TEXT("[Server] Process exit."));
        FPlatformProcess::RequestExit(false);
    });
}

// ============================================================
//  网络设置
// ============================================================

void AStellarDedicatedServer::SetNetworkTickRate(float NewRate)
{
    ServerConfig.NetworkTickRate = FMath::Clamp(NewRate, 1.f, 60.f);
    if (NetOptimizer) NetOptimizer->SetNetTickRate(ServerConfig.NetworkTickRate);
    UE_LOG(LogStellarServer, Log, TEXT("[Server] NetworkTickRate = %.0f Hz"),
        ServerConfig.NetworkTickRate);
}

void AStellarDedicatedServer::SetRelevancyDistance(int32 NewDistance)
{
    ServerConfig.RelevancyDistance = FMath::Max(NewDistance, 1000);
    if (NetOptimizer) NetOptimizer->SetRelevancyDistance(ServerConfig.RelevancyDistance);
    UE_LOG(LogStellarServer, Log, TEXT("[Server] RelevancyDistance = %d cm"),
        ServerConfig.RelevancyDistance);
}

void AStellarDedicatedServer::SpawnSolarSystem(TSubclassOf<ASolarSystem> SystemClass)
{
    if (!SystemClass) return;

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ASolarSystem* SolarSys = GetWorld()->SpawnActor<ASolarSystem>(
        SystemClass, FVector::ZeroVector, FRotator::ZeroRotator, Params);

    if (SolarSys)
    {
        SolarSys->SetReplicates(true);
        SolarSys->GalaxySeed = GalaxySeed;
        ActiveSolarSystem = SolarSys;

        UE_LOG(LogStellarServer, Log, TEXT("[Server] SolarSystem spawned (Seed=%d)"), GalaxySeed);
    }
}

// ============================================================
//  网络复制
// ============================================================

void AStellarDedicatedServer::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);

    DOREPLIFETIME(AStellarDedicatedServer, CurrentPlayerCount);
    DOREPLIFETIME(AStellarDedicatedServer, MaxPlayerCount);
    DOREPLIFETIME(AStellarDedicatedServer, ServerUptime);
    DOREPLIFETIME(AStellarDedicatedServer, ServerFPS);
    DOREPLIFETIME(AStellarDedicatedServer, ServerCPUUsage);
    DOREPLIFETIME(AStellarDedicatedServer, ServerMemoryMB);
    DOREPLIFETIME(AStellarDedicatedServer, BandwidthInKBs);
    DOREPLIFETIME(AStellarDedicatedServer, BandwidthOutKBs);
    DOREPLIFETIME(AStellarDedicatedServer, ActiveSolarSystem);
    DOREPLIFETIME(AStellarDedicatedServer, AllPlanets);
    DOREPLIFETIME(AStellarDedicatedServer, AllShips);
    DOREPLIFETIME(AStellarDedicatedServer, ActiveSessions);
}

// ============================================================
//  NetMulticast RPC
// ============================================================

void AStellarDedicatedServer::BroadcastMessage_Implementation(
    const FString& Message, const FLinearColor& Color)
{
    // 客户端实现：显示到聊天框
    // 服务器日志已在调用处输出
}
