// MainMenu.cpp
#include "UI/MainMenu.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Slider.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "Internationalization/Regex.h"

void UMainMenu::NativeConstruct()
{
    Super::NativeConstruct();

    // 绑定按钮
    if (Button_NewGame)
        Button_NewGame->OnClicked.AddDynamic(this, &UMainMenu::OnNewGameClicked);
    if (Button_Continue)
        Button_Continue->OnClicked.AddDynamic(this, &UMainMenu::OnContinueClicked);
    if (Button_Settings)
        Button_Settings->OnClicked.AddDynamic(this, &UMainMenu::OnSettingsClicked);
    if (Button_Quit)
        Button_Quit->OnClicked.AddDynamic(this, &UMainMenu::OnQuitClicked);
    if (Button_BackFromSettings)
        Button_BackFromSettings->OnClicked.AddDynamic(this, &UMainMenu::OnBackFromSettingsClicked);

    // 设置滑块
    if (Slider_MasterVolume)
        Slider_MasterVolume->OnValueChanged.AddDynamic(this, &UMainMenu::OnMasterVolumeChanged);
    if (Slider_MusicVolume)
        Slider_MusicVolume->OnValueChanged.AddDynamic(this, &UMainMenu::OnMusicVolumeChanged);
    if (Slider_SFXVolume)
        Slider_SFXVolume->OnValueChanged.AddDynamic(this, &UMainMenu::OnSFXVolumeChanged);
    if (Slider_MouseSensitivity)
        Slider_MouseSensitivity->OnValueChanged.AddDynamic(this, &UMainMenu::OnMouseSensitivityChanged);

    // 复选框
    if (Check_Fullscreen)
        Check_Fullscreen->OnCheckStateChanged.AddDynamic(this, &UMainMenu::OnFullscreenChanged);
    if (Check_VSync)
        Check_VSync->OnCheckStateChanged.AddDynamic(this, &UMainMenu::OnVSyncChanged);

    // 下拉框
    if (Combo_Quality)
    {
        Combo_Quality->AddOption(TEXT("Low"));
        Combo_Quality->AddOption(TEXT("Medium"));
        Combo_Quality->AddOption(TEXT("High"));
        Combo_Quality->AddOption(TEXT("Epic"));
        Combo_Quality->OnSelectionChanged.AddDynamic(this, &UMainMenu::OnQualityChanged);
    }

    if (Combo_Resolution)
    {
        Combo_Resolution->AddOption(TEXT("1920x1080"));
        Combo_Resolution->AddOption(TEXT("2560x1440"));
        Combo_Resolution->AddOption(TEXT("3840x2160"));
        Combo_Resolution->AddOption(TEXT("1280x720"));
    }

    // 版本号
    if (Text_Version)
        Text_Version->SetText(FText::FromString(TEXT("v5.0")));

    // 刷新继续按钮
    RefreshContinueButton();

    // 加载设置
    LoadSettings();

    // 默认显示主页
    ShowMainPage();
}

void UMainMenu::OnNewGameClicked()
{
    UGameplayStatics::OpenLevel(this, FName(TEXT("MainMap")), true, TEXT("?NewGame=1"));
}

void UMainMenu::OnContinueClicked()
{
    if (HasSaveGame(0))
    {
        UGameplayStatics::OpenLevel(this, FName(TEXT("MainMap")), true, TEXT("?LoadSlot=0"));
    }
}

void UMainMenu::OnSettingsClicked()
{
    ShowSettingsPage();
}

void UMainMenu::OnQuitClicked()
{
#if WITH_EDITOR
    UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
#else
    FGenericPlatformMisc::RequestExit(false);
#endif
}

void UMainMenu::OnBackFromSettingsClicked()
{
    SaveSettings();
    ShowMainPage();
}

void UMainMenu::ShowMainPage()
{
    if (Switcher_Main && Page_MainMenu)
    {
        Switcher_Main->SetActiveWidget(Page_MainMenu);
    }
}

void UMainMenu::ShowSettingsPage()
{
    if (Switcher_Main && Page_Settings)
    {
        Switcher_Main->SetActiveWidget(Page_Settings);
    }
}

void UMainMenu::RefreshContinueButton()
{
    if (Button_Continue)
    {
        bool bHas = HasSaveGame(0);
        Button_Continue->SetIsEnabled(bHas);
    }
}

bool UMainMenu::HasSaveGame(int32 Slot) const
{
    FString SlotName = FString::Printf(TEXT("SaveSlot_%d"), Slot);
    return UGameplayStatics::DoesSaveGameExist(SlotName, 0);
}

// ---- 设置回调 ----
void UMainMenu::OnMasterVolumeChanged(float Value)
{
    UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
    if (Settings)
    {
        // 主音量通过 ConsoleVariable 控制
        // 实际音频分类由 AudioManager 处理
    }
}

void UMainMenu::OnMusicVolumeChanged(float Value)
{
    // 由 AudioManager 处理
}

void UMainMenu::OnSFXVolumeChanged(float Value)
{
    // 由 AudioManager 处理
}

void UMainMenu::OnQualityChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    UGameUserSettings* S = UGameUserSettings::GetGameUserSettings();
    if (!S) return;

    if (SelectedItem == TEXT("Low"))       S->SetOverallScalabilityLevel(0);
    else if (SelectedItem == TEXT("Medium")) S->SetOverallScalabilityLevel(1);
    else if (SelectedItem == TEXT("High"))   S->SetOverallScalabilityLevel(2);
    else if (SelectedItem == TEXT("Epic"))   S->SetOverallScalabilityLevel(3);
    S->ApplySettings(true);
}

void UMainMenu::OnFullscreenChanged(bool bIsChecked)
{
    UGameUserSettings* S = UGameUserSettings::GetGameUserSettings();
    if (!S) return;
    S->SetFullscreenMode(bIsChecked ? EWindowMode::Fullscreen : EWindowMode::Windowed);
    S->ApplySettings(true);
}

void UMainMenu::OnVSyncChanged(bool bIsChecked)
{
    UGameUserSettings* S = UGameUserSettings::GetGameUserSettings();
    if (!S) return;
    S->SetVSyncEnabled(bIsChecked);
    S->ApplySettings(true);
}

void UMainMenu::OnMouseSensitivityChanged(float Value)
{
    GConfig->SetFloat(TEXT("/Script/Engine.InputSettings"), TEXT("MouseSensitivity"), Value, GGameIni);
    GConfig->Flush(false, GGameIni);
}

void UMainMenu::SaveSettings()
{
    UGameUserSettings* S = UGameUserSettings::GetGameUserSettings();
    if (S) S->SaveSettings();

    if (Slider_MasterVolume)
        GConfig->SetFloat(TEXT("StellarSystem"), TEXT("MasterVolume"), Slider_MasterVolume->GetValue(), GGameIni);
    if (Slider_MouseSensitivity)
        GConfig->SetFloat(TEXT("StellarSystem"), TEXT("MouseSensitivity"), Slider_MouseSensitivity->GetValue(), GGameIni);
    GConfig->Flush(false, GGameIni);
}

void UMainMenu::LoadSettings()
{
    float Val = 1.f;
    if (GConfig->GetFloat(TEXT("StellarSystem"), TEXT("MasterVolume"), Val, GGameIni))
    {
        if (Slider_MasterVolume) Slider_MasterVolume->SetValue(Val);
    }
    if (GConfig->GetFloat(TEXT("StellarSystem"), TEXT("MouseSensitivity"), Val, GGameIni))
    {
        if (Slider_MouseSensitivity) Slider_MouseSensitivity->SetValue(Val);
    }
}
