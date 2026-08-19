// RespawnSystem.h
// 复活点设置 + 管理 + 复活逻辑

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RespawnSystem.generated.h"

class UBoxComponent;
class UParticleSystemComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ERespawnType : uint8
{
    PlanetSurface,   // 星球表面
    SpaceStation,    // 空间站
    ShipCockpit,     // 飞船驾驶舱
    Outpost,         // 前哨站
    CapitalCity,     // 首都
    HiddenCache      // 隐藏复活点（探索发现）
};

USTRUCT(BlueprintType)
struct FRespawnPointData
{
    GENERATED_BODY()

    // 唯一 ID
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString PointID;

    // 显示名称
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString DisplayName;

    // 位置
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Location;

    // 朝向
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRotator Rotation;

    // 所属星球
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString PlanetID;

    // 复活点类型
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ERespawnType Type = ERespawnType::PlanetSurface;

    // 是否解锁
    UPROPERTY(BlueprintReadWrite)
    bool bUnlocked = false;

    // 解锁条件描述
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString UnlockCondition;

    // 复活时恢复 HP 比例
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "1"))
    float HealOnRespawn = 0.5f;

    // 复活时恢复护盾比例
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "1"))
    float ShieldRestoreOnRespawn = 0.3f;

    // 复活时消耗（弹药/消耗品）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bConsumeResourcesOnRespawn = false;

    // 是否为安全区（复活后无敌 5 秒）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bSafeZone = true;

    // 安全时间（秒）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "30"))
    float SafeTime = 5.f;

    // 派系归属（决定谁可以用）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString OwningFaction;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRespawnPointUnlocked, FString, PointID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerRespawned, FString, PointID, APawn*, NewPawn);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAllRespawnPointsLocked);

UCLASS(BlueprintType)
class ARespawnManager : public AActor
{
    GENERATED_BODY()

public:
    ARespawnManager();

    // —— 复活点数据 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Respawn")
    TArray<FRespawnPointData> RespawnPoints;

    // 默认复活延迟
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Respawn")
    float DefaultRespawnDelay = 10.f;

    // 自动选择最近复活点
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Respawn")
    bool bAutoSelectNearest = true;

    // 复活时是否恢复维生指标
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Respawn")
    bool bRestoreVitalsOnRespawn = true;

    // 最大复活次数（0=无限）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Respawn")
    int32 MaxRespawns = 0;

    // —— 事件 ——
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnRespawnPointUnlocked OnRespawnPointUnlocked;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnPlayerRespawned OnPlayerRespawned;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnAllRespawnPointsLocked OnAllRespawnPointsLocked;

    // —— 接口 ——

    // 注册复活点（由星球/建筑生成时调用）
    UFUNCTION(BlueprintCallable, Category = "Respawn")
    void RegisterRespawnPoint(const FRespawnPointData& PointData);

    // 解锁复活点
    UFUNCTION(BlueprintCallable, Category = "Respawn")
    bool UnlockRespawnPoint(const FString& PointID, const FString& UnlockerID);

    // 锁定复活点
    UFUNCTION(BlueprintCallable, Category = "Respawn")
    void LockRespawnPoint(const FString& PointID);

    // 获取所有可用复活点
    UFUNCTION(BlueprintCallable, Category = "Respawn")
    TArray<FRespawnPointData> GetAvailableRespawnPoints(const FString& PlayerID) const;

    // 获取最近的复活点
    UFUNCTION(BlueprintCallable, Category = "Respawn")
    FRespawnPointData GetNearestRespawnPoint(const FVector& Location) const;

    // 执行复活
    UFUNCTION(BlueprintCallable, Category = "Respawn")
    APawn* ExecuteRespawn(const FString& PlayerID, const FString& PointID);

    // 快速复活（自动选最近）
    UFUNCTION(BlueprintCallable, Category = "Respawn")
    APawn* QuickRespawn(const FString& PlayerID, const FVector& CurrentLocation);

    // 设置复活点（玩家主动）
    UFUNCTION(BlueprintCallable, Category = "Respawn")
    bool SetCustomRespawnPoint(const FString& PlayerID, const FVector& Location,
        const FRotator& Rotation);

    // 获取复活次数
    UFUNCTION(BlueprintCallable, Category = "Respawn")
    int32 GetRespawnCount(const FString& PlayerID) const;

    // 重置复活次数（检查点）
    UFUNCTION(BlueprintCallable, Category = "Respawn")
    void ResetRespawnCount(const FString& PlayerID);

    // 网络同步
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void BeginPlay() override;

private:
    // 玩家复活次数记录
    UPROPERTY(Replicated)
    TMap<FString, int32> PlayerRespawnCounts;

    // 玩家自定义复活点
    UPROPERTY(Replicated)
    TMap<FString, FRespawnPointData> CustomRespawnPoints;

    // 生成复活特效
    void SpawnRespawnEffects(const FVector& Location);

    // 恢复玩家状态
    void RestorePlayerState(APawn* NewPawn, const FRespawnPointData& Point);

    // 应用安全时间（无敌）
    void ApplySafeTime(APawn* NewPawn, float Duration);
};
