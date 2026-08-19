// ============================================================
// 路径: Source/StellarSystem/Public/Death/ShipInvalidationSystem.h
// 作用: 飞船索赔后失效 + 爆炸/残骸 N 秒后自动消失
// 修改于: v7.6 (原船货物转移/定时销毁/索赔后禁止登船)
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShipInvalidationSystem.generated.h"

class AShipPawn;

// 飞船失效原因
UENUM(BlueprintType)
enum class EShipInvalidationReason : uint8
{
    Claimed        UMETA(DisplayName = "Insurance Claimed (已索赔)"),
    Destroyed      UMETA(DisplayName = "Destroyed (被毁)"),
    Abandoned      UMETA(DisplayName = "Abandoned (遗弃)"),
    Impounded     UMETA(DisplayName = "Impounded (被扣押)")
};

// 残骸/失效飞船状态(增强: 货物/可搜刮物品)
USTRUCT(BlueprintType)
struct FInvalidatedShipState
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FName ShipID;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    EShipInvalidationReason Reason = EShipInvalidationReason::Destroyed;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float TimeOfDeath = 0.f;

    // ★ 关键: 失效总时长(到期销毁)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float DespawnTimer = 300.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float MaxDespawnTime = 300.f; // 原始值(用于UI显示)

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    bool bIsWreck = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    bool bCanBeClaimed = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FVector LastKnownLocation = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float HullAtDeath = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FName OriginalOwnerID;

    // ★ 新增: 货物是否还在(被毁时有概率保留)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    bool bCargoPreserved = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float CargoValueAtDeath = 0.f;

    // ★ 新增: 是否已通知客户端
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    bool bNotificationSent = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShipInvalidated, FName, ShipID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShipDespawned, FName, ShipID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWreckLooted, FName, ShipID, AActor*, Looter);
// ★ 新增: 货物转移事件
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnCargoTransferred, FName, OldShipID, FName, NewShipID, float, CargoValue);

UCLASS(BlueprintType)
class AShipInvalidationManager : public AActor
{
    GENERATED_BODY()

public:
    AShipInvalidationManager();

    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;

    // —— 失效时长(可配置) ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invalidation|Timing")
    float DefaultDespawnTime = 300.f; // 被毁残骸5分钟

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invalidation|Timing")
    float WreckDespawnTime = 600.f; // 可搜刮残骸10分钟

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invalidation|Timing")
    float ClaimedDespawnTime = 30.f; // 已索赔船30秒消失

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invalidation|Timing")
    float DestroyedDespawnTime = 120.f; // 爆炸残骸2分钟

    // —— 失效表现 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invalidation|Visual")
    bool bDisableMovementOnInvalid = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invalidation|Visual")
    bool bCollapseWreckMesh = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invalidation|Visual")
    bool bFadeOutBeforeDespawn = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invalidation|Visual")
    float FadeOutDuration = 5.f;

    // ★ 新增: 被毁时货物保留概率
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invalidation|Cargo")
    float CargoPreserveChance = 0.3f; // 30% 货物可搜刮

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invalidation|Cargo")
    bool bTransferCargoOnClaim = true; // 索赔时转移货物到新船

    // —— 玩家提示 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invalidation|Notification")
    bool bNotifyOwnerOnInvalid = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invalidation|Notification")
    bool bNotifyOwnerOnDespawn = true;

    // ========== 核心接口 ==========

    // ★ 关键修复: 飞船被毁 → 失效 + 货物处理 + 计时销毁
    UFUNCTION(BlueprintCallable, Category = "Invalidation")
    void OnShipDestroyed(AShipPawn* Ship, const FName& DestroyerID = NAME_None);

    // ★ 关键修复: 飞船被索赔 → 立即失效 + 货物转移 + 快速消失
    UFUNCTION(BlueprintCallable, Category = "Invalidation")
    void OnShipClaimed(AShipPawn* OldShip, AShipPawn* NewShip, FName NewShipID);

    // 玩家主动遗弃飞船
    UFUNCTION(BlueprintCallable, Category = "Invalidation")
    void OnShipAbandoned(AShipPawn* Ship);

    // 管理员扣押
    UFUNCTION(BlueprintCallable, Category = "Invalidation")
    void ImpoundShip(AShipPawn* Ship, FString Reason);

    // ========== 查询 ==========
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Invalidation")
    bool IsShipValid(AShipPawn* Ship) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Invalidation")
    bool CanPlayerBoard(AShipPawn* Ship, const FName& PlayerID) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Invalidation")
    FInvalidatedShipState GetShipState(FName ShipID) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Invalidation")
    TArray<FInvalidatedShipState> GetAllWrecks() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Invalidation")
    float GetRemainingDespawnTime(FName ShipID) const;

    // ========== 重生/复活 ==========
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Invalidation")
    void Server_SpawnReplacementShip(FName OwnerID, FName ShipClassID, FVector SpawnLocation, const struct FShipSavedConfig& Config);

    // ========== 搜刮 ==========
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Invalidation")
    void Server_LootWreck(AController* Player, FName WreckShipID);

    // ★ 新增: 转移货物到新船
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Invalidation")
    void Server_TransferCargoToNewShip(AController* Player, FName OldShipID, FName NewShipID);

    // ========== 事件 ==========
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnShipInvalidated OnShipInvalidatedEvent;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnShipDespawned OnShipDespawnedEvent;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnWreckLooted OnWreckLootedEvent;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnCargoTransferred OnCargoTransferredEvent;

    // ========== 网络 ==========
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

private:
    UPROPERTY(Replicated)
    TMap<FName, FInvalidatedShipState> InvalidatedShips;

    UPROPERTY()
    TMap<FName, AActor*> WreckActors;

    // ★ 新增: 待销毁队列(带精确计时)
    struct FDespawnEntry
    {
        FName ShipID;
        float TimeRemaining;
        bool bIsActorValid;
    };
    UPROPERTY()
    TArray<FDespawnEntry> DespawnQueue;

    void TickDespawnTimers(float Dt);
    void DespawnShip(FName ShipID);
    void PlayWreckEffects(AShipPawn* Ship);
    void FadeOutActor(AActor* Actor, float Duration);
    void NotifyOwner(FName ShipID, FString Message);

    // ★ 新增: 转移货物
    void TransferCargo(AShipPawn* FromShip, AShipPawn* ToShip);

    // 引用
    class UCargoComponent* GetShipCargo(AShipPawn* Ship) const;
};
