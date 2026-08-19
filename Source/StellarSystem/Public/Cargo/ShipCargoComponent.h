// ============================================================
// 路径: Source/StellarSystem/Public/Cargo/ShipCargoComponent.h
// 作用: 飞船货舱 —— 容量/重量/自动装卸/网络同步
// 依赖: Character/InventoryComponent.h
// 新增于: v7.4
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShipCargoComponent.generated.h"

class AShipPawn;

// 单个货物条目
USTRUCT(BlueprintType)
struct FCargoEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cargo")
    FName ItemID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cargo")
    int32 Quantity = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cargo")
    float UnitWeight = 1.f; // 单件重量(kg)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cargo")
    float UnitVolume = 1.f; // 单件体积(m³)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cargo")
    bool bIsPerishable = false; // 易腐(任务货)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cargo")
    float PerishTimer = 0.f; // 剩余保鲜时间(秒)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cargo")
    FName QuestBinding = NAME_None; // 绑定的任务ID(任务货不可丢弃)

    float TotalWeight() const { return Quantity * UnitWeight; }
    float TotalVolume() const { return Quantity * UnitVolume; }
};

// 货物装卸模式
UENUM(BlueprintType)
enum class ECargoTransferMode : uint8
{
    Manual      UMETA(DisplayName = "Manual (手动)"),
    AutoOnDock  UMETA(DisplayName = "Auto on Dock (靠港自动)"),
    AutoOnQuest UMETA(DisplayName = "Auto on Quest Submit (提交自动)")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCargoChanged, FName, ItemID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCargoFull, float, OverflowWeight);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCargoPerished, FName, ItemID);

UCLASS(ClassGroup=(Ship|Cargo), meta=(BlueprintSpawnableComponent))
class UShipCargoComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UShipCargoComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(float Dt, ELevelTick Tick, FActorComponentTickFunction* Fn) override;

    // —— 容量 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Cargo|Capacity")
    float MaxCargoWeight = 5000.f; // kg

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Cargo|Capacity")
    float MaxCargoVolume = 2500.f; // m³

    // —— 当前货物 ——
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Cargo|State")
    TArray<FCargoEntry> Cargo;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cargo|State")
    float CurrentWeight = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cargo|State")
    float CurrentVolume = 0.f;

    // —— 装卸模式 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cargo|Transfer")
    ECargoTransferMode TransferMode = ECargoTransferMode::AutoOnDock;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cargo|Transfer")
    float AutoLoadDurationPerUnit = 0.05f; // 每单位自动装载耗时(秒)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cargo|Transfer")
    float AutoUnloadDurationPerUnit = 0.05f;

    // ========== 查询 ==========
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Cargo")
    bool CanFit(FName ItemID, int32 Quantity, float UnitWeight, float UnitVolume) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Cargo")
    int32 GetItemQuantity(FName ItemID) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Cargo")
    float GetFreeWeight() const { return FMath::Max(0.f, MaxCargoWeight - CurrentWeight); }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Cargo")
    float GetFreeVolume() const { return FMath::Max(0.f, MaxCargoVolume - CurrentVolume); }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Cargo")
    float GetCargoFillPercent() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Cargo")
    bool HasQuestCargo(FName QuestID) const;

    // ========== 装载 ==========
    // 手动装载(玩家/任务调用)
    UFUNCTION(BlueprintCallable, Category = "Cargo")
    bool LoadCargo(FName ItemID, int32 Quantity, float UnitWeight, float UnitVolume,
                   FName QuestBinding = NAME_None, bool bPerishable = false, float PerishTime = 0.f);

    // 自动装载(靠港/任务提交时批量)
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Cargo")
    void Server_AutoLoadFromStation(FName StationID, const TArray<FCargoEntry>& Incoming);

    // ========== 卸载 ==========
    UFUNCTION(BlueprintCallable, Category = "Cargo")
    bool UnloadCargo(FName ItemID, int32 Quantity);

    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Cargo")
    void Server_AutoUnloadToStation(FName StationID, const TArray<FName>& ItemIDs);

    // 卸载全部(清空货舱)
    UFUNCTION(BlueprintCallable, Category = "Cargo")
    void UnloadAll();

    // ========== 任务集成 ==========
    // 任务提交时调用: 检查并扣除任务货物
    UFUNCTION(BlueprintCallable, Category = "Cargo")
    bool ConsumeQuestCargo(FName QuestID, FName ItemID, int32 RequiredQty);

    // ========== 网络复制 ==========
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

    // ========== 事件 ==========
    UPROPERTY(BlueprintAssignable, Category = "Cargo|Events")
    FOnCargoChanged OnCargoAdded;

    UPROPERTY(BlueprintAssignable, Category = "Cargo|Events")
    FOnCargoChanged OnCargoRemoved;

    UPROPERTY(BlueprintAssignable, Category = "Cargo|Events")
    FOnCargoFull OnCargoFull;

    UPROPERTY(BlueprintAssignable, Category = "Cargo|Events")
    FOnCargoPerished OnCargoPerished;

private:
    // 内部
    void RecalculateTotals();
    int32 FindCargoIndex(FName ItemID) const;
    void TickPerishables(float Dt);

    UPROPERTY()
    AShipPawn* OwnerShip = nullptr;

    // 易腐货物计时器
    TMap<FName, float> PerishTimers;
};
