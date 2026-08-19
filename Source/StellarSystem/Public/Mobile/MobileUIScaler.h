// MobileUIScaler.h
// v7.2 — DPI-aware UI scaling for phones and tablets

#pragma once

#include "CoreMinimal.h"
#include "MobileUIScaler.generated.h"

/** Device form factor classification */
UENUM(BlueprintType)
enum class EDeviceFormFactor : uint8
{
    PhoneSmall  UMETA(DisplayName = "Small Phone (<5.5\")"),
    PhoneLarge  UMETA(DisplayName = "Large Phone (5.5-7\")"),
    Tablet      UMETA(DisplayName = "Tablet (7-11\")"),
    TabletLarge UMETA(DisplayName = "Large Tablet (>11\")"),
    Foldable    UMETA(DisplayName = "Foldable (adaptive)"),
    Unknown     UMETA(DisplayName = "Unknown"),
};

/** Safe area (notch / punch-hole / gesture bar) */
USTRUCT(BlueprintType)
struct FSafeAreaInsets
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    float Top = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float Bottom = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float Left = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float Right = 0.f;
};

/** Scaling configuration */
USTRUCT(BlueprintType)
struct FUIScaleConfig
{
    GENERATED_BODY()

    /** Base canvas resolution (reference) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector2D ReferenceResolution = FVector2D(1920.f, 1080.f);

    /** Minimum scale factor */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MinScale = 0.6f;

    /** Maximum scale factor */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxScale = 1.8f;

    /** Scale curve exponent (1=linear, >1=favor larger) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ScaleCurve = 1.15f;

    /** Font scale multiplier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FontScale = 1.0f;

    /** Icon scale multiplier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float IconScale = 1.0f;

    /** Touch target minimum size (pixels) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MinTouchTarget = 48.f;

    /** Auto-rotate between landscape/portrait */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bAutoRotate = true;

    /** Prefer landscape (games usually do) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bPreferLandscape = true;
};

/**
 * UMobileUIScaler — singleton subsystem
 * Detects device, computes safe area, provides scaling factors
 * for all mobile UI widgets.
 */
UCLASS(BlueprintType)
class STELLARSYSTEM_API UMobileUIScaler : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Get the singleton instance */
    UFUNCTION(BlueprintPure, Category = "Mobile UI", meta = (WorldContext = "WorldContextObject"))
    static UMobileUIScaler* Get(const UObject* WorldContextObject);

    /** Detect device form factor */
    UFUNCTION(BlueprintPure, Category = "Mobile UI")
    EDeviceFormFactor GetDeviceFormFactor() const { return DeviceType; }

    /** Get safe area insets in pixels */
    UFUNCTION(BlueprintPure, Category = "Mobile UI")
    FSafeAreaInsets GetSafeArea() const { return SafeArea; }

    /** Get overall UI scale factor */
    UFUNCTION(BlueprintPure, Category = "Mobile UI")
    float GetUIScale() const { return UIScale; }

    /** Get font scale (for text widgets) */
    UFUNCTION(BlueprintPure, Category = "Mobile UI")
    float GetFontScale() const { return Config.FontScale * UIScale; }

    /** Get icon/button scale */
    UFUNCTION(BlueprintPure, Category = "Mobile UI")
    float GetIconScale() const { return Config.IconScale * UIScale; }

    /** Is portrait orientation */
    UFUNCTION(BlueprintPure, Category = "Mobile UI")
    bool IsPortrait() const;

    /** Is landscape orientation */
    UFUNCTION(BlueprintPure, Category = "Mobile UI")
    bool IsLandscape() const;

    /** Get screen size in pixels */
    UFUNCTION(BlueprintPure, Category = "Mobile UI")
    FVector2D GetScreenSize() const;

    /** Clamp a position to safe area */
    UFUNCTION(BlueprintCallable, Category = "Mobile UI")
    FVector2D ClampToSafeArea(const FVector2D& Position, float Margin = 8.f) const;

    /** Get recommended button size (scaled touch target) */
    UFUNCTION(BlueprintPure, Category = "Mobile UI")
    float GetRecommendedButtonSize() const { return Config.MinTouchTarget * UIScale; }

    /** Force refresh (call on orientation change) */
    UFUNCTION(BlueprintCallable, Category = "Mobile UI")
    void Refresh();

    /** Configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mobile UI")
    FUIScaleConfig Config;

    /** Event: safe area changed (orientation/device change) */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSafeAreaChanged);
    UPROPERTY(BlueprintAssignable, Category = "Mobile UI")
    FOnSafeAreaChanged OnSafeAreaChanged;

    /** Event: orientation changed */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOrientationChanged, bool, bIsPortrait);
    UPROPERTY(BlueprintAssignable, Category = "Mobile UI")
    FOnOrientationChanged OnOrientationChanged;

private:
    void DetectDevice();
    void ComputeSafeArea();
    void ComputeUIScale();

    EDeviceFormFactor DeviceType = EDeviceFormFactor::Unknown;
    FSafeAreaInsets SafeArea;
    float UIScale = 1.f;
    FVector2D CachedScreenSize = FVector2D::ZeroVector;
    bool bCachedPortrait = false;
};
