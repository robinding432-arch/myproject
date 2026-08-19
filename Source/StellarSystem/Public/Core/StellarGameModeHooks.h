// ============================================================
// 路径: Source/StellarSystem/Public/Core/StellarGameModeHooks.h
// 作用: v7.6 新增系统的 GameMode 接口钩子
// 新增于: v7.6
// 说明: 这些函数需要在 StellarGameMode 中实现,
//        作为各子系统的中央调度入口
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "StellarGameModeHooks.generated.h"

class AShipPawn;
struct FShipSavedConfig;

UCLASS()
class UStellarGameModeHooks : public UObject
{
    GENERATED_BODY()

public:
    // ========== 飞船管理 ==========

    // 注册飞船到全局表
    UFUNCTION(BlueprintCallable, Category = "GM|Ship")
    virtual void RegisterShip(AShipPawn* Ship) {}

    // 按 ID 查找飞船
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GM|Ship")
    virtual AShipPawn* FindShipByID(FName ShipID) const { return nullptr; }

    // 飞船被毁回调(通知保险系统)
    UFUNCTION(BlueprintCallable, Category = "GM|Ship")
    virtual void OnShipDestroyed(AShipPawn* Ship) {}

    // 飞船靠港回调(通知货运系统)
    UFUNCTION(BlueprintCallable, Category = "GM|Ship")
    virtual void OnShipDocked(AShipPawn* Ship, AActor* Station) {}

    // 按配置生成新飞船(索赔用)
    UFUNCTION(BlueprintCallable, Category = "GM|Ship")
    virtual AShipPawn* SpawnShipWithConfig(FName OwnerID, FName ShipClassID,
                                          const FVector& SpawnLocation,
                                          const FShipSavedConfig& Config) { return nullptr; }

    // 在飞船附近生成 Character
    UFUNCTION(BlueprintCallable, Category = "GM|Ship")
    virtual class AMyCharacter* SpawnCharacterNearShip(AShipPawn* Ship, const FVector& SpawnOffset) { return nullptr; }

    // ========== 玩家主权建筑 ==========

    // 检查是否是玩家主权建筑
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GM|Station")
    virtual bool IsPlayerOwnedStation(FName StationID) const { return false; }

    // 获取建筑所有者
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GM|Station")
    virtual FName GetStationOwnerID(FName StationID) const { return NAME_None; }

    // 获取玩家主权建筑位置
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GM|Station")
    virtual FVector GetPlayerStructureLocation(FName StructureID) const { return FVector::ZeroVector; }

    // 获取建筑内复活点
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GM|Station")
    virtual FVector GetPlayerStructureSpawnLocation(FName StructureID) const { return FVector::ZeroVector; }

    // 税收: 加到玩家建筑
    UFUNCTION(BlueprintCallable, Category = "GM|Station")
    virtual void AddTaxToPlayerStation(FName StationID, float Amount) {}

    // 税收: 加到 NPC 站点
    UFUNCTION(BlueprintCallable, Category = "GM|Station")
    virtual void AddTaxToStation(FName StationID, float Amount) {}

    // ========== 复活管理 ==========

    // 获取医院复活位置
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GM|Respawn")
    virtual FVector GetHospitalSpawnLocation(FName HospitalID) const { return FVector::ZeroVector; }

    // 获取最近复活点
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GM|Respawn")
    virtual FVector GetNearestRespawnLocation(const FVector& FromLocation) const { return FVector::ZeroVector; }

    // 生成 Character
    UFUNCTION(BlueprintCallable, Category = "GM|Respawn")
    virtual AMyCharacter* SpawnCharacterAt(AController* Player, const FVector& Location) { return nullptr; }

    // ========== 会议室 ==========

    // 检查玩家是否在会议室
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GM|Conference")
    virtual bool GetPlayerConferenceRoom(AController* Player, FName& OutRoomID) const { return false; }
};
