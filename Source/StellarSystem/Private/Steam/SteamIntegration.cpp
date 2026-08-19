// SteamIntegration.cpp
#include "Steam/SteamIntegration.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Engine/Engine.h"

#if WITH_STEAMWORKS
#include "steam/steam_api.h"
#pragma comment(lib, "steam_api.lib")
#endif

USteamIntegration::USteamIntegration()
{
    bInitialized = false;
}

bool USteamIntegration::Initialize()
{
#if WITH_STEAMWORKS
    if (SteamAPI_Init())
    {
        bInitialized = true;
        UE_LOG(LogTemp, Log, TEXT("[Steam] Initialized. Persona: %s"), *GetPersonaName());
        return true;
    }
    UE_LOG(LogTemp, Warning, TEXT("[Steam] Failed to init"));
#endif
    return false;
}

void USteamIntegration::Shutdown()
{
#if WITH_STEAMWORKS
    if (bInitialized)
    {
        SteamAPI_Shutdown();
        bInitialized = false;
    }
#endif
}

bool USteamIntegration::IsSteamRunning() const
{
#if WITH_STEAMWORKS
    return bInitialized && SteamAPI_IsSteamRunning();
#else
    return false;
#endif
}

FString USteamIntegration::AchievementToString(EAchievement A) const
{
    switch (A)
    {
    case EAchievement::FirstLaunch:      return TEXT("ACH_FIRST_LAUNCH");
    case EAchievement::FirstPlanetLand:   return TEXT("ACH_FIRST_PLANET");
    case EAchievement::FirstWarp:        return TEXT("ACH_FIRST_WARP");
    case EAchievement::VisitAll8Planets: return TEXT("ACH_VISIT_ALL_8");
    case EAchievement::KillEnemyShip:     return TEXT("ACH_FIRST_KILL");
    case EAchievement::CollectRareItem:   return TEXT("ACH_RARE_LOOT");
    case EAchievement::MaxLevelShip:      return TEXT("ACH_MAX_SHIP");
    case EAchievement::SurviveStorm:      return TEXT("ACH_STORM_RIDER");
    case EAchievement::CreditMillion:     return TEXT("ACH_MILLIONAIRE");
    case EAchievement::DieFirstTime:      return TEXT("ACH_FIRST_DEATH");
    }
    return TEXT("");
}

bool USteamIntegration::UnlockAchievement(EAchievement Achievement)
{
#if WITH_STEAMWORKS
    if (!bInitialized) return false;
    FString ID = AchievementToString(Achievement);
    if (SteamUserStats()->SetAchievement(TCHAR_TO_UTF8(*ID)))
    {
        SteamUserStats()->StoreStats();
        UnlockedAchievements.Add((uint8)Achievement);
        UE_LOG(LogTemp, Log, TEXT("[Steam] Unlocked: %s"), *ID);
        return true;
    }
#endif
    return false;
}

bool USteamIntegration::ClearAchievement(EAchievement Achievement)
{
#if WITH_STEAMWORKS
    if (!bInitialized) return false;
    FString ID = AchievementToString(Achievement);
    if (SteamUserStats()->ClearAchievement(TCHAR_TO_UTF8(*ID)))
    {
        SteamUserStats()->StoreStats();
        UnlockedAchievements.Remove((uint8)Achievement);
        return true;
    }
#endif
    return false;
}

bool USteamIntegration::HasAchievement(EAchievement Achievement) const
{
#if WITH_STEAMWORKS
    if (!bInitialized) return false;
    bool bAchieved = false;
    FString ID = AchievementToString(Achievement);
    SteamUserStats()->GetAchievement(TCHAR_TO_UTF8(*ID), &bAchieved);
    return bAchieved;
#else
    return UnlockedAchievements.Contains((uint8)Achievement);
#endif
}

void USteamIntegration::ResetAllAchievements()
{
#if WITH_STEAMWORKS
    if (!bInitialized) return;
    for (uint8 i = 0; i <= (uint8)EAchievement::DieFirstTime; ++i)
    {
        FString ID = AchievementToString((EAchievement)i);
        SteamUserStats()->ClearAchievement(TCHAR_TO_UTF8(*ID));
    }
    SteamUserStats()->StoreStats();
#endif
    UnlockedAchievements.Reset();
}

// ---- Cloud Save ----
bool USteamIntegration::WriteCloudFile(const FString& FileName, const TArray<uint8>& Data)
{
#if WITH_STEAMWORKS
    if (!bInitialized) return false;
    FString FullName = CloudPrefix + FileName;
    const char* Name = TCHAR_TO_UTF8(*FullName);
    if (SteamRemoteStorage()->FileWrite(Name, Data.GetData(), Data.Num()))
    {
        UE_LOG(LogTemp, Log, TEXT("[Steam Cloud] Wrote %d bytes to %s"), Data.Num(), *FullName);
        return true;
    }
#endif
    return false;
}

bool USteamIntegration::ReadCloudFile(const FString& FileName, TArray<uint8>& OutData)
{
#if WITH_STEAMWORKS
    if (!bInitialized) return false;
    FString FullName = CloudPrefix + FileName;
    const char* Name = TCHAR_TO_UTF8(*FullName);
    if (SteamRemoteStorage()->FileExists(Name))
    {
        int32 Size = SteamRemoteStorage()->GetFileSize(Name);
        OutData.SetNum(Size);
        SteamRemoteStorage()->FileRead(Name, OutData.GetData(), Size);
        return true;
    }
#endif
    return false;
}

bool USteamIntegration::CloudFileExists(const FString& FileName) const
{
#if WITH_STEAMWORKS
    if (!bInitialized) return false;
    FString FullName = CloudPrefix + FileName;
    return SteamRemoteStorage()->FileExists(TCHAR_TO_UTF8(*FullName));
#else
    return false;
#endif
}

bool USteamIntegration::DeleteCloudFile(const FString& FileName)
{
#if WITH_STEAMWORKS
    if (!bInitialized) return false;
    FString FullName = CloudPrefix + FileName;
    return SteamRemoteStorage()->FileDelete(TCHAR_TO_UTF8(*FullName));
#else
    return false;
#endif
}

TArray<FCloudSaveMeta> USteamIntegration::GetCloudFileList() const
{
    TArray<FCloudSaveMeta> Out;
#if WITH_STEAMWORKS
    if (!bInitialized) return Out;
    int32 Count = SteamRemoteStorage()->GetFileCount();
    for (int32 i = 0; i < Count; ++i)
    {
        int32 Size = 0;
        const char* Name = SteamRemoteStorage()->GetFileNameAndSize(i, &Size);
        FString FName(UTF8_TO_TCHAR(Name));
        if (FName.StartsWith(CloudPrefix))
        {
            FCloudSaveMeta Meta;
            Meta.FileName = FName.RightChop(CloudPrefix.Len());
            Meta.FileSize = Size;
            Out.Add(Meta);
        }
    }
#endif
    return Out;
}

int32 USteamIntegration::GetCloudQuotaUsed() const
{
#if WITH_STEAMWORKS
    if (!bInitialized) return 0;
    uint64 Total = 0, Available = 0;
    SteamRemoteStorage()->GetQuota(&Total, &Available);
    return (int32)(Total - Available);
#endif
    return 0;
}

int32 USteamIntegration::GetCloudQuotaTotal() const
{
#if WITH_STEAMWORKS
    if (!bInitialized) return 0;
    uint64 Total = 0, Available = 0;
    SteamRemoteStorage()->GetQuota(&Total, &Available);
    return (int32)Total;
#endif
    return 0;
}

// ---- Rich Presence ----
void USteamIntegration::SetRichPresence(const FString& Key, const FString& Value)
{
#if WITH_STEAMWORKS
    if (!bInitialized) return;
    SteamFriends()->SetRichPresence(TCHAR_TO_UTF8(*Key), TCHAR_TO_UTF8(*Value));
#endif
}

void USteamIntegration::ClearRichPresence()
{
#if WITH_STEAMWORKS
    if (!bInitialized) return;
    SteamFriends()->ClearRichPresence();
#endif
}

// ---- Stats ----
void USteamIntegration::IncrementStat(const FString& StatName, int32 Amount)
{
#if WITH_STEAMWORKS
    if (!bInitialized) return;
    int32 Current = 0;
    SteamUserStats()->GetStat(TCHAR_TO_UTF8(*StatName), &Current);
    SteamUserStats()->SetStat(TCHAR_TO_UTF8(*StatName), Current + Amount);
#endif
}

void USteamIntegration::SetStat(const FString& StatName, int32 Value)
{
#if WITH_STEAMWORKS
    if (!bInitialized) return;
    SteamUserStats()->SetStat(TCHAR_TO_UTF8(*StatName), Value);
#endif
}

void USteamIntegration::StoreStats()
{
#if WITH_STEAMWORKS
    if (bInitialized) SteamUserStats()->StoreStats();
#endif
}

// ---- Utilities ----
FString USteamIntegration::GetSteamID() const
{
#if WITH_STEAMWORKS
    if (bInitialized && SteamUser())
        return FString::Printf(TEXT("%llu"), SteamUser()->GetSteamID().ConvertToUint64());
#endif
    return TEXT("0");
}

FString USteamIntegration::GetPersonaName() const
{
#if WITH_STEAMWORKS
    if (bInitialized && SteamFriends())
        return FString(UTF8_TO_TCHAR(SteamFriends()->GetPersonaName()));
#endif
    return TEXT("Player");
}

// ---- Event Callbacks ----
void USteamIntegration::OnPlayerLandedOnPlanet(const FString& PlanetName)
{
    if (!bInitialized) return;
    SetRichPresence(TEXT("planet"), PlanetName);
    IncrementStat(TEXT("planets_visited"), 1);

    static TSet<FString> Visited;
    Visited.Add(PlanetName);
    if (Visited.Num() >= 8)
    {
        UnlockAchievement(EAchievement::VisitAll8Planets);
    }
}

void USteamIntegration::OnPlayerWarped(const FString& Destination)
{
    if (!bInitialized) return;
    SetRichPresence(TEXT("status"), FString::Printf(TEXT("Warping to %s"), *Destination));
    UnlockAchievement(EAchievement::FirstWarp);
    IncrementStat(TEXT("warps"), 1);
}

void USteamIntegration::OnPlayerKilledEnemy()
{
    if (!bInitialized) return;
    UnlockAchievement(EAchievement::KillEnemyShip);
    IncrementStat(TEXT("kills"), 1);
}

void USteamIntegration::OnPlayerCollectedRare()
{
    if (!bInitialized) return;
    UnlockAchievement(EAchievement::CollectRareItem);
}

void USteamIntegration::OnPlayerDied()
{
    if (!bInitialized) return;
    UnlockAchievement(EAchievement::DieFirstTime);
    IncrementStat(TEXT("deaths"), 1);
}
