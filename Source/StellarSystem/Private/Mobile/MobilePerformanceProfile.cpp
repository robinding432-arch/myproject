// MobilePerformanceProfile.cpp
// v7.2 — SoC detection and quality preset application

#include "Mobile/MobilePerformanceProfile.h"
#include "HAL/Platform.h"
#include "Misc/CommandLine.h"
#include "Engine/Engine.h"
#include "RendererInterface.h"
#include "TimerManager.h"

void UMobilePerformanceProfile::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    DetectDevice();
    AutoDetectAndApply();
}

void UMobilePerformanceProfile::Deinitialize()
{
    Super::Deinitialize();
}

UMobilePerformanceProfile* UMobilePerformanceProfile::Get(const UObject* WorldContextObject)
{
    if (!WorldContextObject) return nullptr;
    UWorld* World = WorldContextObject->GetWorld();
    if (!World) return nullptr;
    UGameInstance* GI = World->GetGameInstance();
    if (!GI) return nullptr;
    return GI->GetSubsystem<UMobilePerformanceProfile>();
}

void UMobilePerformanceProfile::DetectDevice()
{
#if PLATFORM_ANDROID
    DetectAndroidDevice();
#elif PLATFORM_IOS
    DetectIOSDevice();
#else
    // Desktop fallback for testing
    Device.ChipsetName = TEXT("Desktop (testing)");
    Device.Vendor = ESoCVendor::Unknown;
    Device.CPUCores = 8;
    Device.GPUCores = 8;
    Device.RAM_MB = 8192;
    Device.bIsTablet = false;
    Device.bIsFoldable = false;
    Device.OSVersion = TEXT("Win/Mac/Linux");
    Device.bSupportsVulkan = true;
    Device.bSupportsMetal = false;
#endif
}

void UMobilePerformanceProfile::DetectAndroidDevice()
{
    // In production: query android.os.Build via JNI
    // For now, set conservative defaults
    Device.ChipsetName = FAndroidMisc::GetDeviceModel();
    FString Brand = FAndroidMisc::GetDeviceBrand().ToLower();

    if (Brand.Contains(TEXT("qualcomm")) || Brand.Contains(TEXT("snapdragon")) || Brand.Contains(TEXT("samsung")) || Brand.Contains(TEXT("xiaomi")) || Brand.Contains(TEXT("oneplus")))
    {
        Device.Vendor = ESoCVendor::Qualcomm;
    }
    else if (Brand.Contains(TEXT("mediatek")) || Brand.Contains(TEXT("dimensity")))
    {
        Device.Vendor = ESoCVendor::MediaTek;
    }
    else if (Brand.Contains(TEXT("huawei")) || Brand.Contains(TEXT("kirin")))
    {
        Device.Vendor = ESoCVendor::Huawei;
    }
    else if (Brand.Contains(TEXT("google")) || Brand.Contains(TEXT("pixel")))
    {
        Device.Vendor = ESoCVendor::Google;
    }

    Device.CPUCores = FPlatformMisc::NumberOfCores();
    Device.RAM_MB = FPlatformMemory::GetPhysicalGBRam() * 1024;
    Device.bIsTablet = (FAndroidMisc::GetDeviceType() == FAndroidMisc::DEVICE_TYPE_TABLET);
    Device.OSVersion = FAndroidMisc::GetOSVersion();
    Device.bSupportsVulkan = true; // Assume Vulkan capable
}

void UMobilePerformanceProfile::DetectIOSDevice()
{
    Device.Vendor = ESoCVendor::Apple;
    Device.ChipsetName = FIOSPlatformMisc::GetDefaultDeviceProfileName();
    Device.CPUCores = FPlatformMisc::NumberOfCores();
    Device.RAM_MB = FPlatformMemory::GetPhysicalGBRam() * 1024;
    Device.bIsTablet = (FIOSPlatformMisc::GetDeviceType() == FIOSPlatformMisc::DEVICE_TYPE_IPAD);
    Device.OSVersion = FIOSPlatformMisc::GetOSVersion();
    Device.bSupportsVulkan = false;
    Device.bSupportsMetal = true;
}

EMobileQuality UMobilePerformanceProfile::ScoreToTier(int32 Score) const
{
    if (Score >= 80) return EMobileQuality::Ultra;
    if (Score >= 60) return EMobileQuality::High;
    if (Score >= 40) return EMobileQuality::Medium;
    if (Score >= 20) return EMobileQuality::Low;
    return EMobileQuality::UltraLow;
}

int32 UMobilePerformanceProfile::RateDevice() const
{
    int32 Score = 0;

    // RAM weight: 30 pts
    Score += FMath::Min(Device.RAM_MB / 256, 30);

    // CPU cores: 20 pts
    Score += FMath::Min(Device.CPUCores * 3, 20);

    // GPU cores (estimated): 20 pts
    Score += FMath::Min(Device.GPUCores * 3, 20);

    // Vendor bonus
    switch (Device.Vendor)
    {
    case ESoCVendor::Apple:
        Score += 15; // Apple GPUs are efficient
        break;
    case ESoCVendor::Qualcomm:
        Score += 10;
        break;
    case ESoCVendor::MediaTek:
        Score += 8;
        break;
    case ESoCVendor::Google:
        Score += 12; // Tensor optimized
        break;
    default:
        break;
    }

    // Tablet gets slight bonus (better thermals)
    if (Device.bIsTablet) Score += 5;

    return FMath::Min(Score, 100);
}

void UMobilePerformanceProfile::AutoDetectAndApply()
{
    int32 Score = RateDevice();
    EMobileQuality Tier = ScoreToTier(Score);
    SetQualityTier(Tier);
}

void UMobilePerformanceProfile::SetQualityTier(EMobileQuality Tier)
{
    if (QualityTier == Tier) return;
    QualityTier = Tier;
    Settings = GetRecommendedSettings(Tier);
    ApplySettings();
    OnQualityChanged.Broadcast(Tier);
}

FMobileQualitySettings UMobilePerformanceProfile::GetRecommendedSettings(EMobileQuality Tier) const
{
    FMobileQualitySettings S;
    switch (Tier)
    {
    case EMobileQuality::UltraLow:
        S.ResolutionScale = 0.5f;
        S.ViewDistance = 2000.f;
        S.ShadowQuality = 0;
        S.TextureQuality = 0;
        S.EffectsQuality = 0;
        S.FoliageDensity = 0.2f;
        S.AntiAliasing = 0;
        S.TargetFrameRate = 30;
        S.bDynamicResolution = true;
        S.MinResolutionScale = 0.4f;
        S.ParticleQuality = 0;
        S.PhysicsHz = 20;
        S.AudioQuality = 0;
        S.NetworkTickRate = 10;
        break;
    case EMobileQuality::Low:
        S.ResolutionScale = 0.6f;
        S.ViewDistance = 3500.f;
        S.ShadowQuality = 0;
        S.TextureQuality = 1;
        S.EffectsQuality = 0;
        S.FoliageDensity = 0.35f;
        S.AntiAliasing = 1;
        S.TargetFrameRate = 30;
        S.bDynamicResolution = true;
        S.MinResolutionScale = 0.5f;
        S.ParticleQuality = 0;
        S.PhysicsHz = 25;
        S.AudioQuality = 1;
        S.NetworkTickRate = 15;
        break;
    case EMobileQuality::Medium:
        S.ResolutionScale = 0.75f;
        S.ViewDistance = 5000.f;
        S.ShadowQuality = 1;
        S.TextureQuality = 1;
        S.EffectsQuality = 1;
        S.FoliageDensity = 0.5f;
        S.AntiAliasing = 1;
        S.TargetFrameRate = 45;
        S.bDynamicResolution = true;
        S.MinResolutionScale = 0.6f;
        S.ParticleQuality = 1;
        S.PhysicsHz = 30;
        S.AudioQuality = 1;
        S.NetworkTickRate = 20;
        break;
    case EMobileQuality::High:
        S.ResolutionScale = 0.85f;
        S.ViewDistance = 8000.f;
        S.ShadowQuality = 2;
        S.TextureQuality = 2;
        S.EffectsQuality = 2;
        S.FoliageDensity = 0.75f;
        S.AntiAliasing = 2;
        S.TargetFrameRate = 60;
        S.bDynamicResolution = true;
        S.MinResolutionScale = 0.7f;
        S.ParticleQuality = 2;
        S.PhysicsHz = 60;
        S.AudioQuality = 2;
        S.NetworkTickRate = 25;
        break;
    case EMobileQuality::Ultra:
        S.ResolutionScale = 1.0f;
        S.ViewDistance = 12000.f;
        S.ShadowQuality = 3;
        S.TextureQuality = 3;
        S.EffectsQuality = 2;
        S.FoliageDensity = 1.0f;
        S.AntiAliasing = 2;
        S.TargetFrameRate = 60;
        S.bDynamicResolution = false;
        S.MinResolutionScale = 0.85f;
        S.ParticleQuality = 2;
        S.PhysicsHz = 60;
        S.AudioQuality = 2;
        S.NetworkTickRate = 30;
        break;
    }
    return S;
}

void UMobilePerformanceProfile::ApplySettings()
{
    // Apply resolution scale
    if (GEngine)
    {
        GEngine->SetScreenPercentage(Settings.ResolutionScale * 100.f);
    }

    // Set frame rate cap
    FPlatformMisc::SetFrameRateLimit(Settings.TargetFrameRate);

    // Apply texture quality via console variables
    IConsoleVariable* TexCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streaming.PoolSize"));
    if (TexCVar)
    {
        int32 PoolSize = (Settings.TextureQuality + 1) * 256; // 256/512/768/1024
        TexCVar->Set(PoolSize);
    }

    // Shadow quality
    IConsoleVariable* ShadowCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.Quality"));
    if (ShadowCVar) ShadowCVar->Set(Settings.ShadowQuality);

    // Anti-aliasing
    IConsoleVariable* AACVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AntiAliasingMethod"));
    if (AACVar) AACVar->Set(Settings.AntiAliasing);

    // Effects
    IConsoleVariable* FXCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.EffectsQuality"));
    if (FXCVar) FXCVar->Set(Settings.EffectsQuality);

    // Dynamic resolution
    IConsoleVariable* DRCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DynamicRes.OperationMode"));
    if (DRCVar) DRCVar->Set(Settings.bDynamicResolution ? 1 : 0);

    // Min resolution
    IConsoleVariable* MinResCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DynamicRes.MinScreenPercentage"));
    if (MinResCVar) MinResCVar->Set((int32)(Settings.MinResolutionScale * 100.f));

    // Physics sub-step
    // (would set PhysicsSettings::MaxSubsteps via config)

    // Network tick rate
    // (applied via NetDriver config)
}

void UMobilePerformanceProfile::TickPerformance(float DeltaTime)
{
    UpdateFrameStats(DeltaTime);
    CheckThermalState();

    // Dynamic resolution adjustment
    if (Settings.bDynamicResolution)
    {
        DRTimer += DeltaTime;
        if (DRTimer >= 1.f) // Check every second
        {
            DRTimer = 0.f;
            float TargetFPS = (float)Settings.TargetFrameRate;
            float Ratio = CurrentFPS / TargetFPS;

            if (Ratio < 0.85f) // Below 85% target → drop res
            {
                CurrentResolutionScale = FMath::Max(Settings.MinResolutionScale, CurrentResolutionScale - 0.05f);
                GEngine->SetScreenPercentage(CurrentResolutionScale * 100.f);
            }
            else if (Ratio > 0.98f) // Near target → try raising
            {
                CurrentResolutionScale = FMath::Min(Settings.ResolutionScale, CurrentResolutionScale + 0.02f);
                GEngine->SetScreenPercentage(CurrentResolutionScale * 100.f);
            }
        }
    }
}

void UMobilePerformanceProfile::UpdateFrameStats(float DeltaTime)
{
    if (DeltaTime <= 0.f) return;
    float FPS = 1.f / DeltaTime;

    // EMA
    CurrentFPS = (CurrentFPS == 0.f) ? FPS : (CurrentFPS * 0.9f + FPS * 0.1f);

    // History
    FPSHistory.Add(FPS);
    if (FPSHistory.Num() > 60) FPSHistory.RemoveAt(0);
}

float UMobilePerformanceProfile::GetAverageFPS() const
{
    if (FPSHistory.Num() == 0) return CurrentFPS;
    float Sum = 0.f;
    for (float F : FPSHistory) Sum += F;
    return Sum / (float)FPSHistory.Num();
}

void UMobilePerformanceProfile::CheckThermalState()
{
    ThermalCheckTimer += 0.016f; // Approximate
    if (ThermalCheckTimer < 5.f) return;
    ThermalCheckTimer = 0.f;

    // On mobile, would call platform-specific thermal API
    // For now, use FPS trend as proxy
    float AvgFPS = GetAverageFPS();
    float TargetFPS = (float)Settings.TargetFrameRate;

    if (AvgFPS < TargetFPS * 0.6f && !bThermalThrottling)
    {
        bThermalThrottling = true;
        DowngradeOneLevel();
        OnThermalWarning.Broadcast();
    }
    else if (AvgFPS > TargetFPS * 0.85f && bThermalThrottling)
    {
        bThermalThrottling = false;
    }
}

void UMobilePerformanceProfile::DowngradeOneLevel()
{
    EMobileQuality NewTier;
    switch (QualityTier)
    {
    case EMobileQuality::Ultra:     NewTier = EMobileQuality::High; break;
    case EMobileQuality::High:      NewTier = EMobileQuality::Medium; break;
    case EMobileQuality::Medium:    NewTier = EMobileQuality::Low; break;
    case EMobileQuality::Low:       NewTier = EMobileQuality::UltraLow; break;
    default: return; // Already at lowest
    }
    SetQualityTier(NewTier);
}
