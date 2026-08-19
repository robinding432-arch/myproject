// StellarGameMode.h
// 游戏总控：星系注册/飞船注册/存档/游戏规则/天气/Steam/音频
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "StellarGameMode.generated.h"

class USaveManager;
class UAudioManager;
class USteamIntegration;
class ASpaceWeather;
class AProceduralPlanet;
class AShipPawn;
class ASolarSystem;
class UStellarSaveGame;
class AAntiCheatManager;
class UPauseMenu;
class APerformanceManager;     // v6.6 性能管理器
class AObjectPoolManager;     // v6.6 对象池
class ANetworkOptimizer;      // v6.6 网络优化器
class AStartupOptimizer;      // v6.6 启动优化器

UCLASS()
class AStellarGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AStellarGameMode();

    virtual void Tick(float Dt) override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    // ---- 星系 ----
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
    int32 GalaxySeed = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated)
    TArray<AProceduralPlanet*> AllPlanets;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated)
    TArray<AShipPawn*> AllShips;

    // 太阳系引用（v5.0 新增）
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated)
    ASolarSystem* ActiveSolarSystem = nullptr;

    // ---- 玩家飞船 ----
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Replicated)
    AShipPawn* PlayerShip = nullptr;

    // ---- 存档 ----
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    USaveManager* SaveManager = nullptr;

    // ---- 音频（v5.0 新增）----
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UAudioManager* AudioMgr = nullptr;

    // ---- Steam（v5.0 新增）----
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    USteamIntegration* SteamInt = nullptr;

    // ---- 天气（v5.0 新增）----
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    ASpaceWeather* WeatherSystem = nullptr;

    // ---- 反作弊（v6.5 新增）----
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    AAntiCheatManager* AntiCheat = nullptr;

    // ---- 暂停菜单引用（v6.5 修正）----
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UPauseMenu* ActivePauseMenu = nullptr;

    // ---- 经济 ----
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float GlobalInflation = 1.f;

    // ---- API ----
    UFUNCTION(BlueprintCallable)
    void RegisterPlanet(AProceduralPlanet* Planet);

    UFUNCTION(BlueprintCallable)
    void UnregisterPlanet(AProceduralPlanet* Planet);

    UFUNCTION(BlueprintCallable)
    void RegisterShip(AShipPawn* Ship);

    UFUNCTION(BlueprintCallable)
    void UnregisterShip(AShipPawn* Ship);

    UFUNCTION(BlueprintCallable)
    AProceduralPlanet* FindNearestPlanetTo(const FVector& Location) const;

    UFUNCTION(BlueprintCallable)
    TArray<AProceduralPlanet*> GetPlanetsInRange(const FVector& Location, float Range) const;

    UFUNCTION(BlueprintCallable)
    AShipPawn* FindNearestShipTo(const FVector& Location, AShipPawn* Ignore = nullptr) const;

    // ---- 存档快捷 ----
    UFUNCTION(BlueprintCallable)
    bool SaveCurrentGame(int32 Slot = 0);

    UFUNCTION(BlueprintCallable)
    bool LoadGameSlot(int32 Slot);

    // ---- 游戏规则 ----
    UFUNCTION(BlueprintCallable)
    void SetGameRule(FName RuleName, float Value);

    UFUNCTION(BlueprintCallable, BlueprintPure)
    float GetGameRule(FName RuleName) const;

    // ---- 太阳系（v5.0）----
    UFUNCTION(BlueprintCallable)
    void SpawnSolarSystem(TSubclassOf<ASolarSystem> SystemClass);

    UFUNCTION(BlueprintCallable)
    void SetActiveSolarSystem(ASolarSystem* SolarSys);

    // ---- 反作弊（v6.5）----
    UFUNCTION(BlueprintCallable)
    void RegisterAntiCheatClient(APlayerController* PC, const FString& PlayerID,
        const FString& ClientVersion, const FString& ClientChecksum);

    UFUNCTION(BlueprintCallable)
    AAntiCheatManager* GetAntiCheatManager() const { return AntiCheat; }

    // ---- 暂停菜单（v6.5 修正）----
    // 由 PlayerController 在打开暂停菜单时调用
    UFUNCTION(BlueprintCallable)
    void NotifyPauseMenuOpened(UPauseMenu* PauseMenu);

    // 本地暂停状态（不影响其他玩家）
    UFUNCTION(BlueprintCallable)
    void OnLocalPauseStateChanged(bool bPaused);

    UFUNCTION(BlueprintPure)
    bool IsLocalPauseActive() const { return bLocalPauseActive; }

    // ---- 事件 ----
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlanetRegistered, AProceduralPlanet*, Planet);
    UPROPERTY(BlueprintAssignable)
    FOnPlanetRegistered OnPlanetRegistered;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShipRegistered, AShipPawn*, Ship);
    UPROPERTY(BlueprintAssignable)
    FOnShipRegistered OnShipRegistered;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGameSaved);
    UPROPERTY(BlueprintAssignable)
    FOnGameSaved OnGameSaved;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSolarSystemReady, ASolarSystem*, SolarSystem);
    UPROPERTY(BlueprintAssignable)
    FOnSolarSystemReady OnSolarSystemReady;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

protected:
    UPROPERTY(Replicated)
    TMap<FName, float> GameRules;

    void InitializeGalaxy();
    void HandleAutoSave(float Dt);

    // 初始化子系统
    void InitSubsystems();

    // v6.5：本地暂停状态（仅影响本地玩家，不冻结世界）
    UPROPERTY(Replicated)
    bool bLocalPauseActive = false;

    // v6.5：是否为多人游戏（由 GameMode 根据 NetMode 自动设置）
    UPROPERTY(Replicated)
    bool bIsMultiplayerGame = false;

    // ---- v6.6 性能优化 ----
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    APerformanceManager* PerfManager = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    AObjectPoolManager* PoolManager = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    ANetworkOptimizer* NetOptimizer = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    AStartupOptimizer* StartupOpt = nullptr;

    // 获取性能管理器（便捷方法）
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Performance")
    APerformanceManager* GetPerformanceManager() const { return PerfManager; }

    // 获取对象池管理器
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Performance")
    AObjectPoolManager* GetPoolManager() const { return PoolManager; }

    // 获取网络优化器
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Performance|Network")
    ANetworkOptimizer* GetNetworkOptimizer() const { return NetOptimizer; }

    // 获取启动优化器
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Performance|Startup")
    AStartupOptimizer* GetStartupOptimizer() const { return StartupOpt; }

    // 一键性能诊断
    UFUNCTION(BlueprintCallable, Category = "Performance")
    FString RunPerformanceDiagnostic() const;
};
