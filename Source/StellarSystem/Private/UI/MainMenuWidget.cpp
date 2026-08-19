// MainMenuWidget.cpp
// 主菜单 UI 实现

#include "UI/MainMenuWidget.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/CheckBox.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Account/AccountSystem.h"

UMainMenuWidget::UMainMenuWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SplashTimer = 0.f;
}

void UMainMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 启动画面 → 4 秒后自动跳转
    SwitchToState(EMenuState::Splash);
}

void UMainMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (CurrentState == EMenuState::Splash)
    {
        SplashTimer += InDeltaTime;
        if (SplashTimer >= SplashDuration)
        {
            AutoAdvanceFromSplash();
        }
    }
}

void UMainMenuWidget::SwitchToState(EMenuState NewState)
{
    CurrentState = NewState;
    OnMenuStateChanged.Broadcast(NewState);

    switch (NewState)
    {
    case EMenuState::Splash:
        OnSplashFinished();
        break;
    case EMenuState::LegalNotice:
        ShowLegalNotice();
        break;
    case EMenuState::Login:
        ShowLoginScreen();
        break;
    case EMenuState::Register:
        ShowRegisterScreen();
        break;
    case EMenuState::MainMenu:
        ShowMainMenu();
        break;
    case EMenuState::Settings:
        ShowSettings();
        break;
    case EMenuState::Multiplayer:
        ShowMultiplayer();
        break;
    case EMenuState::Loading:
        ShowLoading();
        break;
    }
}

void UMainMenuWidget::AutoAdvanceFromSplash()
{
    // 启动画面结束 → 法律声明
    SwitchToState(EMenuState::LegalNotice);
}

void UMainMenuWidget::ShowLegalNotice()
{
    // 显示法律声明 + Accept/Decline 按钮
    // 蓝图端实现具体 UI 布局
    OnLegalNoticeAccepted();
}

void UMainMenuWidget::AcceptLegalNotice()
{
    // 保存用户同意记录
    GConfig->SetBool(TEXT("Legal"), TEXT("bAcceptedNotice"), true, GGameIni);
    GConfig->Flush(false, GGameIni);

    // 进入登录界面
    SwitchToState(EMenuState::Login);
}

void UMainMenuWidget::DeclineLegalNotice()
{
    // 拒绝 → 退出游戏
    OnLegalNoticeDeclined();
    QuitGame();
}

void UMainMenuWidget::ShowLoginScreen()
{
    // 用户名 + 密码输入框 + 登录按钮 + 注册链接
    // 蓝图实现具体布局
}

void UMainMenuWidget::ShowRegisterScreen()
{
    // 用户名 + 密码 + 确认密码 + 邮箱 + 注册按钮
    // 蓝图实现具体布局
}

void UMainMenuWidget::ShowMainMenu()
{
    // 新游戏 / 继续 / 多人 / 设置 / 退出
    // 蓝图实现具体布局
}

void UMainMenuWidget::ShowSettings()
{
    // 音量滑块 / 分辨率下拉 / 语言选择 / 画质预设
    // 蓝图实现具体布局
}

void UMainMenuWidget::ShowMultiplayer()
{
    // 创建房间 / 搜索房间 / 好友列表 / 快速匹配
    // 蓝图实现具体布局
}

void UMainMenuWidget::ShowLoading()
{
    // 加载进度条 + 提示文字
    // 蓝图实现具体布局
}

// —— 登录/注册逻辑 ——

void UMainMenuWidget::AttemptLogin(const FString& Username, const FString& Password)
{
    if (Username.IsEmpty() || Password.IsEmpty())
    {
        OnLoginFailed(TEXT("用户名和密码不能为空"));
        return;
    }

    // 获取 GameMode 的 AccountSystem
    UWorld* World = GetWorld();
    if (!World) return;

    AStellarGameMode* GM = World->GetAuthGameMode<AStellarGameMode>();
    if (!GM || !GM->AccountSystem)
    {
        OnLoginFailed(TEXT("服务器未就绪，请重试"));
        return;
    }

    // 调用 AccountSystem 登录
    FString ErrorMsg;
    bool bSuccess = GM->AccountSystem->Login(Username, Password, ErrorMsg);

    if (bSuccess)
    {
        bIsLoggedIn = true;
        CurrentUsername = Username;
        OnLoginSuccess(Username);

        // 进入主菜单
        SwitchToState(EMenuState::MainMenu);
    }
    else
    {
        OnLoginFailed(ErrorMsg);
    }
}

void UMainMenuWidget::AttemptRegister(const FString& Username, const FString& Password,
    const FString& Email)
{
    FString ErrorMsg;
    if (!ValidateUsername(Username, ErrorMsg))
    {
        OnRegisterFailed(ErrorMsg);
        return;
    }
    if (!ValidatePassword(Password, ErrorMsg))
    {
        OnRegisterFailed(ErrorMsg);
        return;
    }
    if (!Email.IsEmpty() && !ValidateEmail(Email, ErrorMsg))
    {
        OnRegisterFailed(ErrorMsg);
        return;
    }

    UWorld* World = GetWorld();
    if (!World) return;

    AStellarGameMode* GM = World->GetAuthGameMode<AStellarGameMode>();
    if (!GM || !GM->AccountSystem)
    {
        OnRegisterFailed(TEXT("服务器未就绪"));
        return;
    }

    FString ResultMsg;
    bool bSuccess = GM->AccountSystem->Register(Username, Password, Email, ResultMsg);

    if (bSuccess)
    {
        OnRegisterSuccess(Username);
        // 自动登录
        AttemptLogin(Username, Password);
    }
    else
    {
        OnRegisterFailed(ResultMsg);
    }
}

bool UMainMenuWidget::ValidateUsername(const FString& Username, FString& OutError)
{
    if (Username.Len() < 3)
    {
        OutError = TEXT("用户名至少 3 个字符");
        return false;
    }
    if (Username.Len() > 20)
    {
        OutError = TEXT("用户名最多 20 个字符");
        return false;
    }
    // 只允许字母数字下划线
    for (TCHAR C : Username)
    {
        if (!FChar::IsAlnum(C) && C != '_')
        {
            OutError = TEXT("用户名只能包含字母、数字和下划线");
            return false;
        }
    }
    return true;
}

bool UMainMenuWidget::ValidatePassword(const FString& Password, FString& OutError)
{
    if (Password.Len() < 6)
    {
        OutError = TEXT("密码至少 6 个字符");
        return false;
    }
    if (Password.Len() > 64)
    {
        OutError = TEXT("密码最多 64 个字符");
        return false;
    }
    // 必须包含字母和数字
    bool bHasAlpha = false, bHasDigit = false;
    for (TCHAR C : Password)
    {
        if (FChar::IsAlpha(C)) bHasAlpha = true;
        if (FChar::IsDigit(C)) bHasDigit = true;
    }
    if (!bHasAlpha || !bHasDigit)
    {
        OutError = TEXT("密码必须同时包含字母和数字");
        return false;
    }
    return true;
}

bool UMainMenuWidget::ValidateEmail(const FString& Email, FString& OutError)
{
    if (!Email.Contains(TEXT("@")) || !Email.Contains(TEXT(".")))
    {
        OutError = TEXT("邮箱格式不正确");
        return false;
    }
    return true;
}

// —— 主菜单操作 ——

void UMainMenuWidget::StartNewGame()
{
    if (!bIsLoggedIn)
    {
        SwitchToState(EMenuState::Login);
        return;
    }

    SwitchToState(EMenuState::Loading);

    // 通知 GameMode 开始新游戏
    UWorld* World = GetWorld();
    if (World)
    {
        AStellarGameMode* GM = World->GetAuthGameMode<AStellarGameMode>();
        if (GM)
        {
            GM->StartNewGame(CurrentUsername);
        }
    }

    // 移除菜单，进入游戏
    RemoveFromParent();
}

void UMainMenuWidget::ContinueGame()
{
    if (!bIsLoggedIn)
    {
        SwitchToState(EMenuState::Login);
        return;
    }

    // 加载最新存档
    UWorld* World = GetWorld();
    if (World)
    {
        AStellarGameMode* GM = World->GetAuthGameMode<AStellarGameMode>();
        if (GM && GM->SaveManager)
        {
            // 加载最新存档
            TArray<FString> Slots = GM->SaveManager->GetSaveSlotList();
            if (Slots.Num() > 0)
            {
                Slots.Sort(); // 按时间戳排序
                GM->SaveManager->LoadGame(Slots.Last());
            }
        }
    }

    RemoveFromParent();
}

void UMainMenuWidget::OpenSettings()
{
    SwitchToState(EMenuState::Settings);
}

void UMainMenuWidget::OpenMultiplayer()
{
    if (!bIsLoggedIn)
    {
        SwitchToState(EMenuState::Login);
        return;
    }
    SwitchToState(EMenuState::Multiplayer);
}

void UMainMenuWidget::QuitGame()
{
    UWorld* World = GetWorld();
    if (World)
    {
        UKismetSystemLibrary::QuitGame(World,
            World->GetFirstPlayerController(), EQuitPreference::Quit, false);
    }
}

// —— 设置 ——

void UMainMenuWidget::SetMasterVolume(float Volume)
{
    Volume = FMath::Clamp(Volume, 0.f, 1.f);
    if (UGameUserSettings* Settings = GEngine->GetGameUserSettings())
    {
        Settings->SetAudioQualityLevel(FMath::RoundToInt(Volume * 10.f));
    }

    // 通过 AudioManager 设置
    // AudioManager->SetMasterVolume(Volume);
}

void UMainMenuWidget::SetMusicVolume(float Volume)
{
    Volume = FMath::Clamp(Volume, 0.f, 1.f);
    // AudioManager->SetMusicVolume(Volume);
}

void UMainMenuWidget::SetSFXVolume(float Volume)
{
    Volume = FMath::Clamp(Volume, 0.f, 1.f);
    // AudioManager->SetSFXVolume(Volume);
}

void UMainMenuWidget::SetResolution(int32 Width, int32 Height, bool bFullscreen)
{
    if (UGameUserSettings* Settings = GEngine->GetGameUserSettings())
    {
        Settings->SetScreenResolution(FIntPoint(Width, Height));
        Settings->SetFullscreenMode(bFullscreen ? EWindowMode::Fullscreen
                                              : EWindowMode::Windowed);
        Settings->ApplySettings(true);
    }
}

void UMainMenuWidget::SetLanguage(const FString& CultureCode)
{
    FInternationalization::Get().SetCurrentCulture(CultureCode);
    GConfig->SetString(TEXT("Internationalization"), TEXT("Culture"), *CultureCode, GGameIni);
    GConfig->Flush(false, GGameIni);
}
