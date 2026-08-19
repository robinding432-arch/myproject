// MobileMainMenuWidget.cpp
// v7.2 — Mobile main menu implementation

#include "Mobile/MobileMainMenuWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/Engine.h"
#include "TimerManager.h"

void UMobileMainMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Bind button events
    if (BtnNewGame)   BtnNewGame->OnClicked.AddDynamic(this, &UMobileMainMenuWidget::HandleNewGamePressed);
    if (BtnContinue)   BtnContinue->OnClicked.AddDynamic(this, &UMobileMainMenuWidget::HandleContinuePressed);
    if (BtnSettings)   BtnSettings->OnClicked.AddDynamic(this, &UMobileMainMenuWidget::HandleSettingsPressed);
    if (BtnMultiplayer) BtnMultiplayer->OnClicked.AddDynamic(this, &UMobileMainMenuWidget::HandleMultiplayerPressed);
    if (BtnQuit)       BtnQuit->OnClicked.AddDynamic(this, &UMobileMainMenuWidget::HandleQuitPressed);

    // Set version subtitle
    SetSubtitle(FString::Printf(TEXT("v7.2 Mobile Edition")));

    // Start background animation
    StartBackgroundAnimation();
}

void UMobileMainMenuWidget::SetSubtitle(const FString& Text)
{
    if (TxtSubtitle) TxtSubtitle->SetText(FText::FromString(Text));
}

void UMobileMainMenuWidget::SetLoadingVisible(bool bVisible)
{
    if (ImgLoading) ImgLoading->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UMobileMainMenuWidget::ShowToast(const FString& Message, float DurationSeconds)
{
    if (!TxtToast) return;
    TxtToast->SetText(FText::FromString(Message));
    TxtToast->SetVisibility(ESlateVisibility::Visible);

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ToastTimer);
        GetWorld()->GetTimerManager().SetTimer(ToastTimer, [this]()
        {
            if (TxtToast) TxtToast->SetVisibility(ESlateVisibility::Collapsed);
        }, DurationSeconds, false);
    }
}

void UMobileMainMenuWidget::StartBackgroundAnimation()
{
    bBgAnimActive = true;
    BgAnimTime = 0.f;

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(BgAnimTimer);
        GetWorld()->GetTimerManager().SetTimer(BgAnimTimer, this, &UMobileMainMenuWidget::UpdateBackgroundAnimation, 0.033f, true);
    }
}

void UMobileMainMenuWidget::StopBackgroundAnimation()
{
    bBgAnimActive = false;
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(BgAnimTimer);
}

void UMobileMainMenuWidget::UpdateBackgroundAnimation()
{
    if (!bBgAnimActive || !ImgBackground) return;
    BgAnimTime += 0.033f;

    // Subtle parallax: shift background UVs
    float OffsetX = FMath::Sin(BgAnimTime * 0.1f) * 0.02f;
    float OffsetY = FMath::Cos(BgAnimTime * 0.07f) * 0.015f;

    // Apply to image brush (simplified — full impl would use material)
    // ImgBackground->SetBrushUV(FVector2D(OffsetX, OffsetY));
}

void UMobileMainMenuWidget::HandleNewGamePressed()    { OnNewGamePressed.Broadcast(); }
void UMobileMainMenuWidget::HandleContinuePressed()   { OnContinuePressed.Broadcast(); }
void UMobileMainMenuWidget::HandleSettingsPressed()   { OnSettingsPressed.Broadcast(); }
void UMobileMainMenuWidget::HandleMultiplayerPressed(){ OnMultiplayerPressed.Broadcast(); }
void UMobileMainMenuWidget::HandleQuitPressed()      { OnQuitPressed.Broadcast(); }
