// ShipAIController.cpp
#include "Ship/ShipAIController.h"
#include "Ship/ShipPawn.h"
#include "Planet/ProceduralPlanet.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

void AShipAIController::BeginPlay()
{
    Super::BeginPlay();

    if (bWarpAtGameStart)
    {
        AITimeAtCurrent = 0.f;
        bAITraveling = false;
    }
}

void AShipAIController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    AShipPawn* Ship = Cast<AShipPawn>(GetPawn());
    if (!Ship || Ship->bInWarp) return;

    if (!bAITraveling)
    {
        AITimeAtCurrent += DeltaTime;

        if (AITimeAtCurrent > AIMaxStayTime)
        {
            PickNextPlanet();
        }
        else if (AITimeAtCurrent > AIMinStayTime && FMath::FRand() < DeltaTime * 0.1f)
        {
            // 随机提前离开
            PickNextPlanet();
        }
    }
}

void AShipAIController::PickNextPlanet()
{
    AShipPawn* Ship = Cast<AShipPawn>(GetPawn());
    if (!Ship) return;

    AProceduralPlanet* Next = ScoreAndPickPlanet();
    if (Next && Next != CurrentPlanet)
    {
        TargetPlanet = Next;
        bAITraveling = true;
        AITimeAtCurrent = 0.f;

        Ship->ServerWarpToPlanet(Next);
        OnAITargetPicked.Broadcast(Next);
    }
}

void AShipAIController::ForceWarpTo(AProceduralPlanet* Planet)
{
    if (!Planet) return;

    AShipPawn* Ship = Cast<AShipPawn>(GetPawn());
    if (!Ship) return;

    TargetPlanet = Planet;
    bAITraveling = true;
    AITimeAtCurrent = 0.f;

    Ship->ServerWarpToPlanet(Planet);
    OnAITargetPicked.Broadcast(Planet);
}

void AShipAIController::StopAI()
{
    bAITraveling = false;
    TargetPlanet = nullptr;
    OnAIStopped.Broadcast();
}

void AShipAIController::ResumeAI()
{
    AITimeAtCurrent = 0.f;
    bAITraveling = false;
}

AProceduralPlanet* AShipAIController::ScoreAndPickPlanet() const
{
    TArray<AActor*> AllPlanets;
    UGameplayStatics::GetAllActorsOfClass(
        GetWorld(), AProceduralPlanet::StaticClass(), AllPlanets);

    if (AllPlanets.Num() == 0) return nullptr;

    AShipPawn* Ship = Cast<AShipPawn>(GetPawn());
    if (!Ship) return nullptr;

    FVector ShipPos = Ship->GetActorLocation();

    float BestScore = -1.f;
    AProceduralPlanet* Best = nullptr;

    for (AActor* Act : AllPlanets)
    {
        AProceduralPlanet* Planet = Cast<AProceduralPlanet>(Act);
        if (!Planet || Planet == CurrentPlanet) continue;

        float Dist = FVector::Dist(ShipPos, Planet->GetActorLocation());

        if (Dist > AIExplorationRadius) continue;

        float Score = 0.f;

        // 距离权重（偏好中等距离）
        float DistNorm = Dist / AIExplorationRadius;
        Score += (1.f - DistNorm) * DistanceWeight;

        // 未访问偏好
        if (!VisitedPlanets.Contains(Planet->GetFName()))
            Score += UnvisitedWeight;

        // 随机探索
        Score += FMath::FRand() * RandomWeight;

        // 避免反复去同一颗
        if (TargetPlanet == Planet)
            Score -= 10.f;

        if (Score > BestScore)
        {
            BestScore = Score;
            Best = Planet;
        }
    }

    return Best;
}

AProceduralPlanet* AShipAIController::FindRandomPlanetInRange() const
{
    TArray<AActor*> AllPlanets;
    UGameplayStatics::GetAllActorsOfClass(
        GetWorld(), AProceduralPlanet::StaticClass(), AllPlanets);

    TArray<AProceduralPlanet*> InRange;
    AShipPawn* Ship = Cast<AShipPawn>(GetPawn());
    if (!Ship) return nullptr;

    for (AActor* Act : AllPlanets)
    {
        AProceduralPlanet* P = Cast<AProceduralPlanet>(Act);
        if (P && FVector::Dist(Ship->GetActorLocation(), P->GetActorLocation()) < AIExplorationRadius)
            InRange.Add(P);
    }

    if (InRange.Num() == 0) return nullptr;
    return InRange[FMath::RandRange(0, InRange.Num() - 1)];
}

AProceduralPlanet* AShipAIController::FindNearestUnexploredPlanet() const
{
    TArray<AActor*> AllPlanets;
    UGameplayStatics::GetAllActorsOfClass(
        GetWorld(), AProceduralPlanet::StaticClass(), AllPlanets);

    AShipPawn* Ship = Cast<AShipPawn>(GetPawn());
    if (!Ship) return nullptr;

    FVector ShipPos = Ship->GetActorLocation();
    float BestDist = MAX_FLT;
    AProceduralPlanet* Best = nullptr;

    for (AActor* Act : AllPlanets)
    {
        AProceduralPlanet* P = Cast<AProceduralPlanet>(Act);
        if (!P || VisitedPlanets.Contains(P->GetFName())) continue;

        float D = FVector::Dist(ShipPos, P->GetActorLocation());
        if (D < BestDist)
        {
            BestDist = D;
            Best = P;
        }
    }
    return Best;
}

void AShipAIController::EvaluateArrival()
{
    bAITraveling = false;
    CurrentPlanet = TargetPlanet;
    if (TargetPlanet)
        VisitedPlanets.AddUnique(TargetPlanet->GetFName());
    TotalWarpsCompleted++;
    OnAIWarped.Broadcast(TargetPlanet);
    RetryTimer = 0.f;
}
