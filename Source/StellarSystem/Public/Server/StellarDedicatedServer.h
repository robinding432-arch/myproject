// StellarDedicatedServer.h
// ============================================================
//  专用服务器 GameMode — Headless 运行，无渲染/音频/UI
//
//  职责：
//    - 管理所有连接的客户端
//    - 运行游戏逻辑（星系/物理/战斗/PvP）
//    - 反作弊监控
//    - 网络优化（相关性剔除/带宽整形）
//    - 自动存档（定时 + 玩家退出时）
//    - 服务器状态广播（MOTD/玩家数/版本）
//
//  不包含：
//    - 任何 UMG/Slate UI
//    - 任何音频
//    - 任何程序化 Mesh 生成（客户端负责表现）
//    - 任何 Niagara/粒子
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "StellarDedicatedServer.generated.h"

class USaveManager;
class AAntiCheatManager;
class ANetworkOptimizer;
class AObjectPoolManager;
class ASolarSystem;
class AProceduralPlanet;
class AShipPawn;

// 服务器配置（从 INI 读取）
USTRUCT(BlueprintType)
struct FServerConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxPlayers = 32;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ServerName = TEXT("StellarSystem Dedicated Server");

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString MOTD = TEXT("Welcome to StellarSystem!");

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Port = 7777;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableAntiCheat = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnablePvP = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AutoSaveInterval = 300.f;  // 5 分钟

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 GalaxySeed = 0;             // 0 = 随机

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float NetworkTickRate = 30.f;     // Hz

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 RelevancyDistance = 50000;  // cm

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bAllowMods = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString RequiredClientVersion = TEXT("6.7.0");
};

// 玩家会话信息
USTRUCT(BlueprintType)
struct FPlayerSession
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString PlayerID;

    UPROPERTY(BlueprintReadOnly)
    FString PlayerName;

    UPROPERTY(BlueprintReadOnly)
    FString IPAddress;

    UPROPERTY(BlueprintReadOnly)
    float ConnectedTime = 0.f;

    UPROPERTY(BlueprintReadOnly)
    int32 Ping = 0;

    UPROPERTY(BlueprintReadOnly)
    float TrustScore = 100.f;

    UPROPERTY(BlueprintReadOnly)
    bool bIsBanned = false;
};

UCLASS(BlueprintType)
class AStellarDedicatedServer : public AGameModeBase
{
    GENERATED_BODY()

public:
    AStellarDedicatedServer();

    virtual void Tick(float Dt) override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    // ---- 服务器配置 ----
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Server")
    FServerConfig ServerConfig;

    // ---- 运行时状态 ----
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated)
    int32 CurrentPlayerCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated)
    int32 MaxPlayerCount = 32;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated)
    float ServerUptime = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated)
    float ServerFPS = 60.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated)
    float ServerCPUUsage = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated)
    float ServerMemoryMB = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated)
    float BandwidthInKBs = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated)
    float BandwidthOutKBs = 0.f;

    // ---- 子系统引用 ----
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    USaveManager* SaveManager = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    AAntiCheatManager* AntiCheat = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    ANetworkOptimizer* NetOptimizer = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    AObjectPoolManager* PoolManager = nullptr;

    // ---- 星系 ----
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated)
    ASolarSystem* ActiveSolarSystem = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated)
    TArray<AProceduralPlanet*> AllPlanets;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated)
    TArray<AShipPawn*> AllShips;

    // ---- 玩家会话 ----
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated)
    TArray<FPlayerSession> ActiveSessions;

    // ============================================================
    //  API
    // ============================================================

    // 玩家连接/断开
    UFUNCTION(BlueprintCallable)
    void OnPlayerConnected(APlayerController* PC, const FString& PlayerID, const FString& Version);

    UFUNCTION(BlueprintCallable)
    void OnPlayerDisconnected(APlayerController* PC);

    // 踢出玩家
    UFUNCTION(BlueprintCallable)
    void KickPlayer(const FString& PlayerID, const FString& Reason);

    // 封禁玩家
    UFUNCTION(BlueprintCallable)
    void BanPlayer(const FString& PlayerID, bool bHardwareBan = false);

    // 广播消息给所有客户端
    UFUNCTION(BlueprintCallable, NetMulticast, Reliable)
    void BroadcastMessage(const FString& Message, const FLinearColor& Color);

    // 获取服务器状态（JSON 格式，供 Web 面板查询）
    UFUNCTION(BlueprintCallable, BlueprintPure)
    FString GetServerStatusJSON() const;

    // 控制台命令
    UFUNCTION(Exec)
    void SetMaxPlayers(int32 NewMax);

    UFUNCTION(Exec)
    void ListPlayers();

    UFUNCTION(Exec)
    void ServerSay(const FString& Msg);

    UFUNCTION(Exec)
    void SaveNow();

    UFUNCTION(Exec)
    void ShutdownServer(int32 DelaySeconds = 10);

    // 网络优化
    UFUNCTION(BlueprintCallable)
    void SetNetworkTickRate(float NewRate);

    UFUNCTION(BlueprintCallable)
    void SetRelevancyDistance(int32 NewDistance);

    // 星系管理
    UFUNCTION(BlueprintCallable)
    void SpawnSolarSystem(TSubclassOf<ASolarSystem> SystemClass);

    // 自动存档
    UFUNCTION(BlueprintCallable)
    void ForceAutoSave();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

protected:
    // 初始化
    void LoadServerConfig();
    void InitSubsystems();
    void InitGalaxy();

    // 定时任务
    void HandleAutoSave(float Dt);
    void UpdateServerStats(float Dt);
    void CleanupDisconnectedPlayers();

    // 心跳检测
    void CheckPlayerHeartbeats(float Dt);

    // 关闭流程
    void BeginShutdown(int32 DelaySeconds);
    void ExecuteShutdown();

    // 内部状态
    float AutoSaveTimer = 0.f;
    float StatUpdateTimer = 0.f;
    float HeartbeatTimer = 0.f;
    bool bIsShuttingDown = false;
    int32 ShutdownCountdown = 0;

    // 网络统计
    int64 LastNetBytesIn = 0;
    int64 LastNetBytesOut = 0;
    float LastNetSampleTime = 0.f;
};
