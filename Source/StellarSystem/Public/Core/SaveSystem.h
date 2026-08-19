// SaveSystem.h
// 存档系统：自动 5min + 多槽位 + JSON 序列化 + Steam 云存档
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SaveSystem.generated.h"

class UInventoryComponent;
class UCurrencyComponent;
class UVitalsComponent;
class UShipLoadoutComponent;
class AStellarGameMode;
class USteamIntegration;

// 单槽存档数据
UCLASS()
class UStellarSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    // 元信息
    UPROPERTY(VisibleAnywhere)
    FString PlayerName;

    UPROPERTY(VisibleAnywhere)
    FDateTime SaveTime;

    UPROPERTY(VisibleAnywhere)
    FString CurrentPlanetID;

    UPROPERTY(VisibleAnywhere)
    FString CurrentShipID;

    // 角色
    UPROPERTY(VisibleAnywhere)
    FName CharacterSeed;

    UPROPERTY(VisibleAnywhere)
    float Health = 100.f;

    UPROPERTY(VisibleAnywhere)
    float Oxygen = 100.f;

    UPROPERTY(VisibleAnywhere)
    float Energy = 100.f;

    UPROPERTY(VisibleAnywhere)
    float Hunger = 0.f;

    UPROPERTY(VisibleAnywhere)
    float Thirst = 0.f;

    UPROPERTY(VisibleAnywhere)
    float Radiation = 0.f;

    UPROPERTY(VisibleAnywhere)
    FVector CharacterLocation = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere)
    FRotator CharacterRotation = FRotator::ZeroRotator;

    // 货币
    UPROPERTY(VisibleAnywhere)
    TMap<FName, int32> Currencies;

    // 背包
    UPROPERTY(VisibleAnywhere)
    TArray<FInventoryItem> InventoryItems;

    UPROPERTY(VisibleAnywhere)
    TMap<FName, FName> EquippedItems;

    // 飞船
    UPROPERTY(VisibleAnywhere)
    TArray<FShipComponentParams> ShipComponents;

    UPROPERTY(VisibleAnywhere)
    FVector ShipLocation = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere)
    FRotator ShipRotation = FRotator::ZeroRotator;

    UPROPERTY(VisibleAnywhere)
    int32 ShipSeed = 0;

    // 弹药
    UPROPERTY(VisibleAnywhere)
    TMap<FName, int32> AmmoStock;

    // 消耗品
    UPROPERTY(VisibleAnywhere)
    TMap<FName, int32> ConsumableStock;

    UPROPERTY(VisibleAnywhere)
    TMap<int32, FName> HotbarSlots;

    // 已知星球
    UPROPERTY(VisibleAnywhere)
    TArray<FName> DiscoveredPlanets;

    // 星系种子
    UPROPERTY(VisibleAnywhere)
    int32 GalaxySeed = 0;

    // 游戏时间
    UPROPERTY(VisibleAnywhere)
    float GameTimeSeconds = 0.f;

    // 天气状态
    UPROPERTY(VisibleAnywhere)
    TArray<uint8> WeatherEventType;

    UPROPERTY(VisibleAnywhere)
    TArray<float> WeatherIntensity;

    // 成就进度（镜像 Steam）
    UPROPERTY(VisibleAnywhere)
    TArray<uint8> UnlockedAchievements;
};

// 存档管理器（GameMode 持有）
UCLASS()
class USaveManager : public UObject
{
    GENERATED_BODY()

public:
    USaveManager();

    // 槽位（0~9）
    static const int32 MaxSlots = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AutoSaveInterval = 300.f; // 5 分钟

    UPROPERTY(VisibleAnywhere)
    float AutoSaveTimer = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    int32 CurrentSlot = 0;

    // Steam 云存档（v5.0）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bUseSteamCloud = true;

    // ---- API ----
    UFUNCTION(BlueprintCallable)
    bool SaveGame(int32 Slot = -1);

    UFUNCTION(BlueprintCallable)
    bool LoadGame(int32 Slot);

    UFUNCTION(BlueprintCallable)
    TArray<FString> GetSaveSlotList() const;

    UFUNCTION(BlueprintCallable)
    bool DeleteSave(int32 Slot);

    UFUNCTION(BlueprintCallable)
    bool HasSave(int32 Slot) const;

    UFUNCTION(BlueprintCallable)
    FDateTime GetSaveTime(int32 Slot) const;

    // 自动存档 Tick
    UFUNCTION(BlueprintCallable)
    void TickAutoSave(float Dt);

    // 快速存档/读档
    UFUNCTION(BlueprintCallable)
    bool QuickSave();

    UFUNCTION(BlueprintCallable)
    bool QuickLoad();

    // ---- 内部 ----
    FString GetSlotName(int32 Slot) const;
    void PopulateFromWorld(UStellarSaveGame* Save);
    void ApplyToWorld(UStellarSaveGame* Save);

    // 序列化到 JSON 字节（用于 Steam 云）
    TArray<uint8> SerializeToJSON(UStellarSaveGame* Save) const;
    UStellarSaveGame* DeserializeFromJSON(const TArray<uint8>& Data) const;

    UPROPERTY()
    UWorld* WorldRef = nullptr;

private:
    // 尝试用 Steam 云存档
    bool TrySaveToSteamCloud(int32 Slot, const TArray<uint8>& Data);
    bool TryLoadFromSteamCloud(int32 Slot, TArray<uint8>& OutData);

    // 本地存档路径
    FString GetLocalSavePath(int32 Slot) const;
};
