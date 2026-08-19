// ============================================================
// 路径: Source/StellarSystem/Public/Trade/CargoMissionSystem.h
// 作用: 货运任务系统 —— 在 NPC 空间站/太空港接取任务后
//        货物自动装船，飞到目的地后自动卸船完成任务
// 依赖: Cargo/ShipCargoComponent.h, Trade/PlayerTradeSystem.h
//       AI/QuestSystemV2.h, Station/PlanetarySpaceport.h
// 新增于: v7.5
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CargoMissionSystem.generated.h"

class AShipPawn;
class UShipCargoComponent;
class UQuestManagerV2;
class APlayerProximityGiveManager;

// 货运任务状态
UENUM(BlueprintType)
enum class ECargoMissionStatus : uint8
{
    Available   UMETA(DisplayName = "可接取"),
    Accepted    UMETA(DisplayName = "已接取(装货中)"),
    Loaded      UMETA(DisplayName = "已装船(飞行中)"),
    InTransit   UMETA(DisplayName = "运输中"),
    Arrived     UMETA(DisplayName = "已到达目的地"),
    Unloading   UMETA(DisplayName = "卸货中"),
    Completed   UMETA(DisplayName = "已完成"),
    Failed      UMETA(DisplayName = "失败"),
    Expired     UMETA(DisplayName = "已过期")
};

// 货运货物条目(任务用)
USTRUCT(BlueprintType)
struct FCargoMissionItem
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FName ItemID;

    UPROPERTY(BlueprintReadOnly)
    FString DisplayName;

    UPROPERTY(BlueprintReadOnly)
    int32 Quantity = 0;

    UPROPERTY(BlueprintReadOnly)
    float UnitWeight = 1.f; // kg

    UPROPERTY(BlueprintReadOnly)
    float UnitVolume = 1.f; // m³

    UPROPERTY(BlueprintReadOnly)
    bool bIsPerishable = false;

    UPROPERTY(BlueprintReadOnly)
    float PerishTime = 0.f; // 保鲜时间(秒), 0=不腐

    // 任务绑定(不可丢弃)
    UPROPERTY(BlueprintReadOnly)
    FName MissionID = NAME_None;
};

// 单个货运任务
USTRUCT(BlueprintType)
struct FCargoMission
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FName MissionID;

    UPROPERTY(BlueprintReadOnly)
    FString Title; // "运输钛矿: 地球 → 火星"

    UPROPERTY(BlueprintReadOnly)
    FString Description;

    // 起点/终点
    UPROPERTY(BlueprintReadOnly)
    FName PickupStationID;

    UPROPERTY(BlueprintReadOnly)
    FString PickupStationName;

    UPROPERTY(BlueprintReadOnly)
    FName DeliverStationID;

    UPROPERTY(BlueprintReadOnly)
    FString DeliverStationName;

    // 货物
    UPROPERTY(BlueprintReadOnly)
    TArray<FCargoMissionItem> Cargo;

    // 总重/总体积(自动算)
    UPROPERTY(BlueprintReadOnly)
    float TotalWeight = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float TotalVolume = 0.f;

    // 奖励
    UPROPERTY(BlueprintReadOnly)
    float CreditReward = 0.f;

    UPROPERTY(BlueprintReadOnly)
    TMap<FName, int32> ItemRewards; // 额外物品奖励

    UPROPERTY(BlueprintReadOnly)
    TMap<FName, int32> ReputationRewards; // 派系声望

    // 状态
    UPROPERTY(BlueprintReadOnly)
    ECargoMissionStatus Status = ECargoMissionStatus::Available;

    // 时限
    UPROPERTY(BlueprintReadOnly)
    float TimeLimit = 0.f; // 0=无限制

    UPROPERTY(BlueprintReadOnly)
    float TimeRemaining = 0.f;

    // 创建时间
    UPROPERTY(BlueprintReadOnly)
    float CreatedAt = 0.f;

    // 所属派系(影响声望)
    UPROPERTY(BlueprintReadOnly)
    FName OwningFaction = NAME_None;

    // 难度
    UPROPERTY(BlueprintReadOnly)
    int32 DifficultyTier = 1; // 1~5

    // 风险标签(经过危险区域?)
    UPROPERTY(BlueprintReadOnly)
    bool bHighRiskRoute = false;

    // 是否易腐(全局)
    UPROPERTY(BlueprintReadOnly)
    bool bAnyPerishable = false;
};

// 货运任务板(挂在 NPC 空间站/太空港)
USTRUCT(BlueprintType)
struct FCargoMissionBoard
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FName StationID;

    UPROPERTY(BlueprintReadOnly)
    TArray<FName> AvailableMissionIDs;

    UPROPERTY(BlueprintReadOnly)
    int32 MaxConcurrentMissions = 5; // 该站同时可接任务数
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnCargoMissionAccepted, FName, MissionID, FString, PlayerNetID, int32, CargoCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnCargoLoaded, FName, MissionID, FString, PlayerNetID, float, TotalWeight);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnCargoMissionCompleted, FName, MissionID, FString, PlayerNetID, float, Reward);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnCargoMissionFailed, FName, MissionID, FString, Reason, bool, bPerished);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCargoPerished, FName, MissionID, FName, ItemID);

// ============================================================
// 货运任务管理器（WorldSubsystem）
// ============================================================
UCLASS(BlueprintType)
class UCargoMissionManager : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;

    // —— 配置 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CargoMission|Config")
    float DefaultExpiryTime = 1800.f; // 默认30分钟时限

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CargoMission|Config")
    float PerishWarningThreshold = 0.2f; // 剩余20%时间时警告

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CargoMission|Config")
    int32 MaxPlayerActiveMissions = 3; // 每人最多同时3个货运任务

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CargoMission|Config")
    float AutoLoadDurationPerUnit = 0.1f; // 每件装货耗时

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CargoMission|Config")
    float AutoUnloadDurationPerUnit = 0.1f;

    // —— 距离 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CargoMission|Range")
    float PickupInteractRange = 500.f; // 接取/提交距离

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CargoMission|Range")
    float ShipDockRange = 2000.f; // 飞船靠港距离

    // —— 生成 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CargoMission|Generation")
    bool bAutoGenerateMissions = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CargoMission|Generation")
    int32 MissionsPerStation = 3; // 每站默认3个任务

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CargoMission|Generation")
    float MissionRegenerateInterval = 600.f; // 10分钟刷新一次

    // ========== 任务板(查询可用任务) ==========
    // 获取某站点的可用货运任务
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "CargoMission")
    TArray<FCargoMission> GetAvailableMissionsAtStation(FName StationID) const;

    // 获取所有站点的可用任务(星图用)
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "CargoMission")
    TArray<FCargoMission> GetAllAvailableMissions() const;

    // ========== 接取任务 ==========
    // 玩家在 NPC 站点接取货运任务
    // → 自动检测玩家飞船是否在旁边
    // → 自动把货物装到飞船货舱
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "CargoMission")
    void Server_AcceptCargoMission(AController* Player, FName MissionID);

    // ========== 自动装船(接取后) ==========
    // 检查飞船是否在站点附近 → 自动装货
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "CargoMission")
    void Server_AutoLoadMissionCargo(AController* Player, FName MissionID);

    // ========== 到达目的地 ==========
    // 飞船靠港时调用 → 检查是否有对应任务 → 自动卸货 → 完成任务
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "CargoMission")
    bool CheckArrivalAndComplete(AController* Player, FName StationID);

    // 飞船靠港时由 ProximityDelivery 调用
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "CargoMission")
    void Server_OnShipDockedAtStation(AController* Player, FName StationID, AShipPawn* Ship);

    // ========== 自动卸船(到达后) ==========
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "CargoMission")
    void Server_AutoUnloadMissionCargo(AController* Player, FName MissionID);

    // ========== 取消/放弃 ==========
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "CargoMission")
    void Server_AbandonMission(AController* Player, FName MissionID, bool bReturnCargo);

    // ========== 玩家任务查询 ==========
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "CargoMission")
    TArray<FCargoMission> GetPlayerActiveMissions(AController* Player) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "CargoMission")
    FCargoMission GetMission(FName MissionID) const;

    // 获取任务进度(0~1)
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "CargoMission")
    float GetMissionProgress(FName MissionID) const;

    // ========== 生成 ==========
    // 在指定站点生成货运任务
    UFUNCTION(BlueprintCallable, Category = "CargoMission")
    FName GenerateCargoMission(FName FromStation, FName ToStation, int32 DifficultyTier = 1);

    // 自动为所有站点生成任务网络
    UFUNCTION(BlueprintCallable, Category = "CargoMission")
    void GenerateAllStationMissions();

    // 注册任务板
    UFUNCTION(BlueprintCallable, Category = "CargoMission")
    void RegisterMissionBoard(FName StationID, int32 MaxConcurrent = 5);

    // ========== 事件 ==========
    UPROPERTY(BlueprintAssignable, Category = "CargoMission|Events")
    FOnCargoMissionAccepted OnMissionAccepted;

    UPROPERTY(BlueprintAssignable, Category = "CargoMission|Events")
    FOnCargoLoaded OnCargoLoaded;

    UPROPERTY(BlueprintAssignable, Category = "CargoMission|Events")
    FOnCargoMissionCompleted OnMissionCompleted;

    UPROPERTY(BlueprintAssignable, Category = "CargoMission|Events")
    FOnCargoMissionFailed OnMissionFailed;

    UPROPERTY(BlueprintAssignable, Category = "CargoMission|Events")
    FOnCargoPerished OnCargoPerished;

private:
    // 所有任务
    UPROPERTY()
    TMap<FName, FCargoMission> AllMissions;

    // 任务板
    UPROPERTY()
    TMap<FName, FCargoMissionBoard> MissionBoards;

    // 玩家活跃任务: NetID → MissionIDs
    UPROPERTY()
    TMap<FString, TArray<FName>> PlayerMissions;

    // 自动生成计时器
    float GenerationTimer = 0.f;

    // 内部方法
    void TickMissionTimers(float Dt);
    void TickPerishables(float Dt);
    void ExpireMission(FName MissionID, FString Reason);
    bool ValidatePlayerCanAccept(AController* Player, const FCargoMission& Mission) const;
    UShipCargoComponent* GetPlayerShipCargo(AController* Player) const;
    AController* FindPlayerByNetID(FString NetID) const;
    void GrantMissionRewards(AController* Player, const FCargoMission& Mission);
    void FailMission(AController* Player, FName MissionID, FString Reason, bool bPerished);
    FName GenerateMissionID() const;
    float CalculateDistance(FName FromStation, FName ToStation) const;
    float CalculateReward(const FCargoMission& Mission) const;
    void RemoveMissionFromBoard(FName StationID, FName MissionID);
};
