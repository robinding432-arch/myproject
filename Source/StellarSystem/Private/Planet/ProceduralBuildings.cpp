#include "Planet/ProceduralBuildings.h"
#include "Planet/ProceduralPlanet.h"
#include "AssetRegistry.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AProceduralBuildings::AProceduralBuildings()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AProceduralBuildings::BeginPlay()
{
    Super::BeginPlay();
}

void AProceduralBuildings::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    StreamingTimer += DeltaTime;
    if (StreamingTimer >= RebuildInterval)
    {
        StreamingTimer = 0.f;
        UpdateStreaming();
    }

    // 【Fix 2】分帧增量生成：每帧消费队列中的一部分
    if (PendingBuildingsToGenerate.Num() > 0)
    {
        int32 SpawnedThisFrame = 0;
        for (int32 i = PendingBuildingsToGenerate.Num() - 1; i >= 0 && SpawnedThisFrame < MaxBuildingsPerFrame; --i)
        {
            int32 Idx = PendingBuildingsToGenerate[i];
            if (AllBuildings.IsValidIndex(Idx))
            {
                GenerateBuildingMesh(AllBuildings[Idx].Type, Idx);
            }
            PendingBuildingsToGenerate.RemoveAt(i);
            ++SpawnedThisFrame;
        }
    }
}

// —— 主生成入口 ——

void AProceduralBuildings::GenerateBuildingsForPlanet(AActor* PlanetActor, int32 Seed)
{
    ParentPlanet = PlanetActor;
    AllBuildings.Reset();
    ActiveIndices.Reset();

    if (!PlanetActor) return;

    FRandomStream Rand(Seed == 0 ? FMath::Rand() : Seed);

    // 获取行星参数
    float PlanetRadius = 100000.f;
    FVector PlanetCenter = PlanetActor->GetActorLocation();

    // 从行星查询接口获取（如果有的话）
    if (AProceduralPlanet* Planet = Cast<AProceduralPlanet>(PlanetActor))
    {
        // PlanetRadius = Planet->GetRadius(); // 如果暴露了 getter
    }

    int32 BuildingsSpawned = 0;

    for (const FBuildingGenConfig& Config : GenerationConfigs)
    {
        if (BuildingsSpawned >= TotalBuildingBudget) break;

        int32 Count = Rand.RandRange(Config.MinCount, Config.MaxCount);
        Count = FMath::Min(Count, TotalBuildingBudget - BuildingsSpawned);

        for (int32 i = 0; i < Count; ++i)
        {
            // 在球面上随机取点
            float Theta = Rand.FRandRange(0.f, 2.f * PI);
            float Phi = FMath::Acos(Rand.FRandRange(-1.f, 1.f));
            FVector Dir(
                FMath::Sin(Phi) * FMath::Cos(Theta),
                FMath::Sin(Phi) * FMath::Sin(Theta),
                FMath::Cos(Phi)
            );

            // 密度过滤（用噪声让建筑聚集成聚落）
            float DensityVal = BuildingNoise(Dir * 10.f, 3);
            if (DensityVal < (1.f - Config.Density)) continue;

            // 计算地表位置
            FVector SurfacePos = PlanetCenter + Dir * (PlanetRadius + SurfaceOffset);

            // 建筑实例
            FBuildingInstance Inst;
            Inst.Type = Config.BuildingType;
            Inst.WorldPosition = SurfacePos;
            Inst.Rotation = FRotationMatrix::MakeFromZ(Dir).Rotator();
            Inst.Scale = Rand.FRandRange(0.7f, 1.5f);
            Inst.Seed = Rand.Rand();
            Inst.OwnerFaction = TEXT("Independent");
            Inst.Population = Rand.RandRange(10, 5000);
            Inst.bIsAbandoned = Rand.FRand() < 0.05f; // 5% 废弃率

            AllBuildings.Add(Inst);
            BuildingsSpawned++;

            // 广播
            OnBuildingSpawned.Broadcast(Inst);
        }
    }

    // 初始全部激活
    for (int32 i = 0; i < AllBuildings.Num(); ++i)
        ActiveIndices.Add(i);

    // 生成 Mesh
    RebuildAllBuildingMeshes();

    UE_LOG(LogTemp, Log, TEXT("[Buildings] Generated %d buildings on planet"), AllBuildings.Num());
}

void AProceduralBuildings::GenerateBuildingAt(const FVector& WorldPos, EBuildingType Type, int32 Seed)
{
    FBuildingInstance Inst;
    Inst.Type = Type;
    Inst.WorldPosition = WorldPos;
    Inst.Rotation = FRotator::ZeroRotator;
    Inst.Scale = 1.f;
    Inst.Seed = Seed == 0 ? FMath::Rand() : Seed;

    int32 Idx = AllBuildings.Add(Inst);
    ActiveIndices.Add(Idx);

    GenerateBuildingMesh(Type, Idx);
}

// —— Mesh 重建 ——

void AProceduralBuildings::RebuildAllBuildingMeshes()
{
    // 清空旧 Mesh
    for (auto& Pair : BuildingMeshes)
    {
        if (Pair.Value) Pair.Value->DestroyComponent();
    }
    BuildingMeshes.Reset();

    // 【Fix 2】改为分帧增量生成：把所有索引推入队列
    PendingBuildingsToGenerate.Reset();
    for (int32 Idx : ActiveIndices)
    {
        PendingBuildingsToGenerate.Add(Idx);
    }
    // Tick 中每帧消费 MaxBuildingsPerFrame 个
}

void AProceduralBuildings::GenerateBuildingMesh(EBuildingType Type, int32 InstanceIndex)
{
    if (!AllBuildings.IsValidIndex(InstanceIndex)) return;

    const FBuildingInstance& Inst = AllBuildings[InstanceIndex];

    // 尝试资产覆盖
    if (bUseAssetRegistry)
    {
        // 查 AssetRegistry（简化：直接程序化生成）
        // 实际应从 GameMode 获取 Registry 并查询
    }

    // 程序化生成
    switch (Type)
    {
    case EBuildingType::Habitation:    GenerateHabitation(Inst); break;
    case EBuildingType::Industrial:    GenerateIndustrial(Inst); break;
    case EBuildingType::Research:      GenerateResearch(Inst); break;
    case EBuildingType::Military:      GenerateMilitary(Inst); break;
    case EBuildingType::Trade:         GenerateTrade(Inst); break;
    case EBuildingType::Farm:          GenerateFarm(Inst); break;
    case EBuildingType::Mining:        GenerateMining(Inst); break;
    case EBuildingType::Communication:  GenerateCommunication(Inst); break;
    case EBuildingType::Energy:        GenerateEnergy(Inst); break;
    case EBuildingType::Storage:       GenerateStorage(Inst); break;
    }
}

// —— 各建筑类型生成 ——

void AProceduralBuildings::GenerateHabitation(const FBuildingInstance& Inst)
{
    // 高层公寓：堆叠的盒子
    FRandomStream Rand(Inst.Seed);
    FColor BaseColor = GetBuildingColor(EBuildingType::Habitation, Inst.Seed);

    UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this);
    Mesh->SetupAttachment(RootComponent);
    Mesh->RegisterComponent();

    TArray<FVector> Verts;
    TArray<int32> Tris;
    TArray<FColor> Colors;
    int32 VO = 0;

    int32 Floors = Rand.RandRange(3, 12);
    float FloorHeight = Rand.RandRange(300.f, 600.f);
    float Width = Rand.RandRange(400.f, 800.f);
    float Depth = Rand.RandRange(400.f, 800.f);

    for (int32 f = 0; f < Floors; ++f)
    {
        float Z = f * FloorHeight;
        FVector Center = Inst.WorldPosition + FVector(0, 0, Z);

        // 每层略微收缩（塔楼效果）
        float Shrink = 1.f - (float)f / Floors * 0.15f;
        FVector Size(Width * Shrink, Depth * Shrink, FloorHeight * 0.9f);

        // 随机偏移
        Center += FVector(Rand.FRandRange(-50, 50), Rand.FRandRange(-50, 50), 0);

        AddBoxToMesh(Mesh, Center, Size, BaseColor, VO);

        // 窗户发光（随机楼层）
        if (Rand.FRand() < 0.3f)
        {
            FColor LightColor(255, 255, 200, 255);
            AddBoxToMesh(Mesh, Center + FVector(0, Size.Y * 0.51f, 0),
                FVector(Size.X * 0.8f, 10.f, Size.Z * 0.6f), LightColor, VO);
        }
    }

    // 屋顶天线
    if (Rand.FRand() < 0.4f)
    {
        FVector AntennaBase = Inst.WorldPosition + FVector(0, 0, Floors * FloorHeight);
        AddCylinderToMesh(Mesh, AntennaBase, 20.f, 300.f, 6, FColor(180, 180, 180), VO);
    }

    Mesh->CreateMeshSection(0, Verts, Tris, TArray<FVector>(), TArray<FVector2D>(), Colors,
        TArray<FProcMeshTangent>(), true);
    Mesh->SetWorldTransform(FTransform(Inst.Rotation, Inst.WorldPosition, FVector(Inst.Scale)));

    BuildingMeshes.Add(EBuildingType::Habitation, Mesh);
}

void AProceduralBuildings::GenerateIndustrial(const FBuildingInstance& Inst)
{
    // 工厂：大平房 + 烟囱
    FRandomStream Rand(Inst.Seed);
    UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this);
    Mesh->SetupAttachment(RootComponent);
    Mesh->RegisterComponent();

    TArray<FVector> Verts;
    TArray<int32> Tris;
    TArray<FColor> Colors;
    int32 VO = 0;

    FColor BaseColor(120, 120, 130, 255);

    // 主厂房
    FVector Size(Rand.RandRange(800, 2000), Rand.RandRange(800, 2000), Rand.RandRange(400, 800));
    AddBoxToMesh(Mesh, Inst.WorldPosition + FVector(0, 0, Size.Z * 0.5f),
        Size, BaseColor, VO);

    // 烟囱 x2
    for (int32 i = 0; i < 2; ++i)
    {
        FVector ChimneyPos = Inst.WorldPosition +
            FVector(Rand.FRandRange(-Size.X * 0.3f, Size.X * 0.3f),
                    Rand.FRandRange(-Size.Y * 0.3f, Size.Y * 0.3f),
                    Size.Z + 200.f);
        AddCylinderToMesh(Mesh, ChimneyPos, 80.f, 600.f, 8, FColor(80, 80, 80), VO);
    }

    // 管道
    AddCylinderToMesh(Mesh, Inst.WorldPosition + FVector(Size.X * 0.5f, 0, Size.Z * 0.5f),
        30.f, Size.Y * 0.8f, 6, FColor(150, 100, 50), VO);

    Mesh->CreateMeshSection(0, Verts, Tris, TArray<FVector>(), TArray<FVector2D>(), Colors,
        TArray<FProcMeshTangent>(), true);
    Mesh->SetWorldTransform(FTransform(Inst.Rotation, Inst.WorldPosition, FVector(Inst.Scale)));

    BuildingMeshes.Add(EBuildingType::Industrial, Mesh);
}

void AProceduralBuildings::GenerateResearch(const FBuildingInstance& Inst)
{
    // 科研站：圆顶 + 天线阵列
    FRandomStream Rand(Inst.Seed);
    UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this);
    Mesh->SetupAttachment(RootComponent);
    Mesh->RegisterComponent();

    TArray<FVector> Verts;
    TArray<int32> Tris;
    TArray<FColor> Colors;
    int32 VO = 0;

    // 主圆顶
    float DomeRadius = Rand.RandRange(500.f, 1000.f);
    AddDomeToMesh(Mesh, Inst.WorldPosition + FVector(0, 0, DomeRadius * 0.5f),
        DomeRadius, FColor(200, 220, 255, 200), VO);

    // 中心塔
    AddCylinderToMesh(Mesh, Inst.WorldPosition + FVector(0, 0, DomeRadius),
        30.f, DomeRadius * 1.5f, 8, FColor(100, 150, 255), VO);

    // 环绕天线
    int32 AntennaCount = Rand.RandRange(4, 8);
    for (int32 i = 0; i < AntennaCount; ++i)
    {
        float Angle = (float)i / AntennaCount * 2.f * PI;
        FVector AntPos = Inst.WorldPosition +
            FVector(FMath::Cos(Angle) * DomeRadius * 1.2f,
                    FMath::Sin(Angle) * DomeRadius * 1.2f,
                    DomeRadius * 0.3f);
        AddCylinderToMesh(Mesh, AntPos, 15.f, 400.f, 5, FColor(180, 180, 200), VO);
    }

    Mesh->CreateMeshSection(0, Verts, Tris, TArray<FVector>(), TArray<FVector2D>(), Colors,
        TArray<FProcMeshTangent>(), true);
    Mesh->SetWorldTransform(FTransform(Inst.Rotation, Inst.WorldPosition, FVector(Inst.Scale)));

    BuildingMeshes.Add(EBuildingType::Research, Mesh);
}

void AProceduralBuildings::GenerateMilitary(const FBuildingInstance& Inst)
{
    // 军事基地：碉堡 + 炮塔 + 围墙
    FRandomStream Rand(Inst.Seed);
    UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this);
    Mesh->SetupAttachment(RootComponent);
    Mesh->RegisterComponent();

    TArray<FVector> Verts;
    TArray<int32> Tris;
    TArray<FColor> Colors;
    int32 VO = 0;

    FColor CamoColor(80, 90, 60, 255);

    // 中央指挥所
    AddBoxToMesh(Mesh, Inst.WorldPosition + FVector(0, 0, 300.f),
        FVector(600, 600, 600), CamoColor, VO);

    // 炮塔 x4
    for (int32 i = 0; i < 4; ++i)
    {
        float Angle = (float)i / 4 * 2.f * PI + PI * 0.25f;
        FVector TurretPos = Inst.WorldPosition +
            FVector(FMath::Cos(Angle) * 800.f, FMath::Sin(Angle) * 800.f, 200.f);
        AddCylinderToMesh(Mesh, TurretPos, 100.f, 400.f, 8, FColor(60, 60, 60), VO);
        // 炮管
        AddCylinderToMesh(Mesh, TurretPos + FVector(0, 0, 400.f),
            20.f, 300.f, 6, FColor(50, 50, 50), VO);
    }

    // 围墙
    for (int32 i = 0; i < 8; ++i)
    {
        float Angle = (float)i / 8 * 2.f * PI;
        FVector WallPos = Inst.WorldPosition +
            FVector(FMath::Cos(Angle) * 1200.f, FMath::Sin(Angle) * 1200.f, 150.f);
        AddBoxToMesh(Mesh, WallPos, FVector(100, 200, 300), CamoColor, VO);
    }

    Mesh->CreateMeshSection(0, Verts, Tris, TArray<FVector>(), TArray<FVector2D>(), Colors,
        TArray<FProcMeshTangent>(), true);
    Mesh->SetWorldTransform(FTransform(Inst.Rotation, Inst.WorldPosition, FVector(Inst.Scale)));

    BuildingMeshes.Add(EBuildingType::Military, Mesh);
}

void AProceduralBuildings::GenerateTrade(const FBuildingInstance& Inst)
{
    // 贸易站：开放式大集市
    FRandomStream Rand(Inst.Seed);
    UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this);
    Mesh->SetupAttachment(RootComponent);
    Mesh->RegisterComponent();

    TArray<FVector> Verts;
    TArray<int32> Tris;
    TArray<FColor> Colors;
    int32 VO = 0;

    // 中央穹顶
    float DomeR = 800.f;
    AddDomeToMesh(Mesh, Inst.WorldPosition + FVector(0, 0, DomeR * 0.5f),
        DomeR, FColor(220, 180, 100, 180), VO);

    // 环绕摊位
    int32 StallCount = Rand.RandRange(6, 12);
    for (int32 i = 0; i < StallCount; ++i)
    {
        float Angle = (float)i / StallCount * 2.f * PI;
        FVector StallPos = Inst.WorldPosition +
            FVector(FMath::Cos(Angle) * DomeR * 1.3f,
                    FMath::Sin(Angle) * DomeR * 1.3f,
                    150.f);
        FColor StallColor(Rand.RandRange(100, 255), Rand.RandRange(100, 255), Rand.RandRange(100, 255));
        AddBoxToMesh(Mesh, StallPos, FVector(200, 200, 300), StallColor, VO);
    }

    Mesh->CreateMeshSection(0, Verts, Tris, TArray<FVector>(), TArray<FVector2D>(), Colors,
        TArray<FProcMeshTangent>(), true);
    Mesh->SetWorldTransform(FTransform(Inst.Rotation, Inst.WorldPosition, FVector(Inst.Scale)));

    BuildingMeshes.Add(EBuildingType::Trade, Mesh);
}

void AProceduralBuildings::GenerateFarm(const FBuildingInstance& Inst)
{
    // 农业穹顶：透明穹顶 + 内部网格
    FRandomStream Rand(Inst.Seed);
    UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this);
    Mesh->SetupAttachment(RootComponent);
    Mesh->RegisterComponent();

    TArray<FVector> Verts;
    TArray<int32> Tris;
    TArray<FColor> Colors;
    int32 VO = 0;

    // 多个小穹顶
    int32 DomeCount = Rand.RandRange(3, 7);
    for (int32 i = 0; i < DomeCount; ++i)
    {
        float Angle = (float)i / DomeCount * 2.f * PI;
        FVector DomePos = Inst.WorldPosition +
            FVector(FMath::Cos(Angle) * 600.f, FMath::Sin(Angle) * 600.f, 0);
        float R = Rand.RandRange(200.f, 400.f);
        AddDomeToMesh(Mesh, DomePos + FVector(0, 0, R * 0.4f), R, FColor(100, 255, 100, 100), VO);
    }

    Mesh->CreateMeshSection(0, Verts, Tris, TArray<FVector>(), TArray<FVector2D>(), Colors,
        TArray<FProcMeshTangent>(), true);
    Mesh->SetWorldTransform(FTransform(Inst.Rotation, Inst.WorldPosition, FVector(Inst.Scale)));

    BuildingMeshes.Add(EBuildingType::Farm, Mesh);
}

void AProceduralBuildings::GenerateMining(const FBuildingInstance& Inst)
{
    // 采矿设施：塔架 + 钻头
    FRandomStream Rand(Inst.Seed);
    UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this);
    Mesh->SetupAttachment(RootComponent);
    Mesh->RegisterComponent();

    TArray<FVector> Verts;
    TArray<int32> Tris;
    TArray<FColor> Colors;
    int32 VO = 0;

    // 钻井塔
    float TowerH = Rand.RandRange(1500.f, 3000.f);
    AddPyramidToMesh(Mesh, Inst.WorldPosition, 300.f, TowerH, FColor(180, 140, 60), VO);

    // 基座
    AddBoxToMesh(Mesh, Inst.WorldPosition + FVector(0, 0, 200.f),
        FVector(600, 600, 400), FColor(100, 100, 100), VO);

    // 传送带
    for (int32 i = 0; i < 3; ++i)
    {
        float Angle = (float)i / 3 * 2.f * PI;
        FVector BeltPos = Inst.WorldPosition +
            FVector(FMath::Cos(Angle) * 800.f, FMath::Sin(Angle) * 800.f, 100.f);
        AddBoxToMesh(Mesh, BeltPos, FVector(100, 500, 50), FColor(150, 120, 60), VO);
    }

    Mesh->CreateMeshSection(0, Verts, Tris, TArray<FVector>(), TArray<FVector2D>(), Colors,
        TArray<FProcMeshTangent>(), true);
    Mesh->SetWorldTransform(FTransform(Inst.Rotation, Inst.WorldPosition, FVector(Inst.Scale)));

    BuildingMeshes.Add(EBuildingType::Mining, Mesh);
}

void AProceduralBuildings::GenerateCommunication(const FBuildingInstance& Inst)
{
    // 通信塔：超高塔 + 碟形天线
    FRandomStream Rand(Inst.Seed);
    UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this);
    Mesh->SetupAttachment(RootComponent);
    Mesh->RegisterComponent();

    TArray<FVector> Verts;
    TArray<int32> Tris;
    TArray<FColor> Colors;
    int32 VO = 0;

    // 主塔
    float TowerH = Rand.RandRange(2000.f, 5000.f);
    AddCylinderToMesh(Mesh, Inst.WorldPosition + FVector(0, 0, TowerH * 0.5f),
        50.f, TowerH, 6, FColor(200, 200, 220), VO);

    // 碟形天线
    FVector DishPos = Inst.WorldPosition + FVector(0, 0, TowerH * 0.7f);
    AddDomeToMesh(Mesh, DishPos, 300.f, FColor(180, 180, 200, 200), VO);

    // 横臂
    for (int32 i = 0; i < 3; ++i)
    {
        float Angle = (float)i / 3 * 2.f * PI;
        FVector ArmEnd = DishPos + FVector(FMath::Cos(Angle) * 400.f, FMath::Sin(Angle) * 400.f, 0);
        AddCylinderToMesh(Mesh, (DishPos + ArmEnd) * 0.5f, 15.f,
            FVector::Dist(DishPos, ArmEnd), 5, FColor(150, 150, 170), VO);
    }

    Mesh->CreateMeshSection(0, Verts, Tris, TArray<FVector>(), TArray<FVector2D>(), Colors,
        TArray<FProcMeshTangent>(), true);
    Mesh->SetWorldTransform(FTransform(Inst.Rotation, Inst.WorldPosition, FVector(Inst.Scale)));

    BuildingMeshes.Add(EBuildingType::Communication, Mesh);
}

void AProceduralBuildings::GenerateEnergy(const FBuildingInstance& Inst)
{
    // 能源站：太阳能板阵列 / 核反应堆
    FRandomStream Rand(Inst.Seed);
    UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this);
    Mesh->SetupAttachment(RootComponent);
    Mesh->RegisterComponent();

    TArray<FVector> Verts;
    TArray<int32> Tris;
    TArray<FColor> Colors;
    int32 VO = 0;

    bool bSolar = Rand.FRand() < 0.6f;

    if (bSolar)
    {
        // 太阳能板阵列
        int32 PanelRows = Rand.RandRange(3, 6);
        int32 PanelCols = Rand.RandRange(3, 6);
        for (int32 r = 0; r < PanelRows; ++r)
        {
            for (int32 c = 0; c < PanelCols; ++c)
            {
                FVector PanelPos = Inst.WorldPosition +
                    FVector((c - PanelCols * 0.5f) * 400.f,
                            (r - PanelRows * 0.5f) * 400.f,
                            200.f);
                AddBoxToMesh(Mesh, PanelPos, FVector(350, 350, 20), FColor(30, 50, 150), VO);
            }
        }
    }
    else
    {
        // 核反应堆：圆柱体 + 冷却塔
        AddCylinderToMesh(Mesh, Inst.WorldPosition + FVector(0, 0, 500.f),
            400.f, 1000.f, 12, FColor(180, 180, 180), VO);
        // 冷却塔
        for (int32 i = 0; i < 3; ++i)
        {
            float Angle = (float)i / 3 * 2.f * PI;
            FVector TowerPos = Inst.WorldPosition +
                FVector(FMath::Cos(Angle) * 800.f, FMath::Sin(Angle) * 800.f, 400.f);
            AddCylinderToMesh(Mesh, TowerPos, 150.f, 800.f, 8, FColor(200, 200, 200), VO);
        }
    }

    Mesh->CreateMeshSection(0, Verts, Tris, TArray<FVector>(), TArray<FVector2D>(), Colors,
        TArray<FProcMeshTangent>(), true);
    Mesh->SetWorldTransform(FTransform(Inst.Rotation, Inst.WorldPosition, FVector(Inst.Scale)));

    BuildingMeshes.Add(EBuildingType::Energy, Mesh);
}

void AProceduralBuildings::GenerateStorage(const FBuildingInstance& Inst)
{
    // 仓储：大盒子堆叠
    FRandomStream Rand(Inst.Seed);
    UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this);
    Mesh->SetupAttachment(RootComponent);
    Mesh->RegisterComponent();

    TArray<FVector> Verts;
    TArray<int32> Tris;
    TArray<FColor> Colors;
    int32 VO = 0;

    int32 ContainerCount = Rand.RandRange(4, 10);
    for (int32 i = 0; i < ContainerCount; ++i)
    {
        FVector Pos = Inst.WorldPosition +
            FVector(Rand.FRandRange(-800, 800), Rand.FRandRange(-800, 800), 200.f);
        FVector Size(Rand.RandRange(200, 400), Rand.RandRange(200, 400), Rand.RandRange(200, 400));
        FColor C(150, Rand.RandRange(50, 150), 50, 255);
        AddBoxToMesh(Mesh, Pos, Size, C, VO);
    }

    Mesh->CreateMeshSection(0, Verts, Tris, TArray<FVector>(), TArray<FVector2D>(), Colors,
        TArray<FProcMeshTangent>(), true);
    Mesh->SetWorldTransform(FTransform(Inst.Rotation, Inst.WorldPosition, FVector(Inst.Scale)));

    BuildingMeshes.Add(EBuildingType::Storage, Mesh);
}

// —— 工具函数 ——

void AProceduralBuildings::AddBoxToMesh(UProceduralMeshComponent* Mesh,
    const FVector& Center, const FVector& Size, const FColor& Color, int32& VertexOffset)
{
    if (!Mesh) return;

    TArray<FVector> BoxVerts;
    TArray<int32> BoxTris;
    TArray<FColor> BoxColors;

    FVector H = Size * 0.5f;
    // 8 个顶点
    BoxVerts.Add(Center + FVector(-H.X, -H.Y, -H.Z));
    BoxVerts.Add(Center + FVector( H.X, -H.Y, -H.Z));
    BoxVerts.Add(Center + FVector( H.X,  H.Y, -H.Z));
    BoxVerts.Add(Center + FVector(-H.X,  H.Y, -H.Z));
    BoxVerts.Add(Center + FVector(-H.X, -H.Y,  H.Z));
    BoxVerts.Add(Center + FVector( H.X, -H.Y,  H.Z));
    BoxVerts.Add(Center + FVector( H.X,  H.Y,  H.Z));
    BoxVerts.Add(Center + FVector(-H.X,  H.Y,  H.Z));

    // 6 面 × 2 三角形
    auto AddFace = [&](int32 A, int32 B, int32 C, int32 D)
    {
        BoxTris.Add(A); BoxTris.Add(B); BoxTris.Add(C);
        BoxTris.Add(A); BoxTris.Add(C); BoxTris.Add(D);
    };
    AddFace(0, 1, 2, 3); // 底
    AddFace(4, 6, 5, 7); // 顶
    AddFace(0, 4, 5, 1); // 前
    AddFace(2, 6, 7, 3); // 后
    AddFace(0, 3, 7, 4); // 左
    AddFace(1, 5, 6, 2); // 右

    for (int32 i = 0; i < 8; ++i) BoxColors.Add(Color);

    Mesh->CreateMeshSection(VertexOffset / 8, BoxVerts, BoxTris,
        TArray<FVector>(), TArray<FVector2D>(), BoxColors,
        TArray<FProcMeshTangent>(), true);
    VertexOffset += 8;
}

void AProceduralBuildings::AddCylinderToMesh(UProceduralMeshComponent* Mesh,
    const FVector& Base, float Radius, float Height, int32 Segments,
    const FColor& Color, int32& VertexOffset)
{
    if (!Mesh) return;

    TArray<FVector> Verts;
    TArray<int32> Tris;
    TArray<FColor> Colors;

    for (int32 i = 0; i <= Segments; ++i)
    {
        float Angle = (float)i / Segments * 2.f * PI;
        FVector Offset(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0);

        Verts.Add(Base + Offset);
        Verts.Add(Base + Offset + FVector(0, 0, Height));

        Colors.Add(Color);
        Colors.Add(Color);
    }

    for (int32 i = 0; i < Segments; ++i)
    {
        int32 a = i * 2;
        int32 b = (i + 1) * 2;
        Tris.Add(a); Tris.Add(b); Tris.Add(a + 1);
        Tris.Add(a + 1); Tris.Add(b); Tris.Add(b + 1);
    }

    Mesh->CreateMeshSection(VertexOffset / 2, Verts, Tris,
        TArray<FVector>(), TArray<FVector2D>(), Colors,
        TArray<FProcMeshTangent>(), true);
    VertexOffset += Segments * 2;
}

void AProceduralBuildings::AddPyramidToMesh(UProceduralMeshComponent* Mesh,
    const FVector& Base, float BaseSize, float Height, const FColor& Color, int32& VertexOffset)
{
    if (!Mesh) return;

    TArray<FVector> Verts;
    TArray<int32> Tris;
    TArray<FColor> Colors;

    float H = BaseSize * 0.5f;
    Verts.Add(Base + FVector(-H, -H, 0));
    Verts.Add(Base + FVector( H, -H, 0));
    Verts.Add(Base + FVector( H,  H, 0));
    Verts.Add(Base + FVector(-H,  H, 0));
    Verts.Add(Base + FVector(0, 0, Height));

    Tris.Add(0); Tris.Add(1); Tris.Add(4);
    Tris.Add(1); Tris.Add(2); Tris.Add(4);
    Tris.Add(2); Tris.Add(3); Tris.Add(4);
    Tris.Add(3); Tris.Add(0); Tris.Add(4);
    Tris.Add(0); Tris.Add(3); Tris.Add(2);
    Tris.Add(0); Tris.Add(2); Tris.Add(1);

    for (int32 i = 0; i < 5; ++i) Colors.Add(Color);

    Mesh->CreateMeshSection(VertexOffset / 5, Verts, Tris,
        TArray<FVector>(), TArray<FVector2D>(), Colors,
        TArray<FProcMeshTangent>(), true);
    VertexOffset += 5;
}

void AProceduralBuildings::AddDomeToMesh(UProceduralMeshComponent* Mesh,
    const FVector& Center, float Radius, const FColor& Color, int32& VertexOffset)
{
    if (!Mesh) return;

    TArray<FVector> Verts;
    TArray<int32> Tris;
    TArray<FColor> Colors;

    const int32 LatSegs = 6;
    const int32 LonSegs = 8;

    for (int32 Lat = 0; Lat <= LatSegs; ++Lat)
    {
        float Phi = (float)Lat / LatSegs * PI * 0.5f; // 半球
        float Y = FMath::Cos(Phi) * Radius;
        float R = FMath::Sin(Phi) * Radius;

        for (int32 Lon = 0; Lon <= LonSegs; ++Lon)
        {
            float Theta = (float)Lon / LonSegs * 2.f * PI;
            Verts.Add(Center + FVector(FMath::Cos(Theta) * R, FMath::Sin(Theta) * R, Y));
            Colors.Add(FLinearColor(Color).LinearRGBToHSV().HSVToLinearRGB().ToFColor(true));
        }
    }

    for (int32 Lat = 0; Lat < LatSegs; ++Lat)
    {
        for (int32 Lon = 0; Lon < LonSegs; ++Lon)
        {
            int32 a = Lat * (LonSegs + 1) + Lon;
            int32 b = a + (LonSegs + 1);
            Tris.Add(a); Tris.Add(b); Tris.Add(a + 1);
            Tris.Add(a + 1); Tris.Add(b); Tris.Add(b + 1);
        }
    }

    Mesh->CreateMeshSection(VertexOffset / (LatSegs + 1), Verts, Tris,
        TArray<FVector>(), TArray<FVector2D>(), Colors,
        TArray<FProcMeshTangent>(), true);
    VertexOffset += (LatSegs + 1) * (LonSegs + 1);
}

FColor AProceduralBuildings::GetBuildingColor(EBuildingType Type, int32 Seed) const
{
    FRandomStream Rand(Seed);
    switch (Type)
    {
    case EBuildingType::Habitation:    return FColor(180, 160, 140, 255);
    case EBuildingType::Industrial:    return FColor(120, 120, 130, 255);
    case EBuildingType::Research:      return FColor(140, 180, 255, 200);
    case EBuildingType::Military:      return FColor(80, 90, 60, 255);
    case EBuildingType::Trade:         return FColor(220, 180, 100, 200);
    case EBuildingType::Farm:          return FColor(100, 200, 100, 150);
    case EBuildingType::Mining:        return FColor(180, 140, 60, 255);
    case EBuildingType::Communication: return FColor(200, 200, 220, 255);
    case EBuildingType::Energy:        return FColor(180, 180, 200, 255);
    case EBuildingType::Storage:       return FColor(150, 120, 80, 255);
    }
    return FColor::White;
}

// —— 查询 ——

FBuildingInstance AProceduralBuildings::GetNearestBuilding(const FVector& WorldPos) const
{
    FBuildingInstance Nearest;
    Nearest.Seed = -1;
    float MinDist = FLT_MAX;

    for (const FBuildingInstance& B : AllBuildings)
    {
        float D = FVector::DistSquared(WorldPos, B.WorldPosition);
        if (D < MinDist)
        {
            MinDist = D;
            Nearest = B;
        }
    }
    return Nearest;
}

TArray<FBuildingInstance> AProceduralBuildings::GetBuildingsByType(EBuildingType Type) const
{
    TArray<FBuildingInstance> Result;
    for (const FBuildingInstance& B : AllBuildings)
    {
        if (B.Type == Type) Result.Add(B);
    }
    return Result;
}

// —— 流式 ——

void AProceduralBuildings::UpdateStreaming()
{
    APawn* Player = GetWorld()->GetFirstPlayerController()
        ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
    if (!Player) return;

    FVector PlayerPos = Player->GetActorLocation();
    float RadiusSq = StreamingRadius * StreamingRadius;

    TSet<int32> NewActive;
    for (int32 i = 0; i < AllBuildings.Num(); ++i)
    {
        if (FVector::DistSquared(PlayerPos, AllBuildings[i].WorldPosition) < RadiusSq)
        {
            NewActive.Add(i);
        }
    }

    if (NewActive.Num() != ActiveIndices.Num())
    {
        ActiveIndices = MoveTemp(NewActive);
        RebuildAllBuildingMeshes();
    }
}

// —— 噪声 ——

float AProceduralBuildings::BuildingNoise(const FVector& Pos, int32 Octaves) const
{
    float Result = 0.f;
    float Amp = 1.f;
    float Freq = 1.f;
    float TotalAmp = 0.f;

    FVector P = Pos;
    for (int32 i = 0; i < Octaves; ++i)
    {
        // 简化 3D 噪声（生产环境应换 OpenSimplex）
        float N = FMath::Sin(P.X * Freq) * FMath::Cos(P.Y * Freq * 0.7f)
                + FMath::Sin(P.Z * Freq * 1.3f + P.X * 0.5f);
        Result += N * Amp;
        TotalAmp += Amp;
        Amp *= 0.5f;
        Freq *= 2.f;
    }

    return (Result / TotalAmp) * 0.5f + 0.5f; // 0~1
}
