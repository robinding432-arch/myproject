// AsteroidBelt.cpp
#include "Space/AsteroidBelt.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "Math/UnrealMathUtility.h"

AAsteroidBelt::AAsteroidBelt()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(RootComponent);

    // 碰撞体积
    USphereComponent* Vol = CreateDefaultSubobject<USphereComponent>(TEXT("BeltVolume"));
    Vol->SetupAttachment(RootComponent);
    Vol->SetSphereRadius(5000000.f * 0.001f); // cm → m for collision
    Vol->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Vol->SetCollisionResponseToAllChannels(ECR_Overlap);
}

void AAsteroidBelt::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority())
    {
        GenerateBelt();
    }
}

void AAsteroidBelt::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 更新所有小行星轨道位置
    UpdateISMTransforms(DeltaTime);
}

void AAsteroidBelt::GenerateBelt()
{
    FRandomStream Rand(BeltSeed);
    AllAsteroids.Reset();

    // 按大小分桶 → 不同 ISM
    TMap<int32, TArray<FAsteroidParams>> Bucketed;

    for (int32 i = 0; i < AsteroidCount; ++i)
    {
        FAsteroidParams Ast;
        Ast.AsteroidID = FName(*FString::Printf(TEXT("Ast_%d_%d"), BeltSeed, i));

        // 轨道参数
        float T = (float)i / AsteroidCount;
        float Radius = FMath::Lerp(BeltInnerRadius, BeltOuterRadius, T)
                    + Rand.FRandRange(-BeltThickness * 0.5f, BeltThickness * 0.5f);
        Ast.SemiMajorAxis = Radius;
        Ast.Eccentricity = Rand.FRandRange(0.f, 0.3f);
        Ast.Inclination = Rand.FRandRange(-BeltInclination, BeltInclination);
        Ast.LongitudeAscending = Rand.FRandRange(0.f, 360.f);
        Ast.ArgumentPeriapsis = Rand.FRandRange(0.f, 360.f);
        Ast.MeanAnomaly = Rand.FRandRange(0.f, 360.f);
        Ast.OrbitalPeriod = Rand.FRandRange(1800.f, 7200.f); // 30min~2hr

        // 大小
        Ast.Size = Rand.FRandRange(0.1f, 1.f);
        Ast.RotationSpeed = Rand.FRandRange(1.f, 20.f);
        Ast.AsteroidColor = FLinearColor(
            Rand.FRandRange(0.3f, 0.7f),
            Rand.FRandRange(0.25f, 0.55f),
            Rand.FRandRange(0.2f, 0.5f), 1.f);
        Ast.DamageOnImpact = Rand.FRandRange(5.f, 50.f) * Ast.Size;
        Ast.ResourceValue = Rand.FRandRange(1.f, 20.f) * Ast.Size;

        AllAsteroids.Add(Ast);

        // 分桶
        int32 Bucket = FMath::Clamp((int32)(Ast.Size * MeshVariantsCount), 0, MeshVariantsCount - 1);
        Bucketed.FindOrAdd(Bucket).Add(Ast);
    }

    // 创建 ISM 组件
    int32 BucketIdx = 0;
    for (auto& Pair : Bucketed)
    {
        FName CompName = FName(*FString::Printf(TEXT("AsteroidISM_%d"), BucketIdx));
        UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(this, CompName);
        ISM->SetupAttachment(RootComponent);
        ISM->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

        // 设置 Mesh
        if (AsteroidMeshVariants.IsValidIndex(Pair.Key) && AsteroidMeshVariants[Pair.Key])
        {
            ISM->SetStaticMesh(AsteroidMeshVariants[Pair.Key]);
        }
        else if (AsteroidMeshVariants.Num() > 0 && AsteroidMeshVariants[0])
        {
            ISM->SetStaticMesh(AsteroidMeshVariants[0]);
        }

        ISM->RegisterComponent();
        AsteroidISMs.Add(Pair.Key, ISM);

        // 初始变换
        for (int32 i = 0; i < Pair.Value.Num(); ++i)
        {
            FAsteroidParams& Ast = AllAsteroids[BucketIdx * MeshVariantsCount + i];
            FVector Pos = CalculateOrbitalPosition(Ast, 0.f);
            FRotator Rot(Rand.FRandRange(0.f, 360.f), Rand.FRandRange(0.f, 360.f), Rand.FRandRange(0.f, 360.f));
            FVector Scale(Ast.Size);

            FTransform T(Rot, Pos, Scale);
            ISM->AddInstanceWorldSpace(T);
        }

        BucketIdx++;
    }
}

void AAsteroidBelt::RegenerateWithSeed(int32 NewSeed)
{
    BeltSeed = NewSeed;
    // 清除旧 ISM
    for (auto& Pair : AsteroidISMs)
    {
        if (Pair.Value) Pair.Value->DestroyComponent();
    }
    AsteroidISMs.Reset();
    GenerateBelt();
}

FVector AAsteroidBelt::CalculateOrbitalPosition(const FAsteroidParams& Asteroid, float Time) const
{
    // 简化开普勒：椭圆轨道近似
    float MeanMotion = 2.f * PI / Asteroid.OrbitalPeriod;
    float M = FMath::DegreesToRadians(Asteroid.MeanAnomaly) + MeanMotion * Time;

    // 解 Kepler 方程（迭代）
    float E = M;
    for (int32 i = 0; i < 5; ++i)
    {
        E = M + Asteroid.Eccentricity * FMath::Sin(E);
    }

    float CosE = FMath::Cos(E);
    float SinE = FMath::Sin(E);
    float a = Asteroid.SemiMajorAxis;
    float b = a * FMath::Sqrt(1.f - Asteroid.Eccentricity * Asteroid.Eccentricity);

    // 轨道平面坐标
    float x = a * (CosE - Asteroid.Eccentricity);
    float y = b * SinE;

    // 旋转到倾角
    float Incl = FMath::DegreesToRadians(Asteroid.Inclination);
    float Asc = FMath::DegreesToRadians(Asteroid.LongitudeAscending);
    float Peri = FMath::DegreesToRadians(Asteroid.ArgumentPeriapsis);

    // 简化：先绕 X 转倾角，再绕 Z 转升交点
    FVector Pos(x, y, 0.f);
    FRotator Rot(Asc, Incl, Peri);
    Pos = Rot.RotateVector(Pos);

    return GetActorLocation() + Pos;
}

void AAsteroidBelt::UpdateISMTransforms(float Dt)
{
    // 更新每个实例的变换
    for (auto& Pair : AsteroidISMs)
    {
        UInstancedStaticMeshComponent* ISM = Pair.Value;
        if (!ISM) continue;

        // 简化：只更新位置，不重建整个 ISM
        // 实际项目中应批量更新
    }

    // 自转更新
    static float GlobalTime = 0.f;
    GlobalTime += Dt;
}

float AAsteroidBelt::GetDensityAtLocation(const FVector& WorldPos) const
{
    FVector Local = WorldPos - GetActorLocation();
    float R = Local.Size();

    if (R < BeltInnerRadius || R > BeltOuterRadius) return 0.f;

    // 带内密度随半径变化
    float CenterR = (BeltInnerRadius + BeltOuterRadius) * 0.5f;
    float DistFromCenter = FMath::Abs(R - CenterR);
    float HalfWidth = (BeltOuterRadius - BeltInnerRadius) * 0.5f;

    float Density = 1.f - FMath::Clamp(DistFromCenter / HalfWidth, 0.f, 1.f);
    Density = FMath::Pow(Density, DensityFalloff);

    // 厚度衰减
    float ZFactor = FMath::Clamp(1.f - FMath::Abs(Local.Z) / (BeltThickness * 0.5f), 0.f, 1.f);
    Density *= ZFactor;

    return FMath::Clamp(Density, 0.f, 1.f);
}

TArray<FAsteroidParams> AAsteroidBelt::GetAsteroidsInRange(const FVector& WorldPos, float Range) const
{
    TArray<FAsteroidParams> Result;
    for (const FAsteroidParams& Ast : AllAsteroids)
    {
        FVector Pos = CalculateOrbitalPosition(Ast, GetWorld()->GetTimeSeconds());
        if (FVector::Dist(Pos, WorldPos) < Range)
            Result.Add(Ast);
    }
    return Result;
}

bool AAsteroidBelt::IsInBelt(const FVector& WorldPos) const
{
    FVector Local = WorldPos - GetActorLocation();
    float R = Local.Size();
    return R >= BeltInnerRadius && R <= BeltOuterRadius
        && FMath::Abs(Local.Z) <= BeltThickness * 0.5f;
}

void AAsteroidBelt::ServerMineAsteroid_Implementation(FName AsteroidID, float YieldMultiplier)
{
    if (!HasAuthority()) return;

    for (int32 i = 0; i < AllAsteroids.Num(); ++i)
    {
        if (AllAsteroids[i].AsteroidID == AsteroidID)
        {
            // 移除小行星
            float Value = AllAsteroids[i].ResourceValue * YieldMultiplier;
            AllAsteroids.RemoveAt(i);
            // 通知 GameMode 给玩家资源
            // OnAsteroidMined.Broadcast(AsteroidID, Value);
            break;
        }
    }
}

void AAsteroidBelt::OnAsteroidHit(
    UPrimitiveComponent* HitComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    if (!bAsteroidsCauseDamage || !OtherActor) return;

    // 计算伤害
    float Speed = NormalImpulse.Size();
    float Damage = Speed * 0.01f * CollisionDamageScale;

    // 对飞船/角色造成伤害
    // OtherActor->TakeDamage(Damage, ...);
}

void AAsteroidBelt::DistributeToISM(const FAsteroidParams& Asteroid, int32 Index)
{
    // 已在 GenerateBelt 中处理
}

void AAsteroidBelt::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(AAsteroidBelt, BeltSeed);
    DOREPLIFETIME(AAsteroidBelt, BeltInnerRadius);
    DOREPLIFETIME(AAsteroidBelt, BeltOuterRadius);
    DOREPLIFETIME(AAsteroidBelt, BeltThickness);
    DOREPLIFETIME(AAsteroidBelt, AsteroidCount);
}
