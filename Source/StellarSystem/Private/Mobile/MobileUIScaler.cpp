// MobileUIScaler.cpp
// v7.2 — DPI-aware UI scaling implementation

#include "Mobile/MobileUIScaler.h"
#include "Engine/Engine.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/App.h"

void UMobileUIScaler::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Refresh();
}

void UMobileUIScaler::Deinitialize()
{
    Super::Deinitialize();
}

UMobileUIScaler* UMobileUIScaler::Get(const UObject* WorldContextObject)
{
    if (!WorldContextObject) return nullptr;
    UWorld* World = WorldContextObject->GetWorld();
    if (!World) return nullptr;
    UGameInstance* GI = World->GetGameInstance();
    if (!GI) return nullptr;
    return GI->GetSubsystem<UMobileUIScaler>();
}

void UMobileUIScaler::Refresh()
{
    DetectDevice();
    ComputeSafeArea();
    ComputeUIScale();

    OnSafeAreaChanged.Broadcast();
    OnOrientationChanged.Broadcast(bCachedPortrait);
}

void UMobileUIScaler::DetectDevice()
{
    // Get screen size
    FVector2D Screen = GetScreenSize();
    float DiagonalInches = 0.f;

    // Approximate DPI (platform-specific would be more accurate)
    float ApproxDPI = FMath::Sqrt(Screen.X * Screen.X + Screen.Y * Screen.Y) / 5.5f; // rough

    // Classify by diagonal
    if (Screen.X < 500.f)
    {
        DeviceType = EDeviceFormFactor::PhoneSmall;
    }
    else if (Screen.X < 900.f)
    {
        DeviceType = EDeviceFormFactor::PhoneLarge;
    }
    else if (Screen.X < 1400.f)
    {
        DeviceType = EDeviceFormFactor::Tablet;
    }
    else
    {
        DeviceType = EDeviceFormFactor::TabletLarge;
    }

    // Foldable detection (very rough — would need JNI on Android)
    if (Screen.X > 1600.f && Screen.Y > 2000.f)
    {
        DeviceType = EDeviceFormFactor::Foldable;
    }

    CachedScreenSize = Screen;
    bCachedPortrait = Screen.Y > Screen.X;
}

void UMobileUIScaler::ComputeSafeArea()
{
    // Default safe area (platform-specific would query notch/punch-hole)
    FVector2D Screen = GetScreenSize();

    // Conservative defaults (iPhone notch ~44pt top, 34pt bottom)
    float TopInset = bCachedPortrait ? 60.f : 20.f;
    float BottomInset = bCachedPortrait ? 40.f : 20.f;
    float SideInset = bCachedPortrait ? 0.f : 30.f;

    // Scale by DPI
    float DPIScale = FMath::Sqrt(Screen.X * Screen.Y) / 2200.f;
    DPIScale = FMath::Clamp(DPIScale, 0.5f, 2.0f);

    SafeArea.Top = TopInset * DPIScale;
    SafeArea.Bottom = BottomInset * DPIScale;
    SafeArea.Left = SideInset * DPIScale;
    SafeArea.Right = SideInset * DPIScale;
}

void UMobileUIScaler::ComputeUIScale()
{
    FVector2D Screen = GetScreenSize();
    float ReferenceArea = Config.ReferenceResolution.X * Config.ReferenceResolution.Y;
    float CurrentArea = Screen.X * Screen.Y;
    float Ratio = FMath::Sqrt(CurrentArea / ReferenceArea);

    // Apply curve
    float Scaled = FMath::Pow(Ratio, Config.ScaleCurve);

    // Clamp
    UIScale = FMath::Clamp(Scaled, Config.MinScale, Config.MaxScale);

    // Device-type modifier
    switch (DeviceType)
    {
    case EDeviceFormFactor::PhoneSmall:  UIScale *= 0.85f; break;
    case EDeviceFormFactor::PhoneLarge:  UIScale *= 1.0f;  break;
    case EDeviceFormFactor::Tablet:      UIScale *= 1.2f;  break;
    case EDeviceFormFactor::TabletLarge: UIScale *= 1.4f;  break;
    case EDeviceFormFactor::Foldable:    UIScale *= 1.15f; break;
    default: break;
    }

    UIScale = FMath::Clamp(UIScale, Config.MinScale, Config.MaxScale);
}

bool UMobileUIScaler::IsPortrait() const
{
    return bCachedPortrait;
}

bool UMobileUIScaler::IsLandscape() const
{
    return !bCachedPortrait;
}

FVector2D UMobileUIScaler::GetScreenSize() const
{
    if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
    {
        FVector2D Size;
        GEngine->GameViewport->Viewport->GetSize(Size);
        return Size;
    }
    return CachedScreenSize;
}

FVector2D UMobileUIScaler::ClampToSafeArea(const FVector2D& Position, float Margin) const
{
    FVector2D Screen = GetScreenSize();
    return FVector2D(
        FMath::Clamp(Position.X, SafeArea.Left + Margin, Screen.X - SafeArea.Right - Margin),
        FMath::Clamp(Position.Y, SafeArea.Top + Margin, Screen.Y - SafeArea.Bottom - Margin)
    );
}
