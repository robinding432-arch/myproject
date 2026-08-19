// MobileLODManager.cpp
// v7.2 — Aggressive LOD/culling for mobile

#include "Mobile/MobileLODManager.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/LightComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "TimerManager.h"

void UMobileLODManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    SetStrategy(EMobileLODStrategy::Balanced);
}

void UMobileLODManager::Deinitialize()
{
    TrackedActors.Empty();
    Super::Deinitialize();
}

UMobileLODManager* UMobileLODManager::Get(const UObject* WorldContextObject)
{
    if (!WorldContextObject) return nullptr;
    UWorld* World = WorldContextObject->GetWorld();
    if (!World) return nullptr;
    return World->GetSubsystem<UMobileLODManager>();
}

void UMobileLODManager::SetStrategy(EMobileLODStrategy Strategy)
{
    if (CurrentStrategy == Strategy) return;
    CurrentStrategy = Strategy;

    switch (Strategy)
    {
    case EMobileLODStrategy::Aggressive: ApplyAggressiveSettings(); break;
    case EMobileLODStrategy::Balanced:   ApplyBalancedSettings(); break;
    case EMobileLODStrategy::Quality:    ApplyQualitySettings(); break;
    }

    ApplySettings();
    OnSettingsChanged.Broadcast(Settings);
}

void UMobileLODManager::ApplyAggressiveSettings()
{
    Settings.MaxStaticMeshLOD = 3;
    Settings.MaxSkeletalLOD = 2;
    Settings.MaxPlanetLOD = 3;
    Settings.MaxShipLOD = 2;
    Settings.MaxStationLOD = 3;
    Settings.MaxSmallObjectDistance = 1500.f;
    Settings.MaxMediumObjectDistance = 4000.f;
    Settings.MaxLargeObjectDistance = 10000.f;
    Settings.ParticleCullDistance = 2500.f;
    Settings.LightCullDistance = 2000.f;
    Settings.MaxActiveParticles = 16;
    Settings.MaxShadowLights = 0;
    Settings.OcclusionAggression = 0.9f;
    Settings.bStreamLandscapeLOD = true;
    Settings.LandscapeLODBias = 2;
}

void UMobileLODManager::ApplyBalancedSettings()
{
    Settings.MaxStaticMeshLOD = 2;
    Settings.MaxSkeletalLOD = 1;
    Settings.MaxPlanetLOD = 2;
    Settings.MaxShipLOD = 1;
    Settings.MaxStationLOD = 2;
    Settings.MaxSmallObjectDistance = 3000.f;
    Settings.MaxMediumObjectDistance = 8000.f;
    Settings.MaxLargeObjectDistance = 20000.f;
    Settings.ParticleCullDistance = 5000.f;
    Settings.LightCullDistance = 4000.f;
    Settings.MaxActiveParticles = 32;
    Settings.MaxShadowLights = 1;
    Settings.OcclusionAggression = 0.7f;
    Settings.bStreamLandscapeLOD = true;
    Settings.LandscapeLODBias = 1;
}

void UMobileLODManager::ApplyQualitySettings()
{
    Settings.MaxStaticMeshLOD = 1;
    Settings.MaxSkeletalLOD = 0;
    Settings.MaxPlanetLOD = 1;
    Settings.MaxShipLOD = 0;
    Settings.MaxStationLOD = 1;
    Settings.MaxSmallObjectDistance = 5000.f;
    Settings.MaxMediumObjectDistance = 12000.f;
    Settings.MaxLargeObjectDistance = 30000.f;
    Settings.ParticleCullDistance = 8000.f;
    Settings.LightCullDistance = 6000.f;
    Settings.MaxActiveParticles = 64;
    Settings.MaxShadowLights = 2;
    Settings.OcclusionAggression = 0.5f;
    Settings.bStreamLandscapeLOD = false;
    Settings.LandscapeLODBias = 0;
}

void UMobileLODManager::ApplySettings()
{
    // Apply to all tracked actors immediately
    UpdateActorLODs();
    CullDistantObjects();
}

void UMobileLODManager::RegisterActor(AActor* Actor)
{
    if (Actor) TrackedActors.Add(Actor);
}

void UMobileLODManager::UnregisterActor(AActor* Actor)
{
    if (Actor)
    {
        TWeakObjectPtr<AActor> Weak(Actor);
        TrackedActors.Remove(Weak);
    }
}

void UMobileLODManager::Update(float DeltaTime)
{
    UpdateTimer += DeltaTime;
    StatUpdateTimer += DeltaTime;

    if (UpdateTimer >= UPDATE_INTERVAL)
    {
        UpdateTimer = 0.f;
        UpdateActorLODs();
        CullDistantObjects();
    }

    if (StatUpdateTimer >= 1.f)
    {
        StatUpdateTimer = 0.f;
        UpdatePerformanceStats(DeltaTime);
    }
}

void UMobileLODManager::UpdateActorLODs()
{
    UWorld* World = GetWorld();
    if (!World) return;

    for (TWeakObjectPtr<AActor> Weak : TrackedActors)
    {
        if (!Weak.IsValid()) continue;
        AActor* Actor = Weak.Get();

        // Static mesh LOD
        TArray<UStaticMeshComponent*> MeshComps;
        Actor->GetComponents<UStaticMeshComponent>(MeshComps);
        for (UStaticMeshComponent* SMC : MeshComps)
        {
            if (SMC) SMC->SetForcedLodModel(FMath::Min(Settings.MaxStaticMeshLOD + 1, 8));
        }

        // Skeletal mesh LOD
        TArray<USkeletalMeshComponent*> SkelComps;
        Actor->GetComponents<USkeletalMeshComponent>(SkelComps);
        for (USkeletalMeshComponent* SKC : SkelComps)
        {
            if (SKC) SKC->SetForcedLodModel(FMath::Min(Settings.MaxSkeletalLOD + 1, 8));
        }
    }
}

void UMobileLODManager::CullDistantObjects()
{
    UWorld* World = GetWorld();
    if (!World) return;

    APlayerController* PC = World->GetFirstPlayerController();
    if (!PC || !PC->GetPawn()) return;
    FVector PlayerLoc = PC->GetPawn()->GetActorLocation();

    for (TWeakObjectPtr<AActor> Weak : TrackedActors)
    {
        if (!Weak.IsValid()) continue;
        AActor* Actor = Weak.Get();

        float Dist = FVector::Dist(PlayerLoc, Actor->GetActorLocation());

        // Determine actor size category by class
        bool bIsLarge = Actor->GetActorScale3D().GetMax() > 1000.f;
        bool bIsMedium = Actor->GetActorScale3D().GetMax() > 200.f;

        float MaxDist = Settings.MaxSmallObjectDistance;
        if (bIsLarge)       MaxDist = Settings.MaxLargeObjectDistance;
        else if (bIsMedium)  MaxDist = Settings.MaxMediumObjectDistance;

        // Set visibility based on distance
        bool bShouldBeVisible = Dist < MaxDist;
        if (Actor->IsActorTickEnabled() != bShouldBeVisible)
        {
            Actor->SetActorTickEnabled(bShouldBeVisible);
            Actor->SetActorHiddenInGame(!bShouldBeVisible);
        }
    }
}

void UMobileLODManager::UpdatePerformanceStats(float DeltaTime)
{
    // Estimate triangle count from tracked actors
    int32 TriCount = 0;
    int32 DrawCalls = 0;

    for (TWeakObjectPtr<AActor> Weak : TrackedActors)
    {
        if (!Weak.IsValid()) continue;
        AActor* Actor = Weak.Get();
        if (Actor->IsHidden()) continue;

        TArray<UStaticMeshComponent*> MeshComps;
        Actor->GetComponents<UStaticMeshComponent>(MeshComps);
        for (UStaticMeshComponent* SMC : MeshComps)
        {
            if (SMC && SMC->GetStaticMesh())
            {
                DrawCalls++;
                // Rough triangle estimate
                TriCount += 500; // Average
            }
        }
    }

    EstimatedTriangles = TriCount;
    DrawCallCount = DrawCalls;

    // Check warnings
    CheckPerformanceWarnings();
}

void UMobileLODManager::CheckPerformanceWarnings()
{
    WarningCooldown -= 1.f; // Updated per second
    if (WarningCooldown > 0.f) return;

    bool bWarning = false;
    if (EstimatedTriangles > 500000) bWarning = true;
    if (DrawCallCount > 2000) bWarning = true;

    if (bWarning)
    {
        WarningCooldown = 10.f;
        OnPerformanceWarning.Broadcast();

        // Auto-degrade
        if (CurrentStrategy == EMobileLODStrategy::Quality)
            SetStrategy(EMobileLODStrategy::Balanced);
        else if (CurrentStrategy == EMobileLODStrategy::Balanced)
            SetStrategy(EMobileLODStrategy::Aggressive);
    }
}

void UMobileLODManager::SetMaxDrawDistance(EFoliageType FoliageType, float Distance)
{
    // Override specific foliage type distance
    // (EFoliageType would be defined elsewhere)
    ApplySettings();
}
