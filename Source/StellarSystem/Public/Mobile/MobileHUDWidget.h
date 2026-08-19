// MobileHUDWidget.h
// v7.2 — Mobile-optimized in-game HUD

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MobileHUDWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UImage;
class UCanvasPanel;
class UOverlay;
class UButton;
class UWidgetSwitcher;

/** HUD display mode */
UENUM(BlueprintType)
enum class EHUDMode : uint8
{
    Combat      UMETA(DisplayName = "Combat"),
    Navigation  UMETA(DisplayName = "Navigation"),
    Mining      UMETA(DisplayName = "Mining"),
    Trading     UMETA(DisplayName = "Trading"),
    Social      UMETA(DisplayName = "Social"),
    Minimal     UMETA(DisplayName = "Minimal"),
};

/**
 * WMobileHUD — compact, touch-friendly HUD
 * Collapsible panels, large readable text, minimal clutter
 */
UCLASS(BlueprintType)
class STELLARSYSTEM_API UMobileHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    /** Set HUD mode (changes visible panels) */
    UFUNCTION(BlueprintCallable, Category = "Mobile HUD")
    void SetHUDMode(EHUDMode Mode);

    /** Get current HUD mode */
    UFUNCTION(BlueprintPure, Category = "Mobile HUD")
    EHUDMode GetHUDMode() const { return CurrentMode; }

    /** Toggle minimal mode (one-tap to hide all) */
    UFUNCTION(BlueprintCallable, Category = "Mobile HUD")
    void ToggleMinimalMode();

    /** Update health bar (0-1) */
    UFUNCTION(BlueprintCallable, Category = "Mobile HUD")
    void SetHealth(float Value);

    /** Update shield bar (0-1) */
    UFUNCTION(BlueprintCallable, Category = "Mobile HUD")
    void SetShield(float Value);

    /** Update oxygen bar (0-1) */
    UFUNCTION(BlueprintCallable, Category = "Mobile HUD")
    void SetOxygen(float Value);

    /** Update energy bar (0-1) */
    UFUNCTION(BlueprintCallable, Category = "Mobile HUD")
    void SetEnergy(float Value);

    /** Set speed text */
    UFUNCTION(BlueprintCallable, Category = "Mobile HUD")
    void SetSpeed(float SpeedKph);

    /** Set altitude text */
    UFUNCTION(BlueprintCallable, Category = "Mobile HUD")
    void SetAltitude(float AltitudeMeters);

    /** Set target info */
    UFUNCTION(BlueprintCallable, Category = "Mobile HUD")
    void SetTargetInfo(const FString& TargetName, float DistanceKm, float HullPercent);

    /** Clear target info */
    UFUNCTION(BlueprintCallable, Category = "Mobile HUD")
    void ClearTargetInfo();

    /** Show notification (top-right) */
    UFUNCTION(BlueprintCallable, Category = "Mobile HUD")
    void ShowNotification(const FString& Text, float DurationSeconds = 4.f, FLinearColor Color = FLinearColor::White);

    /** Set chat message (auto-fades) */
    UFUNCTION(BlueprintCallable, Category = "Mobile HUD")
    void AddChatMessage(const FString& Sender, const FString& Message, FLinearColor SenderColor = FLinearColor::Cyan);

    /** Set compass heading (degrees 0-360) */
    UFUNCTION(BlueprintCallable, Category = "Mobile HUD")
    void SetCompassHeading(float HeadingDegrees);

    /** Set minimap texture/rotation */
    UFUNCTION(BlueprintCallable, Category = "Mobile HUD")
    void SetMinimapRotation(float RotationDegrees);

    /** Pulse damage indicator */
    UFUNCTION(BlueprintCallable, Category = "Mobile HUD")
    void PulseDamageDirection(const FVector& DamageDirection);

    /** Event: fire button pressed */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFirePressed);
    UPROPERTY(BlueprintAssignable, Category = "Mobile HUD")
    FOnFirePressed OnFirePressed;

    /** Event: fire button released */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFireReleased);
    UPROPERTY(BlueprintAssignable, Category = "Mobile HUD")
    FOnFireReleased OnFireReleased;

    /** Event: jump/boost button */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBoostPressed);
    UPROPERTY(BlueprintAssignable, Category = "Mobile HUD")
    FOnBoostPressed OnBoostPressed;

    /** Event: interact button */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInteractPressed);
    UPROPERTY(BlueprintAssignable, Category = "Mobile HUD")
    FOnInteractPressed OnInteractPressed;

    /** Event: menu button */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMenuPressed);
    UPROPERTY(BlueprintAssignable, Category = "Mobile HUD")
    FOnMenuPressed OnMenuPressed;

    /** Event: map toggle */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMapToggled);
    UPROPERTY(BlueprintAssignable, Category = "Mobile HUD")
    FOnMapToggled OnMapToggled;

protected:
    /** UI Bindings */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UProgressBar* ProgressHealth;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UProgressBar* ProgressShield;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UProgressBar* ProgressOxygen;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UProgressBar* ProgressEnergy;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* TxtSpeed;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* TxtAltitude;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* TxtTarget;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* TxtNotification;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* TxtChat;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UImage* ImgCompass;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UImage* ImgDamageIndicator;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UButton* BtnFire;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UButton* BtnBoost;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UButton* BtnInteract;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UButton* BtnMenu;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UButton* BtnMap;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UWidgetSwitcher* PanelModeSwitcher;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UOverlay* OverlayNotifications;

    /** Button handlers */
    UFUNCTION()
    void HandleFirePressed();

    UFUNCTION()
    void HandleFireReleased();

    UFUNCTION()
    void HandleBoostPressed();

    UFUNCTION()
    void HandleInteractPressed();

    UFUNCTION()
    void HandleMenuPressed();

    UFUNCTION()
    void HandleMapPressed();

private:
    EHUDMode CurrentMode = EHUDMode::Navigation;

    /** Auto-hide timers */
    FTimerHandle NotificationTimer;
    FTimerHandle ChatTimer;
    FTimerHandle DamageFadeTimer;
    float DamageIndicatorAlpha = 0.f;

    /** Chat message queue */
    TArray<FString> ChatHistory;
    int32 MaxChatLines = 5;

    /** Update tick */
    void TickNotifications(float DeltaTime);
    void TickDamageIndicator(float DeltaTime);
    void FadeOutNotification();
    void FadeOutChat();
};
