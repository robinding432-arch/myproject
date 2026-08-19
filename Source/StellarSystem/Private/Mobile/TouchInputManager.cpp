// TouchInputManager.cpp
// v7.2 — Touch input implementation

#include "Mobile/TouchInputManager.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/PlatformApplicationMisc.h"

UTouchInputManager::UTouchInputManager()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetComponentTickEnabled(true);
}

void UTouchInputManager::BeginPlay()
{
    Super::BeginPlay();

    // Default gesture bindings
    GestureBindings.Add(ETouchGesture::Tap, FName("Jump"));
    GestureBindings.Add(ETouchGesture::DoubleTap, FName("Interact"));
    GestureBindings.Add(ETouchGesture::LongPress, FName("Aim"));
    GestureBindings.Add(ETouchGesture::SwipeLeft, FName("PrevWeapon"));
    GestureBindings.Add(ETouchGesture::SwipeRight, FName("NextWeapon"));
    GestureBindings.Add(ETouchGesture::SwipeUp, FName("ThrustUp"));
    GestureBindings.Add(ETouchGesture::SwipeDown, FName("ThrustDown"));
    GestureBindings.Add(ETouchGesture::PinchIn, FName("ZoomIn"));
    GestureBindings.Add(ETouchGesture::PinchOut, FName("ZoomOut"));
    GestureBindings.Add(ETouchGesture::TwoFingerTap, FName("Map"));

    // Default joystick centers (will be overridden by adaptive)
    FVector2D Screen = GetOwner<APlayerController>() ? 
        FVector2D(GEngine->GameViewport->Viewport->GetSizeXY()) : FVector2D(1920, 1080);
    LeftJoystickCenter = FVector2D(Screen.X * 0.2f, Screen.Y * 0.7f);
    RightJoystickCenter = FVector2D(Screen.X * 0.8f, Screen.Y * 0.6f);
}

void UTouchInputManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bTouchEnabled) return;

    DetectGestures(DeltaTime);

    // Decay pinch delta
    PinchDelta = FMath::Lerp(PinchDelta, 0.f, FMath::Clamp(DeltaTime * 5.f, 0.f, 1.f));

    // Refresh DPI cache periodically
    if (GetWorld() && GetWorld()->GetTimeSeconds() - LastDPICheck > 2.f)
    {
        LastDPICheck = GetWorld()->GetTimeSeconds();
        CachedDPIScale = 1.f;
        if (GEngine && GEngine->GameViewport)
        {
            FVector2D ViewportSize = FVector2D(GEngine->GameViewport->Viewport->GetSizeXY());
            float Diagonal = FMath::Sqrt(ViewportSize.X * ViewportSize.X + ViewportSize.Y * ViewportSize.Y);
            CachedDPIScale = FMath::Clamp(Diagonal / 2200.f, 0.5f, 2.0f);
        }
    }
}

void UTouchInputManager::SetTouchEnabled(bool bEnabled)
{
    bTouchEnabled = bEnabled;
    if (!bEnabled) ResetTouchState();
}

bool UTouchInputManager::IsMobilePlatform() const
{
    return (PLATFORM_ANDROID || PLATFORM_IOS);
}

float UTouchInputManager::GetDPIScale() const
{
    return CachedDPIScale;
}

void UTouchInputManager::SetLeftJoystickCenter(const FVector2D& ScreenPos)
{
    LeftJoystickCenter = ScreenPos;
    LeftJoystick.ScreenPosition = ScreenPos;
}

void UTouchInputManager::SetRightJoystickCenter(const FVector2D& ScreenPos)
{
    RightJoystickCenter = ScreenPos;
    RightJoystick.ScreenPosition = ScreenPos;
}

void UTouchInputManager::ResetTouchState()
{
    ActiveTouches.Empty();
    LeftJoystick = FJoystickState();
    RightJoystick = FJoystickState();
    PinchDelta = 0.f;
    bLeftJoystickActive = false;
    bRightJoystickActive = false;
}

void UTouchInputManager::BindGestureToAction(ETouchGesture Gesture, const FName& ActionName)
{
    GestureBindings.Add(Gesture, ActionName);
}

FName UTouchInputManager::GetGestureBinding(ETouchGesture Gesture) const
{
    const FName* Found = GestureBindings.Find(Gesture);
    return Found ? *Found : FName();
}

// ─── Internal touch tracking ───

UTouchInputManager::FTouchTrack* UTouchInputManager::FindTouch(int32 FingerIndex)
{
    for (FTouchTrack& T : ActiveTouches) if (T.FingerIndex == FingerIndex && T.bIsActive) return &T;
    return nullptr;
}

UTouchInputManager::FTouchTrack* UTouchInputManager::AddTouch(int32 FingerIndex, const FVector2D& Pos)
{
    FTouchTrack& T = ActiveTouches.AddDefaulted_GetRef();
    T.FingerIndex = FingerIndex;
    T.StartPos = Pos;
    T.CurrentPos = Pos;
    T.StartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    T.bIsActive = true;
    return &T;
}

void UTouchInputManager::RemoveTouch(int32 FingerIndex)
{
    for (int32 i = 0; i < ActiveTouches.Num(); i++)
    {
        if (ActiveTouches[i].FingerIndex == FingerIndex)
        {
            ActiveTouches.RemoveAt(i);
            break;
        }
    }
}

// ─── Gesture detection ───

void UTouchInputManager::DetectGestures(float DeltaTime)
{
    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

    // Check pinch (2 touches)
    if (ActiveTouches.Num() == 2)
    {
        CheckPinchGesture();
    }

    // Check tap/long-press (1 touch)
    if (ActiveTouches.Num() == 1)
    {
        FTouchTrack& T = ActiveTouches[0];
        float HoldTime = Now - T.StartTime;
        FVector2D Delta = T.CurrentPos - T.StartPos;
        float Dist = Delta.Size();

        // Long press
        if (HoldTime > Config.LongPressTime && Dist < Config.SwipeMinDistance * 0.5f)
        {
            FireGesture(ETouchGesture::LongPress);
            T.StartTime = Now + 999.f; // Fire once
        }

        // Swipe detection on release
    }

    CheckTapGestures();
}

void UTouchInputManager::CheckTapGestures()
{
    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

    // Double-tap detection
    if (LastTapTime > 0 && (Now - LastTapTime) < Config.DoubleTapTime)
    {
        FireGesture(ETouchGesture::DoubleTap);
        LastTapTime = -1.f;
    }
}

void UTouchInputManager::CheckPinchGesture()
{
    if (ActiveTouches.Num() < 2) return;

    FVector2D Mid = (ActiveTouches[0].CurrentPos + ActiveTouches[1].CurrentPos) * 0.5f;
    float Dist = FVector2D::Distance(ActiveTouches[0].CurrentPos, ActiveTouches[1].CurrentPos);

    if (LastPinchDistance > 0)
    {
        float Delta = LastPinchDistance - Dist;
        if (FMath::Abs(Delta) > Config.PinchMinDelta)
        {
            PinchDelta = Delta;
            FireGesture(Delta > 0 ? ETouchGesture::PinchIn : ETouchGesture::PinchOut);
        }
    }
    LastPinchDistance = Dist;
}

void UTouchInputManager::FireGesture(ETouchGesture Gesture)
{
    OnGestureDetected.Broadcast(Gesture);

    // Inject as input action
    FName ActionName = GetGestureBinding(Gesture);
    if (!ActionName.IsNone())
    {
        InjectInputAction(ActionName, true);
        // Schedule release next tick
        GetWorld()->GetTimerManager().SetTimerForNextTick([this, ActionName]()
        {
            InjectInputAction(ActionName, false);
        });
    }

    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    if (Gesture == ETouchGesture::Tap || Gesture == ETouchGesture::DoubleTap)
    {
        LastTapTime = Now;
    }
}

// ─── Joystick update ───

void UTouchInputManager::UpdateJoystick(FJoystickState& OutState, const FVector2D& Center, const FVector2D& Current, float DeadZone)
{
    FVector2D Raw = Current - Center;
    float MaxRadius = 100.f * GetDPIScale();
    float Length = Raw.Size();

    if (Length < KINDA_SMALL_NUMBER)
    {
        OutState.Direction = FVector2D::ZeroVector;
        OutState.Magnitude = 0.f;
        OutState.bIsActive = false;
        return;
    }

    FVector2D Normalized = Raw / Length;
    float ClampedLen = FMath::Min(Length, MaxRadius);
    float Mag = FMath::Clamp(ClampedLen / MaxRadius, 0.f, 1.f);

    // Apply dead zone
    if (Mag < DeadZone)
    {
        OutState.Direction = FVector2D::ZeroVector;
        OutState.Magnitude = 0.f;
    }
    else
    {
        float ScaledMag = (Mag - DeadZone) / (1.f - DeadZone);
        OutState.Direction = Normalized * ScaledMag;
        OutState.Magnitude = ScaledMag;
    }
    OutState.bIsActive = true;
}

// ─── Input injection ───

void UTouchInputManager::InjectInputAction(const FName& ActionName, bool bPressed)
{
    APlayerController* PC = Cast<APlayerController>(GetOwner());
    if (!PC || !PC->PlayerInput) return;

    FInputActionBinding* Binding = nullptr;
    // Note: in shipping, we use legacy PlayerInput::ExecInputCommands
    // For simplicity, call the action directly via console command
    FString Cmd = FString::Printf(TEXT("Action:%s"), *ActionName.ToString());
    if (bPressed)
    {
        PC->ConsoleCommand(Cmd + TEXT(" 1"), true);
    }
    else
    {
        PC->ConsoleCommand(Cmd + TEXT(" 0"), true);
    }
}
