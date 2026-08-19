#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "PvPSystem.generated.h"

// —— 伤害类型 ——
UCLASS()
class UDamageType_Kinetic : public UDamageType { GENERATED_BODY() };
UCLASS()
class UDamageType_Energy : public UDamageType { GENERATED_BODY() };
UCLASS()
class UDamageType_Thermal : public UDamageType { GENERATED_BODY() };
UCLASS()
class UDamageType_EMP : public UDamageType { GENERATED_BODY() };
UCLASS()
class UDamageType_Radiation : public UDamageType { GENERATED_BODY() };

// —— 伤害事件 ——
USTRUCT(BlueprintType)
struct FDamageEventInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    APawn* Attacker = nullptr;

    UPROPERTY(BlueprintReadOnly)
    APawn* Victim = nullptr;

    UPROPERTY(BlueprintReadOnly)
    float DamageAmount = 0.f;

    UPROPERTY(BlueprintReadOnly)
    TSubclassOf<UDamageType> DamageType;

    UPROPERTY(BlueprintReadOnly)
    FVector HitLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    FVector HitDirection = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    bool bIsCritical = false;

    UPROPERTY(BlueprintReadOnly)
    FString WeaponName;
};

// —— 爆炸效果配置 ——
USTRUCT(BlueprintType)
struct FExplosionEffectConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UNiagaraSystem* ExplosionNS = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    USoundBase* ExplosionSound = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ExplosionScale = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ExplosionDuration = 2.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor ExplosionColor = FLinearColor(1.f, 0.5f, 0.1f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ShockwaveRadius = 500.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ShockwaveForce = 1000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DebrisCount = 12;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DebrisSpeed = 800.f;
};

// —— 死亡效果配置 ——
USTRUCT(BlueprintType)
struct FDeathEffectConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UNiagaraSystem* DeathNS = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    USoundBase* DeathSound = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RagdollDuration = 5.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FadeOutDuration = 3.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bSpawnCorpse = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CorpseLifetime = 60.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UNiagaraSystem* BloodNS = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UNiagaraSystem* SparksNS = nullptr;
};

// —— PvP 战斗管理器（GameState 级） ——
UCLASS()
class STELLARSYSTEM_API APvPCombatManager : public AActor
{
    GENERATED_BODY()

public:
    APvPCombatManager();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // —— 伤害处理（服务端权威） ——
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
    void Server_ApplyDamage(APawn* Victim, float Damage, APawn* InstigatorPawn,
        TSubclassOf<UDamageType> DamageTypeClass, const FVector& HitLocation,
        const FVector& HitDirection, bool bIsCritical, const FString& WeaponName);

    // —— 飞船爆炸 ——
    UFUNCTION(BlueprintCallable, NetMulticast, Reliable)
    void Multicast_SpawnShipExplosion(const FVector& Location, float ShipSize,
        const FLinearColor& ExplosionTint);

    // —— 角色死亡效果 ——
    UFUNCTION(BlueprintCallable, NetMulticast, Reliable)
    void Multicast_SpawnDeathEffect(APawn* DeadPawn, const FVector& DeathLocation,
        bool bIsShip);

    // —— 复活点系统 ——
    UFUNCTION(BlueprintCallable, Server, Reliable)
    void Server_SetRespawnPoint(APawn* Player, const FVector& Location, AActor* AnchorActor);

    UFUNCTION(BlueprintCallable, Server, Reliable)
    void Server_RespawnPlayer(APawn* Player);

    // —— 击杀记录 ——
    UFUNCTION(BlueprintCallable, BlueprintPure)
    int32 GetKillCount(APawn* Player) const;

    UFUNCTION(BlueprintCallable, BlueprintPure)
    int32 GetDeathCount(APawn* Player) const;

    // —— 配置 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    FExplosionEffectConfig DefaultShipExplosion;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    FExplosionEffectConfig DefaultCharacterExplosion;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    FDeathEffectConfig DefaultDeathEffect;

    // —— 全局事件 ——
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerKilled, const FDamageEventInfo&, KillInfo);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnExplosionSpawned, const FVector&, Location, float, Size);

    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnPlayerKilled OnPlayerKilled;

    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnExplosionSpawned OnExplosionSpawned;

    // —— PvP 设置 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PvP")
    bool bFriendlyFire = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PvP")
    float ShipExplosionDamageRadius = 1500.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PvP")
    float ShipExplosionMaxDamage = 200.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PvP")
    float RespawnDelay = 5.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PvP")
    int32 MaxRespawns = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PvP")
    bool bShipCollisionsEnabled = true;

private:
    // 击杀/死亡统计
    TMap<FString, int32> KillCounts;
    TMap<FString, int32> DeathCounts;
    TMap<FString, int32> RespawnCounts;

    // 复活点
    struct FRespawnPoint
    {
        FVector Location;
        AActor* Anchor = nullptr; // 行星/空间站等
        float LastUseTime = 0.f;
    };
    TMap<FString, FRespawnPoint> RespawnPoints;

    // 处理击杀
    void HandleKill(APawn* Killer, APawn* Victim, const FDamageEventInfo& DamageInfo);

    // 生成碎片
    void SpawnDebris(const FVector& Location, float Count, float Speed, float Radius);

    // 链式爆炸（一艘船炸了引燃旁边）
    void ChainExplosion(const FVector& Location, float Radius);
};
