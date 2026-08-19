// CharacterCustomization.cpp
#include "Character/CharacterCustomization.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"

// ============ UFaceGenerator ============

FFaceParameters UFaceGenerator::GenerateFromSeed(int32 Seed, EFaceStylePreset Style)
{
    FRandomStream Rand(Seed);
    FFaceParameters P;

    // 先全随机
    P.HeadWidth = Rand.FRandRange(0.35f, 0.65f);
    P.HeadHeight = Rand.FRandRange(0.4f, 0.6f);
    P.HeadDepth = Rand.FRandRange(0.4f, 0.6f);
    P.BrowRidge = Rand.FRandRange(0.3f, 0.7f);
    P.JawWidth = Rand.FRandRange(0.3f, 0.7f);
    P.JawLength = Rand.FRandRange(0.3f, 0.7f);
    P.ChinProminence = Rand.FRandRange(0.3f, 0.7f);

    P.EyeSize = Rand.FRandRange(0.35f, 0.65f);
    P.EyeSpacing = Rand.FRandRange(0.35f, 0.65f);
    P.EyeVerticalPos = Rand.FRandRange(0.4f, 0.6f);
    P.BrowThickness = Rand.FRandRange(0.3f, 0.7f);
    P.BrowAngle = Rand.FRandRange(0.2f, 0.8f);

    P.NoseSize = Rand.FRandRange(0.35f, 0.65f);
    P.NoseBridgeWidth = Rand.FRandRange(0.35f, 0.65f);
    P.NoseTipAngle = Rand.FRandRange(0.3f, 0.7f);
    P.NostrilSize = Rand.FRandRange(0.35f, 0.65f);

    P.MouthWidth = Rand.FRandRange(0.35f, 0.65f);
    P.MouthThickness = Rand.FRandRange(0.35f, 0.65f);
    P.LipFullness = Rand.FRandRange(0.3f, 0.7f);
    P.SmileCurve = Rand.FRandRange(0.3f, 0.7f);

    P.EarSize = Rand.FRandRange(0.35f, 0.65f);
    P.EarAngle = Rand.FRandRange(0.3f, 0.7f);

    // 皮肤色调（基于种族/随机）
    float SkinHue = Rand.FRandRange(0.03f, 0.12f);
    float SkinSat = Rand.FRandRange(0.15f, 0.45f);
    P.SkinTone = FLinearColor(
        FMath::Clamp(0.6f + SkinHue, 0.3f, 0.95f),
        FMath::Clamp(0.4f + SkinHue * 0.8f, 0.2f, 0.85f),
        FMath::Clamp(0.3f + SkinHue * 0.5f, 0.15f, 0.75f),
        1.f
    );
    P.SkinRoughness = Rand.FRandRange(0.3f, 0.7f);
    P.Freckles = Rand.FRandRange(0.f, 0.4f);
    P.Wrinkles = Rand.FRandRange(0.f, 0.3f);
    P.Scars = Rand.FRand() < 0.15f ? Rand.FRandRange(0.1f, 0.4f) : 0.f;

    // 毛发
    P.HairColor = FLinearColor(
        Rand.FRandRange(0.05f, 0.4f),
        Rand.FRandRange(0.03f, 0.3f),
        Rand.FRandRange(0.02f, 0.25f),
        1.f
    );
    P.HairLength = Rand.FRandRange(0.2f, 0.8f);
    P.HairThickness = Rand.FRandRange(0.3f, 0.8f);
    P.HairCurl = Rand.FRandRange(0.1f, 0.6f);
    P.BeardColor = P.HairColor;
    P.BeardDensity = Rand.FRandRange(0.f, 0.6f);

    // 体型
    P.BodyHeight = Rand.FRandRange(0.35f, 0.65f);
    P.BodyBuild = Rand.FRandRange(0.3f, 0.7f);
    P.ShoulderWidth = Rand.FRandRange(0.35f, 0.65f);
    P.WaistWidth = Rand.FRandRange(0.35f, 0.65f);
    P.ArmLength = Rand.FRandRange(0.4f, 0.6f);
    P.LegLength = Rand.FRandRange(0.4f, 0.6f);

    P.AgeAppearance = Rand.FRandRange(0.15f, 0.55f);
    P.Femininity = Rand.FRandRange(0.2f, 0.8f);

    // 应用风格预设
    ApplyPreset(P, Style, Rand);

    // 性别倾向微调
    if (P.Femininity > 0.6f)
    {
        P.JawWidth *= 0.85f;
        P.BrowRidge *= 0.7f;
        P.LipFullness = FMath::Clamp(P.LipFullness * 1.3f, 0.3f, 1.f);
        P.BodyBuild *= 0.8f;
        P.ShoulderWidth *= 0.85f;
    }
    else if (P.Femininity < 0.4f)
    {
        P.JawWidth *= 1.15f;
        P.BrowRidge *= 1.3f;
        P.LipFullness *= 0.8f;
        P.BodyBuild = FMath::Clamp(P.BodyBuild * 1.2f, 0.3f, 1.f);
        P.ShoulderWidth *= 1.15f;
    }

    ClampParameters(P);
    return P;
}

FFaceParameters UFaceGenerator::Mutate(const FFaceParameters& Base, int32 Seed, float Strength)
{
    FRandomStream Rand(Seed);
    FFaceParameters P = Base;

    // 每个参数以小概率偏移
    auto MutateFloat = [&](float& Val)
    {
        if (Rand.FRand() < 0.6f)
            Val = FMath::Clamp(Val + Rand.FRandRange(-Strength, Strength), 0.f, 1.f);
    };

    MutateFloat(P.HeadWidth); MutateFloat(P.HeadHeight); MutateFloat(P.HeadDepth);
    MutateFloat(P.JawWidth); MutateFloat(P.JawLength); MutateFloat(P.ChinProminence);
    MutateFloat(P.EyeSize); MutateFloat(P.EyeSpacing); MutateFloat(P.BrowThickness);
    MutateFloat(P.NoseSize); MutateFloat(P.NoseBridgeWidth); MutateFloat(P.NostrilSize);
    MutateFloat(P.MouthWidth); MutateFloat(P.LipFullness); MutateFloat(P.SmileCurve);
    MutateFloat(P.HairLength); MutateFloat(P.HairCurl); MutateFloat(P.BeardDensity);
    MutateFloat(P.SkinRoughness); MutateFloat(P.Freckles); MutateFloat(P.Wrinkles);

    ClampParameters(P);
    return P;
}

void UFaceGenerator::ApplyPreset(FFaceParameters& P, EFaceStylePreset Preset, FRandomStream& Rand)
{
    switch (Preset)
    {
    case EFaceStylePreset::Heroic:
        P.JawWidth = 0.65f; P.ChinProminence = 0.7f; P.BrowRidge = 0.7f;
        P.EyeSize = 0.5f; P.NoseSize = 0.55f; P.BodyBuild = 0.7f;
        P.Symmetry = 0.9f; break;
    case EFaceStylePreset::Villainous:
        P.BrowAngle = 0.8f; P.BrowThickness = 0.7f; P.LipFullness = 0.35f;
        P.NoseTipAngle = 0.3f; P.Asymmetry = 0.4f; P.Scars = 0.3f; break;
    case EFaceStylePreset::Cute:
        P.EyeSize = 0.75f; P.HeadWidth = 0.65f; P.HeadHeight = 0.6f;
        P.NoseSize = 0.35f; P.MouthWidth = 0.5f; P.LipFullness = 0.7f;
        P.JawWidth = 0.5f; P.ChinProminence = 0.4f; break;
    case EFaceStylePreset::Rugged:
        P.Wrinkles = 0.6f; P.Scars = 0.4f; P.BrowThickness = 0.8f;
        P.JawWidth = 0.7f; P.BrowRidge = 0.75f; P.BeardDensity = 0.6f; break;
    case EFaceStylePreset::Elegant:
        P.EyeSize = 0.45f; P.NoseBridgeWidth = 0.35f; P.NoseSize = 0.5f;
        P.MouthWidth = 0.45f; P.LipFullness = 0.5f; P.CheekboneHeight = 0.7f; break;
    case EFaceStylePreset::Alien:
        P.HeadDepth = 0.8f; P.EyeSize = 0.8f; P.EyeSpacing = 0.3f;
        P.NoseSize = 0.2f; P.MouthWidth = 0.8f; P.ChinProminence = 0.8f;
        P.HeadHeight = 0.75f; break;
    case EFaceStylePreset::Elder:
        P.AgeAppearance = 0.85f; P.Wrinkles = 0.8f; P.SkinRoughness = 0.8f;
        P.JawLength = 0.6f; P.LipFullness = 0.35f; P.HairThickness = 0.3f; break;
    case EFaceStylePreset::Youthful:
        P.AgeAppearance = 0.15f; P.Wrinkles = 0.05f; P.SkinRoughness = 0.3f;
        P.EyeSize = 0.65f; P.LipFullness = 0.65f; P.CheekFullness = 0.7f; break;
    default: break;
    }
}

void UFaceGenerator::ClampParameters(FFaceParameters& P)
{
    auto Clamp01 = [](float& v) { v = FMath::Clamp(v, 0.f, 1.f); };
    Clamp01(P.HeadWidth); Clamp01(P.HeadHeight); Clamp01(P.HeadDepth);
    Clamp01(P.BrowRidge); Clamp01(P.JawWidth); Clamp01(P.JawLength);
    Clamp01(P.ChinProminence); Clamp01(P.EyeSize); Clamp01(P.EyeSpacing);
    Clamp01(P.EyeVerticalPos); Clamp01(P.BrowThickness); Clamp01(P.BrowAngle);
    Clamp01(P.NoseSize); Clamp01(P.NoseBridgeWidth); Clamp01(P.NoseTipAngle);
    Clamp01(P.NostrilSize); Clamp01(P.MouthWidth); Clamp01(P.MouthThickness);
    Clamp01(P.LipFullness); Clamp01(P.SmileCurve); Clamp01(P.EarSize);
    Clamp01(P.EarAngle); Clamp01(P.SkinRoughness); Clamp01(P.Freckles);
    Clamp01(P.Wrinkles); Clamp01(P.Scars); Clamp01(P.HairLength);
    Clamp01(P.HairThickness); Clamp01(P.HairCurl); Clamp01(P.BeardDensity);
    Clamp01(P.BodyHeight); Clamp01(P.BodyBuild); Clamp01(P.ShoulderWidth);
    Clamp01(P.WaistWidth); Clamp01(P.ArmLength); Clamp01(P.LegLength);
    Clamp01(P.AgeAppearance); Clamp01(P.Femininity);
}

// ============ UCharacterCustomizationComponent ============

UCharacterCustomizationComponent::UCharacterCustomizationComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UCharacterCustomizationComponent::BeginPlay()
{
    Super::BeginPlay();

    if (CharacterSeed == 0) CharacterSeed = FMath::Rand();

    if (HeadMesh == nullptr)
    {
        HeadMesh = NewObject<UProceduralMeshComponent>(GetOwner(), TEXT("HeadMesh"));
        HeadMesh->SetupAttachment(GetOwner()->GetRootComponent());
        HeadMesh->RegisterComponent();
    }
    if (BodyMesh == nullptr)
    {
        BodyMesh = NewObject<UProceduralMeshComponent>(GetOwner(), TEXT("BodyMesh"));
        BodyMesh->SetupAttachment(GetOwner()->GetRootComponent());
        BodyMesh->RegisterComponent();
    }
    if (HairMesh == nullptr)
    {
        HairMesh = NewObject<UProceduralMeshComponent>(GetOwner(), TEXT("HairMesh"));
        HairMesh->SetupAttachment(GetOwner()->GetRootComponent());
        HairMesh->RegisterComponent();
    }
    if (BeardMesh == nullptr)
    {
        BeardMesh = NewObject<UProceduralMeshComponent>(GetOwner(), TEXT("BeardMesh"));
        BeardMesh->SetupAttachment(GetOwner()->GetRootComponent());
        BeardMesh->RegisterComponent();
    }

    if (HasAuthority() || GetOwner()->GetLocalRole() == ROLE_AutonomousProxy)
    {
        FaceParams = UFaceGenerator::GenerateFromSeed(CharacterSeed);
        ApplyFaceParameters(FaceParams);
    }
}

void UCharacterCustomizationComponent::RegenerateFromSeed(int32 NewSeed)
{
    CharacterSeed = NewSeed;
    FaceParams = UFaceGenerator::GenerateFromSeed(NewSeed);
    ApplyFaceParameters(FaceParams);
}

void UCharacterCustomizationComponent::ApplyFaceParameters(const FFaceParameters& Params)
{
    GenerateHeadMesh(Params);
    GenerateBodyMesh(Params);
    GenerateHairMesh(Params);
    GenerateBeardMesh(Params);
    ApplySkinMaterial(Params);
    ApplyBodyProportions(Params);
}

void UCharacterCustomizationComponent::GenerateHeadMesh(const FFaceParameters& P)
{
    if (!HeadMesh) return;

    TArray<FVector> Verts;
    TArray<int32> Tris;
    TArray<FVector> Norms;
    TArray<FVector2D> UVs;
    TArray<FColor> Colors;
    TArray<FProcMeshTangent> Tangents;

    const int32 Segs = 24;
    // 基础椭球，按参数拉伸
    float W = FMath::Lerp(8.f, 16.f, P.HeadWidth);
    float H = FMath::Lerp(10.f, 18.f, P.HeadHeight);
    float D = FMath::Lerp(9.f, 17.f, P.HeadDepth);

    for (int32 i = 0; i <= Segs; ++i)
    {
        float V = (float)i / Segs;
        float Phi = V * PI;
        for (int32 j = 0; j <= Segs; ++j)
        {
            float U = (float)j / Segs;
            float Theta = U * 2.f * PI;

            FVector S(FMath::Sin(Phi)*FMath::Cos(Theta),
                      FMath::Sin(Phi)*FMath::Sin(Theta),
                      FMath::Cos(Phi));
            FVector Pos(S.X * W, S.Y * D, S.Z * H);

            // 下颌收缩
            if (S.Z < -0.2f)
            {
                float T = (S.Z + 1.f) / 0.8f;
                float JawW = FMath::Lerp(FMath::Lerp(0.6f, 1.f, P.JawWidth), 1.f, T);
                Pos.X *= JawW;
                Pos.Y *= JawW * FMath::Lerp(0.8f, 1.f, P.ChinProminence);
            }

            // 眉骨突出
            if (S.Z > 0.1f && S.Z < 0.4f && FMath::Abs(S.X) > 0.2f)
            {
                Pos.Y += FMath::Lerp(0.f, 2.f, P.BrowRidge) * S.Z;
            }

            Verts.Add(Pos);
            Norms.Add(S.GetSafeNormal());
            UVs.Add(FVector2D(U, V));
            Colors.Add(FColor(200, 170, 140, 255));
        }
    }

    for (int32 i = 0; i < Segs; ++i)
    {
        for (int32 j = 0; j < Segs; ++j)
        {
            int32 a = i*(Segs+1)+j, b = a+(Segs+1);
            Tris.Add(a); Tris.Add(b); Tris.Add(a+1);
            Tris.Add(a+1); Tris.Add(b); Tris.Add(b+1);
        }
    }

    HeadMesh->CreateMeshSection(0, Verts, Tris, Norms, UVs, Colors, Tangents, true);
    HeadMesh->SetRelativeLocation(FVector(0, 0, 20.f)); // 头顶上方
}

void UCharacterCustomizationComponent::GenerateBodyMesh(const FFaceParameters& P)
{
    if (!BodyMesh) return;

    // 简化的参数化人体：躯干+四肢用胶囊近似
    float Height = FMath::Lerp(80.f, 120.f, P.BodyHeight);
    float Build = FMath::Lerp(0.7f, 1.5f, P.BodyBuild);
    float ShoulderW = FMath::Lerp(15.f, 30.f, P.ShoulderWidth) * Build;
    float WaistW = FMath::Lerp(10.f, 22.f, P.WaistWidth) * Build;
    float ArmLen = FMath::Lerp(25.f, 45.f, P.ArmLength);
    float LegLen = FMath::Lerp(30.f, 55.f, P.LegLength);

    // 躯干：梯形棱柱
    TArray<FVector> V;
    TArray<int32> T;
    // 上肩
    V.Add(FVector(-ShoulderW, -10*Build, Height*0.3f));  // 0
    V.Add(FVector(ShoulderW, -10*Build, Height*0.3f));   // 1
    V.Add(FVector(-ShoulderW, 10*Build, Height*0.3f));   // 2
    V.Add(FVector(ShoulderW, 10*Build, Height*0.3f));    // 3
    // 腰
    V.Add(FVector(-WaistW, -7*Build, -Height*0.1f));    // 4
    V.Add(FVector(WaistW, -7*Build, -Height*0.1f));     // 5
    V.Add(FVector(-WaistW, 7*Build, -Height*0.1f));     // 6
    V.Add(FVector(WaistW, 7*Build, -Height*0.1f));      // 7

    auto AddQuad = [&](int32 a,int32 b,int32 c,int32 d)
    { T.Add(a); T.Add(b); T.Add(c); T.Add(c); T.Add(b); T.Add(d); };
    AddQuad(0,2,1,3); // front
    AddQuad(5,7,4,6); // back
    AddQuad(0,1,4,5); // bottom
    AddQuad(2,6,3,7); // top
    AddQuad(0,4,2,6); // left
    AddQuad(1,3,5,7); // right

    TArray<FVector> N; N.SetNum(V.Num());
    TArray<FVector2D> UV; UV.SetNum(V.Num());
    TArray<FColor> C; C.SetNum(V.Num());
    TArray<FProcMeshTangent> Tan; Tan.SetNum(V.Num());
    for (int32 i=0;i<V.Num();++i) C[i]=FColor(180,150,120,255);

    BodyMesh->CreateMeshSection(0, V, T, N, UV, C, Tan, true);
    BodyMesh->SetRelativeLocation(FVector(0, 0, -Height*0.5f));
}

void UCharacterCustomizationComponent::GenerateHairMesh(const FFaceParameters& P)
{
    if (!HairMesh) return;
    HairMesh->ClearAllMeshSections();

    if (P.HairLength < 0.1f) return; // 光头

    TArray<FVector> V;
    TArray<int32> T;
    TArray<FVector> N;
    TArray<FVector2D> UV;
    TArray<FColor> C;
    TArray<FProcMeshTangent> Tan;

    // 头发：头部上方的椭球壳
    float HairH = FMath::Lerp(2.f, 12.f, P.HairLength);
    float HairW = FMath::Lerp(0.8f, 1.2f, P.HairThickness);
    int32 Segs = 16;
    float Curl = P.HairCurl;

    for (int32 i = 0; i <= Segs; ++i)
    {
        float Vr = (float)i / Segs;
        float Phi = Vr * PI * 0.6f; // 只取上半球偏下
        for (int32 j = 0; j <= Segs; ++j)
        {
            float Ur = (float)j / Segs;
            float Theta = Ur * 2.f * PI;

            FVector S(FMath::Sin(Phi)*FMath::Cos(Theta),
                      FMath::Sin(Phi)*FMath::Sin(Theta),
                      FMath::Cos(Phi));
            // 卷曲扰动
            float CurlNoise = FMath::Sin(Theta*3 + Phi*2)*Curl*0.3f;
            FVector Pos(S.X*(10*HairW+CurlNoise), S.Y*(10*HairW+CurlNoise), S.Z*(10+HairH)+5);

            V.Add(Pos);
            N.Add(S.GetSafeNormal());
            UV.Add(FVector2D(Ur,Vr));
            C.Add(FColor(40,30,20,255));
        }
    }

    for (int32 i = 0; i < Segs; ++i)
    {
        for (int32 j = 0; j < Segs; ++j)
        {
            int32 a=i*(Segs+1)+j, b=a+(Segs+1);
            T.Add(a); T.Add(b); T.Add(a+1);
            T.Add(a+1); T.Add(b); T.Add(b+1);
        }
    }

    HairMesh->CreateMeshSection(0, V, T, N, UV, C, Tan, false);
    HairMesh->SetRelativeLocation(FVector(0, 0, 20.f));
}

void UCharacterCustomizationComponent::GenerateBeardMesh(const FFaceParameters& P)
{
    if (!BeardMesh) return;
    BeardMesh->ClearAllMeshSections();
    if (P.BeardDensity < 0.1f) return;

    // 简化：下颌区域的薄片
    TArray<FVector> V;
    TArray<int32> T;
    float D = P.BeardDensity;
    V.Add(FVector(-4, 8, -6)); V.Add(FVector(4, 8, -6));
    V.Add(FVector(-5, 8, -2)); V.Add(FVector(5, 8, -2));
    V.Add(FVector(-3, 8.5f, -7)); V.Add(FVector(3, 8.5f, -7));

    T.Add(0); T.Add(1); T.Add(2); T.Add(2); T.Add(1); T.Add(3);
    T.Add(0); T.Add(2); T.Add(4); T.Add(4); T.Add(2); T.Add(5);

    TArray<FVector> N; N.SetNum(V.Num());
    TArray<FVector2D> UV; UV.SetNum(V.Num());
    TArray<FColor> C; C.SetNum(V.Num());
    TArray<FProcMeshTangent> Tan; Tan.SetNum(V.Num());
    for (auto& c : C) c = FColor(50,40,30,255);

    BeardMesh->CreateMeshSection(0, V, T, N, UV, C, Tan, false);
    BeardMesh->SetRelativeLocation(FVector(0, 0, 20.f));
}

void UCharacterCustomizationComponent::ApplySkinMaterial(const FFaceParameters& P)
{
    // 创建动态材质实例，设置皮肤参数
    // 实际项目中这里用 M_Skin 材质模板
    if (HeadMesh && HeadMesh->GetNumSections() > 0)
    {
        // UMaterialInterface* SkinMat = LoadObject<UMaterialInterface>(...);
        // UMaterialInstanceDynamic* Dyn = UMaterialInstanceDynamic::Create(SkinMat, this);
        // Dyn->SetVectorParameterValue("SkinTone", P.SkinTone);
        // Dyn->SetScalarParameterValue("Roughness", P.SkinRoughness);
        // Dyn->SetScalarParameterValue("Freckles", P.Freckles);
        // Dyn->SetScalarParameterValue("Wrinkles", P.Wrinkles);
        // Dyn->SetScalarParameterValue("Scars", P.Scars);
        // HeadMesh->SetMaterial(0, Dyn);
    }
}

void UCharacterCustomizationComponent::ApplyBodyProportions(const FFaceParameters& P)
{
    // 通过缩放 Mesh 实现身高/体型差异
    if (BodyMesh)
    {
        float ScaleH = FMath::Lerp(0.85f, 1.15f, P.BodyHeight);
        float ScaleB = FMath::Lerp(0.8f, 1.3f, P.BodyBuild);
        BodyMesh->SetRelativeScale3D(FVector(ScaleB, ScaleB, ScaleH));
    }
}

void UCharacterCustomizationComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(UCharacterCustomizationComponent, FaceParams);
    DOREPLIFETIME(UCharacterCustomizationComponent, CharacterSeed);
}
