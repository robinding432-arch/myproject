#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AccountSystem.generated.h"

// —— 账号数据（保存到本地） ——
USTRUCT(BlueprintType)
struct FAccountData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Username;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString PasswordHash;  // SHA-256 哈希

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Email;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime CreatedDate;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime LastLoginDate;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SteamID;  // 绑定的 SteamID

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 AccountLevel = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 TotalPlayTimeSeconds = 0;

    // 序列化
    void SaveToJSON(TSharedPtr<FJsonObject> JsonObject) const;
    void LoadFromJSON(TSharedPtr<FJsonObject> JsonObject);
};

// —— 登录会话（运行时） ——
USTRUCT(BlueprintType)
struct FLoginSession
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString Username;

    UPROPERTY(BlueprintReadOnly)
    FString AuthToken;  // 随机生成的会话令牌

    UPROPERTY(BlueprintReadOnly)
    FDateTime LoginTime;

    UPROPERTY(BlueprintReadOnly)
    bool bIsValid = false;

    FString GenerateToken() const;
    bool IsExpired() const;
};

// —— 账号系统（GameInstanceSubsystem） ——
UCLASS()
class STELLARSYSTEM_API UAccountSystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // —— 注册 ——
    UFUNCTION(BlueprintCallable, Category = "Account")
    bool RegisterAccount(const FString& Username, const FString& Password, const FString& Email);

    // —— 登录 ——
    UFUNCTION(BlueprintCallable, Category = "Account")
    bool Login(const FString& Username, const FString& Password);

    // —— 用 Steam 快速登录 ——
    UFUNCTION(BlueprintCallable, Category = "Account")
    bool LoginWithSteam(const FString& SteamID);

    // —— 用 WeGame 快速登录 ——
    UFUNCTION(BlueprintCallable, Category = "Account")
    bool LoginWithWeGame(const FString& RailID);

    // —— 登出 ——
    UFUNCTION(BlueprintCallable, Category = "Account")
    void Logout();

    // —— 查询 ——
    UFUNCTION(BlueprintCallable, Category = "Account")
    bool IsLoggedIn() const { return CurrentSession.bIsValid; }

    UFUNCTION(BlueprintCallable, Category = "Account")
    FString GetCurrentUsername() const { return CurrentSession.Username; }

    UFUNCTION(BlueprintCallable, Category = "Account")
    int32 GetAccountLevel() const { return CurrentAccount.AccountLevel; }

    // —— 事件 ——
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLoginResult, bool, bSuccess);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRegisterResult, bool, bSuccess);

    UPROPERTY(BlueprintAssignable, Category = "Account|Events")
    FOnLoginResult OnLoginResult;

    UPROPERTY(BlueprintAssignable, Category = "Account|Events")
    FOnRegisterResult OnRegisterResult;

    // 当前会话
    UPROPERTY(BlueprintReadOnly, Category = "Account")
    FLoginSession CurrentSession;

    UPROPERTY(BlueprintReadOnly, Category = "Account")
    FAccountData CurrentAccount;

private:
    // 账号存储路径
    FString GetAccountFilePath(const FString& Username) const;
    FString GetAccountDirectory() const;

    // 密码哈希
    FString HashPassword(const FString& Password) const;

    // 加载/保存账号
    bool LoadAccount(const FString& Username, FAccountData& OutData) const;
    bool SaveAccount(const FAccountData& AccountData) const;

    // 验证用户名格式
    bool IsValidUsername(const FString& Username) const;
    bool IsValidPassword(const FString& Password) const;
    bool IsValidEmail(const FString& Email) const;
};
