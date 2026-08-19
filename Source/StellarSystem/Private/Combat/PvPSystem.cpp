#include "Combat/PvPSystem.h"
#include "GameFramework/Pawn.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/Classes/PhysicsEngine/RadialForceComponent.h"
#include "Online/AntiCheatManager.h"
#include "Core/StellarGameMode.h"

APvPCombatManager::APvPCombatManager()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
}

void APvPCombatManager::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogTemp, Log, TEXT("[PvP] Combat Manager initialized"));
}

void APvPCombatManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    // 后续可加：持续伤害区域、毒气云、辐射区等
}

// —— 服务端伤害处理 ——

void APvPCombatManager::Server_ApplyDamage_Implementation(APawn* Victim,
    float Damage, APawn* InstigatorPawn, TSubclassOf<UDamageType> DamageTypeClass,
    const FVector& HitLocation, const FVector& HitDirection, bool bIsCritical,
    const FString& WeaponName)
{
    if (!Victim || !Victim->GetController()) return;

    // v6.5：反作弊 — 先验证伤害合法性
    AStellarGameMode* GM = Cast<AStellarGameMode>(GetWorld()->GetAuthGameMode());
    if (GM && GM->AntiCheat)
    {
        // 让反作弊系统校验伤害值
        GM->AntiCheat->Server_ValidateDamage(
            InstigatorPawn, Victim, Damage, WeaponName, HitLocation);

        // 如果信任分过低，直接拒绝伤害
        FString AttackerID = InstigatorPawn->GetName();
        float Trust = GM->AntiCheat->GetPlayerTrustScore(AttackerID);
        if (Trust < 20.f)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[PvP] Damage BLOCKED - low trust: %.0f (%.1f dmg from %s)"),
                Trust, Damage, *AttackerID);
            return; // 拒绝可疑伤害
        }
    }

    // 友伤检查
    if (!bFriendlyFire)
    {
        // 同队伍不伤害（简化：SteamID 同前缀为同队）
        // 实际项目应从 PlayerState 拿 TeamID
    }

    // 应用伤害
    FDamageEvent DamageEvent(DamageTypeClass);
    Victim->TakeDamage(Damage, DamageEvent, InstigatorPawn->GetController(), this);

    // 构造伤害信息
    FDamageEventInfo Info;
    Info.Attacker = InstigatorPawn;
    Info.Victim = Victim;
    Info.DamageAmount = Damage;
    Info.DamageType = DamageTypeClass;
    Info.HitLocation = HitLocation;
    Info.HitDirection = HitDirection;
    Info.bIsCritical = bIsCritical;
    Info.WeaponName = WeaponName;

    // 检查是否致死
    if (Victim->GetHealth() <= 0.f || (Victim->GetHealth() - Damage) <= 0.f)
    {
        HandleKill(InstigatorPawn, Victim, Info);
    }

    // 伤害数字飘字（客户端 RPC）
    Multicast_SpawnDamageNumber(HitLocation, Damage, bIsCritical);

    UE_LOG(LogTemp, Log, TEXT("[PvP] Damage: %s → %s (%.1f, %s)"),
        *GetNameSafe(InstigatorPawn), *GetNameSafe(Victim),
        Damage, bIsCritical ? TEXT("CRIT") : TEXT("normal"));
}

bool APvPCombatManager::Server_ApplyDamage_Validate(APawn* Victim, float Damage,
    APawn* InstigatorPawn, TSubclassOf<UDamageType> DamageTypeClass,
    const FVector& HitLocation, const FVector& HitDirection, bool bIsCritical,
    const FString& WeaponName)
{
    // 防作弊：伤害值合理范围
    return Damage > 0.f && Damage < 10000.f;
}

// —— 爆炸效果 ——

void APvPCombatManager::Multicast_SpawnShipExplosion_Implementation(
    const FVector& Location, float ShipSize, const FLinearColor& ExplosionTint)
{
    // Niagara 粒子
    if (DefaultShipExplosion.ExplosionNS)
    {
        UNiagaraComponent* NS = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            GetWorld(), DefaultShipExplosion.ExplosionNS, Location,
            FRotator::ZeroRotator, FVector(ShipSize * 0.01f), true);
        if (NS)
        {
            NS->SetVariableLinearColor(TEXT("UserColor"), ExplosionTint);
            NS->SetVariableFloat(TEXT("UserScale"), ShipSize * 0.01f);
        }
    }

    // 音效
    if (DefaultShipExplosion.ExplosionSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, DefaultShipExplosion.ExplosionSound,
            Location, ShipSize * 0.001f, 1.f, 0.f);
    }

    // 冲击波
    SpawnDebris(Location, DefaultShipExplosion.DebrisCount, DefaultShipExplosion.DebrisSpeed,
        DefaultShipExplosion.ShockwaveRadius * ShipSize * 0.01f);

    // 链式爆炸
    GetWorld()->GetTimerManager().SetTimerForNextTick([this, Location, ShipSize]()
    {
        ChainExplosion(Location, ShipSize * 10.f);
    });

    OnExplosionSpawned.Broadcast(Location, ShipSize);
    UE_LOG(LogTemp, Log, TEXT("[PvP] Ship explosion at %s (size %.0f)"), *Location.ToString(), ShipSize);
}

void APvPCombatManager::Multicast_SpawnDeathEffect_Implementation(APawn* DeadPawn,
    const FVector& DeathLocation, bool bIsShip)
{
    if (!DeadPawn) return;

    // 死亡粒子
    if (DefaultDeathEffect.DeathNS)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            GetWorld(), DefaultDeathEffect.DeathNS, DeathLocation,
            FRotator::ZeroRotator, FVector(1.f), true);
    }

    // 死亡音效
    if (DefaultDeathEffect.DeathSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, DefaultDeathEffect.DeathSound,
            DeathLocation);
    }

    // 血液/火花
    if (bIsShip && DefaultDeathEffect.SparksNS)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            GetWorld(), DefaultDeathEffect.SparksNS, DeathLocation,
            FRotator::ZeroRotator, FVector(2.f), true);
    }
    else if (!bIsShip && DefaultDeathEffect.BloodNS)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            GetWorld(), DefaultDeathEffect.BloodNS, DeathLocation,
            FRotator::ZeroRotator, FVector(1.f), true);
    }

    // 布娃娃
    if (DeadPawn->GetMesh())
    {
        DeadPawn->GetMesh()->SetSimulatePhysics(true);
        DeadPawn->GetMesh()->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
        DeadPawn->GetMesh()->AddImpulse(FVector::UpVector * 50000.f, NAME_None, true);
    }

    // 延迟淡出/销毁
    FTimerHandle TimerHandle;
    GetWorld()->GetTimerManager().SetTimer(TimerHandle, [DeadPawn, this]()
    {
        if (DeadPawn && DefaultDeathEffect.bSpawnCorpse)
        {
            // 生成尸体 Actor（简化：直接保留并淡出）
            if (DeadPawn->GetMesh())
            {
                DeadPawn->GetMesh()->SetScalarParameterValueOnMaterials(TEXT("Opacity"), 0.f);
            }
            DeadPawn->Destroy();
        }
    }, DefaultDeathEffect.RagdollDuration, false);

    UE_LOG(LogTemp, Log, TEXT("[PvP] Death effect: %s (ship=%s)"),
        *GetNameSafe(DeadPawn), bIsShip ? TEXT("Y") : TEXT("N"));
}

// —— 击杀处理 ——

void APvPCombatManager::HandleKill(APawn* Killer, APawn* Victim, const FDamageEventInfo& DamageInfo)
{
    if (!Killer || !Victim) return;

    FString KillerName = Killer->GetName();
    FString VictimName = Victim->GetName();

    KillCounts.FindOrAdd(KillerName)++;
    DeathCounts.FindOrAdd(VictimName)++;

    OnPlayerKilled.Broadcast(DamageInfo);

    // 通知被杀者
    if (APlayerController* VictimPC = Cast<APlayerController>(Victim->GetController()))
    {
        // 客户端显示死亡界面
        Client_ShowDeathScreen(VictimPC, Killer, DamageInfo);
    }

    // 生成死亡效果
    Multicast_SpawnDeathEffect(Victim, Victim->GetActorLocation(),
        Victim->ActorHasTag(TEXT("Ship")));

    // 如果是飞船，额外大爆炸
    if (Victim->ActorHasTag(TEXT("Ship")))
    {
        float ShipSize = Victim->GetSimpleCollisionRadius() * 2.f;
        FLinearColor Tint(1.f, 0.6f, 0.1f, 1.f);
        Multicast_SpawnShipExplosion(Victim->GetActorLocation(), ShipSize, Tint);
    }

    // 检查复活点
    FString VictimKey = VictimName;
    if (RespawnPoints.Contains(VictimKey))
    {
        // 有复活点 → 延迟复活
        FTimerHandle Handle;
        GetWorld()->GetTimerManager().SetTimer(Handle, [this, Victim]()
        {
            Server_RespawnPlayer(Victim);
        }, RespawnDelay, false);
    }

    UE_LOG(LogTemp, Warning, TEXT("[PvP] ★ KILL: %s  →  %s  (%s, %.0f dmg)"),
        *KillerName, *VictimName, *DamageInfo.WeaponName, DamageInfo.DamageAmount);
}

// —— 复活点系统 ——

void APvPCombatManager::Server_SetRespawnPoint_Implementation(APawn* Player,
    const FVector& Location, AActor* AnchorActor)
{
    if (!Player) return;

    FString Key = Player->GetName();
    FRespawnPoint& Point = RespawnPoints.FindOrAdd(Key);
    Point.Location = Location;
    Point.Anchor = AnchorActor;
    Point.LastUseTime = FPlatformTime::Seconds();

    UE_LOG(LogTemp, Log, TEXT("[PvP] Respawn point set for %s at %s"), *Key, *Location.ToString());
}

bool APvPCombatManager::Server_SetRespawnPoint_Validate(APawn* Player,
    const FVector& Location, AActor* AnchorActor)
{
    return Player != nullptr;
}

void APvPCombatManager::Server_RespawnPlayer_Implementation(APawn* Player)
{
    if (!Player) return;

    FString Key = Player->GetName();

    // 检查复活次数
    int32& Count = RespawnCounts.FindOrAdd(Key);
    if (Count >= MaxRespawns)
    {
        UE_LOG(LogTemp, Log, TEXT("[PvP] %s exceeded max respawns (%d)"), *Key, MaxRespawns);
        // 通知客户端：游戏结束
        return;
    }
    Count++;

    // 找复活点
    FVector SpawnLocation;
    if (FRespawnPoint* Point = RespawnPoints.Find(Key))
    {
        SpawnLocation = Point->Location;
    }
    else
    {
        // 默认：回到最近的星球表面
        SpawnLocation = FVector::ZeroVector; // GameMode 会处理
    }

    // 生成新 Pawn
    if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
    {
        // 销毁旧 Pawn
        Player->Destroy();

        // 通知 GameMode 生成新 Pawn
        if (AGameModeBase* GM = GetWorld()->GetAuthGameMode())
        {
            AActor* StartSpot = nullptr;
            // 用 SpawnLocation 附近找 PlayerStart
            TArray<AActor*> Starts;
            UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), Starts);
            if (Starts.Num() > 0)
            {
                StartSpot = Starts[FMath::RandRange(0, Starts.Num() - 1)];
            }

            GM->RestartPlayerAtPlayerStart(PC, StartSpot ? StartSpot : nullptr);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[PvP] Respawned %s (count %d/%d)"), *Key, Count, MaxRespawns);
}

bool APvPCombatManager::Server_RespawnPlayer_Validate(APawn* Player)
{
    return Player != nullptr;
}

// —— 查询 ——

int32 APvPCombatManager::GetKillCount(APawn* Player) const
{
    if (!Player) return 0;
    const int32* Count = KillCounts.Find(Player->GetName());
    return Count ? *Count : 0;
}

int32 APvPCombatManager::GetDeathCount(APawn* Player) const
{
    if (!Player) return 0;
    const int32* Count = DeathCounts.Find(Player->GetName());
    return Count ? *Count : 0;
}

// —— 私有工具 ——

void APvPCombatManager::SpawnDebris(const FVector& Location, float Count, float Speed, float Radius)
{
    // 简化版：用 Niagara 替代真实物理碎片
    if (DefaultShipExplosion.ExplosionNS)
    {
        for (int32 i = 0; i < (int32)Count; ++i)
        {
            FVector Dir = FMath::VRand();
            FVector DebrisPos = Location + Dir * FMath::FRandRange(0.f, Radius * 0.3f);
            UNiagaraComponent* NS = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                GetWorld(), DefaultShipExplosion.ExplosionNS, DebrisPos,
                FRotator::ZeroRotator, FVector(0.1f), true);
            if (NS)
            {
                NS->SetVariableVec3(TEXT("Direction"), Dir * Speed);
            }
        }
    }
}

void APvPCombatManager::ChainExplosion(const FVector& Location, float Radius)
{
    // 查找半径内的其他飞船
    TArray<AActor*> Overlapping;
    // 用 SphereOverlapActors 找附近飞船
    // 简化：直接对范围内所有带 "Ship" 标签的 Actor 施加伤害
    TArray<AActor*> Ships;
    UGameplayStatics::GetAllActorsWithTag(GetWorld(), TEXT("Ship"), Ships);

    for (AActor* Ship : Ships)
    {
        if (!Ship || Ship->GetActorLocation().Equals(Location, 1.f)) continue;
        float Dist = FVector::Dist(Ship->GetActorLocation(), Location);
        if (Dist < Radius)
        {
            float Dmg = ShipExplosionMaxDamage * (1.f - Dist / Radius) * 0.5f;
            if (Dmg > 10.f)
            {
                FDamageEvent Evt(UDamageType_Thermal::StaticClass());
                Ship->TakeDamage(Dmg, Evt, nullptr, this);
                UE_LOG(LogTemp, Log, TEXT("[PvP] Chain explosion: %.0f dmg to %s"), Dmg, *Ship->GetName());
            }
        }
    }
}

// —— 客户端 RPC ——

void APvPCombatManager::Client_ShowDeathScreen_Implementation(APlayerController* PC,
    APawn* Killer, const FDamageEventInfo& DamageInfo)
{
    // 蓝图实现：显示死亡界面 + 重生倒计时
    // 这里只发事件
    UE_LOG(LogTemp, Log, TEXT("[PvP] Client: Show death screen for %s"), *GetNameSafe(PC));
}

void APvPCombatManager::Multicast_SpawnDamageNumber_Implementation(const FVector& Location,
    float Damage, bool bIsCritical)
{
    // 蓝图实现：飘字伤害数字
    // 这里留接口
}
