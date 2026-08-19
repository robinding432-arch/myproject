// MobileMainMenuWidget.h
// v7.2 — Touch-optimized main menu for mobile

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MobileMainMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UImage;
class UVerticalBox;
class UHorizontalBox;

/**
 * WMobileMainMenu — simplified main menu for touch screens
 * Large buttons, minimal text, big touch targets
 */
UCLASS(BlueprintType)
class STELLARSYSTEM_API UMobileMainMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

    /** Set the subtitle text (version info) */
    UFUNCTION(BlueprintCallable, Category = "Mobile Menu")
    void SetSubtitle(const FString& Text);

    /** Show/hide the loading spinner */
    UFUNCTION(BlueprintCallable, Category = "Mobile Menu")
    void SetLoadingVisible(bool bVisible);

    /** Show a temporary toast message */
    UFUNCTION(BlueprintCallable, Category = "Mobile Menu")
    void ShowToast(const FString& Message, float DurationSeconds = 3.f);

    /** Animate the background stars (subtle parallax) */
    UFUNCTION(BlueprintCallable, Category = "Mobile Menu")
    void StartBackgroundAnimation();

    /** Stop background animation */
    UFUNCTION(BlueprintCallable, Category = "Mobile Menu")
    void StopBackgroundAnimation();

    /** Event: New Game pressed */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNewGamePressed);
    UPROPERTY(BlueprintAssignable, Category = "Mobile Menu")
    FOnNewGamePressed OnNewGamePressed;

    /** Event: Continue pressed */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnContinuePressed);
    UPROPERTY(BlueprintAssignable, Category = "Mobile Menu")
    FOnContinuePressed OnContinuePressed;

    /** Event: Settings pressed */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSettingsPressed);
    UPROPERTY(BlueprintAssignable, Category = "Mobile Menu")
    FOnSettingsPressed OnSettingsPressed;

    /** Event: Multiplayer pressed */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMultiplayerPressed);
    UPROPERTY(BlueprintAssignable, Category = "Mobile Menu")
    FOnMultiplayerPressed OnMultiplayerPressed;

    /** Event: Quit pressed */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnQuitPressed);
    UPROPERTY(BlueprintAssignable, Category = "Mobile Menu")
    FOnQuitPressed OnQuitPressed;

protected:
    /** UI Bindings (set in BP subclass) */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UButton* BtnNewGame;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UButton* BtnContinue;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UButton* BtnSettings;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UButton* BtnMultiplayer;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UButton* BtnQuit;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* TxtSubtitle;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* TxtToast;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UImage* ImgLoading;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UImage* ImgBackground;

    /** Button press handlers */
    UFUNCTION()
    void HandleNewGamePressed();

    UFUNCTION()
    void HandleContinuePressed();

    UFUNCTION()
    void HandleSettingsPressed();

    UFUNCTION()
    void HandleMultiplayerPressed();

    UFUNCTION()
    void HandleQuitPressed();

private:
    /** Toast timer */
    FTimerHandle ToastTimer;

    /** Background animation */
    FTimerHandle BgAnimTimer;
    float BgAnimTime = 0.f;
    bool bBgAnimActive = false;

    void UpdateBackgroundAnimation();
};
