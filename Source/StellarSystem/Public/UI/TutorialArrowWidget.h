// TutorialArrowWidget.h
// 屏幕边缘箭头：指向目标方向

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TutorialArrowWidget.generated.h"

class UImage;
class UCanvasPanel;

UCLASS()
class STELLARSYSTEM_API UTutorialArrowWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(meta = (BindWidget))
    UImage* ArrowImage;

    UPROPERTY(meta = (BindWidget))
    UCanvasPanel* ArrowCanvas;

    // —— 设置箭头指向（屏幕坐标）——
    UFUNCTION(BlueprintCallable)
    void SetArrowPosition(float ScreenX, float ScreenY);

    UFUNCTION(BlueprintCallable)
    void SetArrowRotation(float AngleDegrees);

    UFUNCTION(BlueprintCallable)
    void SetArrowColor(FLinearColor Color);

    UFUNCTION(BlueprintCallable)
    void SetDistance(float Distance);

    // —— 动画 ——
    UFUNCTION(BlueprintCallable)
    void PlayPulseAnimation(float PulseSpeed = 2.f);

    UFUNCTION(BlueprintCallable)
    void StopPulseAnimation();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    float PulseTimer = 0.f;
    float PulseSpeed = 2.f;
    bool bPulsing = false;
    float CurrentDistance = 0.f;
};
