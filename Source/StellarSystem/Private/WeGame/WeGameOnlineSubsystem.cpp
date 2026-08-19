// WeGameOnlineSubsystem.cpp
// v6.9 — WeGame 在线子系统实现

#include "WeGame/WeGameOnlineSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogWeGameOnline, Log, All);

void UWeGameOnlineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    bInSession = false;
    CachedPingMs = 0;
    UE_LOG(LogWeGameOnline, Log, TEXT("UWeGameOnlineSubsystem initialized"));
}

void UWeGameOnlineSubsystem::Deinitialize()
{
    if (bInSession)
    {
        DestroySession();
    }
    Super::Deinitialize();
}

// ============================================================
//  会话操作
// ============================================================

void UWeGameOnlineSubsystem::CreateSession(int32 MaxPlayers, bool bIsLAN, bool bIsPrivate, const FString& SessionName)
{
#if WITH_WEGAME
    if (bInSession)
    {
        UE_LOG(LogWeGameOnline, Warning, TEXT("CreateSession: already in a session"));
        OnSessionCreated.Broadcast(false);
        return;
    }

    // WeGame Rail SDK 的会话管理通过 IRailNetwork 实现
    // 主机端：创建会话并等待客户端连接
    rail::IRailNetwork* Network = rail::RailFactory()->NetworkHelper();
    if (!Network)
    {
        UE_LOG(LogWeGameOnline, Error, TEXT("Rail NetworkHelper not available"));
        OnSessionCreated.Broadcast(false);
        return;
    }

    // 设置会话参数
    rail::RailNetworkSessionOptions Options;
    FMemory::Memzero(&Options, sizeof(Options));
    Options.max_players = MaxPlayers;
    Options.is_lan = bIsLAN;
    Options.is_private = bIsPrivate;
    Options.session_name = TCHAR_TO_ANSI(*SessionName);

    rail::RailResult Result = Network->CreateSession(Options);
    if (Result == rail::kSuccess)
    {
        bInSession = true;
        CurrentSessionID = SessionName;
        UE_LOG(LogWeGameOnline, Log, TEXT("Session created: %s (max=%d)"), *SessionName, MaxPlayers);
        OnSessionCreated.Broadcast(true);
    }
    else
    {
        UE_LOG(LogWeGameOnline, Error, TEXT("CreateSession failed: %d"), (int32)Result);
        OnSessionCreated.Broadcast(false);
    }
#else
    // Stub 模式
    bInSession = true;
    CurrentSessionID = SessionName;
    UE_LOG(LogWeGameOnline, Log, TEXT("[STUB] Session created: %s (max=%d)"), *SessionName, MaxPlayers);
    OnSessionCreated.Broadcast(true);
#endif
}

void UWeGameOnlineSubsystem::FindSessions(bool bIsLAN, int32 MaxResults)
{
#if WITH_WEGAME
    rail::IRailNetwork* Network = rail::RailFactory()->NetworkHelper();
    if (!Network)
    {
        OnSessionSearchComplete.Broadcast(false);
        return;
    }

    rail::RailNetworkSearchOptions Options;
    FMemory::Memzero(&Options, sizeof(Options));
    Options.max_results = MaxResults;
    Options.is_lan = bIsLAN;

    rail::RailResult Result = Network->SearchSessions(Options);
    if (Result == rail::kSuccess)
    {
        // 结果通过 kRailEventNetworkSearchSessionsResult 回调
        // 在回调中填充 FoundSessions 并广播
        UE_LOG(LogWeGameOnline, Log, TEXT("Session search initiated (max=%d)"), MaxResults);
    }
    else
    {
        UE_LOG(LogWeGameOnline, Warning, TEXT("SearchSessions failed: %d"), (int32)Result);
        OnSessionSearchComplete.Broadcast(false);
    }
#else
    // Stub: 返回模拟结果
    FoundSessions.Empty();
    FWeGameSessionInfo Dummy;
    Dummy.SessionID = TEXT("stub_session_001");
    Dummy.HostDisplayName = TEXT("Stub Host");
    Dummy.CurrentPlayers = 3;
    Dummy.MaxPlayers = 8;
    Dummy.Region = TEXT("Asia");
    Dummy.bIsLAN = false;
    Dummy.bIsPrivate = false;
    FoundSessions.Add(Dummy);

    UE_LOG(LogWeGameOnline, Log, TEXT("[STUB] Found %d sessions"), FoundSessions.Num());
    OnSessionSearchComplete.Broadcast(true);
#endif
}

void UWeGameOnlineSubsystem::JoinSession(const FString& SessionID)
{
#if WITH_WEGAME
    if (bInSession)
    {
        UE_LOG(LogWeGameOnline, Warning, TEXT("JoinSession: already in a session"));
        OnSessionJoined.Broadcast(false);
        return;
    }

    rail::IRailNetwork* Network = rail::RailFactory()->NetworkHelper();
    if (!Network)
    {
        OnSessionJoined.Broadcast(false);
        return;
    }

    rail::RailResult Result = Network->JoinSession(TCHAR_TO_ANSI(*SessionID));
    if (Result == rail::kSuccess)
    {
        bInSession = true;
        CurrentSessionID = SessionID;
        UE_LOG(LogWeGameOnline, Log, TEXT("Joining session: %s"), *SessionID);
        // 结果通过 kRailEventNetworkJoinSessionResult 回调
    }
    else
    {
        UE_LOG(LogWeGameOnline, Error, TEXT("JoinSession failed: %d"), (int32)Result);
        OnSessionJoined.Broadcast(false);
    }
#else
    bInSession = true;
    CurrentSessionID = SessionID;
    UE_LOG(LogWeGameOnline, Log, TEXT("[STUB] Joined session: %s"), *SessionID);
    OnSessionJoined.Broadcast(true);
#endif
}

void UWeGameOnlineSubsystem::JoinFriendSession(const FString& FriendRailID)
{
#if WITH_WEGAME
    rail::IRailNetwork* Network = rail::RailFactory()->NetworkHelper();
    if (!Network)
    {
        OnSessionJoined.Broadcast(false);
        return;
    }

    rail::RailID FriendID;
    FriendID.set_id(FCString::Strtoui64(*FriendRailID, nullptr, 10));

    rail::RailResult Result = Network->JoinFriendSession(FriendID);
    if (Result == rail::kSuccess)
    {
        UE_LOG(LogWeGameOnline, Log, TEXT("Joining friend session: %s"), *FriendRailID);
    }
    else
    {
        UE_LOG(LogWeGameOnline, Error, TEXT("JoinFriendSession failed: %d"), (int32)Result);
        OnSessionJoined.Broadcast(false);
    }
#else
    JoinSession(TEXT("friend_session_") + FriendRailID);
#endif
}

void UWeGameOnlineSubsystem::DestroySession()
{
#if WITH_WEGAME
    if (!bInSession) return;

    rail::IRailNetwork* Network = rail::RailFactory()->NetworkHelper();
    if (Network)
    {
        Network->LeaveSession();
        Network->DestroySession();
    }
#endif
    bInSession = false;
    CurrentSessionID.Empty();
    CachedPingMs = 0;
    UE_LOG(LogWeGameOnline, Log, TEXT("Session destroyed"));
    OnSessionDestroyed.Broadcast(true);
}

void UWeGameOnlineSubsystem::StartSession()
{
#if WITH_WEGAME
    if (!bInSession) return;

    rail::IRailNetwork* Network = rail::RailFactory()->NetworkHelper();
    if (Network)
    {
        Network->StartSession();
    }
#endif
    UE_LOG(LogWeGameOnline, Log, TEXT("Session started — accepting connections"));
}

// ============================================================
//  好友系统
// ============================================================

void UWeGameOnlineSubsystem::RequestFriendList()
{
#if WITH_WEGAME
    rail::IRailFriends* Friends = rail::RailFactory()->RailFriends();
    if (!Friends)
    {
        OnFriendListReceived.Broadcast(false);
        return;
    }

    rail::RailResult Result = Friends->AsyncGetFriendsList("FriendList");
    if (Result == rail::kSuccess)
    {
        UE_LOG(LogWeGameOnline, Log, TEXT("Friend list requested"));
        // 结果通过 kRailEventFriendsGetFriendsListResult 回调
    }
    else
    {
        OnFriendListReceived.Broadcast(false);
    }
#else
    // Stub
    CachedFriendNames.Empty();
    CachedFriendRailIDs.Empty();
    CachedFriendNames.Add(TEXT("WeGameFriend_Alpha"));
    CachedFriendNames.Add(TEXT("WeGameFriend_Beta"));
    CachedFriendRailIDs.Add(TEXT("10001"));
    CachedFriendRailIDs.Add(TEXT("10002"));
    UE_LOG(LogWeGameOnline, Log, TEXT("[STUB] Friend list: %d friends"), CachedFriendNames.Num());
    OnFriendListReceived.Broadcast(true);
#endif
}

void UWeGameOnlineSubsystem::InviteFriendToSession(const FString& FriendRailID)
{
#if WITH_WEGAME
    rail::IRailNetwork* Network = rail::RailFactory()->NetworkHelper();
    if (!Network) return;

    rail::RailID FriendID;
    FriendID.set_id(FCString::Strtoui64(*FriendRailID, nullptr, 10));

    rail::RailResult Result = Network->InviteFriend(FriendID, TCHAR_TO_ANSI(*CurrentSessionID));
    if (Result == rail::kSuccess)
    {
        UE_LOG(LogWeGameOnline, Log, TEXT("Invited friend %s to session"), *FriendRailID);
    }
#endif
}

// ============================================================
//  服务器浏览
// ============================================================

void UWeGameOnlineSubsystem::RefreshServerList()
{
    // 委托给 FindSessions
    FindSessions(false, 50);
}

// ============================================================
//  数据发送（RailNetwork）
// ============================================================

bool UWeGameOnlineSubsystem::SendDataToPlayer(const FString& TargetRailID, const TArray<uint8>& Data, bool bReliable)
{
#if WITH_WEGAME
    if (!bInSession) return false;

    rail::IRailNetwork* Network = rail::RailFactory()->NetworkHelper();
    if (!Network) return false;

    rail::RailID Target;
    Target.set_id(FCString::Strtoui64(*TargetRailID, nullptr, 10));

    rail::RailResult Result = Network->SendData(
        rail::RailFactory()->RailPlayer()->GetRailID(),
        Target,
        Data.GetData(),
        Data.Num(),
        bReliable ? rail::kRailNetworkSendReliable : rail::kRailNetworkSendUnreliable
    );
    return Result == rail::kSuccess;
#else
    UE_LOG(LogWeGameOnline, Verbose, TEXT("[STUB] SendDataToPlayer %s (%d bytes, reliable=%d)"),
        *TargetRailID, Data.Num(), bReliable);
    return true;
#endif
}

bool UWeGameOnlineSubsystem::SendDataToAll(const TArray<uint8>& Data, bool bReliable)
{
#if WITH_WEGAME
    if (!bInSession) return false;

    rail::IRailNetwork* Network = rail::RailFactory()->NetworkHelper();
    if (!Network) return false;

    // 广播给会话中所有玩家
    rail::RailResult Result = Network->SendDataToAll(
        Data.GetData(),
        Data.Num(),
        bReliable ? rail::kRailNetworkSendReliable : rail::kRailNetworkSendUnreliable
    );
    return Result == rail::kSuccess;
#else
    UE_LOG(LogWeGameOnline, Verbose, TEXT("[STUB] SendDataToAll (%d bytes)"), Data.Num());
    return true;
#endif
}

// ============================================================
//  内部回调处理
// ============================================================

void UWeGameOnlineSubsystem::HandleSessionCreated(bool bSuccess)
{
    bInSession = bSuccess;
    OnSessionCreated.Broadcast(bSuccess);
}

void UWeGameOnlineSubsystem::HandleSessionJoined(bool bSuccess)
{
    bInSession = bSuccess;
    if (bSuccess)
    {
        UE_LOG(LogWeGameOnline, Log, TEXT("Successfully joined session"));
    }
    OnSessionJoined.Broadcast(bSuccess);
}

void UWeGameOnlineSubsystem::HandleSessionSearchComplete(bool bSuccess)
{
    if (bSuccess)
    {
        UE_LOG(LogWeGameOnline, Log, TEXT("Session search complete: %d results"), FoundSessions.Num());
    }
    OnSessionSearchComplete.Broadcast(bSuccess);
}

void UWeGameOnlineSubsystem::HandleFriendListReceived(bool bSuccess)
{
    OnFriendListReceived.Broadcast(bSuccess);
}

void UWeGameOnlineSubsystem::HandleDataReceived(const FString& SenderID, const TArray<uint8>& Data)
{
    OnDataReceived.Broadcast(SenderID, Data);
}

void UWeGameOnlineSubsystem::HandleLobbyInvite(const FString& InviteData)
{
    UE_LOG(LogWeGameOnline, Log, TEXT("Lobby invite received: %s"), *InviteData);
    OnLobbyInviteReceived.Broadcast(InviteData);
}
