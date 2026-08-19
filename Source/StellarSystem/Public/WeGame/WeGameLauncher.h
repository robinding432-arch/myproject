// WeGameLauncher.h
// v6.9 — WeGame 平台启动器
// 在 GameInstance 初始化时调用，负责：
//   1) 初始化 Rail SDK
//   2) 检查 WeGame 客户端状态
//   3) 获取 SessionTicket
//   4) 驱动每帧事件循环
//   5) 监听系统状态变化（客户端退出→游戏强退）

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WeGameLauncher.generated.h"

#ifndef WITH_WEGAME
#define WITH_WEGAME 0
#endif

#if WITH_WEGAME
#include "rail/sdk/rail_api.h"
#endif

// —— 系统状态 ——
UENUM(BlueprintType)
enum class EWeGameSystemState : uint8
{
    Unknown       UMETA(DisplayName = "Unknown"),
    ClientRunning UMETA(DisplayName = "Client Running"),
    ClientExiting UMETA(DisplayName = "Client Exiting"),
    ClientOffline UMETA(DisplayName = "Client Offline"),
    SDKError       UMETA(DisplayName = "SDK Error")
};

// —— 委托 ——
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeGameSystemStateChanged, EWeGameSystemState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWeGameClientExit);

UCLASS()
class STELLARSYSTEM_API UWeGameLauncher : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ---- 初始化 ----
    UFUNCTION(BlueprintCallable, Category = "WeGame|Launcher")
    bool InitializeForWeGame(int32 AppId, const FString& AppVersion);

    // ---- 每帧驱动 ----
    void Tick(float DeltaTime);

    // ---- 状态查询 ----
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WeGame|Launcher")
    EWeGameSystemState GetSystemState() const { return CurrentState; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WeGame|Launcher")
    bool IsWeGameEnvironment() const { return bIsWeGameEnvironment; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WeGame|Launcher")
    bool IsClientRunning() const { return bClientRunning; }

    // ---- 强退游戏（由防沉迷或客户端退出触发）----
    UFUNCTION(BlueprintCallable, Category = "WeGame|Launcher")
    void ForceExitGame(const FString& Reason);

    // ---- 委托 ----
    UPROPERTY(BlueprintAssignable, Category = "WeGame|Launcher|Events")
    FOnWeGameSystemStateChanged OnSystemStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "WeGame|Launcher|Events")
    FOnWeGameClientExit OnClientExit;

    // ---- 环境检测 ----
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WeGame|Launcher")
    static bool DetectWeGameEnvironment();

private:
    // 内部状态
    EWeGameSystemState CurrentState = EWeGameSystemState::Unknown;
    bool bIsWeGameEnvironment = false;
    bool bClientRunning = false;
    bool bSDKInitialized = false;

    // 计时器
    float EventPollTimer = 0.0f;
    static constexpr float PollInterval = 0.1f; // 100ms

    // 应用信息
    int32 CachedAppId = 0;
    FString CachedAppVersion;

    // 处理系统状态变化
    void HandleSystemStateChanged(EWeGameSystemState NewState);

    // 检查 WeGame 客户端进程
    bool CheckWeGameClientProcess() const;
};
