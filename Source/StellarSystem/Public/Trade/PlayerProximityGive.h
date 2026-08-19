// ============================================================
// 路径: Source/StellarSystem/Public/Trade/PlayerProximityGive.h
// 作用: 玩家↔玩家 近距离物品给付系统
//       —— 面对面移交物品/货币/装备，距离校验+服务端权威
// 依赖: Character/InventoryComponent.h, Character/CurrencyComponent.h
// 新增于: v7.5
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PlayerProximityGive.generated.h"

class AMyCharacter;
class UInventoryComponent;
class UCurrencyComponent;

// 给付请求状态
UENUM(BlueprintType)
enum class EGiveRequestStatus : uint8
{
    Pending    UMETA(DisplayName = "等待对方确认"),
    Accepted   UMETA(DisplayName = "已接受"),
    Rejected   UMETA(DisplayName = "已拒绝"),
    Expired    UMETA(DisplayName = "已过期"),
    Cancelled  UMETA(DisplayName = "已取消"),
    Completed  UMETA(DisplayName = "已完成")
};

// 单个给付物品条目
USTRUCT(BlueprintType)
struct FGiveItemEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FName ItemID;

    UPROPERTY(BlueprintReadOnly)
    int32 Quantity = 0;

    UPROPERTY(BlueprintReadOnly)
    FString DisplayName;

    UPROPERTY(BlueprintReadOnly)
    bool bIsEquipped = false; // 是否从装备栏卸下给付
};

// 给付请求（一次完整的面对面交易）
USTRUCT(BlueprintType)
struct FGiveRequest
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FName RequestID;

    UPROPERTY(BlueprintReadOnly)
    FString FromPlayerNetID;   // 给付方

    UPROPERTY(BlueprintReadOnly)
    FString ToPlayerNetID;     // 接收方

    UPROPERTY(BlueprintReadOnly)
    TArray<FGiveItemEntry> Items; // 给付物品列表

    UPROPERTY(BlueprintReadOnly)
    float CreditAmount = 0.f;   // 附带货币

    UPROPERTY(BlueprintReadOnly)
    EGiveRequestStatus Status = EGiveRequestStatus::Pending;

    UPROPERTY(BlueprintReadOnly)
    float CreatedAt = 0.f;      // 创建时间戳

    UPROPERTY(BlueprintReadOnly)
    float ExpiresAt = 0.f;      // 过期时间戳

    UPROPERTY(BlueprintReadOnly)
    bool bSenderConfirmed = false; // 给付方确认

    UPROPERTY(BlueprintReadOnly)
    bool bReceiverConfirmed = false; // 接收方确认
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnGiveRequestReceived, FName, RequestID, FString, FromPlayer, int32, ItemCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnGiveRequestUpdated, FName, RequestID, EGiveRequestStatus, Status, FString, Message, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnGiveItemsTransferred, FName, RequestID, FString, ReceiverNetID, int32, TotalItems);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGiveRequestExpired, FName, RequestID);

// ============================================================
// 玩家近距离给付管理器（挂在 GameMode/WorldSubsystem）
// ============================================================
UCLASS(BlueprintType)
class APlayerProximityGiveManager : public AActor
{
    GENERATED_BODY()

public:
    APlayerProximityGiveManager();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // —— 距离参数 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProximityGive|Range")
    float MaxGiveDistance = 400.f; // cm (约4米，面对面距离)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProximityGive|Range")
    float MaxGiveDistanceSq; // 平方缓存(自动算)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProximityGive|Range")
    bool bRequireLineOfSight = true; // 需要视线(不能隔墙)

    // —— 请求超时 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProximityGive|Timing")
    float RequestTimeout = 30.f; // 请求30秒过期

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProximityGive|Timing")
    float ConfirmTimeout = 15.f; // 确认后15秒未完成则取消

    // —— 限制 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProximityGive|Limits")
    int32 MaxItemsPerGive = 20; // 单次最多给付20件

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProximityGive|Limits")
    float MaxCreditPerGive = 1000000.f; // 单次最多100万信用点

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProximityGive|Limits")
    bool bAllowEquippedItemGive = true; // 是否允许给付已装备物品

    // —— 安全 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProximityGive|Security")
    bool bLogAllTransactions = true; // 记录所有给付日志(反作弊)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProximityGive|Security")
    int32 MaxGivesPerMinute = 10; // 防刷限制

    // ========== 发起给付 ==========
    // 给付方调用：向目标玩家发起给付请求
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "ProximityGive")
    void Server_InitiateGive(AController* Sender, AController* Receiver,
                             const TArray<FGiveItemEntry>& Items, float CreditAmount);

    // ========== 响应给付 ==========
    // 接收方接受
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "ProximityGive")
    void Server_AcceptGive(AController* Receiver, FName RequestID);

    // 接收方拒绝
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "ProximityGive")
    void Server_RejectGive(AController* Receiver, FName RequestID, FString Reason);

    // 给付方取消
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "ProximityGive")
    void Server_CancelGive(AController* Sender, FName RequestID);

    // ========== 查询 ==========
    // 获取附近可给付的玩家列表(距离内+视线)
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ProximityGive")
    TArray<FString> GetNearbyPlayersForGive(AController* Player) const;

    // 获取待处理的给付请求
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ProximityGive")
    TArray<FGiveRequest> GetPendingRequestsForPlayer(AController* Player) const;

    // 获取请求详情
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ProximityGive")
    FGiveRequest GetGiveRequest(FName RequestID) const;

    // 检查两人是否在给付距离内
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ProximityGive")
    bool ArePlayersInGiveRange(AController* PlayerA, AController* PlayerB) const;

    // ========== 飞船↔飞船 给付 ==========
    // 两艘飞船靠近时转移货物
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "ProximityGive|Ship")
    void Server_TransferCargoBetweenShips(AController* Sender, AController* Receiver,
                                          const TArray<FName>& ItemIDs, const TArray<int32>& Quantities);

    // 飞船间货币转移
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "ProximityGive|Ship")
    void Server_TransferCreditsBetweenShips(AController* Sender, AController* Receiver, float Amount);

    // 飞船给付距离(比地面远)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProximityGive|Ship")
    float ShipGiveDistance = 1500.f; // cm (飞船对接距离)

    // ========== 事件 ==========
    UPROPERTY(BlueprintAssignable, Category = "ProximityGive|Events")
    FOnGiveRequestReceived OnGiveRequestReceived;

    UPROPERTY(BlueprintAssignable, Category = "ProximityGive|Events")
    FOnGiveRequestUpdated OnGiveRequestUpdated;

    UPROPERTY(BlueprintAssignable, Category = "ProximityGive|Events")
    FOnGiveItemsTransferred OnGiveItemsTransferred;

    UPROPERTY(BlueprintAssignable, Category = "ProximityGive|Events")
    FOnGiveRequestExpired OnGiveRequestExpired;

private:
    // 活跃请求列表
    UPROPERTY()
    TMap<FName, FGiveRequest> ActiveRequests;

    // 玩家频率限制(防刷)
    UPROPERTY()
    TMap<FString, TArray<float>> PlayerGiveTimestamps; // NetID → 时间戳列表

    // 过期检查
    void TickExpireRequests(float CurrentTime);

    // 距离校验(服务端)
    bool ValidateDistance(AController* A, AController* B, float MaxDist) const;

    // 视线校验
    bool HasLineOfSight(AController* A, AController* B) const;

    // 执行实际物品转移
    void ExecuteTransfer(FGiveRequest& Request);

    // 记录日志
    void LogTransaction(const FGiveRequest& Request, bool bSuccess, FString Note);

    // 频率检查
    bool CheckRateLimit(FString NetID, float CurrentTime);

    // 生成 RequestID
    FName GenerateRequestID() const;
};
