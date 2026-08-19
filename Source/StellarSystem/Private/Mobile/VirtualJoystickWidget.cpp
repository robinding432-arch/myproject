// VirtualJoystickWidget.cpp
// v7.2 — Virtual joystick implementation

#include "Mobile/VirtualJoystickWidget.h"
#include "Engine/Engine.h"
#include "Rendering/DrawElements.h"
#include "Input/HittestGrid.h"

UVirtualJoystickWidget::UVirtualJoystickWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bIsLeftJoystick = true;
    Style.BaseRadius = 80.f;
    Style.ThumbRadius = 35.f;
    Style.IdleOpacity = 0.3f;
    Style.ActiveOpacity = 0.8f;
    Style.FadeSpeed = 3.f;
    DeadZone = 0.15f;
    bEnabled = true;
    CurrentOpacity = 0.3f;
}

void UVirtualJoystickWidget::SetCenter(const FVector2D& ScreenPos)
{
    Center = ScreenPos;
}

void UVirtualJoystickWidget::ResetJoystick()
{
    Output = FVector2D::ZeroVector;
    bIsActive = false;
    ThumbOffset = FVector2D::ZeroVector;
    CurrentOpacity = Style.IdleOpacity;
    OnOutputChanged.Broadcast(Output);
}

void UVirtualJoystickWidget::SetDeadZone(float InDeadZone)
{
    DeadZone = FMath::Clamp(InDeadZone, 0.f, 0.5f);
}

void UVirtualJoystickWidget::SetEnabled(bool bInEnabled)
{
    bEnabled = bInEnabled;
    if (!bEnabled) ResetJoystick();
}

void UVirtualJoystickWidget::NativePaint(FPaintContext& InContext) const
{
    const FPaintGeometry& Geo = InContext.MyClippingRect;
    FVector2D LocalCenter = Center; // In widget local space

    // Draw base circle
    FLinearColor BaseCol = Style.BaseColor;
    BaseCol.A *= CurrentOpacity;
    // Use slate draw
    // (Simplified: actual circle drawing done in BP or Slate)
}

FReply UVirtualJoystickWidget::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
    if (!bEnabled) return FReply::Unhandled();

    FVector2D LocalPos = InGeometry.AbsoluteToLocal(InGestureEvent.GetScreenSpacePosition());

    // Check if touch is within activation radius
    float ActivateRadius = Style.BaseRadius * 1.5f;
    if (FVector2D::Distance(LocalPos, Center) <= ActivateRadius)
    {
        bIsActive = true;
        ThumbOffset = LocalPos - Center;
        // Clamp
        float MaxLen = Style.BaseRadius;
        if (ThumbOffset.Size() > MaxLen) ThumbOffset = ThumbOffset.GetSafeNormal() * MaxLen;

        // Compute output
        FVector2D Normalized = ThumbOffset / Style.BaseRadius;
        float Mag = Normalized.Size();
        if (Mag < DeadZone) Output = FVector2D::ZeroVector;
        else
        {
            float Scaled = (Mag - DeadZone) / (1.f - DeadZone);
            Output = Normalized.GetSafeNormal() * FMath::Min(Scaled, 1.f);
        }

        OnOutputChanged.Broadcast(Output);
        return FReply::Handled().SetUserFocusRecursively(this->TakeWidget());
    }
    return FReply::Unhandled();
}

FReply UVirtualJoystickWidget::NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
    if (!bIsActive) return FReply::Unhandled();

    FVector2D LocalPos = InGeometry.AbsoluteToLocal(InGestureEvent.GetScreenSpacePosition());
    ThumbOffset = LocalPos - Center;
    float MaxLen = Style.BaseRadius;
    if (ThumbOffset.Size() > MaxLen) ThumbOffset = ThumbOffset.GetSafeNormal() * MaxLen;

    FVector2D Normalized = ThumbOffset / Style.BaseRadius;
    float Mag = Normalized.Size();
    if (Mag < DeadZone) Output = FVector2D::ZeroVector;
    else
    {
        float Scaled = (Mag - DeadZone) / (1.f - DeadZone);
        Output = Normalized.GetSafeNormal() * FMath::Min(Scaled, 1.f);
    }

    OnOutputChanged.Broadcast(Output);
    return FReply::Handled();
}

FReply UVirtualJoystickWidget::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
    if (!bIsActive) return FReply::Unhandled();
    ResetJoystick();
    return FReply::Handled();
}
