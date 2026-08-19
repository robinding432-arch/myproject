#include "Economy/MiningSystem.h"
#include "Planet/ProceduralPlanet.h"
#include "GameFramework/Actor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ParticleSystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Math/UnrealMathUtility.h"

// ========== UMiningLaserComponent ==========

UMiningLaserComponent::UMiningLaserComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UMiningLaserComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bIsMining || !CurrentTarget) return;

    MiningTimer += DeltaTime;
    if (MiningTimer >= ExtractInterval)
    {
        MiningTimer = 0.f;

        // 检查距离
        float Dist = FVector::Dist(GetOwner()->GetActorLocation(), CurrentTarget->GetActorLocation());
        if (Dist > MiningRange)
        {
            StopMining();
            return;
        }

        // 通知服务端扣矿
        APlanetMiningManager* PlanetMM = nullptr; // 由外部设置或查找
        // 简化：直接通过 Target 的 Owner 获取
        if (AActor* Target = CurrentTarget)
        {
            // 从 Target 上找 MiningManager 引用（需要在生成时设置）
            // 这里用接口或 Tag 查找
        }

        // 本地先播放效果
        // 实际扣矿在 Server_ExtractOre 里做
    }
}

void UMiningLaserComponent::StartMining(AActor* TargetVein)
{
    if (!TargetVein) return;

    CurrentTarget = TargetVein;
    bIsMining = true;
    MiningTimer = 0.f;

    // 播放采矿光束粒子效果（接口，美术可覆盖）
    // OnMiningStarted.Broadcast();
}

void UMiningLaserComponent::StopMining()
{
    bIsMining = false;
    CurrentTarget = nullptr;
    MiningTimer = 0.f;

    // 结算本次会话
    for (auto& Pair : SessionYield)
    {
        // 交给背包系统处理
        // OnSessionComplete.Broadcast(Pair.Key, Pair.Value);
    }
    SessionYield.Empty();
}

// ========== APlanetMiningManager ==========

APlanetMiningManager::APlanetMiningManager()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
}

void APlanetMiningManager::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        // 查找所属行星
        TArray<AActor*> Planets;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), AProceduralPlanet::StaticClass(), Planets);
        for (AActor* P : Planets)
        {
            // 假设行星在生成时设置 Owner
            if (FVector::Dist(P->GetActorLocation(), GetActorLocation()) < 1000.f)
            {
                OwnerPlanet = Cast<AProceduralPlanet>(P);
                break;
            }
        }

        // 自动生成矿脉
        FRandomStream RandStream(FMath::Rand());
        GenerateOreVeins(RandStream.GetCurrentSeed(), 200);
    }
}

void APlanetMiningManager::GenerateOreVeins(int32 Seed, int32 VeinCount)
{
    if (!OwnerPlanet) return;

    FRandomStream Rand(Seed);
    OreVeins.Empty();
    OreVeins.Reserve(VeinCount);

    FVector PlanetCenter = OwnerPlanet->GetActorLocation();
    float PlanetRadius = OwnerPlanet->GetPlanetRadius(); // 需要暴露

    for (int32 i = 0; i < VeinCount; ++i)
    {
        // 均匀球面随机点
        float Theta = Rand.FRandRange(0.f, 2.f * PI);
        float Phi = FMath::Acos(Rand.FRandRange(-1.f, 1.f));
        FVector Dir(
            FMath::Sin(Phi) * FMath::Cos(Theta),
            FMath::Sin(Phi) * FMath::Sin(Theta),
            FMath::Cos(Phi)
        );

        // 获取该点 Biome
        FName Biome = OwnerPlanet->GetBiomeAtWorldPos(PlanetCenter + Dir * PlanetRadius);

        // 按 Biome 加权选矿石类型
        EOreType OreType = PickOreTypeForBiome(Biome, Rand);

        // 品质
        float Quality = Rand.FRandRange(QualityRange.X, QualityRange.Y);

        // 储量
        float Total = Rand.FRandRange(50.f, 300.f) * Quality;

        // 缩放
        float Scale = Rand.FRandRange(0.5f, 2.5f);

        FOreVein Vein;
        Vein.WorldLocation = PlanetCenter + Dir * (PlanetRadius + 100.f);
        Vein.OreType = OreType;
        Vein.RemainingAmount = Total;
        Vein.TotalAmount = Total;
        Vein.Quality = Quality;
        Vein.bDepleted = false;
        Vein.Scale = Scale;
        Vein.Rotation = FRotator(
            Rand.FRandRange(0.f, 360.f),
            Rand.FRandRange(0.f, 360.f),
            Rand.FRandRange(0.f, 360.f)
        );

        OreVeins.Add(Vein);

        // 生成 Mesh Actor
        SpawnVeinMesh(Vein, OreVeins.Num() - 1);
    }
}

EOreType APlanetMiningManager::PickOreTypeForBiome(FName Biome, FRandomStream& Rand) const
{
    // Biome → 矿石权重表
    // 简化版：用 OreDatabase 的 PreferredBiomes 匹配
    TArray<TPair<EOreType, float>> WeightedOptions;

    for (const FOreData& Ore : OreDatabase)
    {
        float Weight = 1.f; // 默认权重

        // 检查 Biome 偏好
        for (const FName& Preferred : Ore.PreferredBiomes)
        {
            if (Preferred == Biome)
            {
                Weight *= (2.f / FMath::Max(Ore.Rarity, 0.01f));
                break;
            }
        }

        WeightedOptions.Add(TPair<EOreType, float>(Ore.OreType, Weight));
    }

    // 加权随机
    float TotalWeight = 0.f;
    for (const auto& Pair : WeightedOptions) TotalWeight += Pair.Value;

    float Roll = Rand.FRandRange(0.f, TotalWeight);
    float Accum = 0.f;
    for (const auto& Pair : WeightedOptions)
    {
        Accum += Pair.Value;
        if (Roll <= Accum) return Pair.Key;
    }

    return EOreType::Iron; // fallback
}

void APlanetMiningManager::SpawnVeinMesh(const FOreVein& Vein, int32 Index)
{
    // 程序化生成一个简单 Mesh 代表矿脉
    // 实际项目中这里应该 SpawnActor + ProceduralMeshComponent
    // 简化为在 World 中 Spawn 一个 StaticMeshActor

    UWorld* World = GetWorld();
    if (!World) return;

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // 用一个基础球体做占位
    AActor* VeinActor = World->SpawnActor<AActor>(
        AActor::StaticClass(), Vein.WorldLocation, Vein.Rotation, Params);

    if (VeinActor)
    {
        // 添加标识
        VeinActor->Tags.Add(FName(*FString::Printf(TEXT("OreVein_%d"), Index)));
        VeinActor->Tags.Add(FName(*UEnum::GetValueAsString(Vein.OreType)));

        RegisterVeinActor(VeinActor, Index);
    }
}

TArray<FOreVein> APlanetMiningManager::GetVeinsByType(EOreType OreType) const
{
    TArray<FOreVein> Result;
    for (const FOreVein& V : OreVeins)
    {
        if (V.OreType == OreType && !V.bDepleted)
            Result.Add(V);
    }
    return Result;
}

void APlanetMiningManager::RegisterVeinActor(AActor* VeinActor, int32 VeinIndex)
{
    if (VeinActor && VeinIndex >= 0 && VeinIndex < OreVeins.Num())
    {
        VeinActorToIndex.Add(VeinActor, VeinIndex);
    }
}

void APlanetMiningManager::Server_ExtractOre_Implementation(int32 VeinIndex, float Amount, AActor* Extractor)
{
    if (!HasAuthority()) return;
    if (VeinIndex < 0 || VeinIndex >= OreVeins.Num()) return;

    FOreVein& Vein = OreVeins[VeinIndex];
    if (Vein.bDepleted) return;

    float ActualAmount = FMath::Min(Amount, Vein.RemainingAmount);
    Vein.RemainingAmount -= ActualAmount;

    if (Vein.RemainingAmount <= 0.f)
    {
        Vein.bDepleted = true;
        Vein.RemainingAmount = 0.f;

        // 通知所有客户端矿脉耗尽
        // Multicast_OnVeinDepleted(VeinIndex);
    }

    // 给采集者加矿石（通过背包系统接口）
    // IInventoryInterface::AddOre(Extractor, Vein.OreType, ActualAmount * Vein.Quality);
}
