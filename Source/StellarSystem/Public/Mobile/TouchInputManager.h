// TouchInputManager.h
// v7.2 — Mobile touch input system
// Supports multi-touch, virtual joysticks, gestures, and adaptive input mapping

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerInput.h"
#include "TouchInputManager.generated.h"

/** Touch gesture types recognized by the system */
UENUM(BlueprintType)
enum class ETouchGesture : uint8
{
    None        UMETA(DisplayName = "None"),
    Tap         UMETA(DisplayName = "Tap"),
    DoubleTap   UMETA(DisplayName = "Double Tap"),
    LongPress   UMETA(DisplayName = "Long Press"),
    SwipeLeft   UMETA(DisplayName = "Swipe Left"),
    SwipeRight  UMETA(DisplayName = "Swipe Right"),
    SwipeUp     UMETA(DisplayName = "Swipe Up"),
    SwipeDown   UMETA(DisplayName = "Swipe Down"),
    PinchIn     UMETA(DisplayName = "Pinch In"),
    PinchOut    UMETA(DisplayName = "Pinch Out"),
    TwoFingerTap UMETA(DisplayName = "Two Finger Tap"),
};

/** Virtual joystick axis state */
USTRUCT(BlueprintType)
struct FJoystickState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FVector2D Direction = FVector2D::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    float Magnitude = 0.f;

    UPROPERTY(BlueprintReadOnly)
    bool bIsActive = false;

    UPROPERTY(BlueprintReadOnly)
    FVector2D ScreenPosition = FVector2D::ZeroVector;
};

/** Configuration for touch input behavior */
USTRUCT(BlueprintType)
struct FTouchInputConfig
{
    GENERATED_BODY()

    /** Left joystick dead zone (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LeftDeadZone = 0.15f;

    /** Right joystick dead zone (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RightDeadZone = 0.1f;

    /** Double-tap time window (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DoubleTapTime = 0.3f;

    /** Long press duration (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LongPressTime = 0.6f;

    /** Swipe minimum distance (pixels) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SwipeMinDistance = 50.f;

    /** Pinch minimum distance change (pixels) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PinchMinDelta = 20.f;

    /** Joystick adaptive position (recenter on touch) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bAdaptiveJoystick = true;

    /** Haptic feedback on joystick engage */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bHapticFeedback = true;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGestureDetected, ETouchGesture, Gesture);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnJoystickChanged, const FJoystickState&, State);

/**
 * UTStaticFunctionLib — static helpers for touch math
 */
UCLASS()
class UTouchMath : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure, Category = "Touch|Math")
    static FVector2D ClampToCircle(const FVector2D& Vector, float MaxRadius)
    {
        const float Len = Vector.Size();
        return Len > MaxRadius ? Vector * (MaxRadius / Len) : Vector;
    }

    UFUNCTION(BlueprintPure, Category = "Touch|Math")
    static float VectorAngleDegrees(const FVector2D& V)
    {
        return FMath::RadiansToDegrees(FMath::Atan2(V.Y, V.X));
    }
};

/**
 * UTouchInputManager — singleton component attached to PlayerController
 * Handles all touch input, virtual joysticks, gestures, and maps them
 * to existing Input Actions so no game logic changes are needed.
 */
UCLASS(BlueprintType, Blueprintable)
class STELLARSYSTEM_API UTouchInputManager : public UActorComponent
{
    GENERATED_BODY()

public:
    UTouchInputManager();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /** Configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Touch Config")
    FTouchInputConfig Config;

    /** Current left joystick state (movement) */
    UPROPERTY(BlueprintReadOnly, Category = "Touch State")
    FJoystickState LeftJoystick;

    /** Current right joystick state (look/aim) */
    UPROPERTY(BlueprintReadOnly, Category = "Touch State")
    FJoystickState RightJoystick;

    /** Pinch delta (positive = zoom in / pinch out) */
    UPROPERTY(BlueprintReadOnly, Category = "Touch State")
    float PinchDelta = 0.f;

    /** Event: gesture detected */
    UPROPERTY(BlueprintAssignable, Category = "Touch Events")
    FOnGestureDetected OnGestureDetected;

    /** Event: left joystick changed */
    UPROPERTY(BlueprintAssignable, Category = "Touch Events")
    FOnJoystickChanged OnLeftJoystickChanged;

    /** Event: right joystick changed */
    UPROPERTY(BlueprintAssignable, Category = "Touch Events")
    FOnJoystickChanged OnRightJoystickChanged;

    /** Enable/disable touch input (e.g. disable when in menu) */
    UFUNCTION(BlueprintCallable, Category = "Touch")
    void SetTouchEnabled(bool bEnabled);

    /** Check if device is a mobile platform */
    UFUNCTION(BlueprintPure, Category = "Touch")
    bool IsMobilePlatform() const;

    /** Get screen DPI scale for UI sizing */
    UFUNCTION(BlueprintPure, Category = "Touch")
    float GetDPIScale() const;

    /** Set joystick center programmatically (for respawn UI) */
    UFUNCTION(BlueprintCallable, Category = "Touch")
    void SetLeftJoystickCenter(const FVector2D& ScreenPos);

    UFUNCTION(BlueprintCallable, Category = "Touch")
    void SetRightJoystickCenter(const FVector2D& ScreenPos);

    /** Reset all touch state (call on game pause/resume) */
    UFUNCTION(BlueprintCallable, Category = "Touch")
    void ResetTouchState();

    /** Map a gesture to an input action name */
    UFUNCTION(BlueprintCallable, Category = "Touch")
    void BindGestureToAction(ETouchGesture Gesture, const FName& ActionName);

    /** Get gesture binding */
    UFUNCTION(BlueprintPure, Category = "Touch")
    FName GetGestureBinding(ETouchGesture Gesture) const;

private:
    /** Internal touch tracking */
    struct FTouchTrack
    {
        FVector2D StartPos;
        FVector2D CurrentPos;
        float StartTime = 0.f;
        bool bIsActive = false;
        int32 FingerIndex = -1;
    };

    TArray<FTouchTrack> ActiveTouches;
    FTouchTrack* FindTouch(int32 FingerIndex);
    FTouchTrack* AddTouch(int32 FingerIndex, const FVector2D& Pos);
    void RemoveTouch(int32 FingerIndex);

    /** Joystick center positions (adaptive) */
    FVector2D LeftJoystickCenter;
    FVector2D RightJoystickCenter;
    bool bLeftJoystickActive = false;
    bool bRightJoystickActive = false;

    /** Gesture detection */
    void DetectGestures(float DeltaTime);
    void CheckTapGestures();
    void CheckPinchGesture();
    void FireGesture(ETouchGesture Gesture);

    /** Joystick update */
    void UpdateJoystick(FJoystickState& OutState, const FVector2D& Center, const FVector2D& Current, float DeadZone);

    /** Map touch to input actions */
    void InjectInputAction(const FName& ActionName, bool bPressed);

    /** Gesture → Action bindings */
    TMap<ETouchGesture, FName> GestureBindings;

    /** Double-tap tracking */
    float LastTapTime = -1.f;
    FVector2D LastTapPos = FVector2D::ZeroVector;

    /** Pinch tracking */
    float LastPinchDistance = 0.f;

    /** Enabled flag */
    bool bTouchEnabled = true;

    /** Cached DPI scale */
    mutable float CachedDPIScale = 1.f;
    mutable float LastDPICheck = 0.f;
};
