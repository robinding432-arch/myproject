#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Online/SteamOnlineSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSessionCreated, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSessionJoined, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSessionDestroyed, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSessionSearchComplete, bool, bWasSuccessful);

UCLASS()
class STELLARSYSTEM_API USteamOnlineSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // —— 会话操作 ——
    UFUNCTION(BlueprintCallable, Category = "Online|Session")
    void CreateSession(int32 MaxPlayers = 8, bool bIsLAN = false, const FString& SessionName = TEXT("StellarSession"));

    UFUNCTION(BlueprintCallable, Category = "Online|Session")
    void FindSessions(bool bIsLAN = false, int32 MaxResults = 20);

    UFUNCTION(BlueprintCallable, Category = "Online|Session")
    void JoinSession(const FString& SessionId);

    UFUNCTION(BlueprintCallable, Category = "Online|Session")
    void DestroySession();

    UFUNCTION(BlueprintCallable, Category = "Online|Session")
    void StartSession();

    // —— 玩家信息 ——
    UFUNCTION(BlueprintCallable, Category = "Online|Player")
    FString GetLocalPlayerName() const;

    UFUNCTION(BlueprintCallable, Category = "Online|Player")
    FString GetLocalPlayerSteamID() const;

    UFUNCTION(BlueprintCallable, Category = "Online|Player")
    bool IsLoggedIn() const;

    // —— 事件委托 ——
    UPROPERTY(BlueprintAssignable, Category = "Online|Events")
    FOnSessionCreated OnSessionCreated;

    UPROPERTY(BlueprintAssignable, Category = "Online|Events")
    FOnSessionJoined OnSessionJoined;

    UPROPERTY(BlueprintAssignable, Category = "Online|Events")
    FOnSessionDestroyed OnSessionDestroyed;

    UPROPERTY(BlueprintAssignable, Category = "Online|Events")
    FOnSessionSearchComplete OnSessionSearchComplete;

    // 搜索结果
    UPROPERTY(BlueprintReadOnly, Category = "Online|Session")
    TArray<FString> FoundSessionNames;

    UPROPERTY(BlueprintReadOnly, Category = "Online|Session")
    TArray<int32> FoundSessionPlayerCounts;

    UPROPERTY(BlueprintReadOnly, Category = "Online|Session")
    TArray<int32> FoundSessionMaxPlayers;

private:
    // 内部回调
    void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
    void OnFindSessionsComplete(bool bWasSuccessful);
    void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
    void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
    void OnStartSessionComplete(FName SessionName, bool bWasSuccessful);

    // 当前会话接口缓存
    IOnlineSessionPtr SessionInterface;
    TSharedPtr<FOnlineSessionSearch> SessionSearch;

    // 待加入的会话名
    FName PendingJoinSession;
};
