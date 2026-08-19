// VirtualJoystickWidget.h
// v7.2 — On-screen virtual joystick for touch devices

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Mobile/TouchInputManager.h"
#include "VirtualJoystickWidget.generated.h"

/** Visual style for the joystick */
USTRUCT(BlueprintType)
struct FJoystickStyle
{
    GENERATED_BODY()

    /** Base radius in pixels (DPI-scaled) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BaseRadius = 80.f;

    /** Thumb radius in pixels */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ThumbRadius = 35.f;

    /** Base color (semi-transparent) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor BaseColor = FLinearColor(1.f, 1.f, 1.f, 0.15f);

    /** Thumb color */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor ThumbColor = FLinearColor(1.f, 1.f, 1.f, 0.5f);

    /** Fade out opacity when not in use */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float IdleOpacity = 0.3f;

    /** Full opacity when active */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ActiveOpacity = 0.8f;

    /** Fade speed */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FadeSpeed = 3.f;
};

/**
 * WVirtualJoystick — drawable virtual joystick widget
 * Can be Left (movement) or Right (look/aim)
 */
UCLASS(BlueprintType)
class STELLARSYSTEM_API UVirtualJoystickWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UVirtualJoystickWidget(const FObjectInitializer& ObjectInitializer);

    /** Which joystick this represents */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Joystick")
    bool bIsLeftJoystick = true;

    /** Visual style */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Joystick")
    FJoystickStyle Style;

    /** Current output (-1..1, -1..1) */
    UPROPERTY(BlueprintReadOnly, Category = "Joystick")
    FVector2D Output = FVector2D::ZeroVector;

    /** Is finger currently on this joystick */
    UPROPERTY(BlueprintReadOnly, Category = "Joystick")
    bool bIsActive = false;

    /** Set center position (for adaptive mode) */
    UFUNCTION(BlueprintCallable, Category = "Joystick")
    void SetCenter(const FVector2D& ScreenPos);

    /** Get current center */
    UFUNCTION(BlueprintPure, Category = "Joystick")
    FVector2D GetCenter() const { return Center; }

    /** Reset to idle state */
    UFUNCTION(BlueprintCallable, Category = "Joystick")
    void ResetJoystick();

    /** Set dead zone (0-1) */
    UFUNCTION(BlueprintCallable, Category = "Joystick")
    void SetDeadZone(float DeadZone);

    /** Enable/disable this joystick */
    UFUNCTION(BlueprintCallable, Category = "Joystick")
    void SetEnabled(bool bEnabled);

    /** Override paint to draw joystick graphics */
    virtual void NativePaint(FPaintContext& InContext) const override;

    /** Handle touch */
    virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
    virtual FReply NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
    virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;

    /** Called when output changes (for BP binding) */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnJoystickOutput, const FVector2D&, Output);
    UPROPERTY(BlueprintAssignable, Category = "Joystick")
    FOnJoystickOutput OnOutputChanged;

private:
    FVector2D Center = FVector2D(150.f, 400.f);  // Default bottom-left for left stick
    float DeadZone = 0.15f;
    bool bEnabled = true;
    float CurrentOpacity = 0.3f;
    mutable FVector2D ThumbOffset = FVector2D::ZeroVector;
};
