// ============================================================
// 路径: Source/StellarSystem/Public/Death/PlayerDeathSystem.h
// 作用: 玩家死亡 → 尸体立即消失 + 装备/背包完整转移 → 医院复活
// 修改于: v7.6 (确保100%物品转移/尸体立即消失/无敌期)
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PlayerDeathSystem.generated.h"

class AMyCharacter;
class ARespawnManager;
class UInventoryComponent;

// 死亡原因
UENUM(BlueprintType)
enum class EDeathCause : uint8
{
    Combat         UMETA(DisplayName = "Combat (战斗)"),
    Environment    UMETA(DisplayName = "Environment (环境)"),
    SpaceExposure UMETA(DisplayName = "Space Exposure (太空暴露)"),
    Drowning       UMETA(DisplayName = "Drowning (溺水)"),
    Falling        UMETA(DisplayName = "Falling (坠落)"),
    Explosion      UMETA(DisplayName = "Explosion (爆炸)"),
    ShipDestroyed  UMETA(DisplayName = "Ship Destroyed (飞船被毁)"),
    Suicide        UMETA(DisplayName = "Suicide (自杀)"),
    Admin          UMETA(DisplayName = "Admin (管理员)")
};

// 死亡时保存的玩家状态(用于复活时恢复)
USTRUCT(BlueprintType)
struct FPlayerDeathSnapshot
{
    GENERATED_BODY()

    UPROPERTY()
    FString PlayerID;

    UPROPERTY()
    FString PlayerName;

    UPROPERTY()
    EDeathCause Cause = EDeathCause::Combat;

    UPROPERTY()
    FName KillerID = NAME_None;

    UPROPERTY()
    FString KillerName;

    UPROPERTY()
    FVector DeathLocation = FVector::ZeroVector;

    UPROPERTY()
    FDateTime DeathTime;

    // —— 装备完整快照(100% 保留) ——
    UPROPERTY()
    TMap<uint8, FInventorySlot> EquippedItems;

    // —— 背包完整快照(100% 保留) ——
    UPROPERTY()
    TArray<FInventorySlot> InventorySlots;

    UPROPERTY()
    float InventoryWeight = 0.f;

    // —— 快捷栏 ——
    UPROPERTY()
    TArray<FName> HotbarItems;

    // —— 弹药(100% 保留) ——
    UPROPERTY()
    TMap<FName, int32> AmmoInventory;

    // —— 货币(部分保留,部分掉落) ——
    UPROPERTY()
    TMap<FName, float> CurrencyAtDeath;

    UPROPERTY()
    TMap<FName, float> CurrencyLost;

    // —— 维生指标 ——
    UPROPERTY()
    float HealthAtDeath = 0.f;
    UPROPERTY()
    float StaminaAtDeath = 0.f;
    UPROPERTY()
    float HungerAtDeath = 0.f;
    UPROPERTY()
    float ThirstAtDeath = 0.f;
    UPROPERTY()
    float OxygenAtDeath = 0.f;

    // ★ 关键: 死亡时身上的所有物品清单(用于验证100%转移)
    UPROPERTY()
    TArray<FName> AllItemsAtDeath;

    UPROPERTY()
    int32 TotalItemCountAtDeath = 0;

    // —— 尸体相关 ——
    UPROPERTY()
    bool bBodySpawned = false;

    UPROPERTY()
    FName CorpseActorID = NAME_None;

    UPROPERTY()
    float CorpseLifetime = 0.f;

    // —— 复活目标 ——
    UPROPERTY()
    FName RespawnPointID = NAME_None;

    UPROPERTY()
    bool bIsHospitalRespawn = false;

    // —— 飞船相关(死亡时所在的飞船) ——
    UPROPERTY()
    FName ShipAtDeath = NAME_None;

    UPROPERTY()
    bool bShipDestroyed = false;
};

// 死亡掉落规则(修改于v7.6: 医院复活保留一切)
USTRUCT(BlueprintType)
struct FDeathPenaltyRules
{
    GENERATED_BODY()

    // 医院复活: 只丢信用点(5%), 装备/背包/弹药全保留
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float CreditLossPercentHospital = 0.05f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float PremiumLossPercentHospital = 0.0f;

    // 野外复活: 丢更多
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float CreditLossPercentWilderness = 0.10f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float PremiumLossPercentWilderness = 0.05f;

    // 装备是否掉落(医院=否, 野外=小概率)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bDropEquippedItemsHospital = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bDropEquippedItemsWilderness = false; // 野外10%概率掉一件

    // 背包物品是否掉落
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bDropInventoryItemsHospital = false; // 医院: 全保留

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bDropInventoryItemsWilderness = true; // 野外: 掉30%

    // 弹药是否掉落
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bDropAmmoHospital = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bDropAmmoWilderness = true; // 野外掉50%

    // ★ 关键: 尸体立即消失
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CorpseLifetime = 0.f; // 0 = 立即消失

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bCorpseInstantDespawn = true; // ★ 立即消失(无延迟)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bFadeOutCorpse = true; // 淡出(极短)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CorpseFadeDuration = 0.5f; // 0.5秒淡出(几乎瞬间)

    // 尸体是否可搜刮
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bCorpseCanBeLooted = false; // PvE 不可搜刮

    // 医院复活是否免费
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bHospitalRespawnFree = true;

    // 非医院复活(野外)是否有惩罚
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float WildernessRespawnPenalty = 0.10f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerDied, const FPlayerDeathSnapshot&, Snapshot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerRespawnedWithGear, AMyCharacter*, NewChar, const FPlayerDeathSnapshot&, Snapshot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCorpseDespawned, FName, CorpseID);
// ★ 新增: 物品转移验证
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnInventoryTransferred, FName, PlayerID, int32, ItemCount, bool, bSuccess);

UCLASS(BlueprintType)
class APlayerDeathManager : public AActor
{
    GENERATED_BODY()

public:
    APlayerDeathManager();

    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;

    // —— 规则 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Rules")
    FDeathPenaltyRules PenaltyRules;

    // —— 尸体表现(修改于v7.6: 默认立即消失) ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Corpse")
    bool bSpawnCorpseActor = false; // ★ 默认关闭(立即消失)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Corpse")
    bool bFadeOutCorpse = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Corpse")
    float CorpseFadeDuration = 0.5f; // ★ 极短淡出

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Corpse")
    bool bPlayDeathAnimation = false; // ★ 关闭(立即消失不需要)

    // —— 复活规则 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Respawn")
    bool bPreferHospitalRespawn = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Respawn")
    float HospitalHealPercent = 0.8f; // 医院复活恢复 80% HP

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Respawn")
    float WildernessHealPercent = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Respawn")
    float RespawnInvulnerabilityTime = 5.f; // 复活无敌

    // ★ 新增: 复活延迟(让玩家看到死亡画面)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Respawn")
    float RespawnDelay = 3.f; // 3秒后自动复活

    // ========== 核心接口 ==========

    // 玩家死亡(被调用)
    UFUNCTION(BlueprintCallable, Category = "Death")
    void OnPlayerDied(AMyCharacter* DeadCharacter, EDeathCause Cause,
                      const FName& KillerID = NAME_None, const FString& KillerName = TEXT(""));

    // ★ 关键: 创建尸体(立即淡出/销毁, 无延迟)
    UFUNCTION(BlueprintCallable, Category = "Death")
    void SpawnCorpse(AMyCharacter* DeadCharacter, const FPlayerDeathSnapshot& Snapshot);

    // 立即销毁尸体(无延迟)
    UFUNCTION(BlueprintCallable, Category = "Death")
    void InstantDespawnCorpse(FName CorpseID);

    // 带淡出的销毁(极短)
    UFUNCTION(BlueprintCallable, Category = "Death")
    void FadeOutAndDespawnCorpse(FName CorpseID, float FadeTime);

    // ========== 复活(带完整物品转移) ==========
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Death|Respawn")
    void Server_RespawnAtHospital(AController* Player, FName HospitalPointID);

    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Death|Respawn")
    void Server_RespawnAtNearest(AController* Player);

    // ★ 新增: 在玩家主权建筑复活
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Death|Respawn")
    void Server_RespawnAtPlayerStructure(AController* Player, FName StructureID);

    // 强制复活(管理员)
    UFUNCTION(BlueprintCallable, Category = "Death|Admin")
    void AdminForceRespawn(AController* Player, FVector Location, FRotator Rotation);

    // ========== 查询 ==========
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Death")
    FPlayerDeathSnapshot GetDeathSnapshot(AController* Player) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Death")
    bool HasPendingDeath(AController* Player) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Death")
    TArray<FPlayerDeathSnapshot> GetAllPendingDeaths() const;

    // ========== 物品转移(核心) ==========
    // 死亡时: 保存完整快照(100% 捕获)
    UFUNCTION(BlueprintCallable, Category = "Death|Transfer")
    FPlayerDeathSnapshot CaptureDeathSnapshot(AMyCharacter* Character, EDeathCause Cause);

    // ★ 关键修复: 复活时100%恢复所有物品
    UFUNCTION(BlueprintCallable, Category = "Death|Transfer")
    void RestoreInventoryToNewPawn(AMyCharacter* NewCharacter, const FPlayerDeathSnapshot& Snapshot);

    // 计算死亡惩罚
    UFUNCTION(BlueprintCallable, Category = "Death|Transfer")
    void ApplyDeathPenalty(AMyCharacter* Character, FPlayerDeathSnapshot& Snapshot);

    // ★ 新增: 验证物品完整性
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Death|Transfer")
    bool VerifyInventoryIntegrity(AMyCharacter* Character, const FPlayerDeathSnapshot& Snapshot) const;

    // ========== 事件 ==========
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnPlayerDied OnPlayerDiedEvent;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnPlayerRespawnedWithGear OnPlayerRespawnedWithGearEvent;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnCorpseDespawned OnCorpseDespawnedEvent;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnInventoryTransferred OnInventoryTransferredEvent;

    // ========== 网络 ==========
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

private:
    UPROPERTY(Replicated)
    TMap<FString, FPlayerDeathSnapshot> PendingDeaths;

    UPROPERTY()
    TMap<FName, AActor*> CorpseActors;

    // ★ 复活计时器(延迟自动复活)
    struct FRespawnTimer
    {
        FString PlayerNetID;
        float TimeRemaining;
        FName RespawnType; // Hospital/Wilderness/Structure
        FName LocationID;
    };
    UPROPERTY()
    TArray<FRespawnTimer> PendingRespawns;

    void TickRespawnTimers(float Dt);

    // 内部辅助
    void SaveInventorySnapshot(AMyCharacter* Char, FPlayerDeathSnapshot& Out);
    void RestoreEquippedItems(AMyCharacter* Char, const FPlayerDeathSnapshot& Snap);
    void RestoreInventorySlots(AMyCharacter* Char, const FPlayerDeathSnapshot& Snap);
    void RestoreAmmo(AMyCharacter* Char, const FPlayerDeathSnapshot& Snap);
    void RestoreCurrency(AMyCharacter* Char, const FPlayerDeathSnapshot& Snap);
    void RestoreHotbar(AMyCharacter* Char, const FPlayerDeathSnapshot& Snap);
    void ApplyHospitalHealing(AMyCharacter* Char, float HealPercent);
    void GrantRespawnInvulnerability(AMyCharacter* Char, float Duration);

    ARespawnManager* GetRespawnManager() const;

    FName GenerateCorpseID();
    int32 CorpseIDCounter = 0;
};
