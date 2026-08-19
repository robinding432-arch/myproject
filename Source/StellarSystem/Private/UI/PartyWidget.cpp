// ============================================================
// PartyWidget.cpp
// 组队 UI 实现
// ============================================================

#include "UI/PartyWidget.h"
#include "UI/PartyDelegates.h"
#include "Core/StellarPlayerController.h"
#include "Core/PartySystem.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerState.h"

void UPartyWidget::InitializePartyUI(AStellarPlayerController* Controller)
{
    BoundController = Controller;

    if (GetWorld())
    {
        PartySys = GetWorld()->GetSubsystem<UPartySystem>();
    }

    RefreshPartyInfo();
}

TArray<FPartyMemberDisplay> UPartyWidget::GetPartyMembers() const
{
    TArray<FPartyMemberDisplay> Result;

    if (!PartySys || !BoundController) return Result;

    FName PlayerID(*BoundController->GetName());
    TArray<FPartyMemberInfo> Members = PartySys->GetPartyMembers(PlayerID);

    for (const FPartyMemberInfo& Info : Members)
    {
        Result.Add(ConvertToDisplay(Info));
    }

    return Result;
}

TArray<FPartyInviteDisplay> UPartyWidget::GetPendingInvites() const
{
    TArray<FPartyInviteDisplay> Result;

    if (!PartySys || !BoundController) return Result;

    FName PlayerID(*BoundController->GetName());
    TArray<FPartyInvite> Invites = PartySys->GetPendingInvitesForPlayer(PlayerID);

    for (const FPartyInvite& Invite : Invites)
    {
        Result.Add(ConvertInviteToDisplay(Invite));
    }

    return Result;
}

void UPartyWidget::InvitePlayer(const FString& PlayerName, const FString& Message)
{
    if (!PartySys || !BoundController) return;

    FName TargetID(*PlayerName);
    PartySys->Server_InvitePlayer(BoundController, TargetID, Message);
}

void UPartyWidget::InviteByID(const FName& PlayerID, const FString& Message)
{
    if (!PartySys || !BoundController) return;

    PartySys->Server_InvitePlayer(BoundController, PlayerID, Message);
}

void UPartyWidget::AcceptInvite(const FName& InviteID)
{
    if (!PartySys || !BoundController) return;

    PartySys->Server_AcceptInvite(BoundController, InviteID);
}

void UPartyWidget::DeclineInvite(const FName& InviteID)
{
    if (!PartySys || !BoundController) return;

    PartySys->Server_DeclineInvite(BoundController, InviteID);
}

void UPartyWidget::KickMember(const FName& PlayerID, const FString& Reason)
{
    if (!PartySys || !BoundController) return;

    PartySys->Server_KickMember(BoundController, PlayerID, Reason);
}

void UPartyWidget::TransferLeadership(const FName& NewLeaderID)
{
    if (!PartySys || !BoundController) return;

    PartySys->Server_TransferLeadership(BoundController, NewLeaderID);
}

void UPartyWidget::PromoteMember(const FName& PlayerID)
{
    if (!PartySys || !BoundController) return;

    PartySys->Server_PromoteMember(BoundController, PlayerID, EPartyPermission::Officer);
}

void UPartyWidget::DemoteMember(const FName& PlayerID)
{
    if (!PartySys || !BoundController) return;

    PartySys->Server_DemoteMember(BoundController, PlayerID, EPartyPermission::Recruit);
}

void UPartyWidget::ToggleShareMode(FName Mode, bool bEnabled)
{
    if (!PartySys || !BoundController) return;

    // 转换 FName 到 EPartyShareMode
    EPartyShareMode ShareMode = EPartyShareMode::None;
    FString ModeStr = Mode.ToString();
    if (ModeStr == TEXT("Loot")) ShareMode = EPartyShareMode::Loot;
    else if (ModeStr == TEXT("XP")) ShareMode = EPartyShareMode::XP;
    else if (ModeStr == TEXT("Reputation")) ShareMode = EPartyShareMode::Reputation;
    else if (ModeStr == TEXT("Radar")) ShareMode = EPartyShareMode::Radar;
    else if (ModeStr == TEXT("Resources")) ShareMode = EPartyShareMode::Resources;
    else if (ModeStr == TEXT("All")) ShareMode = EPartyShareMode::All;

    PartySys->Server_SetShareMode(BoundController, ShareMode, bEnabled);
}

void UPartyWidget::SetFormation(FName FormationType, float Spacing)
{
    if (!PartySys || !BoundController) return;

    EFormationType Formation = EFormationType::Free;
    FString FStr = FormationType.ToString();
    if (FStr == TEXT("VFormation")) Formation = EFormationType::VFormation;
    else if (FStr == TEXT("LineAbreast")) Formation = EFormationType::LineAbreast;
    else if (FStr == TEXT("Column")) Formation = EFormationType::Column;
    else if (FStr == TEXT("Wedge")) Formation = EFormationType::Wedge;
    else if (FStr == TEXT("Diamond")) Formation = EFormationType::Diamond;

    PartySys->Server_SetFormation(BoundController, Formation, Spacing);
}

void UPartyWidget::MuteMember(const FName& PlayerID)
{
    if (!PartySys || !BoundController) return;

    PartySys->Server_MutePlayer(BoundController, PlayerID);
}

void UPartyWidget::UnmuteMember(const FName& PlayerID)
{
    if (!PartySys || !BoundController) return;

    PartySys->Server_UnmutePlayer(BoundController, PlayerID);
}

void UPartyWidget::SetVoiceMode(bool bPushToTalk, float Range)
{
    if (!PartySys || !BoundController) return;

    PartySys->Server_SetVoiceMode(BoundController, bPushToTalk, Range);
}

void UPartyWidget::PlaceTargetMarker(const FVector& WorldLocation, const FString& Label)
{
    if (!PartySys || !BoundController) return;

    PartySys->Server_SetTargetMarker(BoundController, WorldLocation, Label);
}

void UPartyWidget::ClearMyMarker()
{
    if (!PartySys || !BoundController) return;

    PartySys->Server_ClearTargetMarker(BoundController);
}

void UPartyWidget::LeaveParty()
{
    if (!PartySys || !BoundController) return;

    PartySys->Server_LeaveParty(BoundController);
}

void UPartyWidget::DisbandParty()
{
    if (!PartySys || !BoundController) return;

    PartySys->Server_DisbandParty(BoundController);
}

void UPartyWidget::RefreshPartyInfo()
{
    LastRefreshTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    OnPartyUpdated.Broadcast();
}

FPartyMemberDisplay UPartyWidget::ConvertToDisplay(const FPartyMemberInfo& Info) const
{
    FPartyMemberDisplay Display;
    Display.PlayerID = Info.PlayerID;
    Display.DisplayName = Info.DisplayName;
    Display.bIsLeader = (Info.Permission == EPartyPermission::Leader);
    Display.bIsOnline = (Info.Status != EPartyMemberStatus::Offline);
    Display.bIsTalking = Info.bIsTalking;
    Display.bIsMuted = Info.bVoiceMuted;
    Display.Distance = 0.f; // 由蓝图计算实际距离

    // 状态文本
    switch (Info.Status)
    {
    case EPartyMemberStatus::Online:    Display.StatusText = TEXT("Online"); Display.StatusColor = FLinearColor::Green(); break;
    case EPartyMemberStatus::InGame:    Display.StatusText = TEXT("In Game"); Display.StatusColor = FLinearColor(0.f, 0.8f, 0.f); break;
    case EPartyMemberStatus::InSpace:   Display.StatusText = TEXT("In Space"); Display.StatusColor = FLinearColor(0.2f, 0.6f, 1.f); break;
    case EPartyMemberStatus::OnPlanet:  Display.StatusText = TEXT("On Planet"); Display.StatusColor = FLinearColor(0.8f, 0.6f, 0.2f); break;
    case EPartyMemberStatus::InStation: Display.StatusText = TEXT("In Station"); Display.StatusColor = FLinearColor(0.5f, 0.5f, 0.8f); break;
    case EPartyMemberStatus::AFK:       Display.StatusText = TEXT("AFK"); Display.StatusColor = FLinearColor(0.8f, 0.8f, 0.f); break;
    case EPartyMemberStatus::Dead:      Display.StatusText = TEXT("Downed"); Display.StatusColor = FLinearColor(1.f, 0.2f, 0.2f); break;
    case EPartyMemberStatus::Offline:   Display.StatusText = TEXT("Offline"); Display.StatusColor = FLinearColor(0.3f, 0.3f, 0.3f); break;
    default:                            Display.StatusText = TEXT("Unknown"); Display.StatusColor = FLinearColor::Gray(); break;
    }

    // 位置文本
    if (Info.CurrentPlanet != NAME_None)
    {
        Display.LocationText = FString::Printf(TEXT("Planet: %s"), *Info.CurrentPlanet.ToString());
    }
    else if (Info.CurrentStation != NAME_None)
    {
        Display.LocationText = FString::Printf(TEXT("Station: %s"), *Info.CurrentStation.ToString());
    }
    else
    {
        Display.LocationText = TEXT("Unknown");
    }

    // 权限文本
    switch (Info.Permission)
    {
    case EPartyPermission::Leader:  Display.PermissionText = TEXT("Leader"); break;
    case EPartyPermission::Officer: Display.PermissionText = TEXT("Officer"); break;
    case EPartyPermission::Member:  Display.PermissionText = TEXT("Member"); break;
    case EPartyPermission::Recruit: Display.PermissionText = TEXT("Recruit"); break;
    default: Display.PermissionText = TEXT(""); break;
    }

    return Display;
}

FPartyInviteDisplay UPartyWidget::ConvertInviteToDisplay(const FPartyInvite& Invite) const
{
    FPartyInviteDisplay Display;
    Display.InviteID = Invite.InviteID;
    Display.FromPlayerName = Invite.FromPlayerID.ToString();
    Display.InviteMessage = Invite.InviteMessage;

    if (GetWorld())
    {
        float CurrentTime = GetWorld()->GetTimeSeconds();
        Display.TimeRemaining = FMath::Max(0.f, Invite.ExpiryTime - CurrentTime);
    }
    else
    {
        Display.TimeRemaining = 0.f;
    }

    return Display;
}

void UPartyWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!GetWorld()) return;

    float CurrentTime = GetWorld()->GetTimeSeconds();

    if (CurrentTime - LastRefreshTime >= UIRefreshInterval)
    {
        RefreshPartyInfo();
    }
}
