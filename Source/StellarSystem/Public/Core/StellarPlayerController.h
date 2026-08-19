// ============================================================
// 路径: Source/StellarSystem/Public/Core/StellarPlayerController.h
// 作用: 玩家控制器 — 输入模式切换/Pawn 管理/Possess 切换
// 依赖: 无
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "StellarPlayerController.generated.h"

class AMyCharacter;
class AShipPawn;
class AStellarGameMode;

UENUM(BlueprintType)
enum class EControlMode : uint8
{
    Ground,     // 地面行走
    Orbit,      // 轨道飞行
    Ship,       // 飞船驾驶
    Station     // 空间站内
};

UCLASS()
class AStellarPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AStellarPlayerController();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // —— 控制模式 ——
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Control")
    EControlMode CurrentMode = EControlMode::Ground;

    UFUNCTION(BlueprintCallable, Category = "Control")
    void SwitchToCharacter(AMyCharacter* Character);

    UFUNCTION(BlueprintCallable, Category = "Control")
    void SwitchToShip(AShipPawn* Ship);

    UFUNCTION(BlueprintCallable, Category = "Control")
    void SwitchToOrbit();

    // —— 输入模式 ——
    void SetGameInputMode();
    void SetUIMode();

    // —— 当前操控的 Pawn 引用 ——
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    AMyCharacter* CurrentCharacter = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    AShipPawn* CurrentShip = nullptr;

    // —— 事件 ——
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnControlModeChanged, EControlMode, NewMode);
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnControlModeChanged OnControlModeChanged;

private:
    void UpdateInputMode();

    // v6.5：反作弊心跳
    FTimerHandle HeartbeatTimerHandle;
    void SendHeartbeat();
};
