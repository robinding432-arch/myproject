// TutorialPromptWidget.h
// 教程提示 UI：屏幕下方中央显示引导文本 + 进度条

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TutorialPromptWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UButton;
class UImage;

UCLASS()
class STELLARSYSTEM_API UTutorialPromptWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // —— UI 组件（在蓝图里绑定同名控件）——
    UPROPERTY(meta = (BindWidget))
    UTextBlock* PromptText;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* DetailText;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* RewardText;

    UPROPERTY(meta = (BindWidget))
    UProgressBar* ProgressBar;

    UPROPERTY(meta = (BindWidget))
    UButton* SkipButton;

    UPROPERTY(meta = (BindWidget))
    UImage* BackgroundPanel;

    UPROPERTY(meta = (BindWidget))
    UImage* KeyHighlightIcon;

    // —— 设置文本 ——
    UFUNCTION(BlueprintCallable)
    void SetPromptText(const FText& Text);

    UFUNCTION(BlueprintCallable)
    void SetDetailText(const FText& Text);

    UFUNCTION(BlueprintCallable)
    void SetRewardText(const FText& Text);

    UFUNCTION(BlueprintCallable)
    void SetProgress(float Progress);

    UFUNCTION(BlueprintCallable)
    void HighlightKey(const FString& KeyName);

    UFUNCTION(BlueprintCallable)
    void PlayShowAnimation();

    UFUNCTION(BlueprintCallable)
    void PlayHideAnimation();

    // —— 跳过回调 ——
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSkipTutorial);
    UPROPERTY(BlueprintAssignable)
    FOnSkipTutorial OnSkipTutorial;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    void OnSkipClicked();

    float DisplayTimer = 0.f;
    bool bIsShowing = false;
};
