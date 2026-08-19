// PauseMenuWidget.cpp
// 暂停菜单 + 死亡复活界面实现

#include "UI/PauseMenuWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "SaveSystem/SaveManager.h"
#include "Quests/QuestSystemV2.h"
#include "SteamIntegration/SteamAchievements.h"

UPauseMenuWidget::UPauseMenuWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    CountdownTimer = 0.f;
    bIsDead = false;
}

void UPauseMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SwitchTab(EPauseMenuTab::SaveLoad);
}

void UPauseMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (bIsDead && CountdownTimer > 0.f)
    {
        TickCountdown(InDeltaTime);
    }
}

void UPauseMenuWidget::SwitchTab(EPauseMenuTab NewTab)
{
    CurrentTab = NewTab;
    OnTabChanged.Broadcast(NewTab);

    switch (NewTab)
    {
    case EPauseMenuTab::SaveLoad:
        PopulateSaveList();
        break;
    case EPauseMenuTab::Quests:
        PopulateQuestList();
        break;
    case EPauseMenuTab::Achievements:
        PopulateAchievementList();
        break;
    case EPauseMenuTab::Respawn:
        // 显示复活点列表
        break;
    case EPauseMenuTab::Spectate:
        // 显示观战目标列表
        break;
    default:
        break;
    }
}

// —— 存档/读档 ——

void UPauseMenuWidget::SaveGame(const FString& SlotName)
{
    UWorld* World = GetWorld();
    if (!World) return;

    AStellarGameMode* GM = World->GetAuthGameMode<AStellarGameMode>();
    if (!GM || !GM->SaveManager) return;

    bool bSuccess = GM->SaveManager->SaveGame(SlotName);
    UE_LOG(LogTemp, Log, TEXT("[PauseMenu] Save %s: %s"),
        *SlotName, bSuccess ? TEXT("OK") : TEXT("FAIL"));
}

void UPauseMenuWidget::LoadGame(const FString& SlotName)
{
    UWorld* World = GetWorld();
    if (!World) return;

    AStellarGameMode* GM = World->GetAuthGameMode<AStellarGameMode>();
    if (!GM || !GM->SaveManager) return;

    bool bSuccess = GM->SaveManager->LoadGame(SlotName);
    if (bSuccess)
    {
        // 关闭暂停菜单
        RemoveFromParent();
        // 恢复游戏
        World->GetFirstPlayerController()->SetPause(false);
    }
}

TArray<FString> UPauseMenuWidget::GetSaveSlotList() const
{
    UWorld* World = GetWorld();
    if (!World) return {};

    AStellarGameMode* GM = World->GetAuthGameMode<AStellarGameMode>();
    if (!GM || !GM->SaveManager) return {};

    return GM->SaveManager->GetSaveSlotList();
}

FString UPauseMenuWidget::GetSaveInfo(const FString& SlotName) const
{
    UWorld* World = GetWorld();
    if (!World) return TEXT("");

    AStellarGameMode* GM = World->GetAuthGameMode<AStellarGameMode>();
    if (!GM || !GM->SaveManager) return TEXT("");

    FString Info;
    GM->SaveManager->GetSaveInfo(SlotName, Info);
    return Info;
}

void UPauseMenuWidget::DeleteSave(const FString& SlotName)
{
    UWorld* World = GetWorld();
    if (!World) return;

    AStellarGameMode* GM = World->GetAuthGameMode<AStellarGameMode>();
    if (!GM || !GM->SaveManager) return;

    GM->SaveManager->DeleteSave(SlotName);
}

void UPauseMenuWidget::QuickSave()
{
    FString SlotName = FString::Printf(TEXT("QuickSave_%s"),
        *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
    SaveGame(SlotName);
}

void UPauseMenuWidget::QuickLoad()
{
    TArray<FString> Slots = GetSaveSlotList();
    if (Slots.Num() == 0) return;

    // 找最新的 QuickSave
    FString Latest;
    for (const FString& S : Slots)
    {
        if (S.Contains(TEXT("QuickSave")) || S.Contains(TEXT("AutoSave")))
        {
            if (Latest.IsEmpty() || S > Latest)
                Latest = S;
        }
    }

    // 没有快速存档就加载最新的普通存档
    if (Latest.IsEmpty()) Latest = Slots.Last();

    LoadGame(Latest);
}

// —— 复活系统 ——

void UPauseMenuWidget::SetDeathState(const FString& DeathCause, float CountdownTime)
{
    bIsDead = true;
    LastDeathCause = DeathCause;
    CountdownTimer = CountdownTime > 0.f ? CountdownTime : DefaultRespawnDelay;
    RespawnCountdown = CountdownTimer;

    SwitchTab(EPauseMenuTab::Respawn);
}

void UPauseMenuWidget::TickCountdown(float DeltaTime)
{
    CountdownTimer -= DeltaTime;
    RespawnCountdown = CountdownTimer;

    if (CountdownTimer <= 0.f)
    {
        CountdownTimer = 0.f;

        if (bAutoRespawn)
        {
            AutoRespawnIfEnabled();
        }
    }
}

void UPauseMenuWidget::AutoRespawnIfEnabled()
{
    if (!bAutoRespawn)
    {
        // 切换到手动选择复活点
        SwitchTab(EPauseMenuTab::Respawn);
        return;
    }

    FString Nearest = GetNearestRespawnPoint();
    if (!Nearest.IsEmpty())
    {
        RespawnAtPoint(Nearest);
    }
    else
    {
        // 没有复活点 → 返回主菜单
        ReturnToMainMenu();
    }
}

void UPauseMenuWidget::RespawnAtPoint(const FString& RespawnPointID)
{
    SelectedRespawnPoint = RespawnPointID;

    // 通知 GameMode 复活
    UWorld* World = GetWorld();
    if (World)
    {
        AStellarGameMode* GM = World->GetAuthGameMode<AStellarGameMode>();
        if (GM)
        {
            GM->RespawnPlayerAt(RespawnPointID);
        }
    }

    bIsDead = false;
    CountdownTimer = 0.f;

    OnRespawnSelected.Broadcast(RespawnPointID);

    // 关闭菜单
    RemoveFromParent();
}

void UPauseMenuWidget::EnterSpectatorMode()
{
    UWorld* World = GetWorld();
    if (World)
    {
        AStellarGameMode* GM = World->GetAuthGameMode<AStellarGameMode>();
        if (GM)
        {
            GM->EnterSpectatorMode(World->GetFirstPlayerController());
        }
    }

    SwitchTab(EPauseMenuTab::Spectate);
}

void UPauseMenuWidget::SetRespawnPoints(const TArray<FString>& PointNames,
    const TArray<FString>& PointIDs)
{
    RespawnPointNames = PointNames;
    RespawnPointIDs = PointIDs;
}

FString UPauseMenuWidget::GetNearestRespawnPoint() const
{
    if (RespawnPointIDs.Num() == 0) return TEXT("");

    // 简化：返回第一个（实际应计算距离）
    return RespawnPointIDs[0];
}

// —— 退出 ——

void UPauseMenuWidget::ConfirmQuit(bool bSaveBeforeQuit)
{
    if (bSaveBeforeQuit)
    {
        QuickSave();
    }

    OnQuitConfirmed.Broadcast(true);

    // 返回主菜单或退出
    UWorld* World = GetWorld();
    if (World)
    {
        UKismetSystemLibrary::QuitGame(World,
            World->GetFirstPlayerController(), EQuitPreference::Quit, false);
    }
}

void UPauseMenuWidget::ReturnToMainMenu()
{
    OnReturnToMainMenu.Broadcast();

    // 保存后回到主菜单
    QuickSave();

    UWorld* World = GetWorld();
    if (World)
    {
        // 加载主菜单地图
        UGameplayStatics::OpenLevel(World, FName(TEXT("MainMenuMap")));
    }
}

// —— 任务/成就 ——

TArray<FString> UPauseMenuWidget::GetActiveQuests() const
{
    UWorld* World = GetWorld();
    if (!World) return {};

    AStellarGameMode* GM = World->GetAuthGameMode<AStellarGameMode>();
    if (!GM || !GM->QuestSystem) return {};

    return GM->QuestSystem->GetActiveQuestIDs();
}

FString UPauseMenuWidget::GetQuestDescription(const FString& QuestID) const
{
    UWorld* World = GetWorld();
    if (!World) return TEXT("");

    AStellarGameMode* GM = World->GetAuthGameMode<AStellarGameMode>();
    if (!GM || !GM->QuestSystem) return TEXT("");

    return GM->QuestSystem->GetQuestDescription(QuestID);
}

TArray<FString> UPauseMenuWidget::GetUnlockedAchievements() const
{
    // 从 Steam 成就系统获取
    TArray<FString> Result;
    // ... 实际实现查询 SteamAchievements 组件
    return Result;
}

TArray<FString> UPauseMenuWidget::GetLockedAchievements() const
{
    TArray<FString> Result;
    // ... 查询未解锁成就
    return Result;
}

float UPauseMenuWidget::GetAchievementProgress(const FString& AchievementID) const
{
    // 返回 0~1 进度
    return 0.f;
}

// —— 私有方法 ——

void UPauseMenuWidget::PopulateSaveList()
{
    // 蓝图端实现具体 UI 列表填充
}

void UPauseMenuWidget::PopulateQuestList()
{
    // 蓝图端实现
}

void UPauseMenuWidget::PopulateAchievementList()
{
    // 蓝图端实现
}
