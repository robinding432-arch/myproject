// TransportBridge.cpp
// StellarSystem v6.8 — 桥接层实现

#include "Network/TransportBridge.h"
#include "Network/NetworkTransportOptimizer.h"
#include "HAL/RunnableThread.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Sockets/Public/SocketSubsystem.h"
#include "Sockets/Public/IPv4Address.h"
#include "Sockets/Public/IPv4Endpoint.h"
#include "Sockets/Public/Common/UdpSocketBuilder.h"
#include "Interfaces/IPv4/IPv4Interface.h"

// ═══════════════════════════════════════
//  构造 / 生命周期
// ═══════════════════════════════════════

UTransportBridge::UTransportBridge()
    : Socket(nullptr)
    , SendRunnable(nullptr)
    , SendThread(nullptr)
    , RecvRunnable(nullptr)
    , RecvThread(nullptr)
    , bIsRunning(false)
    , bIsServer(false)
    , LastTickTime(0.f)
{
}

bool UTransportBridge::Start(bool bInIsServer, const FString& InRemoteIP, int32 InRemotePort)
{
    bIsServer = bInIsServer;

    if (!InRemoteIP.IsEmpty()) RemoteIP = InRemoteIP;
    if (InRemotePort > 0) RemotePort = InRemotePort;

    // 创建传输优化器
    Transport = NewObject<UNetworkTransportOptimizer>();
    if (!Transport)
    {
        LogBridge(TEXT("Failed to create NetworkTransportOptimizer"), true);
        return false;
    }
    Transport->Initialize(bIsServer, RemoteIP);

    // 绑定事件
    Transport->OnMessageReceived.AddLambda([this](const FNetMessage& Msg) {
        // 收到消息，存入接收队列
        TArray<uint8> Raw;
        Raw.SetNumUninitialized(sizeof(FNetMessageHeader) + Msg.Payload.Num());
        memcpy(Raw.GetData(), &Msg.Header, sizeof(FNetMessageHeader));
        memcpy(Raw.GetData() + sizeof(FNetMessageHeader), Msg.Payload.GetData(), Msg.Payload.Num());

        FScopeLock Lock(&RecvCS);
        RecvQueue.Enqueue(Raw);
    });

    Transport->OnConnectionStateChanged.AddLambda([this](EConnectionState NewState) {
        OnStateChanged.Broadcast(NewState);
    });

    Transport->OnRollbackOccurred.AddLambda([this](int32 Frames) {
        OnRollback.Broadcast(Frames);
    });

    // 创建 Socket
    if (!CreateSocket())
    {
        return false;
    }

    // 启动线程
    bIsRunning = true;

    if (bUseThreadedRecv)
    {
        RecvRunnable = new FTransportRecvRunnable(this);
        RecvThread = FRunnableThread::Create(RecvRunnable, TEXT("TransportRecv"), 0, TPri_Normal);
    }

    if (bUseThreadedSend)
    {
        SendRunnable = new FTransportSendRunnable(this);
        SendThread = FRunnableThread::Create(SendRunnable, TEXT("TransportSend"), 0, TPri_Normal);
    }

    LastTickTime = FPlatformTime::Seconds();
    LogBridge(FString::Printf(TEXT("Bridge started (Server=%s, Remote=%s:%d)"),
              bIsServer ? TEXT("Yes") : TEXT("No"), *RemoteIP, RemotePort));
    return true;
}

void UTransportBridge::Stop()
{
    bIsRunning = false;

    // 等待线程结束
    if (RecvThread)
    {
        RecvThread->Kill(true);
        delete RecvThread;
        RecvThread = nullptr;
    }
    if (SendThread)
    {
        SendThread->Kill(true);
        delete SendThread;
        SendThread = nullptr;
    }

    // 关闭 Socket
    CloseSocket();

    // 关闭传输优化器
    if (Transport)
    {
        Transport->Shutdown();
    }

    // 清空队列
    {
        FScopeLock Lock(&SendCS);
        TArray<uint8> Dummy;
        while (SendQueue.Dequeue(Dummy)) {}
    }
    {
        FScopeLock Lock(&RecvCS);
        TArray<uint8> Dummy;
        while (RecvQueue.Dequeue(Dummy)) {}
    }

    LogBridge(TEXT("Bridge stopped"));
}

void UTransportBridge::Tick(float DeltaTime)
{
    if (!bIsRunning || !Transport) return;

    // 1. 驱动传输优化器
    Transport->Tick(DeltaTime);

    // 2. 从 RecvQueue 取出数据 → 喂给优化器
    DrainRecvQueueToTransport();

    // 3. 从优化器发送队列 → SendQueue → Socket
    DrainTransportSendToQueue();

    // 4. 如果不使用独立发送线程，在主线程发送
    if (!bUseThreadedSend)
    {
        ProcessGameThreadSend(DeltaTime);
    }

    // 5. 检查连接超时
    float Now = FPlatformTime::Seconds();
    // (简化：由优化器内部心跳处理)

    LastTickTime = Now;
}

// ═══════════════════════════════════════
//  Socket 管理
// ═══════════════════════════════════════

bool UTransportBridge::CreateSocket()
{
    FIPv4Address LocalAddr(127, 0, 0, 1);
    FIPv4Address RemoteAddr;
    if (!FIPv4Address::Parse(RemoteIP, RemoteAddr))
    {
        LogBridge(FString::Printf(TEXT("Invalid remote IP: %s"), *RemoteIP), true);
        return false;
    }

    RemoteEndpoint = FIPv4Endpoint(RemoteAddr, (uint16)RemotePort);

    // 创建 UDP Socket
    Socket = FUdpSocketBuilder(TEXT("StellarTransport"))
        .AsReusable()
        .WithBroadcast()
        .WithReceiveBufferSize(SocketRecvBufferSize)
        .WithSendBufferSize(SocketSendBufferSize)
        .BoundToAddress(LocalAddr)
        .BoundToPort((uint16)(LocalPort > 0 ? LocalPort : 0))
        .Build();

    if (!Socket)
    {
        LogBridge(TEXT("Failed to create UDP socket"), true);
        return false;
    }

    // 设置非阻塞
    Socket->SetNonBlocking(true);
    Socket->SetNoDelay(true);

    // 获取实际绑定的本地端口
    FIPv4Endpoint BoundEndpoint;
    if (Socket->GetLocalAddress(BoundEndpoint))
    {
        LocalEndpoint = BoundEndpoint;
        LogBridge(FString::Printf(TEXT("Socket bound to %s"), *BoundEndpoint.ToString()));
    }

    return true;
}

void UTransportBridge::CloseSocket()
{
    if (Socket)
    {
        Socket->Close();
        ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM, false);
        if (SocketSub)
        {
            SocketSub->DestroySocket(Socket);
        }
        Socket = nullptr;
    }
}

// ═══════════════════════════════════════
//  发送（GameThread）
// ═══════════════════════════════════════

bool UTransportBridge::Send(ENetChannel Channel, ENetPriority Priority,
                              const TArray<uint8>& Payload, bool bReliable)
{
    if (!Transport || !bIsRunning) return false;
    return Transport->SendMessage(Channel, Priority, Payload, bReliable);
}

bool UTransportBridge::SendStruct(ENetChannel Channel, ENetPriority Priority,
                                    const FString& StructName, const TArray<uint8>& Data)
{
    if (!Transport || !bIsRunning) return false;
    return Transport->SendStruct(Channel, Priority, StructName, Data);
}

uint16 UTransportBridge::SendInput(const FPredictedState& InputState)
{
    if (!Transport || !bIsRunning) return 0;
    return Transport->SendInputCommand(InputState);
}

void UTransportBridge::Flush()
{
    if (!Transport) return;
    Transport->FlushBatch();
}

// ═══════════════════════════════════════
//  接收（GameThread）
// ═══════════════════════════════════════

bool UTransportBridge::Receive(FNetMessage& OutMessage)
{
    if (!Transport) return false;
    return Transport->DequeueMessage(OutMessage);
}

int32 UTransportBridge::GetPendingMessageCount() const
{
    if (!Transport) return 0;
    return Transport->GetSendQueueLength();
}

// ═══════════════════════════════════════
//  快照
// ═══════════════════════════════════════

void UTransportBridge::SendWorldSnapshot(const TArray<FPredictedState>& WorldState)
{
    if (!Transport) return;
    Transport->SendSnapshot(WorldState);
}

void UTransportBridge::ApplyReceivedSnapshot(float RenderTime)
{
    if (!Transport) return;
    // 从传输器获取最新快照并应用
    // (简化：由 Transport 内部处理)
}

// ═══════════════════════════════════════
//  统计
// ═══════════════════════════════════════

FTransportStats UTransportBridge::GetStats() const
{
    if (Transport) return Transport->GetStats();
    return FTransportStats();
}

FString UTransportBridge::GetDiagnosticString() const
{
    if (Transport) return Transport->GetDiagnosticString();
    return TEXT("Transport not initialized");
}

bool UTransportBridge::IsConnected() const
{
    return bIsRunning && Socket != nullptr;
}

float UTransportBridge::GetCurrentLossRate() const
{
    if (Transport) return Transport->GetCurrentLossRate();
    return 0.f;
}

float UTransportBridge::GetCurrentRTT() const
{
    if (Transport) return Transport->GetCurrentRTT();
    return 0.f;
}

float UTransportBridge::GetCompressionRatio() const
{
    if (Transport) return Transport->GetCompressionRatio();
    return 1.f;
}

// ═══════════════════════════════════════
//  内部：队列管理
// ═══════════════════════════════════════

void UTransportBridge::DrainRecvQueueToTransport()
{
    // 从 RecvQueue 取出原始数据 → 喂给 Transport
    TArray<uint8> RawData;
    {
        FScopeLock Lock(&RecvCS);
        if (!RecvQueue.Dequeue(RawData)) return;
    }

    Transport->ProcessIncoming(RawData, RawData.Num());
}

void UTransportBridge::DrainTransportSendToQueue()
{
    // 从 Transport 的发送队列取出 → 放入 Socket 发送队列
    // (实际由 SendThread 或 ProcessGameThreadSend 处理)
}

void UTransportBridge::ProcessGameThreadSend(float DeltaTime)
{
    if (!Socket || !Transport) return;

    // 从 Transport 获取待发送消息并写到 Socket
    // 简化：Transport 内部维护 SendQueue，这里通过事件驱动
    // 实际实现中，Transport 应在 SendMessage 时通过委托通知 Bridge
}

// ═══════════════════════════════════════
//  日志
// ═══════════════════════════════════════

void UTransportBridge::LogBridge(const FString& Msg, bool bError)
{
    FString Prefix = bError ? TEXT("[TransportBridge][ERROR] ")
                           : TEXT("[TransportBridge] ");
    if (bError || LogLevel >= ETransportLogLevel::Info)
    {
        if (bError)
            UE_LOG(LogTemp, Error, TEXT("%s%s"), *Prefix, *Msg);
        else
            UE_LOG(LogTemp, Log, TEXT("%s%s"), *Prefix, *Msg);
    }
}

// ═══════════════════════════════════════
//  可运行线程类（发送/接收）
// ═══════════════════════════════════════

/** 接收线程 */
class FTransportRecvRunnable : public FRunnable
{
public:
    FTransportRecvRunnable(UTransportBridge* InBridge) : Bridge(InBridge), bStop(false) {}

    virtual bool Init() override { return true; }
    virtual void Stop() override { bStop = true; }
    virtual void Exit() override {}

    virtual uint32 Run() override
    {
        while (!bStop)
        {
            if (!Bridge || !Bridge->Socket) break;

            uint8 Buffer[2048];
            int32 BytesRead = 0;

            TSharedRef<FInternetAddr> SenderAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM, false)->CreateInternetAddr();
            bool bHasData = Bridge->Socket->RecvFrom(Buffer, sizeof(Buffer), BytesRead, *SenderAddr);
            if (bHasData && BytesRead > 0)
            {
                TArray<uint8> Data;
                Data.SetNumUninitialized(BytesRead);
                memcpy(Data.GetData(), Buffer, BytesRead);

                // 喂给传输优化器
                if (Bridge->Transport)
                {
                    Bridge->Transport->ProcessIncoming(Data, BytesRead);
                }
            }
            else
            {
                // 无数据，让出 CPU
                FPlatformProcess::Sleep(0.001f);  // 1ms
            }
        }
        return 0;
    }

private:
    UTransportBridge* Bridge;
    bool bStop;
};

/** 发送线程 */
class FTransportSendRunnable : public FRunnable
{
public:
    FTransportSendRunnable(UTransportBridge* InBridge) : Bridge(InBridge), bStop(false) {}

    virtual bool Init() override { return true; }
    virtual void Stop() override { bStop = true; }
    virtual void Exit() override {}

    virtual uint32 Run() override
    {
        while (!bStop)
        {
            if (!Bridge || !Bridge->Socket) break;

            // 从 Bridge 的发送队列取数据
            TArray<uint8> Data;
            bool bGotData = false;
            {
                // 通过 Bridge 的 SendQueue (线程安全)
                if (Bridge->Transport)
                {
                    // 简化：从 Transport 的发送队列取
                    // 实际应通过 Bridge 暴露的线程安全接口
                    bGotData = false; // 由 Transport 线程驱动
                }
            }

            if (bGotData && Data.Num() > 0)
            {
                int32 BytesSent = 0;
                Bridge->Socket->SendTo(Data.GetData(), Data.Num(), BytesSent,
                    *Bridge->RemoteEndpoint.ToInternetAddr());
            }

            FPlatformProcess::Sleep(0.002f);  // 2ms
        }
        return 0;
    }

private:
    UTransportBridge* Bridge;
    bool bStop;
};
