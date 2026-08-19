// ============================================================
// 路径: Source/StellarSystem/Public/UI/SpaceportUI.h
// 作用: 太空港总控 UI（整合电梯/区域导航/机库/商店入口）
// 依赖: Station/PlanetarySpaceport.h
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SpaceportUI.generated.h"

class APlanetarySpaceport;

// 太空港区域信息
USTRUCT(BlueprintType)
struct FSpaceportZoneDisplay
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString ZoneName;

    UPROPERTY(BlueprintReadOnly)
    FString ZoneType;

    UPROPERTY(BlueprintReadOnly)
    bool bIsAccessible = true;

    UPROPERTY(BlueprintReadOnly)
    bool bRequiresSecurity = false;

    UPROPERTY(BlueprintReadOnly)
    FString RestrictionText;
};

// 太空港主 UI Widget
UCLASS(BlueprintType)
class USpaceportUI : public UUserWidget
{
    GENERATED_BODY()

public:
    // —— 初始化 ——
    UFUNCTION(BlueprintCallable, Category = "SpaceportUI")
    void InitializeSpaceportUI(APlanetarySpaceport* Spaceport);

    // —— 获取所有区域 ——
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SpaceportUI")
    TArray<FSpaceportZoneDisplay> GetAllZones() const;

    // —— 导航到区域 ——
    UFUNCTION(BlueprintCallable, Category = "SpaceportUI")
    void NavigateToZone(int32 ZoneIndex);

    // —— 使用电梯 ——
    UFUNCTION(BlueprintCallable, Category = "SpaceportUI")
    void UseElevatorToOrbit();

    UFUNCTION(BlueprintCallable, Category = "SpaceportUI")
    void UseElevatorToSurface();

    // —— 获取机库列表 ——
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SpaceportUI")
    TArray<FString> GetHangarList() const;

    // —— 租赁机库 ——
    UFUNCTION(BlueprintCallable, Category = "SpaceportUI")
    void RentHangar(int32 HangarIndex, int32 Weeks);

    // —— 获取太空港信息 ——
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SpaceportUI")
    FString GetSpaceportName() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SpaceportUI")
    FString GetSecurityLevelText() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SpaceportUI")
    FString GetFactionText() const;

    // —— 刷新 ——
    UFUNCTION(BlueprintCallable, Category = "SpaceportUI")
    void RefreshUI();

    // —— Tick ——
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
    UPROPERTY()
    APlanetarySpaceport* BoundSpaceport = nullptr;

    float LastRefreshTime = 0.f;
    const float RefreshInterval = 2.f;
};
