// CharacterCustomization.h
// AI 程序化捏脸系统：参数化面部生成 + 混合变形 + 材质参数
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CharacterCustomization.generated.h"

class USkeletalMeshComponent;
class UProceduralMeshComponent;
class UStaticMeshComponent;

// 面部特征参数（全部 0~1 归一化，由 AI 或玩家调节）
USTRUCT(BlueprintType)
struct FFaceParameters
{
    GENERATED_BODY()

    // —— 颅骨轮廓 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float HeadWidth = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float HeadHeight = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float HeadDepth = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float BrowRidge = 0.5f;       // 眉骨突出
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float JawWidth = 0.5f;        // 下颌宽
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float JawLength = 0.5f;       // 下颌长
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float ChinProminence = 0.5f;  // 下巴突出

    // —— 眼睛 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float EyeSize = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float EyeSpacing = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float EyeVerticalPos = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float BrowThickness = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float BrowAngle = 0.5f;        // 上扬/下垂

    // —— 鼻子 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float NoseSize = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float NoseBridgeWidth = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float NoseTipAngle = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float NostrilSize = 0.5f;

    // —— 嘴 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float MouthWidth = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float MouthThickness = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float LipFullness = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float SmileCurve = 0.5f;

    // —— 耳朵 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float EarSize = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float EarAngle = 0.5f;

    // —— 皮肤 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor SkinTone = FLinearColor(0.85f, 0.7f, 0.55f, 1.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float SkinRoughness = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float Freckles = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float Wrinkles = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float Scars = 0.f;

    // —— 毛发 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor HairColor = FLinearColor(0.2f, 0.15f, 0.1f, 1.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float HairLength = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float HairThickness = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float HairCurl = 0.3f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor BeardColor = FLinearColor(0.2f, 0.15f, 0.1f, 1.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float BeardDensity = 0.f;

    // —— 体型 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float BodyHeight = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float BodyBuild = 0.5f;       // 瘦→壮
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float ShoulderWidth = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float WaistWidth = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float ArmLength = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float LegLength = 0.5f;

    // —— 年龄感 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float AgeAppearance = 0.3f;   // 0=幼, 0.5=青, 1=老

    // —— 性别倾向（连续参数，非二元）——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
    float Femininity = 0.5f;      // 0=男性化, 1=女性化
};

// AI 捏脸生成器：Seed → 全套面部参数
UCLASS(BlueprintType)
class UFaceGenerator : public UObject
{
    GENERATED_BODY()

public:
    // 从种子生成一组合理、美观的面部参数
    UFUNCTION(BlueprintCallable, Category = "Character|Customization")
    static FFaceParameters GenerateFromSeed(int32 Seed, EFaceStylePreset Style = EFaceStylePreset::Random);

    // 在基础参数上做小幅随机变异（用于 NPC 多样性）
    UFUNCTION(BlueprintCallable, Category = "Character|Customization")
    static FFaceParameters Mutate(const FFaceParameters& Base, int32 Seed, float Strength = 0.1f);

    // 风格预设
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
    TEnumAsByte<EFaceStylePreset> Preset = EFaceStylePreset::Random;

private:
    static void ApplyPreset(FFaceParameters& P, EFaceStylePreset Preset, FRandomStream& Rand);
    static void ClampParameters(FFaceParameters& P);
};

// 风格预设枚举
UENUM(BlueprintType)
enum class EFaceStylePreset : uint8
{
    Random,
    Heroic,         // 英雄：对称、端正、下巴有力
    Villainous,     // 反派：不对称、薄唇、深眉
    Cute,           // 可爱：大眼、圆脸、小鼻
    Rugged,         // 粗犷：皱纹、疤痕、浓眉
    Elegant,        // 优雅：细长眼、高颧骨
    Alien,          // 异形：极端比例
    Elder,          // 年长：皱纹、松弛
    Youthful        // 年轻：饱满、光滑
};

// 应用到角色 Mesh 的组件
UCLASS(BlueprintType)
class UCharacterCustomizationComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCharacterCustomizationComponent();

    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
    FFaceParameters FaceParams;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CharacterSeed = 0;

    // 程序化基础头部 Mesh（参数化椭球 + 特征位移）
    UPROPERTY(VisibleAnywhere)
    UProceduralMeshComponent* HeadMesh;

    UPROPERTY(VisibleAnywhere)
    UProceduralMeshComponent* BodyMesh;

    UPROPERTY(VisibleAnywhere)
    UProceduralMeshComponent* HairMesh;

    UPROPERTY(VisibleAnywhere)
    UProceduralMeshComponent* BeardMesh;

    // 重生角色外观
    UFUNCTION(BlueprintCallable)
    void RegenerateFromSeed(int32 NewSeed);

    UFUNCTION(BlueprintCallable)
    void ApplyFaceParameters(const FFaceParameters& Params);

    // 网络同步
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    void GenerateHeadMesh(const FFaceParameters& P);
    void GenerateBodyMesh(const FFaceParameters& P);
    void GenerateHairMesh(const FFaceParameters& P);
    void GenerateBeardMesh(const FFaceParameters& P);
    void ApplySkinMaterial(const FFaceParameters& P);
    void ApplyBodyProportions(const FFaceParameters& P);

    // 混合变形（Morph Target）参数计算
    float CalculateMorphValue(FFaceParameters& P);
};
