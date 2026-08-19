// StarmapSystem.cpp
#include "Starmap/StarmapSystem.h"
#include "Planet/ProceduralPlanet.h"
#include "Ship/ShipPawn.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Math/UnrealMathUtility.h"

UStarmapComponent::UStarmapComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UStarmapComponent::BeginPlay()
{
    Super::BeginPlay();
    CurrentScanMode = EScanMode::Passive;
}

void UStarmapComponent::TickComponent(float Dt, ELevelTick Tick, FActorComponentTickFunction* Fn)
{
    Super::TickComponent(Dt, Tick, Fn);

    switch (CurrentScanMode)
    {
    case EScanMode::Passive:
        ProcessPassiveScan(Dt);
        break;
    case EScanMode::Active:
        ProcessActiveScan(Dt);
        break;
    case EScanMode::Deep:
        ProcessDeepScan(Dt);
        break;
    }
}

void UStarmapComponent::StartActiveScan()
{
    CurrentScanMode = EScanMode::Active;
}

void UStarmapComponent::StopActiveScan()
{
    CurrentScanMode = EScanMode::Passive;
}

void UStarmapComponent::StartDeepScan(FName TargetID)
{
    DeepScanTargetID = TargetID;
    DeepScanTimer = 0.f;
    CurrentScanMode = EScanMode::Deep;
}

void UStarmapComponent::CancelDeepScan()
{
    DeepScanTargetID = NAME_None;
    DeepScanTimer = 0.f;
    CurrentScanMode = EScanMode::Passive;
}

bool UStarmapComponent::IsInRange(const FStarmapEntry& Entry) const
{
    if (AActor* Owner = GetOwner())
    {
        float Dist = FVector::Dist(Owner->GetActorLocation(), Entry.WorldPosition);
        return Dist < PassiveScanRange;
    }
    return false;
}

TArray<FStarmapEntry> UStarmapComponent::GetEntriesInRange(float Range) const
{
    TArray<FStarmapEntry> Result;
    for (const FStarmapEntry& E : KnownEntries)
    {
        if (AActor* Owner = GetOwner())
        {
            float Dist = FVector::Dist(Owner->GetActorLocation(), E.WorldPosition);
            if (Dist < Range) Result.Add(E);
        }
    }
    return Result;
}

TArray<FStarmapEntry> UStarmapComponent::FilterByTag(FName Tag) const
{
    TArray<FStarmapEntry> Result;
    for (const FStarmapEntry& E : KnownEntries)
    {
        if (E.Tags.Contains(Tag)) Result.Add(E);
    }
    return Result;
}

TArray<FStarmapEntry> UStarmapComponent::FilterByThreat(float MinT, float MaxT) const
{
    TArray<FStarmapEntry> Result;
    for (const FStarmapEntry& E : KnownEntries)
    {
        if (E.ThreatLevel >= MinT && E.ThreatLevel <= MaxT) Result.Add(E);
    }
    return Result;
}

bool UStarmapComponent::LockTarget(FName EntryID)
{
    for (FStarmapEntry& E : KnownEntries)
    {
        if (E.EntryID == EntryID)
        {
            E.bLocked = true;
            LockedTargetID = EntryID;
            OnTargetLocked.Broadcast(EntryID);
            return true;
        }
    }
    return false;
}

void UStarmapComponent::UnlockTarget(FName EntryID)
{
    for (FStarmapEntry& E : KnownEntries)
    {
        if (E.EntryID == EntryID)
        {
            E.bLocked = false;
            if (LockedTargetID == EntryID) LockedTargetID = NAME_None;
            OnTargetUnlocked.Broadcast(EntryID);
            return;
        }
    }
}

FStarmapEntry UStarmapComponent::GetLockedTarget() const
{
    for (const FStarmapEntry& E : KnownEntries)
        if (E.EntryID == LockedTargetID) return E;
    return FStarmapEntry();
}

void UStarmapComponent::AddKnownEntry(const FStarmapEntry& Entry)
{
    KnownEntries.Add(Entry);
    OnEntryDiscovered.Broadcast(Entry);
}

void UStarmapComponent::ProcessPassiveScan(float Dt)
{
    if (!GetWorld()) return;

    TArray<AActor*> Planets;
    UGameplayStatics::GetAllActorsOfClass(
        GetWorld(), AProceduralPlanet::StaticClass(), Planets);

    AActor* Owner = GetOwner();
    if (!Owner) return;

    for (AActor* Act : Planets)
    {
        AProceduralPlanet* Planet = Cast<AProceduralPlanet>(Act);
        if (!Planet) continue;

        float Dist = FVector::Dist(Owner->GetActorLocation(), Planet->GetActorLocation());
        if (Dist < PassiveScanRange)
        {
            FName PID = Planet->GetFName();
            bool bAlreadyKnown = false;
            for (const FStarmapEntry& E : KnownEntries)
                if (E.EntryID == PID) { bAlreadyKnown = true; break; }

            if (!bAlreadyKnown)
            {
                FStarmapEntry NewEntry;
                NewEntry.EntryID = PID;
                NewEntry.DisplayName = FText::FromName(PID);
                NewEntry.WorldPosition = Planet->GetActorLocation();
                NewEntry.PlanetRadius = Planet->PlanetRadius;
                NewEntry.bDiscovered = true;
                NewEntry.Description = GeneratePlanetDescription(Planet->RandomSeed, NewEntry);
                DiscoverEntry(NewEntry);
            }
        }
    }
}

void UStarmapComponent::ProcessActiveScan(float Dt)
{
    // 消耗能量，扩大范围
    ProcessPassiveScan(Dt);
    // ActiveScan 额外扫描更远距离
}

void UStarmapComponent::ProcessDeepScan(float Dt)
{
    DeepScanTimer += Dt;
    CurrentDeepScanProgress = DeepScanTimer / 10.f; // 10秒深扫

    if (DeepScanTimer >= 10.f)
    {
        // 完成深扫，解锁完整信息
        for (FStarmapEntry& E : KnownEntries)
        {
            if (E.EntryID == DeepScanTargetID)
            {
                E.bDiscovered = true;
                OnDeepScanCompleted.Broadcast(E);
                break;
            }
        }
        CancelDeepScan();
    }
}

void UStarmapComponent::TryDiscover(AProceduralPlanet* Planet)
{
    if (!Planet) return;
    FStarmapEntry E;
    E.EntryID = Planet->GetFName();
    E.DisplayName = FText::FromName(E.EntryID);
    E.WorldPosition = Planet->GetActorLocation();
    E.PlanetRadius = Planet->PlanetRadius;
    DiscoverEntry(E);
}

void UStarmapComponent::DiscoverEntry(FStarmapEntry& Entry)
{
    Entry.bDiscovered = true;
    KnownEntries.Add(Entry);
    OnEntryDiscovered.Broadcast(Entry);
}

FText UStarmapComponent::GeneratePlanetDescription(int32 Seed, const FStarmapEntry& Entry)
{
    FString Desc;
    FRandomStream Rand(Seed);

    // 随机描述片段
    TArray<FString> Prefixes = {
        TEXT("一颗表面覆盖着"), TEXT("这颗星球以"), TEXT("探测器传回数据显示"),
        TEXT("初步扫描表明"), TEXT("轨道观测发现")
    };
    TArray<FString> Middles = {
        TEXT("广阔的沙漠和稀疏的绿洲"), TEXT("密集的丛林和活跃的火山"),
        TEXT("冰冻的平原和深不见底的峡谷"), TEXT("碧蓝的海洋和翠绿的岛屿"),
        TEXT("崎岖的岩石和古老的陨石坑"), TEXT("发光的晶体和奇异的构造")
    };
    TArray<FString> Suffixes = {
        TEXT("。存在微弱的生命信号。"),
        TEXT("。大气成分适宜人类呼吸。"),
        TEXT("。辐射水平异常偏高。"),
        TEXT("。表面温度极端。"),
        TEXT("。未发现明显威胁。")
    };

    Desc = Prefixes[Rand.RandRange(0, Prefixes.Num()-1)] + TEXT(" ") +
           Middles[Rand.RandRange(0, Middles.Num()-1)] + TEXT(" ") +
           Suffixes[Rand.RandRange(0, Suffixes.Num()-1)];

    return FText::FromString(Desc);
}

FString UStarmapComponent::GeneratePlanetName(int32 Seed)
{
    FRandomStream Rand(Seed);
    const TArray<FString> Prefixes = {TEXT("Kepler"), TEXT("Trappist"), TEXT("Gliese"), TEXT("HD"), TEXT("Proxima"), TEXT("Wolf")};
    const TArray<FString> Suffixes = {TEXT("b"), TEXT("c"), TEXT("d"), TEXT("e"), TEXT("f"), TEXT("g")};
    return Prefixes[Rand.RandRange(0, Prefixes.Num()-1)] + TEXT("-") +
           FString::FromInt(Rand.RandRange(100, 9999)) + TEXT(" ") +
           Suffixes[Rand.RandRange(0, Suffixes.Num()-1)];
}

FString UStarmapComponent::GenerateBiomeDescription(int32 Seed, const TArray<FName>& Tags)
{
    return TEXT("表面特征待进一步扫描确认。");
}

FString UStarmapComponent::GenerateThreatDescription(int32 Seed, float Threat)
{
    if (Threat > 0.7f) return TEXT("高危区域，建议保持距离。");
    if (Threat > 0.4f) return TEXT("中等威胁，建议谨慎接近。");
    return TEXT("低威胁区域。");
}

TArray<FVector> UStarmapComponent::CalculateRoute(const TArray<FName>& Waypoints) const
{
    TArray<FVector> Route;
    // 简化：直线连接各点
    for (const FStarmapEntry& E : KnownEntries)
    {
        if (Waypoints.Contains(E.EntryID))
            Route.Add(E.WorldPosition);
    }
    return Route;
}
