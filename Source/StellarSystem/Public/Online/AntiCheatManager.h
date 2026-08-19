// AntiCheatManager.h
// 反外挂管理器 v6.5
//
// 设计原则（深度防御）：
//   1. 服务端权威：所有关键状态由服务器校验，客户端只是"显示器"
//   2. 多层检测：速度/位置/伤害/计时/内存/完整性 多维度交叉验证
//   3. 渐进惩罚：警告 → 踢出 → 封禁 → 硬件封禁
//   4. EAC 就绪：预留 Easy Anti-Cheat 接入接口
//
// 检测项：
//   - 速度异常（加速挂）
//   - 位置跳变（传送挂）
//   - 伤害异常（秒杀挂/无限伤害）
//   - 射击频率（连发挂）
//   - 计时器篡改（加速挂）
//   - 内存完整性（CRC 校验关键数据）
//   - 客户端版本匹配
//   - 资源文件哈希校验
//
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AntiCheatManager.generated.h"

class APlayerController;
class APawn;
class AStellarGameMode;

// 作弊类型枚举
UENUM(BlueprintType)
enum class ECheatType : uint8
{
    SpeedHack,          // 移动速度异常
    TeleportHack,       // 位置瞬间跳变
    DamageHack,         // 伤害值异常
    FireRateHack,       // 射击频率异常
    TimerManipulation,   // 计时器篡改
    MemoryTampering,     // 内存篡改
    VersionMismatch,     // 版本不匹配
    FileIntegrityFail,   // 文件完整性校验失败
    ResourceHack,        // 资源/货币异常增长
    InputSpoofing,      // 输入伪造
    ReplayAnalysis,      // 回放分析标记
    StatisticalAnomaly   // 统计异常（K/D 离群）
};

// 惩罚等级
UENUM(BlueprintType)
enum class EAntiCheatPenalty : uint8
{
    Warning,         // 警告（可忽略）
    Cooldown,        // 冷却（短时间禁止操作）
    Kick,            // 踢出当前会话
    TempBan,         // 临时封禁（24h）
    PermaBan,        // 永久封禁
    HWIDBan          // 硬件封禁（最重）
};

// 单次违规记录
USTRUCT(BlueprintType)
struct FCheatViolation
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    ECheatType Type = ECheatType::SpeedHack;

    UPROPERTY(BlueprintReadOnly)
    float DetectedValue = 0.f;  // 检测到的值

    UPROPERTY(BlueprintReadOnly)
    float ExpectedValue = 0.f;   // 期望值

    UPROPERTY(BlueprintReadOnly)
    float Severity = 0.f;        // 严重度 0~1

    UPROPERTY(BlueprintReadOnly)
    FDateTime Timestamp;

    UPROPERTY(BlueprintReadOnly)
    FString Context;             // 上下文描述
};

// 玩家信任档案（持续累积）
USTRUCT(BlueprintType)
struct FPlayerTrustProfile
{
    GENERATED_BODY()

    UPROPERTY()
    FString PlayerID;            // SteamID / AccountID

    UPROPERTY()
    int32 TotalSessions = 0;

    UPROPERTY()
    int32 TotalPlaytimeMinutes = 0;

    UPROPERTY()
    int32 ViolationCount = 0;

    UPROPERTY()
    float TrustScore = 100.f;    // 100=完全可信，0=必是作弊

    UPROPERTY()
    TArray<FCheatViolation> RecentViolations;

    UPROPERTY()
    FDateTime LastVerifiedTime;

    UPROPERTY()
    bool bBanned = false;

    UPROPERTY()
    FString BanReason;

    // 计算综合风险评分
    float CalculateRiskScore() const
    {
        float Risk = 100.f - TrustScore;

        // 近期违规加权
        for (const FCheatViolation& V : RecentViolations)
        {
            // 近 1 小时的违规权重高
            float AgeHours = (FDateTime::UtcNow() - V.Timestamp).GetTotalHours();
            float Weight = FMath::Clamp(1.f - AgeHours / 24.f, 0.1f, 1.f);
            Risk += V.Severity * 20.f * Weight;
        }

        return FMath::Clamp(Risk, 0.f, 100.f);
    }
};

// 反作弊管理器（GameState 级，权威运行在服务器）
UCLASS(Blueprintable)
class STELLARSYSTEM_API AAntiCheatManager : public AActor
{
    GENERATED_BODY()

public:
    AAntiCheatManager();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // =====================================================================
    // 初始化
    // =====================================================================

    // 新玩家加入时注册
    UFUNCTION(BlueprintCallable, Server, Reliable)
    void Server_RegisterPlayer(APlayerController* PC, const FString& PlayerID,
        const FString& ClientVersion, const FString& ClientChecksum);

    // 玩家离开时注销
    UFUNCTION(BlueprintCallable, Server, Reliable)
    void Server_UnregisterPlayer(APlayerController* PC);

    // =====================================================================
    // 实时检测接口（由其他系统调用）
    // =====================================================================

    // 速度检测：客户端报告速度，服务端校验
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
    void Server_ReportMovement(APawn* Pawn, const FVector& Position,
        const FVector& Velocity, float ClientTime);

    // 伤害检测：客户端请求伤害，服务端二次校验
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
    void Server_ValidateDamage(APawn* Attacker, APawn* Victim,
        float ClaimedDamage, const FString& WeaponName, const FVector& HitLocation);

    // 射击检测：射击频率 + 命中率
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
    void Server_ReportShot(APawn* Shooter, const FVector& ShotOrigin,
        const FVector& ShotDirection, float ClientTime);

    // 资源变更检测（货币/物品）
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
    void Server_ReportResourceChange(APawn* Player, const FString& ResourceType,
        int32 OldValue, int32 NewValue, const FString& Source);

    // 客户端完整性校验（定期心跳）
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
    void Server_Heartbeat(APlayerController* PC, const FString& MemoryChecksum,
        float ClientTimeDelta);

    // =====================================================================
    // 查询接口
    // =====================================================================

    UFUNCTION(BlueprintPure, Category = "AntiCheat")
    float GetPlayerTrustScore(const FString& PlayerID) const;

    UFUNCTION(BlueprintPure, Category = "AntiCheat")
    int32 GetViolationCount(const FString& PlayerID) const;

    UFUNCTION(BlueprintPure, Category = "AntiCheat")
    bool IsPlayerBanned(const FString& PlayerID) const;

    UFUNCTION(BlueprintPure, Category = "AntiCheat")
    EAntiCheatPenalty GetRecommendedPenalty(const FString& PlayerID) const;

    // =====================================================================
    // 管理接口（GM/管理员）
    // =====================================================================

    UFUNCTION(BlueprintCallable, Category = "AntiCheat|Admin")
    void BanPlayer(const FString& PlayerID, EAntiCheatPenalty Penalty,
        const FString& Reason);

    UFUNCTION(BlueprintCallable, Category = "AntiCheat|Admin")
    void UnbanPlayer(const FString& PlayerID);

    UFUNCTION(BlueprintCallable, Category = "AntiCheat|Admin")
    void SetDetectionSensitivity(float NewSensitivity); // 0.5=宽松, 2.0=严格

    // =====================================================================
    // 配置
    // =====================================================================

    // 速度检测
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection|Speed")
    float MaxWalkSpeed = 1200.f;       // 最大地面移动速度 cm/s

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection|Speed")
    float MaxShipSpeed = 15000.f;      // 最大飞船速度 cm/s

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection|Speed")
    float SpeedTolerance = 1.2f;      // 容差倍数（允许 20% 超出）

    // 位置检测
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection|Position")
    float MaxTeleportDistance = 5000.f; // 单次移动最大距离 cm

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection|Position")
    float PositionLerpTolerance = 200.f; // 插值容差 cm

    // 伤害检测
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection|Damage")
    float MaxSingleHitDamage = 5000.f;  // 单次最大伤害

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection|Damage")
    float MaxDPS = 10000.f;            // 每秒最大伤害输出

    // 射击检测
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection|FireRate")
    float MinShotInterval = 0.05f;     // 最小射击间隔（秒）= 最高 20 发/秒

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection|FireRate")
    float MaxHitRate = 0.95f;          // 最大命中率（超过即可疑）

    // 计时器检测
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection|Timer")
    float MaxTimeDelta = 0.5f;         // 客户端-服务器时间差上限（秒）

    // 全局
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AntiCheat")
    float DetectionSensitivity = 1.0f; // 全局灵敏度

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AntiCheat")
    bool bEnableAutoPenalty = true;     // 自动执行惩罚

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AntiCheat")
    bool bEnableEAC = false;            // Easy Anti-Cheat 开关（需 SDK）

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AntiCheat")
    FString CurrentClientVersion = TEXT("1.0.0");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AntiCheat")
    FString ExpectedClientChecksum = TEXT(""); // 预期客户端校验和

    // =====================================================================
    // 事件
    // =====================================================================

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnCheatDetected,
        const FString&, PlayerID, ECheatType, Type, float, Severity);

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerBanned,
        const FString&, PlayerID, const FString&, Reason);

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerKicked,
        const FString&, PlayerID);

    UPROPERTY(BlueprintAssignable, Category = "AntiCheat|Events")
    FOnCheatDetected OnCheatDetected;

    UPROPERTY(BlueprintAssignable, Category = "AntiCheat|Events")
    FOnPlayerBanned OnPlayerBanned;

    UPROPERTY(BlueprintAssignable, Category = "AntiCheat|Events")
    FOnPlayerKicked OnPlayerKicked;

protected:
    // 玩家档案库
    UPROPERTY()
    TMap<FString, FPlayerTrustProfile> PlayerProfiles;

    // 临时数据（当前 tick 用）
    struct FPlayerRuntimeData
    {
        FVector LastPosition = FVector::ZeroVector;
        float LastShotTime = 0.f;
        int32 ShotCountInWindow = 0;
        float DamageInWindow = 0.f;
        float WindowStartTime = 0.f;
        float LastHeartbeatTime = 0.f;
        FString ClientVersion;
        FString MemoryChecksum;
    };
    TMap<FString, FPlayerRuntimeData> RuntimeData;

    // 检测灵敏度
    float GetAdjustedSensitivity() const { return DetectionSensitivity; }

    // 内部检测方法
    void DetectSpeedHack(const FString& PlayerID, const FVector& Velocity, bool bIsShip);
    void DetectTeleportHack(const FString& PlayerID, const FVector& OldPos, const FVector& NewPos);
    void DetectFireRateHack(const FString& PlayerID, float CurrentTime);
    void DetectDamageHack(const FString& PlayerID, float ClaimedDamage);
    void DetectTimerHack(const FString& PlayerID, float ClientTimeDelta);
    void DetectVersionMismatch(const FString& PlayerID, const FString& ClientVersion);
    void DetectMemoryTampering(const FString& PlayerID, const FString& ClientChecksum);

    // 违规处理
    void ReportViolation(const FString& PlayerID, ECheatType Type,
        float DetectedValue, float ExpectedValue, float Severity, const FString& Context);
    void ApplyPenalty(const FString& PlayerID, ECheatType Type, float Severity);
    EAntiCheatPenalty DeterminePenalty(const FPlayerTrustProfile& Profile, float NewSeverity) const;

    // 网络 RPC 验证
    bool ValidateCommon(APawn* Pawn, const TCHAR* FunctionName) const;

    // 获取 PlayerID
    FString GetPlayerID(APlayerController* PC) const;
    FString GetPlayerID(APawn* Pawn) const;

    // 定期全量扫描
    void PeriodicFullScan(float DeltaTime);
    float FullScanTimer = 0.f;
    static constexpr float FullScanInterval = 30.f; // 每 30 秒一次全量扫描

    // 统计异常检测（K/D 比率等）
    void DetectStatisticalAnomaly(const FString& PlayerID);

    // EAC 接口（预留）
    void InitializeEAC();
    void ShutdownEAC();
    bool VerifyWithEAC(const FString& PlayerID, const FString& Token) const;
};
