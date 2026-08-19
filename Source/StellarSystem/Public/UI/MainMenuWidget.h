// MainMenuWidget.h
// 主菜单 UI：启动画面 → 法律声明 → 登录/注册 → 主菜单
// 纯 C++ Widget（不依赖 UMG 蓝图，开箱即用）

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"

class UVerticalBox;
class UHorizontalBox;
class UButton;
class UTextBlock;
class UEditableTextBox;
class UCheckBox;
class UImage;
class UBorder;

UENUM(BlueprintType)
enum class EMenuState : uint8
{
    Splash,        // 启动画面（公司 Logo）
    LegalNotice,    // 法律声明
    Login,         // 登录
    Register,      // 注册
    MainMenu,      // 主菜单
    Settings,      // 设置
    Multiplayer,    // 多人
    Loading         // 加载中
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMenuStateChanged, EMenuState, NewState);

UCLASS(Blueprintable, BlueprintType)
class UMainMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UMainMenuWidget(const FObjectInitializer& ObjectInitializer);

    // —— 菜单状态 ——
    UPROPERTY(BlueprintReadOnly, Category = "Menu")
    EMenuState CurrentState = EMenuState::Splash;

    UPROPERTY(BlueprintAssignable, Category = "Menu|Events")
    FOnMenuStateChanged OnMenuStateChanged;

    // —— 公共接口 ——
    UFUNCTION(BlueprintCallable, Category = "Menu")
    void SwitchToState(EMenuState NewState);

    UFUNCTION(BlueprintCallable, Category = "Menu")
    void AttemptLogin(const FString& Username, const FString& Password);

    UFUNCTION(BlueprintCallable, Category = "Menu")
    void AttemptRegister(const FString& Username, const FString& Password,
        const FString& Email);

    UFUNCTION(BlueprintCallable, Category = "Menu")
    void StartNewGame();

    UFUNCTION(BlueprintCallable, Category = "Menu")
    void ContinueGame();

    UFUNCTION(BlueprintCallable, Category = "Menu")
    void OpenSettings();

    UFUNCTION(BlueprintCallable, Category = "Menu")
    void OpenMultiplayer();

    UFUNCTION(BlueprintCallable, Category = "Menu")
    void QuitGame();

    UFUNCTION(BlueprintCallable, Category = "Menu")
    void AcceptLegalNotice();

    UFUNCTION(BlueprintCallable, Category = "Menu")
    void DeclineLegalNotice();

    // —— 设置 ——
    UFUNCTION(BlueprintCallable, Category = "Settings")
    void SetMasterVolume(float Volume);

    UFUNCTION(BlueprintCallable, Category = "Settings")
    void SetMusicVolume(float Volume);

    UFUNCTION(BlueprintCallable, Category = "Settings")
    void SetSFXVolume(float Volume);

    UFUNCTION(BlueprintCallable, Category = "Settings")
    void SetResolution(int32 Width, int32 Height, bool bFullscreen);

    UFUNCTION(BlueprintCallable, Category = "Settings")
    void SetLanguage(const FString& CultureCode);

    // —— 事件回调（蓝图可覆盖） ——
    UFUNCTION(BlueprintImplementableEvent, Category = "Menu|Events")
    void OnLoginSuccess(const FString& Username);

    UFUNCTION(BlueprintImplementableEvent, Category = "Menu|Events")
    void OnLoginFailed(const FString& ErrorMessage);

    UFUNCTION(BlueprintImplementableEvent, Category = "Menu|Events")
    void OnRegisterSuccess(const FString& Username);

    UFUNCTION(BlueprintImplementableEvent, Category = "Menu|Events")
    void OnRegisterFailed(const FString& ErrorMessage);

    UFUNCTION(BlueprintImplementableEvent, Category = "Menu|Events")
    void OnSplashFinished();

    UFUNCTION(BlueprintImplementableEvent, Category = "Menu|Events")
    void OnLegalNoticeAccepted();

    UFUNCTION(BlueprintImplementableEvent, Category = "Menu|Events")
    void OnLegalNoticeDeclined();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // 启动画面计时
    UPROPERTY(BlueprintReadOnly, Category = "Menu|Splash")
    float SplashTimer = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Splash")
    float SplashDuration = 4.f; // 公司 Logo 显示 4 秒

    // 公司信息
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Splash")
    FString CompanyName = TEXT("Stellar Forge Studios");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Splash")
    FString GameVersion = TEXT("v6.3");

    // 法律声明文本
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Legal")
    FString LegalNoticeText = TEXT(
        "LEGAL NOTICE\n\n"
        "© 2026 Stellar Forge Studios. All Rights Reserved.\n\n"
        "This software is protected by copyright law and international treaties. "
        "Unauthorized reproduction or distribution of this program, or any portion of it, "
        "may result in severe civil and criminal penalties.\n\n"
        "STEAM WORKSHOP: User-generated content is the property of its respective creators. "
        "Stellar Forge Studios does not endorse or assume responsibility for "
        "user-generated content.\n\n"
        "PRIVACY: This game collects anonymous gameplay telemetry to improve your experience. "
        "No personal data is sold to third parties. See our Privacy Policy at "
        "https://stellarsystem.game/privacy\n\n"
        "RATINGS: Rated T for Teen by ESRB. Contains mild violence and online interactions.\n\n"
        "By clicking 'Accept', you agree to the End User License Agreement (EULA) "
        "and acknowledge that you have read the Privacy Policy."
    );

    // 网络请求（登录/注册走 GameMode）
    UFUNCTION()
    void HandleLoginResponse(bool bSuccess, const FString& Message);

    UFUNCTION()
    void HandleRegisterResponse(bool bSuccess, const FString& Message);

private:
    // 自动跳转逻辑
    void AutoAdvanceFromSplash();
    void ShowLegalNotice();
    void ShowLoginScreen();
    void ShowRegisterScreen();
    void ShowMainMenu();
    void ShowSettings();
    void ShowMultiplayer();
    void ShowLoading();

    // 验证输入
    bool ValidateUsername(const FString& Username, FString& OutError);
    bool ValidatePassword(const FString& Password, FString& OutError);
    bool ValidateEmail(const FString& Email, FString& OutError);

    // 网络回调绑定
    FDelegateHandle LoginDelegateHandle;
    FDelegateHandle RegisterDelegateHandle;

    // 当前用户
    FString CurrentUsername;
    bool bIsLoggedIn = false;
};
