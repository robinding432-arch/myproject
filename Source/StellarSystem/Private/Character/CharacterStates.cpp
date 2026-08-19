// CharacterStates.cpp
#include "Character/CharacterStates.h"
#include "Net/UnrealNetwork.h"

UCharacterStateMachine::UCharacterStateMachine()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UCharacterStateMachine::BeginPlay()
{
    Super::BeginPlay();
    PreviousState = CurrentState;
    OnStateEntered.Broadcast(CurrentState);
}

void UCharacterStateMachine::TickComponent(float Dt, ELevelTick Tick, FActorComponentTickFunction* Fn)
{
    Super::TickComponent(Dt, Tick, Fn);
    StateTime += Dt;
    TickState(Dt);
}

void UCharacterStateMachine::ServerRequestState_Implementation(ECharacterState NewState, EStateChangeReason Reason)
{
    if (!CanEnterState(NewState)) return;

    ECharacterState Old = CurrentState;
    ExitState(Old, Reason);
    EnterState(NewState, Reason);

    PreviousState = Old;
    StateTime = 0.f;
    OnStateChanged.Broadcast(Old, NewState, Reason);
}

bool UCharacterStateMachine::CanEnterState(ECharacterState NewState) const
{
    switch (NewState)
    {
        case ECharacterState::Dead:
            return CurrentState != ECharacterState::Dead;
        case ECharacterState::InShip:
            return CurrentState == ECharacterState::Walking || CurrentState == ECharacterState::Orbiting;
        case ECharacterState::EVA:
            return CurrentState == ECharacterState::Orbiting;
        default:
            return true;
    }
}

void UCharacterStateMachine::EnterState(ECharacterState NewState, EStateChangeReason Reason)
{
    CurrentState = NewState;
    OnStateEntered.Broadcast(NewState);
}

void UCharacterStateMachine::ExitState(ECharacterState OldState, EStateChangeReason Reason)
{
    OnStateExited.Broadcast(OldState);
}

void UCharacterStateMachine::TickState(float Dt)
{
    // 状态特定逻辑
    if (CurrentState == ECharacterState::Dead)
    {
        // 禁用输入
    }
}

void UCharacterStateMachine::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(UCharacterStateMachine, CurrentState);
}
