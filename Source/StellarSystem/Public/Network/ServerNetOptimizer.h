// ServerNetOptimizer.h
// StellarSystem v6.8 — 服务器端网络优化（多客户端管理）

#pragma once

#include "CoreMinimal.h"
#include "Network/NetworkTransportOptimizer.h"
#include "ServerNetOptimizer.generated.h"

/** 单个客户端连接信息 */
USTRUCT(BlueprintType)
struct FClientConnection
{
    GENERATED_BODY()

    UPROPERTY() FString          ClientIP;
    UPROPERTY() int32            ClientPort = 0;
    UPROPERTY() uint16           NextSendSeq = 0;
    UPROPERTY() uint16           NextRecvSeq = 0;
    UPROPERTY() float            LastActivity = 0.f;
    UPROPERTY() float            Ping        = 0.f;
    UPROPERTY() float            Jitter      = 0.f;
    UPROPERTY() float            LossRate   = 0.f;
    UPROPERTY() int32            BytesOut   = 0;
    UPROPERTY() int32            BytesIn    = 0;
    UPROPERTY() int32            PacketsOut = 0;
    UPROPERTY() int32            PacketsIn  = 0;
    UPROPERTY() int32            Retransmits = 0;
    UPROPERTY() EConnectionState State      = EConnectionState::Connecting;
    UPROPERTY() int32            PlayerId   = -1;
    UPROPERTY() float            BWLimitKBps = 512.f;
    UPROPERTY() float            CurrentBPS  = 0.f;  // 当前带宽使用
    UPROPERTY() FCongestionState Congestion;          // 每客户端拥塞状态

    // 可靠消息队列（每客户端独立）
    UPROPERTY() TArray<FNetMessage> ReliableQueue;

    // 最近发送的快照（用于冗余）
    UPROPERTY() TArray<FSnapshot> RecentSnapshots;

    // 延迟补偿缓冲区
    UPROPERTY() TArray<FSnapshot> LagCompBuffer;
};

/**
 * UServerNetOptimizer
 *
 * 服务器端多客户端网络优化：
 * - 每客户端独立序列号/拥塞控制
 * - 批量发送（合并多个客户端更新到单个 UDP 包）
 * - 自适应带宽分配（按玩家重要性/距离）
 * - 智能冗余（高丢包客户端多发冗余快照）
 * - 连接质量监控 + 自动降频
 */
UCLASS(BlueprintType, Config=Network)
class STELLARSYSTEM_API UServerNetOptimizer : public UObject
{
    GENERATED_BODY()

public:
    UServerNetOptimizer();

    // ═══════════════════════════════════════
    //  配置
    // ═══════════════════════════════════════

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="ServerNet")
    int32  MaxClients        = 64;

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="ServerNet")
    float  ServerTickRate    = 30.f;   // Hz

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="ServerNet")
    float  MaxBandwidthKBps = 8192.f; // 总出口带宽

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="ServerNet")
    int32  BatchSize         = 8;      // 每批最多合并 8 个客户端

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="ServerNet")
    float  ClientTimeout     = 30.f;   // 秒

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="ServerNet")
    bool   bEnableAdaptiveRedundancy = true;

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="ServerNet")
    bool   bEnableBatching   = true;

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="ServerNet")
    float  HighLossThreshold = 0.15f;  // >15% 丢包视为高丢包

    // ═══════════════════════════════════════
    //  生命周期
    // ═══════════════════════════════════════

    UFUNCTION(BlueprintCallable, Category="ServerNet")
    void Initialize();

    UFUNCTION(BlueprintCallable, Category="ServerNet")
    void Shutdown();

    UFUNCTION(BlueprintCallable, Category="ServerNet")
    void Tick(float DeltaTime);

    // ═══════════════════════════════════════
    //  连接管理
    // ═══════════════════════════════════════

    UFUNCTION(BlueprintCallable, Category="ServerNet|Connections")
    int32 AddClient(const FString& ClientIP, int32 ClientPort, int32 PlayerId);

    UFUNCTION(BlueprintCallable, Category="ServerNet|Connections")
    void RemoveClient(int32 ClientIndex);

    UFUNCTION(BlueprintCallable, Category="ServerNet|Connections")
    void UpdateClientActivity(int32 ClientIndex, const FNetMessage& Msg);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="ServerNet|Connections")
    int32 GetClientCount() const { return Clients.Num(); }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="ServerNet|Connections")
    FClientConnection GetClientInfo(int32 ClientIndex) const;

    // ═══════════════════════════════════════
    //  发送（服务器 → 客户端）
    // ═══════════════════════════════════════

    UFUNCTION(BlueprintCallable, Category="ServerNet|Send")
    bool SendToClient(int32 ClientIndex, ENetChannel Channel, ENetPriority Priority,
                      const TArray<uint8>& Payload, bool bReliable = true);

    UFUNCTION(BlueprintCallable, Category="ServerNet|Send")
    void BroadcastToAll(ENetChannel Channel, ENetPriority Priority,
                        const TArray<uint8>& Payload, bool bReliable = false);

    UFUNCTION(BlueprintCallable, Category="ServerNet|Send")
    void BroadcastToNearby(ENetChannel Channel, ENetPriority Priority,
                            const TArray<uint8>& Payload,
                            const FVector& SourceLocation, float Radius,
                            bool bReliable = false);

    UFUNCTION(BlueprintCallable, Category="ServerNet|Send")
    void SendWorldSnapshot(int32 ClientIndex, const TArray<FPredictedState>& WorldState);

    UFUNCTION(BlueprintCallable, Category="ServerNet|Send")
    void FlushAllBatches();

    // ═══════════════════════════════════════
    //  接收（客户端 → 服务器）
    // ═══════════════════════════════════════

    UFUNCTION(BlueprintCallable, Category="ServerNet|Receive")
    void ProcessIncomingFromClient(int32 ClientIndex, const TArray<uint8>& RawData, int32 DataSize);

    UFUNCTION(BlueprintCallable, Category="ServerNet|Receive")
    bool DequeueMessageForClient(int32 ClientIndex, FNetMessage& OutMessage);

    // ═══════════════════════════════════════
    //  带宽管理
    // ═══════════════════════════════════════

    UFUNCTION(BlueprintCallable, Category="ServerNet|Bandwidth")
    void AllocateBandwidth();

    UFUNCTION(BlueprintCallable, Category="ServerNet|Bandwidth")
    void SetClientPriority(int32 ClientIndex, float Priority);  // 0~1

    // ═══════════════════════════════════════
    //  统计
    // ═══════════════════════════════════════

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="ServerNet|Stats")
    float GetTotalBandwidthOut() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="ServerNet|Stats")
    float GetTotalBandwidthIn() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="ServerNet|Stats")
    float GetAveragePing() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="ServerNet|Stats")
    float GetWorstLossRate() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="ServerNet|Stats")
    int32 GetTotalPacketsOut() const;

    UFUNCTION(BlueprintCallable, Category="ServerNet|Stats")
    FString GetDiagnosticString() const;

    // ═══════════════════════════════════════
    //  委托
    // ═══════════════════════════════════════

    // Note: C++ multicast delegates (not BlueprintAssignable)
    // Bind via AddLambda() / AddUObject()
    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnClientConnected, int32, ClientIndex, int32, PlayerId);
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnClientDisconnected, int32, ClientIndex);
    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnClientMessage, int32, ClientIndex, FNetMessage /*copy*/, Message);
    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnClientQualityChanged, int32, ClientIndex, float, LossRate);

    FOnClientConnected OnClientConnected;
    FOnClientDisconnected OnClientDisconnected;
    FOnClientMessage OnClientMessage;
    FOnClientQualityChanged OnClientQualityChanged;

private:
    // 所有客户端连接
    UPROPERTY()
    TArray<FClientConnection> Clients;

    // 每客户端发送队列
    TMap<int32, TArray<FNetMessage> > ClientSendQueues;
    TMap<int32, TArray<FNetMessage> > ClientReliableQueues;
    TMap<int32, TQueue<FNetMessage> > ClientDeliverQueues;

    // 每客户端统计
    TMap<int32, FTransportStats> ClientStats;
    TMap<int32, float> ClientPriorities;

    // 批量缓冲
    TMap<int32, TArray<FNetMessage>> BatchBuffers;
    float LastBatchFlush = 0.f;

    // 全局统计
    float TotalBytesOut = 0;
    float TotalBytesIn  = 0;
    int32 TotalPacketsOut = 0;
    int32 TotalPacketsIn  = 0;

    // 内部方法
    void  SendRawToClient(int32 ClientIndex, const FNetMessage& Msg);
    void  ProcessAckFromClient(int32 ClientIndex, const FNetAckPacket& Ack);
    void  UpdateClientCongestion(int32 ClientIndex, float DeltaTime);
    void  CheckClientTimeouts(float CurrentTime);
    void  AdaptiveRedundancy(int32 ClientIndex);
    void  BuildBatchedPacket(int32 ClientIndex, TArray<uint8>& OutPacket);
    FNetMessage CreateAckForClient(int32 ClientIndex);
    void  LogServerNet(const FString& Msg, bool bError = false);
    int32 FindClientByAddress(const FString& IP, int32 Port) const;
};
