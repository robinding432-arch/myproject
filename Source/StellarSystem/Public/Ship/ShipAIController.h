// ShipAIController.h
// 飞船 AI：自主选星 → 跃迁 → 停留 → 循环
#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "ShipAIController.generated.h"

class AShipPawn;
class AProceduralPlanet;

UCLASS()
class AShipAIController : public AAIController
{
    GENERATED_BODY()

public:
    virtual void Tick(float DeltaTime) override;
    virtual void BeginPlay() override;

    // —— 配置 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    float ExplorationRadius = 50000000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    float MinTimeAtPlanet = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    float MaxTimeAtPlanet = 60.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    bool bWarpAtGameStart = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    float WarpRetryDelay = 5.f;

    // —— 状态 ——
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    bool bTraveling = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    AProceduralPlanet* CurrentPlanet = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    AProceduralPlanet* TargetPlanet = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float TimeAtCurrentPlanet = 0.f;

    // —— 行为 API ——
    UFUNCTION(BlueprintCallable, Category = "AI")
    void PickNextPlanet();

    UFUNCTION(BlueprintCallable, Category = "AI")
    void ForceWarpTo(AProceduralPlanet* Planet);

    UFUNCTION(BlueprintCallable, Category = "AI")
    void StopAI();

    UFUNCTION(BlueprintCallable, Category = "AI")
    void ResumeAI();

    // —— 策略权重（越高越优先）——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Strategy")
    float DistanceWeight = 1.f;     // 距离偏好

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Strategy")
    float UnvisitedWeight = 3.f;    // 未访问星球偏好

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Strategy")
    float RandomWeight = 0.5f;     // 随机探索

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Strategy")
    float MinDistanceBetweenVisits = 1000000.f; // 避免反复去同一颗

    // 已访问记录
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TArray<FName> VisitedPlanets;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 TotalWarpsCompleted = 0;

    // 事件
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAITargetPicked, AProceduralPlanet*, Planet);
    UPROPERTY(BlueprintAssignable)
    FOnAITargetPicked OnAITargetPicked;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAIWarped, AProceduralPlanet*, ArrivedAt);
    UPROPERTY(BlueprintAssignable)
    FOnAIWarped OnAIWarped;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAIStopped);
    UPROPERTY(BlueprintAssignable)
    FOnAIStopped OnAIStopped;

private:
    float RetryTimer = 0.f;

    AProceduralPlanet* FindRandomPlanetInRange() const;
    AProceduralPlanet* FindNearestUnexploredPlanet() const;
    AProceduralPlanet* ScoreAndPickPlanet() const;
    void EvaluateArrival();
};
