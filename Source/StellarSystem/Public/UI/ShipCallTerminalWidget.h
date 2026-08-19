// ============================================================
// 路径: Source/StellarSystem/Public/UI/ShipCallTerminalWidget.h
// 作用: 呼船终端 UI（太空港个人机库内）
// 依赖: Ship/InsuranceSystem.h, Station/PlanetarySpaceport.h
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShipCallTerminalWidget.generated.h"

class APlanetarySpaceport;
class UInsuranceManager;
struct FInsurancePolicy;

// 呼船终端状态
UENUM(BlueprintType)
enum class ETerminalState : uint8
{
    Idle,           // 空闲
    SelectingShip,  // 选择飞船
    CallingShip,    // 呼船中
    ShipEnRoute,    // 飞船在途中
    ShipArrived,    // 飞船已到达
    ProcessingClaim,// 处理索赔
    Error           // 错误
};

// 呼船终端 Widget
UCLASS(BlueprintType)
class UShipCallTerminalWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // —— 初始化 ——
    UFUNCTION(BlueprintCallable, Category = "Terminal")
    void InitializeTerminal(APlanetarySpaceport* Spaceport, const FString& PlayerID);

    // —— 获取可用飞船列表 ——
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Terminal")
    TArray<FInsurancePolicy> GetAvailableShips() const;

    // —— 呼船 ——
    UFUNCTION(BlueprintCallable, Category = "Terminal")
    void CallShip(const FName& ShipID);

    // —— 加速呼船（付费） ——
    UFUNCTION(BlueprintCallable, Category = "Terminal")
    void ExpediteShipCall(const FName& ShipID, float Fee);

    // —— 从保险索赔新飞船 ——
    UFUNCTION(BlueprintCallable, Category = "Terminal")
    void ClaimNewShip(const FName& PolicyID);

    // —— 查询呼船状态 ——
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Terminal")
    ETerminalState GetCallStatus() const;

    // —— 查询预计到达时间 ——
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Terminal")
    float GetEstimatedArrival() const;

    // —— 取消呼船 ——
    UFUNCTION(BlueprintCallable, Category = "Terminal")
    void CancelShipCall();

    // —— 刷新列表 ——
    UFUNCTION(BlueprintCallable, Category = "Terminal")
    void RefreshShipList();

    // —— 获取当前机库信息 ——
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Terminal")
    FString GetHangarInfo() const;

    // —— 切换机库（多机库拥有者） ——
    UFUNCTION(BlueprintCallable, Category = "Terminal")
    void SwitchHangar(const FName& HangarID);

    // —— 事件绑定（蓝图） ——
    UPROPERTY(BlueprintAssignable, Category = "Terminal|Events")
    FOnTerminalStateChanged OnStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Terminal|Events")
    FOnShipCallUpdate OnShipCallUpdate;

    UPROPERTY(BlueprintAssignable, Category = "Terminal|Events")
    FOnTerminalError OnTerminalError;

    // —— Tick ——
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
    // 绑定的太空港
    UPROPERTY()
    APlanetarySpaceport* BoundSpaceport = nullptr;

    // 绑定的保险管理器
    UPROPERTY()
    UInsuranceManager* InsuranceMgr = nullptr;

    // 当前玩家 ID
    FString CurrentPlayerID;

    // 当前状态
    ETerminalState CurrentState = ETerminalState::Idle;

    // 当前呼船请求
    FName ActiveShipCallID;

    // 上次刷新时间
    float LastRefreshTime = 0.f;
    const float RefreshInterval = 2.f;

    // 更新状态
    void UpdateCallStatus(float DeltaTime);

    // 显示错误
    void ShowError(const FString& ErrorMessage);
};
