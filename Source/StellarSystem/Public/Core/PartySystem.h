// ============================================================
// 路径: Source/StellarSystem/Public/Core/PartySystem.h
// 作用: 玩家组队系统（模仿星际公民 Squad/Party）
//       支持邀请/踢人/权限/共享/语音/标记/任务分配
// 依赖: Core/StellarPlayerController.h, Core/FactionManager.h
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "GameplayTagContainer.h"
#include "PartySystem.generated.h"

class AStellarPlayerController;
class UFactionManager;

// 队伍成员权限
UENUM(BlueprintType)
enum class EPartyPermission : uint8
{
    Leader         UMETA(DisplayName = "Party Leader"),
    Officer        UMETA(DisplayName = "Officer (Can Invite/Kick)"),
    Member         UMETA(DisplayName = "Member (Basic)"),
    Recruit        UMETA(DisplayName = "Recruit (Limited)"),
    MAX
};

// 队伍成员状态
UENUM(BlueprintType)
enum class EPartyMemberStatus : uint8
{
    Online         UMETA(DisplayName = "Online"),
    InGame         UMETA(DisplayName = "In Game"),
    InSpace        UMETA(DisplayName = "In Space"),
    OnPlanet       UMETA(DisplayName = "On Planet"),
    InStation      UMETA(DisplayName = "In Station"),
    AFK            UMETA(DisplayName = "AFK"),
    Dead           UMETA(DisplayName = "Dead/Downed"),
    Offline        UMETA(DisplayName = "Offline"),
    MAX
};

// 共享设置
UENUM(BlueprintType)
enum class EPartyShareMode : uint8
{
    None           UMETA(DisplayName = "No Sharing"),
    Loot           UMETA(DisplayName = "Loot Sharing"),
    XP             UMETA(DisplayName = "XP Sharing"),
    Reputation     UMETA(DisplayName = "Reputation Sharing"),
    Radar          UMETA(DisplayName = "Radar/Map Sharing"),
    Resources      UMETA(DisplayName = "Resource Nodes"),
    All            UMETA(DisplayName = "Share Everything"),
    MAX
};

// 队伍编队阵型
UENUM(BlueprintType)
enum class EFormationType : uint8
{
    Free           UMETA(DisplayName = "Free Flight"),
    VFormation     UMETA(DisplayName = "V Formation"),
    LineAbreast    UMETA(DisplayName = "Line Abreast"),
    Column         UMETA(DisplayName = "Column"),
    Wedge          UMETA(DisplayName = "Wedge"),
    Diamond        UMETA(DisplayName = "Diamond"),
    MAX
};

// 单个队员信息
USTRUCT(BlueprintType)
struct FPartyMemberInfo
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
    FName PlayerID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
    EPartyPermission Permission = EPartyPermission::Member;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
    EPartyMemberStatus Status = EPartyMemberStatus::Online;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
    FVector LastKnownLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
    FName CurrentPlanet = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
    FName CurrentStation = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
    float LastUpdateTime = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
    bool bIsInSameInstance = false; // 是否在同一服务器实例

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
    bool bIsInSameShip = false; // 是否在同一艘飞船上

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
    FName AssignedShipID = NAME_None; // 分配到的飞船

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
    int32 PartySlot = 0; // 队伍中的编号 0~N

    // 语音状态
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
    bool bVoiceMuted = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
    bool bVoiceDeafened = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
    bool bIsTalking = false;

    // 战斗标记
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
    bool bHasTargetMarker = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
    FVector TargetMarkerLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
    FString TargetMarkerLabel;
};

// 队伍邀请
USTRUCT(BlueprintType)
struct FPartyInvite
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName InviteID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName FromPlayerID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName ToPlayerID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName PartyID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ExpiryTime = 0.f; // 过期时间戳

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsFromFaction = false; // 派系邀请

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString InviteMessage; // 自定义邀请语
};

// 队伍设置
USTRUCT(BlueprintType)
struct FPartySettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bOpenParty = false; // 开放加入

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bAllowFactionMembers = true; // 允许同派系加入

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxMembers = 8; // 最大人数

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EFormationType Formation = EFormationType::Free;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FormationSpacing = 500.f; // 编队间距 cm

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<EPartyShareMode> SharedItems;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bSharedReputation = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bSharedBounties = false; // 共享悬赏目标

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bSharedQuests = false; // 共享任务进度

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bVoiceEnabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bPushToTalk = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float VoiceProximityRange = 100000.f; // 近距离语音范围 cm

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bAllowCrossFaction = false; // 允许跨派系组队

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName HomeBaseStation = NAME_None; // 队伍集合点
};

// 组队系统管理器（GameState 子系统）
UCLASS(BlueprintType)
class UPartySystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;

    // ========== 队伍创建/解散 ==========

    // 创建队伍
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Party")
    void Server_CreateParty(AController* Leader, const FPartySettings& Settings);

    // 解散队伍
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Party")
    void Server_DisbandParty(AController* Leader);

    // 离开队伍
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Party")
    void Server_LeaveParty(AController* Member);

    // ========== 邀请 ==========

    // 邀请玩家
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Party|Invite")
    void Server_InvitePlayer(AController* FromPlayer, FName TargetPlayerID, const FString& Message);

    // 接受邀请
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Party|Invite")
    void Server_AcceptInvite(AController* Player, FName InviteID);

    // 拒绝邀请
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Party|Invite")
    void Server_DeclineInvite(AController* Player, FName InviteID);

    // 取消邀请
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Party|Invite")
    void Server_CancelInvite(AController* Player, FName InviteID);

    // 获取待处理邀请
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Party|Invite")
    TArray<FPartyInvite> GetPendingInvitesForPlayer(FName PlayerID) const;

    // ========== 成员管理 ==========

    // 踢人
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Party|Members")
    void Server_KickMember(AController* Leader, FName TargetPlayerID, const FString& Reason);

    // 转让队长
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Party|Members")
    void Server_TransferLeadership(AController* Leader, FName NewLeaderID);

    // 提升权限
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Party|Members")
    void Server_PromoteMember(AController* Leader, FName TargetPlayerID, EPartyPermission NewPerm);

    // 降权
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Party|Members")
    void Server_DemoteMember(AController* Leader, FName TargetPlayerID, EPartyPermission NewPerm);

    // 查询队员
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Party|Members")
    TArray<FPartyMemberInfo> GetPartyMembers(FName PlayerID) const;

    // 查询玩家所在队伍
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Party|Members")
    FName GetPlayerPartyID(FName PlayerID) const;

    // ========== 共享设置 ==========

    // 修改共享模式
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Party|Share")
    void Server_SetShareMode(AController* Leader, EPartyShareMode Mode, bool bEnabled);

    // 修改编队
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Party|Formation")
    void Server_SetFormation(AController* Leader, EFormationType Formation, float Spacing);

    // 更新队员位置（每帧调用）
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Party|Tracking")
    void Server_UpdateMemberLocation(AController* Member, const FVector& Location);

    // 设置目标标记
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Party|Markers")
    void Server_SetTargetMarker(AController* Member, const FVector& Location, const FString& Label);

    // 清除目标标记
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Party|Markers")
    void Server_ClearTargetMarker(AController* Member);

    // ========== 语音 ==========

    // 静音玩家
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Party|Voice")
    void Server_MutePlayer(AController* FromPlayer, FName TargetPlayerID);

    // 取消静音
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Party|Voice")
    void Server_UnmutePlayer(AController* FromPlayer, FName TargetPlayerID);

    // 设置语音模式
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Party|Voice")
    void Server_SetVoiceMode(AController* Player, bool bPushToTalk, float ProximityRange);

    // 语音活动更新
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Party|Voice")
    void Server_SetTalkingState(AController* Player, bool bIsTalking);

    // ========== 跨实例/跨服务器 ==========

    // 队伍是否在同服务器
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Party|Instance")
    bool IsPartyInSameInstance(FName PartyID) const;

    // 传送队员到同一位置（队长权限）
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Party|Instance")
    void Server_TeleportPartyToLeader(AController* Leader);

    // 设置集合点
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Party|Instance")
    void Server_SetRallyPoint(AController* Leader, const FVector& Location, FName StationID);

    // ========== 队伍战斗 ==========

    // 共享击杀通知
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Party|Combat")
    void Server_NotifyKill(AController* Killer, AController* Victim, float BountyAmount);

    // 共享伤害通知
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Party|Combat")
    void Server_NotifyDamage(AController* Attacker, AController* Victim, float Damage);

    // 队伍复活（队友在范围内可复活倒下的队员）
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Party|Combat")
    void Server_RequestRevive(AController* Medic, AController* DownedTeammate);

    // ========== 队伍任务 ==========

    // 共享任务进度
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Party|Quests")
    void Server_ShareQuestProgress(AController* Member, FName QuestID, float ProgressDelta);

    // 分配战利品
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Party|Loot")
    void Server_DistributeLoot(AController* Leader, FName ItemID, int32 Quantity);

    // 需求分配模式
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Party|Loot")
    void Server_SetLootMode(AController* Leader, FName Mode); // "FreeForAll", "NeedBeforeGreed", "RoundRobin"

    // ========== 设置 ==========

    // 修改队伍设置
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Party|Settings")
    void Server_UpdatePartySettings(AController* Leader, const FPartySettings& NewSettings);

    // 获取队伍设置
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Party|Settings")
    FPartySettings GetPartySettings(FName PartyID) const;

    // ========== 派系整合 ==========

    // 派系自动组队（同派系队友自动邀请）
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Party|Faction")
    void Server_AutoInviteFactionMembers(AController* Player, EFactionId FactionId);

    // 检查跨派系组队合法性
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Party|Faction")
    bool CanCrossFactionParty(EFactionId A, EFactionId B) const;

    // ========== 事件 ==========
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPartyCreated, FName, PartyID, AController*, Leader, int32, MemberCount);
    UPROPERTY(BlueprintAssignable, Category = "Party|Events")
    FOnPartyCreated OnPartyCreated;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPartyDisbanded, FName, PartyID, AController*, Leader);
    UPROPERTY(BlueprintAssignable, Category = "Party|Events")
    FOnPartyDisbanded OnPartyDisbanded;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMemberJoined, FName, PartyID, FName, PlayerID, FString, DisplayName);
    UPROPERTY(BlueprintAssignable, Category = "Party|Events")
    FOnMemberJoined OnMemberJoined;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMemberLeft, FName, PartyID, FName, PlayerID, FString, Reason);
    UPROPERTY(BlueprintAssignable, Category = "Party|Events")
    FOnMemberLeft OnMemberLeft;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnInviteReceived, FName, InviteID, FName, FromPlayer, FString, Message);
    UPROPERTY(BlueprintAssignable, Category = "Party|Events")
    FOnInviteReceived OnInviteReceived;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMemberDied, FName, PartyID, FName, PlayerID, AController*, Killer);
    UPROPERTY(BlueprintAssignable, Category = "Party|Events")
    FOnMemberDied OnMemberDied;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLeaderChanged, FName, PartyID, FName, NewLeaderID);
    UPROPERTY(BlueprintAssignable, Category = "Party|Events")
    FOnLeaderChanged OnLeaderChanged;

    // ========== 网络 ==========
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

private:
    // 所有队伍 (PartyID → 成员列表)
    UPROPERTY()
    TMap<FName, TArray<FPartyMemberInfo>> AllParties;

    // 队伍设置
    UPROPERTY()
    TMap<FName, FPartySettings> PartySettingsMap;

    // 玩家 → 队伍 映射
    UPROPERTY()
    TMap<FName, FName> PlayerToPartyMap;

    // 待处理邀请
    UPROPERTY()
    TArray<FPartyInvite> PendingInvites;

    // 生成唯一 ID
    FName GeneratePartyID() const;
    FName GenerateInviteID() const;

    // 清理过期邀请
    void CleanupExpiredInvites(float CurrentTime);

    // 检查队伍人数上限
    bool CanAddMember(FName PartyID) const;

    // 更新队员状态
    void UpdateMemberStatus(float DeltaTime);

    // 派系管理器引用
    UPROPERTY()
    UFactionManager* FactionManagerRef = nullptr;

    // 邀请过期时间（秒）
    UPROPERTY(EditAnywhere, Category = "Party|Settings")
    float InviteExpiryDuration = 60.f; // 60秒过期

    // 位置更新频率
    UPROPERTY(EditAnywhere, Category = "Party|Settings")
    float LocationUpdateInterval = 0.5f;

    float LocationUpdateTimer = 0.f;
};
