// OceanShader.cpp
#include "Planet/OceanShader.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

UOceanDriverComponent::UOceanDriverComponent()
{
    PrimaryComponentTick.bCanEverTick = true;

    // 默认材质
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> OceanMatFinder(
        TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
    if (OceanMatFinder.Succeeded())
    {
        // 不直接设，等 BeginPlay 创建 MID
    }
}

void UOceanDriverComponent::BeginPlay()
{
    Super::BeginPlay();
    InitializeOceanMaterial();
}

void UOceanDriverComponent::TickComponent(float Dt, ELevelTick Tick, FActorComponentTickFunction* Fn)
{
    Super::TickComponent(Dt, Tick, Fn);
    CurrentTime += Dt * OceanParams.WaveSpeed;
    UpdateMaterialParameters();
}

void UOceanDriverComponent::InitializeOceanMaterial()
{
    if (!TargetOceanMesh) return;
    UMaterialInterface* Parent = TargetOceanMesh->GetMaterial(0);
    if (!Parent) return;

    OceanMID = UMaterialInstanceDynamic::Create(Parent, this);
    if (OceanMID)
    {
        TargetOceanMesh->SetMaterial(0, OceanMID);
        UpdateMaterialParameters();
    }
}

void UOceanDriverComponent::SetOceanTime(float Time)
{
    CurrentTime = Time;
    UpdateMaterialParameters();
}

void UOceanDriverComponent::UpdateMaterialParameters()
{
    if (!OceanMID) return;

    OceanMID->SetVectorParameterValue(FName("BaseColor"), FLinearColor(OceanParams.BaseColor));
    OceanMID->SetVectorParameterValue(FName("DeepColor"), FLinearColor(OceanParams.DeepColor));
    OceanMID->SetVectorParameterValue(FName("FoamColor"), FLinearColor(OceanParams.FoamColor));
    OceanMID->SetScalarParameterValue(FName("WaveAmplitude"), OceanParams.WaveAmplitude);
    OceanMID->SetScalarParameterValue(FName("WaveFrequency"), OceanParams.WaveFrequency);
    OceanMID->SetScalarParameterValue(FName("Time"), CurrentTime);
    OceanMID->SetScalarParameterValue(FName("NormalStrength"), OceanParams.NormalStrength);
    OceanMID->SetScalarParameterValue(FName("FoamThreshold"), OceanParams.FoamThreshold);
    OceanMID->SetScalarParameterValue(FName("Opacity"), OceanParams.Opacity);
    OceanMID->SetScalarParameterValue(FName("FresnelPower"), OceanParams.FresnelPower);
}
