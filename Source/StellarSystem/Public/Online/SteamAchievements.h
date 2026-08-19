#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SteamAchievements.generated.h"

// —— 成就定义 ——
UENUM(BlueprintType)
enum class EAchievementID : uint8
{
    FirstSteps,         // 完成第一个任务
    SpaceExplorer,      // 访问 5 个不同星球
    GalaxyTraveler,      // 跃迁 50 次
    WarpSpeed,           // 连续跃迁 10 次不中断
    FirstBlood,          // 第一次 PvP 击杀
    ShipDestroyer,       // 摧毁 25 艘飞船
    PlanetTamer,        // 在一个星球停留超过 1 小时
    Merchant,            // 完成 20 笔交易
    Billionaire,        // 累计获得 1,000,000 信用点
    Survivor,           // 在辐射风暴中存活
    SolarSailor,        // 在太阳风中航行 5 分钟
    Miner,              // 开采 100 单位矿石
    Architect,          // 建造第一个建筑
    Scientist,          // 完成 10 个研究任务
    Diplomat,           // 完成 5 个外交任务
    Pacifist,           // 完成 10 个任务不杀任何人
    Berserker,         // 一次任务中击杀 10 个敌人
    SpeedRunner,        // 30 分钟内完成 5 个任务
    Collector,          // 收集所有类型资源
    Veteran,           // 达到 50 级
    Legend,            // 达到 100 级
    Completionist,     // 完成所有星球的所有任务
    NoMansSky,         // 访问 100 个不同星球
    LoneWolf,          // 单排模式下赢得 10 场 PvP
    TeamPlayer,         // 组队模式下完成 20 个任务
    FirstDeath,         // 第一次死亡
    Resurrection,       // 复活 10 次
    EMPMaster,          // 用 EMP 武器瘫痪 5 艘飞船
    FleetCommander,     // 同时指挥 5 艘飞船
    GalaxyConqueror     // 占领所有恒星系
};

// —— 成就数据 ——
USTRUCT(BlueprintType)
struct FAchievementData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EAchievementID ID = EAchievementID::FirstSteps;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Title;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsHidden = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxProgress = 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    int32 CurrentProgress = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    bool bIsUnlocked = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SteamAchievementID;  // Steam 平台对应 ID

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 IconIndex = 0;

    bool IsComplete() const { return bIsUnlocked || CurrentProgress >= MaxProgress; }
};

// —— Steam 成就系统 ——
UCLASS()
class STELLARSYSTEM_API USteamAchievements : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // —— 初始化成就列表 ——
    UFUNCTION(BlueprintCallable, Category = "Achievements")
    void InitializeAchievements();

    // —— 解锁成就 ——
    UFUNCTION(BlueprintCallable, Category = "Achievements")
    void UnlockAchievement(EAchievementID AchievementID);

    // —— 进度型成就 ——
    UFUNCTION(BlueprintCallable, Category = "Achievements")
    void AddProgress(EAchievementID AchievementID, int32 Amount = 1);

    // —— 查询 ——
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Achievements")
    bool IsUnlocked(EAchievementID AchievementID) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Achievements")
    int32 GetProgress(EAchievementID AchievementID) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Achievements")
    TArray<FAchievementData> GetAllAchievements() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Achievements")
    TArray<FAchievementData> GetUnlockedAchievements() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Achievements")
    float GetCompletionPercentage() const;

    // —— Steam 云存档 ——
    UFUNCTION(BlueprintCallable, Category = "Steam|Cloud")
    bool WriteCloudFile(const FString& FileName, const TArray<uint8>& Data);

    UFUNCTION(BlueprintCallable, Category = "Steam|Cloud")
    bool ReadCloudFile(const FString& FileName, TArray<uint8>& OutData);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Steam|Cloud")
    bool DoesCloudFileExist(const FString& FileName) const;

    UFUNCTION(BlueprintCallable, Category = "Steam|Cloud")
    void DeleteCloudFile(const FString& FileName);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Steam|Cloud")
    TArray<FString> GetCloudFileNames() const;

    // —— Steam Rich Presence ——
    UFUNCTION(BlueprintCallable, Category = "Steam|Presence")
    void SetRichPresence(const FString& Key, const FString& Value);

    UFUNCTION(BlueprintCallable, Category = "Steam|Presence")
    void ClearRichPresence();

    // —— 事件 ——
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAchievementUnlocked, EAchievementID, AchievementID);
    UPROPERTY(BlueprintAssignable, Category = "Achievements|Events")
    FOnAchievementUnlocked OnAchievementUnlocked;

private:
    // 成就存储
    UPROPERTY()
    TMap<EAchievementID, FAchievementData> AchievementMap;

    // Steam API 调用
    void NotifySteamAchievement(const FString& SteamID);
    void SyncFromSteam();
    void SyncToSteam(EAchievementID ID);

    // 成就定义
    void DefineAchievement(EAchievementID ID, const FString& Title,
        const FString& Desc, int32 MaxProg, const FString& SteamID,
        bool bHidden = false);
};
