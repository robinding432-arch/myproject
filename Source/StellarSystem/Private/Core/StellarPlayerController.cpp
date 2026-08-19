// ============================================================
// 路径: Source/StellarSystem/Private/Core/StellarPlayerController.cpp
// 作用: 玩家控制器实现
// 依赖: Core/StellarPlayerController.h
// ============================================================

#include "Core/StellarPlayerController.h"
#include "Character/MyCharacter.h"
#include "Ship/ShipPawn.h"
#include "UI/PauseMenu.h"
#include "Core/StellarGameMode.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Character.h"
#include "HAL/PlatformTime.h"
#include "Engine/World.h"
#include "TimerManager.h"

AStellarPlayerController::AStellarPlayerController()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AStellarPlayerController::BeginPlay()
{
    Super::BeginPlay();
    SetGameInputMode();

    // v6.5：通知 GameMode 本玩家已就绪（用于反作弊注册）
    if (AStellarGameMode* GM = Cast<AStellarGameMode>(GetWorld()->GetAuthGameMode()))
    {
        FString PlayerID = GetName();
        FString Version = TEXT("1.0.0"); // 从配置读取
        FString Checksum = TEXT("");   // 客户端计算自己的校验和

        GM->RegisterAntiCheatClient(this, PlayerID, Version, Checksum);

        // 启动心跳（每 10 秒向反作弊系统报告）
        GetWorld()->GetTimerManager().SetTimerForNextTick([this, GM]()
        {
            // 延迟到下一帧，确保一切就绪
            GetWorld()->GetTimerManager().SetTimer(
                HeartbeatTimerHandle, this,
                &AStellarPlayerController::SendHeartbeat, 10.f, true);
        });
    }
}

void AStellarPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
    // 停止心跳
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(HeartbeatTimerHandle);
    }

    // 通知反作弊系统注销
    if (AStellarGameMode* GM = Cast<AStellarGameMode>(GetWorld()->GetAuthGameMode()))
    {
        if (GM->AntiCheat)
        {
            GM->AntiCheat->Server_UnregisterPlayer(this);
        }
    }

    Super::EndPlay(Reason);
}

void AStellarPlayerController::SendHeartbeat()
{
    if (!HasAuthority()) return;

    AStellarGameMode* GM = Cast<AStellarGameMode>(GetWorld()->GetAuthGameMode());
    if (!GM || !GM->AntiCheat) return;

    FString Checksum = TEXT(""); // 实际项目中应计算关键内存区域的 CRC32
    float ClientTime = FPlatformTime::Seconds();
    GM->AntiCheat->Server_Heartbeat(this, Checksum, ClientTime);
}

void AStellarPlayerController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void AStellarPlayerController::SwitchToCharacter(AMyCharacter* Character)
{
    if (!Character) return;

    Possess(Character);
    CurrentCharacter = Character;
    CurrentShip = nullptr;
    CurrentMode = EControlMode::Ground;

    SetGameInputMode();
    OnControlModeChanged.Broadcast(CurrentMode);

    UE_LOG(LogTemp, Log, TEXT("[PC] Switched to Character"));
}

void AStellarPlayerController::SwitchToShip(AShipPawn* Ship)
{
    if (!Ship) return;

    Possess(Ship);
    CurrentShip = Ship;
    CurrentCharacter = nullptr;
    CurrentMode = EControlMode::Ship;

    SetGameInputMode();
    OnControlModeChanged.Broadcast(CurrentMode);

    UE_LOG(LogTemp, Log, TEXT("[PC] Switched to Ship"));
}

void AStellarPlayerController::SwitchToOrbit()
{
    CurrentMode = EControlMode::Orbit;
    OnControlModeChanged.Broadcast(CurrentMode);
}

void AStellarPlayerController::SetGameInputMode()
{
    FInputModeGameOnly GameMode;
    SetInputMode(GameMode);
    bShowMouseCursor = false;
}

void AStellarPlayerController::SetUIMode()
{
    FInputModeUIOnly UIMode;
    SetInputMode(UIMode);
    bShowMouseCursor = true;
}

void AStellarPlayerController::UpdateInputMode()
{
    if (CurrentMode == EControlMode::Ship || CurrentMode == EControlMode::Ground)
        SetGameInputMode();
    else
        SetUIMode();
}
