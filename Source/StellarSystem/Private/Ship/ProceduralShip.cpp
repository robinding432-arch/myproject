// ProceduralShip.cpp
#include "Ship/ProceduralShip.h"
#include "Character/MyCharacter.h"
#include "Ship/ShipAIController.h"
#include "Components/ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Math/UnrealMathUtility.h"

AProceduralShip::AProceduralShip()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    HullMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("HullMesh"));
    HullMesh->SetupAttachment(Root);

    WingMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WingMesh"));
    WingMesh->SetupAttachment(Root);

    EngineMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("EngineMesh"));
    EngineMesh->SetupAttachment(Root);

    CockpitMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CockpitMesh"));
    CockpitMesh->SetupAttachment(Root);

    InteractionVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionVolume"));
    InteractionVolume->SetupAttachment(Root);
    InteractionVolume->SetBoxExtent(FVector(300.f, 300.f, 300.f));
    InteractionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    CockpitArea = CreateDefaultSubobject<UBoxComponent>(TEXT("CockpitArea"));
    CockpitArea->SetupAttachment(Root);
    CockpitArea->SetBoxExtent(FVector(150.f, 150.f, 150.f));
    CockpitArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    CockpitArea->SetCollisionResponseToAllChannels(ECR_Ignore);
    CockpitArea->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void AProceduralShip::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority() && GenParams.ShipClass == EShipClass::Fighter)
    {
        // 如果没设过，用默认值生成
        GenerateShip();
    }

    InteractionVolume->OnComponentBeginOverlap.AddDynamic(
        this, &AProceduralShip::OnInteractionOverlap);
}

void AProceduralShip::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bAIDriven && AIController)
    {
        // AI 逻辑由 Controller 驱动
    }
}

void AProceduralShip::GenerateShip()
{
    FRandomStream Rand(GenParams.Seed);

    switch (GenParams.ShipClass)
    {
    case EShipClass::Fighter:
        GenParams.HullLength = Rand.RandRange(600.f, 900.f);
        GenParams.HullWidth = Rand.RandRange(150.f, 250.f);
        GenParams.HullHeight = Rand.RandRange(100.f, 180.f);
        GenParams.EngineCount = 2;
        GenParams.WingCount = 2;
        GenParams.MaxWarpRange = 5000000.f;
        GenParams.TopSpeed = 8000.f;
        break;
    case EShipClass::Freighter:
        GenParams.HullLength = Rand.RandRange(1500.f, 2500.f);
        GenParams.HullWidth = Rand.RandRange(500.f, 800.f);
        GenParams.HullHeight = Rand.RandRange(400.f, 600.f);
        GenParams.EngineCount = 4;
        GenParams.WingCount = 0;
        GenParams.bHasCargoBay = true;
        GenParams.MaxWarpRange = 3000000.f;
        GenParams.TopSpeed = 2500.f;
        break;
    case EShipClass::Explorer:
        GenParams.HullLength = Rand.RandRange(1000.f, 1400.f);
        GenParams.HullWidth = Rand.RandRange(300.f, 450.f);
        GenParams.HullHeight = Rand.RandRange(200.f, 350.f);
        GenParams.EngineCount = 2;
        GenParams.WingCount = 2;
        GenParams.MaxWarpRange = 15000000.f;
        GenParams.TopSpeed = 5000.f;
        break;
    case EShipClass::Capital:
        GenParams.HullLength = Rand.RandRange(3000.f, 5000.f);
        GenParams.HullWidth = Rand.RandRange(1000.f, 1500.f);
        GenParams.HullHeight = Rand.RandRange(800.f, 1200.f);
        GenParams.EngineCount = 6;
        GenParams.WingCount = 4;
        GenParams.bHasCargoBay = true;
        GenParams.MaxWarpRange = 8000000.f;
        GenParams.TopSpeed = 1500.f;
        break;
    }

    GenerateHull(GenParams.HullLength, GenParams.HullWidth, GenParams.HullHeight);
    GenerateWings(GenParams.HullLength, GenParams.HullWidth, GenParams.HullHeight);
    GenerateEngines(GenParams.EngineCount, GenParams.HullLength);
    GenerateCockpit();
    GenerateInterior();

    InteractionVolume->SetRelativeLocation(FVector(GenParams.HullLength * 0.3f, 0, 0));
}

void AProceduralShip::RegenerateWithSeed(int32 NewSeed)
{
    GenParams.Seed = NewSeed;
    GenerateShip();
}

void AProceduralShip::GenerateHull(float L, float W, float H)
{
    TArray<FVector> V;
    TArray<int32> T;
    TArray<FVector> N;
    TArray<FVector2D> UV;
    TArray<FColor> C;
    TArray<FProcMeshTangent> Tangents;

    const int32 Segments = 16;
    FRandomStream Rand(GenParams.Seed + 100);

    for (int32 Lon = 0; Lon <= Segments; ++Lon)
    {
        float LonR = (float)Lon / Segments;
        float Theta = LonR * 2.f * PI;

        for (int32 Lat = 0; Lat <= Segments; ++Lat)
        {
            float LatR = (float)Lat / Segments;
            float Phi = LatR * PI;

            FVector Sphere(FMath::Sin(Phi)*FMath::Cos(Theta),
                          FMath::Sin(Phi)*FMath::Sin(Theta),
                          FMath::Cos(Phi));

            FVector Scaled(Sphere.X * L * 0.5f, Sphere.Y * W * 0.5f, Sphere.Z * H * 0.5f);

            float Noise = ShipNoise(Scaled * 0.01f);
            Scaled += Sphere * Noise * 10.f;

            if (Sphere.X > 0.f)
                Scaled.X *= FMath::Lerp(1.f, 0.3f, Sphere.X);

            V.Add(Scaled);
            N.Add(Sphere.GetSafeNormal());
            UV.Add(FVector2D(LonR, LatR));
            C.Add(FColor(180, 180, 190, 255));
            Tangents.Add(FProcMeshTangent(0,1,0));
        }
    }

    for (int32 Lon = 0; Lon < Segments; ++Lon)
        for (int32 Lat = 0; Lat < Segments; ++Lat)
        {
            int32 A = Lon * (Segments+1) + Lat;
            int32 B = A + (Segments+1);
            T.Add(A); T.Add(B); T.Add(A+1);
            T.Add(A+1); T.Add(B); T.Add(B+1);
        }

    HullMesh->CreateMeshSection(0, V, T, N, UV, C, Tangents, true);
}

void AProceduralShip::GenerateWings(float L, float W, float H)
{
    if (GenParams.WingCount == 0) return;

    TArray<FVector> V;
    TArray<int32> T;
    TArray<FVector> N;
    TArray<FVector2D> UV;
    TArray<FColor> C;
    TArray<FProcMeshTangent> Tangents;

    float WingY = W * 0.5f;
    float WingSpan = W * 1.5f;
    float WingX = L * 0.15f;
    float WingZ = H * 0.05f;

    V.Add(FVector(-WingX, WingY, 0));       // 0
    V.Add(FVector(WingX, WingY, 0));        // 1
    V.Add(FVector(-WingX*1.5f, WingSpan, WingZ)); // 2
    V.Add(FVector(WingX*0.8f, WingSpan, WingZ));  // 3
    V.Add(FVector(-WingX, WingY, -WingZ*2));  // 4
    V.Add(FVector(WingX, WingY, -WingZ*2));   // 5

    T.Add(0); T.Add(2); T.Add(1);
    T.Add(1); T.Add(2); T.Add(3);
    T.Add(4); T.Add(5); T.Add(2);
    T.Add(5); T.Add(3); T.Add(2);
    T.Add(0); T.Add(4); T.Add(2);
    T.Add(1); T.Add(3); T.Add(5);
    T.Add(0); T.Add(5); T.Add(4);
    T.Add(0); T.Add(1); T.Add(5);

    for (int32 i = 0; i < V.Num(); ++i)
    {
        N.Add(FVector::UpVector);
        UV.Add(FVector2D::ZeroVector);
        C.Add(FColor(160, 160, 170, 255));
        Tangents.Add(FProcMeshTangent(1,0,0));
    }

    WingMesh->CreateMeshSection(0, V, T, N, UV, C, Tangents, true);

    // 右翼镜像
    TArray<FVector> RightV;
    for (const FVector& Vec : V) RightV.Add(FVector(Vec.X, -Vec.Y, Vec.Z));
    WingMesh->CreateMeshSection(1, RightV, T, N, UV, C, Tangents, true);
}

void AProceduralShip::GenerateEngines(int32 Count, float L)
{
    TArray<FVector> V;
    TArray<int32> T;
    TArray<FVector> N;
    TArray<FVector2D> UV;
    TArray<FColor> C;
    TArray<FProcMeshTangent> Tangents;

    float EngineR = GenParams.HullHeight * 0.15f;
    float EngineL = L * 0.25f;
    float YOff = GenParams.HullWidth * 0.35f;
    float ZOff = -GenParams.HullHeight * 0.2f;

    TArray<FVector> Positions;
    if (Count == 2)
    {
        Positions.Add(FVector(-L*0.45f, -YOff, ZOff));
        Positions.Add(FVector(-L*0.45f, YOff, ZOff));
    }
    else if (Count >= 4)
    {
        Positions.Add(FVector(-L*0.45f, -YOff, ZOff));
        Positions.Add(FVector(-L*0.45f, YOff, ZOff));
        Positions.Add(FVector(-L*0.35f, -YOff*0.6f, ZOff));
        Positions.Add(FVector(-L*0.35f, YOff*0.6f, ZOff));
    }

    int32 Segments = 12;
    for (int32 e = 0; e < Positions.Num(); ++e)
    {
        FVector Base = Positions[e];
        int32 BaseIdx = V.Num();

        for (int32 i = 0; i <= Segments; ++i)
        {
            float A = (float)i / Segments * 2.f * PI;
            FVector Circle(FMath::Cos(A)*EngineR, FMath::Sin(A)*EngineR, 0);

            V.Add(Base + Circle);
            N.Add(Circle.GetSafeNormal());
            UV.Add(FVector2D((float)i/Segments, 0));

            V.Add(Base + FVector(-EngineL, 0, 0) + Circle);
            N.Add(Circle.GetSafeNormal());
            UV.Add(FVector2D((float)i/Segments, 1));
        }

        for (int32 i = 0; i < Segments; ++i)
        {
            int32 a = BaseIdx + i*2;
            int32 b = BaseIdx + i*2 + 2;
            int32 c = a + 1;
            int32 d = b + 1;
            T.Add(a); T.Add(b); T.Add(c);
            T.Add(c); T.Add(b); T.Add(d);
        }
    }

    for (int32 i = 0; i < V.Num(); ++i)
    {
        C.Add(FColor(100, 100, 110, 255));
        Tangents.Add(FProcMeshTangent(0,0,1));
    }

    EngineMesh->CreateMeshSection(0, V, T, N, UV, C, Tangents, true);
}

void AProceduralShip::GenerateCockpit()
{
    if (!GenParams.bHasCockpit) return;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(
        TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereFinder.Succeeded())
    {
        CockpitMesh->SetStaticMesh(SphereFinder.Object);
        CockpitMesh->SetRelativeLocation(FVector(
            GenParams.HullLength * 0.2f, 0, GenParams.HullHeight * 0.4f));
        CockpitMesh->SetRelativeScale3D(FVector(
            GenParams.HullWidth * 0.3f / 50.f,
            GenParams.HullWidth * 0.3f / 50.f,
            GenParams.HullHeight * 0.4f / 50.f));
    }
}

void AProceduralShip::GenerateInterior()
{
    CockpitArea->SetRelativeLocation(FVector(
        GenParams.HullLength * 0.15f, 0, GenParams.HullHeight * 0.2f));
    CockpitArea->SetBoxExtent(FVector(
        GenParams.HullLength * 0.3f,
        GenParams.HullWidth * 0.35f,
        GenParams.HullHeight * 0.4f));
}

float AProceduralShip::ShipNoise(FVector P) const
{
    return FMath::Sin(P.X*0.5f)*FMath::Cos(P.Y*0.3f)*0.5f
         + FMath::Sin(P.Z*0.7f + P.X*0.2f)*0.3f;
}

FVector AProceduralShip::GetCockpitEntryPoint() const
{
    return GetActorLocation() + GetActorRotation().RotateVector(
        FVector(GenParams.HullLength*0.3f, 0, -GenParams.HullHeight*0.3f));
}

FVector AProceduralShip::GetCockpitWorldPos() const
{
    return GetActorLocation() + GetActorRotation().RotateVector(
        FVector(GenParams.HullLength*0.2f, 0, GenParams.HullHeight*0.4f));
}

void AProceduralShip::EnterShip_Implementation(APawn* Pawn)
{
    if (!HasAuthority() || !Pawn) return;

    APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
    if (!PC) return;

    bPlayerInside = true;
    Pawn->SetActorHiddenInGame(true);
    Pawn->SetActorEnableCollision(false);
    PC->Possess(this);
}

void AProceduralShip::ExitShip_Implementation()
{
    if (!HasAuthority()) return;

    FVector ExitPos = GetActorLocation() + GetActorRotation().RotateVector(
        FVector(GenParams.HullLength*0.6f, GenParams.HullWidth*0.8f, 0));

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    AMyCharacter* NewChar = GetWorld()->SpawnActor<AMyCharacter>(
        AMyCharacter::StaticClass(), ExitPos, GetActorRotation(), SpawnParams);

    if (NewChar)
    {
        APlayerController* PC = Cast<APlayerController>(GetController());
        if (PC)
        {
            PC->Possess(NewChar);
        }
        bPlayerInside = false;
    }
}

void AProceduralShip::AI_StartExploration()
{
    bAIDriven = true;
    AITimeAtCurrent = 0.f;
    bAITraveling = false;
}

void AProceduralShip::AI_PickNextPlanet()
{
    // 由 AIController 调用
}

void AProceduralShip::AI_WarpToCurrentTarget()
{
    // 跃迁逻辑
}

void AProceduralShip::OnInteractionOverlap(
    UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    // 提示 "Press E to Enter Ship"
}

void AProceduralShip::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(AProceduralShip, GenParams);
    DOREPLIFETIME(AProceduralShip, bPlayerInside);
    DOREPLIFETIME(AProceduralShip, bDoorsOpen);
    DOREPLIFETIME(AProceduralShip, CurrentFuel);
    DOREPLIFETIME(AProceduralShip, CurrentPlanet);
}
