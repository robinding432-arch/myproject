// ServerNetOptimizer.cpp
// StellarSystem v6.8 — 服务器端网络优化实现

#include "Network/ServerNetOptimizer.h"
#include "HAL/PlatformTime.h"
#include "Misc/Crc.h"

// ═══════════════════════════════════════
//  构造 / 生命周期
// ═══════════════════════════════════════

UServerNetOptimizer::UServerNetOptimizer()
    : TotalBytesOut(0)
    , TotalBytesIn(0)
    , TotalPacketsOut(0)
    , TotalPacketsIn(0)
    , LastBatchFlush(0.f)
{
}

void UServerNetOptimizer::Initialize()
{
    Clients.Empty();
    ClientSendQueues.Empty();
    ClientReliableQueues.Empty();
    ClientDeliverQueues.Empty();
    ClientStats.Empty();
    ClientPriorities.Empty();
    BatchBuffers.Empty();

    LogServerNet(TEXT("ServerNetOptimizer initialized"));
}

void UServerNetOptimizer::Shutdown()
{
    for (int32 i = Clients.Num() - 1; i >= 0; i--)
    {
        RemoveClient(i);
    }
    LogServerNet(TEXT("ServerNetOptimizer shutdown"));
}

void UServerNetOptimizer::Tick(float DeltaTime)
{
    float Now = FPlatformTime::Seconds();

    // 1. 检查客户端超时
    CheckClientTimeouts(Now);

    // 2. 更新每客户端拥塞控制
    for (int32 i = 0; i < Clients.Num(); i++)
    {
        UpdateClientCongestion(i, DeltaTime);
    }

    // 3. 带宽分配
    AllocateBandwidth();

    // 4. 自适应冗余
    if (bEnableAdaptiveRedundancy)
    {
        for (int32 i = 0; i < Clients.Num(); i++)
        {
            AdaptiveRedundancy(i);
        }
    }

    // 5. 批量刷新
    if (bEnableBatching && (Now - LastBatchFlush) >= (1.f / ServerTickRate))
    {
        FlushAllBatches();
        LastBatchFlush = Now;
    }

    // 6. 更新全局统计
    float TotalOut = 0, TotalIn = 0;
    for (auto& Pair : ClientStats)
    {
        TotalOut += Pair.Value.BWOutgoing;
        TotalIn  += Pair.Value.BWIncoming;
    }
    // (统计已在各方法中更新)
}

// ═══════════════════════════════════════
//  连接管理
// ═══════════════════════════════════════

int32 UServerNetOptimizer::AddClient(const FString& ClientIP, int32 ClientPort, int32 PlayerId)
{
    // 检查是否已存在
    int32 Existing = FindClientByAddress(ClientIP, ClientPort);
    if (Existing >= 0) return Existing;

    FClientConnection Conn;
    Conn.ClientIP    = ClientIP;
    Conn.ClientPort  = ClientPort;
    Conn.PlayerId   = PlayerId;
    Conn.NextSendSeq = FMath::RandRange(0, 10000);
    Conn.NextRecvSeq = 0;
    Conn.LastActivity = FPlatformTime::Seconds();
    Conn.State       = EConnectionState::Connecting;
    Conn.Congestion.CWnd  = 64;
    Conn.Congestion.Ssthresh = 128;
    Conn.Congestion.InSlowStart = true;
    Conn.Congestion.BottleneckBW = 512.f;

    Clients.Add(Conn);
    int32 Index = Clients.Num() - 1;

    ClientSendQueues.Add(Index, TArray<FNetMessage>());
    ClientReliableQueues.Add(Index, TArray<FNetMessage>());
    ClientDeliverQueues.Add(Index, TQueue<FNetMessage>());
    ClientStats.Add(Index, FTransportStats());
    ClientPriorities.Add(Index, 1.f);

    LogServerNet(FString::Printf(TEXT("Client added: %s:%d (PlayerId=%d, Index=%d)"),
                                *ClientIP, ClientPort, PlayerId, Index));

    OnClientConnected.Broadcast(Index, PlayerId);
    return Index;
}

void UServerNetOptimizer::RemoveClient(int32 ClientIndex)
{
    if (!Clients.IsValidIndex(ClientIndex)) return;

    FString IP = Clients[ClientIndex].ClientIP;
    int32 Port = Clients[ClientIndex].ClientPort;

    Clients.RemoveAt(ClientIndex);
    ClientSendQueues.Remove(ClientIndex);
    ClientReliableQueues.Remove(ClientIndex);
    ClientDeliverQueues.Remove(ClientIndex);
    ClientStats.Remove(ClientIndex);
    ClientPriorities.Remove(ClientIndex);

    LogServerNet(FString::Printf(TEXT("Client removed: %s:%d (Index=%d)"), *IP, Port, ClientIndex));

    OnClientDisconnected.Broadcast(ClientIndex);
}

void UServerNetOptimizer::UpdateClientActivity(int32 ClientIndex, const FNetMessage& Msg)
{
    if (!Clients.IsValidIndex(ClientIndex)) return;

    Clients[ClientIndex].LastActivity = FPlatformTime::Seconds();
    Clients[ClientIndex].PacketsIn++;
    Clients[ClientIndex].BytesIn += Msg.TotalSize();

    if (Clients[ClientIndex].State == EConnectionState::Connecting)
    {
        Clients[ClientIndex].State = EConnectionState::Connected;
    }
}

FClientConnection UServerNetOptimizer::GetClientInfo(int32 ClientIndex) const
{
    if (Clients.IsValidIndex(ClientIndex))
    {
        return Clients[ClientIndex];
    }
    return FClientConnection();
}

int32 UServerNetOptimizer::FindClientByAddress(const FString& IP, int32 Port) const
{
    for (int32 i = 0; i < Clients.Num(); i++)
    {
        if (Clients[i].ClientIP == IP && Clients[i].ClientPort == Port)
        {
            return i;
        }
    }
    return -1;
}

// ═══════════════════════════════════════
//  发送
// ═══════════════════════════════════════

bool UServerNetOptimizer::SendToClient(int32 ClientIndex, ENetChannel Channel,
                                       ENetPriority Priority,
                                       const TArray<uint8>& Payload, bool bReliable)
{
    if (!Clients.IsValidIndex(ClientIndex)) return false;

    FNetMessage Msg;
    Msg.Header.Magic    = 0x5354;
    Msg.Header.Channel  = (uint8)Channel;
    Msg.Header.Priority = (uint8)Priority;
    Msg.Header.Sequence = Clients[ClientIndex].NextSendSeq;
    Clients[ClientIndex].NextSendSeq =
        (Clients[ClientIndex].NextSendSeq + 1) % 65536;

    if (bReliable) Msg.Header.SetReliable();

    Msg.Payload = Payload;
    Msg.Header.PayloadSize = Payload.Num();
    Msg.Header.Checksum = FCrc::MemCrc16(Payload.GetData(), Payload.Num());

    // 放入对应队列
    if (bReliable)
    {
        ClientReliableQueues[ClientIndex].Add(Msg);
    }

    // 按优先级插入发送队列
    TArray<FNetMessage>& Queue = ClientSendQueues[ClientIndex];
    int32 InsertIdx = 0;
    for (int32 i = 0; i < Queue.Num(); i++)
    {
        if ((int32)Priority >= (int32)(ENetPriority)Queue[i].Header.Priority)
        {
            InsertIdx = i;
            break;
        }
        InsertIdx = i + 1;
    }
    Queue.Insert(Msg, InsertIdx);

    // 更新统计
    ClientStats[ClientIndex].PacketsSent++;
    ClientStats[ClientIndex].BytesSent += Msg.TotalSize();
    TotalPacketsOut++;
    TotalBytesOut += Msg.TotalSize();

    return true;
}

void UServerNetOptimizer::BroadcastToAll(ENetChannel Channel, ENetPriority Priority,
                                          const TArray<uint8>& Payload, bool bReliable)
{
    for (int32 i = 0; i < Clients.Num(); i++)
    {
        SendToClient(i, Channel, Priority, Payload, bReliable);
    }
}

void UServerNetOptimizer::BroadcastToNearby(ENetChannel Channel, ENetPriority Priority,
                                              const TArray<uint8>& Payload,
                                              const FVector& SourceLocation, float Radius,
                                              bool bReliable)
{
    // 简化：需要位置信息，这里广播给所有（实际应查询空间哈希）
    // TODO: 接入 World 的空间查询
    BroadcastToAll(Channel, Priority, Payload, bReliable);
}

void UServerNetOptimizer::SendWorldSnapshot(int32 ClientIndex, const TArray<FPredictedState>& WorldState)
{
    if (!Clients.IsValidIndex(ClientIndex)) return;

    // 确定冗余份数（高丢包 → 更多冗余）
    int32 Redundancy = 3;
    if (Clients[ClientIndex].LossRate > HighLossThreshold)
    {
        Redundancy = 5;
    }

    // 构建快照
    FSnapshot Snap;
    Snap.Seq = (uint16)(Clients[ClientIndex].RecentSnapshots.Num() + 1);
    Snap.ServerTime = FPlatformTime::Seconds();
    Snap.ActorStates = WorldState;
    Snap.Redundancy = (uint8)Redundancy;

    Clients[ClientIndex].RecentSnapshots.Add(Snap);
    if (Clients[ClientIndex].RecentSnapshots.Num() > 32)
    {
        Clients[ClientIndex].RecentSnapshots.RemoveAt(0);
    }

    // 序列化
    TArray<uint8> Serialized;
    int32 StateSize = 2 + 4 + 2 + WorldState.Num() * 50; // 估计
    Serialized.SetNumUninitialized(StateSize);
    int32 Off = 0;
    memcpy(Serialized.GetData() + Off, &Snap.Seq, 2); Off += 2;
    memcpy(Serialized.GetData() + Off, &Snap.ServerTime, 4); Off += 4;
    uint16 Count = (uint16)WorldState.Num();
    memcpy(Serialized.GetData() + Off, &Count, 2); Off += 2;

    for (const auto& PS : WorldState)
    {
        memcpy(Serialized.GetData() + Off, &PS.SequenceNum, 2); Off += 2;
        memcpy(Serialized.GetData() + Off, &PS.Position.X, 4); Off += 4;
        memcpy(Serialized.GetData() + Off, &PS.Position.Y, 4); Off += 4;
        memcpy(Serialized.GetData() + Off, &PS.Position.Z, 4); Off += 4;
        memcpy(Serialized.GetData() + Off, &PS.Rotation.Pitch, 4); Off += 4;
        memcpy(Serialized.GetData() + Off, &PS.Rotation.Yaw, 4); Off += 4;
        memcpy(Serialized.GetData() + Off, &PS.Rotation.Roll, 4); Off += 4;
        memcpy(Serialized.GetData() + Off, &PS.Timestamp, 4); Off += 4;
        memcpy(Serialized.GetData() + Off, &PS.InputFlags, 1); Off += 1;
    }
    Serialized.SetNum(Off);

    // 冗余发送
    for (int32 r = 0; r < Redundancy; r++)
    {
        SendToClient(ClientIndex, ENetChannel::UnreliableSequenced,
                     ENetPriority::High, Serialized, false);
    }

    ClientStats[ClientIndex].SnapshotsSent++;
}

void UServerNetOptimizer::FlushAllBatches()
{
    for (int32 i = 0; i < Clients.Num(); i++)
    {
        TArray<FNetMessage>& Queue = ClientSendQueues[i];
        if (Queue.Num() == 0) continue;

        // 构建批量包
        TArray<uint8> Batched;
        int32 Count = Queue.Num();
        Batched.SetNumUninitialized(4);
        memcpy(Batched.GetData(), &Count, 4);

        for (const auto& Msg : Queue)
        {
            int32 MsgSize = Msg.TotalSize();
            int32 OldLen = Batched.Num();
            Batched.SetNumUninitialized(OldLen + 2 + MsgSize);
            uint16 Size16 = (uint16)MsgSize;
            memcpy(Batched.GetData() + OldLen, &Size16, 2);
            memcpy(Batched.GetData() + OldLen + 2, &Msg.Header, sizeof(FNetMessageHeader));
            memcpy(Batched.GetData() + OldLen + 2 + sizeof(FNetMessageHeader),
                   Msg.Payload.GetData(), Msg.Payload.Num());
        }

        // 发送批量包
        FNetMessage BatchMsg;
        BatchMsg.Header.Magic = 0x5354;
        BatchMsg.Header.Channel = (uint8)ENetChannel::Priority;
        BatchMsg.Header.Priority = (uint8)ENetPriority::Normal;
        BatchMsg.Header.Sequence = Clients[i].NextSendSeq;
        Clients[i].NextSendSeq = (Clients[i].NextSendSeq + 1) % 65536;
        BatchMsg.Header.SetCompressed();
        BatchMsg.Payload = Batched;
        BatchMsg.Header.PayloadSize = Batched.Num();
        BatchMsg.Header.Checksum = FCrc::MemCrc16(Batched.GetData(), Batched.Num());

        // 通过 Socket 发送（需要外部 Socket 引用）
        // 这里通过委托或直接调用
        SendRawToClient(i, BatchMsg);

        Queue.Empty();
    }
}

// ═══════════════════════════════════════
//  接收
// ═══════════════════════════════════════

void UServerNetOptimizer::ProcessIncomingFromClient(int32 ClientIndex,
                                                     const TArray<uint8>& RawData,
                                                     int32 DataSize)
{
    if (!Clients.IsValidIndex(ClientIndex)) return;
    if (DataSize < sizeof(FNetMessageHeader)) return;

    FNetMessage Msg;
    memcpy(&Msg.Header, RawData.GetData(), sizeof(FNetMessageHeader));

    if (Msg.Header.Magic != 0x5354)
    {
        LogServerNet(FString::Printf(TEXT("Client %d: Invalid magic"), ClientIndex), true);
        return;
    }

    if (Msg.Header.PayloadSize > 0)
    {
        Msg.Payload.SetNumUninitialized(Msg.Header.PayloadSize);
        memcpy(Msg.Payload.GetData(), RawData.GetData() + sizeof(FNetMessageHeader),
               Msg.Header.PayloadSize);

        uint16 CalcCRC = FCrc::MemCrc16(Msg.Payload.GetData(), Msg.Payload.Num());
        if (CalcCRC != Msg.Header.Checksum)
        {
            Clients[ClientIndex].LossRate = FMath::Min(1.f,
                Clients[ClientIndex].LossRate + 0.01f);
            return;
        }
    }

    // 更新活动
    UpdateClientActivity(ClientIndex, Msg);

    // 处理 ACK
    if (Msg.Header.IsAck())
    {
        FNetAckPacket Ack;
        if (Msg.Payload.Num() >= 12)
        {
            memcpy(&Ack.BaseSequence, Msg.Payload.GetData(), 2);
            memcpy(&Ack.AckBitfield, Msg.Payload.GetData() + 2, 4);
            memcpy(&Ack.RTT, Msg.Payload.GetData() + 6, 4);
            memcpy(&Ack.Jitter, Msg.Payload.GetData() + 10, 2);

            // 从可靠队列移除
            TArray<FNetMessage>& RelQ = ClientReliableQueues[ClientIndex];
            for (int32 j = RelQ.Num() - 1; j >= 0; j--)
            {
                if (RelQ[j].Header.Sequence == Ack.BaseSequence)
                {
                    RelQ.RemoveAt(j);
                    break;
                }
            }

            // 更新 RTT
            if (Ack.RTT > 0)
            {
                Clients[ClientIndex].Ping = (Clients[ClientIndex].Ping * 0.8f) + ((float)Ack.RTT * 0.2f);
            }
        }
        return;
    }

    // 发送 ACK
    FNetMessage AckMsg = CreateAckForClient(ClientIndex);
    SendRawToClient(ClientIndex, AckMsg);

    // 放入交付队列
    ClientDeliverQueues[ClientIndex].Enqueue(Msg);

    // 通知
    OnClientMessage.Broadcast(ClientIndex, Msg);
}

bool UServerNetOptimizer::DequeueMessageForClient(int32 ClientIndex, FNetMessage& OutMessage)
{
    if (!ClientDeliverQueues.Contains(ClientIndex)) return false;
    return ClientDeliverQueues[ClientIndex].Dequeue(OutMessage);
}

// ═══════════════════════════════════════
//  带宽管理
// ═══════════════════════════════════════

void UServerNetOptimizer::AllocateBandwidth()
{
    if (Clients.Num() == 0 || MaxBandwidthKBps <= 0) return;

    // 按优先级分配带宽
    float TotalPriority = 0.f;
    for (int32 i = 0; i < Clients.Num(); i++)
    {
        float P = ClientPriorities.Contains(i) ? ClientPriorities[i] : 1.f;
        TotalPriority += P;
    }

    for (int32 i = 0; i < Clients.Num(); i++)
    {
        float P = ClientPriorities.Contains(i) ? ClientPriorities[i] : 1.f;
        float Allocated = MaxBandwidthKBps * (P / TotalPriority);
        Clients[i].BWLimitKBps = Allocated;

        // 如果客户端超出带宽，丢弃低优先级包
        float CurrentBPS = ClientStats[i].BWOutgoing;
        if (CurrentBPS > Allocated)
        {
            TArray<FNetMessage>& Queue = ClientSendQueues[i];
            for (int32 j = Queue.Num() - 1; j >= 0; j--)
            {
                ENetPriority P = (ENetPriority)Queue[j].Header.Priority;
                if (P == ENetPriority::Low || P == ENetPriority::Background)
                {
                    Queue.RemoveAt(j);
                }
            }
        }
    }
}

void UServerNetOptimizer::SetClientPriority(int32 ClientIndex, float Priority)
{
    if (Clients.IsValidIndex(ClientIndex))
    {
        ClientPriorities[ClientIndex] = FMath::Clamp(Priority, 0.f, 10.f);
    }
}

// ═══════════════════════════════════════
//  统计
// ═══════════════════════════════════════

float UServerNetOptimizer::GetTotalBandwidthOut() const
{
    float Total = 0;
    for (const auto& Pair : ClientStats)
    {
        Total += Pair.Value.BWOutgoing;
    }
    return Total;
}

float UServerNetOptimizer::GetTotalBandwidthIn() const
{
    float Total = 0;
    for (const auto& Pair : ClientStats)
    {
        Total += Pair.Value.BWIncoming;
    }
    return Total;
}

float UServerNetOptimizer::GetAveragePing() const
{
    if (Clients.Num() == 0) return 0.f;
    float Sum = 0;
    for (const auto& C : Clients)
    {
        Sum += C.Ping;
    }
    return Sum / (float)Clients.Num();
}

float UServerNetOptimizer::GetWorstLossRate() const
{
    float Worst = 0;
    for (const auto& C : Clients)
    {
        Worst = FMath::Max(Worst, C.LossRate);
    }
    return Worst;
}

int32 UServerNetOptimizer::GetTotalPacketsOut() const
{
    return TotalPacketsOut;
}

FString UServerNetOptimizer::GetDiagnosticString() const
{
    FString Result = FString::Printf(TEXT(
        "===== ServerNet Diagnostic =====\n"
        "  Clients: %d\n"
        "  Total Out: %.1f KB/s  Total In: %.1f KB/s\n"
        "  Total Packets Out: %d  In: %d\n"
        "  Avg Ping: %.0f ms  Worst Loss: %.1f%%\n"
        "  [Per-Client]\n"),
        Clients.Num(),
        GetTotalBandwidthOut(), GetTotalBandwidthIn(),
        TotalPacketsOut, TotalPacketsIn,
        GetAveragePing(), GetWorstLossRate() * 100.f
    );

    for (int32 i = 0; i < Clients.Num(); i++)
    {
        const FClientConnection& C = Clients[i];
        Result += FString::Printf(TEXT(
            "    [%d] %s:%d  Ping=%dms  Loss=%.1f%%  "
            "In=%dKB  Out=%dKB  CWnd=%d  State=%d\n"),
            i, *C.ClientIP, C.ClientPort,
            (int)C.Ping, C.LossRate * 100.f,
            C.BytesIn / 1024, C.BytesOut / 1024,
            C.Congestion.CWnd, (int)C.State
        );
    }

    Result += TEXT("===== End Diagnostic =====");
    return Result;
}

// ═══════════════════════════════════════
//  内部
// ═══════════════════════════════════════

void UServerNetOptimizer::SendRawToClient(int32 ClientIndex, const FNetMessage& Msg)
{
    // 实际发送需要通过 Socket
    // 这里存储到待发送列表，由外部驱动
    // 或通过委托通知 Bridge 层
    Clients[ClientIndex].BytesOut += Msg.TotalSize();
    Clients[ClientIndex].PacketsOut++;
    TotalBytesOut += Msg.TotalSize();
    TotalPacketsOut++;
}

void UServerNetOptimizer::ProcessAckFromClient(int32 ClientIndex, const FNetAckPacket& Ack)
{
    // 已在 ProcessIncomingFromClient 中处理
}

void UServerNetOptimizer::UpdateClientCongestion(int32 ClientIndex, float DeltaTime)
{
    FClientConnection& C = Clients[ClientIndex];
    FTransportStats& S = ClientStats[ClientIndex];

    // 简化 BBR
    float LossRate = C.LossRate;
    if (LossRate > 0.1f)
    {
        // 丢包 → 降窗
        C.Congestion.CWnd = FMath::Max(4, C.Congestion.CWnd / 2);
        C.Congestion.InSlowStart = false;
        C.Congestion.Ssthresh = C.Congestion.CWnd;
    }
    else
    {
        if (C.Congestion.InSlowStart)
        {
            C.Congestion.CWnd += 1;
            if (C.Congestion.CWnd >= C.Congestion.Ssthresh)
            {
                C.Congestion.InSlowStart = false;
            }
        }
        else
        {
            C.Congestion.CWnd += 1.0f / FMath::Max(C.Congestion.CWnd, 1);
        }
    }

    // 更新带宽估计
    if (DeltaTime > 0)
    {
        float Delivered = C.BytesOut;
        float Bps = Delivered / DeltaTime / 1024.f;
        C.Congestion.BottleneckBW = C.Congestion.BottleneckBW * 0.9f + Bps * 0.1f;
        C.BytesOut = 0;  // 重置计数
    }

    // 更新统计
    S.BWOutgoing = C.Congestion.BottleneckBW;
    S.AvgRTT = C.Ping;
    S.LossRate = LossRate;
}

void UServerNetOptimizer::CheckClientTimeouts(float CurrentTime)
{
    TArray<int32> ToRemove;
    for (int32 i = 0; i < Clients.Num(); i++)
    {
        float Idle = CurrentTime - Clients[i].LastActivity;
        if (Idle > ClientTimeout)
        {
            LogServerNet(FString::Printf(TEXT("Client %d timeout (idle %.0fs)"), i, Idle), true);
            ToRemove.Add(i);
        }
    }
    for (int32 Idx : ToRemove)
    {
        RemoveClient(Idx);
    }
}

void UServerNetOptimizer::AdaptiveRedundancy(int32 ClientIndex)
{
    FClientConnection& C = Clients[ClientIndex];

    if (C.LossRate > 0.2f)
    {
        C.RecentSnapshots.Last().Redundancy = 5;
    }
    else if (C.LossRate > 0.1f)
    {
        C.RecentSnapshots.Last().Redundancy = 4;
    }
    else
    {
        C.RecentSnapshots.Last().Redundancy = 3;
    }
}

FNetMessage UServerNetOptimizer::CreateAckForClient(int32 ClientIndex)
{
    FNetMessage Ack;
    Ack.Header.Magic = 0x5354;
    Ack.Header.Channel = (uint8)ENetChannel::Heartbeat;
    Ack.Header.SetAck();
    Ack.Header.Sequence = Clients[ClientIndex].NextSendSeq;
    Clients[ClientIndex].NextSendSeq =
        (Clients[ClientIndex].NextSendSeq + 1) % 65536;

    Ack.Payload.SetNumUninitialized(12);
    uint16 BaseSeq = 0; // 应从可靠队列获取最新确认
    memcpy(Ack.Payload.GetData(), &BaseSeq, 2);
    uint32 Bitfield = 0;
    memcpy(Ack.Payload.GetData() + 2, &Bitfield, 4);
    int32 RTT = (int32)Clients[ClientIndex].Ping;
    memcpy(Ack.Payload.GetData() + 6, &RTT, 4);
    int32 Jitter = 0;
    memcpy(Ack.Payload.GetData() + 10, &Jitter, 2);

    Ack.Header.PayloadSize = 12;
    Ack.Header.Checksum = FCrc::MemCrc16(Ack.Payload.GetData(), 12);

    return Ack;
}

void UServerNetOptimizer::LogServerNet(const FString& Msg, bool bError)
{
    FString Prefix = bError ? TEXT("[ServerNet][ERROR] ")
                            : TEXT("[ServerNet] ");
    if (bError)
        UE_LOG(LogTemp, Error, TEXT("%s%s"), *Prefix, *Msg);
    else
        UE_LOG(LogTemp, Log, TEXT("%s%s"), *Prefix, *Msg);
}
