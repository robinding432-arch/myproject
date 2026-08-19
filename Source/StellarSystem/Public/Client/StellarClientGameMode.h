// StellarClientGameMode.h
// ============================================================
//  客户端专用 GameMode — 连接 Dedicated Server 时使用
//
//  职责：
//    - 处理本地玩家输入
//    - 管理本地表现（渲染/音频/UI/粒子）
//    - 程序化 Mesh 生成（行星/飞船/建筑）
//    - 与服务器同步状态
//    - 本地缓存（对象池/性能管理）
//
//  不包含：
//    - 服务器权威逻辑（反作弊/存档/经济计算）
//    - 服务端网络优化
//    - 会话管理
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "StellarClientGameMode.generated.h"

class UAudioManager;
class APerformanceManager;
class AObjectPoolManager;
class ASolarSystem;
class AProceduralPlanet;
class AShipPawn;
class AAntiCheatManager;  // 客户端轻量检测

// 连接状态
UENUM(BlueprintType)
enum class EClientConnectionState : uint8
{
    Disconnected,
    Connecting,
    Authenticating,
    Connected,
    Reconnecting,
    Kicked,
    Banned,
};

// 服务器信息（从 Steam 服务器浏览器获取）
USTRUCT(BlueprintType)
struct FServerInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString ServerName;

    UPROPERTY(BlueprintReadOnly)
    FString IPAddress;

    UPROPERTY(BlueprintReadOnly)
    int32 Port = 7777;

    UPROPERTY(BlueprintReadOnly)
    int32 PlayerCount = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 MaxPlayers = 32;

    UPROPERTY(BlueprintReadOnly)
    float Ping = 0.f;

    UPROPERTY(BlueprintReadOnly)
    FString GameVersion;

    UPROPERTY(BlueprintReadOnly)
    bool bHasPassword = false;

    UPROPERTY(BlueprintReadOnly)
    bool bPvPEnabled = true;

    UPROPERTY(BlueprintReadOnly)
    FString MOTD;
};

UCLASS(BlueprintType)
class AStellarClientGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AStellarClientGameMode();

    virtual void Tick(float Dt) override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    // ---- 连接状态 ----
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    EClientConnectionState ConnectionState = EClientConnectionState::Disconnected;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FString ConnectionStatusMessage = TEXT("Not connected");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float ReconnectAttempts = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float TimeSinceLastServerUpdate = 0.f;

    // ---- 本地子系统（表现层）----
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UAudioManager* AudioMgr = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    APerformanceManager* PerfManager = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    AObjectPoolManager* PoolManager = nullptr;

    // 客户端轻量反作弊（只做本地检测，不仲裁）
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    AAntiCheatManager* LocalAntiCheat = nullptr;

    // ---- 服务器信息 ----
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated)
    FString ServerName = TEXT("Unknown");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated)
    int32 ServerMaxPlayers = 32;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated)
    int32 ServerCurrentPlayers = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated)
    float ServerUptime = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated)
    FString ServerMOTD;

    // ---- 本地星系引用（服务器 Replicate 过来）----
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    ASolarSystem* ActiveSolarSystem = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TArray<AProceduralPlanet*> LocalPlanets;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TArray<AShipPawn*> LocalShips;

    // ============================================================
    //  API
    // ============================================================

    // 连接服务器
    UFUNCTION(BlueprintCallable)
    void ConnectToServer(const FString& IPAddress, int32 Port = 7777);

    // 断开连接
    UFUNCTION(BlueprintCallable)
    void DisconnectFromServer(const FString& Reason = TEXT("User disconnected"));

    // 尝试重连
    UFUNCTION(BlueprintCallable)
    void AttemptReconnect();

    // 获取服务器列表（从 Steam 服务器浏览器）
    UFUNCTION(BlueprintCallable, BlueprintPure)
    TArray<FServerInfo> GetServerList() const;

    // 发送聊天消息到服务器
    UFUNCTION(BlueprintCallable)
    void SendChatMessage(const FString& Message);

    // 接收服务器广播（由服务器 NetMulticast 调用）
    UFUNCTION(BlueprintCallable, NetMulticast, Reliable)
    void OnServerBroadcast(const FString& Message, const FLinearColor& Color);

    // 接收踢出通知
    UFUNCTION(BlueprintCallable, Client, Reliable)
    void OnKickedFromServer(const FString& Reason);

    // 接收封禁通知
    UFUNCTION(BlueprintCallable, Client, Reliable)
    void OnBannedFromServer(const FString& Reason, bool bHardwareBan);

    // 本地性能诊断
    UFUNCTION(BlueprintCallable, BlueprintPure)
    FString RunLocalDiagnostic() const;

    // 设置质量等级（覆盖性能管理器自动检测）
    UFUNCTION(BlueprintCallable)
    void SetManualQuality(int32 Tier);  // 0=Low, 1=Medium, 2=High, 3=Ultra

    UFUNCTION(BlueprintCallable, BlueprintPure)
    bool IsConnected() const
    {
        return ConnectionState == EClientConnectionState::Connected;
    }

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

protected:
    // 连接超时检测
    void CheckConnectionTimeout(float Dt);

    // 初始化本地子系统
    void InitLocalSubsystems();

    // 清理本地资源
    void CleanupLocalResources();

    // 内部状态
    float ConnectionTimeout = 15.f;  // 15 秒连接超时
    float ReconnectDelay = 3.f;      // 重连间隔
    int32 MaxReconnectAttempts = 5;
    bool bAutoReconnect = true;

    // 本地消息缓存（聊天记录）
    UPROPERTY()
    TArray<FString> LocalChatLog;
    int32 MaxChatLogSize = 100;
};
