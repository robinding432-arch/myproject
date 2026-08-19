#include "UI/SplashScreen.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"

void USplashScreen::NativeConstruct()
{
    Super::NativeConstruct();

    CurrentPhase = ESplashPhase::CompanyLogo;
    PhaseTimer = 0.f;
    bSkipped = false;

    // 随机选一条 Loading Tip
    if (LoadingTips.Num() > 0)
    {
        CurrentTipIndex = FMath::RandRange(0, LoadingTips.Num() - 1);
    }

    // 确保鼠标可见
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
    {
        PC->bShowMouseCursor = true;
        PC->SetInputMode(FInputModeUIOnly());
    }

    UE_LOG(LogTemp, Log, TEXT("[Splash] Starting splash sequence"));
}

void USplashScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (bSkipped) return;

    PhaseTimer += InDeltaTime;

    float CurrentDuration = 0.f;
    switch (CurrentPhase)
    {
    case ESplashPhase::CompanyLogo:
        CurrentDuration = CompanyLogoDuration;
        break;
    case ESplashPhase::LegalDisclaimer:
        CurrentDuration = LegalDisclaimerDuration;
        break;
    case ESplashPhase::LoadingTip:
        CurrentDuration = LoadingTipDuration;
        break;
    case ESplashPhase::FadingOut:
        CurrentDuration = FadeOutDuration;
        break;
    }

    if (PhaseTimer >= CurrentDuration)
    {
        AdvancePhase();
    }
}

void USplashScreen::AdvancePhase()
{
    switch (CurrentPhase)
    {
    case ESplashPhase::CompanyLogo:
        CurrentPhase = ESplashPhase::LegalDisclaimer;
        UE_LOG(LogTemp, Log, TEXT("[Splash] Phase: Legal Disclaimer"));
        break;
    case ESplashPhase::LegalDisclaimer:
        CurrentPhase = ESplashPhase::LoadingTip;
        UE_LOG(LogTemp, Log, TEXT("[Splash] Phase: Loading Tip"));
        break;
    case ESplashPhase::LoadingTip:
        CurrentPhase = ESplashPhase::FadingOut;
        UE_LOG(LogTemp, Log, TEXT("[Splash] Phase: Fading Out"));
        break;
    case ESplashPhase::FadingOut:
        OnAllPhasesComplete();
        break;
    }

    PhaseTimer = 0.f;
}

void USplashScreen::OnAllPhasesComplete()
{
    bSkipped = true;
    UE_LOG(LogTemp, Log, TEXT("[Splash] Sequence complete → Main Menu"));

    OnSplashComplete.Broadcast();

    // 淡出后移除自身
    RemoveFromParent();
}

void USplashScreen::SkipSplash()
{
    if (bSkipped) return;

    UE_LOG(LogTemp, Log, TEXT("[Splash] Skipped by user"));
    CurrentPhase = ESplashPhase::FadingOut;
    PhaseTimer = FadeOutDuration * 0.5f; // 快速淡出
}

float USplashScreen::GetPhaseProgress() const
{
    float Duration = 1.f;
    switch (CurrentPhase)
    {
    case ESplashPhase::CompanyLogo: Duration = CompanyLogoDuration; break;
    case ESplashPhase::LegalDisclaimer: Duration = LegalDisclaimerDuration; break;
    case ESplashPhase::LoadingTip: Duration = LoadingTipDuration; break;
    case ESplashPhase::FadingOut: Duration = FadeOutDuration; break;
    }
    return FMath::Clamp(PhaseTimer / FMath::Max(Duration, 0.001f), 0.f, 1.f);
}

FString USplashScreen::GetCurrentTip() const
{
    if (LoadingTips.IsValidIndex(CurrentTipIndex))
        return LoadingTips[CurrentTipIndex];
    return TEXT("");
}

float USplashScreen::GetGlobalAlpha() const
{
    // 淡出阶段透明度递减
    if (CurrentPhase == ESplashPhase::FadingOut)
    {
        return FMath::Clamp(1.f - (PhaseTimer / FMath::Max(FadeOutDuration, 0.001f)), 0.f, 1.f);
    }
    return 1.f;
}
