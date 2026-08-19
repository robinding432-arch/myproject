#include "Online/SteamOnlineSubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

void USteamOnlineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // 获取在线子系统
    IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
    if (OnlineSub)
    {
        SessionInterface = OnlineSub->GetSessionInterface();
        if (SessionInterface.IsValid())
        {
            // 绑定回调
            SessionInterface->OnCreateSessionCompleteDelegates.AddUObject(
                this, &USteamOnlineSubsystem::OnCreateSessionComplete);
            SessionInterface->OnFindSessionsCompleteDelegates.AddUObject(
                this, &USteamOnlineSubsystem::OnFindSessionsComplete);
            SessionInterface->OnJoinSessionCompleteDelegates.AddUObject(
                this, &USteamOnlineSubsystem::OnJoinSessionComplete);
            SessionInterface->OnDestroySessionCompleteDelegates.AddUObject(
                this, &USteamOnlineSubsystem::OnDestroySessionComplete);
            SessionInterface->OnStartSessionCompleteDelegates.AddUObject(
                this, &USteamOnlineSubsystem::OnStartSessionComplete);

            UE_LOG(LogTemp, Log, TEXT("[Online] Steam Online Subsystem initialized"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[Online] No OnlineSubsystem found (running in offline mode)"));
    }
}

void USteamOnlineSubsystem::Deinitialize()
{
    if (SessionInterface.IsValid())
    {
        if (SessionInterface->GetNamedSession(NAME_GameSession))
        {
            SessionInterface->DestroySession(NAME_GameSession);
        }
    }
    Super::Deinitialize();
}

void USteamOnlineSubsystem::CreateSession(int32 MaxPlayers, bool bIsLAN, const FString& SessionName)
{
    if (!SessionInterface.IsValid())
    {
        OnSessionCreated.Broadcast(false);
        return;
    }

    FOnlineSessionSettings SessionSettings;
    SessionSettings.bIsLANMatch = bIsLAN;
    SessionSettings.NumPublicConnections = MaxPlayers;
    SessionSettings.NumPrivateConnections = 0;
    SessionSettings.bAllowJoinInProgress = true;
    SessionSettings.bShouldAdvertise = true;
    SessionSettings.bUsesPresence = true;
    SessionSettings.bAllowJoinViaPresence = true;
    SessionSettings.Set(FName("SessionName"), SessionName, EOnlineDataAdvertisementType::ViaOnlineService);

    SessionInterface->CreateSession(0, NAME_GameSession, SessionSettings);
    UE_LOG(LogTemp, Log, TEXT("[Online] Creating session: %s (MaxPlayers=%d)"), *SessionName, MaxPlayers);
}

void USteamOnlineSubsystem::FindSessions(bool bIsLAN, int32 MaxResults)
{
    if (!SessionInterface.IsValid())
    {
        OnSessionSearchComplete.Broadcast(false);
        return;
    }

    SessionSearch = MakeShared<FOnlineSessionSearch>();
    SessionSearch->bIsLanQuery = bIsLAN;
    SessionSearch->MaxSearchResults = MaxResults;
    SessionSearch->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);

    SessionInterface->FindSessions(0, SessionSearch.ToSharedRef());
    UE_LOG(LogTemp, Log, TEXT("[Online] Searching for sessions..."));
}

void USteamOnlineSubsystem::JoinSession(const FString& SessionId)
{
    if (!SessionInterface.IsValid() || !SessionSearch.IsValid())
    {
        OnSessionJoined.Broadcast(false);
        return;
    }

    // 从搜索结果中找匹配的会话
    for (const FOnlineSessionSearchResult& Result : SessionSearch->SearchResults)
    {
        FString FoundId;
        Result.Session.SessionSettings.Get(FName("SessionName"), FoundId);
        if (FoundId == SessionId || Result.GetSessionIdStr() == SessionId)
        {
            PendingJoinSession = Result.Session.GetSessionName();
            SessionInterface->JoinSession(0, NAME_GameSession, Result);
            UE_LOG(LogTemp, Log, TEXT("[Online] Joining session: %s"), *SessionId);
            return;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[Online] Session not found: %s"), *SessionId);
    OnSessionJoined.Broadcast(false);
}

void USteamOnlineSubsystem::DestroySession()
{
    if (SessionInterface.IsValid() && SessionInterface->GetNamedSession(NAME_GameSession))
    {
        SessionInterface->DestroySession(NAME_GameSession);
    }
}

void USteamOnlineSubsystem::StartSession()
{
    if (SessionInterface.IsValid())
    {
        SessionInterface->StartSession(NAME_GameSession);
    }
}

// —— 回调 ——

void USteamOnlineSubsystem::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
    UE_LOG(LogTemp, Log, TEXT("[Online] CreateSession: %s = %s"),
        *SessionName.ToString(), bWasSuccessful ? TEXT("SUCCESS") : TEXT("FAILED"));
    OnSessionCreated.Broadcast(bWasSuccessful);

    if (bWasSuccessful)
    {
        // 自动开始会话
        StartSession();
    }
}

void USteamOnlineSubsystem::OnFindSessionsComplete(bool bWasSuccessful)
{
    FoundSessionNames.Empty();
    FoundSessionPlayerCounts.Empty();
    FoundSessionMaxPlayers.Empty();

    if (bWasSuccessful && SessionSearch.IsValid())
    {
        for (const FOnlineSessionSearchResult& Result : SessionSearch->SearchResults)
        {
            FString Name;
            Result.Session.SessionSettings.Get(FName("SessionName"), Name);
            if (Name.IsEmpty()) Name = Result.GetSessionIdStr();

            FoundSessionNames.Add(Name);
            FoundSessionPlayerCounts.Add(Result.Session.NumOpenPublicConnections);
            FoundSessionMaxPlayers.Add(Result.Session.SessionSettings.NumPublicConnections);
        }
        UE_LOG(LogTemp, Log, TEXT("[Online] Found %d sessions"), FoundSessionNames.Num());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[Online] Session search failed"));
    }

    OnSessionSearchComplete.Broadcast(bWasSuccessful && SessionSearch.IsValid()
        && SessionSearch->SearchResults.Num() > 0);
}

void USteamOnlineSubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
    bool bSuccess = (Result == EOnJoinSessionCompleteResult::Success);
    UE_LOG(LogTemp, Log, TEXT("[Online] JoinSession: %s = %d"),
        *SessionName.ToString(), (int32)Result);

    if (bSuccess && SessionInterface.IsValid())
    {
        // 获取连接字符串并跳转
        FString ConnectString;
        if (SessionInterface->GetResolvedConnectString(NAME_GameSession, ConnectString))
        {
            UE_LOG(LogTemp, Log, TEXT("[Online] Connecting to: %s"), *ConnectString);
            if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
            {
                PC->ClientTravel(ConnectString, TRAVEL_Absolute);
            }
        }
    }

    OnSessionJoined.Broadcast(bSuccess);
}

void USteamOnlineSubsystem::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
    UE_LOG(LogTemp, Log, TEXT("[Online] DestroySession: %s = %s"),
        *SessionName.ToString(), bWasSuccessful ? TEXT("SUCCESS") : TEXT("FAILED"));
    OnSessionDestroyed.Broadcast(bWasSuccessful);
}

void USteamOnlineSubsystem::OnStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
    UE_LOG(LogTemp, Log, TEXT("[Online] StartSession: %s = %s"),
        *SessionName.ToString(), bWasSuccessful ? TEXT("SUCCESS") : TEXT("FAILED"));
}

// —— 玩家信息 ——

FString USteamOnlineSubsystem::GetLocalPlayerName() const
{
    if (IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get())
    {
        if (IOnlineIdentityPtr Identity = OnlineSub->GetIdentityInterface())
        {
            TSharedPtr<const FUniqueNetId> UserId = Identity->GetUniquePlayerId(0);
            if (UserId.IsValid())
            {
                return Identity->GetPlayerNickname(*UserId);
            }
        }
    }
    return FString(TEXT("OfflinePlayer"));
}

FString USteamOnlineSubsystem::GetLocalPlayerSteamID() const
{
    if (IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get())
    {
        if (IOnlineIdentityPtr Identity = OnlineSub->GetIdentityInterface())
        {
            TSharedPtr<const FUniqueNetId> UserId = Identity->GetUniquePlayerId(0);
            if (UserId.IsValid())
            {
                return UserId->ToString();
            }
        }
    }
    return FString(TEXT("00000000000000000"));
}

bool USteamOnlineSubsystem::IsLoggedIn() const
{
    if (IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get())
    {
        if (IOnlineIdentityPtr Identity = OnlineSub->GetIdentityInterface())
        {
            TSharedPtr<const FUniqueNetId> UserId = Identity->GetUniquePlayerId(0);
            return UserId.IsValid() && Identity->GetLoginStatus(*UserId) == ELoginStatus::LoggedIn;
        }
    }
    return false;
}
