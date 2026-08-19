// TutorialPromptWidget.cpp

#include "UI/TutorialPromptWidget.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

void UTutorialPromptWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 绑定跳过按钮
    if (SkipButton)
    {
        SkipButton->OnClicked.AddDynamic(this, &UTutorialPromptWidget::OnSkipClicked);
    }

    bIsShowing = true;
    DisplayTimer = 0.f;

    // 播放显示动画
    PlayShowAnimation();
}

void UTutorialPromptWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    DisplayTimer += InDeltaTime;

    // 脉冲效果：让提示文本微微闪烁以吸引注意
    if (PromptText)
    {
        float Alpha = 0.85f + 0.15f * FMath::Sin(DisplayTimer * 3.f);
        FLinearColor Color = PromptText->GetColorAndOpacity().GetSpecifiedColor();
        Color.A = Alpha;
        PromptText->SetColorAndOpacity(FSlateColor(Color));
    }
}

void UTutorialPromptWidget::SetPromptText(const FText& Text)
{
    if (PromptText)
    {
        PromptText->SetText(Text);
    }
}

void UTutorialPromptWidget::SetDetailText(const FText& Text)
{
    if (DetailText)
    {
        DetailText->SetText(Text);
    }
}

void UTutorialPromptWidget::SetRewardText(const FText& Text)
{
    if (RewardText)
    {
        RewardText->SetText(Text);
    }
}

void UTutorialPromptWidget::SetProgress(float Progress)
{
    if (ProgressBar)
    {
        ProgressBar->SetPercent(FMath::Clamp(Progress, 0.f, 1.f));
    }
}

void UTutorialPromptWidget::HighlightKey(const FString& KeyName)
{
    if (KeyHighlightIcon)
    {
        // 根据键名切换不同的图标
        // 实际项目中这里会加载对应的按键图标
        KeyHighlightIcon->SetVisibility(ESlateVisibility::Visible);
    }
}

void UTutorialPromptWidget::PlayShowAnimation()
{
    // 如果有 UMG 动画就播放
    if (UWidgetAnimation* ShowAnim = GetAnimationByName(FName("Show")))
    {
        PlayAnimation(ShowAnim);
    }
}

void UTutorialPromptWidget::PlayHideAnimation()
{
    if (UWidgetAnimation* HideAnim = GetAnimationByName(FName("Hide")))
    {
        PlayAnimation(HideAnim);
    }
}

void UTutorialPromptWidget::OnSkipClicked()
{
    UE_LOG(LogTemp, Log, TEXT("[TutorialPrompt] Skip button clicked"));
    OnSkipTutorial.Broadcast();
}
