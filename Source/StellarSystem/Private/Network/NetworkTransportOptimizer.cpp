// NetworkTransportOptimizer.cpp
// StellarSystem v6.8 — 传输优化器完整实现

#include "Network/NetworkTransportOptimizer.h"
#include "HAL/PlatformTime.h"
#include "HAL/UnrealMemory.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"
#include "Misc/Crc.h"
#include "Misc/ScopeLock.h"
#include "Async/Async.h"

// ═══════════════════════════════════════
//  构造 / 生命周期
// ═══════════════════════════════════════

UNetworkTransportOptimizer::UNetworkTransportOptimizer()
    : bIsServer(false)
    , ConnectionState(EConnectionState::Disconnected)
    , NextSendSeq(0)
    , NextRecvSeq(0)
    , LastAckedSeq(0)
    , JitterBufferSize(0.05f)
    , JitterAccumulator(0.f)
    , LastRTTSample(0.f)
    , RTTAccumulator(0.f)
    , RTTSamples(0)
    , LastHeartbeatTime(0.f)
    , MissedHeartbeats(0)
    , OptimalMTU(1200)
    , bMTUProbing(false)
    , MTUProbeSize(576)
    , LastMTUProbeTime(0.f)
    , LastBatchFlush(0.f)
    , AckBaseSeq(0)
{
    // 初始化拥塞状态
    Congestion.BottleneckBW   = BandwidthLimitKBps > 0 ? BandwidthLimitKBps : 1000.f;
    Congestion.MinRTT         = 50.f;
    Congestion.SmoothedRTT    = 80.f;
    Congestion.RTTVar         = 10.f;
    Congestion.CWnd           = 64;
    Congestion.Ssthresh       = 128;
    Congestion.InSlowStart    = true;
    Congestion.BytesInFlight  = 0;
    Congestion.LastLossTime   = 0.f;
}

void UNetworkTransportOptimizer::Initialize(bool bInIsServer, const FString& InRemoteAddr)
{
    bIsServer  = bInIsServer;
    RemoteAddr = InRemoteAddr;

    SetConnectionState(EConnectionState::Connecting);

    // 初始化序列号（随机起点防重放）
    NextSendSeq = FMath::RandRange(0, 10000);
    NextRecvSeq = 0;
    LastAckedSeq = NextSendSeq;

    // 启动 MTU 探测
    StartMTUProbe();

    // 发送握手
    FNetMessage Handshake;
    Handshake.Header.Magic = 0x5354;
    Handshake.Header.Channel = (uint8)ENetChannel::Heartbeat;
    Handshake.Header.SetReliable();
    Handshake.Header.Sequence = NextSendSeq;
    NextSendSeq = IncrementSeq(NextSendSeq);
    Handshake.Payload.Add(bIsServer ? 0x01 : 0x00);  // 0=client, 1=server
    // 写入 MTU 探测大小
    TArray<uint8> MTUBytes;
    MTUBytes.SetNumUninitialized(4);
    memcpy(MTUBytes.GetData(), &MTUProbeSize, 4);
    Handshake.Payload.Append(MTUBytes);

    SendQueue.Insert(Handshake, 0);  // 插队到最前

    LogTransportEvent(TEXT("Transport initialized (MTU probe started)"));
}

void UNetworkTransportOptimizer::Shutdown()
{
    // 发送断开通知
    if (ConnectionState != EConnectionState::Disconnected)
    {
        FNetMessage Disconnect;
        Disconnect.Header.Magic = 0x5354;
        Disconnect.Header.Channel = (uint8)ENetChannel::Heartbeat;
        Disconnect.Header.SetReliable();
        Disconnect.Header.Sequence = NextSendSeq;
        NextSendSeq = IncrementSeq(NextSendSeq);
        Disconnect.Payload.Add(0xFF);  // disconnect flag
        SendQueue.Insert(Disconnect, 0);
        FlushBatch();
    }

    // 清空队列
    SendQueue.Empty();
    ReliableQueue.Empty();
    RecvQueue.Empty();
    DeliverQueue.Empty();
    FragmentAssembly.Empty();
    FragmentTimers.Empty();
    PredictedStates.Empty();
    SnapshotBuffer.Empty();
    InterpCache.Empty();
    ServerSnapshotHistory.Empty();
    BatchBuffer.Empty();
    PendingAcks.Empty();

    SetConnectionState(EConnectionState::Disconnected);
    LogTransportEvent(TEXT("Transport shutdown complete"));
}

void UNetworkTransportOptimizer::Tick(float DeltaTime)
{
    if (ConnectionState == EConnectionState::Disconnected) return;

    // 1. 更新 RTT / 抖动统计
    UpdateStats(DeltaTime);

    // 2. 检查重传
    CheckRetransmits(DeltaTime);

    // 3. 更新抖动缓冲
    UpdateJitterBuffer(DeltaTime);

    // 4. 心跳
    UpdateHeartbeat(DeltaTime);

    // 5. MTU 探测
    UpdateMTUProbe(DeltaTime);

    // 6. 批量刷新
    if (BatchBuffer.Num() > 0 && (FPlatformTime::Seconds() - LastBatchFlush) > 0.05f)
    {
        FlushBatch();
    }

    // 7. 带宽限制
    EnforceBandwidthLimit(DeltaTime);

    // 8. 清理过期分片
    CleanupOldFragments(FPlatformTime::Seconds());

    // 9. 服务器时间推进
    if (bIsServer)
    {
        ServerTime += DeltaTime;
    }

    // 10. 拥塞状态更新
    if (Stats.LossRate > 0.1f)
    {
        SetConnectionState(EConnectionState::Congested);
    }
    else if (ConnectionState == EConnectionState::Congested && Stats.LossRate < 0.05f)
    {
        SetConnectionState(EConnectionState::Connected);
    }
}

// ═══════════════════════════════════════
//  发送
// ═══════════════════════════════════════

bool UNetworkTransportOptimizer::SendMessage(ENetChannel Channel, ENetPriority Priority,
                                             const TArray<uint8>& Payload, bool bReliable)
{
    if (ConnectionState == EConnectionState::Disconnected) return false;

    FNetMessage Msg;
    Msg.Header.Magic    = 0x5354;
    Msg.Header.Channel  = (uint8)Channel;
    Msg.Header.Priority = (uint8)Priority;
    Msg.Header.Sequence = NextSendSeq;
    NextSendSeq = IncrementSeq(NextSendSeq);

    if (bReliable)
    {
        Msg.Header.SetReliable();
        // 存入可靠队列等待确认
        ReliableQueue.Add(Msg);
    }

    // 压缩
    TArray<uint8> ProcessedPayload = Payload;
    if (bEnableDeltaCompression && ProcessedPayload.Num() > CompressionThreshold)
    {
        // 尝试增量压缩（与上一个同通道消息比较）
        // 简化：直接用 LZ4
        if (CompressPayload(ProcessedPayload, DefaultCompression))
        {
            Msg.Header.SetCompressed();
            Stats.BytesUncompressed += Payload.Num();
            Stats.BytesCompressed  += ProcessedPayload.Num();
        }
    }

    // 位压缩（小整数优化）
    if (bEnableBitPacking && ProcessedPayload.Num() < 64)
    {
        // 对 <= 64 字节的小包尝试位压缩
        // 实际压缩在 CompressPayload 内部处理
    }

    Msg.Payload = ProcessedPayload;
    Msg.Header.PayloadSize = ProcessedPayload.Num();
    Msg.Header.Checksum = FCrc::MemCrc16(ProcessedPayload.GetData(), ProcessedPayload.Num());

    // 分片检查
    int32 TotalMsgSize = Msg.TotalSize();
    if (TotalMsgSize > OptimalMTU)
    {
        // 需要分片
        TArray<FNetMessage> Frags;
        FragmentMessage(Msg, Frags);
        for (auto& Frag : Frags)
        {
            EnqueueByPriority(Frag, Priority);
            Stats.FragmentsSent++;
        }
    }
    else
    {
        EnqueueByPriority(Msg, Priority);
    }

    Stats.BytesSent += TotalMsgSize;
    Stats.PacketsSent++;

    return true;
}

void UNetworkTransportOptimizer::EnqueueByPriority(FNetMessage& Msg, ENetPriority Priority)
{
    // 按优先级插入（高优先级在前）
    int32 InsertIdx = 0;
    for (int32 i = 0; i < SendQueue.Num(); i++)
    {
        ENetPriority QP = (ENetPriority)SendQueue[i].Header.Priority;
        if ((int32)Priority >= (int32)QP)
        {
            InsertIdx = i;
            break;
        }
        InsertIdx = i + 1;
    }
    SendQueue.Insert(Msg, InsertIdx);
    Stats.SendQueueSize = SendQueue.Num();
}

bool UNetworkTransportOptimizer::SendStruct(ENetChannel Channel, ENetPriority Priority,
                                           const FString& StructName, const TArray<uint8>& SerializedData)
{
    // 在 payload 前加结构体名称（用于接收端反序列化路由）
    TArray<uint8> FullPayload;
    FTCHARToUTF8 Conv(*StructName);
    int32 NameLen = Conv.Length();
    FullPayload.SetNumUninitialized(4 + NameLen + SerializedData.Num());
    memcpy(FullPayload.GetData(), &NameLen, 4);
    memcpy(FullPayload.GetData() + 4, Conv.Get(), NameLen);
    memcpy(FullPayload.GetData() + 4 + NameLen, SerializedData.GetData(), SerializedData.Num());

    return SendMessage(Channel, Priority, FullPayload, true);
}

uint16 UNetworkTransportOptimizer::SendInputCommand(const FPredictedState& InputState)
{
    // 客户端预测：保存输入到历史
    if (!bIsServer)
    {
        PredictedStates.Add(InputState);
        if (PredictedStates.Num() > MaxPredictedStates)
        {
            PredictedStates.RemoveAt(0);
        }
    }

    // 序列化输入状态
    TArray<uint8> Data;
    Data.SetNumUninitialized(4 + 12 + 12 + 4 + 1);
    int32 Offset = 0;
    memcpy(Data.GetData() + Offset, &InputState.SequenceNum, 2); Offset += 2;
    memcpy(Data.GetData() + Offset, &InputState.SequenceNum, 2); Offset += 2; // padding
    // Position
    memcpy(Data.GetData() + Offset, &InputState.Position.X, 4); Offset += 4;
    memcpy(Data.GetData() + Offset, &InputState.Position.Y, 4); Offset += 4;
    memcpy(Data.GetData() + Offset, &InputState.Position.Z, 4); Offset += 4;
    // Rotation (as Euler)
    memcpy(Data.GetData() + Offset, &InputState.Rotation.Pitch, 4); Offset += 4;
    memcpy(Data.GetData() + Offset, &InputState.Rotation.Yaw, 4); Offset += 4;
    memcpy(Data.GetData() + Offset, &InputState.Rotation.Roll, 4); Offset += 4;
    // Timestamp
    memcpy(Data.GetData() + Offset, &InputState.Timestamp, 4); Offset += 4;
    // InputFlags
    memcpy(Data.GetData() + Offset, &InputState.InputFlags, 1);

    SendMessage(ENetChannel::Unreliable, ENetPriority::High, Data, false);
    return InputState.SequenceNum;
}

void UNetworkTransportOptimizer::FlushBatch()
{
    // 合并小包为一个大包发送
    if (BatchBuffer.Num() == 0) return;

    int32 TotalSize = 0;
    for (const auto& M : BatchBuffer) TotalSize += M.TotalSize();

    if (TotalSize <= OptimalMTU)
    {
        // 合并发送
        FNetMessage Batched;
        Batched.Header.Magic = 0x5354;
        Batched.Header.Channel = (uint8)ENetChannel::Priority;
        Batched.Header.Priority = (uint8)ENetPriority::Normal;
        Batched.Header.Sequence = NextSendSeq;
        NextSendSeq = IncrementSeq(NextSendSeq);
        Batched.Header.SetCompressed();

        // 写入合并头
        TArray<uint8> Merged;
        Merged.SetNumUninitialized(4);  // count
        int32 Count = BatchBuffer.Num();
        memcpy(Merged.GetData(), &Count, 4);

        for (const auto& M : BatchBuffer)
        {
            // 每条消息写 [2字节size][header][payload]
            int32 MsgSize = M.TotalSize();
            int32 OldLen = Merged.Num();
            Merged.SetNumUninitialized(OldLen + 2 + MsgSize);
            uint16 Size16 = (uint16)MsgSize;
            memcpy(Merged.GetData() + OldLen, &Size16, 2);
            memcpy(Merged.GetData() + OldLen + 2, &M.Header, sizeof(FNetMessageHeader));
            memcpy(Merged.GetData() + OldLen + 2 + sizeof(FNetMessageHeader),
                   M.Payload.GetData(), M.Payload.Num());
        }

        Batched.Payload = Merged;
        Batched.Header.PayloadSize = Merged.Num();
        SendQueue.Insert(Batched, 0);  // 优先发送
    }
    else
    {
        // 太大，逐个发送
        for (auto& M : BatchBuffer)
        {
            SendQueue.Add(M);
        }
    }

    BatchBuffer.Empty();
    LastBatchFlush = FPlatformTime::Seconds();
}

// ═══════════════════════════════════════
//  接收
// ═══════════════════════════════════════

void UNetworkTransportOptimizer::ProcessIncoming(const TArray<uint8>& RawData, int32 DataSize)
{
    if (DataSize < sizeof(FNetMessageHeader)) return;

    // 解析头部
    FNetMessage Msg;
    memcpy(&Msg.Header, RawData.GetData(), sizeof(FNetMessageHeader));

    // 校验 Magic
    if (Msg.Header.Magic != 0x5354)
    {
        LogTransportEvent(TEXT("Invalid magic number"), true);
        return;
    }

    // 校验 Checksum
    if (Msg.Header.PayloadSize > 0 && Msg.Header.PayloadSize <= (uint16)(DataSize - sizeof(FNetMessageHeader)))
    {
        Msg.Payload.SetNumUninitialized(Msg.Header.PayloadSize);
        memcpy(Msg.Payload.GetData(), RawData.GetData() + sizeof(FNetMessageHeader), Msg.Header.PayloadSize);

        uint16 CalcCRC = FCrc::MemCrc16(Msg.Payload.GetData(), Msg.Payload.Num());
        if (CalcCRC != Msg.Header.Checksum)
        {
            Stats.PacketsLost++;
            LogTransportEvent(FString::Printf(TEXT("Checksum mismatch (got %u, calc %u)"),
                              Msg.Header.Checksum, CalcCRC), true);
            return;  // 丢弃损坏包
        }
    }

    Stats.BytesReceived += DataSize;
    Stats.PacketsReceived++;

    // 解压
    if (Msg.Header.IsCompressed() && Msg.Payload.Num() > 0)
    {
        DecompressPayload(Msg.Payload, DefaultCompression);
    }

    // 处理 ACK
    if (Msg.Header.IsAck())
    {
        FNetAckPacket Ack;
        // 解析 ACK 包
        if (Msg.Payload.Num() >= 10)
        {
            memcpy(&Ack.BaseSequence, Msg.Payload.GetData(), 2);
            memcpy(&Ack.AckBitfield, Msg.Payload.GetData() + 2, 4);
            memcpy(&Ack.RTT, Msg.Payload.GetData() + 6, 4);
            memcpy(&Ack.Jitter, Msg.Payload.GetData() + 10, 4);
            ProcessAckPacket(Ack);
        }
        return;
    }

    // 分片重组
    if (Msg.Header.IsFragment())
    {
        FNetMessage Complete;
        if (ReassembleFragment(Msg, Complete))
        {
            // 重组完成，继续处理
            Msg = Complete;
        }
        else
        {
            return;  // 等待更多分片
        }
    }

    // 重复检测
    if (IsSeqNewer(Msg.Header.Sequence, NextRecvSeq) == false &&
        Msg.Header.Sequence != NextRecvSeq)
    {
        // 旧包或重复，丢弃
        return;
    }

    // 更新接收序列
    NextRecvSeq = Msg.Header.Sequence;

    // 发送 ACK
    SendAck(Msg.Header.Sequence);

    // 抖动缓冲
    if (bAdaptiveJitter && Msg.Header.IsReliable() == false)
    {
        // 不可靠消息走抖动缓冲
        JitterBuffer.Add(Msg);
    }
    else
    {
        // 可靠消息直接交付
        DeliverQueue.Enqueue(Msg);
        OnMessageReceived.Broadcast(Msg);
    }

    // 更新 RTT 估计（从 ACK 往返计算）
    if (Msg.Header.Ack != 0)
    {
        float Now = FPlatformTime::Seconds();
        // 简化：用序列号差估算
    }
}

bool UNetworkTransportOptimizer::DequeueMessage(FNetMessage& OutMessage)
{
    if (DeliverQueue.IsEmpty()) return false;
    return DeliverQueue.Dequeue(OutMessage);
}

// ═══════════════════════════════════════
//  快照 / 预测 / 回滚
// ═══════════════════════════════════════

void UNetworkTransportOptimizer::SendSnapshot(const TArray<FPredictedState>& WorldState)
{
    if (!bIsServer) return;

    // 生成快照
    FSnapshot Snap;
    Snap.Seq = (uint16)(ServerSnapshotHistory.Num() + 1);
    Snap.ServerTime = ServerTime;
    Snap.ActorStates = WorldState;
    Snap.Redundancy = SnapshotRedundancy;

    // 存入历史（用于延迟补偿）
    ServerSnapshotHistory.Add(Snap);
    if (ServerSnapshotHistory.Num() > 128)
    {
        ServerSnapshotHistory.RemoveAt(0);
    }

    // 冗余发送：发送 N 份（带不同序列号）
    for (int32 i = 0; i < Snap.Redundancy; i++)
    {
        TArray<uint8> Serialized;
        // 序列化快照
        Serialized.SetNumUninitialized(2 + 4 + 2 + WorldState.Num() * (2+12+12+4+1));
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

        // 冗余副本用 UnreliableSequenced
        SendMessage(ENetChannel::UnreliableSequenced, ENetPriority::High, Serialized, false);
    }

    Stats.SnapshotsSent++;
}

void UNetworkTransportOptimizer::ApplySnapshot(const FSnapshot& Snapshot, float RenderTime)
{
    if (bIsServer) return;

    // 存入缓冲
    SnapshotBuffer.Add(Snapshot);
    if (SnapshotBuffer.Num() > 32) SnapshotBuffer.RemoveAt(0);

    // 按序列排序
    SnapshotBuffer.Sort([](const FSnapshot& A, const FSnapshot& B) {
        return A.Seq < B.Seq;
    });

    // 找到两个相邻快照做插值
    if (SnapshotBuffer.Num() >= 2)
    {
        const FSnapshot* Older = nullptr;
        const FSnapshot* Newer = nullptr;

        for (int32 i = 0; i < SnapshotBuffer.Num() - 1; i++)
        {
            if (SnapshotBuffer[i].ServerTime <= RenderTime &&
                SnapshotBuffer[i + 1].ServerTime >= RenderTime)
            {
                Older = &SnapshotBuffer[i];
                Newer = &SnapshotBuffer[i + 1];
                break;
            }
        }

        if (Older && Newer && Older->ActorStates.Num() > 0)
        {
            float TimeSpan = Newer->ServerTime - Older->ServerTime;
            float Alpha = (TimeSpan > 0) ? (RenderTime - Older->ServerTime) / TimeSpan : 0.f;
            Alpha = FMath::Clamp(Alpha, 0.f, 1.f);

            FPredictedState Interp = InterpolateState(*Older, *Newer, Alpha);
            // 存入缓存供 GetInterpolatedState 使用
            // (简化：只存第一个 Actor 的状态)
            if (InterpCache.Num() == 0)
            {
                InterpCache.Add(FName("Default"), Interp);
            }
            else
            {
                InterpCache[FName("Default")] = Interp;
            }
        }
    }
}

bool UNetworkTransportOptimizer::GetInterpolatedState(FPredictedState& OutState, const FName& ActorName)
{
    if (InterpCache.Contains(ActorName))
    {
        OutState = InterpCache[ActorName];
        return true;
    }
    if (InterpCache.Num() > 0)
    {
        // 返回默认
        for (auto& Pair : InterpCache)
        {
            OutState = Pair.Value;
            return true;
        }
    }
    return false;
}

FPredictedState UNetworkTransportOptimizer::InterpolateState(const FSnapshot& Older,
                                                            const FSnapshot& Newer,
                                                            float Alpha)
{
    FPredictedState Result;
    Result.SequenceNum = Older.ActorStates[0].SequenceNum;
    Result.Position = FMath::Lerp(Older.ActorStates[0].Position, Newer.ActorStates[0].Position, Alpha);
    Result.Rotation = FMath::Lerp(Older.ActorStates[0].Rotation, Newer.ActorStates[0].Rotation, Alpha);
    Result.Velocity = FMath::Lerp(Older.ActorStates[0].Velocity, Newer.ActorStates[0].Velocity, Alpha);
    Result.Timestamp = FMath::Lerp(Older.ServerTime, Newer.ServerTime, Alpha);
    Result.InputFlags = Older.ActorStates[0].InputFlags;
    return Result;
}

void UNetworkTransportOptimizer::ApplyRollback(int32 RollbackFrames)
{
    if (bIsServer) return;
    if (PredictedStates.Num() == 0) return;

    int32 TargetIdx = FMath::Max(0, PredictedStates.Num() - 1 - RollbackFrames);
    // 截断到回滚点
    PredictedStates.SetNum(TargetIdx + 1);

    Stats.PredictedRollbacks++;
    OnRollbackOccurred.Broadcast(RollbackFrames);

    LogTransportEvent(FString::Printf(TEXT("Rollback %d frames"), RollbackFrames));
}

bool UNetworkTransportOptimizer::DequeueRollback(int32& OutRollbackFrames)
{
    // 简化：每次 Tick 检查是否需要回滚
    // 实际应由服务器校正包触发
    if (Stats.PredictedRollbacks > 0)
    {
        OutRollbackFrames = 1;  // 默认回滚 1 帧
        return true;
    }
    return false;
}

// ═══════════════════════════════════════
//  延迟补偿
// ═══════════════════════════════════════

void UNetworkTransportOptimizer::RewindWorld(float ClientTime, TFunction<void()> HitDetection)
{
    if (!bIsServer) return;

    // 找到对应时间的快照
    const FSnapshot* TargetSnap = nullptr;
    for (int32 i = ServerSnapshotHistory.Num() - 1; i >= 0; i--)
    {
        if (ServerSnapshotHistory[i].ServerTime <= ClientTime)
        {
            TargetSnap = &ServerSnapshotHistory[i];
            break;
        }
    }

    if (TargetSnap)
    {
        // 保存当前状态
        TArray<FSnapshot> CurrentState = ServerSnapshotHistory;
        // 回滚到目标快照
        // (实际实现应备份各 Actor 状态后恢复)
        // 这里调用外部命中检测
        if (HitDetection) HitDetection();
        // 恢复
        ServerSnapshotHistory = CurrentState;
    }
}

float UNetworkTransportOptimizer::GetEstimatedServerTime() const
{
    if (bIsServer) return ServerTime;
    // 客户端估计：本地时间 + 当前 RTT/2
    return FPlatformTime::Seconds() + (Stats.AvgRTT / 2000.f);  // RTT ms → 秒 / 2
}

// ═══════════════════════════════════════
//  MTU 探测
// ═══════════════════════════════════════

void UNetworkTransportOptimizer::StartMTUProbe()
{
    bMTUProbing = true;
    MTUProbeSize = 576;  // 起始探测值
    LastMTUProbeTime = FPlatformTime::Seconds();

    LogTransportEvent(FString::Printf(TEXT("MTU probe started at %d"), MTUProbeSize));
}

void UNetworkTransportOptimizer::UpdateMTUProbe(float DeltaTime)
{
    if (!bMTUProbing) return;

    float Now = FPlatformTime::Seconds();
    if ((Now - LastMTUProbeTime) < 1.0f) return;  // 每秒探测一次

    // 发送探测包
    FNetMessage Probe;
    Probe.Header.Magic = 0x5354;
    Probe.Header.Channel = (uint8)ENetChannel::Heartbeat;
    Probe.Header.Sequence = NextSendSeq;
    NextSendSeq = IncrementSeq(NextSendSeq);
    Probe.Header.SetReliable();

    // 填充到 MTU 大小
    Probe.Payload.SetNumUninitialized(MTUProbeSize);
    for (int32 i = 0; i < MTUProbeSize; i++) Probe.Payload[i] = (uint8)(i & 0xFF);
    Probe.Header.PayloadSize = MTUProbeSize;
    Probe.Header.Checksum = FCrc::MemCrc16(Probe.Payload.GetData(), MTUProbeSize);

    SendQueue.Insert(Probe, 0);

    // 升级探测大小
    static const int32 MTUSteps[] = {576, 1024, 1200, 1400, 1472, 1500};
    static int32 StepIdx = 0;
    StepIdx++;
    if (StepIdx < UE_ARRAY_COUNT(MTUSteps))
    {
        MTUProbeSize = MTUSteps[StepIdx];
    }
    else
    {
        // 探测完成，使用最大成功值
        bMTUProbing = false;
        // 如果没收到 ACK 确认，保守用 1200
        if (OptimalMTU == 1200 && StepIdx > 2)
        {
            OptimalMTU = MTUSteps[StepIdx - 1];
        }
        LogTransportEvent(FString::Printf(TEXT("MTU probe complete: optimal=%d"), OptimalMTU));
    }

    LastMTUProbeTime = Now;
}

// ═══════════════════════════════════════
//  拥塞控制（简化 BBR）
// ═══════════════════════════════════════

void UNetworkTransportOptimizer::UpdateCongestion(float DeltaTime, bool bPacketLost)
{
    // 更新 RTT 统计
    // 简化：用固定估计
    float EstRTT = 80.f;  // 实际应从 ACK 计算

    Congestion.SmoothedRTT = Congestion.SmoothedRTT * 0.875f + EstRTT * 0.125f;
    Congestion.RTTVar = Congestion.RTTVar * 0.75f + FMath::Abs(EstRTT - Congestion.SmoothedRTT) * 0.25f;

    if (bPacketLost)
    {
        // 丢包 → 进入拥塞避免
        Congestion.InSlowStart = false;
        Congestion.Ssthresh = FMath::Max(Congestion.CWnd / 2, 4);
        Congestion.CWnd = Congestion.Ssthresh;
        Congestion.LastLossTime = FPlatformTime::Seconds();
        OnCongestionDetected.Broadcast(Stats.LossRate);
    }
    else
    {
        if (Congestion.InSlowStart)
        {
            // 慢启动：指数增长
            Congestion.CWnd += 1;
            if (Congestion.CWnd >= Congestion.Ssthresh)
            {
                Congestion.InSlowStart = false;
            }
        }
        else
        {
            // 拥塞避免：线性增长
            Congestion.CWnd += 1.0f / Congestion.CWnd;
        }
    }

    // 更新瓶颈带宽估计
    if (DeltaTime > 0)
    {
        float Delivered = Stats.BytesSent - Congestion.BytesInFlight;
        float Bps = (Delivered / DeltaTime) / 1024.f;  // KB/s
        Congestion.BottleneckBW = Congestion.BottleneckBW * 0.9f + Bps * 0.1f;
    }
}

void UNetworkTransportOptimizer::CheckRetransmits(float DeltaTime)
{
    float Now = FPlatformTime::Seconds();
    TArray<int32> ToRemove;

    for (int32 i = 0; i < ReliableQueue.Num(); i++)
    {
        FNetMessage& Msg = ReliableQueue[i];
        // 简化：用序列号差估算时间
        // 实际应记录发送时间戳
        uint16 SeqDiff = (uint16)(NextSendSeq - Msg.Header.Sequence);
        float EstAge = SeqDiff * 0.05f;  // 假设每包 50ms

        if (EstAge > RetransmitTimeout)
        {
            if (Msg.Header.Ack == 0xFFFF || Msg.Header.Ack < MaxRetransmits)
            {
                // 重传
                Msg.Header.Ack++;  // 用 Ack 字段计数重传次数
                SendQueue.Insert(Msg, 0);  // 优先重传
                Stats.PacketsRetrans++;
                LogTransportEvent(FString::Printf(TEXT("Retransmit seq=%u (attempt %u)"),
                              Msg.Header.Sequence, Msg.Header.Ack), true);
            }
            else
            {
                // 超过最大重传次数 → 断开
                LogTransportEvent(TEXT("Max retransmits exceeded, disconnecting"), true);
                SetConnectionState(EConnectionState::Disconnected);
                break;
            }
        }
    }
}

// ═══════════════════════════════════════
//  抖动缓冲
// ═══════════════════════════════════════

void UNetworkTransportOptimizer::UpdateJitterBuffer(float DeltaTime)
{
    if (JitterBuffer.Num() == 0) return;

    JitterAccumulator += DeltaTime;

    // 自适应抖动缓冲大小
    if (bAdaptiveJitter)
    {
        float TargetJitter = FMath::Clamp(Stats.Jitter / 1000.f * 2.f, JitterBufferMin, JitterBufferMax);
        JitterBufferSize = FMath::Lerp(JitterBufferSize, TargetJitter, 0.1f);
    }

    if (JitterAccumulator >= JitterBufferSize)
    {
        // 释放缓冲的消息
        for (auto& Msg : JitterBuffer)
        {
            DeliverQueue.Enqueue(Msg);
            OnMessageReceived.Broadcast(Msg);
        }
        JitterBuffer.Empty();
        JitterAccumulator = 0.f;
    }
}

// ═══════════════════════════════════════
//  心跳
// ═══════════════════════════════════════

void UNetworkTransportOptimizer::UpdateHeartbeat(float DeltaTime)
{
    float Now = FPlatformTime::Seconds();

    if ((Now - LastHeartbeatTime) >= HeartbeatInterval)
    {
        FNetMessage HB;
        HB.Header.Magic = 0x5354;
        HB.Header.Channel = (uint8)ENetChannel::Heartbeat;
        HB.Header.Sequence = NextSendSeq;
        NextSendSeq = IncrementSeq(NextSendSeq);
        HB.Header.SetReliable();

        // 心跳负载：本地时间戳 + 当前 RTT
        HB.Payload.SetNumUninitialized(8);
        double LocalTime = FPlatformTime::Seconds();
        memcpy(HB.Payload.GetData(), &LocalTime, 4);
        float RTTms = Stats.AvgRTT;
        memcpy(HB.Payload.GetData() + 4, &RTTms, 4);

        SendQueue.Insert(HB, 0);  // 心跳优先
        LastHeartbeatTime = Now;
    }
}

// ═══════════════════════════════════════
//  ACK 管理
// ═══════════════════════════════════════

void UNetworkTransportOptimizer::SendAck(uint16 AckSeq)
{
    FNetAckPacket Ack;
    Ack.BaseSequence = AckSeq;
    Ack.AckBitfield = 0;
    // 设置位图（简化：只 ACK 当前序列）
    Ack.RTT = (int32)Stats.AvgRTT;
    Ack.Jitter = (int32)Stats.Jitter;

    FNetMessage AckMsg;
    AckMsg.Header.Magic = 0x5354;
    AckMsg.Header.Channel = (uint8)ENetChannel::Heartbeat;
    AckMsg.Header.SetAck();
    AckMsg.Header.Sequence = NextSendSeq;
    NextSendSeq = IncrementSeq(NextSendSeq);
    AckMsg.Payload.SetNumUninitialized(12);
    memcpy(AckMsg.Payload.GetData(), &Ack.BaseSequence, 2);
    memcpy(AckMsg.Payload.GetData() + 2, &Ack.AckBitfield, 4);
    memcpy(AckMsg.Payload.GetData() + 6, &Ack.RTT, 4);
    memcpy(AckMsg.Payload.GetData() + 10, &Ack.Jitter, 2);

    // ACK 包不可靠发送（节省带宽）
    SendQueue.Insert(AckMsg, 0);
}

void UNetworkTransportOptimizer::ProcessAckPacket(const FNetAckPacket& Ack)
{
    // 从可靠队列移除已确认的消息
    uint16 AckSeq = Ack.BaseSequence;
    for (int32 i = ReliableQueue.Num() - 1; i >= 0; i--)
    {
        if (ReliableQueue[i].Header.Sequence == AckSeq)
        {
            ReliableQueue.RemoveAt(i);
            LastAckedSeq = AckSeq;
            break;
        }
    }

    // 更新 RTT
    if (Ack.RTT > 0)
    {
        Stats.AvgRTT = Stats.AvgRTT * 0.8f + (float)Ack.RTT * 0.2f;
    }
    if (Ack.Jitter > 0)
    {
        Stats.Jitter = Stats.Jitter * 0.8f + (float)Ack.Jitter * 0.2f;
    }

    // 更新拥塞（无丢包）
    UpdateCongestion(0.05f, false);
}

// ═══════════════════════════════════════
//  分片 / 重组
// ═══════════════════════════════════════

void UNetworkTransportOptimizer::FragmentMessage(const FNetMessage& Msg, TArray<FNetMessage>& OutFrags)
{
    int32 TotalPayload = Msg.Payload.Num();
    int32 FragPayloadSize = OptimalMTU - sizeof(FNetMessageHeader) - 8;  // 预留分片头
    FragPayloadSize = FMath::Max(FragPayloadSize, 256);
    int32 TotalFrags = (TotalPayload + FragPayloadSize - 1) / FragPayloadSize;

    uint32 MsgId = GenerateMessageId();

    for (int32 i = 0; i < TotalFrags; i++)
    {
        FNetMessage Frag;
        Frag.Header.Magic = 0x5354;
        Frag.Header.Channel = Msg.Header.Channel;
        Frag.Header.Priority = Msg.Header.Priority;
        Frag.Header.Sequence = NextSendSeq;
        NextSendSeq = IncrementSeq(NextSendSeq);
        Frag.Header.SetFragment();
        if (Msg.Header.IsReliable()) Frag.Header.SetReliable();

        int32 Offset = i * FragPayloadSize;
        int32 ThisSize = FMath::Min(FragPayloadSize, TotalPayload - Offset);

        // 分片头：[4字节MsgId][2字节TotalFrags][2字节FragIndex]
        Frag.Payload.SetNumUninitialized(8 + ThisSize);
        memcpy(Frag.Payload.GetData(), &MsgId, 4);
        uint16 TF = (uint16)TotalFrags;
        uint16 FI = (uint16)i;
        memcpy(Frag.Payload.GetData() + 4, &TF, 2);
        memcpy(Frag.Payload.GetData() + 6, &FI, 2);
        memcpy(Frag.Payload.GetData() + 8, Msg.Payload.GetData() + Offset, ThisSize);

        Frag.Header.PayloadSize = Frag.Payload.Num();
        Frag.Header.Checksum = FCrc::MemCrc16(Frag.Payload.GetData(), Frag.Payload.Num());

        OutFrags.Add(Frag);
    }
}

bool UNetworkTransportOptimizer::ReassembleFragment(const FNetMessage& FragMsg, FNetMessage& OutComplete)
{
    if (FragMsg.Payload.Num() < 8) return false;

    uint32 MsgId;
    uint16 TotalFrags, FragIndex;
    memcpy(&MsgId, FragMsg.Payload.GetData(), 4);
    memcpy(&TotalFrags, FragMsg.Payload.GetData() + 4, 2);
    memcpy(&FragIndex, FragMsg.Payload.GetData() + 6, 2);

    // 存储分片
    if (!FragmentAssembly.Contains(MsgId))
    {
        FragmentAssembly.Add(MsgId, TArray<FFragmentInfo>());
        FragmentTimers.Add(MsgId, FPlatformTime::Seconds());
    }

    FFragmentInfo Info;
    Info.MessageId = MsgId;
    Info.TotalFrags = TotalFrags;
    Info.FragIndex = FragIndex;
    Info.FragSize = FragMsg.Payload.Num() - 8;
    Info.Data.SetNumUninitialized(Info.FragSize);
    memcpy(Info.Data.GetData(), FragMsg.Payload.GetData() + 8, Info.FragSize);
    FragmentAssembly[MsgId].Add(Info);

    // 检查是否收齐
    if (FragmentAssembly[MsgId].Num() >= TotalFrags)
    {
        // 按索引排序
        FragmentAssembly[MsgId].Sort([](const FFragmentInfo& A, const FFragmentInfo& B) {
            return A.FragIndex < B.FragIndex;
        });

        // 拼接
        int32 TotalSize = 0;
        for (const auto& F : FragmentAssembly[MsgId]) TotalSize += F.FragSize;

        OutComplete.Header = FragMsg.Header;
        OutComplete.Header.SetFragment();  // 清除分片标志
        OutComplete.Payload.SetNumUninitialized(TotalSize);

        int32 Offset = 0;
        for (const auto& F : FragmentAssembly[MsgId])
        {
            memcpy(OutComplete.Payload.GetData() + Offset, F.Data.GetData(), F.FragSize);
            Offset += F.FragSize;
        }

        OutComplete.Header.PayloadSize = TotalSize;
        OutComplete.Header.Checksum = FCrc::MemCrc16(OutComplete.Payload.GetData(), TotalSize);

        // 清理
        FragmentAssembly.Remove(MsgId);
        FragmentTimers.Remove(MsgId);
        Stats.FragmentsReassembled++;

        return true;
    }

    return false;
}

void UNetworkTransportOptimizer::CleanupOldFragments(float CurrentTime)
{
    TArray<uint32> ToRemove;
    for (auto& Pair : FragmentTimers)
    {
        if ((CurrentTime - Pair.Value) > FragReassemblyTimeout)
        {
            ToRemove.Add(Pair.Key);
        }
    }
    for (uint32 Key : ToRemove)
    {
        FragmentAssembly.Remove(Key);
        FragmentTimers.Remove(Key);
        LogTransportEvent(FString::Printf(TEXT("Fragment assembly timeout: MsgId=%u"), Key), true);
    }
}

// ═══════════════════════════════════════
//  压缩
// ═══════════════════════════════════════

bool UNetworkTransportOptimizer::CompressPayload(TArray<uint8>& Data, ECompressionAlgo Algo)
{
    if (Data.Num() < CompressionThreshold) return false;

    // 【Fix 6】高带宽时跳过压缩：压缩本身有 CPU 开销，
    // 当带宽充裕时，省下的 CPU 时间比省下的字节更有价值
    float CurrentBW = Stats.BWOutgoing; // KB/s
    if (CurrentBW > 500.f && Algo != ECompressionAlgo::BitPacked)
    {
        // 带宽 > 500KB/s 且不是 bool 位压缩 → 跳过
        return false;
    }

    TArray<uint8> Compressed;
    int32 CompressedSize = Data.Num();
    Compressed.SetNumUninitialized(CompressedSize);

    bool bCompressed = false;

    switch (Algo)
    {
    case ECompressionAlgo::LZ4:
        // UE 内置 LZ4
        if (FCompression::CompressMemory(ECompressionFlags::COMPRESS_LZ4,
            Compressed.GetData(), CompressedSize,
            Data.GetData(), Data.Num()))
        {
            if (CompressedSize < Data.Num())
            {
                Data = Compressed;
                Data.SetNum(CompressedSize);
                bCompressed = true;
            }
        }
        break;

    case ECompressionAlgo::BitPacked:
    {
        // 位压缩：针对 bool 数组和枚举
        // 每 8 个 bool → 1 字节
        int32 BoolCount = Data.Num();
        int32 ByteCount = (BoolCount + 7) / 8;
        Compressed.SetNumUninitialized(ByteCount + 4);
        // 写入原始大小
        memcpy(Compressed.GetData(), &BoolCount, 4);
        for (int32 i = 0; i < BoolCount; i++)
        {
            if (Data[i] != 0)
            {
                Compressed[4 + i / 8] |= (1 << (i % 8));
            }
        }
        Data = Compressed;
        bCompressed = true;
        break;
    }

    case ECompressionAlgo::Delta:
    {
        // 增量编码：与前一帧 XOR
        // 简化实现
        if (Data.Num() >= 4)
        {
            for (int32 i = Data.Num() - 1; i >= 4; i--)
            {
                Data[i] ^= Data[i - 4];
            }
            bCompressed = true;
        }
        break;
    }

    default:
        // ZLib/Snappy/Oodle 需要第三方库
        // 回退到 LZ4
        if (Algo != ECompressionAlgo::None)
        {
            return CompressPayload(Data, ECompressionAlgo::LZ4);
        }
        break;
    }

    return bCompressed;
}

bool UNetworkTransportOptimizer::DecompressPayload(TArray<uint8>& Data, ECompressionAlgo Algo)
{
    // 对应 CompressPayload 的解压
    switch (Algo)
    {
    case ECompressionAlgo::LZ4:
    {
        // 需要先知道解压后大小（通常存在包头部，这里用估计）
        int32 UncompressedSize = Data.Num() * 3;  // 估计膨胀比
        TArray<uint8> Decompressed;
        Decompressed.SetNumUninitialized(UncompressedSize);
        if (FCompression::UncompressMemory(ECompressionFlags::COMPRESS_LZ4,
            Decompressed.GetData(), UncompressedSize,
            Data.GetData(), Data.Num()))
        {
            Data = Decompressed;
            return true;
        }
        break;
    }
    case ECompressionAlgo::BitPacked:
    {
        if (Data.Num() < 4) return false;
        int32 BoolCount;
        memcpy(&BoolCount, Data.GetData(), 4);
        TArray<uint8> Decompressed;
        Decompressed.SetNumUninitialized(BoolCount);
        for (int32 i = 0; i < BoolCount; i++)
        {
            Decompressed[i] = (Data[4 + i / 8] & (1 << (i % 8))) ? 1 : 0;
        }
        Data = Decompressed;
        return true;
    }
    case ECompressionAlgo::Delta:
    {
        // 反向 XOR
        for (int32 i = 4; i < Data.Num(); i++)
        {
            Data[i] ^= Data[i - 4];
        }
        return true;
    }
    default:
        break;
    }
    return false;
}

TArray<uint8> UNetworkTransportOptimizer::DeltaEncode(const TArray<uint8>& Current,
                                                      const TArray<uint8>& Previous)
{
    int32 MinLen = FMath::Min(Current.Num(), Previous.Num());
    TArray<uint8> Delta;
    Delta.SetNumUninitialized(MinLen + 4);
    memcpy(Delta.GetData(), &MinLen, 4);
    for (int32 i = 0; i < MinLen; i++)
    {
        Delta[4 + i] = Current[i] ^ Previous[i];
    }
    return Delta;
}

TArray<uint8> UNetworkTransportOptimizer::DeltaDecode(const TArray<uint8>& Delta,
                                                      const TArray<uint8>& Base)
{
    int32 MinLen;
    memcpy(&MinLen, Delta.GetData(), 4);
    TArray<uint8> Result;
    Result.SetNumUninitialized(FMath::Max(MinLen, Base.Num()));
    for (int32 i = 0; i < Result.Num(); i++)
    {
        if (i < MinLen && i < Base.Num())
            Result[i] = Delta[4 + i] ^ Base[i];
        else if (i < Base.Num())
            Result[i] = Base[i];
    }
    return Result;
}

// ═══════════════════════════════════════
//  带宽限制
// ═══════════════════════════════════════

void UNetworkTransportOptimizer::EnforceBandwidthLimit(float DeltaTime)
{
    if (BandwidthLimitKBps <= 0) return;

    float CurrentBPS = (Stats.BytesSent / 1024.f) / FMath::Max(DeltaTime, 0.001f);
    if (CurrentBPS > BandwidthLimitKBps)
    {
        // 丢弃低优先级包
        for (int32 i = SendQueue.Num() - 1; i >= 0; i--)
        {
            ENetPriority P = (ENetPriority)SendQueue[i].Header.Priority;
            if (P == ENetPriority::Low || P == ENetPriority::Background)
            {
                SendQueue.RemoveAt(i);
            }
        }
    }
}

// ═══════════════════════════════════════
//  统计
// ═══════════════════════════════════════

void UNetworkTransportOptimizer::UpdateStats(float DeltaTime)
{
    // 计算带宽
    static float BWAccum = 0;
    static float BWTime = 0;
    BWAccum += Stats.BytesSent;
    BWTime += DeltaTime;
    if (BWTime >= 1.0f)
    {
        Stats.BWOutgoing = BWAccum / BWTime / 1024.f;
        Stats.BWIncoming = Stats.BytesReceived / BWTime / 1024.f;
        Stats.PacketRateOut = Stats.PacketsSent / BWTime;
        Stats.PacketRateIn = Stats.PacketsReceived / BWTime;
        BWAccum = 0;
        BWTime = 0;
        Stats.BytesSent = 0;
        Stats.BytesReceived = 0;
        Stats.PacketsSent = 0;
        Stats.PacketsReceived = 0;
    }

    // 计算压缩比
    if (Stats.BytesUncompressed > 0)
    {
        Stats.CompressionRatio = (float)Stats.BytesCompressed / (float)Stats.BytesUncompressed;
    }

    // 计算丢包率
    int32 TotalSent = Stats.PacketsSent + Stats.PacketsRetrans;
    if (TotalSent > 0)
    {
        Stats.LossRate = (float)Stats.PacketsLost / (float)TotalSent;
    }

    // 更新队列大小
    Stats.SendQueueSize = SendQueue.Num();
    Stats.RecvQueueSize = RecvQueue.Num();
}

void UNetworkTransportOptimizer::ResetStats()
{
    FMemory::Memzero(&Stats, sizeof(FTransportStats));
    Stats.CompressionRatio = 1.f;
}

FString UNetworkTransportOptimizer::GetDiagnosticString() const
{
    return FString::Printf(TEXT(
        "===== Network Transport Diagnostic =====\n"
        "  State: %d\n"
        "  [Throughput]\n"
        "    Out: %.1f KB/s (%.0f pkt/s)\n"
        "    In:  %.1f KB/s (%.0f pkt/s)\n"
        "  [Quality]\n"
        "    RTT: %.1f ms\n"
        "    Jitter: %.1f ms\n"
        "    Loss: %.1f%%\n"
        "    Compression: %.1f%%\n"
        "  [Queues]\n"
        "    Send: %d  Recv: %d  Reliable: %d\n"
        "  [Fragments]\n"
        "    Sent: %d  Reassembled: %d  Pending: %d\n"
        "  [Congestion]\n"
        "    CWnd: %d  SST: %d  SlowStart: %s\n"
        "    Est BW: %.0f KB/s  MinRTT: %.0f ms\n"
        "  [Prediction]\n"
        "    States: %d  Snapshots: %d  Rollbacks: %d\n"
        "  [MTU]\n"
        "    Optimal: %d  Probing: %s\n"
        "===== End Diagnostic ====="),
        (int)ConnectionState,
        Stats.BWOutgoing, Stats.PacketRateOut,
        Stats.BWIncoming, Stats.PacketRateIn,
        Stats.AvgRTT, Stats.Jitter, Stats.LossRate * 100.f,
        (1.f - Stats.CompressionRatio) * 100.f,
        Stats.SendQueueSize, Stats.RecvQueueSize, ReliableQueue.Num(),
        Stats.FragmentsSent, Stats.FragmentsReassembled, FragmentAssembly.Num(),
        Congestion.CWnd, Congestion.Ssthresh, Congestion.InSlowStart ? TEXT("Yes") : TEXT("No"),
        Congestion.BottleneckBW, Congestion.MinRTT,
        PredictedStates.Num(), SnapshotBuffer.Num(), Stats.PredictedRollbacks,
        OptimalMTU, bMTUProbing ? TEXT("Yes") : TEXT("No")
    );
}

// ═══════════════════════════════════════
//  辅助
// ═══════════════════════════════════════

void UNetworkTransportOptimizer::SetConnectionState(EConnectionState NewState)
{
    if (ConnectionState != NewState)
    {
        ConnectionState = NewState;
        OnConnectionStateChanged.Broadcast(NewState);
    }
}

uint16 UNetworkTransportOptimizer::GenerateMessageId()
{
    static uint32 Counter = 0;
    return (uint16)(++Counter);
}

uint16 UNetworkTransportOptimizer::IncrementSeq(uint16 Seq) const
{
    return (Seq + 1) % MaxSequence;
}

bool UNetworkTransportOptimizer::IsSeqNewer(uint16 A, uint16 B) const
{
    // 处理回绕的序列号比较
    int32 Diff = (int32)A - (int32)B;
    if (Diff > MaxSequence / 2) Diff -= MaxSequence;
    if (Diff < -MaxSequence / 2) Diff += MaxSequence;
    return Diff > 0;
}

void UNetworkTransportOptimizer::LogTransportEvent(const FString& Event, bool bWarning)
{
    if (bWarning)
        UE_LOG(LogTemp, Warning, TEXT("[NetTransport] %s"), *Event);
    else
        UE_LOG(LogTemp, Log, TEXT("[NetTransport] %s"), *Event);
}

void UNetworkTransportOptimizer::BitPackBools(TArray<uint8>& Out, const TArray<bool>& Bools)
{
    int32 ByteCount = (Bools.Num() + 7) / 8;
    Out.SetNumUninitialized(ByteCount);
    FMemory::Memzero(Out.GetData(), ByteCount);
    for (int32 i = 0; i < Bools.Num(); i++)
    {
        if (Bools[i]) Out[i / 8] |= (1 << (i % 8));
    }
}

void UNetworkTransportOptimizer::BitUnpackBools(TArray<bool>& Out, const TArray<uint8>& In, int32 Count)
{
    Out.SetNumUninitialized(Count);
    for (int32 i = 0; i < Count; i++)
    {
        Out[i] = (In[i / 8] & (1 << (i % 8))) != 0;
    }
}
