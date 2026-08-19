// MobileLODManager.h
// v7.2 — Aggressive LOD and culling for mobile

#pragma once

#include "CoreMinimal.h"
#include "MobileLODManager.generated.h"

/** LOD strategy for a specific feature */
UENUM(BlueprintType)
enum class EMobileLODStrategy : uint8
{
    Aggressive  UMETA(DisplayName = "Aggressive (max perf)"),
    Balanced    UMETA(DisplayName = "Balanced"),
    Quality     UMETA(DisplayName = "Quality (max visuals)"),
};

/** Per-feature LOD settings */
USTRUCT(BlueprintType)
struct FMobileLODSettings
{
    GENERATED_BODY()

    /** Max LOD level for static meshes (0=highest) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxStaticMeshLOD = 2;

    /** Max LOD level for skeletal meshes */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxSkeletalLOD = 1;

    /** Max LOD level for planet terrain */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxPlanetLOD = 2;

    /** Max LOD level for ships */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxShipLOD = 1;

    /** Max LOD level for space stations */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxStationLOD = 2;

    /** Max draw distance for small objects (cm) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxSmallObjectDistance = 3000.f;

    /** Max draw distance for medium objects (cm) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxMediumObjectDistance = 8000.f;

    /** Max draw distance for large objects (cm) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxLargeObjectDistance = 20000.f;

    /** Cull distance for particles (cm) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ParticleCullDistance = 5000.f;

    /** Cull distance for lights (cm) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LightCullDistance = 4000.f;

    /** Max simultaneous particle systems */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxActiveParticles = 32;

    /** Max shadow-casting lights */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxShadowLights = 1;

    /** Occlusion culling aggressiveness (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OcclusionAggression = 0.7f;

    /** Enable/disable landscape LOD streaming */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bStreamLandscapeLOD = true;

    /** Landscape LOD bias (positive = lower quality) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 LandscapeLODBias = 1;
};

/**
 * UMobileLODManager — manages all LOD/culling settings for mobile
 * Applies settings globally, monitors performance, auto-adjusts.
 */
UCLASS(BlueprintType)
class STELLARSYSTEM_API UMobileLODManager : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Get singleton */
    UFUNCTION(BlueprintPure, Category = "Mobile LOD", meta = (WorldContext = "WorldContextObject"))
    static UMobileLODManager* Get(const UObject* WorldContextObject);

    /** Set strategy (changes all settings at once) */
    UFUNCTION(BlueprintCallable, Category = "Mobile LOD")
    void SetStrategy(EMobileLODStrategy Strategy);

    /** Get current strategy */
    UFUNCTION(BlueprintPure, Category = "Mobile LOD")
    EMobileLODStrategy GetStrategy() const { return CurrentStrategy; }

    /** Get current settings */
    UFUNCTION(BlueprintPure, Category = "Mobile LOD")
    FMobileLODSettings GetSettings() const { return Settings; }

    /** Override specific setting */
    UFUNCTION(BlueprintCallable, Category = "Mobile LOD")
    void SetMaxDrawDistance(EFoliageType FoliageType, float Distance);

    /** Force apply all settings to the world */
    UFUNCTION(BlueprintCallable, Category = "Mobile LOD")
    void ApplySettings();

    /** Register an actor for mobile LOD management */
    UFUNCTION(BlueprintCallable, Category = "Mobile LOD")
    void RegisterActor(AActor* Actor);

    /** Unregister an actor */
    UFUNCTION(BlueprintCallable, Category = "Mobile LOD")
    void UnregisterActor(AActor* Actor);

    /** Update (call from tick) */
    UFUNCTION(BlueprintCallable, Category = "Mobile LOD")
    void Update(float DeltaTime);

    /** Get current triangle count estimate */
    UFUNCTION(BlueprintPure, Category = "Mobile LOD")
    int32 GetEstimatedTriangles() const { return EstimatedTriangles; }

    /** Get current draw call count */
    UFUNCTION(BlueprintPure, Category = "Mobile LOD")
    int32 GetDrawCallCount() const { return DrawCallCount; }

    /** Event: settings changed */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLODSettingsChanged, const FMobileLODSettings&, Settings);
    UPROPERTY(BlueprintAssignable, Category = "Mobile LOD")
    FOnLODSettingsChanged OnSettingsChanged;

    /** Event: performance warning (too many tris/draws) */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPerformanceWarning);
    UPROPERTY(BlueprintAssignable, Category = "Mobile LOD")
    FOnPerformanceWarning OnPerformanceWarning;

private:
    void ApplyAggressiveSettings();
    void ApplyBalancedSettings();
    void ApplyQualitySettings();
    void UpdateActorLODs();
    void CullDistantObjects();
    void UpdatePerformanceStats(float DeltaTime);
    void CheckPerformanceWarnings();

    EMobileLODStrategy CurrentStrategy = EMobileLODStrategy::Balanced;
    FMobileLODSettings Settings;

    /** Tracked actors */
    TSet<TWeakObjectPtr<AActor>> TrackedActors;

    /** Performance stats */
    int32 EstimatedTriangles = 0;
    int32 DrawCallCount = 0;
    float StatUpdateTimer = 0.f;

    /** Warning cooldown */
    float WarningCooldown = 0.f;

    /** Update timer */
    float UpdateTimer = 0.f;
    static constexpr float UPDATE_INTERVAL = 0.5f;
};
