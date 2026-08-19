// TutorialCinematicWidget.h
// 教程过场 UI：开场/结尾的全屏剧情画面

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TutorialCinematicWidget.generated.h"

class UTextBlock;
class UImage;
class UButton;
class UCanvasPanel;

UENUM(BlueprintType)
enum class ETutorialCinematicType : uint8
{
    Intro_Starfield,    // 开场：从太空俯瞰
    Outro_Welcome,      // 结尾：欢迎来到宇宙
    Death_Screen,       // 死亡画面
    Respawn_Cinematic   // 复活过场
};

UCLASS()
class STELLARSYSTEM_API UTutorialCinematicWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(meta = (BindWidget))
    UCanvasPanel* RootCanvas;

    UPROPERTY(meta = (BindWidget))
    UImage* BackgroundImage;

    UPROPERTY(meta = (BindWidget))
    UImage* LogoImage;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* TitleText;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* SubtitleText;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* BodyText;

    UPROPERTY(meta = (BindWidget))
    UButton* ContinueButton;

    UPROPERTY(meta = (BindWidget))
    UImage* ScanlineOverlay;

    UPROPERTY(meta = (BindWidget))
    UImage* VignetteOverlay;

    // —— 设置内容 ——
    UFUNCTION(BlueprintCallable)
    void SetupCinematic(ETutorialCinematicType Type);

    UFUNCTION(BlueprintCallable)
    void SetTitleText(const FText& Text);

    UFUNCTION(BlueprintCallable)
    void SetSubtitleText(const FText& Text);

    UFUNCTION(BlueprintCallable)
    void SetBodyText(const FText& Text);

    UFUNCTION(BlueprintCallable)
    void PlayFadeIn(float Duration = 2.f);

    UFUNCTION(BlueprintCallable)
    void PlayFadeOut(float Duration = 1.5f);

    UFUNCTION(BlueprintCallable)
    void PlayStarfieldAnimation(float Duration = 8.f);

    // —— 完成回调 ——
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCinematicComplete);
    UPROPERTY(BlueprintAssignable)
    FOnCinematicComplete OnCinematicComplete;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnContinueClicked);
    UPROPERTY(BlueprintAssignable)
    FOnContinueClicked OnContinueClicked;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    void OnContinueButtonClicked();
    void AnimateStarfield(float DeltaTime);
    void AnimateScanlines(float DeltaTime);

    float CinematicTimer = 0.f;
    float FadeAlpha = 0.f;
    bool bFadingIn = false;
    bool bFadingOut = false;
    float FadeDuration = 2.f;
    bool bStarfieldActive = false;
    float StarfieldTime = 0.f;
};
