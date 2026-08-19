// TransportBridge.h
// StellarSystem v6.8 — 桥接层：将 UNetworkTransportOptimizer 接入 UE 网络

#pragma once

#include "CoreMinimal.h"
#include "Network/NetworkTransportOptimizer.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "TransportBridge.generated.h"

class FSocket;
class FRunnable;
class FRunnableThread;

UENUM(BlueprintType)
enum class ETransportLogLevel : uint8
{
    None,
    ErrorsOnly,
    Warnings,
    Info,
    Verbose,
    MAX
};

/**
 * UTransportBridge
 *
 * 负责：
 * 1. 创建/管理 Socket（UDP）
 * 2. 收发线程（独立线程，不阻塞 GameThread）
 * 3. 将接收到的数据喂给 UNetworkTransportOptimizer
 * 4. 将优化器要发送的数据通过 Socket 发出
 * 5. 在 GameThread Tick 中驱动优化器
 */
UCLASS(BlueprintType, Config=Network)
class STELLARSYSTEM_API UTransportBridge : public UObject
{
    GENERATED_BODY()

public:
    UTransportBridge();

    // ═══════════════════════════════════════
    //  配置
    // ═══════════════════════════════════════

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Bridge|Connection")
    FString RemoteIP = TEXT("127.0.0.1");

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Bridge|Connection")
    int32  RemotePort = 7777;

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Bridge|Connection")
    int32  LocalPort = 0;  // 0 = 自动分配

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Bridge|Performance")
    int32  SocketSendBufferSize = 262144;   // 256KB

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Bridge|Performance")
    int32  SocketRecvBufferSize = 1048576;  // 1MB

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Bridge|Performance")
    int32  MaxPacketSize = 1200;

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Bridge|Performance")
    bool   bUseThreadedSend = true;

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Bridge|Performance")
    bool   bUseThreadedRecv = true;

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Bridge|Logging")
    ETransportLogLevel LogLevel = ETransportLogLevel::Warnings;

    // ═══════════════════════════════════════
    //  生命周期
    // ═══════════════════════════════════════

    UFUNCTION(BlueprintCallable, Category="Bridge")
    bool Start(bool bIsServer, const FString& InRemoteIP = TEXT(""), int32 InRemotePort = 7777);

    UFUNCTION(BlueprintCallable, Category="Bridge")
    void Stop();

    UFUNCTION(BlueprintCallable, Category="Bridge")
    void Tick(float DeltaTime);

    // ═══════════════════════════════════════
    //  发送（GameThread 调用）
    // ═══════════════════════════════════════

    UFUNCTION(BlueprintCallable, Category="Bridge|Send")
    bool Send(ENetChannel Channel, ENetPriority Priority,
              const TArray<uint8>& Payload, bool bReliable = true);

    UFUNCTION(BlueprintCallable, Category="Bridge|Send")
    bool SendStruct(ENetChannel Channel, ENetPriority Priority,
                    const FString& StructName, const TArray<uint8>& Data);

    UFUNCTION(BlueprintCallable, Category="Bridge|Send")
    uint16 SendInput(const FPredictedState& InputState);

    UFUNCTION(BlueprintCallable, Category="Bridge|Send")
    void Flush();

    // ═══════════════════════════════════════
    //  接收（GameThread 调用）
    // ═══════════════════════════════════════

    UFUNCTION(BlueprintCallable, Category="Bridge|Receive")
    bool Receive(FNetMessage& OutMessage);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="Bridge|Receive")
    int32 GetPendingMessageCount() const;

    // ═══════════════════════════════════════
    //  快照
    // ═══════════════════════════════════════

    UFUNCTION(BlueprintCallable, Category="Bridge|Snapshot")
    void SendWorldSnapshot(const TArray<FPredictedState>& WorldState);

    UFUNCTION(BlueprintCallable, Category="Bridge|Snapshot")
    void ApplyReceivedSnapshot(float RenderTime);

    // ═══════════════════════════════════════
    //  统计
    // ═══════════════════════════════════════

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="Bridge|Stats")
    FTransportStats GetStats() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="Bridge|Stats")
    FString GetDiagnosticString() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="Bridge|Stats")
    bool IsConnected() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="Bridge|Stats")
    float GetCurrentLossRate() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="Bridge|Stats")
    float GetCurrentRTT() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="Bridge|Stats")
    float GetCompressionRatio() const;

    // ═══════════════════════════════════════
    //  事件
    // ═══════════════════════════════════════

    // Note: These are C++ multicast delegates (not BlueprintAssignable)
    // Use AddLambda() / AddUObject() to bind
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnBridgeMessage, FNetMessage /*copy*/, Message);
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnBridgeStateChanged, uint8 /*EConnectionState*/, NewState);
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnBridgeRollback, int32, RollbackFrames);

    FOnBridgeMessage OnMessage;
    FOnBridgeStateChanged OnStateChanged;
    FOnBridgeRollback OnRollback;

    // ═══════════════════════════════════════
    //  内部
    // ═══════════════════════════════════════

private:
    // 传输优化器（核心）
    UPROPERTY()
    UNetworkTransportOptimizer* Transport;

    // Socket
    FSocket* Socket;
    FIPv4Endpoint RemoteEndpoint;
    FIPv4Endpoint LocalEndpoint;

    // 线程
    FRunnable*   SendRunnable;
    FRunnableThread* SendThread;
    FRunnable*   RecvRunnable;
    FRunnableThread* RecvThread;

    // 线程安全队列
    TQueue<TArray<uint8>> SendQueue;       // GameThread → SendThread
    TQueue<TArray<uint8>> RecvQueue;       // RecvThread → GameThread
    FCriticalSection        SendCS;
    FCriticalSection        RecvCS;

    // 状态
    bool  bIsRunning;
    bool  bIsServer;
    float LastTickTime;

    // 内部方法
    bool  CreateSocket();
    void  CloseSocket();
    void  ProcessGameThreadSend(float DeltaTime);
    void  DrainRecvQueueToTransport();
    void  DrainTransportSendToQueue();
    void  LogBridge(const FString& Msg, bool bError = false);

    // 线程入口
    void  RecvThreadMain();
    void  SendThreadMain();

    // 快照缓冲
    TArray<FSnapshot> PendingSnapshots;
};
