// ============================================================
// 路径: Source/StellarSystem/Public/Cargo/ProximityDeliverySystem.h
// 作用: 近距离给付 —— 玩家/飞船靠近 NPC/空间站/货柜
//        按 E 自动交付任务物品并推进任务
// 依赖: AI/QuestSystemV2.h, Cargo/ShipCargoComponent.h, Station/PlanetarySpaceport.h
// 新增于: v7.4
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProximityDeliverySystem.generated.h"

class UBoxComponent;
class USphereComponent;
class UQuestManagerV2;
class UShipCargoComponent;
class APlayerController;

// 可交付目标类型
UENUM(BlueprintType)
enum class EDeliveryTargetType : uint8
{
    NPC             UMETA(DisplayName = "NPC (地面)"),
    StationTerminal UMETA(DisplayName = "空间站终端"),
    SpaceportDesk  UMETA(DisplayName = "太空港服务台"),
    CargoContainer  UMETA(DisplayName = "货柜/仓库"),
    DropPod         UMETA(DisplayName = "投放舱")
};

// 单个可交付任务提示
USTRUCT(BlueprintType)
struct FAvailableDelivery
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FName QuestID;

    UPROPERTY(BlueprintReadOnly)
    FName ObjectiveID;

    UPROPERTY(BlueprintReadOnly)
    FText DisplayName;

    UPROPERTY(BlueprintReadOnly)
    FName RequiredItemID;

    UPROPERTY(BlueprintReadOnly)
    int32 RequiredQuantity = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 PlayerHasQuantity = 0;

    UPROPERTY(BlueprintReadOnly)
    bool bCanDeliver = false; // 数量足够且距离足够
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNearbyDeliveryUpdated, const TArray<FAvailableDelivery>&, Available);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDeliverySucceeded, FName, QuestID, FName, ItemID, int32, Quantity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDeliveryFailed, FName, QuestID, FString, Reason, bool, bOutOfRange);

UCLASS(BlueprintType)
class AProximityDeliveryManager : public AActor
{
    GENERATED_BODY()

public:
    AProximityDeliveryManager();

    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;

    // —— 交互距离 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery|Range")
    float PlayerInteractRange = 300.f; // cm (约3米)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery|Range")
    float ShipDockRange = 1500.f; // cm (飞船对接距离)

    // —— 自动提交设置 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery|Auto")
    bool bAutoSubmitOnEnterRange = true; // 飞船靠港时自动提交任务货

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery|Auto")
    bool bRequirePlayerConfirm = true; // 玩家需按 E 确认(地面NPC)

    // —— 视觉提示 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery|UI")
    bool bShowInteractionPrompt = true; // 显示 "按 E 交付"

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery|UI")
    bool bShowProgressBar = true; // 显示距离进度条

    // ========== 玩家地面交付 ==========
    // 玩家靠近 NPC 时轮询可交付任务
    UFUNCTION(BlueprintCallable, Category = "Delivery")
    TArray<FAvailableDelivery> GetAvailableDeliveriesForPlayer(AController* Player, AActor* NPCActor) const;

    // 玩家按 E 执行交付
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Delivery")
    void Server_ExecutePlayerDelivery(AController* Player, AActor* NPCActor, FName QuestID, FName ObjectiveID);

    // ========== 飞船靠港自动交付 ==========
    // 飞船进入空间站对接范围时调用
    UFUNCTION(BlueprintCallable, Category = "Delivery")
    void OnShipDockedAtStation(AActor* Station, AShipPawn* Ship);

    // 自动从货舱卸下任务货物并完成目标
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Delivery")
    void Server_AutoSubmitCargoToStation(AActor* Station, AShipPawn* Ship, FName QuestID);

    // ========== 货物自动装船(接取任务时) ==========
    // 在 NPC/空间站接取货运任务后,自动把货物装到船上
    UFUNCTION(BlueprintCallable, Category = "Delivery")
    void AutoLoadCargoOnAccept(AController* Player, FName QuestID, AActor* PickupStation);

    // ========== 查询 ==========
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Delivery")
    bool IsPlayerInRange(AController* Player, AActor* TargetActor) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Delivery")
    bool IsShipInDockingRange(AShipPawn* Ship, AActor* Station) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Delivery")
    float GetDeliveryProgress(AController* Player, AActor* Target) const;

    // ========== 事件 ==========
    UPROPERTY(BlueprintAssignable, Category = "Delivery|Events")
    FOnNearbyDeliveryUpdated OnNearbyDeliveriesChanged;

    UPROPERTY(BlueprintAssignable, Category = "Delivery|Events")
    FOnDeliverySucceeded OnDeliverySucceeded;

    UPROPERTY(BlueprintAssignable, Category = "Delivery|Events")
    FOnDeliveryFailed OnDeliveryFailed;

private:
    // 内部
    UQuestManagerV2* GetQuestManager() const;
    UShipCargoComponent* GetShipCargo(AShipPawn* Ship) const;
    void TransferInventoryToCargo(AController* Player, FName ItemID, int32 Qty, UShipCargoComponent* Cargo);
    void CompleteDeliveryObjective(AController* Player, FName QuestID, FName ObjectiveID, int32 Qty);

    // 玩家距离检查缓存(避免每帧查)
    mutable TMap<TWeakObjectPtr<AController>, TWeakObjectPtr<AActor>> LastNearestTarget;
};
