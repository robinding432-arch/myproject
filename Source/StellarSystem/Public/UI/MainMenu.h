// MainMenu.h
// 主菜单 Widget：新游戏 / 继续 / 设置 / 退出
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenu.generated.h"

class UButton;
class UTextBlock;
class UWidgetSwitcher;
class USlider;
class UCheckBox;
class UComboBoxString;

UCLASS(Blueprintable)
class UMainMenu : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

    // ---- 主菜单按钮 ----
    UPROPERTY(meta = (BindWidget))
    UButton* Button_NewGame = nullptr;

    UPROPERTY(meta = (BindWidget))
    UButton* Button_Continue = nullptr;

    UPROPERTY(meta = (BindWidget))
    UButton* Button_Settings = nullptr;

    UPROPERTY(meta = (BindWidget))
    UButton* Button_Quit = nullptr;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* Text_Title = nullptr;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* Text_Version = nullptr;

    // ---- 设置页 ----
    UPROPERTY(meta = (BindWidget))
    UWidgetSwitcher* Switcher_Main = nullptr;

    UPROPERTY(meta = (BindWidget))
    UWidget* Page_MainMenu = nullptr;

    UPROPERTY(meta = (BindWidget))
    UWidget* Page_Settings = nullptr;

    // 设置项
    UPROPERTY(meta = (BindWidget))
    USlider* Slider_MasterVolume = nullptr;

    UPROPERTY(meta = (BindWidget))
    USlider* Slider_MusicVolume = nullptr;

    UPROPERTY(meta = (BindWidget))
    USlider* Slider_SFXVolume = nullptr;

    UPROPERTY(meta = (BindWidget))
    UComboBoxString* Combo_Resolution = nullptr;

    UPROPERTY(meta = (BindWidget))
    UCheckBox* Check_Fullscreen = nullptr;

    UPROPERTY(meta = (BindWidget))
    UCheckBox* Check_VSync = nullptr;

    UPROPERTY(meta = (BindWidget))
    UComboBoxString* Combo_Quality = nullptr;

    UPROPERTY(meta = (BindWidget))
    USlider* Slider_MouseSensitivity = nullptr;

    UPROPERTY(meta = (BindWidget))
    UButton* Button_BackFromSettings = nullptr;

    // ---- 事件绑定 ----
    UFUNCTION()
    void OnNewGameClicked();

    UFUNCTION()
    void OnContinueClicked();

    UFUNCTION()
    void OnSettingsClicked();

    UFUNCTION()
    void OnQuitClicked();

    UFUNCTION()
    void OnBackFromSettingsClicked();

    // 设置变更回调
    UFUNCTION()
    void OnMasterVolumeChanged(float Value);

    UFUNCTION()
    void OnMusicVolumeChanged(float Value);

    UFUNCTION()
    void OnSFXVolumeChanged(float Value);

    UFUNCTION()
    void OnQualityChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    UFUNCTION()
    void OnFullscreenChanged(bool bIsChecked);

    UFUNCTION()
    void OnVSyncChanged(bool bIsChecked);

    UFUNCTION()
    void OnMouseSensitivityChanged(float Value);

    // ---- 外部调用 ----
    UFUNCTION(BlueprintCallable)
    void RefreshContinueButton();

    UFUNCTION(BlueprintCallable)
    void ShowMainPage();

    UFUNCTION(BlueprintCallable)
    void ShowSettingsPage();

protected:
    // 保存设置到 Config
    void SaveSettings();
    void LoadSettings();

    // 检查是否有存档
    bool HasSaveGame(int32 Slot = 0) const;

    // 应用视频设置
    void ApplyResolution();
    void ApplyQuality();
};
