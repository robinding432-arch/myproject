// ============================================================
// 路径: Source/StellarSystem/Private/Station/ProceduralStation.cpp
// 作用: 程序化空间站实现（4 种类型）
// 依赖: Station/ProceduralStation.h, Core/AssetRegistry.h
// ============================================================

#include "Station/ProceduralStation.h"
#include "Core/AssetRegistry.h"
#include "Components/ProceduralMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Net/UnrealNetwork.h"
#include "Math/UnrealMathUtility.h"

// ======================== 构造 ========================

AProceduralStation::AProceduralStation()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;

    StationName = GenerateStationName();
}

void AProceduralStation::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        GenerateStation();
    }
}

void AProceduralStation::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(AProceduralStation, StationType);
    DOREPLIFETIME(AProceduralStation, StationRadius);
    DOREPLIFETIME(AProceduralStation, ModuleCount);
    DOREPLIFETIME(AProceduralStation, StationName);
    DOREPLIFETIME(AProceduralStation, FactionOwner);
    DOREPLIFETIME(AProceduralStation, Modules);
    DOREPLIFETIME(AProceduralStation, Services);
    DOREPLIFETIME(AProceduralStation, DockedShips);
}

// ======================== Tick ========================

void AProceduralStation::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bRotates && StationType == EStationType::Ring)
    {
        AddActorLocalRotation(FRotator(0.f, RotationSpeed * DeltaTime, 0.f));
    }
}

// ======================== 生成分发 ========================

void AProceduralStation::GenerateStation()
{
    // 尝试资产覆盖
    UAssetRegistry* Reg = GetDefault<UAssetRegistry>();
    if (Reg)
    {
        FGameplayTagContainer Tags;
        FName StationName = FName(*FString::Printf(TEXT("Station_%s"),
            *UEnum::GetValueAsString(StationType)));
        // 如果有美术资产就跳过生成
    }

    switch (StationType)
    {
        case EStationType::Ring:       GenerateRingStation(); break;
        case EStationType::Spherical:  GenerateSphericalStation(); break;
        case EStationType::Cylindrical: GenerateCylindricalStation(); break;
        case EStationType::Modular:    GenerateModularStation(); break;
    }

    GenerateDockingZone();
    GenerateServiceZones();
    RollServices();

    UE_LOG(LogTemp, Log, TEXT("[Station] Generated: %s (%s)"),
        *StationName, *UEnum::GetValueAsString(StationType));
}

// ======================== 环形站 ========================

void AProceduralStation::GenerateRingStation()
{
    // 环形主体
    int32 Segments = 32;
    float RingR = StationRadius;
    float TubeR = StationRadius * 0.15f;

    for (int32 m = 0; m < ModuleCount; ++m)
    {
        float Angle = (float)m / ModuleCount * 2.f * PI;
        FVector ModulePos(
            FMath::Cos(Angle) * RingR,
            FMath::Sin(Angle) * RingR,
            0
        );

        FStationModule Mod;
        Mod.ModuleID = FName(*FString::Printf(TEXT("RingMod_%d"), m));
        Mod.ModuleName = FString::Printf(TEXT("Habitat Module %d"), m);
        Mod.RelativeLocation = ModulePos;
        Mod.RelativeRotation = FRotator(0, FMath::RadiansToDegrees(Angle), 0);
        Mod.Scale = FVector(TubeR * 0.01f, TubeR * 0.01f, StationRadius * 0.005f);
        Mod.bHasDockingBay = (m == 0);
        Mod.bHasTradeTerminal = (m == ModuleCount / 4);
        Mod.bHasRepairBay = (m == ModuleCount / 2);
        Mod.bHasMissionBoard = (m == 3 * ModuleCount / 4);

        Modules.Add(Mod);
        GenerateModuleMesh(Mod, m);
    }
}

// ======================== 球形站 ========================

void AProceduralStation::GenerateSphericalStation()
{
    // 中心球 + 辐射状模块
    FRandomStream Rand(Seed);

    // 中心球
    FStationModule Core;
    Core.ModuleID = FName("Core");
    Core.ModuleName = TEXT("Central Hub");
    Core.RelativeLocation = FVector::ZeroVector;
    Core.Scale = FVector(StationRadius * 0.01f);
    Core.bHasTradeTerminal = true;
    Core.bHasMissionBoard = true;
    Modules.Add(Core);

    for (int32 m = 0; m < ModuleCount; ++m)
    {
        float Angle = Rand.GetFraction() * 2.f * PI;
        float Elevation = Rand.GetFraction() * PI - PI * 0.5f;
        float Dist = StationRadius * Rand.FRandRange(0.8f, 1.5f);

        FVector ModPos(
            FMath::Cos(Elevation) * FMath::Cos(Angle) * Dist,
            FMath::Cos(Elevation) * FMath::Sin(Angle) * Dist,
            FMath::Sin(Elevation) * Dist
        );

        FStationModule Mod;
        Mod.ModuleID = FName(*FString::Printf(TEXT("SphereMod_%d"), m));
        Mod.ModuleName = FString::Printf(TEXT("Spoke Module %d"), m);
        Mod.RelativeLocation = ModPos;
        Mod.Scale = FVector(StationRadius * 0.008f);
        Mod.bHasDockingBay = (m < 2);
        Mod.bHasRepairBay = (m == 2);

        Modules.Add(Mod);
        GenerateModuleMesh(Mod, m + 1);
    }
}

// ======================== 圆柱形站 ========================

void AProceduralStation::GenerateCylindricalStation()
{
    // 中央圆柱 + 上下盖
    float CylHeight = StationRadius * 2.f;
    float CylRadius = StationRadius * 0.4f;

    // 圆柱主体模块
    int32 StackCount = FMath::Min(ModuleCount, 6);
    for (int32 m = 0; m < StackCount; ++m)
    {
        float Z = (float)m / StackCount * CylHeight - CylHeight * 0.5f;

        FStationModule Mod;
        Mod.ModuleID = FName(*FString::Printf(TEXT("CylMod_%d"), m));
        Mod.ModuleName = FString::Printf(TEXT("Industrial Deck %d"), m);
        Mod.RelativeLocation = FVector(0, 0, Z);
        Mod.Scale = FVector(CylRadius * 0.01f, CylRadius * 0.01f, CylHeight / StackCount * 0.01f);
        Mod.bHasRepairBay = (m == StackCount / 2);
        Mod.bHasDockingBay = (m == 0 || m == StackCount - 1);

        Modules.Add(Mod);
        GenerateModuleMesh(Mod, m);
    }
}

// ======================== 模块化集群 ========================

void AProceduralStation::GenerateModularStation()
{
    FRandomStream Rand(Seed);

    for (int32 m = 0; m < ModuleCount; ++m)
    {
        FVector ModPos(
            Rand.FRandRange(-StationRadius * 0.8f, StationRadius * 0.8f),
            Rand.FRandRange(-StationRadius * 0.8f, StationRadius * 0.8f),
            Rand.FRandRange(-StationRadius * 0.5f, StationRadius * 0.5f)
        );

        FStationModule Mod;
        Mod.ModuleID = FName(*FString::Printf(TEXT("Mod_%d"), m));
        Mod.ModuleName = FString::Printf(TEXT("Module %d"), m);
        Mod.RelativeLocation = ModPos;
        Mod.Scale = FVector(Rand.FRandRange(0.5f, 1.5f));
        Mod.bHasDockingBay = (m < 3);
        Mod.bHasTradeTerminal = (m == 3);
        Mod.bHasRepairBay = (m == 4);
        Mod.bHasMissionBoard = (m == 5);

        Modules.Add(Mod);
        GenerateModuleMesh(Mod, m);
    }
}

// ======================== 模块 Mesh ========================

void AProceduralStation::GenerateModuleMesh(const FStationModule& Mod, int32 Index)
{
    UProceduralMeshComponent* ModMesh = NewObject<UProceduralMeshComponent>(this);
    ModMesh->SetupAttachment(RootComponent);
    ModMesh->SetRelativeLocation(Mod.RelativeLocation);
    ModMesh->SetRelativeRotation(Mod.RelativeRotation);
    ModMesh->SetRelativeScale3D(Mod.Scale);
    ModMesh->RegisterComponent();

    // 简单立方体
    TArray<FVector> Verts;
    TArray<int32> Tris;
    TArray<FVector> Norms;
    TArray<FVector2D> UVs;
    TArray<FColor> Colors;
    TArray<FProcMeshTangent> Tangents;

    float S = 200.f; // 基础尺寸
    Verts.Add(FVector(-S,-S,-S)); Verts.Add(FVector(S,-S,-S));
    Verts.Add(FVector(S,S,-S));   Verts.Add(FVector(-S,S,-S));
    Verts.Add(FVector(-S,-S,S));  Verts.Add(FVector(S,-S,S));
    Verts.Add(FVector(S,S,S));    Verts.Add(FVector(-S,S,S));

    Tris.Add(0); Tris.Add(2); Tris.Add(1);
    Tris.Add(0); Tris.Add(3); Tris.Add(2);
    Tris.Add(1); Tris.Add(5); Tris.Add(4);
    Tris.Add(1); Tris.Add(2); Tris.Add(5);
    Tris.Add(2); Tris.Add(6); Tris.Add(5);
    Tris.Add(2); Tris.Add(3); Tris.Add(6);
    Tris.Add(3); Tris.Add(7); Tris.Add(6);
    Tris.Add(3); Tris.Add(0); Tris.Add(7);
    Tris.Add(0); Tris.Add(4); Tris.Add(7);
    Tris.Add(0); Tris.Add(1); Tris.Add(4);
    Tris.Add(4); Tris.Add(6); Tris.Add(7);
    Tris.Add(4); Tris.Add(5); Tris.Add(6);

    for (int32 i = 0; i < 8; ++i)
    {
        Norms.Add(FVector::UpVector);
        UVs.Add(FVector2D::ZeroVector);
        // 按功能着色
        FColor C(150, 150, 160);
        if (Mod.bHasDockingBay) C = FColor(100, 150, 200);
        else if (Mod.bHasTradeTerminal) C = FColor(200, 180, 100);
        else if (Mod.bHasRepairBay) C = FColor(180, 100, 100);
        else if (Mod.bHasMissionBoard) C = FColor(100, 200, 150);
        Colors.Add(C);
    }

    ModMesh->CreateMeshSection(0, Verts, Tris, Norms, UVs, Colors, Tangents, true);
    ModMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ModuleMeshes.Add(ModMesh);
}

// ======================== 停靠区/服务区 ========================

void AProceduralStation::GenerateDockingZone()
{
    DockingZone = NewObject<UBoxComponent>(this);
    DockingZone->SetupAttachment(RootComponent);
    DockingZone->SetRelativeLocation(FVector(StationRadius * 1.2f, 0, 0));
    DockingZone->SetBoxExtent(FVector(500.f, 500.f, 500.f));
    DockingZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    DockingZone->SetCollisionResponseToAllChannels(ECR_Ignore);
    DockingZone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    DockingZone->RegisterComponent();
}

void AProceduralStation::GenerateServiceZones()
{
    for (int32 i = 0; i < Modules.Num(); ++i)
    {
        const FStationModule& Mod = Modules[i];
        if (!Mod.bHasTradeTerminal && !Mod.bHasRepairBay && !Mod.bHasMissionBoard)
            continue;

        UBoxComponent* Zone = NewObject<UBoxComponent>(this);
        Zone->SetupAttachment(RootComponent);
        Zone->SetRelativeLocation(Mod.RelativeLocation);
        Zone->SetBoxExtent(FVector(300.f, 300.f, 300.f));
        Zone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Zone->SetCollisionResponseToAllChannels(ECR_Ignore);
        Zone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
        Zone->RegisterComponent();
        ServiceZones.Add(Zone);
    }
}

// ======================== 服务 ========================

void AProceduralStation::RollServices()
{
    // 贸易终端
    FStationService Trade;
    Trade.ServiceName = TEXT("Trade Terminal");
    Trade.ServiceID = FName("Trade");
    Trade.PriceMultiplier = (FactionOwner == TEXT("Empire")) ? 1.2f : 0.9f;
    Services.Add(Trade);

    // 维修
    FStationService Repair;
    Repair.ServiceName = TEXT("Ship Repair");
    Repair.ServiceID = FName("Repair");
    Repair.PriceMultiplier = 1.0f;
    Services.Add(Repair);

    // 任务板
    FStationService Missions;
    Missions.ServiceName = TEXT("Mission Board");
    Missions.ServiceID = FName("Missions");
    Services.Add(Missions);

    // 补给
    FStationService Supplies;
    Supplies.ServiceName = TEXT("Consumables & Ammo");
    Supplies.ServiceID = FName("Supplies");
    Services.Add(Supplies);
}

FStationService AProceduralStation::GetService(const FName& ServiceID) const
{
    for (const FStationService& S : Services)
    {
        if (S.ServiceID == ServiceID) return S;
    }
    return FStationService();
}

bool AProceduralStation::HasService(const FName& ServiceID) const
{
    for (const FStationService& S : Services)
    {
        if (S.ServiceID == ServiceID) return true;
    }
    return false;
}

TArray<FString> AProceduralStation::GetAvailableServices() const
{
    TArray<FString> Names;
    for (const FStationService& S : Services)
    {
        Names.Add(S.ServiceName);
    }
    return Names;
}

// ======================== 停靠 ========================

void AProceduralStation::DockShip(AActor* Ship)
{
    if (!Ship) return;
    if (!DockedShips.Contains(Ship))
    {
        DockedShips.Add(Ship);
        UE_LOG(LogTemp, Log, TEXT("[Station] %s docked at %s"),
            *Ship->GetName(), *StationName);
    }
}

void AProceduralStation::UndockShip(AActor* Ship)
{
    if (!Ship) return;
    DockedShips.Remove(Ship);
    UE_LOG(LogTemp, Log, TEXT("[Station] %s undocked from %s"),
        *Ship->GetName(), *StationName);
}

// ======================== 工具 ========================

FString AProceduralStation::GenerateStationName() const
{
    TArray<FString> Prefixes = {TEXT("Nova"), TEXT("Zenith"), TEXT("Orion"),
        TEXT("Vanguard"), TEXT("Meridian"), TEXT("Helix"), TEXT("Aegis"), TEXT("Pulsar")};
    TArray<FString> Suffixes = {TEXT("Prime"), TEXT("Station"), TEXT("Hub"),
        TEXT("Outpost"), TEXT("Port"), TEXT("Arsenal"), TEXT("Sanctuary")};

    FRandomStream Rand(FMath::Rand());
    return Prefixes[Rand.RandRange(0,7)] + TEXT(" ") + Suffixes[Rand.RandRange(0,6)];
}
