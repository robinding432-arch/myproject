// OceanShader.h
// 海洋参数结构 + 材质参数驱动接口
#pragma once

#include "CoreMinimal.h"
#include "OceanShader.generated.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;

// 海洋参数（C++ 驱动材质）
USTRUCT(BlueprintType)
struct FOceanParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor BaseColor = FLinearColor(0.02f, 0.05f, 0.2f, 0.85f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor DeepColor = FLinearColor(0.0f, 0.01f, 0.08f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor FoamColor = FLinearColor(1.f, 1.f, 1.f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float WaveAmplitude = 500.f;  // cm

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float WaveFrequency = 0.02f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float WaveSpeed = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float NormalStrength = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FoamThreshold = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Opacity = 0.85f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FresnelPower = 3.0f;
};

// 海洋驱动组件（挂在行星 Actor 上，每帧更新材质参数）
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UOceanDriverComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UOceanDriverComponent();

    virtual void TickComponent(float Dt, ELevelTick Tick, FActorComponentTickFunction* Fn) override;
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FOceanParams OceanParams;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UStaticMeshComponent* TargetOceanMesh = nullptr;

    // 动态创建 MID（如果材质是 M_Ocean 的父类）
    UFUNCTION(BlueprintCallable)
    void InitializeOceanMaterial();

    // 手动驱动时间（暂停时可用）
    UFUNCTION(BlueprintCallable)
    void SetOceanTime(float Time);

private:
    UPROPERTY()
    UMaterialInstanceDynamic* OceanMID = nullptr;

    float CurrentTime = 0.f;

    void UpdateMaterialParameters();
};
