// StellarGameMode.cpp
#include "Core/StellarGameMode.h"
#include "Core/SaveSystem.h"
#include "Audio/AudioManager.h"
#include "Steam/SteamIntegration.h"
#include "Core/SpaceWeather.h"
#include "Core/SolarSystem.h"
#include "Planet/ProceduralPlanet.h"
#include "Ship/ShipPawn.h"
#include "Online/AntiCheatManager.h"
#include "UI/PauseMenu.h"
#include "Core/PerformanceManager.h"    // v6.6
#include "Core/ObjectPool.h"            // v6.6
#include "Core/NetworkOptimizer.h"      // v6.6
#include "Core/StartupOptimizer.h"      // v6.6
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"

AStellarGameMode::AStellarGameMode()
{
    SaveManager = CreateDefaultSubobject<USaveManager>(TEXT("SaveManager"));
    AudioMgr = CreateDefaultSubobject<UAudioManager>(TEXT("AudioMgr"));
    SteamInt = CreateDefaultSubobject<USteamIntegration>(TEXT("SteamInt"));
    WeatherSystem = CreateDefaultSubobject<ASpaceWeather>(TEXT("WeatherSystem"));
    AntiCheat = CreateDefaultSubobject<AAntiCheatManager>(TEXT("AntiCheat"));

    // v6.6：性能优化子系统
    PerfManager = CreateDefaultSubobject<APerformanceManager>(TEXT("PerfManager"));
    PoolManager = CreateDefaultSubobject<AObjectPoolManager>(TEXT("PoolManager"));
    NetOptimizer = CreateDefaultSubobject<ANetworkOptimizer>(TEXT("NetOptimizer"));
    StartupOpt = CreateDefaultSubobject<AStartupOptimizer>(TEXT("StartupOpt"));

    PrimaryActorTick.bCanEverTick = true;
}

void AStellarGameMode::BeginPlay()
{
    Super::BeginPlay();

    if (GalaxySeed == 0) GalaxySeed = FMath::Rand();

    // v6.5：检测是否为多人游戏
    ENetMode NetMode = GetNetMode();
    bIsMultiplayerGame = (NetMode == NM_DedicatedServer || NetMode == NM_ListenServer);
    if (bIsMultiplayerGame)
    {
        UE_LOG(LogTemp, Log, TEXT("[GameMode] Multiplayer detected - pause menu set to LocalOnly"));
    }

    // 初始化子系统
    InitSubsystems();

    // 默认不自动生成，由关卡蓝图或 SpawnSolarSystem 触发
}

void AStellarGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
    // 关闭 Steam
    if (SteamInt) SteamInt->Shutdown();
    Super::EndPlay(Reason);
}

void AStellarGameMode::InitSubsystems()
{
    // 音频
    if (AudioMgr)
    {
        AudioMgr->Init(this);
        AudioMgr->PreloadAllSounds();
        AudioMgr->PlaySpaceAmbient();
    }

    // Steam
    if (SteamInt)
    {
        SteamInt->Initialize();
    }

    // 天气
    if (WeatherSystem && !WeatherSystem->GetWorld())
    {
        // 已经在关卡中则跳过
    }

    // 存档
    if (SaveManager) SaveManager->WorldRef = GetWorld();

    // v6.5：反作弊
    if (AntiCheat && bIsMultiplayerGame)
    {
        UE_LOG(LogTemp, Log, TEXT("[GameMode] Anti-cheat enabled (multiplayer)"));
    }
    else if (AntiCheat)
    {
        UE_LOG(LogTemp, Log, TEXT("[GameMode] Anti-cheat enabled (single-player/offline)"));
    }

    // v6.6：性能优化子系统
    if (PerfManager)
    {
        UE_LOG(LogTemp, Log, TEXT("[GameMode] PerformanceManager ready"));
        // PerfManager 的 BeginPlay 会自动检测硬件+应用质量
    }

    if (PoolManager)
    {
        UE_LOG(LogTemp, Log, TEXT("[GameMode] ObjectPoolManager ready"));
        // 预注册常用池化类
        // （具体类在 BeginPlay 中由 PoolRegistrations 配置）
    }

    if (NetOptimizer)
    {
        UE_LOG(LogTemp, Log, TEXT("[GameMode] NetworkOptimizer ready"));
        // 根据游戏模式设置带宽
        if (bIsMultiplayerGame)
        {
            NetOptimizer->SetBandwidthLimit(512.f, 256.f);  // 512KB/s out, 256KB/s in
        }
    }

    if (StartupOpt)
    {
        UE_LOG(LogTemp, Log, TEXT("[GameMode] StartupOptimizer ready"));
        // 启用崩溃恢复
        StartupOpt->EnableCrashRecovery(true);
    }
}

void AStellarGameMode::Tick(float Dt)
{
    Super::Tick(Dt);
    if (SaveManager) SaveManager->TickAutoSave(Dt);
}

void AStellarGameMode::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(AStellarGameMode, GalaxySeed);
    DOREPLIFETIME(AStellarGameMode, AllPlanets);
    DOREPLIFETIME(AStellarGameMode, AllShips);
    DOREPLIFETIME(AStellarGameMode, PlayerShip);
    DOREPLIFETIME(AStellarGameMode, ActiveSolarSystem);
}

void AStellarGameMode::InitializeGalaxy()
{
    AllPlanets.Empty();
    AllShips.Empty();
}

void AStellarGameMode::RegisterPlanet(AProceduralPlanet* Planet)
{
    if (Planet && !AllPlanets.Contains(Planet))
    {
        AllPlanets.Add(Planet);
        OnPlanetRegistered.Broadcast(Planet);
    }
}

void AStellarGameMode::UnregisterPlanet(AProceduralPlanet* Planet)
{
    AllPlanets.Remove(Planet);
}

void AStellarGameMode::RegisterShip(AShipPawn* Ship)
{
    if (Ship && !AllShips.Contains(Ship))
    {
        AllShips.Add(Ship);
        OnShipRegistered.Broadcast(Ship);
        if (!PlayerShip) PlayerShip = Ship;
    }
}

void AStellarGameMode::UnregisterShip(AShipPawn* Ship)
{
    AllShips.Remove(Ship);
    if (PlayerShip == Ship) PlayerShip = nullptr;
}

AProceduralPlanet* AStellarGameMode::FindNearestPlanetTo(const FVector& Location) const
{
    AProceduralPlanet* Nearest = nullptr;
    float BestDist = TNumericLimits<float>::Max();
    for (AProceduralPlanet* P : AllPlanets)
    {
        if (!P) continue;
        float D = FVector::DistSquared(Location, P->GetActorLocation());
        if (D < BestDist) { BestDist = D; Nearest = P; }
    }
    return Nearest;
}

TArray<AProceduralPlanet*> AStellarGameMode::GetPlanetsInRange(const FVector& Location, float Range) const
{
    TArray<AProceduralPlanet*> Out;
    float R2 = Range * Range;
    for (AProceduralPlanet* P : AllPlanets)
        if (P && FVector::DistSquared(Location, P->GetActorLocation()) <= R2)
            Out.Add(P);
    return Out;
}

AShipPawn* AStellarGameMode::FindNearestShipTo(const FVector& Location, AShipPawn* Ignore) const
{
    AShipPawn* Nearest = nullptr;
    float BestDist = TNumericLimits<float>::Max();
    for (AShipPawn* S : AllShips)
    {
        if (S == Ignore || !S) continue;
        float D = FVector::DistSquared(Location, S->GetActorLocation());
        if (D < BestDist) { BestDist = D; Nearest = S; }
    }
    return Nearest;
}

bool AStellarGameMode::SaveCurrentGame(int32 Slot)
{
    return SaveManager ? SaveManager->SaveGame(Slot) : false;
}

bool AStellarGameMode::LoadGameSlot(int32 Slot)
{
    return SaveManager ? SaveManager->LoadGame(Slot) : false;
}

void AStellarGameMode::SetGameRule(FName RuleName, float Value)
{
    GameRules.Add(RuleName, Value);
}

float AStellarGameMode::GetGameRule(FName RuleName) const
{
    if (const float* V = GameRules.Find(RuleName)) return *V;
    return 0.f;
}

void AStellarGameMode::SpawnSolarSystem(TSubclassOf<ASolarSystem> SystemClass)
{
    if (!SystemClass || !GetWorld()) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    FVector SpawnLoc = FVector::ZeroVector;
    ASolarSystem* Solar = GetWorld()->SpawnActor<ASolarSystem>(
        SystemClass, SpawnLoc, FRotator::ZeroRotator, SpawnParams);

    if (Solar)
    {
        Solar->GenerateSolarSystem();
        ActiveSolarSystem = Solar;
        OnSolarSystemReady.Broadcast(Solar);
    }
}

void AStellarGameMode::SetActiveSolarSystem(ASolarSystem* SolarSys)
{
    ActiveSolarSystem = SolarSys;
}

// =====================================================================
// v6.5：反作弊客户端注册
// =====================================================================

void AStellarGameMode::RegisterAntiCheatClient(APlayerController* PC,
    const FString& PlayerID, const FString& ClientVersion, const FString& ClientChecksum)
{
    if (!AntiCheat || !PC) return;

    UE_LOG(LogTemp, Log, TEXT("[GameMode] Registering player for anti-cheat: %s"), *PlayerID);

    // 服务器端调用 RegisterPlayer
    AntiCheat->Server_RegisterPlayer(PC, PlayerID, ClientVersion, ClientChecksum);
}

// =====================================================================
// v6.5：暂停菜单通知
// =====================================================================

void AStellarGameMode::NotifyPauseMenuOpened(UPauseMenu* PauseMenu)
{
    ActivePauseMenu = PauseMenu;

    if (!PauseMenu) return;

    // 根据当前游戏模式设置暂停策略
    if (bIsMultiplayerGame)
    {
        // 多人游戏：强制 LocalOnly 模式
        PauseMenu->SetPauseMode(EPauseMenuMode::LocalOnly);
        UE_LOG(LogTemp, Log, TEXT("[GameMode] PauseMenu set to LocalOnly (multiplayer)"));
    }
    else
    {
        // 单人游戏：允许完全暂停
        PauseMenu->SetPauseMode(EPauseMenuMode::FullPause);
        UE_LOG(LogTemp, Log, TEXT("[GameMode] PauseMenu set to FullPause (single-player)"));
    }
}

void AStellarGameMode::OnLocalPauseStateChanged(bool bPaused)
{
    bLocalPauseActive = bPaused;

    // 在多人游戏中，暂停菜单只影响本地玩家
    // 其他玩家的模拟继续运行
    if (bIsMultiplayerGame)
    {
        // 可以在这里暂停本地计时器、音频等
        // 但不碰 World->bIsPaused
        UE_LOG(LogTemp, Log, TEXT("[GameMode] Local pause state: %s"),
            bPaused ? TEXT("PAUSED") : TEXT("RESUMED"));
    }
}

// =====================================================================
// v6.6 性能诊断
// =====================================================================

FString AStellarGameMode::RunPerformanceDiagnostic() const
{
    FString Report;
    Report += TEXT("===== StellarSystem Performance Diagnostic =====\n");

    // 性能管理器
    if (PerfManager)
    {
        FPerformanceStats Stats = PerfManager->GetCurrentStats();
        FAdaptiveQualitySettings Q = PerfManager->GetQualitySettings();
        EPerformanceTier Tier = PerfManager->GetPerformanceTier();

        Report += FString::Printf(TEXT("\n[Performance Manager]\n"));
        Report += FString::Printf(TEXT("  Tier: %d\n"), (int32)Tier);
        Report += FString::Printf(TEXT("  Avg FPS: %.1f\n"), Stats.AverageFPS);
        Report += FString::Printf(TEXT("  Frame Time: %.1f ms\n"), Stats.FrameTimeMs);
        Report += FString::Printf(TEXT("  Game Thread: %.1f ms\n"), Stats.GameThreadMs);
        Report += FString::Printf(TEXT("  Render Thread: %.1f ms\n"), Stats.RenderThreadMs);
        Report += FString::Printf(TEXT("  GPU: %.1f ms\n"), Stats.GPUMs);
        Report += FString::Printf(TEXT("  Memory: %.0f MB\n"), Stats.MemoryMB);
        Report += FString::Printf(TEXT("  Draw Calls: %d\n"), Stats.DrawCalls);
        Report += FString::Printf(TEXT("  Triangles: %d\n"), Stats.Triangles);
        Report += FString::Printf(TEXT("  Active Actors: %d\n"), Stats.ActiveActors);
        Report += FString::Printf(TEXT("  Quality: ViewDist=%d Shadow=%d Tex=%d Foliage=%d%%\n"),
            Q.ViewDistance, Q.ShadowQuality, Q.TextureQuality, Q.FoliageDensity);
    }
    else
    {
        Report += TEXT("\n[Performance Manager] NOT INITIALIZED\n");
    }

    // 对象池
    if (PoolManager)
    {
        TArray<FPoolStats> PoolStats = PoolManager->GetAllPoolStats();
        Report += FString::Printf(TEXT("\n[Object Pools] %d pools\n"), PoolStats.Num());
        for (const FPoolStats& PS : PoolStats)
        {
            Report += FString::Printf(TEXT("  %s: Active=%d Inactive=%d Created=%d HitRate=%.1f%% Peak=%d\n"),
                *PS.PoolName.ToString(), PS.ActiveCount, PS.InactiveCount,
                PS.TotalCreated, PS.HitRate * 100.f, PS.PeakActive);
        }
    }

    // 网络优化器
    if (NetOptimizer)
    {
        FBandwidthStats BS = NetOptimizer->GetBandwidthStats();
        Report += TEXT("\n[Network Optimizer]\n");
        Report += FString::Printf(TEXT("  Out: %.1f KB/s\n"), BS.OutgoingKBps);
        Report += FString::Printf(TEXT("  In: %.1f KB/s\n"), BS.IncomingKBps);
        Report += FString::Printf(TEXT("  Replicated Actors: %d\n"), BS.ReplicatedActors);
        Report += FString::Printf(TEXT("  Skipped Updates: %d\n"), BS.SkippedUpdates);
        Report += FString::Printf(TEXT("  Compression: %.1f%%\n"), BS.CompressionRatio * 100.f);
        Report += FString::Printf(TEXT("  Packet Loss: %.1f%%\n"), BS.PacketLossRate * 100.f);
    }

    // 启动优化器
    if (StartupOpt)
    {
        FStartupTiming ST = StartupOpt->GetStartupTiming();
        FStabilityMetrics SM = StartupOpt->GetStabilityMetrics();

        Report += TEXT("\n[Startup]\n");
        Report += FString::Printf(TEXT("  Total: %.0f ms\n"), ST.TotalStartupMs);
        Report += FString::Printf(TEXT("  Engine: %.0f ms\n"), ST.EngineInitMs);
        Report += FString::Printf(TEXT("  Modules: %.0f ms\n"), ST.ModuleLoadMs);
        Report += FString::Printf(TEXT("  Assets: %.0f ms\n"), ST.AssetPreloadMs);
        Report += FString::Printf(TEXT("  World: %.0f ms\n"), ST.WorldBuildMs);
        Report += FString::Printf(TEXT("  Subsystems: %.0f ms\n"), ST.SubsystemInitMs);
        Report += FString::Printf(TEXT("  First Frame: %.0f ms\n"), ST.FirstFrameMs);
        Report += FString::Printf(TEXT("  Preloaded: %d\n"), ST.PreloadedAssets);
        Report += FString::Printf(TEXT("  Deferred: %d\n"), ST.DeferredAssets);

        Report += TEXT("\n[Stability]\n");
        Report += FString::Printf(TEXT("  Uptime: %.0f s\n"), SM.UptimeSeconds);
        Report += FString::Printf(TEXT("  Crashes: %d\n"), SM.CrashCount);
        Report += FString::Printf(TEXT("  Warnings: %d\n"), SM.WarningCount);
        Report += FString::Printf(TEXT("  OOM Events: %d\n"), SM.OutOfMemoryCount);
        Report += FString::Printf(TEXT("  Min FPS: %.1f\n"), SM.MinFPS);
        Report += FString::Printf(TEXT("  Max Frame: %.0f ms\n"), SM.MaxFrameTimeMs);
        Report += FString::Printf(TEXT("  Avg FPS: %.1f\n"), SM.AverageFPS);
        Report += FString::Printf(TEXT("  Peak Memory: %.0f MB\n"), SM.MemoryPeakMB);
    }

    Report += TEXT("\n===== End Diagnostic =====");
    return Report;
}
