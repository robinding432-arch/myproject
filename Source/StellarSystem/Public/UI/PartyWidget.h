// ============================================================
// 路径: Source/StellarSystem/Public/UI/PartyWidget.h
// 作用: 组队 UI（队伍列表/邀请/权限/标记）
// 依赖: Core/PartySystem.h
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PartyWidget.generated.h"

class UPartySystem;
class AStellarPlayerController;

// 队员显示信息
USTRUCT(BlueprintType)
struct FPartyMemberDisplay
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FName PlayerID;

    UPROPERTY(BlueprintReadOnly)
    FString DisplayName;

    UPROPERTY(BlueprintReadOnly)
    FString StatusText;

    UPROPERTY(BlueprintReadOnly)
    FLinearColor StatusColor;

    UPROPERTY(BlueprintReadOnly)
    FString LocationText;

    UPROPERTY(BlueprintReadOnly)
    FString PermissionText;

    UPROPERTY(BlueprintReadOnly)
    bool bIsLeader = false;

    UPROPERTY(BlueprintReadOnly)
    bool bIsOnline = false;

    UPROPERTY(BlueprintReadOnly)
    bool bIsTalking = false;

    UPROPERTY(BlueprintReadOnly)
    bool bIsMuted = false;

    UPROPERTY(BlueprintReadOnly)
    float Distance = 0.f; // 距离队长
};

// 邀请显示信息
USTRUCT(BlueprintType)
struct FPartyInviteDisplay
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FName InviteID;

    UPROPERTY(BlueprintReadOnly)
    FString FromPlayerName;

    UPROPERTY(BlueprintReadOnly)
    FString InviteMessage;

    UPROPERTY(BlueprintReadOnly)
    float TimeRemaining = 0.f;
};

// 组队 Widget
UCLASS(BlueprintType)
class UPartyWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // —— 初始化 ——
    UFUNCTION(BlueprintCallable, Category = "PartyUI")
    void InitializePartyUI(AStellarPlayerController* Controller);

    // —— 获取队伍列表 ——
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "PartyUI")
    TArray<FPartyMemberDisplay> GetPartyMembers() const;

    // —— 获取待处理邀请 ——
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "PartyUI")
    TArray<FPartyInviteDisplay> GetPendingInvites() const;

    // —— 邀请玩家 ——
    UFUNCTION(BlueprintCallable, Category = "PartyUI")
    void InvitePlayer(const FString& PlayerName, const FString& Message);

    UFUNCTION(BlueprintCallable, Category = "PartyUI")
    void InviteByID(const FName& PlayerID, const FString& Message);

    // —— 接受/拒绝邀请 ——
    UFUNCTION(BlueprintCallable, Category = "PartyUI")
    void AcceptInvite(const FName& InviteID);

    UFUNCTION(BlueprintCallable, Category = "PartyUI")
    void DeclineInvite(const FName& InviteID);

    // —— 踢人 ——
    UFUNCTION(BlueprintCallable, Category = "PartyUI")
    void KickMember(const FName& PlayerID, const FString& Reason);

    // —— 转让队长 ——
    UFUNCTION(BlueprintCallable, Category = "PartyUI")
    void TransferLeadership(const FName& NewLeaderID);

    // —— 提升/降权 ——
    UFUNCTION(BlueprintCallable, Category = "PartyUI")
    void PromoteMember(const FName& PlayerID);

    UFUNCTION(BlueprintCallable, Category = "PartyUI")
    void DemoteMember(const FName& PlayerID);

    // —— 设置共享模式 ——
    UFUNCTION(BlueprintCallable, Category = "PartyUI")
    void ToggleShareMode(FName Mode, bool bEnabled);

    // —— 设置编队 ——
    UFUNCTION(BlueprintCallable, Category = "PartyUI")
    void SetFormation(FName FormationType, float Spacing);

    // —— 语音控制 ——
    UFUNCTION(BlueprintCallable, Category = "PartyUI")
    void MuteMember(const FName& PlayerID);

    UFUNCTION(BlueprintCallable, Category = "PartyUI")
    void UnmuteMember(const FName& PlayerID);

    UFUNCTION(BlueprintCallable, Category = "PartyUI")
    void SetVoiceMode(bool bPushToTalk, float Range);

    // —— 放置目标标记 ——
    UFUNCTION(BlueprintCallable, Category = "PartyUI")
    void PlaceTargetMarker(const FVector& WorldLocation, const FString& Label);

    // —— 清除标记 ——
    UFUNCTION(BlueprintCallable, Category = "PartyUI")
    void ClearMyMarker();

    // —— 离开队伍 ——
    UFUNCTION(BlueprintCallable, Category = "PartyUI")
    void LeaveParty();

    // —— 解散队伍 ——
    UFUNCTION(BlueprintCallable, Category = "PartyUI")
    void DisbandParty();

    // —— 刷新 ——
    UFUNCTION(BlueprintCallable, Category = "PartyUI")
    void RefreshPartyInfo();

    // —— 事件 ——
    UPROPERTY(BlueprintAssignable, Category = "PartyUI|Events")
    FOnPartyUIUpdated OnPartyUpdated;

    UPROPERTY(BlueprintAssignable, Category = "PartyUI|Events")
    FOnInviteReceivedUI OnInviteReceived;

    UPROPERTY(BlueprintAssignable, Category = "PartyUI|Events")
    FOnMemberDiedUI OnMemberDied;

    // —— Tick ——
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
    // 绑定的控制器
    UPROPERTY()
    AStellarPlayerController* BoundController = nullptr;

    // 绑定的组队系统
    UPROPERTY()
    UPartySystem* PartySys = nullptr;

    // 上次刷新
    float LastRefreshTime = 0.f;
    const float UIRefreshInterval = 0.5f;

    // 转换队员信息为显示格式
    FPartyMemberDisplay ConvertToDisplay(const FPartyMemberInfo& Info) const;

    // 转换邀请为显示格式
    FPartyInviteDisplay ConvertInviteToDisplay(const FPartyInvite& Invite) const;
};
