// ============================================================
// PartySystem.cpp
// 玩家组队系统实现
// ============================================================

#include "Core/PartySystem.h"
#include "Core/StellarPlayerController.h"
#include "Factions/FactionManager.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameState.h"

UPartySystem::UPartySystem()
{
    InviteExpiryDuration = 60.f;
    LocationUpdateInterval = 0.5f;
}

void UPartySystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (GetWorld() && GetWorld()->GetNetMode() != NM_Client)
    {
        // 尝试获取派系管理器引用
        FactionManagerRef = GetWorld()->GetSubsystem<UFactionManager>();
    }
}

void UPartySystem::Deinitialize()
{
    AllParties.Empty();
    PartySettingsMap.Empty();
    PlayerToPartyMap.Empty();
    PendingInvites.Empty();
    FactionManagerRef = nullptr;

    Super::Deinitialize();
}

void UPartySystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (GetWorld() == nullptr) return;

    float CurrentTime = GetWorld()->GetTimeSeconds();

    // 清理过期邀请
    CleanupExpiredInvites(CurrentTime);

    // 更新队员状态
    UpdateMemberStatus(DeltaTime);

    // 位置更新计时
    LocationUpdateTimer += DeltaTime;
    if (LocationUpdateTimer >= LocationUpdateInterval)
    {
        LocationUpdateTimer = 0.f;

        // 更新所有队员位置
        for (auto& PartyPair : AllParties)
        {
            for (FPartyMemberInfo& Member : PartyPair.Value)
            {
                // 查找对应 Controller 并更新位置
                // 简化：由客户端主动上报
                Member.LastUpdateTime = CurrentTime;
            }
        }
    }
}

// ========== 队伍创建/解散 ==========

void UPartySystem::Server_CreateParty_Implementation(AController* Leader, const FPartySettings& Settings)
{
    if (!Leader) return;

    FString LeaderID = Leader->GetName();
    FName LeaderFName(*LeaderID);

    // 检查是否已在队伍中
    if (PlayerToPartyMap.Contains(LeaderFName)) return;

    FName PartyID = GeneratePartyID();

    FPartyMemberInfo LeaderInfo;
    LeaderInfo.PlayerID = LeaderFName;
    LeaderInfo.DisplayName = LeaderID;
    LeaderInfo.Permission = EPartyPermission::Leader;
    LeaderInfo.Status = EPartyMemberStatus::InGame;
    LeaderInfo.PartySlot = 0;
    LeaderInfo.LastUpdateTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

    TArray<FPartyMemberInfo> Members;
    Members.Add(LeaderInfo);

    AllParties.Add(PartyID, Members);
    PlayerToPartyMap.Add(LeaderFName, PartyID);
    PartySettingsMap.Add(PartyID, Settings);

    OnPartyCreated.Broadcast(PartyID, Leader, 1);
}

void UPartySystem::Server_DisbandParty_Implementation(AController* Leader)
{
    if (!Leader) return;

    FName LeaderFName(*Leader->GetName());
    if (!PlayerToPartyMap.Contains(LeaderFName)) return;

    FName PartyID = PlayerToPartyMap[LeaderFName];

    // 检查是否是队长
    if (AllParties.Contains(PartyID) && AllParties[PartyID].Num() > 0)
    {
        if (AllParties[PartyID][0].PlayerID != LeaderFName) return; // 不是队长
    }

    // 清除所有队员的队伍映射
    for (const FPartyMemberInfo& Member : AllParties[PartyID])
    {
        PlayerToPartyMap.Remove(Member.PlayerID);
    }

    OnPartyDisbanded.Broadcast(PartyID, Leader);

    AllParties.Remove(PartyID);
    PartySettingsMap.Remove(PartyID);
}

void UPartySystem::Server_LeaveParty_Implementation(AController* Member)
{
    if (!Member) return;

    FName MemberFName(*Member->GetName());
    if (!PlayerToPartyMap.Contains(MemberFName)) return;

    FName PartyID = PlayerToPartyMap[MemberFName];

    if (AllParties.Contains(PartyID))
    {
        FString Reason = TEXT("Left the party");

        // 如果是队长离开，转让或解散
        if (AllParties[PartyID][0].PlayerID == MemberFName)
        {
            if (AllParties[PartyID].Num() > 1)
            {
                // 转让给下一个成员
                AllParties[PartyID][1].Permission = EPartyPermission::Leader;
            }
            else
            {
                // 最后一人，解散
                AllParties.Remove(PartyID);
                PartySettingsMap.Remove(PartyID);
                PlayerToPartyMap.Remove(MemberFName);
                return;
            }
        }

        // 移除成员
        for (int32 i = 0; i < AllParties[PartyID].Num(); ++i)
        {
            if (AllParties[PartyID][i].PlayerID == MemberFName)
            {
                AllParties[PartyID].RemoveAt(i);
                break;
            }
        }
    }

    PlayerToPartyMap.Remove(MemberFName);
}

// ========== 邀请 ==========

void UPartySystem::Server_InvitePlayer_Implementation(AController* FromPlayer, FName TargetPlayerID, const FString& Message)
{
    if (!FromPlayer) return;

    FName FromID(*FromPlayer->GetName());

    // 检查邀请者是否在队伍中
    if (!PlayerToPartyMap.Contains(FromID)) return;

    FName PartyID = PlayerToPartyMap[FromID];

    // 检查权限（队长或军官可邀请）
    if (AllParties.Contains(PartyID))
    {
        bool bCanInvite = false;
        for (const FPartyMemberInfo& M : AllParties[PartyID])
        {
            if (M.PlayerID == FromID &&
                (M.Permission == EPartyPermission::Leader || M.Permission == EPartyPermission::Officer))
            {
                bCanInvite = true;
                break;
            }
        }
        if (!bCanInvite) return;
    }

    // 检查目标是否已在队伍中
    if (PlayerToPartyMap.Contains(TargetPlayerID)) return;

    // 检查队伍人数
    if (!CanAddMember(PartyID)) return;

    // 创建邀请
    FPartyInvite Invite;
    Invite.InviteID = GenerateInviteID();
    Invite.FromPlayerID = FromID;
    Invite.ToPlayerID = TargetPlayerID;
    Invite.PartyID = PartyID;
    Invite.ExpiryTime = GetWorld() ? GetWorld()->GetTimeSeconds() + InviteExpiryDuration : 60.f;
    Invite.bIsFromFaction = false;
    Invite.InviteMessage = Message;

    PendingInvites.Add(Invite);
}

void UPartySystem::Server_AcceptInvite_Implementation(AController* Player, FName InviteID)
{
    if (!Player) return;

    FString PlayerID = Player->GetName();
    FName PlayerFName(*PlayerID);

    for (int32 i = 0; i < PendingInvites.Num(); ++i)
    {
        if (PendingInvites[i].InviteID == InviteID && PendingInvites[i].ToPlayerID == PlayerFName)
        {
            FName PartyID = PendingInvites[i].PartyID;

            if (CanAddMember(PartyID))
            {
                FPartyMemberInfo NewMember;
                NewMember.PlayerID = PlayerFName;
                NewMember.DisplayName = PlayerID;
                NewMember.Permission = EPartyPermission::Member;
                NewMember.Status = EPartyMemberStatus::InGame;
                NewMember.PartySlot = AllParties[PartyID].Num();
                NewMember.LastUpdateTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

                AllParties[PartyID].Add(NewMember);
                PlayerToPartyMap.Add(PlayerFName, PartyID);

                OnMemberJoined.Broadcast(PartyID, PlayerFName, PlayerID);
            }

            PendingInvites.RemoveAt(i);
            break;
        }
    }
}

void UPartySystem::Server_DeclineInvite_Implementation(AController* Player, FName InviteID)
{
    if (!Player) return;

    FName PlayerFName(*Player->GetName());

    for (int32 i = 0; i < PendingInvites.Num(); ++i)
    {
        if (PendingInvites[i].InviteID == InviteID && PendingInvites[i].ToPlayerID == PlayerFName)
        {
            PendingInvites.RemoveAt(i);
            break;
        }
    }
}

void UPartySystem::Server_CancelInvite_Implementation(AController* Player, FName InviteID)
{
    if (!Player) return;

    for (int32 i = 0; i < PendingInvites.Num(); ++i)
    {
        if (PendingInvites[i].InviteID == InviteID)
        {
            PendingInvites.RemoveAt(i);
            break;
        }
    }
}

TArray<FPartyInvite> UPartySystem::GetPendingInvitesForPlayer(FName PlayerID) const
{
    TArray<FPartyInvite> Result;
    for (const FPartyInvite& Invite : PendingInvites)
    {
        if (Invite.ToPlayerID == PlayerID)
        {
            Result.Add(Invite);
        }
    }
    return Result;
}

// ========== 成员管理 ==========

void UPartySystem::Server_KickMember_Implementation(AController* Leader, FName TargetPlayerID, const FString& Reason)
{
    if (!Leader) return;

    FName LeaderFName(*Leader->GetName());
    if (!PlayerToPartyMap.Contains(LeaderFName)) return;

    FName PartyID = PlayerToPartyMap[LeaderFName];

    // 检查权限
    bool bIsLeader = false;
    if (AllParties.Contains(PartyID) && AllParties[PartyID].Num() > 0)
    {
        bIsLeader = (AllParties[PartyID][0].PlayerID == LeaderFName);
    }
    if (!bIsLeader) return;

    // 不能踢自己
    if (TargetPlayerID == LeaderFName) return;

    if (AllParties.Contains(PartyID))
    {
        for (int32 i = 0; i < AllParties[PartyID].Num(); ++i)
        {
            if (AllParties[PartyID][i].PlayerID == TargetPlayerID)
            {
                OnMemberLeft.Broadcast(PartyID, TargetPlayerID, Reason);
                AllParties[PartyID].RemoveAt(i);
                break;
            }
        }
    }

    PlayerToPartyMap.Remove(TargetPlayerID);
}

void UPartySystem::Server_TransferLeadership_Implementation(AController* Leader, FName NewLeaderID)
{
    if (!Leader) return;

    FName LeaderFName(*Leader->GetName());
    if (!PlayerToPartyMap.Contains(LeaderFName)) return;

    FName PartyID = PlayerToPartyMap[LeaderFName];

    if (AllParties.Contains(PartyID))
    {
        for (FPartyMemberInfo& M : AllParties[PartyID])
        {
            if (M.PlayerID == LeaderFName)
            {
                M.Permission = EPartyPermission::Officer;
            }
            else if (M.PlayerID == NewLeaderID)
            {
                M.Permission = EPartyPermission::Leader;
            }
        }
        OnLeaderChanged.Broadcast(PartyID, NewLeaderID);
    }
}

void UPartySystem::Server_PromoteMember_Implementation(AController* Leader, FName TargetPlayerID, EPartyPermission NewPerm)
{
    if (!Leader) return;

    FName LeaderFName(*Leader->GetName());
    if (!PlayerToPartyMap.Contains(LeaderFName)) return;

    FName PartyID = PlayerToPartyMap[LeaderFName];

    // 只有队长能提升
    if (AllParties.Contains(PartyID) && AllParties[PartyID].Num() > 0)
    {
        if (AllParties[PartyID][0].PlayerID != LeaderFName) return;
    }

    if (AllParties.Contains(PartyID))
    {
        for (FPartyMemberInfo& M : AllParties[PartyID])
        {
            if (M.PlayerID == TargetPlayerID)
            {
                M.Permission = NewPerm;
                break;
            }
        }
    }
}

void UPartySystem::Server_DemoteMember_Implementation(AController* Leader, FName TargetPlayerID, EPartyPermission NewPerm)
{
    if (!Leader) return;

    FName LeaderFName(*Leader->GetName());
    if (!PlayerToPartyMap.Contains(LeaderFName)) return;

    FName PartyID = PlayerToPartyMap[LeaderFName];

    if (AllParties.Contains(PartyID))
    {
        for (FPartyMemberInfo& M : AllParties[PartyID])
        {
            if (M.PlayerID == TargetPlayerID)
            {
                M.Permission = NewPerm;
                break;
            }
        }
    }
}

TArray<FPartyMemberInfo> UPartySystem::GetPartyMembers(FName PlayerID) const
{
    if (!PlayerToPartyMap.Contains(PlayerID)) return TArray<FPartyMemberInfo>();
    FName PartyID = PlayerToPartyMap[PlayerID];
    if (AllParties.Contains(PartyID)) return AllParties[PartyID];
    return TArray<FPartyMemberInfo>();
}

FName UPartySystem::GetPlayerPartyID(FName PlayerID) const
{
    if (PlayerToPartyMap.Contains(PlayerID)) return PlayerToPartyMap[PlayerID];
    return NAME_None;
}

// ========== 共享设置 ==========

void UPartySystem::Server_SetShareMode_Implementation(AController* Leader, EPartyShareMode Mode, bool bEnabled)
{
    if (!Leader) return;

    FName LeaderFName(*Leader->GetName());
    if (!PlayerToPartyMap.Contains(LeaderFName)) return;

    FName PartyID = PlayerToPartyMap[LeaderFName];

    if (PartySettingsMap.Contains(PartyID))
    {
        FPartySettings& Settings = PartySettingsMap[PartyID];
        if (bEnabled && !Settings.SharedItems.Contains(Mode))
        {
            Settings.SharedItems.Add(Mode);
        }
        else if (!bEnabled)
        {
            Settings.SharedItems.Remove(Mode);
        }
    }
}

void UPartySystem::Server_SetFormation_Implementation(AController* Leader, EFormationType Formation, float Spacing)
{
    if (!Leader) return;

    FName LeaderFName(*Leader->GetName());
    if (!PlayerToPartyMap.Contains(LeaderFName)) return;

    FName PartyID = PlayerToPartyMap[LeaderFName];

    if (PartySettingsMap.Contains(PartyID))
    {
        PartySettingsMap[PartyID].Formation = Formation;
        PartySettingsMap[PartyID].FormationSpacing = FMath::Clamp(Spacing, 100.f, 5000.f);
    }
}

void UPartySystem::Server_UpdateMemberLocation_Implementation(AController* Member, const FVector& Location)
{
    if (!Member) return;

    FName MemberFName(*Member->GetName());
    if (!PlayerToPartyMap.Contains(MemberFName)) return;

    FName PartyID = PlayerToPartyMap[MemberFName];

    if (AllParties.Contains(PartyID))
    {
        for (FPartyMemberInfo& M : AllParties[PartyID])
        {
            if (M.PlayerID == MemberFName)
            {
                M.LastKnownLocation = Location;
                M.LastUpdateTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
                break;
            }
        }
    }
}

void UPartySystem::Server_SetTargetMarker_Implementation(AController* Member, const FVector& Location, const FString& Label)
{
    if (!Member) return;

    FName MemberFName(*Member->GetName());
    if (!PlayerToPartyMap.Contains(MemberFName)) return;

    FName PartyID = PlayerToPartyMap[MemberFName];

    if (AllParties.Contains(PartyID))
    {
        for (FPartyMemberInfo& M : AllParties[PartyID])
        {
            if (M.PlayerID == MemberFName)
            {
                M.bHasTargetMarker = true;
                M.TargetMarkerLocation = Location;
                M.TargetMarkerLabel = Label;
                break;
            }
        }
    }
}

void UPartySystem::Server_ClearTargetMarker_Implementation(AController* Member)
{
    if (!Member) return;

    FName MemberFName(*Member->GetName());
    if (!PlayerToPartyMap.Contains(MemberFName)) return;

    FName PartyID = PlayerToPartyMap[MemberFName];

    if (AllParties.Contains(PartyID))
    {
        for (FPartyMemberInfo& M : AllParties[PartyID])
        {
            if (M.PlayerID == MemberFName)
            {
                M.bHasTargetMarker = false;
                M.TargetMarkerLocation = FVector::ZeroVector;
                M.TargetMarkerLabel = TEXT("");
                break;
            }
        }
    }
}

// ========== 语音 ==========

void UPartySystem::Server_MutePlayer_Implementation(AController* FromPlayer, FName TargetPlayerID)
{
    if (!FromPlayer) return;

    FName FromID(*FromPlayer->GetName());
    if (!PlayerToPartyMap.Contains(FromID)) return;

    FName PartyID = PlayerToPartyMap[FromID];

    if (AllParties.Contains(PartyID))
    {
        for (FPartyMemberInfo& M : AllParties[PartyID])
        {
            if (M.PlayerID == TargetPlayerID)
            {
                M.bVoiceMuted = true;
                break;
            }
        }
    }
}

void UPartySystem::Server_UnmutePlayer_Implementation(AController* FromPlayer, FName TargetPlayerID)
{
    if (!FromPlayer) return;

    FName FromID(*FromPlayer->GetName());
    if (!PlayerToPartyMap.Contains(FromID)) return;

    FName PartyID = PlayerToPartyMap[FromID];

    if (AllParties.Contains(PartyID))
    {
        for (FPartyMemberInfo& M : AllParties[PartyID])
        {
            if (M.PlayerID == TargetPlayerID)
            {
                M.bVoiceMuted = false;
                break;
            }
        }
    }
}

void UPartySystem::Server_SetVoiceMode_Implementation(AController* Player, bool bPushToTalk, float ProximityRange)
{
    if (!Player) return;

    FName PlayerFName(*Player->GetName());
    if (!PlayerToPartyMap.Contains(PlayerFName)) return;

    FName PartyID = PlayerToPartyMap[PlayerFName];

    if (PartySettingsMap.Contains(PartyID))
    {
        PartySettingsMap[PartyID].bPushToTalk = bPushToTalk;
        PartySettingsMap[PartyID].VoiceProximityRange = FMath::Clamp(ProximityRange, 1000.f, 1000000.f);
    }
}

void UPartySystem::Server_SetTalkingState_Implementation(AController* Player, bool bIsTalking)
{
    if (!Player) return;

    FName PlayerFName(*Player->GetName());
    if (!PlayerToPartyMap.Contains(PlayerFName)) return;

    FName PartyID = PlayerToPartyMap[PlayerFName];

    if (AllParties.Contains(PartyID))
    {
        for (FPartyMemberInfo& M : AllParties[PartyID])
        {
            if (M.PlayerID == PlayerFName)
            {
                M.bIsTalking = bIsTalking;
                break;
            }
        }
    }
}

// ========== 跨实例 ==========

bool UPartySystem::IsPartyInSameInstance(FName PartyID) const
{
    if (!AllParties.Contains(PartyID)) return false;

    for (const FPartyMemberInfo& M : AllParties[PartyID])
    {
        if (!M.bIsInSameInstance) return false;
    }
    return true;
}

void UPartySystem::Server_TeleportPartyToLeader_Implementation(AController* Leader)
{
    if (!Leader) return;

    FName LeaderFName(*Leader->GetName());
    if (!PlayerToPartyMap.Contains(LeaderFName)) return;

    FName PartyID = PlayerToPartyMap[LeaderFName];

    // 获取队长位置
    FVector LeaderLoc = Leader->GetPawn() ? Leader->GetPawn()->GetActorLocation() : FVector::ZeroVector;

    // 通知所有队员传送（实际传送由 GameMode 执行）
    if (AllParties.Contains(PartyID))
    {
        for (const FPartyMemberInfo& M : AllParties[PartyID])
        {
            if (M.PlayerID == LeaderFName) continue;

            // 查找对应 Controller 并传送
            // 简化：广播事件让 GameMode 处理
        }
    }
}

void UPartySystem::Server_SetRallyPoint_Implementation(AController* Leader, const FVector& Location, FName StationID)
{
    if (!Leader) return;

    FName LeaderFName(*Leader->GetName());
    if (!PlayerToPartyMap.Contains(LeaderFName)) return;

    FName PartyID = PlayerToPartyMap[LeaderFName];

    if (PartySettingsMap.Contains(PartyID))
    {
        PartySettingsMap[PartyID].HomeBaseStation = StationID;
    }

    // 广播集合点给所有队员
    if (AllParties.Contains(PartyID))
    {
        for (const FPartyMemberInfo& M : AllParties[PartyID])
        {
            // 设置目标标记到集合点
            // 实际实现中会通过 RPC 通知客户端
        }
    }
}

// ========== 战斗 ==========

void UPartySystem::Server_NotifyKill_Implementation(AController* Killer, AController* Victim, float BountyAmount)
{
    if (!Killer) return;

    FName KillerFName(*Killer->GetName());
    if (!PlayerToPartyMap.Contains(KillerFName)) return;

    FName PartyID = PlayerToPartyMap[KillerFName];

    // 共享悬赏
    if (PartySettingsMap.Contains(PartyID) && PartySettingsMap[PartyID].bSharedBounties)
    {
        // 分配赏金给所有队员
        int32 MemberCount = AllParties[PartyID].Num();
        if (MemberCount > 0)
        {
            float Share = BountyAmount / MemberCount;
            // 实际分配给每个队员
        }
    }
}

void UPartySystem::Server_NotifyDamage_Implementation(AController* Attacker, AController* Victim, float Damage)
{
    // 通知队友攻击事件（用于 UI 提示）
    if (!Attacker) return;

    FName AttackerID(*Attacker->GetName());
    if (!PlayerToPartyMap.Contains(AttackerID)) return;

    FName PartyID = PlayerToPartyMap[AttackerID];

    // 广播伤害事件给队友
    if (AllParties.Contains(PartyID))
    {
        for (const FPartyMemberInfo& M : AllParties[PartyID])
        {
            if (M.PlayerID == AttackerID) continue;
            // 通知客户端更新 HUD
        }
    }
}

void UPartySystem::Server_RequestRevive_Implementation(AController* Medic, AController* DownedTeammate)
{
    if (!Medic || !DownedTeammate) return;

    // 检查距离（简化：允许复活）
    // 实际应检查距离和冷却时间

    // 通知 GameMode 复活队友
    // 通过事件广播
}

// ========== 任务 ==========

void UPartySystem::Server_ShareQuestProgress_Implementation(AController* Member, FName QuestID, float ProgressDelta)
{
    if (!Member) return;

    FName MemberID(*Member->GetName());
    if (!PlayerToPartyMap.Contains(MemberID)) return;

    FName PartyID = PlayerToPartyMap[MemberID];

    if (PartySettingsMap.Contains(PartyID) && PartySettingsMap[PartyID].bSharedQuests)
    {
        // 同步任务进度给所有队友
        // 实际应调用 QuestSystem 更新
    }
}

void UPartySystem::Server_DistributeLoot_Implementation(AController* Leader, FName ItemID, int32 Quantity)
{
    if (!Leader) return;

    FName LeaderID(*Leader->GetName());
    if (!PlayerToPartyMap.Contains(LeaderID)) return;

    FName PartyID = PlayerToPartyMap[LeaderID];

    if (AllParties.Contains(PartyID))
    {
        int32 MemberCount = AllParties[PartyID].Num();
        if (MemberCount == 0) return;

        int32 PerMember = Quantity / MemberCount;
        int32 Remainder = Quantity % MemberCount;

        for (int32 i = 0; i < AllParties[PartyID].Num(); ++i)
        {
            int32 Amount = PerMember + (i < Remainder ? 1 : 0);
            // 实际应调用 Inventory 添加物品
        }
    }
}

void UPartySystem::Server_SetLootMode_Implementation(AController* Leader, FName Mode)
{
    // "FreeForAll", "NeedBeforeGreed", "RoundRobin"
    // 存储到 PartySettings
    if (!Leader) return;

    FName LeaderID(*Leader->GetName());
    if (!PlayerToPartyMap.Contains(LeaderID)) return;

    FName PartyID = PlayerToPartyMap[LeaderID];
    // 简化存储：转为字符串存在 HomeBaseStation 字段（实际应有专用字段）
}

// ========== 设置 ==========

void UPartySystem::Server_UpdatePartySettings_Implementation(AController* Leader, const FPartySettings& NewSettings)
{
    if (!Leader) return;

    FName LeaderID(*Leader->GetName());
    if (!PlayerToPartyMap.Contains(LeaderID)) return;

    FName PartyID = PlayerToPartyMap[LeaderID];

    // 检查权限
    if (AllParties.Contains(PartyID) && AllParties[PartyID].Num() > 0)
    {
        if (AllParties[PartyID][0].PlayerID != LeaderID) return;
    }

    PartySettingsMap[PartyID] = NewSettings;
}

FPartySettings UPartySystem::GetPartySettings(FName PartyID) const
{
    if (PartySettingsMap.Contains(PartyID)) return PartySettingsMap[PartyID];
    return FPartySettings();
}

// ========== 派系整合 ==========

void UPartySystem::Server_AutoInviteFactionMembers_Implementation(AController* Player, EFactionId FactionId)
{
    if (!Player || !FactionManagerRef) return;

    // 获取同派系所有玩家
    // 自动发送邀请
    // 简化实现
}

bool UPartySystem::CanCrossFactionParty(EFactionId A, EFactionId B) const
{
    // 敌对派系不能组队
    if (FactionManagerRef)
    {
        EFactionRelation Rel = FactionManagerRef->GetFactionRelation(A, B);
        if (Rel == EFactionRelation::AtWar || Rel == EFactionRelation::Hostile)
        {
            return false;
        }
    }

    // 海盗可以和任何人组队（混乱中立）
    if (A == EFactionId::CrimsonPirates || B == EFactionId::CrimsonPirates)
    {
        return true;
    }

    return true;
}

// ========== 辅助函数 ==========

FName UPartySystem::GeneratePartyID() const
{
    static int32 Counter = 0;
    Counter++;
    return FName(*FString::Printf(TEXT("PTY_%08d_%d"), FMath::RandRange(10000000, 99999999), Counter));
}

FName UPartySystem::GenerateInviteID() const
{
    static int32 Counter = 0;
    Counter++;
    return FName(*FString::Printf(TEXT("INV_%08d_%d"), FMath::RandRange(10000000, 99999999), Counter));
}

void UPartySystem::CleanupExpiredInvites(float CurrentTime)
{
    TArray<int32> ExpiredIndices;
    for (int32 i = 0; i < PendingInvites.Num(); ++i)
    {
        if (CurrentTime >= PendingInvites[i].ExpiryTime)
        {
            ExpiredIndices.Add(i);
        }
    }
    for (int32 idx = ExpiredIndices.Num() - 1; idx >= 0; --idx)
    {
        PendingInvites.RemoveAt(ExpiredIndices[idx]);
    }
}

bool UPartySystem::CanAddMember(FName PartyID) const
{
    if (!AllParties.Contains(PartyID)) return false;
    if (!PartySettingsMap.Contains(PartyID)) return true;

    return AllParties[PartyID].Num() < PartySettingsMap[PartyID].MaxMembers;
}

void UPartySystem::UpdateMemberStatus(float DeltaTime)
{
    if (!GetWorld()) return;

    float CurrentTime = GetWorld()->GetTimeSeconds();
    const float AFKThreshold = 120.f; // 2分钟无更新 = AFK

    for (auto& PartyPair : AllParties)
    {
        for (FPartyMemberInfo& Member : PartyPair.Value)
        {
            float IdleTime = CurrentTime - Member.LastUpdateTime;
            if (IdleTime > AFKThreshold)
            {
                Member.Status = EPartyMemberStatus::AFK;
            }
        }
    }
}

void UPartySystem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    // UWorldSubsystem 默认不复制，需要时添加
}
