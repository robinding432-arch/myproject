// MobilePerformanceProfile.h
// v7.2 — Auto-detect SoC and apply appropriate quality preset

#pragma once

#include "CoreMinimal.h"
#include "MobilePerformanceProfile.generated.h"

/** Quality tiers for mobile */
UENUM(BlueprintType)
enum class EMobileQuality : uint8
{
    UltraLow   UMETA(DisplayName = "Ultra Low (30fps)"),
    Low        UMETA(DisplayName = "Low (30fps)"),
    Medium     UMETA(DisplayName = "Medium (45fps)"),
    High       UMETA(DisplayName = "High (60fps)"),
    Ultra      UMETA(DisplayName = "Ultra (60fps+)"),
};

/** SoC vendor */
UENUM(BlueprintType)
enum class ESoCVendor : uint8
{
    Unknown     UMETA(DisplayName = "Unknown"),
    Qualcomm   UMETA(DisplayName = "Qualcomm Snapdragon"),
    MediaTek   UMETA(DisplayName = "MediaTek Dimensity"),
    Samsung    UMETA(DisplayName = "Samsung Exynos"),
    Apple      UMETA(DisplayName = "Apple A-Series"),
    Huawei     UMETA(DisplayName = "Huawei Kirin"),
    Google     UMETA(DisplayName = "Google Tensor"),
};

/** Detected device info */
USTRUCT(BlueprintType)
struct FDeviceInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    ESoCVendor Vendor = ESoCVendor::Unknown;

    UPROPERTY(BlueprintReadOnly)
    FString ChipsetName = TEXT("Unknown");

    UPROPERTY(BlueprintReadOnly)
    int32 CPUCores = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 GPUCores = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 RAM_MB = 0;

    UPROPERTY(BlueprintReadOnly)
    bool bIsTablet = false;

    UPROPERTY(BlueprintReadOnly)
    bool bIsFoldable = false;

    UPROPERTY(BlueprintReadOnly)
    FString OSVersion = TEXT("");

    UPROPERTY(BlueprintReadOnly)
    bool bSupportsVulkan = false;

    UPROPERTY(BlueprintReadOnly)
    bool bSupportsMetal = false;
};

/** Quality settings applied to the engine */
USTRUCT(BlueprintType)
struct FMobileQualitySettings
{
    GENERATED_BODY()

    /** Render resolution scale (0.5-1.0) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ResolutionScale = 0.75f;

    /** View distance (cm) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ViewDistance = 5000.f;

    /** Shadow quality (0=off, 1=low, 2=med, 3=high) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ShadowQuality = 1;

    /** Texture quality (0=low, 1=med, 2=high, 3=ultra) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 TextureQuality = 1;

    /** Effects quality (0-3) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 EffectsQuality = 1;

    /** Foliage density (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FoliageDensity = 0.5f;

    /** Anti-aliasing (0=off, 1=FXAA, 2=TAA, 3=MSAA2x, 4=MSAA4x) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 AntiAliasing = 1;

    /** Target frame rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 TargetFrameRate = 30;

    /** Allow dynamic resolution scaling */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bDynamicResolution = true;

    /** Min resolution scale when dynamic */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MinResolutionScale = 0.5f;

    /** Niagara particle quality (0=off, 1=low, 2=med) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ParticleQuality = 1;

    /** Physics sub-stepping (lower = cheaper) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 PhysicsHz = 30;

    /** Audio quality (0=low, 1=med, 2=high) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 AudioQuality = 1;

    /** Network update rate (Hz) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 NetworkTickRate = 20;
};

/**
 * UMobilePerformanceProfile — auto-detects device and applies optimal settings
 * Singleton subsystem, initialized at game start.
 */
UCLASS(BlueprintType)
class STELLARSYSTEM_API UMobilePerformanceProfile : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Get singleton */
    UFUNCTION(BlueprintPure, Category = "Mobile Perf", meta = (WorldContext = "WorldContextObject"))
    static UMobilePerformanceProfile* Get(const UObject* WorldContextObject);

    /** Get detected device info */
    UFUNCTION(BlueprintPure, Category = "Mobile Perf")
    FDeviceInfo GetDeviceInfo() const { return Device; }

    /** Get current quality tier */
    UFUNCTION(BlueprintPure, Category = "Mobile Perf")
    EMobileQuality GetQualityTier() const { return QualityTier; }

    /** Get current applied settings */
    UFUNCTION(BlueprintPure, Category = "Mobile Perf")
    FMobileQualitySettings GetSettings() const { return Settings; }

    /** Apply a specific quality tier (user override) */
    UFUNCTION(BlueprintCallable, Category = "Mobile Perf")
    void SetQualityTier(EMobileQuality Tier);

    /** Auto-detect and apply (call on startup) */
    UFUNCTION(BlueprintCallable, Category = "Mobile Perf")
    void AutoDetectAndApply();

    /** Get recommended settings for a tier */
    UFUNCTION(BlueprintPure, Category = "Mobile Perf")
    FMobileQualitySettings GetRecommendedSettings(EMobileQuality Tier) const;

    /** Monitor frame rate and adjust dynamically */
    UFUNCTION(BlueprintCallable, Category = "Mobile Perf")
    void TickPerformance(float DeltaTime);

    /** Force apply current settings to engine */
    UFUNCTION(BlueprintCallable, Category = "Mobile Perf")
    void ApplySettings();

    /** Is device thermal throttling (best-effort detection) */
    UFUNCTION(BlueprintPure, Category = "Mobile Perf")
    bool IsThermalThrottling() const { return bThermalThrottling; }

    /** Get current FPS */
    UFUNCTION(BlueprintPure, Category = "Mobile Perf")
    float GetCurrentFPS() const { return CurrentFPS; }

    /** Get average FPS over last 60 frames */
    UFUNCTION(BlueprintPure, Category = "Mobile Perf")
    float GetAverageFPS() const;

    /** Event: quality tier changed */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQualityChanged, EMobileQuality, NewTier);
    UPROPERTY(BlueprintAssignable, Category = "Mobile Perf")
    FOnQualityChanged OnQualityChanged;

    /** Event: thermal warning */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnThermalWarning);
    UPROPERTY(BlueprintAssignable, Category = "Mobile Perf")
    FOnThermalWarning OnThermalWarning;

private:
    void DetectDevice();
    void DetectAndroidDevice();
    void DetectIOSDevice();
    EMobileQuality ScoreToTier(int32 Score) const;
    int32 RateDevice() const;
    void UpdateFrameStats(float DeltaTime);
    void CheckThermalState();
    void DowngradeOneLevel();

    FDeviceInfo Device;
    EMobileQuality QualityTier = EMobileQuality::Medium;
    FMobileQualitySettings Settings;

    /** Frame stats */
    float CurrentFPS = 60.f;
    TArray<float> FPSHistory;
    int32 FPSHistoryIndex = 0;
    float FPSAccumulator = 0.f;
    int32 FPSAccumCount = 0;

    /** Thermal */
    bool bThermalThrottling = false;
    float ThermalCheckTimer = 0.f;

    /** Dynamic resolution */
    float CurrentResolutionScale = 0.75f;
    float DRTimer = 0.f;
};
