#include "Online/SteamAchievements.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Steamworks/Steamworks.h"
#include "Engine/World.h"

// —— 初始化 ——

void USteamAchievements::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    InitializeAchievements();
    SyncFromSteam();
    UE_LOG(LogTemp, Log, TEXT("[Steam] Achievements initialized (%d achievements)"),
        AchievementMap.Num());
}

void USteamAchievements::Deinitialize()
{
    // 同步到 Steam
    for (const auto& Pair : AchievementMap)
    {
        if (Pair.Value.bIsUnlocked)
        {
            SyncToSteam(Pair.Key);
        }
    }
    Super::Deinitialize();
}

void USteamAchievements::InitializeAchievements()
{
    DefineAchievement(EAchievementID::FirstSteps,     TEXT("First Steps"),         TEXT("Complete your first quest"), 1,  TEXT("ACH_FIRST_QUEST"));
    DefineAchievement(EAchievementID::SpaceExplorer,   TEXT("Space Explorer"),    TEXT("Visit 5 different planets"), 5,  TEXT("ACH_VISIT_5"));
    DefineAchievement(EAchievementID::GalaxyTraveler,  TEXT("Galaxy Traveler"),  TEXT("Warp 50 times"), 50, TEXT("ACH_WARP_50"));
    DefineAchievement(EAchievementID::WarpSpeed,       TEXT("Warp Speed"),        TEXT("Chain 10 warps without interruption"), 10, TEXT("ACH_WARP_CHAIN_10"));
    DefineAchievement(EAchievementID::FirstBlood,       TEXT("First Blood"),       TEXT("Get your first PvP kill"), 1,  TEXT("ACH_FIRST_KILL"));
    DefineAchievement(EAchievementID::ShipDestroyer,    TEXT("Ship Destroyer"),   TEXT("Destroy 25 ships"), 25, TEXT("ACH_DESTROY_25"));
    DefineAchievement(EAchievementID::PlanetTamer,     TEXT("Planet Tamer"),     TEXT("Spend over 1 hour on a single planet"), 60, TEXT("ACH_STAY_60MIN"), true);
    DefineAchievement(EAchievementID::Merchant,         TEXT("Merchant"),          TEXT("Complete 20 trades"), 20, TEXT("ACH_TRADE_20"));
    DefineAchievement(EAchievementID::Billionaire,      TEXT("Billionaire"),      TEXT("Accumulate 1,000,000 credits"), 1000000, TEXT("ACH_CREDITS_1M"), true);
    DefineAchievement(EAchievementID::Survivor,         TEXT("Survivor"),         TEXT("Survive a radiation storm"), 1,  TEXT("ACH_RAD_SURVIVE"));
    DefineAchievement(EAchievementID::SolarSailor,      TEXT("Solar Sailor"),     TEXT("Sail in solar wind for 5 minutes"), 1,  TEXT("ACH_SOLAR_WIND_5M"), true);
    DefineAchievement(EAchievementID::Miner,            TEXT("Miner"),            TEXT("Mine 100 units of ore"), 100, TEXT("ACH_MINE_100"));
    DefineAchievement(EAchievementID::Architect,        TEXT("Architect"),        TEXT("Build your first structure"), 1,  TEXT("ACH_BUILD_1"));
    DefineAchievement(EAchievementID::Scientist,        TEXT("Scientist"),        TEXT("Complete 10 research quests"), 10, TEXT("ACH_RESEARCH_10"));
    DefineAchievement(EAchievementID::Diplomat,         TEXT("Diplomat"),         TEXT("Complete 5 diplomatic quests"), 5,  TEXT("ACH_DIPLO_5"));
    DefineAchievement(EAchievementID::Pacifist,         TEXT("Pacifist"),        TEXT("Complete 10 quests without killing anyone"), 10, TEXT("ACH_PACIFIST_10"), true);
    DefineAchievement(EAchievementID::Berserker,        TEXT("Berserker"),       TEXT("Kill 10 enemies in a single quest"), 10, TEXT("ACH_BERSERKER"));
    DefineAchievement(EAchievementID::SpeedRunner,      TEXT("Speed Runner"),     TEXT("Complete 5 quests in 30 minutes"), 5,  TEXT("ACH_SPEED_5"), true);
    DefineAchievement(EAchievementID::Collector,        TEXT("Collector"),       TEXT("Collect all resource types"), 8,  TEXT("ACH_COLLECT_ALL"), true);
    DefineAchievement(EAchievementID::Veteran,         TEXT("Veteran"),         TEXT("Reach level 50"), 50, TEXT("ACH_LEVEL_50"));
    DefineAchievement(EAchievementID::Legend,           TEXT("Legend"),           TEXT("Reach level 100"), 100, TEXT("ACH_LEVEL_100"), true);
    DefineAchievement(EAchievementID::Completionist,    TEXT("Completionist"),   TEXT("Complete all quests in a star system"), 1,  TEXT("ACH_COMPLETE_SYS"), true);
    DefineAchievement(EAchievementID::NoMansSky,       TEXT("No Man's Sky"),    TEXT("Visit 100 different planets"), 100, TEXT("ACH_VISIT_100"), true);
    DefineAchievement(EAchievementID::LoneWolf,         TEXT("Lone Wolf"),       TEXT("Win 10 solo PvP matches"), 10, TEXT("ACH_SOLO_10"));
    DefineAchievement(EAchievementID::TeamPlayer,       TEXT("Team Player"),     TEXT("Complete 20 team missions"), 20, TEXT("ACH_TEAM_20"));
    DefineAchievement(EAchievementID::FirstDeath,       TEXT("First Death"),     TEXT("Die for the first time"), 1,  TEXT("ACH_FIRST_DEATH"));
    DefineAchievement(EAchievementID::Resurrection,     TEXT("Resurrection"),   TEXT("Respawn 10 times"), 10, TEXT("ACH_RESPAWN_10"));
    DefineAchievement(EAchievementID::EMPMaster,        TEXT("EMP Master"),       TEXT("Disable 5 ships with EMP"), 5,  TEXT("ACH_EMP_5"));
    DefineAchievement(EAchievementID::FleetCommander,   TEXT("Fleet Commander"), TEXT("Command 5 ships simultaneously"), 5,  TEXT("ACH_FLEET_5"));
    DefineAchievement(EAchievementID::GalaxyConqueror, TEXT("Galaxy Conqueror"), TEXT("Conquer all star systems"), 1,  TEXT("ACH_CONQUER_ALL"), true);
}

void USteamAchievements::DefineAchievement(EAchievementID ID, const FString& Title,
    const FString& Desc, int32 MaxProg, const FString& SteamID, bool bHidden)
{
    FAchievementData Data;
    Data.ID = ID;
    Data.Title = Title;
    Data.Description = Desc;
    Data.bIsHidden = bHidden;
    Data.MaxProgress = MaxProg;
    Data.CurrentProgress = 0;
    Data.bIsUnlocked = false;
    Data.SteamAchievementID = SteamID;
    Data.IconIndex = (int32)ID;

    AchievementMap.Add(ID, Data);
}

// —— 解锁 ——

void USteamAchievements::UnlockAchievement(EAchievementID AchievementID)
{
    if (FAchievementData* Ach = AchievementMap.Find(AchievementID))
    {
        if (Ach->bIsUnlocked) return;

        Ach->bIsUnlocked = true;
        Ach->CurrentProgress = Ach->MaxProgress;

        UE_LOG(LogTemp, Warning, TEXT("🏆 ACHIEVEMENT UNLOCKED: %s — %s"),
            *Ach->Title, *Ach->Description);

        // 通知 Steam
        NotifySteamAchievement(Ach->SteamAchievementID);

        // 广播事件
        OnAchievementUnlocked.Broadcast(AchievementID);
    }
}

void USteamAchievements::AddProgress(EAchievementID AchievementID, int32 Amount)
{
    if (FAchievementData* Ach = AchievementMap.Find(AchievementID))
    {
        if (Ach->bIsUnlocked) return;

        Ach->CurrentProgress = FMath::Min(Ach->CurrentProgress + Amount, Ach->MaxProgress);

        UE_LOG(LogTemp, Log, TEXT("[Achievement] %s: %d/%d"),
            *Ach->Title, Ach->CurrentProgress, Ach->MaxProgress);

        if (Ach->CurrentProgress >= Ach->MaxProgress)
        {
            UnlockAchievement(AchievementID);
        }
        else
        {
            // 进度同步到 Steam
            SyncToSteam(AchievementID);
        }
    }
}

// —— 查询 ——

bool USteamAchievements::IsUnlocked(EAchievementID AchievementID) const
{
    if (const FAchievementData* Ach = AchievementMap.Find(AchievementID))
    {
        return Ach->bIsUnlocked;
    }
    return false;
}

int32 USteamAchievements::GetProgress(EAchievementID AchievementID) const
{
    if (const FAchievementData* Ach = AchievementMap.Find(AchievementID))
    {
        return Ach->CurrentProgress;
    }
    return 0;
}

TArray<FAchievementData> USteamAchievements::GetAllAchievements() const
{
    TArray<FAchievementData> Result;
    for (const auto& Pair : AchievementMap) Result.Add(Pair.Value);
    return Result;
}

TArray<FAchievementData> USteamAchievements::GetUnlockedAchievements() const
{
    TArray<FAchievementData> Result;
    for (const auto& Pair : AchievementMap)
    {
        if (Pair.Value.bIsUnlocked) Result.Add(Pair.Value);
    }
    return Result;
}

float USteamAchievements::GetCompletionPercentage() const
{
    if (AchievementMap.Num() == 0) return 0.f;
    int32 Unlocked = 0;
    for (const auto& Pair : AchievementMap)
    {
        if (Pair.Value.bIsUnlocked) Unlocked++;
    }
    return (float)Unlocked / (float)AchievementMap.Num() * 100.f;
}

// —— Steam 云存档 ——

bool USteamAchievements::WriteCloudFile(const FString& FileName, const TArray<uint8>& Data)
{
#if WITH_STEAMWORKS
    if (ISteamRemoteStorage* SteamRemote = SteamRemoteStorage())
    {
        SteamRemote->FileWrite(TCHAR_TO_UTF8(*FileName), Data.GetData(), Data.Num());
        UE_LOG(LogTemp, Log, TEXT("[SteamCloud] Write: %s (%d bytes)"), *FileName, Data.Num());
        return true;
    }
#endif
    // 离线 fallback：存本地
    FString Path = FPaths::ProjectSavedDir() / TEXT("Cloud") / FileName;
    FFileHelper::SaveArrayToFile(Data, *Path);
    UE_LOG(LogTemp, Log, TEXT("[SteamCloud] Offline write: %s"), *Path);
    return true;
}

bool USteamAchievements::ReadCloudFile(const FString& FileName, TArray<uint8>& OutData)
{
#if WITH_STEAMWORKS
    if (ISteamRemoteStorage* SteamRemote = SteamRemoteStorage())
    {
        if (SteamRemote->FileExists(TCHAR_TO_UTF8(*FileName)))
        {
            int32 FileSize = SteamRemote->GetFileSize(TCHAR_TO_UTF8(*FileName));
            OutData.SetNumUninitialized(FileSize);
            SteamRemote->FileRead(TCHAR_TO_UTF8(*FileName), OutData.GetData(), FileSize);
            UE_LOG(LogTemp, Log, TEXT("[SteamCloud] Read: %s (%d bytes)"), *FileName, FileSize);
            return true;
        }
    }
#endif
    // 离线 fallback
    FString Path = FPaths::ProjectSavedDir() / TEXT("Cloud") / FileName;
    return FFileHelper::LoadFileToArray(OutData, *Path);
}

bool USteamAchievements::DoesCloudFileExist(const FString& FileName) const
{
#if WITH_STEAMWORKS
    if (ISteamRemoteStorage* SteamRemote = SteamRemoteStorage())
    {
        return SteamRemote->FileExists(TCHAR_TO_UTF8(*FileName));
    }
#endif
    FString Path = FPaths::ProjectSavedDir() / TEXT("Cloud") / FileName;
    return FPaths::FileExists(Path);
}

void USteamAchievements::DeleteCloudFile(const FString& FileName)
{
#if WITH_STEAMWORKS
    if (ISteamRemoteStorage* SteamRemote = SteamRemoteStorage())
    {
        SteamRemote->FileDelete(TCHAR_TO_UTF8(*FileName));
    }
#endif
    FString Path = FPaths::ProjectSavedDir() / TEXT("Cloud") / FileName;
    IFileManager::Get().Delete(*Path);
    UE_LOG(LogTemp, Log, TEXT("[SteamCloud] Deleted: %s"), *FileName);
}

TArray<FString> USteamAchievements::GetCloudFileNames() const
{
    TArray<FString> Result;

#if WITH_STEAMWORKS
    if (ISteamRemoteStorage* SteamRemote = SteamRemoteStorage())
    {
        int32 Count = SteamRemote->GetFileCount();
        for (int32 i = 0; i < Count; ++i)
        {
            const char* Name = SteamRemote->GetFileNameAndSize(i, nullptr);
            if (Name) Result.Add(FString(UTF8_TO_TCHAR(Name)));
        }
    }
#endif

    // 也包含本地
    FString LocalDir = FPaths::ProjectSavedDir() / TEXT("Cloud");
    TArray<FString> LocalFiles;
    IFileManager::Get().FindFiles(LocalFiles, *(LocalDir / TEXT("*")), true, false);
    for (const FString& F : LocalFiles) Result.AddUnique(F);

    return Result;
}

// —— Rich Presence ——

void USteamAchievements::SetRichPresence(const FString& Key, const FString& Value)
{
#if WITH_STEAMWORKS
    if (ISteamFriends* SteamFriends = SteamFriends())
    {
        SteamFriends->SetRichPresence(TCHAR_TO_UTF8(*Key), TCHAR_TO_UTF8(*Value));
    }
#endif
    UE_LOG(LogTemp, Log, TEXT("[SteamPresence] %s = %s"), *Key, *Value);
}

void USteamAchievements::ClearRichPresence()
{
#if WITH_STEAMWORKS
    if (ISteamFriends* SteamFriends = SteamFriends())
    {
        SteamFriends->ClearRichPresence();
    }
#endif
    UE_LOG(LogTemp, Log, TEXT("[SteamPresence] Cleared"));
}

// —— Steam API 内部 ——

void USteamAchievements::NotifySteamAchievement(const FString& SteamID)
{
#if WITH_STEAMWORKS
    if (ISteamUserStats* Stats = SteamUserStats())
    {
        Stats->SetAchievement(TCHAR_TO_UTF8(*SteamID));
        Stats->StoreStats();
        UE_LOG(LogTemp, Log, TEXT("[Steam] Achievement notified: %s"), *SteamID);
    }
#else
    UE_LOG(LogTemp, Log, TEXT("[Steam] OFFLINE - Achievement would unlock: %s"), *SteamID);
#endif
}

void USteamAchievements::SyncFromSteam()
{
#if WITH_STEAMWORKS
    if (ISteamUserStats* Stats = SteamUserStats())
    {
        for (auto& Pair : AchievementMap)
        {
            bool bAchieved = false;
            if (Stats->GetAchievement(TCHAR_TO_UTF8(*Pair.Value.SteamAchievementID), &bAchieved))
            {
                Pair.Value.bIsUnlocked = bAchieved;
                if (bAchieved) Pair.Value.CurrentProgress = Pair.Value.MaxProgress;
            }
        }
        UE_LOG(LogTemp, Log, TEXT("[Steam] Synced achievements from Steam"));
    }
#endif
}

void USteamAchievements::SyncToSteam(EAchievementID ID)
{
#if WITH_STEAMWORKS
    if (const FAchievementData* Ach = AchievementMap.Find(ID))
    {
        if (ISteamUserStats* Stats = SteamUserStats())
        {
            // Steam 只支持 0/1 进度，复杂进度用统计
            FString StatName = Ach->SteamAchievementID + TEXT("_PROG");
            Stats->SetStat(TCHAR_TO_UTF8(*StatName), (float)Ach->CurrentProgress);
            Stats->StoreStats();
        }
    }
#endif
}
