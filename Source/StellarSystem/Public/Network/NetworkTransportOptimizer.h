// NetworkTransportOptimizer.h
// StellarSystem v6.8 — 客户端/服务器传输优化核心
// 功能：可靠UDP + 预测回滚 + 冗余快照 + 自适应MTU + 增量压缩
//       + 分片重组 + 优先级队列 + 延迟补偿 + 抗抖动缓冲

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "NetworkTransportOptimizer.generated.h"

// ─────────────────────────────────────────────
//  枚举
// ─────────────────────────────────────────────

/** 传输通道类型 */
UENUM(BlueprintType)
enum class ENetChannel : uint8
{
    ReliableOrdered,    // 可靠有序（登录/交易/任务）
    ReliableUnordered,  // 可靠无序（资源加载/配置）
    Unreliable,         // 不可靠（位置/旋转高频更新）
    UnreliableSequenced,// 不可靠有序（动画/音效事件）
    Fragmented,         // 分片通道（大包拆发）
    Priority,           // 优先级队列通道
    Heartbeat,          // 心跳专用
    MAX
};

/** 消息优先级 */
UENUM(BlueprintType)
enum class ENetPriority : uint8
{
    Critical,    // 反作弊/登录验证
    High,        // 战斗/伤害/PvP
    Normal,      // 移动/交互
    Low,         // 聊天/表情
    Background,  // 统计/遥测
    MAX
};

/** 压缩算法 */
UENUM(BlueprintType)
enum class ECompressionAlgo : uint8
{
    None,
    ZLib,
    LZ4,
    Oodle,       // 如果可用
    Snappy,
    Delta,       // 增量（与上帧 diff）
    BitPacked,   // 位压缩（bool/枚举/小整数）
    MAX
};

/** 连接状态 */
UENUM(BlueprintType)
enum class EConnectionState : uint8
{
    Disconnected,
    Connecting,
    Handshaking,
    Authenticated,
    Connected,
    Reconnecting,
    Congested,    // 拥塞
    MAX
};

// ─────────────────────────────────────────────
//  数据结构
// ─────────────────────────────────────────────

/** 单条网络消息头（固定 12 字节） */
USTRUCT(BlueprintType)
struct FNetMessageHeader
{
    GENERATED_BODY()

    UPROPERTY() uint16  Magic      = 0x5354;  // "ST" 标识
    UPROPERTY() uint8   Channel    = 0;        // ENetChannel
    UPROPERTY() uint8   Priority   = 0;        // ENetPriority
    UPROPERTY() uint16  Sequence   = 0;        // 序列号
    UPROPERTY() uint16  Ack        = 0;        // 确认号
    UPROPERTY() uint16  Flags      = 0;        // 见 FNetFlags
    UPROPERTY() uint16  PayloadSize = 0;       // 负载大小
    UPROPERTY() uint16  Checksum   = 0;        // CRC16

    bool IsReliable() const   { return (Flags & 0x01) != 0; }
    bool IsFragment() const   { return (Flags & 0x02) != 0; }
    bool IsCompressed() const { return (Flags & 0x04) != 0; }
    bool IsAck() const       { return (Flags & 0x08) != 0; }
    void SetReliable()       { Flags |= 0x01; }
    void SetFragment()       { Flags |= 0x02; }
    void SetCompressed()     { Flags |= 0x04; }
    void SetAck()            { Flags |= 0x08; }
};

/** 网络消息完整包 */
USTRUCT(BlueprintType)
struct FNetMessage
{
    GENERATED_BODY()

    UPROPERTY() FNetMessageHeader Header;
    UPROPERTY() TArray<uint8>     Payload;

    int32 TotalSize() const { return sizeof(FNetMessageHeader) + Payload.Num(); }
};

/** 分片信息 */
USTRUCT(BlueprintType)
struct FFragmentInfo
{
    GENERATED_BODY()

    UPROPERTY() uint32 MessageId   = 0;
    UPROPERTY() uint16 TotalFrags = 0;
    UPROPERTY() uint16 FragIndex  = 0;
    UPROPERTY() uint16 FragSize   = 0;
    UPROPERTY() TArray<uint8> Data;
};

/** 确认包（批量 ACK） */
USTRUCT(BlueprintType)
struct FNetAckPacket
{
    GENERATED_BODY()

    UPROPERTY() uint16          BaseSequence = 0;
    UPROPERTY() uint32          AckBitfield  = 0;  // 32 个序列号的位图
    UPROPERTY() TArray<uint16>  SelectiveAcks;       // 超出窗口的选择性 ACK
    UPROPERTY() int32           RTT          = 0;    // 报告 RTT
    UPROPERTY() int32           Jitter      = 0;     // 报告抖动
};

/** 客户端预测状态（用于回滚） */
USTRUCT(BlueprintType)
struct FPredictedState
{
    GENERATED_BODY()

    UPROPERTY() uint16      SequenceNum = 0;
    UPROPERTY() FVector     Position;
    UPROPERTY() FRotator    Rotation;
    UPROPERTY() FVector     Velocity;
    UPROPERTY() float       Timestamp   = 0.f;
    UPROPERTY() uint8       InputFlags  = 0;  // 按键位
};

/** 冗余快照 */
USTRUCT(BlueprintType)
struct FSnapshot
{
    GENERATED_BODY()

    UPROPERTY() uint16      Seq         = 0;
    UPROPERTY() float       ServerTime  = 0.f;
    UPROPERTY() TArray<FPredictedState> ActorStates;  // 多个 Actor 的状态
    UPROPERTY() uint8       Redundancy  = 2;   // 冗余份数
};

/** 拥塞控制状态（简化版 BBR） */
USTRUCT(BlueprintType)
struct FCongestionState
{
    GENERATED_BODY()

    UPROPERTY() float BottleneckBW  = 1000.f;  // 估算瓶颈带宽 KB/s
    UPROPERTY() float MinRTT        = 50.f;     // 最小 RTT ms
    UPROPERTY() float SmoothedRTT   = 80.f;
    UPROPERTY() float RTTVar        = 10.f;
    UPROPERTY() int32  CWnd         = 64;       // 拥塞窗口（包数）
    UPROPERTY() int32  Ssthresh     = 128;      // 慢启动阈值
    UPROPERTY() bool   InSlowStart  = true;
    UPROPERTY() int32  BytesInFlight = 0;
    UPROPERTY() float  LastLossTime  = 0.f;
};

/** 传输统计 */
USTRUCT(BlueprintType)
struct FTransportStats
{
    GENERATED_BODY()

    UPROPERTY() int64  BytesSent          = 0;
    UPROPERTY() int64  BytesReceived      = 0;
    UPROPERTY() int64  BytesCompressed   = 0;   // 压缩后总字节
    UPROPERTY() int64  BytesUncompressed = 0;   // 压缩前总字节
    UPROPERTY() float  CompressionRatio  = 1.f;
    UPROPERTY() int32  PacketsSent       = 0;
    UPROPERTY() int32  PacketsLost       = 0;
    UPROPERTY() int32  PacketsRetrans    = 0;
    UPROPERTY() int32  FragmentsSent     = 0;
    UPROPERTY() int32  FragmentsReassembled = 0;
    UPROPERTY() float  LossRate          = 0.f;
    UPROPERTY() float  AvgRTT            = 0.f;
    UPROPERTY() float  Jitter            = 0.f;
    UPROPERTY() int32  SendQueueSize     = 0;
    UPROPERTY() int32  RecvQueueSize     = 0;
    UPROPERTY() float  BWOutgoing        = 0.f;  // KB/s
    UPROPERTY() float  BWIncoming        = 0.f;
    UPROPERTY() float  PacketRateOut     = 0.f;  // pkt/s
    UPROPERTY() float  PacketRateIn      = 0.f;
    UPROPERTY() int32  PredictedRollbacks = 0;
    UPROPERTY() int32  SnapshotsSent    = 0;
};

// ─────────────────────────────────────────────
//  委托
// ─────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMessageReceived, const FNetMessage&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnConnectionStateChanged, EConnectionState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRollbackOccurred, int32, RollbackFrames);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCongestionDetected, float, LossRate);

// ─────────────────────────────────────────────
//  主类
// ─────────────────────────────────────────────

/**
 * UNetworkTransportOptimizer
 *
 * 核心职责：
 * 1. 可靠 UDP 传输（序列号 + ACK + 重传 + 重复检测）
 * 2. 客户端预测 + 服务器回滚（输入同步）
 * 3. 冗余快照（N 份冗余防丢包）
 * 4. 增量压缩（Delta + BitPack + LZ4）
 * 5. 动态 MTU 探测 + 分片重组
 * 6. 优先级队列（Critical > High > Normal > Low > Background）
 * 7. 拥塞控制（BBR 简化版）
 * 8. 抖动缓冲（Jitter Buffer）
 * 9. 延迟补偿（Lag Compensation）
 */
UCLASS(BlueprintType, Config=Network)
class STELLARSYSTEM_API UNetworkTransportOptimizer : public UObject
{
    GENERATED_BODY()

public:
    UNetworkTransportOptimizer();

    // ═══════════════════════════════════════
    //  配置参数（可在 Server.ini / 控制台调）
    // ═══════════════════════════════════════

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Transport|General")
    int32  MaxPacketSize      = 1200;   // 默认 MTU（不含 IP/UDP 头）

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Transport|General")
    int32  MaxRetransmits     = 8;

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Transport|General")
    float  RetransmitTimeout  = 0.5f;   // 初始 RTO（秒）

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Transport|General")
    float  ConnectTimeout     = 10.f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Transport|General")
    int32  MaxSequence        = 65536;  // 16-bit 回绕

    // 压缩
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Transport|Compression")
    ECompressionAlgo DefaultCompression = ECompressionAlgo::LZ4;

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Transport|Compression")
    int32  CompressionThreshold = 64;    // 小于此字节不压缩

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Transport|Compression")
    bool   bEnableDeltaCompression = true;

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Transport|Compression")
    bool   bEnableBitPacking      = true;

    // 分片
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Transport|Fragmentation")
    int32  FragmentSize       = 1024;   // 每片大小

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Transport|Fragmentation")
    float  FragReassemblyTimeout = 5.f;

    // 预测/回滚
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Transport|Prediction")
    int32  MaxPredictedStates = 128;

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Transport|Prediction")
    int32  SnapshotRedundancy = 3;

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Transport|Prediction")
    float  InterpBackTime     = 0.1f;   // 插值回退时间（秒）

    // 抖动缓冲
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Transport|Jitter")
    float  JitterBufferMin    = 0.02f;  // 20ms
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Transport|Jitter")
    float  JitterBufferMax    = 0.15f;  // 150ms
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Transport|Jitter")
    bool   bAdaptiveJitter    = true;

    // 带宽
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Transport|Bandwidth")
    float  BandwidthLimitKBps = 512.f;  // 0 = 无限制
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Transport|Bandwidth")
    float  BurstLimitKB       = 128.f;

    // 心跳
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Transport|Heartbeat")
    float  HeartbeatInterval  = 2.f;
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Transport|Heartbeat")
    int32  MaxMissedHeartbeats = 5;

    // ═══════════════════════════════════════
    //  生命周期
    // ═══════════════════════════════════════

    UFUNCTION(BlueprintCallable, Category="Transport")
    void Initialize(bool bIsServer, const FString& RemoteAddress = TEXT(""));

    UFUNCTION(BlueprintCallable, Category="Transport")
    void Shutdown();

    UFUNCTION(BlueprintCallable, Category="Transport")
    void Tick(float DeltaTime);

    // ═══════════════════════════════════════
    //  发送接口
    // ═══════════════════════════════════════

    /** 发送消息（自动选择可靠/不可靠/压缩/分片） */
    UFUNCTION(BlueprintCallable, Category="Transport|Send")
    bool SendMessage(ENetChannel Channel, ENetPriority Priority,
                    const TArray<uint8>& Payload, bool bReliable = true);

    /** 发送原始结构（自动序列化） */
    UFUNCTION(BlueprintCallable, Category="Transport|Send")
    bool SendStruct(ENetChannel Channel, ENetPriority Priority,
                    const FString& StructName, const TArray<uint8>& SerializedData);

    /** 客户端预测：发送输入命令 */
    UFUNCTION(BlueprintCallable, Category="Transport|Prediction")
    uint16 SendInputCommand(const FPredictedState& InputState);

    /** 批量发送（合并小包） */
    UFUNCTION(BlueprintCallable, Category="Transport|Send")
    void FlushBatch();

    // ═══════════════════════════════════════
    //  接收接口
    // ═══════════════════════════════════════

    /** 接收处理（每帧调用） */
    UFUNCTION(BlueprintCallable, Category="Transport|Receive")
    void ProcessIncoming(const TArray<uint8>& RawData, int32 DataSize);

    /** 获取下一个完整消息（如果有） */
    UFUNCTION(BlueprintCallable, Category="Transport|Receive")
    bool DequeueMessage(FNetMessage& OutMessage);

    /** 获取预测回滚事件 */
    UFUNCTION(BlueprintCallable, Category="Transport|Prediction")
    bool DequeueRollback(int32& OutRollbackFrames);

    // ═══════════════════════════════════════
    //  服务器→客户端快照
    // ═══════════════════════════════════════

    /** 服务器：生成冗余快照 */
    UFUNCTION(BlueprintCallable, Category="Transport|Snapshot")
    void SendSnapshot(const TArray<FPredictedState>& WorldState);

    /** 客户端：应用快照 + 插值 */
    UFUNCTION(BlueprintCallable, Category="Transport|Snapshot")
    void ApplySnapshot(const FSnapshot& Snapshot, float RenderTime);

    /** 获取当前插值后的状态 */
    UFUNCTION(BlueprintCallable, Category="Transport|Snapshot")
    bool GetInterpolatedState(FPredictedState& OutState, const FName& ActorName);

    // ═══════════════════════════════════════
    //  延迟补偿
    // ═══════════════════════════════════════

    /** 服务器：根据客户端延迟回滚世界状态做命中检测 */
    UFUNCTION(BlueprintCallable, Category="Transport|LagComp")
    void RewindWorld(float ClientTime, TFunction<void()> HitDetection);

    /** 客户端：获取当前服务器时间估计 */
    UFUNCTION(BlueprintCallable, Category="Transport|LagComp")
    float GetEstimatedServerTime() const;

    // ═══════════════════════════════════════
    //  MTU 探测
    // ═══════════════════════════════════════

    UFUNCTION(BlueprintCallable, Category="Transport|MTU")
    void StartMTUProbe();

    UFUNCTION(BlueprintCallable, Category="Transport|MTU")
    int32 GetOptimalMTU() const { return OptimalMTU; }

    // ═══════════════════════════════════════
    //  统计 & 诊断
    // ═══════════════════════════════════════

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="Transport|Stats")
    FTransportStats GetStats() const { return Stats; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="Transport|Stats")
    float GetCurrentLossRate() const { return Stats.LossRate; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="Transport|Stats")
    float GetCurrentRTT() const { return Stats.AvgRTT; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="Transport|Stats")
    float GetCompressionRatio() const { return Stats.CompressionRatio; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="Transport|Stats")
    int32 GetSendQueueLength() const { return SendQueue.Num(); }

    UFUNCTION(BlueprintCallable, Category="Transport|Stats")
    FString GetDiagnosticString() const;

    UFUNCTION(BlueprintCallable, Category="Transport|Stats")
    void ResetStats();

    // ═══════════════════════════════════════
    //  事件
    // ═══════════════════════════════════════

    UPROPERTY(BlueprintAssignable, Category="Transport|Events")
    FOnMessageReceived OnMessageReceived;

    UPROPERTY(BlueprintAssignable, Category="Transport|Events")
    FOnConnectionStateChanged OnConnectionStateChanged;

    UPROPERTY(BlueprintAssignable, Category="Transport|Events")
    FOnRollbackOccurred OnRollbackOccurred;

    UPROPERTY(BlueprintAssignable, Category="Transport|Events")
    FOnCongestionDetected OnCongestionDetected;

    // ═══════════════════════════════════════
    //  内部
    // ═══════════════════════════════════════

private:
    // 状态
    bool              bIsServer           = false;
    EConnectionState  ConnectionState    = EConnectionState::Disconnected;
    FString           RemoteAddr;

    // 序列号
    uint16            NextSendSeq        = 0;
    uint16            NextRecvSeq        = 0;
    uint16            LastAckedSeq       = 0;

    // 队列
    TArray<FNetMessage>   SendQueue;        // 按优先级排序
    TArray<FNetMessage>   ReliableQueue;    // 待确认可靠消息
    TArray<FNetMessage>   RecvQueue;        // 已排序接收队列
    TQueue<FNetMessage>   DeliverQueue;     // 交付给上层的队列

    // 分片重组
    TMap<uint32, TArray<FFragmentInfo>> FragmentAssembly;  // MessageId → 分片列表
    TMap<uint32, float>                 FragmentTimers;

    // 预测 & 快照
    TArray<FPredictedState> PredictedStates;   // 客户端：保存的预测历史
    TArray<FSnapshot>       SnapshotBuffer;    // 客户端：收到的快照
    TMap<FName, FPredictedState> InterpCache;  // 当前插值结果

    // 服务器快照历史（用于延迟补偿）
    TArray<FSnapshot>       ServerSnapshotHistory;
    float                   ServerTime       = 0.f;

    // 拥塞控制
    FCongestionState        Congestion;

    // 抖动缓冲
    TArray<FNetMessage>     JitterBuffer;
    float                   JitterBufferSize = 0.05f;
    float                   JitterAccumulator = 0.f;

    // 统计
    FTransportStats         Stats;
    float                   LastRTTSample    = 0.f;
    float                   RTTAccumulator   = 0.f;
    int32                   RTTSamples       = 0;

    // 心跳
    float                   LastHeartbeatTime = 0.f;
    int32                   MissedHeartbeats  = 0;

    // MTU
    int32                   OptimalMTU       = 1200;
    bool                    bMTUProbing     = false;
    int32                   MTUProbeSize    = 576;
    float                   LastMTUProbeTime = 0.f;

    // 批量缓冲
    TArray<FNetMessage>     BatchBuffer;
    float                   LastBatchFlush   = 0.f;

    // ACK 管理
    TArray<uint16>          PendingAcks;
    uint16                  AckBaseSeq      = 0;

    // 内部方法
    void                    SetConnectionState(EConnectionState NewState);
    bool                    CompressPayload(TArray<uint8>& Data, ECompressionAlgo Algo);
    bool                    DecompressPayload(TArray<uint8>& Data, ECompressionAlgo Algo);
    TArray<uint8>           DeltaEncode(const TArray<uint8>& Current, const TArray<uint8>& Previous);
    TArray<uint8>           DeltaDecode(const TArray<uint8>& Delta, const TArray<uint8>& Base);
    void                    FragmentMessage(const FNetMessage& Msg, TArray<FNetMessage>& OutFrags);
    bool                    ReassembleFragment(const FNetMessage& FragMsg, FNetMessage& OutComplete);
    void                    ProcessAckPacket(const FNetAckPacket& Ack);
    void                    SendAck(uint16 AckSeq);
    void                    CheckRetransmits(float DeltaTime);
    void                    UpdateCongestion(float DeltaTime, bool bPacketLost);
    void                    UpdateJitterBuffer(float DeltaTime);
    void                    UpdateHeartbeat(float DeltaTime);
    void                    UpdateMTUProbe(float DeltaTime);
    void                    SortSendQueueByPriority();
    void                    EnforceBandwidthLimit(float DeltaTime);
    FPredictedState         InterpolateState(const FSnapshot& Older, const FSnapshot& Newer, float Alpha);
    void                    ApplyRollback(int32 RollbackFrames);
    uint16                  GenerateMessageId();
    uint16                  IncrementSeq(uint16 Seq) const;
    bool                    IsSeqNewer(uint16 A, uint16 B) const;
    void                    CleanupOldFragments(float CurrentTime);
    void                    UpdateStats(float DeltaTime);
    void                    LogTransportEvent(const FString& Event, bool bWarning = false);

    // 位压缩辅助
    void                    BitPackBools(TArray<uint8>& Out, const TArray<bool>& Bools);
    void                    BitUnpackBools(TArray<bool>& Out, const TArray<uint8>& In, int32 Count);
};
