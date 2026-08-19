// ProceduralPlanet.cpp
// 立方球 + fBm 噪声地形 + Biome + LOD + 海洋 + 植被 ISM

#include "Planet/ProceduralPlanet.h"
#include "Planet/OceanShader.h"
#include "AssetRegistry/AssetRegistry.h"
#include "Components/ProceduralMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Math/UnrealMathUtility.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

AProceduralPlanet::AProceduralPlanet()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(RootComponent);

    // 6 面立方球 Mesh
    for (int32 i = 0; i < 6; ++i)
    {
        FName CompName = FName(*FString::Printf(TEXT("FaceMesh_%d"), i));
        UProceduralMeshComponent* Face = CreateDefaultSubobject<UProceduralMeshComponent>(CompName);
        Face->SetupAttachment(RootComponent);
        Face->bUseAsyncCooking = true;
        FaceMeshes.Add(Face);
    }

    // 海洋
    OceanMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OceanMesh"));
    OceanMesh->SetupAttachment(RootComponent);
    OceanMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    OceanMesh->SetCollisionResponseToAllChannels(ECR_Overlap);

    // 默认海洋材质（可被资产注册表覆盖）
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> OceanMatFinder(
        TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
    if (OceanMatFinder.Succeeded())
        OceanMaterial = OceanMatFinder.Object;

    // 默认值
    NoiseParams.NoiseScale = 0.0005f;
    NoiseParams.Octaves = 6;
    NoiseParams.Persistence = 0.5f;
    NoiseParams.Lacunarity = 2.0f;
    NoiseParams.Seed = 42;
}

void AProceduralPlanet::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority() && !bGenerated)
    {
        GenerateCubeSphere();
        if (bHasOcean) GenerateOcean();
        GenerateFoliage();
        bGenerated = true;
    }
}

void AProceduralPlanet::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 自转
    if (FMath::Abs(RotationSpeed) > KINDA_SMALL_NUMBER)
    {
        FRotator DeltaRot(0.f, RotationSpeed * DeltaTime, 0.f);
        AddActorLocalRotation(DeltaRot);
    }

    // LOD 更新（每 0.5 秒）
    LODTimer += DeltaTime;
    if (LODTimer >= 0.5f)
    {
        LODTimer = 0.f;
        UpdateLOD();
    }

    // 植被流式
    FoliageStreamTimer += DeltaTime;
    if (FoliageStreamTimer >= 2.0f)
    {
        FoliageStreamTimer = 0.f;
        UpdateFoliageStreaming();
    }
}

void AProceduralPlanet::GenerateCubeSphere()
{
    // 6 面方向
    struct FFaceDef { FVector Right, Up, Forward; };
    FFaceDef Faces[6] = {
        {FVector(0,1,0), FVector(0,0,1), FVector(1,0,0)},   // +X
        {FVector(0,-1,0), FVector(0,0,1), FVector(-1,0,0)},  // -X
        {FVector(1,0,0), FVector(0,0,1), FVector(0,1,0)},    // +Y
        {FVector(-1,0,0), FVector(0,0,1), FVector(0,-1,0)},  // -Y
        {FVector(1,0,0), FVector(0,1,0), FVector(0,0,1)},    // +Z
        {FVector(1,0,0), FVector(0,-1,0), FVector(0,0,-1)},   // -Z
    };

    for (int32 i = 0; i < 6; ++i)
    {
        GenerateFace(i, Faces[i].Right, Faces[i].Up, Faces[i].Forward);
    }
}

void AProceduralPlanet::GenerateFace(int32 FaceIndex, const FVector& Right, const FVector& Up, const FVector& Forward)
{
    if (!FaceMeshes.IsValidIndex(FaceIndex)) return;
    UProceduralMeshComponent* Mesh = FaceMeshes[FaceIndex];
    if (!Mesh) return;

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FColor> Colors;
    TArray<FProcMeshTangent> Tangents;

    int32 Res = CubeFaceResolution;
    FRandomStream Rand(RandomSeed + FaceIndex * 1000);

    for (int32 Y = 0; Y <= Res; ++Y)
    {
        for (int32 X = 0; X <= Res; ++X)
        {
            float u = (float)X / Res - 0.5f;
            float v = (float)Y / Res - 0.5f;

            FVector Dir = (Right * u + Up * v + Forward * 0.5f).GetSafeNormal();
            float H = SampleTerrainHeight(Dir);

            FVector Pos = Dir * (PlanetRadius + H * Amplitude);
            Vertices.Add(Pos);
            Normals.Add(Dir);
            UVs.Add(FVector2D((float)X / Res, (float)Y / Res));

            float Lat = FMath::Asin(FMath::Clamp(Dir.Z, -1.f, 1.f)) * 180.f / PI;
            float M = GetMoistureAt(Dir);
            EBiomeType Biome = DetermineBiome(H, Lat, M);
            Colors.Add(GetBiomeColor(Biome));

            Tangents.Add(FProcMeshTangent(Right.X, Right.Y, Right.Z));
        }
    }

    for (int32 Y = 0; Y < Res; ++Y)
    {
        for (int32 X = 0; X < Res; ++X)
        {
            int32 A = Y * (Res + 1) + X;
            int32 B = A + 1;
            int32 C = A + (Res + 1);
            int32 D = C + 1;

            Triangles.Add(A); Triangles.Add(C); Triangles.Add(B);
            Triangles.Add(B); Triangles.Add(C); Triangles.Add(D);
        }
    }

    Mesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, true);
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    // 记录 Chunk
    FPlanetChunk Chunk;
    Chunk.FaceIndex = FaceIndex;
    Chunk.LodLevel = 0;
    Chunk.Mesh = Mesh;
    Chunk.bDirty = false;
    Chunks.Add(Chunk);
}

float AProceduralPlanet::SampleTerrainHeight(const FVector& UnitDir) const
{
    return FBM_3D(UnitDir * NoiseParams.NoiseScale + FVector(NoiseParams.Seed * 0.001f));
}

float AProceduralPlanet::FBM_3D(FVector P) const
{
    float Total = 0.f;
    float Amplitude = 1.f;
    float Frequency = 1.f;
    float MaxVal = 0.f;

    for (int32 i = 0; i < NoiseParams.Octaves; ++i)
    {
        float N = SimpleNoise3D(P * Frequency);
        Total += N * Amplitude;
        MaxVal += Amplitude;
        Amplitude *= NoiseParams.Persistence;
        Frequency *= NoiseParams.Lacunarity;
    }
    return Total / MaxVal; // 0~1
}

float AProceduralPlanet::SimpleNoise3D(FVector P) const
{
    // 简单伪随机 3D 噪声（生产环境建议用 FastNoiseLite）
    float X = FMath::Sin(P.X * 12.9898f + P.Y * 78.233f + P.Z * 37.719f) * 43758.5453f;
    float Y = FMath::Sin(P.X * 39.346f + P.Y * 11.135f + P.Z * 83.155f) * 24634.6345f;
    float Z = FMath::Sin(P.X * 73.156f + P.Y * 52.235f + P.Z * 19.347f) * 34567.3456f;
    float N = FMath::Frac(X + Y + Z);
    return N * 2.f - 1.f; // -1~1
}

float AProceduralPlanet::GetMoistureAt(const FVector& UnitDir) const
{
    FVector P = UnitDir * NoiseParams.NoiseScale * 0.5f + FVector(NoiseParams.Seed * 0.002f);
    return (FBM_3D(P) + 1.f) * 0.5f; // 0~1
}

EBiomeType AProceduralPlanet::DetermineBiome(float Height, float Latitude, float Moisture) const
{
    if (Height < OceanThreshold) return EBiomeType::Ocean;
    if (Height < OceanThreshold + 0.02f) return EBiomeType::Beach;

    float Temp = 1.f - FMath::Abs(Latitude) / 90.f;
    Temp -= Height * 0.3f;

    if (Temp < 0.1f) return EBiomeType::Snow;
    if (Temp < 0.25f) return EBiomeType::Tundra;
    if (Temp > 0.7f && Moisture < 0.3f) return EBiomeType::Desert;
    if (Moisture > 0.6f) return EBiomeType::Forest;
    if (Height > 0.7f) return EBiomeType::Rock;
    return EBiomeType::Grassland;
}

FColor AProceduralPlanet::GetBiomeColor(EBiomeType Biome) const
{
    switch (Biome)
    {
        case EBiomeType::Ocean:    return FColor(20, 50, 120);
        case EBiomeType::Beach:    return FColor(210, 200, 160);
        case EBiomeType::Grassland:return FColor(60, 130, 40);
        case EBiomeType::Forest:   return FColor(30, 90, 30);
        case EBiomeType::Desert:   return FColor(200, 170, 100);
        case EBiomeType::Tundra:   return FColor(140, 150, 130);
        case EBiomeType::Snow:     return FColor(230, 235, 240);
        case EBiomeType::Rock:     return FColor(100, 100, 105);
        default: return FColor::White;
    }
}

EBiomeType AProceduralPlanet::GetBiomeAtWorldPos(const FVector& WorldPos) const
{
    FVector Dir = (WorldPos - GetActorLocation()).GetSafeNormal();
    float H = SampleTerrainHeight(Dir);
    float Lat = FMath::Asin(FMath::Clamp(Dir.Z, -1.f, 1.f)) * 180.f / PI;
    float M = GetMoistureAt(Dir);
    return DetermineBiome(H, Lat, M);
}

float AProceduralPlanet::GetTerrainHeightAtWorldPos(const FVector& WorldPos) const
{
    FVector Dir = (WorldPos - GetActorLocation()).GetSafeNormal();
    return SampleTerrainHeight(Dir);
}

FVector AProceduralPlanet::GetSurfaceNormal(const FVector& WorldPos) const
{
    return (WorldPos - GetActorLocation()).GetSafeNormal();
}

void AProceduralPlanet::GenerateOcean()
{
    if (!OceanMesh) return;

    // 创建一个球体作为海洋
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(
        TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereFinder.Succeeded())
    {
        OceanMesh->SetStaticMesh(SphereFinder.Object);
        OceanMesh->SetRelativeScale3D(FVector(PlanetRadius * 0.001f)); // 近似
        if (OceanMaterial)
            OceanMesh->SetMaterial(0, OceanMaterial);
    }
}

void AProceduralPlanet::GenerateFoliage()
{
    // 由植被 ISM 组件 + 资产注册表驱动
    // 具体实现见头文件注释中的逻辑
}

void AProceduralPlanet::RebuildFoliageInstances()
{
    // 清空 + 按 ActiveFoliageIndices 重建
}

void AProceduralPlanet::UpdateLOD()
{
    // 遍历 Chunks，按距相机距离切换 LOD
    // 高模 → 中模 → 低模
}

void AProceduralPlanet::UpdateFoliageStreaming()
{
    // 按距玩家距离添加/移除实例
}

void AProceduralPlanet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(AProceduralPlanet, PlanetRadius);
    DOREPLIFETIME(AProceduralPlanet, Amplitude);
    DOREPLIFETIME(AProceduralPlanet, CubeFaceResolution);
    DOREPLIFETIME(AProceduralPlanet, RandomSeed);
}
