// WeGameAccountBridge.h
// v6.9 — 桥接 WeGame 登录与现有 AccountSystem
// 玩家通过 WeGame 客户端启动游戏后，自动获取 RailID/SessionTicket
// 然后在后端验证票据、创建/绑定本地账号、完成登录

#pragma once

#include "CoreMinimal.h"
#include "WeGameAccountBridge.generated.h"

// —— 登录结果 ——
UENUM(BlueprintType)
enum class EWeGameLoginResult : uint8
{
    Success             UMETA(DisplayName = "Success"),
    SDKNotInitialized    UMETA(DisplayName = "SDK Not Initialized"),
    TicketAcquisitionFailed UMETA(DisplayName = "Ticket Acquisition Failed"),
    ServerVerificationFailed UMETA(DisplayName = "Server Verification Failed"),
    AccountBanned       UMETA(DisplayName = "Account Banned"),
    NeedsRealNameAuth   UMETA(DisplayName = "Needs Real Name Authentication"),
    UnknownError        UMETA(DisplayName = "Unknown Error")
};

// —— 委托 ——
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeGameLoginComplete, EWeGameLoginResult, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeGameRealNameRequired, FString, AuthURL);

UCLASS()
class STELLARSYSTEM_API UWeGameAccountBridge : public UObject
{
    GENERATED_BODY()

public:
    UWeGameAccountBridge();

    // ---- 主流程 ----

    // 尝试用 WeGame 身份自动登录
    // 1) 检查 SDK 初始化
    // 2) 获取 SessionTicket
    // 3) 发送到游戏服务器验证
    // 4) 创建/加载账号
    // 5) 广播 OnWeGameLoginComplete
    UFUNCTION(BlueprintCallable, Category = "WeGame|Auth")
    void AttemptWeGameLogin();

    // 登出
    UFUNCTION(BlueprintCallable, Category = "WeGame|Auth")
    void Logout();

    // ---- 状态查询 ----

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WeGame|Auth")
    bool IsLoggedIn() const { return bLoggedIn; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WeGame|Auth")
    FString GetBoundUsername() const { return BoundUsername; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WeGame|Auth")
    FString GetRailID() const { return RailID; }

    // ---- 服务端验证（在专用服务器上调用）----

    // 验证 WeGame SessionTicket 的合法性
    // 实际项目中应调用 WeGame 开放平台 Web API
    UFUNCTION(BlueprintCallable, Category = "WeGame|Auth|Server")
    static bool VerifySessionTicket(const FString& RailID, const FString& SessionTicket);

    // 获取玩家在 WeGame 平台的基本信息（服务端调用）
    UFUNCTION(BlueprintCallable, Category = "WeGame|Auth|Server")
    static bool GetPlayerInfoFromWeGame(const FString& RailID, FString& OutDisplayName, int32& OutLevel);

    // ---- 委托 ----

    UPROPERTY(BlueprintAssignable, Category = "WeGame|Auth|Events")
    FOnWeGameLoginComplete OnLoginComplete;

    UPROPERTY(BlueprintAssignable, Category = "WeGame|Auth|Events")
    FOnWeGameRealNameRequired OnRealNameRequired;

    // ---- 配置 ----

    // 游戏服务器地址（用于验证票据）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeGame|Auth|Config")
    FString AuthServerURL = TEXT("https://your-game-server.com/api/wegame/verify");

    // 应用 ID
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeGame|Auth|Config")
    int32 AppID = 0;

private:
    // 内部状态
    bool bLoggedIn = false;
    FString BoundUsername;
    FString RailID;
    FString CachedSessionTicket;

    // 模拟服务端验证（在无法访问真实 API 时使用）
    bool SimulateServerVerification(const FString& InRailID, const FString& InTicket, FString& OutUsername);
};
