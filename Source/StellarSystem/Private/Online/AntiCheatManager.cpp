// AntiCheatManager.cpp
// 反外挂管理器实现 v6.5
//
// 核心原则：服务端权威。所有校验在服务端执行，客户端只是报告数据。
// 每个检测方法都有"严重度"评分，累积到阈值后自动惩罚。

#include "Online/AntiCheatManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/SecureHash.h"
#include "Misc/DateTime.h"
#include "HAL/PlatformTime.h"

// =====================================================================
// 生命周期
// =====================================================================

AAntiCheatManager::AAntiCheatManager()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = false; // 反作弊只在服务器运行
}

void AAntiCheatManager::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogTemp, Log, TEXT("[AntiCheat] Manager initialized (Sensitivity=%.1f, EAC=%s)"),
        DetectionSensitivity, bEnableEAC ? TEXT("ON") : TEXT("OFF"));

    if (bEnableEAC)
    {
        InitializeEAC();
    }
}

void AAntiCheatManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 定期全量扫描
    FullScanTimer += DeltaTime;
    if (FullScanTimer >= FullScanInterval)
    {
        FullScanTimer = 0.f;
        PeriodicFullScan(DeltaTime);
    }
}

// =====================================================================
// 玩家注册 / 注销
// =====================================================================

void AAntiCheatManager::Server_RegisterPlayer_Implementation(
    APlayerController* PC, const FString& PlayerID,
    const FString& ClientVersion, const FString& ClientChecksum)
{
    if (!PC) return;

    // 检查是否已封禁
    if (IsPlayerBanned(PlayerID))
    {
        UE_LOG(LogTemp, Warning, TEXT("[AntiCheat] Reject banned player: %s"), *PlayerID);
        PC->ClientTravel(TEXT(""), TRAVEL_Absolute); // 踢回主菜单
        OnPlayerKicked.Broadcast(PlayerID);
        return;
    }

    // 创建/更新档案
    FPlayerTrustProfile& Profile = PlayerProfiles.FindOrAdd(PlayerID);
    Profile.PlayerID = PlayerID;
    Profile.TotalSessions++;
    Profile.LastVerifiedTime = FDateTime::UtcNow();

    // 记录运行时数据
    FPlayerRuntimeData& RT = RuntimeData.FindOrAdd(PlayerID);
    RT.ClientVersion = ClientVersion;
    RT.MemoryChecksum = ClientChecksum;
    RT.LastHeartbeatTime = FPlatformTime::Seconds();

    // 版本检查
    DetectVersionMismatch(PlayerID, ClientVersion);

    // 文件完整性检查
    if (!ClientChecksum.IsEmpty() && !ExpectedClientChecksum.IsEmpty())
    {
        DetectMemoryTampering(PlayerID, ClientChecksum);
    }

    UE_LOG(LogTemp, Log, TEXT("[AntiCheat] Player registered: %s (Trust=%.0f, Sessions=%d)"),
        *PlayerID, Profile.TrustScore, Profile.TotalSessions);
}

bool AAntiCheatManager::Server_RegisterPlayer_Validate(
    APlayerController* PC, const FString& PlayerID,
    const FString& ClientVersion, const FString& ClientChecksum)
{
    return PC != nullptr && !PlayerID.IsEmpty();
}

void AAntiCheatManager::Server_UnregisterPlayer_Implementation(APlayerController* PC)
{
    if (!PC) return;
    FString ID = GetPlayerID(PC);
    if (FPlayerRuntimeData* RT = RuntimeData.Find(ID))
    {
        // 更新总游戏时间
        if (FPlayerTrustProfile* Profile = PlayerProfiles.Find(ID))
        {
            float PlaytimeSeconds = FPlatformTime::Seconds() - RT.LastHeartbeatTime;
            Profile.TotalPlaytimeMinutes += (int32)(PlaytimeSeconds / 60.f);
        }
        RuntimeData.Remove(ID);
    }
    UE_LOG(LogTemp, Log, TEXT("[AntiCheat] Player unregistered: %s"), *ID);
}

bool AAntiCheatManager::Server_UnregisterPlayer_Validate(APlayerController* PC)
{
    return PC != nullptr;
}

// =====================================================================
// 速度检测
// =====================================================================

void AAntiCheatManager::Server_ReportMovement_Implementation(
    APawn* Pawn, const FVector& Position, const FVector& Velocity, float ClientTime)
{
    if (!ValidateCommon(Pawn, TEXT("ReportMovement"))) return;

    FString ID = GetPlayerID(Pawn);
    FPlayerRuntimeData& RT = RuntimeData.FindOrAdd(ID);

    // 计算速度
    float Speed = Velocity.Size();
    bool bIsShip = Pawn->ActorHasTag(TEXT("Ship"));

    // 速度检测
    DetectSpeedHack(ID, Velocity, bIsShip);

    // 位置跳变检测
    DetectTeleportHack(ID, RT.LastPosition, Position);

    // 更新
    RT.LastPosition = Position;

    // 计时器篡改检测
    float ServerTime = FPlatformTime::Seconds();
    float TimeDelta = FMath::Abs(ClientTime - ServerTime);
    DetectTimerHack(ID, TimeDelta);
}

bool AAntiCheatManager::Server_ReportMovement_Validate(
    APawn* Pawn, const FVector& Position, const FVector& Velocity, float ClientTime)
{
    return Pawn != nullptr && Position.IsValid() && Velocity.IsValid();
}

void AAntiCheatManager::DetectSpeedHack(const FString& PlayerID, const FVector& Velocity, bool bIsShip)
{
    float Speed = Velocity.Size();
    float MaxSpeed = bIsShip ? MaxShipSpeed : MaxWalkSpeed;
    MaxSpeed *= SpeedTolerance; // 容差

    if (Speed > MaxSpeed)
    {
        float Severity = FMath::Clamp((Speed - MaxSpeed) / MaxSpeed, 0.1f, 1.f);
        Severity *= GetAdjustedSensitivity();

        ReportViolation(PlayerID, ECheatType::SpeedHack, Speed, MaxSpeed, Severity,
            FString::Printf(TEXT("Speed: %.0f > %.0f cm/s"), Speed, MaxSpeed));

        ApplyPenalty(PlayerID, ECheatType::SpeedHack, Severity);
    }
}

void AAntiCheatManager::DetectTeleportHack(const FString& PlayerID, const FVector& OldPos, const FVector& NewPos)
{
    float Dist = FVector::Dist(OldPos, NewPos);
    if (Dist > MaxTeleportDistance)
    {
        float Severity = FMath::Clamp(Dist / (MaxTeleportDistance * 5.f), 0.3f, 1.f);
        Severity *= GetAdjustedSensitivity();

        ReportViolation(PlayerID, ECheatType::TeleportHack, Dist, MaxTeleportDistance, Severity,
            FString::Printf(TEXT("Teleport: %.0f cm in one tick"), Dist));

        ApplyPenalty(PlayerID, ECheatType::TeleportHack, Severity);
    }
}

// =====================================================================
// 伤害检测
// =====================================================================

void AAntiCheatManager::Server_ValidateDamage_Implementation(
    APawn* Attacker, APawn* Victim, float ClaimedDamage,
    const FString& WeaponName, const FVector& HitLocation)
{
    if (!ValidateCommon(Attacker, TEXT("ValidateDamage"))) return;

    FString ID = GetPlayerID(Attacker);

    // 伤害值检测
    DetectDamageHack(ID, ClaimedDamage);

    // DPS 检测（窗口内累计）
    FPlayerRuntimeData& RT = RuntimeData.FindOrAdd(ID);
    float Now = FPlatformTime::Seconds();

    if (Now - RT.WindowStartTime > 5.f) // 5 秒窗口
    {
        RT.DamageInWindow = 0.f;
        RT.WindowStartTime = Now;
    }
    RT.DamageInWindow += ClaimedDamage;

    float DPS = RT.DamageInWindow / FMath::Max(Now - RT.WindowStartTime, 0.1f);
    if (DPS > MaxDPS * GetAdjustedSensitivity())
    {
        float Severity = FMath::Clamp(DPS / MaxDPS, 0.3f, 1.f);
        ReportViolation(ID, ECheatType::DamageHack, DPS, MaxDPS, Severity,
            FString::Printf(TEXT("DPS: %.0f > %.0f"), DPS, MaxDPS));
        ApplyPenalty(ID, ECheatType::DamageHack, Severity);
    }

    // 距离合理性（射击距离不应远超过武器射程）
    // 由武器系统自行验证，这里只记录
}

bool AAntiCheatManager::Server_ValidateDamage_Validate(
    APawn* Attacker, APawn* Victim, float ClaimedDamage,
    const FString& WeaponName, const FVector& HitLocation)
{
    return Attacker != nullptr && ClaimedDamage > 0.f && ClaimedDamage < MaxSingleHitDamage * 10.f;
}

void AAntiCheatManager::DetectDamageHack(const FString& PlayerID, float ClaimedDamage)
{
    if (ClaimedDamage > MaxSingleHitDamage)
    {
        float Severity = FMath::Clamp(ClaimedDamage / MaxSingleHitDamage, 0.5f, 1.f);
        Severity *= GetAdjustedSensitivity();

        ReportViolation(PlayerID, ECheatType::DamageHack, ClaimedDamage, MaxSingleHitDamage, Severity,
            FString::Printf(TEXT("Damage: %.0f > %.0f"), ClaimedDamage, MaxSingleHitDamage));
    }
}

// =====================================================================
// 射击频率检测
// =====================================================================

void AAntiCheatManager::Server_ReportShot_Implementation(
    APawn* Shooter, const FVector& ShotOrigin, const FVector& ShotDirection, float ClientTime)
{
    if (!ValidateCommon(Shooter, TEXT("ReportShot"))) return;

    FString ID = GetPlayerID(Shooter);
    FPlayerRuntimeData& RT = RuntimeData.FindOrAdd(ID);
    float Now = FPlatformTime::Seconds();

    DetectFireRateHack(ID, Now);

    RT.LastShotTime = Now;
    RT.ShotCountInWindow++;
}

bool AAntiCheatManager::Server_ReportShot_Validate(
    APawn* Shooter, const FVector& ShotOrigin, const FVector& ShotDirection, float ClientTime)
{
    return Shooter != nullptr && ShotDirection.IsNormalized();
}

void AAntiCheatManager::DetectFireRateHack(const FString& PlayerID, float CurrentTime)
{
    FPlayerRuntimeData* RT = RuntimeData.Find(PlayerID);
    if (!RT) return;

    float Interval = CurrentTime - RT->LastShotTime;
    if (Interval < MinShotInterval && RT->LastShotTime > 0.f)
    {
        float Severity = FMath::Clamp(MinShotInterval / FMath::Max(Interval, 0.001f), 1.f, 10.f) / 10.f;
        Severity *= GetAdjustedSensitivity();

        ReportViolation(PlayerID, ECheatType::FireRateHack, Interval, MinShotInterval, Severity,
            FString::Printf(TEXT("Fire interval: %.3fs < %.3fs"), Interval, MinShotInterval));
        ApplyPenalty(PlayerID, ECheatType::FireRateHack, Severity);
    }
}

// =====================================================================
// 资源变更检测
// =====================================================================

void AAntiCheatManager::Server_ReportResourceChange_Implementation(
    APawn* Player, const FString& ResourceType, int32 OldValue, int32 NewValue, const FString& Source)
{
    if (!ValidateCommon(Player, TEXT("ReportResourceChange"))) return;

    FString ID = GetPlayerID(Player);
    int32 Delta = NewValue - OldValue;

    // 异常增长检测（不应在一帧内获得大量资源）
    const int32 MaxReasonableGain = 10000; // 单次最大合理获得
    if (Delta > MaxReasonableGain)
    {
        float Severity = FMath::Clamp((float)Delta / (MaxReasonableGain * 10.f), 0.3f, 1.f);
        Severity *= GetAdjustedSensitivity();

        ReportViolation(ID, ECheatType::ResourceHack, (float)Delta, (float)MaxReasonableGain, Severity,
            FString::Printf(TEXT("Resource %s: +%d from %s"), *ResourceType, Delta, *Source));
        ApplyPenalty(ID, ECheatType::ResourceHack, Severity);
    }
}

bool AAntiCheatManager::Server_ReportResourceChange_Validate(
    APawn* Player, const FString& ResourceType, int32 OldValue, int32 NewValue, const FString& Source)
{
    return Player != nullptr && !ResourceType.IsEmpty() && NewValue >= 0;
}

// =====================================================================
// 心跳 / 完整性
// =====================================================================

void AAntiCheatManager::Server_Heartbeat_Implementation(
    APlayerController* PC, const FString& MemoryChecksum, float ClientTimeDelta)
{
    if (!PC) return;
    FString ID = GetPlayerID(PC);

    FPlayerRuntimeData& RT = RuntimeData.FindOrAdd(ID);
    RT.LastHeartbeatTime = FPlatformTime::Seconds();
    RT.MemoryChecksum = MemoryChecksum;

    // 时间差检测
    DetectTimerHack(ID, ClientTimeDelta);

    // 内存校验
    if (!MemoryChecksum.IsEmpty() && !ExpectedClientChecksum.IsEmpty())
    {
        DetectMemoryTampering(ID, MemoryChecksum);
    }

    // 更新档案
    if (FPlayerTrustProfile* Profile = PlayerProfiles.Find(ID))
    {
        Profile->LastVerifiedTime = FDateTime::UtcNow();
    }
}

bool AAntiCheatManager::Server_Heartbeat_Validate(
    APlayerController* PC, const FString& MemoryChecksum, float ClientTimeDelta)
{
    return PC != nullptr;
}

void AAntiCheatManager::DetectTimerHack(const FString& PlayerID, float ClientTimeDelta)
{
    if (ClientTimeDelta > MaxTimeDelta)
    {
        float Severity = FMath::Clamp(ClientTimeDelta / (MaxTimeDelta * 5.f), 0.2f, 1.f);
        Severity *= GetAdjustedSensitivity();

        ReportViolation(PlayerID, ECheatType::TimerManipulation, ClientTimeDelta, MaxTimeDelta, Severity,
            FString::Printf(TEXT("Time delta: %.2fs"), ClientTimeDelta));
        ApplyPenalty(PlayerID, ECheatType::TimerManipulation, Severity);
    }
}

void AAntiCheatManager::DetectVersionMismatch(const FString& PlayerID, const FString& ClientVersion)
{
    if (ClientVersion != CurrentClientVersion)
    {
        float Severity = 0.8f; // 版本不匹配很可疑
        ReportViolation(PlayerID, ECheatType::VersionMismatch, 0.f, 0.f, Severity,
            FString::Printf(TEXT("Version: %s != %s"), *ClientVersion, *CurrentClientVersion));

        // 版本不匹配通常直接踢出（可能是旧客户端或篡改）
        if (bEnableAutoPenalty)
        {
            KickPlayer(PlayerID);
        }
    }
}

void AAntiCheatManager::DetectMemoryTampering(const FString& PlayerID, const FString& ClientChecksum)
{
    if (ClientChecksum != ExpectedClientChecksum)
    {
        float Severity = 1.f; // 内存校验失败 = 极可疑
        ReportViolation(PlayerID, ECheatType::MemoryTampering, 0.f, 0.f, Severity,
            FString::Printf(TEXT("Checksum mismatch: %s vs %s"),
                *ClientChecksum, *ExpectedClientChecksum));

        ApplyPenalty(PlayerID, ECheatType::MemoryTampering, Severity);
    }
}

// =====================================================================
// 违规处理 / 惩罚
// =====================================================================

void AAntiCheatManager::ReportViolation(const FString& PlayerID, ECheatType Type,
    float DetectedValue, float ExpectedValue, float Severity, const FString& Context)
{
    FPlayerTrustProfile* Profile = PlayerProfiles.Find(PlayerID);
    if (!Profile) return;

    // 记录违规
    FCheatViolation Violation;
    Violation.Type = Type;
    Violation.DetectedValue = DetectedValue;
    Violation.ExpectedValue = ExpectedValue;
    Violation.Severity = Severity;
    Violation.Timestamp = FDateTime::UtcNow();
    Violation.Context = Context;

    Profile->RecentViolations.Add(Violation);
    Profile->ViolationCount++;

    // 降低信任分
    Profile->TrustScore = FMath::Max(0.f, Profile->TrustScore - Severity * 15.f);

    // 限制历史记录长度
    if (Profile->RecentViolations.Num() > 50)
    {
        Profile->RecentViolations.RemoveAt(0, Profile->RecentViolations.Num() - 50);
    }

    // 广播事件
    OnCheatDetected.Broadcast(PlayerID, Type, Severity);

    UE_LOG(LogTemp, Warning,
        TEXT("[AntiCheat] ★ VIOLATION: %s | Type=%d | Severity=%.2f | Trust=%.0f | %s"),
        *PlayerID, (int32)Type, Severity, Profile->TrustScore, *Context);
}

void AAntiCheatManager::ApplyPenalty(const FString& PlayerID, ECheatType Type, float Severity)
{
    if (!bEnableAutoPenalty) return;

    FPlayerTrustProfile* Profile = PlayerProfiles.Find(PlayerID);
    if (!Profile) return;

    EAntiCheatPenalty Penalty = DeterminePenalty(*Profile, Severity);

    switch (Penalty)
    {
    case EAntiCheatPenalty::Warning:
        // 只记录，不发通知（太频繁）
        break;

    case EAntiCheatPenalty::Cooldown:
        UE_LOG(LogTemp, Warning, TEXT("[AntiCheat] Cooldown applied to %s"), *PlayerID);
        // 由 GameMode 实现具体冷却逻辑
        break;

    case EAntiCheatPenalty::Kick:
        KickPlayer(PlayerID);
        break;

    case EAntiCheatPenalty::TempBan:
        BanPlayer(PlayerID, EAntiCheatPenalty::TempBan, TEXT("Temporary ban: repeated violations"));
        break;

    case EAntiCheatPenalty::PermaBan:
        BanPlayer(PlayerID, EAntiCheatPenalty::PermaBan, TEXT("Permanent ban: severe cheating"));
        break;

    case EAntiCheatPenalty::HWIDBan:
        BanPlayer(PlayerID, EAntiCheatPenalty::HWIDBan, TEXT("Hardware ban: confirmed cheat"));
        break;
    }
}

EAantiCheatPenalty AAntiCheatManager::DeterminePenalty(
    const FPlayerTrustProfile& Profile, float NewSeverity) const
{
    float Risk = Profile.CalculateRiskScore();

    // 单次极严重（如内存篡改）
    if (NewSeverity >= 0.9f)
    {
        return (Profile.ViolationCount > 2) ? EAntiCheatPenalty::HWIDBan : EAntiCheatPenalty::PermaBan;
    }

    // 多次违规
    if (Profile.ViolationCount >= 10 || Risk >= 80.f)
    {
        return EAntiCheatPenalty::PermaBan;
    }

    if (Profile.ViolationCount >= 5 || Risk >= 60.f)
    {
        return EAntiCheatPenalty::TempBan;
    }

    if (Profile.ViolationCount >= 3 || Risk >= 40.f)
    {
        return EAntiCheatPenalty::Kick;
    }

    if (NewSeverity >= 0.5f)
    {
        return EAntiCheatPenalty::Cooldown;
    }

    return EAntiCheatPenalty::Warning;
}

// =====================================================================
// 管理接口
// =====================================================================

void AAntiCheatManager::BanPlayer(const FString& PlayerID, EAntiCheatPenalty Penalty, const FString& Reason)
{
    FPlayerTrustProfile* Profile = PlayerProfiles.Find(PlayerID);
    if (!Profile) return;

    Profile->bBanned = true;
    Profile->BanReason = Reason;

    FString PenaltyStr;
    switch (Penalty)
    {
    case EAntiCheatPenalty::TempBan:  PenaltyStr = TEXT("TempBan (24h)"); break;
    case EAntiCheatPenalty::PermaBan: PenaltyStr = TEXT("PermaBan"); break;
    case EAntiCheatPenalty::HWIDBan: PenaltyStr = TEXT("HWIDBan"); break;
    default: PenaltyStr = TEXT("Kick"); break;
    }

    UE_LOG(LogTemp, Error, TEXT("[AntiCheat] ★ BAN: %s | %s | Reason: %s"),
        *PlayerID, *PenaltyStr, *Reason);

    OnPlayerBanned.Broadcast(PlayerID, Reason);

    // 踢出
    KickPlayer(PlayerID);
}

void AAntiCheatManager::UnbanPlayer(const FString& PlayerID)
{
    if (FPlayerTrustProfile* Profile = PlayerProfiles.Find(PlayerID))
    {
        Profile->bBanned = false;
        Profile->BanReason = TEXT("");
        Profile->TrustScore = FMath::Min(100.f, Profile->TrustScore + 30.f);
        UE_LOG(LogTemp, Log, TEXT("[AntiCheat] Unbanned: %s"), *PlayerID);
    }
}

void AAntiCheatManager::KickPlayer(const FString& PlayerID)
{
    // 通过 GameMode 找 PC 并踢出
    if (UWorld* World = GetWorld())
    {
        for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
        {
            APlayerController* PC = It->Get();
            if (PC && GetPlayerID(PC) == PlayerID)
            {
                PC->ClientTravel(TEXT(""), TRAVEL_Absolute);
                break;
            }
        }
    }
    OnPlayerKicked.Broadcast(PlayerID);
}

void AAntiCheatManager::SetDetectionSensitivity(float NewSensitivity)
{
    DetectionSensitivity = FMath::Clamp(NewSensitivity, 0.1f, 5.f);
    UE_LOG(LogTemp, Log, TEXT("[AntiCheat] Sensitivity set to %.1f"), DetectionSensitivity);
}

// =====================================================================
// 查询
// =====================================================================

float AAntiCheatManager::GetPlayerTrustScore(const FString& PlayerID) const
{
    if (const FPlayerTrustProfile* P = PlayerProfiles.Find(PlayerID))
        return P->TrustScore;
    return 100.f; // 未知玩家默认满分
}

int32 AAntiCheatManager::GetViolationCount(const FString& PlayerID) const
{
    if (const FPlayerTrustProfile* P = PlayerProfiles.Find(PlayerID))
        return P->ViolationCount;
    return 0;
}

bool AAntiCheatManager::IsPlayerBanned(const FString& PlayerID) const
{
    if (const FPlayerTrustProfile* P = PlayerProfiles.Find(PlayerID))
        return P->bBanned;
    return false;
}

EAantiCheatPenalty AAntiCheatManager::GetRecommendedPenalty(const FString& PlayerID) const
{
    if (const FPlayerTrustProfile* P = PlayerProfiles.Find(PlayerID))
    {
        return DeterminePenalty(*P, 0.f);
    }
    return EAntiCheatPenalty::Warning;
}

// =====================================================================
// 定期全量扫描
// =====================================================================

void AAntiCheatManager::PeriodicFullScan(float DeltaTime)
{
    for (auto& Pair : PlayerProfiles)
    {
        const FString& ID = Pair.Key;
        const FPlayerTrustProfile& Profile = Pair.Value;

        // 统计异常：K/D 比率
        DetectStatisticalAnomaly(ID);

        // 心跳超时检测
        if (const FPlayerRuntimeData* RT = RuntimeData.Find(ID))
        {
            float IdleTime = FPlatformTime::Seconds() - RT->LastHeartbeatTime;
            if (IdleTime > 120.f) // 2 分钟无心跳
            {
                UE_LOG(LogTemp, Warning, TEXT("[AntiCheat] No heartbeat from %s for %.0fs"), *ID, IdleTime);
                // 不立即惩罚，可能是网络问题
            }
        }
    }
}

void AAntiCheatManager::DetectStatisticalAnomaly(const FString& PlayerID)
{
    // 从 PvP 系统获取 K/D 数据
    if (APvPCombatManager* PvP = nullptr /* 通过 GameMode 获取 */)
    {
        int32 Kills = PvP->GetKillCount(nullptr); // 需要按 ID 查询
        int32 Deaths = PvP->GetDeathCount(nullptr);

        if (Deaths == 0 && Kills > 20)
        {
            float Severity = FMath::Clamp((float)Kills / 50.f, 0.3f, 0.8f);
            ReportViolation(PlayerID, ECheatType::StatisticalAnomaly, (float)Kills, 20.f, Severity,
                FString::Printf(TEXT("Suspicious K/D: %d/0"), Kills));
        }
    }
}

// =====================================================================
// 工具方法
// =====================================================================

bool AAntiCheatManager::ValidateCommon(APawn* Pawn, const TCHAR* FunctionName) const
{
    if (!Pawn) return false;
    if (!HasAuthority()) return false; // 只在服务器执行
    return true;
}

FString AAntiCheatManager::GetPlayerID(APlayerController* PC) const
{
    if (!PC) return TEXT("Unknown");
    // 优先用 SteamID，fallback 到 PlayerName
    // 实际项目中从 PlayerState 获取
    return PC->GetName();
}

FString AAntiCheatManager::GetPlayerID(APawn* Pawn) const
{
    if (!Pawn) return TEXT("Unknown");
    if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
    {
        return GetPlayerID(PC);
    }
    return Pawn->GetName();
}

// =====================================================================
// EAC 接口（预留）
// =====================================================================

void AAntiCheatManager::InitializeEAC()
{
    UE_LOG(LogTemp, Log, TEXT("[AntiCheat] EAC: Initialization requested (stub)"));
    // 实际集成需要：
    // 1. 链接 EasyAntiCheat SDK 库
    // 2. 在 Build.cs 添加 "EasyAntiCheat" 模块
    // 3. 调用 EOS SDK 的 AntiCheat 接口
    // 4. 在 DefaultEngine.ini 配置 [EasyAntiCheat]
    // 这里只做占位
}

void AAntiCheatManager::ShutdownEAC()
{
    UE_LOG(LogTemp, Log, TEXT("[AntiCheat] EAC: Shutdown (stub)"));
}

bool AAntiCheatManager::VerifyWithEAC(const FString& PlayerID, const FString& Token) const
{
    // 调用 EAC 服务端验证
    return true; // stub
}
