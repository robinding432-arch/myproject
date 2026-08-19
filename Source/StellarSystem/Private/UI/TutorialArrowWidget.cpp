// TutorialArrowWidget.cpp

#include "UI/TutorialArrowWidget.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

void UTutorialArrowWidget::NativeConstruct()
{
    Super::NativeConstruct();

    bPulsing = true;
    PulseTimer = 0.f;

    // 默认颜色：亮青色
    SetArrowColor(FLinearColor(0.f, 0.8f, 1.f, 1.f));
}

void UTutorialArrowWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    PulseTimer += InDeltaTime;

    if (bPulsing && ArrowImage)
    {
        // 脉冲：大小 + 透明度
        float Scale = 1.f + 0.15f * FMath::Sin(PulseTimer * PulseSpeed);
        ArrowImage->SetRenderTransform(FWidgetTransform(
            FVector2D::ZeroVector,
            FVector2D(Scale, Scale),
            FVector2D::ZeroVector,
            0.f
        ));

        float Alpha = 0.7f + 0.3f * FMath::Sin(PulseTimer * PulseSpeed * 1.3f);
        FLinearColor CurrentColor = ArrowImage->GetColorAndOpacity().GetSpecifiedColor();
        CurrentColor.A = Alpha;
        ArrowImage->SetColorAndOpacity(CurrentColor);
    }

    // 距离越远越亮
    if (CurrentDistance > 0.f && ArrowImage)
    {
        float Intensity = FMath::Clamp(CurrentDistance / 10000.f, 0.3f, 1.f);
        FLinearColor C = ArrowImage->GetColorAndOpacity().GetSpecifiedColor();
        C.R = Intensity;
        C.G = Intensity * 0.9f;
        C.B = 1.f;
        ArrowImage->SetColorAndOpacity(C);
    }
}

void UTutorialArrowWidget::SetArrowPosition(float ScreenX, float ScreenY)
{
    if (ArrowCanvas)
    {
        FVector2D Pos(ScreenX, ScreenY);
        ArrowCanvas->SetPosition(pos);
    }
}

void UTutorialArrowWidget::SetArrowRotation(float AngleDegrees)
{
    if (ArrowImage)
    {
        ArrowImage->SetRenderTransformAngle(AngleDegrees);
    }
}

void UTutorialArrowWidget::SetArrowColor(FLinearColor Color)
{
    if (ArrowImage)
    {
        ArrowImage->SetColorAndOpacity(Color);
    }
}

void UTutorialArrowWidget::SetDistance(float Distance)
{
    CurrentDistance = Distance;
}

void UTutorialArrowWidget::PlayPulseAnimation(float InPulseSpeed)
{
    bPulsing = true;
    PulseSpeed = InPulseSpeed;
    PulseTimer = 0.f;
}

void UTutorialArrowWidget::StopPulseAnimation()
{
    bPulsing = false;
}
