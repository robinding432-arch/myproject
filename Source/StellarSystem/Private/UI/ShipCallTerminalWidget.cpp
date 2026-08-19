// ============================================================
// ShipCallTerminalWidget.cpp
// 呼船终端 UI 实现
// ============================================================

#include "UI/ShipCallTerminalWidget.h"
#include "Station/PlanetarySpaceport.h"
#include "Ship/InsuranceSystem.h"
#include "Ship/ShipPawn.h"
#include "Character/MyCharacter.h"
#include "Core/StellarPlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

void UShipCallTerminalWidget::InitializeTerminal(APlanetarySpaceport* Spaceport, const FString& PlayerID)
{
    BoundSpaceport = Spaceport;
    CurrentPlayerID = PlayerID;

    // 获取保险管理器
    if (GetWorld())
    {
        InsuranceMgr = GetWorld()->GetSubsystem<UInsuranceManager>();
    }

    RefreshShipList();
}

TArray<FInsurancePolicy> UShipCallTerminalWidget::GetAvailableShips() const
{
    if (!InsuranceMgr) return TArray<FInsurancePolicy>();

    AController* PlayerController = nullptr;
    if (BoundSpaceport)
    {
        // 查找当前玩家控制器
        TArray<AActor*> Controllers;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), AController::StaticClass(), Controllers);
        for (AActor* A : Controllers)
        {
            AController* C = Cast<AController>(A);
            if (C && C->GetName() == CurrentPlayerID)
            {
                PlayerController = C;
                break;
            }
        }
    }

    if (PlayerController)
    {
        return InsuranceMgr->GetPlayerPolicies(PlayerController);
    }
    return TArray<FInsurancePolicy>();
}

void UShipCallTerminalWidget::CallShip(const FName& ShipID)
{
    if (!BoundSpaceport) return;

    // 查找玩家控制器
    AController* PC = nullptr;
    TArray<AActor*> Controllers;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AController::StaticClass(), Controllers);
    for (AActor* A : Controllers)
    {
        AController* C = Cast<AController>(A);
        if (C && C->GetName() == CurrentPlayerID)
        {
            PC = C;
            break;
        }
    }

    if (PC)
    {
        // 获取第一个可用机库
        TArray<FPersonalHangarDef> Hangars = BoundSpaceport->GetAvailableHangars();
        FName HangarID = (Hangars.Num() > 0) ? Hangars[0].HangarID : FName(TEXT("Hangar_01"));

        // 通过 RPC 呼船
        // 需要先获取 Character
        AMyCharacter* Char = Cast<AMyCharacter>(PC->GetPawn());
        if (Char)
        {
            BoundSpaceport->Server_CallShip(Char, ShipID, HangarID);
            CurrentState = ETerminalState::CallingShip;
        }
    }
}

void UShipCallTerminalWidget::ExpediteShipCall(const FName& ShipID, float Fee)
{
    // 付费加速呼船
    CallShip(ShipID);
    // 实际实现中会从玩家账户扣费
}

void UShipCallTerminalWidget::ClaimNewShip(const FName& PolicyID)
{
    if (!InsuranceMgr) return;

    AController* PC = nullptr;
    TArray<AActor*> Controllers;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AController::StaticClass(), Controllers);
    for (AActor* A : Controllers)
    {
        AController* C = Cast<AController>(A);
        if (C && C->GetName() == CurrentPlayerID)
        {
            PC = C;
            break;
        }
    }

    if (PC)
    {
        InsuranceMgr->Server_FileClaim(PC, PolicyID, TEXT("Terminal Claim"));
        CurrentState = ETerminalState::ProcessingClaim;
    }
}

ETerminalState UShipCallTerminalWidget::GetCallStatus() const
{
    return CurrentState;
}

float UShipCallTerminalWidget::GetEstimatedArrival() const
{
    if (!BoundSpaceport) return 0.f;

    AController* PC = nullptr;
    TArray<AActor*> Controllers;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AController::StaticClass(), Controllers);
    for (AActor* A : Controllers)
    {
        AController* C = Cast<AController>(A);
        if (C && C->GetName() == CurrentPlayerID)
        {
            PC = C;
            break;
        }
    }

    if (PC)
    {
        TArray<FShipCallRequest> Calls = BoundSpaceport->GetPendingShipCalls(FName(*CurrentPlayerID));
        if (Calls.Num() > 0)
        {
            return Calls[0].EstimatedArrivalTime;
        }
    }
    return 0.f;
}

void UShipCallTerminalWidget::CancelShipCall()
{
    CurrentState = ETerminalState::Idle;
    ActiveShipCallID = NAME_None;
}

void UShipCallTerminalWidget::RefreshShipList()
{
    LastRefreshTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    // 蓝图会监听此调用以刷新列表显示
}

FString UShipCallTerminalWidget::GetHangarInfo() const
{
    if (!BoundSpaceport) return TEXT("No Spaceport Bound");

    FString Info = FString::Printf(TEXT("Spaceport: %s\n"), *BoundSpaceport->SpaceportName);
    Info += FString::Printf(TEXT("Hangars Available: %d\n"), BoundSpaceport->GetAvailableHangars().Num());
    Info += FString::Printf(TEXT("Public Slots: %d"), BoundSpaceport->PublicHangarSlots);
    return Info;
}

void UShipCallTerminalWidget::SwitchHangar(const FName& HangarID)
{
    // 切换当前操作的机库
    // 蓝图层处理 UI 更新
    RefreshShipList();
}

void UShipCallTerminalWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!GetWorld()) return;

    float CurrentTime = GetWorld()->GetTimeSeconds();

    // 定期刷新
    if (CurrentTime - LastRefreshTime >= RefreshInterval)
    {
        RefreshShipList();
    }

    // 更新呼船状态
    UpdateCallStatus(InDeltaTime);
}

void UShipCallTerminalWidget::UpdateCallStatus(float DeltaTime)
{
    if (CurrentState == ETerminalState::CallingShip ||
        CurrentState == ETerminalState::ShipEnRoute)
    {
        float ETA = GetEstimatedArrival();
        if (ETA <= 0.f)
        {
            CurrentState = ETerminalState::ShipArrived;
            OnStateChanged.Broadcast((uint8)CurrentState);
        }
        else
        {
            CurrentState = ETerminalState::ShipEnRoute;
        }
    }
}

void UShipCallTerminalWidget::ShowError(const FString& ErrorMessage)
{
    CurrentState = ETerminalState::Error;
    OnTerminalError.Broadcast(ErrorMessage);
}
