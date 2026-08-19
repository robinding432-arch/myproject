// ShipHUD.h
// 飞船 HUD：速度/燃料/锁定/雷达/模式/跃迁进度
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ShipHUD.generated.h"

class UUserWidget;
class UShipRadarWidget;
class UTexture2D;

// HUD 显示模式
UENUM(BlueprintType)
enum class EHUDMode : uint8
{
    Ground,     // 地面模式
    Orbital,    // 轨道模式
    Warping,    // 跃迁中
    Combat,     // 战斗模式
    Ship        // 飞船内
};

// 雷达目标
USTRUCT(BlueprintType)
struct FRadarContact
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName ContactID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector RelativePosition = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Distance = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsHostile = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsLocked = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ThreatLevel = 0.f;
};

UCLASS()
class AShipHUD : public AHUD
{
    GENERATED_BODY()

public:
    AShipHUD();

    virtual void Tick(float DeltaTime) override;
    virtual void BeginPlay() override;

    // —— 当前模式 ——
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    EHUDMode CurrentMode = EHUDMode::Ground;

    // —— 飞船状态（每帧更新）——
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    float ShipSpeed = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    float ShipMaxSpeed = 5000.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    float FuelPercent = 100.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    float ShieldPercent = 100.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    float HullPercent = 100.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    float HeatPercent = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    float WarpProgress = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    bool bInWarp = false;

    // —— 雷达 ——
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    TArray<FRadarContact> RadarContacts;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Radar")
    float RadarRange = 1000000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Radar")
    float RadarSweepSpeed = 2.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    float RadarSweepAngle = 0.f;

    // —— 锁定 ——
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    FName LockedTargetID;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    float LockProgress = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    bool bHasLock = false;

    // —— 维生 HUD（地面模式用）——
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    float HealthPercent = 100.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    float OxygenPercent = 100.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    float EnergyPercent = 100.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    float HungerPercent = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    float ThirstPercent = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    float RadiationLevel = 0.f;

    // —— 提示 ——
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    FText CurrentWarning;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    float WarningTimer = 0.f;

    // —— 快捷栏 ——
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    TMap<int32, FName> HotbarItems;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    int32 ActiveHotbarSlot = 1;

    // —— API ——
    UFUNCTION(BlueprintCallable, Category = "HUD")
    void SetMode(EHUDMode NewMode);

    UFUNCTION(BlueprintCallable, Category = "HUD")
    void AddRadarContact(const FRadarContact& Contact);

    UFUNCTION(BlueprintCallable, Category = "HUD")
    void RemoveRadarContact(FName ContactID);

    UFUNCTION(BlueprintCallable, Category = "HUD")
    void ClearRadar();

    UFUNCTION(BlueprintCallable, Category = "HUD")
    void SetWarning(FText WarningText, float Duration = 3.f);

    UFUNCTION(BlueprintCallable, Category = "HUD")
    void ShowWarpProgress(float Progress);

    UFUNCTION(BlueprintCallable, Category = "HUD")
    void HideWarpProgress();

    // —— 蓝图 Widget 引用 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Widgets")
    TSubclassOf<UUserWidget> MainHUDWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Widgets")
    TSubclassOf<UUserWidget> RadarWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Widgets")
    TSubclassOf<UUserWidget> WarpWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Widgets")
    TSubclassOf<UUserWidget> VitalsWidgetClass;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HUD|Widgets")
    UUserWidget* MainHUDWidget = nullptr;

    // —— 事件 ——
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnModeChanged, EHUDMode, NewMode);
    UPROPERTY(BlueprintAssignable)
    FOnModeChanged OnModeChanged;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWarningFired, FText, Warning);
    UPROPERTY(BlueprintAssignable)
    FOnWarningFired OnWarningFired;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWarpStarted);
    UPROPERTY(BlueprintAssignable)
    FOnWarpStarted OnWarpStarted;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWarpEnded);
    UPROPERTY(BlueprintAssignable)
    FOnWarpEnded OnWarpEnded;

private:
    void UpdateFromShipPawn();
    void UpdateFromCharacter();
    void UpdateRadarSweep(float Dt);
    void SpawnHUDWidgets();
    void DespawnHUDWidgets();
};
