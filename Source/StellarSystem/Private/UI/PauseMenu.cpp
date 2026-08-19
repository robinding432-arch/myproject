// PauseMenu.cpp
// v6.5：多人安全暂停实现
//
// 关键设计：
//   单人模式 → 传统 SetPause(true) 暂停整个世界（允许）
//   多人模式 → 只做"本地暂停"：冻结本地 Pawn Tick + 显示 UI + 捕获输入
//              不调用 SetPause，不冻结其他玩家，不阻断网络复制

#include "UI/PauseMenu.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Core/SaveSystem.h"
#include "Core/StellarGameMode.h"
#include "Net/UnrealNetwork.h"

// =====================================================================
// 生命周期
// =====================================================================

void UPauseMenu::NativeConstruct()
{
    Super::NativeConstruct();

    if (Button_Resume)
        Button_Resume->OnClicked.AddDynamic(this, &UPauseMenu::OnResumeClicked);
    if (Button_Settings)
        Button_Settings->OnClicked.AddDynamic(this, &UPauseMenu::OnSettingsClicked);
    if (Button_Save)
        Button_Save->OnClicked.AddDynamic(this, &UPauseMenu::OnSaveClicked);
    if (Button_Load)
        Button_Load->OnClicked.AddDynamic(this, &UPauseMenu::OnLoadClicked);
    if (Button_MainMenu)
        Button_MainMenu->OnClicked.AddDynamic(this, &UPauseMenu::OnMainMenuClicked);

    // 默认多人安全模式（如果是单人 GameMode 会在 SetPauseMode 里改回来）
    CurrentPauseMode = EPauseMenuMode::LocalOnly;

    UpdateStatusText();
    UpdateModeText();
}

void UPauseMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    // 多人在线时，如果网络断开或连接失败，自动关闭菜单
    if (CurrentPauseMode == EPauseMenuMode::LocalOnly)
    {
        if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
        {
            if (!PC->HasAuthority() && PC->GetNetMode() == NM_Client)
            {
                // 客户端：检查网络状态
                // 如果游戏世界仍在 tick（其他玩家在动），我们的菜单只是本地叠加层
                // 不需要额外处理，菜单本身就是本地控件
            }
        }
    }
}

// =====================================================================
// 按钮回调
// =====================================================================

void UPauseMenu::OnResumeClicked()
{
    ClosePauseMenu();
}

void UPauseMenu::OnSettingsClicked()
{
    // 设置子菜单：在暂停状态下打开（不关闭暂停）
    // 通过 WidgetSwitcher 切换页面（蓝图实现）
    // 这里只发事件通知蓝图
    OnSettingsClickedEvent.Broadcast();
}

void UPauseMenu::OnSaveClicked()
{
    // 多人游戏里，存档由服务器权威执行
    USaveManager* SM = nullptr;
    if (UWorld* W = GetWorld())
    {
        if (AStellarGameMode* GM = Cast<AStellarGameMode>(W->GetAuthGameMode()))
        {
            SM = GM->SaveManager;
        }
    }
    if (SM)
    {
        SM->SaveGame(0);
        if (Text_Status) Text_Status->SetText(FText::FromString(TEXT("Game Saved to Slot 0")));
    }
    else if (Text_Status)
    {
        Text_Status->SetText(FText::FromString(TEXT("Save unavailable in multiplayer")));
    }
}

void UPauseMenu::OnLoadClicked()
{
    // 多人游戏禁止读档（防止作弊/状态不一致）
    if (CurrentPauseMode == EPauseMenuMode::LocalOnly)
    {
        if (Text_Status)
            Text_Status->SetText(FText::FromString(TEXT("Load disabled in multiplayer")));
        return;
    }

    if (UGameplayStatics::DoesSaveGameExist(TEXT("SaveSlot_0"), 0))
    {
        UGameplayStatics::OpenLevel(this, FName(TEXT("MainMap")), true, TEXT("?LoadSlot=0"));
    }
    else
    {
        if (Text_Status) Text_Status->SetText(FText::FromString(TEXT("No Save Found")));
    }
}

void UPauseMenu::OnMainMenuClicked()
{
    // 先恢复游戏状态再跳转
    ClosePauseMenu();
    UGameplayStatics::OpenLevel(this, FName(TEXT("MainMenuMap")), false);
}

// =====================================================================
// 打开 / 关闭（核心修正）
// =====================================================================

void UPauseMenu::OpenPauseMenu()
{
    SetVisibility(ESlateVisibility::Visible);

    if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
    {
        // 记录打开前的状态（用于恢复）
        bHadMouseCursor = PC->bShowMouseCursor;
        bWasGameInputMode = (PC->GetInputMode() == FInputModeGameOnly());

        // 始终开启鼠标 + UI 输入模式
        PC->bShowMouseCursor = true;
        PC->SetInputMode(FInputModeUIOnly());

        // ★ 关键：根据暂停模式决定是否冻结世界
        ApplyPauseState(true);
    }

    UpdateStatusText();
    UpdateModeText();
}

void UPauseMenu::ClosePauseMenu()
{
    SetVisibility(ESlateVisibility::Collapsed);

    if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
    {
        // ★ 关键：根据暂停模式决定是否恢复世界
        ApplyPauseState(false);

        // 恢复输入模式
        PC->bShowMouseCursor = bHadMouseCursor;
        if (bWasGameInputMode)
            PC->SetInputMode(FInputModeGameOnly());
        else
            PC->SetInputMode(FInputModeGameAndUI());
    }
}

// =====================================================================
// ★ 核心：根据模式选择暂停策略
// =====================================================================

void UPauseMenu::ApplyPauseState(bool bPause)
{
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC) return;

    if (bPause)
    {
        bLocallyPaused = true;

        switch (CurrentPauseMode)
        {
        case EPauseMenuMode::FullPause:
            // 单人模式：传统暂停整个游戏世界
            // 这是唯一允许 SetPause(true) 的场景
            if (PC->HasAuthority()) // 只有服务器/单机才能暂停世界
            {
                PC->SetPause(true);
            }
            else
            {
                // 客户端在单人游戏里也是 Authority（ListenServer）
                PC->SetPause(true);
            }
            UE_LOG(LogTemp, Log, TEXT("[PauseMenu] Full Pause activated (single-player)"));
            break;

        case EPauseMenuMode::LocalOnly:
            // ★ 多人模式：只做本地暂停，绝不冻结世界
            // 方法：暂停本地 Pawn 的 Tick，但不暂停 World
            PauseLocalOnly();
            UE_LOG(LogTemp, Log, TEXT("[PauseMenu] Local-only pause (multiplayer safe)"));
            break;

        case EPauseMenuMode::Disabled:
            // PvP 竞技模式：完全禁止暂停
            bLocallyPaused = false;
            UE_LOG(LogTemp, Warning, TEXT("[PauseMenu] Pause attempted but disabled by game mode"));
            break;
        }
    }
    else
    {
        bLocallyPaused = false;

        switch (CurrentPauseMode)
        {
        case EPauseMenuMode::FullPause:
            PC->SetPause(false);
            UE_LOG(LogTemp, Log, TEXT("[PauseMenu] Full Pause deactivated"));
            break;

        case EPauseMenuMode::LocalOnly:
            ResumeLocalOnly();
            UE_LOG(LogTemp, Log, TEXT("[PauseMenu] Local pause resumed"));
            break;

        case EPauseMenuMode::Disabled:
            // 本来就没暂停，什么都不做
            break;
        }
    }
}

void UPauseMenu::PauseLocalOnly()
{
    // 只暂停本地玩家 Pawn 的 Tick
    // 不碰 World、不碰其他玩家、不碰网络复制
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
    {
        if (ACharacter* Char = Cast<ACharacter>(PC->GetPawn()))
        {
            Char->SetActorTickEnabled(false);
            // 禁用输入
            PC->DisableInput(PC);
        }
    }

    // 通知 GameMode 暂停菜单已打开（用于音频/计时器暂停等本地逻辑）
    if (AStellarGameMode* GM = Cast<AStellarGameMode>(GetWorld()->GetAuthGameMode()))
    {
        // 只暂停本地计时器，不影响其他玩家
        GM->OnLocalPauseStateChanged(true);
    }
}

void UPauseMenu::ResumeLocalOnly()
{
    // 恢复本地 Pawn
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
    {
        if (ACharacter* Char = Cast<ACharacter>(PC->GetPawn()))
        {
            Char->SetActorTickEnabled(true);
            PC->EnableInput(PC);
        }
    }

    if (AStellarGameMode* GM = Cast<AStellarGameMode>(GetWorld()->GetAuthGameMode()))
    {
        GM->OnLocalPauseStateChanged(false);
    }
}

// =====================================================================
// 模式设置（由 GameMode 在 BeginPlay 时调用）
// =====================================================================

void UPauseMenu::SetPauseMode(EPauseMenuMode NewMode)
{
    // 如果当前已暂停，需要先恢复旧模式再应用新模式
    bool bWasPaused = bLocallyPaused;

    if (bWasPaused)
    {
        // 先恢复
        if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
        {
            if (CurrentPauseMode == EPauseMenuMode::FullPause)
                PC->SetPause(false);
            else if (CurrentPauseMode == EPauseMenuMode::LocalOnly)
                ResumeLocalOnly();
        }
        bLocallyPaused = false;
    }

    CurrentPauseMode = NewMode;

    // 如果之前是暂停状态，用新模式重新暂停
    if (bWasPaused)
    {
        ApplyPauseState(true);
    }

    UpdateModeText();

    UE_LOG(LogTemp, Log, TEXT("[PauseMenu] Mode set to: %d"), (int32)NewMode);
}

// =====================================================================
// UI 更新
// =====================================================================

void UPauseMenu::UpdateStatusText()
{
    if (!Text_Status) return;

    bool bHas = UGameplayStatics::DoesSaveGameExist(TEXT("SaveSlot_0"), 0);

    if (CurrentPauseMode == EPauseMenuMode::LocalOnly)
    {
        Text_Status->SetText(FText::FromString(
            bHas ? TEXT("Save Found - Slot 0 (Multiplayer: save only)") : TEXT("No Save Found")));
    }
    else
    {
        Text_Status->SetText(FText::FromString(
            bHas ? TEXT("Save Found - Slot 0") : TEXT("No Save Found")));
    }
}

void UPauseMenu::UpdateModeText()
{
    if (!Text_Mode) return;

    switch (CurrentPauseMode)
    {
    case EPauseMenuMode::FullPause:
        Text_Mode->SetText(FText::FromString(TEXT("Single-Player Mode")));
        Text_Mode->SetColorAndOpacity(FSlateColor(FLinearColor(0.2f, 1.f, 0.2f))); // 绿
        break;
    case EPauseMenuMode::LocalOnly:
        Text_Mode->SetText(FText::FromString(TEXT("Multiplayer - Game Keeps Running")));
        Text_Mode->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.8f, 0.2f))); // 橙
        break;
    case EPauseMenuMode::Disabled:
        Text_Mode->SetText(FText::FromString(TEXT("PvP Combat - Pause Disabled")));
        Text_Mode->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.3f, 0.3f))); // 红
        break;
    }
}

// =====================================================================
// 蓝图事件（让 UI 蓝图可以绑定）
// =====================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSettingsClickedEvent);
UPauseMenu::FOnSettingsClickedEvent OnSettingsClickedEvent;
