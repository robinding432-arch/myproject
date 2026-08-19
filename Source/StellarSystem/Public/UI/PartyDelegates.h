// ============================================================
// 路径: Source/StellarSystem/Public/UI/PartyDelegates.h
// 作用: 组队 UI 事件委托声明
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "PartyDelegates.generated.h"

// 组队 UI 更新事件
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPartyUIUpdated);

// 收到邀请事件
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnInviteReceivedUI, FName, InviteID, FString, FromPlayer, FString, Message);

// 队员死亡事件
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMemberDiedUI, FName, PlayerID, FString, PlayerName);

// 终端状态变化事件
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTerminalStateChanged, uint8, NewState);

// 呼船更新事件
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnShipCallUpdate, FName, ShipID, float, ETA);

// 终端错误事件
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTerminalError, FString, ErrorMessage);
