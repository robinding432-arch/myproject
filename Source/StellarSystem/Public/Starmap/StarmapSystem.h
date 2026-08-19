// StarmapSystem.h
// 星图：扫描发现 + 锁定 + 过滤 + 描述生成 + 航线
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StarmapSystem.generated.h"

class AProceduralPlanet;
class AShipPawn;
class UWidgetComponent;

// 星图条目（一个可发现的目标）
USTRUCT(BlueprintType)
struct FStarmapEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName EntryID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector WorldPosition = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PlanetRadius = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor IconColor = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bDiscovered = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bLocked = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ThreatLevel = 0.f;   // 0~1

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText Description;            // AI 生成的描述

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FName> Tags;          // 标签（火山/冰冻/宜居/危险…）
};

// 扫描模式
UENUM(BlueprintType)
enum class EScanMode : uint8
{
    Passive,    // 被动（自动发现附近）
    Active,     // 主动（消耗能量，发现更远）
    Deep        // 深扫（很慢，完整信息）
};

// 星图组件（挂在玩家/飞船上）
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UStarmapComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UStarmapComponent();

    virtual void TickComponent(float Dt, ELevelTick Tick, FActorComponentTickFunction* Fn) override;
    virtual void BeginPlay() override;

    // —— 已知条目 ——
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated)
    TArray<FStarmapEntry> KnownEntries;

    // —— 扫描 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scan")
    float PassiveScanRange = 10000000.f;  // 1 00km

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scan")
    float ActiveScanRange = 50000000.f;   // 500km

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scan")
    float DeepScanRange = 100000000.f;    // 1000km

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scan")
    float ActiveScanEnergyCost = 5.f;     // 每秒消耗能量

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scan")
    float DeepScanTime = 10.f;           // 深扫耗时

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scan")
    float SensorEfficiency = 1.f;        // 传感器倍率

    // 当前扫描进度
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float CurrentDeepScanProgress = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    EScanMode CurrentScanMode = EScanMode::Passive;

    // —— API ——
    UFUNCTION(BlueprintCallable, Category = "Starmap")
    void StartActiveScan();

    UFUNCTION(BlueprintCallable, Category = "Starmap")
    void StopActiveScan();

    UFUNCTION(BlueprintCallable, Category = "Starmap")
    void StartDeepScan(FName TargetID);

    UFUNCTION(BlueprintCallable, Category = "Starmap")
    void CancelDeepScan();

    UFUNCTION(BlueprintCallable, Category = "Starmap")
    bool IsInRange(const FStarmapEntry& Entry) const;

    UFUNCTION(BlueprintCallable, Category = "Starmap")
    TArray<FStarmapEntry> GetEntriesInRange(float Range) const;

    UFUNCTION(BlueprintCallable, Category = "Starmap")
    TArray<FStarmapEntry> FilterByTag(FName Tag) const;

    UFUNCTION(BlueprintCallable, Category = "Starmap")
    TArray<FStarmapEntry> FilterByThreat(float MinThreat, float MaxThreat) const;

    // 锁定
    UFUNCTION(BlueprintCallable, Category = "Starmap")
    bool LockTarget(FName EntryID);

    UFUNCTION(BlueprintCallable, Category = "Starmap")
    void UnlockTarget(FName EntryID);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Starmap")
    FStarmapEntry GetLockedTarget() const;

    // 手动添加（NPC/任务给的情报）
    UFUNCTION(BlueprintCallable, Category = "Starmap")
    void AddKnownEntry(const FStarmapEntry& Entry);

    // 描述生成（AI 风格）
    UFUNCTION(BlueprintCallable, Category = "Starmap|AI")
    static FText GeneratePlanetDescription(int32 Seed, const FStarmapEntry& Entry);

    // 航线
    UFUNCTION(BlueprintCallable, Category = "Starmap")
    TArray<FVector> CalculateRoute(const TArray<FName>& Waypoints) const;

    // 事件
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEntryDiscovered, const FStarmapEntry&, Entry);
    UPROPERTY(BlueprintAssignable)
    FOnEntryDiscovered OnEntryDiscovered;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTargetLocked, FName, EntryID);
    UPROPERTY(BlueprintAssignable)
    FOnTargetLocked OnTargetLocked;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTargetUnlocked, FName, EntryID);
    UPROPERTY(BlueprintAssignable)
    FOnTargetUnlocked OnTargetUnlocked;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeepScanCompleted, const FStarmapEntry&, Entry);
    UPROPERTY(BlueprintAssignable)
    FOnDeepScanCompleted OnDeepScanCompleted;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

private:
    // 扫描逻辑
    void ProcessPassiveScan(float Dt);
    void ProcessActiveScan(float Dt);
    void ProcessDeepScan(float Dt);

    // 发现判定
    void TryDiscover(AProceduralPlanet* Planet);
    void DiscoverEntry(FStarmapEntry& Entry);

    // 描述生成
    static FString GeneratePlanetName(int32 Seed);
    static FString GenerateBiomeDescription(int32 Seed, const TArray<FName>& Tags);
    static FString GenerateThreatDescription(int32 Seed, float Threat);

    // 当前锁定
    FName LockedTargetID;

    // 深扫目标
    FName DeepScanTargetID;
    float DeepScanTimer = 0.f;

    // 已知星球缓存
    UPROPERTY()
    TArray<AProceduralPlanet*> CachedPlanets;
};
