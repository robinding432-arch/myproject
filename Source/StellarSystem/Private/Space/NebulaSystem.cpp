// NebulaSystem.cpp
#include "Space/NebulaSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Components/SphereComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "Math/UnrealMathUtility.h"

ANebulaSystem::ANebulaSystem()
{
    PrimaryActorTick.bCanEverTick = true;

    InfluenceVolume = CreateDefaultSubobject<USphereComponent>(TEXT("InfluenceVolume"));
    InfluenceVolume->SetupAttachment(RootComponent);
    InfluenceVolume->SetSphereRadius(NebulaParams.Radius * 0.001f); // cm → m for collision
    InfluenceVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InfluenceVolume->SetCollisionResponseToAllChannels(ECR_Overlap);

    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> NiagaraFinder(
        TEXT("/Engine/EditorResources/EditorActor.NiagaraActor"));
    // 实际项目中替换为你的粒子系统
}

void ANebulaSystem::BeginPlay()
{
    Super::BeginPlay();
    GenerateNebula();

    InfluenceVolume->OnComponentBeginOverlap.AddDynamic(
        this, &ANebulaSystem::OnActorEnteredVolume);
    InfluenceVolume->OnComponentEndOverlap.AddDynamic(
        this, &ANebulaSystem::OnActorExitedVolume);
}

void ANebulaSystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 自转
    AddActorLocalRotation(FRotator(0.f, NebulaParams.RotationSpeed * DeltaTime, 0.f));

    // 脉动
    CurrentPulsePhase += DeltaTime * 0.5f;
    UpdatePulsation(DeltaTime);

    TimeAccumulator += DeltaTime;
    UpdateParticleColors(DeltaTime);
}

void ANebulaSystem::GenerateNebula()
{
    // 设置体积大小
    if (InfluenceVolume)
        InfluenceVolume->SetSphereRadius(NebulaParams.Radius * 0.001f);

    // 生成粒子偏移（用于密度场采样）
    ParticleOffsets.Reset();
    ParticleSizes.Reset();
    ParticleColors.Reset();

    FRandomStream Rand(NebulaParams.Seed);
    for (int32 i = 0; i < NebulaParams.ParticleCount; ++i)
    {
        // 球内随机分布
        FVector Dir(Rand.FRandRange(-1.f, 1.f), Rand.FRandRange(-1.f, 1.f), Rand.FRandRange(-1.f, 1.f));
        Dir.Normalize();
        float R = Rand.FRandRange(0.f, 1.f);
        R = FMath::Pow(R, 1.f/3.f); // 均匀体积分布
        ParticleOffsets.Add(Dir * R * NebulaParams.Radius);

        ParticleSizes.Add(Rand.FRandRange(0.5f, 1.5f) * NebulaParams.ParticleSize);

        // 颜色混合
        float Mix = Rand.FRand();
        FLinearColor C = FLinearColor::Lerp(NebulaParams.PrimaryColor, NebulaParams.SecondaryColor, Mix);
        ParticleColors.Add(C);
    }
}

void ANebulaSystem::RegenerateWithSeed(int32 NewSeed)
{
    NebulaParams.Seed = NewSeed;
    GenerateNebula();
}

float ANebulaSystem::GetDensityAtLocation(const FVector& WorldPos) const
{
    FVector Local = WorldPos - GetActorLocation();
    return SampleDensityField(Local);
}

float ANebulaSystem::GetRadiationAtLocation(const FVector& WorldPos) const
{
    float Density = GetDensityAtLocation(WorldPos);
    return Density * NebulaParams.RadiationLevel;
}

float ANebulaSystem::GetSensorInterferenceAt(const FVector& WorldPos) const
{
    float Density = GetDensityAtLocation(WorldPos);
    return FMath::Clamp(Density * NebulaParams.SensorInterference, 0.f, 1.f);
}

float ANebulaSystem::SampleDensityField(FVector LocalPos) const
{
    float R = LocalPos.Size();
    if (R > NebulaParams.Radius) return 0.f;

    // 3D 噪声密度
    float Falloff = 1.f - (R / NebulaParams.Radius);
    Falloff = FMath::Pow(Falloff, 2.f);

    FVector P = LocalPos * 0.0001f * (1.f + NebulaParams.Turbulence);
    float N = FMath::Sin(P.X * 12.3f + P.Y * 7.7f) * 0.5f
            + FMath::Cos(P.Y * 5.5f - P.Z * 9.3f) * 0.3f
            + FMath::Sin(P.Z * 8.1f + P.X * 4.7f) * 0.2f;
    N = N * 0.5f + 0.5f; // 0~1

    return FMath::Clamp(N * Falloff * NebulaParams.Density * 2.f, 0.f, 1.f);
}

float ANebulaSystem::SampleNoise3D(FVector P) const
{
    return FMath::Sin(P.X * 3.1f + P.Y * 2.7f + P.Z * 1.9f) * 0.5f
         + FMath::Cos(P.Y * 1.3f - P.Z * 2.1f) * 0.3f;
}

void ANebulaSystem::UpdateParticleColors(float Dt)
{
    // 脉动驱动颜色明暗
    float Pulse = FMath::Sin(CurrentPulsePhase) * 0.5f + 0.5f;
    Pulse *= NebulaParams.PulseAmplitude;

    // 实际项目中更新 Niagara 参数
    // if (NebulaEffect) NebulaEffect->SetVariableFloat(TEXT("Brightness"), 1.f + Pulse);
}

void ANebulaSystem::UpdatePulsation(float Dt)
{
    // 脉动相位更新在 Tick 中完成
}

FNebulaParams ANebulaSystem::MakeEmissionNebula(int32 Seed)
{
    FNebulaParams P;
    P.Type = ENebulaType::Emission;
    P.PrimaryColor = FLinearColor(0.9f, 0.3f, 0.2f, 0.7f);  // 红
    P.SecondaryColor = FLinearColor(1.f, 0.6f, 0.2f, 0.5f);  // 橙
    P.Radius = 1500000.f;
    P.Density = 0.7f;
    P.Turbulence = 0.6f;
    P.PulseSpeed = 0.2f;
    P.PulseAmplitude = 0.15f;
    P.ParticleCount = 8000;
    P.RadiationLevel = 0.05f;
    P.SensorInterference = 0.2f;
    P.Seed = Seed;
    return P;
}

FNebulaParams ANebulaSystem::MakeReflectionNebula(int32 Seed)
{
    FNebulaParams P;
    P.Type = ENebulaType::Reflection;
    P.PrimaryColor = FLinearColor(0.3f, 0.5f, 0.9f, 0.6f);  // 蓝
    P.SecondaryColor = FLinearColor(0.7f, 0.8f, 1.f, 0.4f);   // 浅蓝
    P.Radius = 1200000.f;
    P.Density = 0.5f;
    P.Turbulence = 0.3f;
    P.PulseSpeed = 0.05f;
    P.PulseAmplitude = 0.1f;
    P.ParticleCount = 6000;
    P.RadiationLevel = 0.01f;
    P.SensorInterference = 0.1f;
    P.Seed = Seed;
    return P;
}

FNebulaParams ANebulaSystem::MakeDarkNebula(int32 Seed)
{
    FNebulaParams P;
    P.Type = ENebulaType::DarkNebula;
    P.PrimaryColor = FLinearColor(0.05f, 0.03f, 0.08f, 0.9f); // 近黑
    P.SecondaryColor = FLinearColor(0.1f, 0.08f, 0.15f, 0.7f);
    P.Radius = 2000000.f;
    P.Density = 0.9f;
    P.Turbulence = 0.8f;
    P.PulseSpeed = 0.f; // 暗星云不脉动
    P.PulseAmplitude = 0.f;
    P.ParticleCount = 10000;
    P.RadiationLevel = 0.0f;
    P.SensorInterference = 0.6f; // 强干扰
    P.Seed = Seed;
    return P;
}

FNebulaParams ANebulaSystem::MakePlanetaryNebula(int32 Seed)
{
    FNebulaParams P;
    P.Type = ENebulaType::Planetary;
    P.PrimaryColor = FLinearColor(0.2f, 0.8f, 0.6f, 0.5f); // 绿松石
    P.SecondaryColor = FLinearColor(0.9f, 0.8f, 0.3f, 0.4f); // 金
    P.Radius = 800000.f;
    P.Density = 0.6f;
    P.Turbulence = 0.4f;
    P.PulseSpeed = 0.3f;
    P.PulseAmplitude = 0.25f;
    P.ParticleCount = 5000;
    P.RadiationLevel = 0.02f;
    P.SensorInterference = 0.15f;
    P.Seed = Seed;
    return P;
}

FNebulaParams ANebulaSystem::MakeSupernovaRemnant(int32 Seed)
{
    FNebulaParams P;
    P.Type = ENebulaType::Supernova;
    P.PrimaryColor = FLinearColor(1.f, 0.9f, 0.7f, 0.8f);  // 白热
    P.SecondaryColor = FLinearColor(0.8f, 0.2f, 0.9f, 0.6f); // 紫色丝状
    P.Radius = 3000000.f;
    P.Density = 0.8f;
    P.Turbulence = 1.f;
    P.PulseSpeed = 0.5f;
    P.PulseAmplitude = 0.3f;
    P.ParticleCount = 12000;
    P.RadiationLevel = 0.3f;  // 强辐射
    P.SensorInterference = 0.5f;
    P.Seed = Seed;
    return P;
}

void ANebulaSystem::OnActorEnteredVolume(
    UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    // 通知维生系统：进入高辐射区
    // 通知传感器：干扰增加
}

void ANebulaSystem::OnActorExitedVolume(
    UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    // 恢复正常
}

void ANebulaSystem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(ANebulaSystem, NebulaParams);
}
