// WeGameOnlineSubsystem.h
// v6.9 — WeGame 在线子系统（会话/匹配/服务器浏览）
// 封装 Rail SDK 的网络功能集，提供蓝图可调用的会话管理接口
// 在 WeGame 平台上，多人联机通过 RailNetwork 实现 P2P 或 Relay 连接

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WeGameOnlineSubsystem.generated.h"

#ifndef WITH_WEGAME
#define WITH_WEGAME 0
#endif

#if WITH_WEGAME
#include "rail/sdk/rail_api.h"
#endif

// —— 会话信息 ——
USTRUCT(BlueprintType)
struct FWeGameSessionInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString SessionID;

    UPROPERTY(BlueprintReadOnly)
    FString HostDisplayName;

    UPROPERTY(BlueprintReadOnly)
    int32 CurrentPlayers = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 MaxPlayers = 8;

    UPROPERTY(BlueprintReadOnly)
    FString Region;

    UPROPERTY(BlueprintReadOnly)
    bool bIsLAN = false;

    UPROPERTY(BlueprintReadOnly)
    bool bIsPrivate = false;
};

// —— 网络模式 ——
UENUM(BlueprintType)
enum class EWeGameNetworkMode : uint8
{
    P2P_Direct    UMETA(DisplayName = "P2P Direct"),
    Relay         UMETA(DisplayName = "Relay Server"),
    Dedicated     UMETA(DisplayName = "Dedicated Server")
};

// —— 委托 ——
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeGameSessionCreated, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeGameSessionJoined, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeGameSessionDestroyed, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeGameSessionSearchComplete, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeGameFriendListReceived, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeGameLobbyInviteReceived, FString, InviteData);

UCLASS()
class STELLARSYSTEM_API UWeGameOnlineSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ========== 生命周期 ==========

    UFUNCTION(BlueprintCallable, Category = "WeGame|Session")
    void SetNetworkMode(EWeGameNetworkMode Mode) { NetworkMode = Mode; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WeGame|Session")
    EWeGameNetworkMode GetNetworkMode() const { return NetworkMode; }

    // ========== 会话操作 ==========

    // 创建会话（主机）
    UFUNCTION(BlueprintCallable, Category = "WeGame|Session")
    void CreateSession(int32 MaxPlayers = 8, bool bIsLAN = false, bool bIsPrivate = false,
                      const FString& SessionName = TEXT("StellarSession"));

    // 查找会话
    UFUNCTION(BlueprintCallable, Category = "WeGame|Session")
    void FindSessions(bool bIsLAN = false, int32 MaxResults = 20);

    // 加入会话
    UFUNCTION(BlueprintCallable, Category = "WeGame|Session")
    void JoinSession(const FString& SessionID);

    // 加入好友会话
    UFUNCTION(BlueprintCallable, Category = "WeGame|Session")
    void JoinFriendSession(const FString& FriendRailID);

    // 销毁会话
    UFUNCTION(BlueprintCallable, Category = "WeGame|Session")
    void DestroySession();

    // 开始会话（接受连接）
    UFUNCTION(BlueprintCallable, Category = "WeGame|Session")
    void StartSession();

    // ========== 好友系统 ==========

    UFUNCTION(BlueprintCallable, Category = "WeGame|Friends")
    void RequestFriendList();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WeGame|Friends")
    TArray<FString> GetFriendNames() const { return CachedFriendNames; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WeGame|Friends")
    TArray<FString> GetFriendRailIDs() const { return CachedFriendRailIDs; }

    UFUNCTION(BlueprintCallable, Category = "WeGame|Friends")
    void InviteFriendToSession(const FString& FriendRailID);

    // ========== 服务器浏览 ==========

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WeGame|ServerBrowser")
    TArray<FWeGameSessionInfo> GetFoundSessions() const { return FoundSessions; }

    UFUNCTION(BlueprintCallable, Category = "WeGame|ServerBrowser")
    void RefreshServerList();

    // ========== 事件委托 ==========

    UPROPERTY(BlueprintAssignable, Category = "WeGame|Events")
    FOnWeGameSessionCreated OnSessionCreated;

    UPROPERTY(BlueprintAssignable, Category = "WeGame|Events")
    FOnWeGameSessionJoined OnSessionJoined;

    UPROPERTY(BlueprintAssignable, Category = "WeGame|Events")
    FOnWeGameSessionDestroyed OnSessionDestroyed;

    UPROPERTY(BlueprintAssignable, Category = "WeGame|Events")
    FOnWeGameSessionSearchComplete OnSessionSearchComplete;

    UPROPERTY(BlueprintAssignable, Category = "WeGame|Events")
    FOnWeGameFriendListReceived OnFriendListReceived;

    UPROPERTY(BlueprintAssignable, Category = "WeGame|Events")
    FOnWeGameLobbyInviteReceived OnLobbyInviteReceived;

    // ========== 状态查询 ==========

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WeGame|State")
    bool IsInSession() const { return bInSession; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WeGame|State")
    FString GetCurrentSessionID() const { return CurrentSessionID; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WeGame|State")
    int32 GetPingToHost() const { return CachedPingMs; }

    // ========== 数据发送（RailNetwork）==========

    UFUNCTION(BlueprintCallable, Category = "WeGame|Network")
    bool SendDataToPlayer(const FString& TargetRailID, const TArray<uint8>& Data, bool bReliable = true);

    UFUNCTION(BlueprintCallable, Category = "WeGame|Network")
    bool SendDataToAll(const TArray<uint8>& Data, bool bReliable = true);

    // 委托：收到数据
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWeGameDataReceived, FString, SenderRailID, const TArray<uint8>&, Data);
    UPROPERTY(BlueprintAssignable, Category = "WeGame|Network")
    FOnWeGameDataReceived OnDataReceived;

private:
    // 网络模式
    EWeGameNetworkMode NetworkMode = EWeGameNetworkMode::P2P_Direct;

    // 会话状态
    bool bInSession = false;
    FString CurrentSessionID;
    int32 CachedPingMs = 0;

    // 搜索结果
    UPROPERTY()
    TArray<FWeGameSessionInfo> FoundSessions;

    // 好友缓存
    TArray<FString> CachedFriendNames;
    TArray<FString> CachedFriendRailIDs;

    // 内部回调（Rail SDK 事件处理）
    void HandleSessionCreated(bool bSuccess);
    void HandleSessionJoined(bool bSuccess);
    void HandleSessionSearchComplete(bool bSuccess);
    void HandleFriendListReceived(bool bSuccess);
    void HandleDataReceived(const FString& SenderID, const TArray<uint8>& Data);
    void HandleLobbyInvite(const FString& InviteData);
};
