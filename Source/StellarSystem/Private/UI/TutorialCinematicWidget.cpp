// TutorialCinematicWidget.cpp

#include "UI/TutorialCinematicWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

void UTutorialCinematicWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (ContinueButton)
    {
        ContinueButton->OnClicked.AddDynamic(this, &UTutorialCinematicWidget::OnContinueButtonClicked);
    }

    CinematicTimer = 0.f;
    FadeAlpha = 0.f;
}

void UTutorialCinematicWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    CinematicTimer += InDeltaTime;

    // 淡入
    if (bFadingIn)
    {
        FadeAlpha += InDeltaTime / FadeDuration;
        FadeAlpha = FMath::Clamp(FadeAlpha, 0.f, 1.f);

        if (RootCanvas)
        {
            RootCanvas->SetRenderOpacity(FadeAlpha);
        }

        if (FadeAlpha >= 1.f)
        {
            bFadingIn = false;
        }
    }

    // 淡出
    if (bFadingOut)
    {
        FadeAlpha -= InDeltaTime / FadeDuration;
        FadeAlpha = FMath::Clamp(FadeAlpha, 0.f, 1.f);

        if (RootCanvas)
        {
            RootCanvas->SetRenderOpacity(FadeAlpha);
        }

        if (FadeAlpha <= 0.f)
        {
            bFadingOut = false;
            OnCinematicComplete.Broadcast();
        }
    }

    // 星空动画
    if (bStarfieldActive)
    {
        AnimateStarfield(InDeltaTime);
    }

    // 扫描线动画
    AnimateScanlines(InDeltaTime);
}

void UTutorialCinematicWidget::SetupCinematic(ETutorialCinematicType Type)
{
    switch (Type)
    {
    case ETutorialCinematicType::Intro_Starfield:
        SetTitleText(FText::FromString(TEXT("STELLARSYSTEM")));
        SetSubtitleText(FText::FromString(TEXT("程序化宇宙引擎")));
        SetBodyText(FText::FromString(TEXT("在程序生成的无限宇宙中\n探索 · 贸易 · 战斗 · 生存")));
        PlayStarfieldAnimation(8.f);
        break;

    case ETutorialCinematicType::Outro_Welcome:
        SetTitleText(FText::FromString(TEXT("教程完成")));
        SetSubtitleText(FText::FromString(TEXT("欢迎来到宇宙")));
        SetBodyText(FText::FromString(TEXT("你已掌握生存的基本技能。\n这个无限宇宙，现在任你探索。\n\n+5000 Credits\n+1000 XP\n+ 新手激光步枪")));
        break;

    case ETutorialCinematicType::Death_Screen:
        SetTitleText(FText::FromString(TEXT("你已阵亡")));
        SetSubtitleText(FText::FromString(TEXT("正在重生...")));
        SetBodyText(FText::FromString(TEXT("死亡不是终点。\n你将在最近的复活点重生。")));
        break;

    case ETutorialCinematicType::Respawn_Cinematic:
        SetTitleText(FText::FromString(TEXT("正在重生")));
        SetSubtitleText(FText::FromString(TEXT("...")));
        SetBodyText(FText::FromString(TEXT("物质重组中...\n神经元重新连接...\n意识恢复...")));
        break;
    }
}

void UTutorialCinematicWidget::SetTitleText(const FText& Text)
{
    if (TitleText) TitleText->SetText(Text);
}

void UTutorialCinematicWidget::SetSubtitleText(const FText& Text)
{
    if (SubtitleText) SubtitleText->SetText(Text);
}

void UTutorialCinematicWidget::SetBodyText(const FText& Text)
{
    if (BodyText) BodyText->SetText(Text);
}

void UTutorialCinematicWidget::PlayFadeIn(float Duration)
{
    bFadingIn = true;
    bFadingOut = false;
    FadeDuration = Duration;
    FadeAlpha = 0.f;

    if (RootCanvas)
    {
        RootCanvas->SetRenderOpacity(0.f);
    }
}

void UTutorialCinematicWidget::PlayFadeOut(float Duration)
{
    bFadingOut = true;
    bFadingIn = false;
    FadeDuration = Duration;
    FadeAlpha = 1.f;
}

void UTutorialCinematicWidget::PlayStarfieldAnimation(float Duration)
{
    bStarfieldActive = true;
    StarfieldTime = 0.f;
}

void UTutorialCinematicWidget::AnimateStarfield(float DeltaTime)
{
    StarfieldTime += DeltaTime;

    // 让星空背景缓慢移动/闪烁
    if (BackgroundImage)
    {
        // 旋转星空
        float Rotation = FMath::Fmod(StarfieldTime * 2.f, 360.f);
        BackgroundImage->SetRenderTransformAngle(Rotation * 0.1f);

        // 闪烁
        float Alpha = 0.7f + 0.3f * FMath::Sin(StarfieldTime * 0.5f);
        FLinearColor Color = FLinearColor(1.f, 1.f, 1.f, Alpha);
        BackgroundImage->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, Alpha));
    }

    // Logo 淡入
    if (LogoImage)
    {
        float LogoAlpha = FMath::Clamp(StarfieldTime / 3.f, 0.f, 1.f);
        LogoImage->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, LogoAlpha));
    }
}

void UTutorialCinematicWidget::AnimateScanlines(float DeltaTime)
{
    if (ScanlineOverlay)
    {
        // 扫描线缓慢下移
        float Offset = FMath::Fmod(CinematicTimer * 50.f, 100.f);
        // 通过渲染变换实现
        ScanlineOverlay->SetRenderTransformAngle(0.f);
    }
}

void UTutorialCinematicWidget::OnContinueButtonClicked()
{
    UE_LOG(LogTemp, Log, TEXT("[Cinematic] Continue clicked"));
    OnContinueClicked.Broadcast();
    PlayFadeOut(1.f);
}
