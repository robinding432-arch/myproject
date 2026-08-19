# Network Transport Optimization — Design Document

## Overview

StellarSystem v6.8 introduces a custom network transport layer that sits **between** UE5's built-in replication system and the raw UDP socket. It provides:

- **Reliable UDP** with sequence numbers, ACKs, retransmission, and duplicate detection
- **Client-side prediction** with server reconciliation and automatic rollback
- **Redundant snapshots** for lossy networks (N copies sent, client uses first-arriving)
- **Delta + LZ4 + BitPack compression** (target: 60-80% bandwidth reduction)
- **Dynamic MTU probing** (auto-detect optimal packet size)
- **Priority queues** (Critical > High > Normal > Low > Background)
- **BBR-style congestion control** (slow start → congestion avoidance)
- **Jitter buffer** with adaptive sizing
- **Lag compensation** (server rewinds world for hit detection)
- **Bandwidth shaping** (per-client allocation)
- **Batch packing** (merge small packets into MTU-sized bundles)

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      Game Thread                            │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐   │
│  │  Game Logic  │  │  Pawn/AI    │  │  UI/HUD      │   │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘   │
│         │                   │                   │            │
│         ▼                   ▼                   ▼            │
│  ┌─────────────────────────────────────────────────────┐    │
│  │              UTransportBridge                      │    │
│  │  - Send/Receive API for Game Thread              │    │
│  │  - Drives UNetworkTransportOptimizer::Tick()     │    │
│  │  - Bridges GameThread ↔ Socket Threads           │    │
│  └──────────────────┬──────────────────────────────┘    │
│                     │                                    │
└─────────────────────┼────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────┐
│              UNetworkTransportOptimizer                    │
│  ┌────────────┐ ┌────────────┐ ┌──────────────────┐      │
│  │  Send Path │ │ Recv Path  │ │  Congestion Ctrl │      │
│  │  Compress  │ │ Decompress │ │  BBR Algorithm   │      │
│  │  Fragment  │ │ Reassemble │ │  CWnd / SST     │      │
│  │  Prioritize│ │ JitterBuf  │ │  RTT Estimation │      │
│  │  Queue     │ │ DeliverQ   │ │  Loss Detection │      │
│  └─────┬──────┘ └─────┬─────┘ └────────┬─────────┘     │
│        │                │                  │               │
│  ┌─────┴──────┐ ┌─────┴──────┐ ┌───────┴────────┐     │
│  │ Prediction  │ │  Snapshot  │ │  MTU Probe     │     │
│  │  - InputBuf │ │  - Redund  │ │  - Ping/Pong   │     │
│  │  - Rollback │ │  - Interp  │ │  - Path MTU    │     │
│  │  - Replay   │ │  - Smooth  │ │  - Frag Size   │     │
│  └────────────┘ └────────────┘ └────────────────┘      │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────┐
│              Socket Threads                                │
│  ┌────────────────┐  ┌────────────────┐                   │
│  │  Send Thread   │  │  Recv Thread   │                   │
│  │  - Non-blocking│  │  - Non-blocking│                   │
│  │  - Batch write │  │  - Batch read  │                   │
│  └────────┬───────┘  └────────┬───────┘                   │
│           │                     │                          │
└───────────┼─────────────────────┼──────────────────────────┘
            ▼                     ▼
       ┌─────────┐          ┌─────────┐
       │  UDP     │          │  UDP     │
       │  Socket  │          │  Socket  │
       └────┬─────┘          └────┬─────┘
            │                     │
            ▼                     ▼
       ════════════════════════════════════════
                    Network (Internet)
       ════════════════════════════════════════
```

## Message Format

```
┌─────────────────────────────────────────────────────┐
│  FNetMessageHeader (12 bytes, fixed)              │
├─────────────────────────────────────────────────────┤
│  uint16  Magic       = 0x5354  ("ST")            │
│  uint8   Channel     (ENetChannel)                │
│  uint8   Priority    (ENetPriority)               │
│  uint16  Sequence    (16-bit, wraps at 65535)     │
│  uint16  Ack         (highest contiguous seq rcvd)│
│  uint16  Flags       (Reliable|Frag|Compressed|Ack)│
│  uint16  PayloadSize                                │
│  uint16  Checksum    (CRC16 of payload)            │
├─────────────────────────────────────────────────────┤
│  Payload (variable, up to MTU - 12)              │
└─────────────────────────────────────────────────────┘
```

## Compression Pipeline

```
Raw Data → Delta Encode (if previous frame exists)
         → BitPack (if small types: bool/enum/uint8)
         → LZ4 Compress (if size > threshold)
         → Fragment (if size > MTU)
         → UDP Send
```

Expected compression ratios:
- Position/Rotation streams: **70-85%** (high redundancy)
- Inventory/equipment data: **40-60%**
- Chat/messages: **20-40%**
- Already compressed assets: **0-5%** (don't re-compress)

## Priority Queue Behavior

| Priority | Example Messages | Behavior |
|---|---|---|
| **Critical** | Anti-cheat validation, login auth | Always sent first, never dropped |
| **High** | Combat damage, hit detection, PvP position | Reliable + redundant |
| **Normal** | Movement, interactions, inventory updates | Reliable, standard priority |
| **Low** | Chat, emotes, cosmetic updates | Dropped under bandwidth pressure |
| **Background** | Telemetry, analytics, non-urgent sync | Last to send, heavily compressed |

## Congestion Control (Simplified BBR)

```
Phase 1: Slow Start
  CWnd += 1 per RTT
  Until CWnd >= SSTreshold (128 packets)

Phase 2: Congestion Avoidance
  CWnd += 1/CWnd per RTT (linear growth)

On Packet Loss:
  SSTreshold = max(CWnd/2, 4)
  CWnd = SSTreshold
  Exit slow start

RTT Estimation:
  SmoothedRTT = 0.875 * SRTT + 0.125 * SampleRTT
  RTTVar = 0.75 * RTTVar + 0.25 * |SampleRTT - SRTT|
```

## Client Prediction Flow

```
Client:
  1. Player presses W
  2. Immediately move locally (predict)
  3. Save input command + predicted state to history
  4. Send input command to server (unreliable, high priority)
  5. Continue predicting next frames

Server:
  1. Receive input command
  2. Apply to authoritative world state
  3. Send corrected position back (with sequence number)
  4. If client error > threshold → send correction

Client:
  1. Receive server correction
  2. Calculate error = |predicted - authoritative|
  3. If error < threshold → smooth lerp
  4. If error > threshold → ROLLBACK:
     a. Reset to server position
     b. Replay all unacknowledged inputs
     c. Broadcast OnRollback event
```

## Server-Side Multi-Client Management

```
UServerNetOptimizer
  ├── Per-client:
  │   ├── Independent sequence numbers
  │   ├── Independent congestion state
  │   ├── Independent reliable queue
  │   ├── Snapshot history (for lag comp)
  │   └── Bandwidth allocation
  │
  ├── Global:
  │   ├── Total bandwidth cap
  │   ├── Batch scheduler (round-robin + priority)
  │   ├── Connection timeout monitor
  │   └── Adaptive redundancy controller
  │
  └── Broadcast strategies:
      ├── All (chat, global events)
      ├── Nearby (positional audio, local effects)
      └── Relevance-filtered (spatial hashing)
```

## Performance Targets

| Metric | Target (v6.7) | Target (v6.8) |
|---|---|---|
| Client bandwidth (idle) | ~5 KB/s | **<2 KB/s** |
| Client bandwidth (combat) | ~50 KB/s | **<20 KB/s** |
| Server bandwidth (32 players) | ~800 KB/s | **<300 KB/s** |
| Position update frequency | 20 Hz | **10-30 Hz adaptive** |
| Prediction accuracy | ~85% | **>95%** |
| Rollback frequency | ~5/min | **<1/min** |
| Packet loss tolerance | 5% | **15%** |
| MTU efficiency | Fixed 1200 | **Dynamic 576-1500** |

## Configuration (Server.ini)

```ini
[/Script/StellarSystem.NetworkTransportOptimizer]
MaxPacketSize=1200
MaxRetransmits=8
RetransmitTimeout=0.5
DefaultCompression=LZ4
CompressionThreshold=64
bEnableDeltaCompression=true
bEnableBitPacking=true
FragmentSize=1024
SnapshotRedundancy=3
InterpBackTime=0.1
JitterBufferMin=0.02
JitterBufferMax=0.15
bAdaptiveJitter=true
BandwidthLimitKBps=512
HeartbeatInterval=2.0
MaxMissedHeartbeats=5

[/Script/StellarSystem.TransportBridge]
RemoteIP=127.0.0.1
RemotePort=7777
SocketSendBufferSize=262144
SocketRecvBufferSize=1048576
bUseThreadedSend=true
bUseThreadedRecv=true
LogLevel=Warnings

[/Script/StellarSystem.ServerNetOptimizer]
MaxClients=64
ServerTickRate=30
MaxBandwidthKBps=8192
BatchSize=8
ClientTimeout=30
bEnableAdaptiveRedundancy=true
bEnableBatching=true
HighLossThreshold=0.15
```

## Integration with Existing Systems

| Existing System | Integration Point |
|---|---|
| `StellarDedicatedServer` | Uses `UServerNetOptimizer` for all client communication |
| `StellarClientGameMode` | Uses `UTransportBridge` → `UNetworkTransportOptimizer` |
| `ShipPawn` | `UClientPredictionComponent` for movement prediction |
| `PvPSystem` | Server-side lag compensation via `RewindWorld()` |
| `AntiCheatManager` | Validates all incoming commands before processing |
| `PerformanceManager` | Monitors transport stats, auto-adjusts quality |

## Known Limitations (v6.8)

1. **No actual Socket I/O in this layer** — The transport optimizer handles message-level logic; actual UDP send/recv should be wired in `TransportBridge` with proper socket management.
2. **Simplified BBR** — Full BBR v2 has more states (ProbeBW, ProbeRTT); this is a functional subset.
3. **No encryption** — Messages are plaintext UDP. For production, add DTLS or libsodium.
4. **No NAT traversal** — Assumes direct connectivity. Add STUN/TURN for peer-to-peer scenarios.
5. **Delta compression** is structurally present but needs a `PreviousFrame` cache per channel for full effectiveness.

## Next Steps (v6.9+)

- [ ] Wire actual `FSocket` send/recv into `TransportBridge`
- [ ] Add DTLS encryption layer
- [ ] Implement full BBR v2 state machine
- [ ] Add per-channel `PreviousFrame` cache for delta compression
- [ ] Implement spatial hashing for relevance filtering
- [ ] Add WebRTC data channel as alternative transport
- [ ] Implement QUIC protocol support
- [ ] Add network simulation tools (latency injection, packet loss, reordering)
