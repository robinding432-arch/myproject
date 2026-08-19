#include "Online/AccountSystem.h"
#include "Misc/SecureHash.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/World.h"

// —— 初始化 ——

void UAccountSystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    CurrentSession.bIsValid = false;
    UE_LOG(LogTemp, Log, TEXT("[Account] System initialized. Storage: %s"), *GetAccountDirectory());
}

void UAccountSystem::Deinitialize()
{
    if (CurrentSession.bIsValid)
    {
        Logout();
    }
    Super::Deinitialize();
}

// —— 注册 ——

bool UAccountSystem::RegisterAccount(const FString& Username, const FString& Password, const FString& Email)
{
    // 验证输入
    if (!IsValidUsername(Username))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Account] Registration failed: Invalid username '%s'"), *Username);
        OnRegisterResult.Broadcast(false);
        return false;
    }

    if (!IsValidPassword(Password))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Account] Registration failed: Invalid password"));
        OnRegisterResult.Broadcast(false);
        return false;
    }

    if (!IsValidEmail(Email))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Account] Registration failed: Invalid email"));
        OnRegisterResult.Broadcast(false);
        return false;
    }

    // 检查是否已存在
    if (FPaths::FileExists(GetAccountFilePath(Username)))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Account] Registration failed: Username '%s' already exists"), *Username);
        OnRegisterResult.Broadcast(false);
        return false;
    }

    // 创建账号
    FAccountData NewAccount;
    NewAccount.Username = Username;
    NewAccount.PasswordHash = HashPassword(Password);
    NewAccount.Email = Email;
    NewAccount.CreatedDate = FDateTime::Now();
    NewAccount.LastLoginDate = FDateTime::Now();
    NewAccount.SteamID = TEXT("");
    NewAccount.AccountLevel = 1;
    NewAccount.TotalPlayTimeSeconds = 0;

    if (SaveAccount(NewAccount))
    {
        UE_LOG(LogTemp, Log, TEXT("[Account] Registration success: %s"), *Username);
        OnRegisterResult.Broadcast(true);
        return true;
    }

    OnRegisterResult.Broadcast(false);
    return false;
}

// —— 登录 ——

bool UAccountSystem::Login(const FString& Username, const FString& Password)
{
    FAccountData Account;
    if (!LoadAccount(Username, Account))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Account] Login failed: Username '%s' not found"), *Username);
        OnLoginResult.Broadcast(false);
        return false;
    }

    FString HashedInput = HashPassword(Password);
    if (HashedInput != Account.PasswordHash)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Account] Login failed: Wrong password for '%s'"), *Username);
        OnLoginResult.Broadcast(false);
        return false;
    }

    // 更新登录时间
    Account.LastLoginDate = FDateTime::Now();
    SaveAccount(Account);

    // 创建会话
    CurrentAccount = Account;
    CurrentSession.Username = Username;
    CurrentSession.AuthToken = CurrentSession.GenerateToken();
    CurrentSession.LoginTime = FDateTime::Now();
    CurrentSession.bIsValid = true;

    UE_LOG(LogTemp, Log, TEXT("[Account] Login success: %s (Level %d)"), *Username, Account.AccountLevel);
    OnLoginResult.Broadcast(true);
    return true;
}

bool UAccountSystem::LoginWithSteam(const FString& SteamID)
{
    // 查找绑定此 SteamID 的账号
    FString AccountDir = GetAccountDirectory();
    TArray<FString> Files;
    IFileManager::Get().FindFiles(Files, *(AccountDir / TEXT("*.json")), true, false);

    for (const FString& File : Files)
    {
        FString FilePath = AccountDir / File;
        FString JsonString;
        if (FFileHelper::LoadFileToString(JsonString, *FilePath))
        {
            TSharedPtr<FJsonObject> JsonObject;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
            if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
            {
                FString StoredSteamID;
                if (JsonObject->TryGetStringField(TEXT("SteamID"), StoredSteamID) && StoredSteamID == SteamID)
                {
                    // 找到匹配的账号，用用户名登录
                    FString Username;
                    if (JsonObject->TryGetStringField(TEXT("Username"), Username))
                    {
                        return Login(Username, TEXT("")); // Steam 登录不需要密码
                    }
                }
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[Account] Steam login failed: No account bound to SteamID %s"), *SteamID);
    OnLoginResult.Broadcast(false);
    return false;
}

void UAccountSystem::Logout()
{
    if (CurrentSession.bIsValid)
    {
        // 保存总游玩时间
        if (FDateTime::Now() > CurrentSession.LoginTime)
        {
            FTimespan PlayTime = FDateTime::Now() - CurrentSession.LoginTime;
            CurrentAccount.TotalPlayTimeSeconds += (int32)PlayTime.GetTotalSeconds();
            SaveAccount(CurrentAccount);
        }

        UE_LOG(LogTemp, Log, TEXT("[Account] Logout: %s (Total playtime: %d min)"),
            *CurrentSession.Username, CurrentAccount.TotalPlayTimeSeconds / 60);
    }

    CurrentSession.bIsValid = false;
    CurrentSession.Username = TEXT("");
    CurrentSession.AuthToken = TEXT("");
    CurrentAccount = FAccountData();
}

// —— 工具函数 ——

FString UAccountSystem::HashPassword(const FString& Password) const
{
    // SHA-256 哈希 + 盐
    const FString Salt = TEXT("StellarSystem_2025_Salt_v3");
    FString SaltedPassword = Password + Salt;
    FSHA256 Signature;
    Signature.UpdateWithString(*SaltedPassword, SaltedPassword.Len());
    Signature.Final();
    return Signature.ToString();
}

FString FLoginSession::GenerateToken() const
{
    // 生成随机令牌
    FString Token;
    for (int32 i = 0; i < 32; ++i)
    {
        Token += FString::Printf(TEXT("%02X"), FMath::RandRange(0, 255));
    }
    return Token;
}

bool FLoginSession::IsExpired() const
{
    // 会话 24 小时过期
    FTimespan MaxAge(24, 0, 0);
    return (FDateTime::Now() - LoginTime) > MaxAge;
}

// —— 文件操作 ——

FString UAccountSystem::GetAccountDirectory() const
{
    return FPaths::ProjectSavedDir() / TEXT("Accounts");
}

FString UAccountSystem::GetAccountFilePath(const FString& Username) const
{
    return GetAccountDirectory() / (Username + TEXT(".json"));
}

bool UAccountSystem::LoadAccount(const FString& Username, FAccountData& OutData) const
{
    FString FilePath = GetAccountFilePath(Username);
    if (!FPaths::FileExists(FilePath))
    {
        return false;
    }

    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
    {
        return false;
    }

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        return false;
    }

    OutData.LoadFromJSON(JsonObject);
    return true;
}

bool UAccountSystem::SaveAccount(const FAccountData& AccountData) const
{
    // 确保目录存在
    FString Dir = GetAccountDirectory();
    IFileManager::Get().MakeDirectory(*Dir, true);

    TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
    AccountData.SaveToJSON(JsonObject);

    FString JsonString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
    if (!FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer))
    {
        return false;
    }

    return FFileHelper::SaveStringToFile(JsonString, *GetAccountFilePath(AccountData.Username));
}

// —— 验证 ——

bool UAccountSystem::IsValidUsername(const FString& Username) const
{
    if (Username.Len() < 3 || Username.Len() > 20) return false;
    // 只允许字母数字下划线
    for (TCHAR C : Username)
    {
        if (!FChar::IsAlnum(C) && C != '_') return false;
    }
    return true;
}

bool UAccountSystem::IsValidPassword(const FString& Password) const
{
    if (Password.Len() < 6 || Password.Len() > 50) return false;
    // 至少包含一个数字和一个字母
    bool bHasDigit = false, bHasAlpha = false;
    for (TCHAR C : Password)
    {
        if (FChar::IsDigit(C)) bHasDigit = true;
        if (FChar::IsAlpha(C)) bHasAlpha = true;
    }
    return bHasDigit && bHasAlpha;
}

bool UAccountSystem::IsValidEmail(const FString& Email) const
{
    if (Email.Len() < 5) return false;
    if (!Email.Contains(TEXT("@"))) return false;
    if (!Email.Contains(TEXT("."))) return false;
    return true;
}

// —— FAccountData JSON 序列化 ——

void FAccountData::SaveToJSON(TSharedPtr<FJsonObject> JsonObject) const
{
    JsonObject->SetStringField(TEXT("Username"), Username);
    JsonObject->SetStringField(TEXT("PasswordHash"), PasswordHash);
    JsonObject->SetStringField(TEXT("Email"), Email);
    JsonObject->SetStringField(TEXT("CreatedDate"), CreatedDate.ToString());
    JsonObject->SetStringField(TEXT("LastLoginDate"), LastLoginDate.ToString());
    JsonObject->SetStringField(TEXT("SteamID"), SteamID);
    JsonObject->SetNumberField(TEXT("AccountLevel"), AccountLevel);
    JsonObject->SetNumberField(TEXT("TotalPlayTimeSeconds"), TotalPlayTimeSeconds);
}

void FAccountData::LoadFromJSON(TSharedPtr<FJsonObject> JsonObject)
{
    JsonObject->TryGetStringField(TEXT("Username"), Username);
    JsonObject->TryGetStringField(TEXT("PasswordHash"), PasswordHash);
    JsonObject->TryGetStringField(TEXT("Email"), Email);

    FString DateStr;
    if (JsonObject->TryGetStringField(TEXT("CreatedDate"), DateStr))
        FDateTime::Parse(DateStr, CreatedDate);
    if (JsonObject->TryGetStringField(TEXT("LastLoginDate"), DateStr))
        FDateTime::Parse(DateStr, LastLoginDate);

    JsonObject->TryGetStringField(TEXT("SteamID"), SteamID);
    JsonObject->TryGetNumberField(TEXT("AccountLevel"), AccountLevel);
    JsonObject->TryGetNumberField(TEXT("TotalPlayTimeSeconds"), TotalPlayTimeSeconds);
}
