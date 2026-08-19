// MobileHUDWidget.cpp
// v7.2 — Mobile HUD implementation

#include "Mobile/MobileHUDWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "TimerManager.h"
#include "Engine/Engine.h"

void UMobileHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetHUDMode(EHUDMode::Navigation);
}

void UMobileHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    TickNotifications(InDeltaTime);
    TickDamageIndicator(InDeltaTime);
}

void UMobileHUDWidget::SetHUDMode(EHUDMode Mode)
{
    CurrentMode = Mode;
    // Switch visible panels via WidgetSwitcher
    if (PanelModeSwitcher)
    {
        int32 Index = (int32)Mode;
        PanelModeSwitcher->SetActiveWidgetIndex(FMath::Min(Index, PanelModeSwitcher->GetNumWidgets() - 1));
    }
}

void UMobileHUDWidget::ToggleMinimalMode()
{
    if (CurrentMode == EHUDMode::Minimal)
    {
        SetHUDMode(EHUDMode::Navigation);
    }
    else
    {
        SetHUDMode(EHUDMode::Minimal);
    }
}

void UMobileHUDWidget::SetHealth(float Value)
{
    if (ProgressHealth) ProgressHealth->SetPercent(FMath::Clamp(Value, 0.f, 1.f));
}

void UMobileHUDWidget::SetShield(float Value)
{
    if (ProgressShield) ProgressShield->SetPercent(FMath::Clamp(Value, 0.f, 1.f));
}

void UMobileHUDWidget::SetOxygen(float Value)
{
    if (ProgressOxygen) ProgressOxygen->SetPercent(FMath::Clamp(Value, 0.f, 1.f));
}

void UMobileHUDWidget::SetEnergy(float Value)
{
    if (ProgressEnergy) ProgressEnergy->SetPercent(FMath::Clamp(Value, 0.f, 1.f));
}

void UMobileHUDWidget::SetSpeed(float SpeedKph)
{
    if (TxtSpeed) TxtSpeed->SetText(FText::FromString(FString::Printf(TEXT("%.0f km/h"), SpeedKph)));
}

void UMobileHUDWidget::SetAltitude(float AltitudeMeters)
{
    if (TxtAltitude) TxtAltitude->SetText(FText::FromString(FString::Printf(TEXT("%.0f m"), AltitudeMeters)));
}

void UMobileHUDWidget::SetTargetInfo(const FString& TargetName, float DistanceKm, float HullPercent)
{
    if (TxtTarget)
    {
        TxtTarget->SetText(FText::FromString(
            FString::Printf(TEXT("%s\n%.1f km | Hull %.0f%%"), *TargetName, DistanceKm, HullPercent * 100.f)
        ));
    }
}

void UMobileHUDWidget::ClearTargetInfo()
{
    if (TxtTarget) TxtTarget->SetText(FText::GetEmpty());
}

void UMobileHUDWidget::ShowNotification(const FString& Text, float DurationSeconds, FLinearColor Color)
{
    if (!TxtNotification) return;
    TxtNotification->SetText(FText::FromString(Text));
    TxtNotification->SetColorAndOpacity(FSlateColor(Color));
    TxtNotification->SetVisibility(ESlateVisibility::Visible);

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(NotificationTimer);
        GetWorld()->GetTimerManager().SetTimer(NotificationTimer, this, &UMobileHUDWidget::FadeOutNotification, DurationSeconds, false);
    }
}

void UMobileHUDWidget::AddChatMessage(const FString& Sender, const FString& Message, FLinearColor SenderColor)
{
    if (!TxtChat) return;
    FString Line = FString::Printf(TEXT("[%s] %s"), *Sender, *Message);
    ChatHistory.Add(Line);
    while (ChatHistory.Num() > MaxChatLines) ChatHistory.RemoveAt(0);

    FString Combined;
    for (const FString& S : ChatHistory) Combined += S + TEXT("\n");

    TxtChat->SetText(FText::FromString(Combined));
    TxtChat->SetVisibility(ESlateVisibility::Visible);

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ChatTimer);
        GetWorld()->GetTimerManager().SetTimer(ChatTimer, this, &UMobileHUDWidget::FadeOutChat, 8.f, false);
    }
}

void UMobileHUDWidget::SetCompassHeading(float HeadingDegrees)
{
    if (ImgCompass)
    {
        // Rotate compass image
        // ImgCompass->SetRenderTransformAngle(-HeadingDegrees);
    }
}

void UMobileHUDWidget::SetMinimapRotation(float RotationDegrees)
{
    // Set minimap widget rotation
}

void UMobileHUDWidget::PulseDamageDirection(const FVector& DamageDirection)
{
    DamageIndicatorAlpha = 1.f;
    if (ImgDamageIndicator)
    {
        // Position arrow based on direction
        // ImgDamageIndicator->SetVisibility(ESlateVisibility::Visible);
    }
}

// ─── Tick functions ───

void UMobileHUDWidget::TickNotifications(float DeltaTime)
{
    // Notifications auto-fade handled by timer
}

void UMobileHUDWidget::TickDamageIndicator(float DeltaTime)
{
    if (DamageIndicatorAlpha > 0.f)
    {
        DamageIndicatorAlpha = FMath::Max(0.f, DamageIndicatorAlpha - DeltaTime * 0.5f);
        if (ImgDamageIndicator)
        {
            // FLinearColor C = FLinearColor(1, 0, 0, DamageIndicatorAlpha);
            // ImgDamageIndicator->SetColorAndOpacity(C);
        }
        if (DamageIndicatorAlpha <= 0.f && ImgDamageIndicator)
        {
            // ImgDamageIndicator->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
}

void UMobileHUDWidget::FadeOutNotification()
{
    if (TxtNotification) TxtNotification->SetVisibility(ESlateVisibility::Collapsed);
}

void UMobileHUDWidget::FadeOutChat()
{
    if (TxtChat) TxtChat->SetVisibility(ESlateVisibility::Collapsed);
}

// ─── Button handlers ───

void UMobileHUDWidget::HandleFirePressed()    { OnFirePressed.Broadcast(); }
void UMobileHUDWidget::HandleFireReleased()   { OnFireReleased.Broadcast(); }
void UMobileHUDWidget::HandleBoostPressed()   { OnBoostPressed.Broadcast(); }
void UMobileHUDWidget::HandleInteractPressed(){ OnInteractPressed.Broadcast(); }
void UMobileHUDWidget::HandleMenuPressed()    { OnMenuPressed.Broadcast(); }
void UMobileHUDWidget::HandleMapPressed()    { OnMapToggled.Broadcast(); }
