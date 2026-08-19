// CharacterStates.h
// 角色状态机：Walking/Orbiting/Transition/Dead
#pragma once

#include "CoreMinimal.h"
#include "CharacterStates.generated.h"

UENUM(BlueprintType)
enum class ECharacterState : uint8
{
    Walking,    // 地面行走
    Orbiting,   // 轨道飞行
    Transition, // 过渡动画中
    InShip,     // 在飞船内
    Dead,       // 死亡
    InStation,  // 在空间站内
    EVA         // 舱外活动（太空行走）
};

// 状态切换原因
UENUM(BlueprintType)
enum class EStateChangeReason : uint8
{
    UserInput,      // 玩家操作
    Death,          // 死亡
    Environment,    // 环境变化（如真空）
    ShipEnter,      // 进入飞船
    ShipExit,       // 离开飞船
    StationEnter,   // 进入空间站
    System          // 系统强制
};

// 状态机组件
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UCharacterStateMachine : public UActorComponent
{
    GENERATED_BODY()

public:
    UCharacterStateMachine();

    virtual void TickComponent(float Dt, ELevelTick Tick, FActorComponentTickFunction* Fn) override;
    virtual void BeginPlay() override;

    // 当前状态
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated)
    ECharacterState CurrentState = ECharacterState::Walking;

    // 上一状态（用于回退）
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    ECharacterState PreviousState = ECharacterState::Walking;

    // 状态持续时间
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float StateTime = 0.f;

    // 请求状态切换
    UFUNCTION(BlueprintCallable, Server, Reliable)
    void ServerRequestState(ECharacterState NewState, EStateChangeReason Reason);

    UFUNCTION(BlueprintCallable, Category = "State")
    bool CanEnterState(ECharacterState NewState) const;

    // 快速查询
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "State")
    bool IsOnGround() const { return CurrentState == ECharacterState::Walking; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "State")
    bool IsInSpace() const { return CurrentState == ECharacterState::Orbiting || CurrentState == ECharacterState::EVA; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "State")
    bool IsInShip() const { return CurrentState == ECharacterState::InShip; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "State")
    bool IsAlive() const { return CurrentState != ECharacterState::Dead; }

    // 事件
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnStateChanged,
        ECharacterState, OldState, ECharacterState, NewState, EStateChangeReason, Reason);
    UPROPERTY(BlueprintAssignable)
    FOnStateChanged OnStateChanged;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStateEntered, ECharacterState, State);
    UPROPERTY(BlueprintAssignable)
    FOnStateEntered OnStateEntered;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStateExited, ECharacterState, State);
    UPROPERTY(BlueprintAssignable)
    FOnStateExited OnStateExited;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

private:
    void EnterState(ECharacterState NewState, EStateChangeReason Reason);
    void ExitState(ECharacterState OldState, EStateChangeReason Reason);
    void TickState(float Dt);
};
