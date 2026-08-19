// WeGameAccountBridge.cpp
// v6.9 — WeGame 账号桥接实现

#include "WeGame/WeGameAccountBridge.h"
#include "WeGame/WeGameIntegration.h"
#include "Online/AccountSystem.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

DEFINE_LOG_CATEGORY_STATIC(LogWeGameAuth, Log, All);

UWeGameAccountBridge::UWeGameAccountBridge()
    : bLoggedIn(false)
{
}

// ============================================================
//  主登录流程
// ============================================================

void UWeGameAccountBridge::AttemptWeGameLogin()
{
    // Step 1: 检查 SDK 初始化
    UWeGameIntegration* WeGame = GEngine->GetEngineSubsystem<UWeGameIntegration>();
    if (!WeGame || !WeGame->IsSDKInitialized())
    {
        UE_LOG(LogWeGameAuth, Error, TEXT("WeGame SDK not initialized"));
        OnLoginComplete.Broadcast(EWeGameLoginResult::SDKNotInitialized);
        return;
    }

    // Step 2: 获取 SessionTicket
    if (!WeGame->AcquireSessionTicket())
    {
        UE_LOG(LogWeGameAuth, Error, TEXT("Failed to acquire session ticket"));
        OnLoginComplete.Broadcast(EWeGameLoginResult::TicketAcquisitionFailed);
        return;
    }

    // Step 3: 获取 RailID
    FWeGamePlayerIdentity Identity = WeGame->GetLocalPlayerIdentity();
    RailID = Identity.RailID;

    if (RailID.IsEmpty() || RailID == TEXT("0"))
    {
        UE_LOG(LogWeGameAuth, Error, TEXT("Invalid RailID"));
        OnLoginComplete.Broadcast(EWeGameLoginResult::TicketAcquisitionFailed);
        return;
    }

    CachedSessionTicket = WeGame->GetSessionTicket();

    // Step 4: 发送到游戏服务器验证
    // 实际项目中通过 HTTPS POST 到 AuthServerURL
    // 这里用模拟验证
    FString ResolvedUsername;
    bool bVerified = SimulateServerVerification(RailID, CachedSessionTicket, ResolvedUsername);

    if (!bVerified)
    {
        UE_LOG(LogWeGameAuth, Warning, TEXT("Server verification failed for RailID=%s"), *RailID);
        OnLoginComplete.Broadcast(EWeGameLoginResult::ServerVerificationFailed);
        return;
    }

    // Step 5: 绑定/创建本地账号
    BoundUsername = ResolvedUsername;
    bLoggedIn = true;

    // 如果 AccountSystem 存在，同步登录状态
    if (UAccountSystem* AccountSys = GEngine->GetEngineSubsystem<UAccountSystem>())
    {
        // 用 RailID 作为用户名进行 Steam-like 快速登录
        AccountSys->LoginWithSteam(RailID); // 复用 Steam 快速登录路径
    }

    UE_LOG(LogWeGameAuth, Log, TEXT("WeGame login SUCCESS: RailID=%s Username=%s"),
        *RailID, *BoundUsername);

    // 解锁 WeGame 专属成就
    WeGame->UnlockAchievement(EWeGameAchievement::WeGameFirstLogin);

    // 设置 Rich Presence
    WeGame->SetRichPresence(TEXT("platform"), TEXT("WeGame"));
    WeGame->SetRichPresence(TEXT("status"), TEXT("In StellarSystem"));

    OnLoginComplete.Broadcast(EWeGameLoginResult::Success);
}

void UWeGameAccountBridge::Logout()
{
    bLoggedIn = false;
    BoundUsername.Empty();
    RailID.Empty();
    CachedSessionTicket.Empty();

    // 通知 WeGame 平台
    UWeGameIntegration* WeGame = GEngine->GetEngineSubsystem<UWeGameIntegration>();
    if (WeGame)
    {
        WeGame->ClearRichPresence();
    }

    // 登出 AccountSystem
    if (UAccountSystem* AccountSys = GEngine->GetEngineSubsystem<UAccountSystem>())
    {
        AccountSys->Logout();
    }

    UE_LOG(LogWeGameAuth, Log, TEXT("WeGame logout complete"));
}

// ============================================================
//  服务端验证（静态方法，在专用服务器上调用）
// ============================================================

bool UWeGameAccountBridge::VerifySessionTicket(const FString& InRailID, const FString& InSessionTicket)
{
    if (InRailID.IsEmpty() || InSessionTicket.IsEmpty())
    {
        return false;
    }

    // 实际实现应调用 WeGame 开放平台 Web API
    // POST https://api.wegame.com/v1/auth/verify
    // Headers: { "Authorization": "Bearer <server_token>" }
    // Body: { "rail_id": "...", "session_ticket": "..." }
    //
    // 返回 { "valid": true, "display_name": "...", "level": 42 }
    //
    // 这里返回模拟结果
    return true; // 模拟验证通过
}

bool UWeGameAccountBridge::GetPlayerInfoFromWeGame(const FString& InRailID, FString& OutDisplayName, int32& OutLevel)
{
    if (InRailID.IsEmpty()) return false;

    // 实际实现应调用 WeGame Web API 获取玩家信息
    OutDisplayName = FString::Printf(TEXT("WeGamePlayer_%s"), *InRailID.Right(4));
    OutLevel = 1;
    return true;
}

// ============================================================
//  模拟服务端验证
// ============================================================

bool UWeGameAccountBridge::SimulateServerVerification(const FString& InRailID, const FString& InTicket, FString& OutUsername)
{
    // 模拟网络延迟
    if (InRailID.IsEmpty() || InTicket.IsEmpty())
    {
        return false;
    }

    // 模拟验证逻辑
    // 真实环境：HTTPS POST 到 AuthServerURL
    OutUsername = FString::Printf(TEXT("WeGame_%s"), *InRailID.Right(6));

    // 模拟 95% 成功率
    int32 Hash = GetTypeHash(InRailID + InTicket);
    if ((Hash % 100) < 95)
    {
        return true;
    }
    return false;
}
