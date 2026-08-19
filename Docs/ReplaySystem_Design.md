# Replay / 录像回放系统设计文档

> 版本：v6.3 | 状态：设计稿

---

## 一、设计目标

1. **战斗回放**：PvP 击杀/被击杀自动保存，可随时回放
2. **观战模式**：死亡后进入观战，可看击杀者视角
3. **精彩集锦**：系统自动识别"三杀/爆头/跃迁击杀"等精彩时刻，生成短视频
4. **反作弊取证**：可疑对局自动保存完整录像，供管理员审查

---

## 二、技术选型

### 方案 A：UE DemoNetDriver（推荐 ✅）

```
原理：服务端录制所有 RPC + Actor 属性变化 → 客户端回放
优点：✅ 官方支持  ✅ 网络层天然兼容  ✅ 零额外代码
缺点：❌ 文件较大（每分钟 ~1-5MB）  ❌ 需要 Dedicated Server
```

### 方案 B：自定义事件流

```
原理：只记录关键事件（开火/命中/死亡/跃迁）
优点：✅ 文件极小  ✅ 可跨版本
缺点：❌ 需要自己实现回放渲染
```

**结论：用方案 A 做完整录像，方案 B 做精彩集锦摘要。**

---

## 三、系统架构

```
┌─────────────────────────────────────────────┐
│           AStellarGameMode                  │
│  ┌────────────┐  ┌──────────────────┐     │
│  │ ReplayManager│  │ HighlightDetector│     │
│  └─────┬──────┘  └────────┬─────────┘     │
│         │                  │                │
│  ┌─────▼──────┐  ┌──────▼──────────┐     │
│  │ DemoNetDriver│  │ EventStream      │     │
│  │ (完整录像)   │  │ (精彩事件摘要)   │     │
│  └─────┬──────┘  └──────┬──────────┘     │
│         │                  │                │
│  ┌─────▼──────────────────▼──────────┐     │
│  │         ReplayStorage              │     │
│  │  (本地 + Steam Cloud + 服务端)     │     │
│  └────────────────────────────────────┘     │
└─────────────────────────────────────────────┘
```

---

## 四、核心代码骨架

### 4.1 ReplayManager 组件

```cpp
// ReplayManager.h（待创建）
UCLASS()
class UReplayManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // 开始录制（进战斗时调用）
    UFUNCTION(BlueprintCallable)
    void StartRecording(const FString& SessionName);

    // 停止录制
    UFUNCTION(BlueprintCallable)
    void StopRecording();

    // 保存录像
    UFUNCTION(BlueprintCallable)
    void SaveReplay(const FString& SlotName);

    // 加载并播放录像
    UFUNCTION(BlueprintCallable)
    void PlayReplay(const FString& SlotName);

    // 获取本地录像列表
    UFUNCTION(BlueprintCallable)
    TArray<FString> GetLocalReplayList() const;

    // 删除录像
    UFUNCTION(BlueprintCallable)
    void DeleteReplay(const FString& SlotName);

    // 上传到服务端（用于反作弊审查）
    UFUNCTION(Server, Reliable)
    void UploadReplayToServer(const FString& SlotName);

    // 事件回调
    UPROPERTY(BlueprintAssignable)
    FOnReplaySaved OnReplaySaved;

    UPROPERTY(BlueprintAssignable)
    FOnReplayPlaybackEnded OnReplayEnded;

private:
    bool bIsRecording = false;
    FString CurrentReplayName;
    TArray<FReplayEvent> EventBuffer;
};
```

### 4.2 精彩时刻检测

```cpp
// HighlightDetector.h（待创建）
UCLASS()
class UHighlightDetector : public UObject
{
    GENERATED_BODY()

public:
    // 检测事件是否够"精彩"
    bool EvaluateEvent(const FReplayEvent& Event) const;

    // 自动生成集锦视频（需 RenderMovie 权限）
    void GenerateHighlightClip(const FString& ReplayName,
        const TArray<FReplayEvent>& Events);

    // 精彩类型
    enum class EHighlightType : uint8
    {
        MultiKill_3,      // 三杀
        MultiKill_5,      // 五杀
        Headshot,          // 爆头
        WarpKill,          // 跃迁中击杀
        NearDeathEscape,   // 丝血逃生
        SoloPirateKill,    // 单挑海盗
        CargoRaid,         // 成功劫掠
        PlanetDiscovery,   // 发现新星球
        FirstBlood,        // 首杀
        RevengeKill        // 复仇击杀
    };

private:
    // 阈值配置
    float MultiKillWindow = 10.f;  // 10 秒内多杀
    float NearDeathThreshold = 0.1f; // 血量 < 10%
};
```

### 4.3 录像事件结构

```cpp
USTRUCT()
struct FReplayEvent
{
    GENERATED_BODY()

    float Timestamp;          // 录像内时间
    EHighlightType Type;      // 事件类型
    FString ActorID;          // 主角
    FString TargetID;        // 目标
    FVector Location;         // 位置
    TMap<FString, float> Data; // 额外数据
};
```

---

## 五、DemoNetDriver 配置

### 5.1 DefaultEngine.ini 设置

```ini
[/Script/Engine.DemoNetDriver]
DemoNetDriverClass=Engine.DemoNetDriver
DemoSpectatorClass=Engine.DemoSpectator
MaxDesiredRecordTimeMS=30000
MinRecordTimeMS=10000
NetServerMaxTickRate=60
LagCompensationTickRate=60
```

### 5.2 启动参数

```
# 录制
-StellarSystem -replay -replayname="combat_20260115_001"

# 回放
-StellarSystem -playreplay="combat_20260115_001"
```

---

## 六、观战模式

### 6.1 死亡后自动观战

```cpp
// 在 ShipDamageSystem 的 OnShipDestroyed 里：
void AShipPawn::OnDeath()
{
    // 通知 GameMode
    AStellarGameMode* GM = GetWorld()->GetAuthGameMode<AStellarGameMode>();
    if (GM)
    {
        GM->EnterSpectatorMode(this, LastDamagedBy);
    }
}

// GameMode 里：
void AStellarGameMode::EnterSpectatorMode(APawn* DeadPawn, APawn* Killer)
{
    APlayerController* PC = Cast<APlayerController>(DeadPawn->GetController());
    if (!PC) return;

    // 切换到观战 Pawn
    ASpectatorPawn* Spectator = GetWorld()->SpawnActor<ASpectatorPawn>();
    PC->Possess(Spectator);

    // 跟随击杀者
    if (Killer)
    {
        Spectator->SetViewTarget(Killer);
    }
}
```

### 6.2 观战控制

| 按键 | 功能 |
|---|---|
| 鼠标 | 自由观察 |
| Tab | 切换观战目标 |
| 滚轮 | 缩放距离 |
| C | 切换视角（第一/第三/自由） |
| R | 跟随击杀者回放 |
| Esc | 退出观战 → 复活界面 |

---

## 七、存储策略

| 存储位置 | 用途 | 大小限制 |
|---|---|---|
| 本地 `Saved/Replays/` | 个人录像 | 无限制（用户管理） |
| Steam Cloud | 重要录像备份 | 100MB/用户 |
| 服务端 | 反作弊审查 | 7 天自动清理 |
| CDN | 精彩集锦分享 | 永久 |

---

## 八、UI 设计

### 8.1 录像浏览器

```
┌──────────────────────────────────────┐
│  📼 我的录像                      │
├──────────────────────────────────────┤
│ [🎬] combat_20260115_001  ★★★★☆  │
│      3杀集锦 · 45秒 · 15MB        │
│      [播放] [上传] [删除] [分享]    │
├──────────────────────────────────────┤
│ [🎬] combat_20260114_007  ★★★★★  │
│      五杀！· 1分20秒 · 28MB       │
│      [播放] [上传] [删除] [分享]    │
├──────────────────────────────────────┤
│ [🎬] combat_20260113_002  ★★☆☆☆  │
│      普通战斗 · 2分10秒 · 42MB     │
│      [播放] [上传] [删除] [分享]    │
└──────────────────────────────────────┘
```

### 8.2 回放控制条

```
⏮  ⏪  ▶/⏸  ⏩  ⏭
0:00                   2:10
[━━━━━━━━━━━━━━━●─────]
速度: 1x [0.5x] [1x] [2x] [4x]
```

---

## 九、实现步骤

```
Phase 1（基础）：
  ✅ DemoNetDriver 配置
  ✅ Start/Stop/Save/Load 接口
  ✅ 录像浏览器 UI

Phase 2（观战）：
  ✅ 死亡自动观战
  ✅ 目标切换
  ✅ 视角切换

Phase 3（精彩集锦）：
  ✅ HighlightDetector
  ✅ 自动评分
  ✅ 集锦剪辑生成

Phase 4（社交）：
  ✅ 分享到 Steam 社区
  ✅ 好友录像推荐
  ✅ 每周最佳集锦
```

---

## 十、注意事项

| 问题 | 解决方案 |
|---|---|
| 录像文件过大 | 降低录制帧率到 30fps + 压缩 |
| 版本不兼容 | 录像文件带版本号 + 不兼容时提示重新录制 |
| 作弊伪造录像 | 服务端权威录制 + 数字签名 |
| 回放时卡顿 | 预加载 + 流式读取 |
| 观战时信息泄露 | 观战者看不到击杀者的 HUD/血量 |
