// PlanetLOD.cpp
// 异步多线程 LOD：TaskGraph 后台生成 + 主线程应用
#include "Planet/PlanetLOD.h"
#include "Components/ProceduralMeshComponent.h"
#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#include "HAL/ThreadSafeCounter.h"
#include "Math/UnrealMathUtility.h"
#include "Engine/World.h"

UPlanetLODManager::UPlanetLODManager()
{
    // 默认 6 面
    Chunks.SetNum(6);
    for (int32 i = 0; i < 6; ++i)
    {
        Chunks[i].FaceIndex = i;
        Chunks[i].LodLevel = 2; // 默认低模
    }
}

void UPlanetLODManager::Initialize(AActor* InPlanetOwner, const FLODSettings& InSettings)
{
    PlanetOwner = InPlanetOwner;
    Settings = InSettings;

    // 初始化 6 面 Chunk
    Chunks.SetNum(6);
    for (int32 i = 0; i < 6; ++i)
    {
        Chunks[i].FaceIndex = i;
        Chunks[i].LodLevel = 2;
        Chunks[i].bGenerating = false;
        Chunks[i].bReadyToApply = false;
    }
}

void UPlanetLODManager::Tick(float DeltaTime, const FVector& ViewerLocation)
{
    CheckTimer += DeltaTime;
    if (CheckTimer >= Settings.UpdateInterval)
    {
        CheckTimer = 0.f;
        CheckAndSchedule(ViewerLocation);
    }

    // 应用已完成的 Chunk
    ApplyCompletedChunks();
}

void UPlanetLODManager::CheckAndSchedule(const FVector& ViewerLocation)
{
    if (!PlanetOwner.IsValid()) return;

    // 限制并发
    int32 AvailableSlots = FMath::Max(0, Settings.MaxConcurrentTasks - ActiveTasks);

    for (int32 i = 0; i < Chunks.Num() && AvailableSlots > 0; ++i)
    {
        FLODChunkData& Chunk = Chunks[i];
        if (Chunk.bGenerating || Chunk.bReadyToApply) continue;

        ELODLevel Target = CalculateTargetLOD(i, ViewerLocation);
        int32 TargetInt = (int32)Target;
        if (TargetInt == Chunk.LodLevel) continue;

        ScheduleChunkGeneration(i, Target);
        --AvailableSlots;
    }
}

void UPlanetLODManager::ScheduleChunkGeneration(int32 ChunkIdx, ELODLevel TargetLOD)
{
    if (!Chunks.IsValidIndex(ChunkIdx)) return;
    FLODChunkData& Chunk = Chunks[ChunkIdx];

    // 【Fix 1】LOD 上限：超过 LOD2 的请求强制降级到 LOD2
    // 避免超高细分（LOD0 128^2 = 32K 顶点）导致后台线程卡顿
    if ((int32)TargetLOD < (int32)ELODLevel::LOD2_Low)
    {
        TargetLOD = ELODLevel::LOD2_Low;
    }

    Chunk.bGenerating = true;
    Chunk.LodLevel = (int32)TargetLOD;

    int32 Resolution = Settings.ResolutionLOD1;
    switch (TargetLOD)
    {
    case ELODLevel::LOD0_High:   Resolution = Settings.ResolutionLOD0; break;
    case ELODLevel::LOD1_Medium: Resolution = Settings.ResolutionLOD1; break;
    case ELODLevel::LOD2_Low:    Resolution = Settings.ResolutionLOD2; break;
    case ELODLevel::LOD3_Cull:   Resolution = 8; break;
    }

    // 【Fix 1】帧预算保护：限制并发任务数上限为 2
    // 防止多线程同时做高分辨率噪声采样导致帧率骤降
    if (ActiveTasks >= 2)
    {
        Chunk.bGenerating = false;
        return; // 稍后 Tick 会重试
    }

    // 捕获所需数据（复制到 lambda）
    FLODChunkData* ChunkPtr = &Chunk;
    int32 Res = Resolution;
    float PRadius = PlanetRadius;
    float PAmp = Amplitude;
    float PNoiseScale = NoiseScale;
    int32 POctaves = Octaves;
    float PPers = Persistence;
    float PLac = Lacunarity;
    int32 PSeed = NoiseSeed;
    float POceanThresh = OceanThreshold;

    ++ActiveTasks;

    // 用 AsyncTask 抛到后台线程
    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [=]()
    {
        GenerateChunkData(ChunkPtr, TargetLOD, Res, PRadius, PAmp,
            PNoiseScale, POctaves, PPers, PLac, PSeed, POceanThresh);

        // 回到游戏线程标记就绪
        AsyncTask(ENamedThreads::GameThread, [=]()
        {
            if (ChunkPtr)
            {
                ChunkPtr->bGenerating = false;
                ChunkPtr->bReadyToApply = true;
            }
            --ActiveTasks;
        });
    });
}

void UPlanetLODManager::ApplyCompletedChunks()
{
    AActor* Owner = PlanetOwner.Get();
    if (!Owner) return;

    for (FLODChunkData& Chunk : Chunks)
    {
        if (!Chunk.bReadyToApply) continue;

        UProceduralMeshComponent* Mesh = GetOrCreateChunkMesh(&Chunk - &Chunks[0]);
        if (!Mesh) continue;

        // 主线程安全：CreateMeshSection
        Mesh->CreateMeshSection(0,
            Chunk.PendingVertices,
            Chunk.PendingTriangles,
            Chunk.PendingNormals,
            Chunk.PendingUVs,
            Chunk.PendingColors,
            Chunk.PendingTangents,
            true);

        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

        // 清空 pending
        Chunk.PendingVertices.Reset();
        Chunk.PendingTriangles.Reset();
        Chunk.PendingNormals.Reset();
        Chunk.PendingUVs.Reset();
        Chunk.PendingColors.Reset();
        Chunk.PendingTangents.Reset();
        Chunk.bReadyToApply = false;
    }
}

// ---- 后台执行（不在游戏线程）----
void UPlanetLODManager::GenerateChunkData(
    FLODChunkData* Chunk, ELODLevel LOD,
    int32 Resolution, float PRadius, float PAmp,
    float PNoiseScale, int32 POctaves, float PPers, float PLac, int32 PSeed, float POceanThresh)
{
    if (!Chunk) return;

    TArray<FVector>& Vertices = Chunk->PendingVertices;
    TArray<int32>& Triangles = Chunk->PendingTriangles;
    TArray<FVector>& Normals = Chunk->PendingNormals;
    TArray<FVector2D>& UVs = Chunk->PendingUVs;
    TArray<FColor>& Colors = Chunk->PendingColors;
    TArray<FProcMeshTangent>& Tangents = Chunk->PendingTangents;

    Vertices.Reset();
    Triangles.Reset();
    Normals.Reset();
    UVs.Reset();
    Colors.Reset();
    Tangents.Reset();

    // 6 面方向（与 ProceduralPlanet 一致）
    struct FFaceDef { FVector Right, Up, Forward; };
    static FFaceDef Faces[6] = {
        {FVector(0,1,0), FVector(0,0,1), FVector(1,0,0)},
        {FVector(0,-1,0), FVector(0,0,1), FVector(-1,0,0)},
        {FVector(1,0,0), FVector(0,0,1), FVector(0,1,0)},
        {FVector(-1,0,0), FVector(0,0,1), FVector(0,-1,0)},
        {FVector(1,0,0), FVector(0,1,0), FVector(0,0,1)},
        {FVector(1,0,0), FVector(0,-1,0), FVector(0,0,-1)},
    };

    if (!Chunks.IsValidIndex(Chunk->FaceIndex)) return;
    const FFaceDef& Face = Faces[Chunk->FaceIndex];

    FRandomStream Rand(PSeed + Chunk->FaceIndex * 1000 + (int32)LOD * 7);

    // 【Fix 1】分块生成：每 32 行让出一次，避免后台线程长时间独占
    const int32 YieldInterval = 32;
    int32 YieldCounter = 0;

    for (int32 Y = 0; Y <= Resolution; ++Y)
    {
        for (int32 X = 0; X <= Resolution; ++X)
        {
            float u = (float)X / Resolution - 0.5f;
            float v = (float)Y / Resolution - 0.5f;

            FVector Dir = (Face.Right * u + Face.Up * v + Face.Forward * 0.5f).GetSafeNormal();

            // fBm 噪声
            float Total = 0.f, Amp = 1.f, Freq = 1.f, MaxVal = 0.f;
            FVector P = Dir * PNoiseScale * 1000.f + FVector(PSeed * 0.001f);
            for (int32 o = 0; o < POctaves; ++o)
            {
                // 简易 3D hash 噪声
                FVector Q = P * Freq;
                float N = FMath::Frac(FMath::Sin(Q.X * 12.9898f + Q.Y * 78.233f + Q.Z * 37.719f) * 43758.5453f);
                N = N * 2.f - 1.f;
                Total += N * Amp;
                MaxVal += Amp;
                Amp *= PPers;
                Freq *= PLac;
            }
            float H = (Total / MaxVal) * 0.5f + 0.5f; // 0~1

            float Height = (H - POceanThresh) * PAmp;
            if (H < POceanThresh) Height = 0.f; // 海底压平

            FVector Pos = Dir * (PRadius + Height);
            Vertices.Add(Pos);
            Normals.Add(Dir);

            float u01 = (float)X / Resolution;
            float v01 = (float)Y / Resolution;
            UVs.Add(FVector2D(u01, v01));

            // Biome 着色
            float Lat = FMath::Asin(FMath::Clamp(Dir.Z, -1.f, 1.f)) * 180.f / PI;
            float Temp = 1.f - FMath::Abs(Lat) / 90.f - H * 0.3f;
            FColor C;
            if (H < POceanThresh) C = FColor(20,50,120);
            else if (Temp < 0.1f) C = FColor(230,235,240);
            else if (Temp < 0.25f) C = FColor(140,150,130);
            else if (Temp > 0.7f && H < 0.4f) C = FColor(200,170,100);
            else if (H > 0.6f) C = FColor(100,100,105);
            else if (H > 0.4f) C = FColor(30,90,30);
            else C = FColor(60,130,40);
            Colors.Add(C);

            FVector Tangent = Face.Right.GetSafeNormal();
            Tangents.Add(FProcMeshTangent(Tangent.X, Tangent.Y, Tangent.Z));

            // 【Fix 1】周期性让出线程
            if (++YieldCounter >= YieldInterval * (Resolution + 1))
            {
                YieldCounter = 0;
                FPlatformProcess::SleepNoStats(0); // 让出时间片
            }
        }
    }

    for (int32 Y = 0; Y < Resolution; ++Y)
    {
        for (int32 X = 0; X < Resolution; ++X)
        {
            int32 A = Y * (Resolution + 1) + X;
            int32 B = A + 1;
            int32 C = A + (Resolution + 1);
            int32 D = C + 1;
            Triangles.Add(A); Triangles.Add(C); Triangles.Add(B);
            Triangles.Add(B); Triangles.Add(C); Triangles.Add(D);
        }
    }
}

UProceduralMeshComponent* UPlanetLODManager::GetOrCreateChunkMesh(int32 ChunkIdx)
{
    if (!Chunks.IsValidIndex(ChunkIdx)) return nullptr;
    FLODChunkData& Chunk = Chunks[ChunkIdx];

    if (Chunk.Mesh) return Chunk.Mesh;

    AActor* Owner = PlanetOwner.Get();
    if (!Owner) return nullptr;

    FName CompName = FName(*FString::Printf(TEXT("LODMesh_%d"), ChunkIdx));
    UProceduralMeshComponent* NewMesh = NewObject<UProceduralMeshComponent>(Owner, CompName);
    if (NewMesh)
    {
        NewMesh->RegisterComponent();
        NewMesh->AttachToComponent(Owner->GetRootComponent(),
            FAttachmentTransformRules::KeepRelativeTransform);
        NewMesh->bUseAsyncCooking = true;
        Chunk.Mesh = NewMesh;
    }
    return NewMesh;
}

ELODLevel UPlanetLODManager::CalculateTargetLOD(int32 ChunkIdx, const FVector& ViewerLocation) const
{
    FVector Center = GetChunkWorldCenter(ChunkIdx);
    float Dist = FVector::Dist(ViewerLocation, Center);

    if (Dist < Settings.DistanceLOD0) return ELODLevel::LOD0_High;
    if (Dist < Settings.DistanceLOD1) return ELODLevel::LOD1_Medium;
    if (Dist < Settings.DistanceLOD2) return ELODLevel::LOD2_Low;
    return ELODLevel::LOD3_Cull;
}

FVector UPlanetLODManager::GetChunkWorldCenter(int32 ChunkIdx) const
{
    AActor* Owner = PlanetOwner.Get();
    if (!Owner) return FVector::ZeroVector;

    // 6 面中心方向
    static FVector FaceCenters[6] = {
        FVector( 1,0,0), FVector(-1,0,0),
        FVector(0, 1,0), FVector(0,-1,0),
        FVector(0,0, 1), FVector(0,0,-1),
    };
    if (!FaceCenters[ChunkIdx].IsNearlyZero())
    {
        return Owner->GetActorLocation() + FaceCenters[ChunkIdx] * PlanetRadius;
    }
    return Owner->GetActorLocation();
}

void UPlanetLODManager::CancelAllTasks()
{
    for (FLODChunkData& Chunk : Chunks)
    {
        Chunk.bGenerating = false;
        Chunk.bReadyToApply = false;
        Chunk.PendingVertices.Reset();
        Chunk.PendingTriangles.Reset();
        Chunk.PendingNormals.Reset();
        Chunk.PendingUVs.Reset();
        Chunk.PendingColors.Reset();
        Chunk.PendingTangents.Reset();
    }
    ActiveTasks = 0;
}

void UPlanetLODManager::RebuildAll()
{
    CancelAllTasks();
    for (int32 i = 0; i < Chunks.Num(); ++i)
    {
        Chunks[i].LodLevel = -1; // 强制重建
    }
}

void UPlanetLODManager::SetNoiseParams(float InNoiseScale, int32 InOctaves, float InPersistence, float InLacunarity, int32 InSeed)
{
    NoiseScale = InNoiseScale;
    Octaves = InOctaves;
    Persistence = InPersistence;
    Lacunarity = InLacunarity;
    NoiseSeed = InSeed;
}

void UPlanetLODManager::SetPlanetParams(float InRadius, float InAmplitude, float InOceanThreshold)
{
    PlanetRadius = InRadius;
    Amplitude = InAmplitude;
    OceanThreshold = InOceanThreshold;
}

bool UPlanetLODManager::IsGenerating() const
{
    return ActiveTasks > 0;
}

int32 UPlanetLODManager::GetActiveTaskCount() const
{
    return ActiveTasks;
}

float UPlanetLODManager::GetGenerationProgress() const
{
    // 简化：按就绪/总数估算
    int32 Total = Chunks.Num();
    if (Total == 0) return 1.f;
    int32 Pending = 0;
    for (const FLODChunkData& C : Chunks)
    {
        if (C.bGenerating || C.bReadyToApply) ++Pending;
    }
    return 1.f - (float)Pending / (float)Total;
}
