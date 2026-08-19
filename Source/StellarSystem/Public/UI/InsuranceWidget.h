// ============================================================
// 路径: Source/StellarSystem/Public/UI/InsuranceWidget.h
// 作用: 保险管理 UI（查看保单/提交索赔/付费加速）
// 依赖: Ship/InsuranceSystem.h
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InsuranceWidget.generated.h"

class UInsuranceManager;

// 保单显示信息
USTRUCT(BlueprintType)
struct FPolicyDisplay
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FName PolicyID;

    UPROPERTY(BlueprintReadOnly)
    FString VehicleName;

    UPROPERTY(BlueprintReadOnly)
    FString TierText;

    UPROPERTY(BlueprintReadOnly)
    FString CoverageText;

    UPROPERTY(BlueprintReadOnly)
    FString ExpiryText;

    UPROPERTY(BlueprintReadOnly)
    FString DeductibleText;

    UPROPERTY(BlueprintReadOnly)
    FString PremiumText;

    UPROPERTY(BlueprintReadOnly)
    bool bIsExpired = false;

    UPROPERTY(BlueprintReadOnly)
    bool bVehicleLost = false;

    UPROPERTY(BlueprintReadOnly)
    int32 ClaimsUsed = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 ClaimsMax = 0;
};

// 索赔显示信息
USTRUCT(BlueprintType)
struct FClaimDisplay
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FName ClaimID;

    UPROPERTY(BlueprintReadOnly)
    FString VehicleName;

    UPROPERTY(BlueprintReadOnly)
    FString StatusText;

    UPROPERTY(BlueprintReadOnly)
    FLinearColor StatusColor;

    UPROPERTY(BlueprintReadOnly)
    float ProgressPercent = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float ETA = 0.f;

    UPROPERTY(BlueprintReadOnly)
    FString DeductibleText;

    UPROPERTY(BlueprintReadOnly)
    bool bCanExpedite = false;

    UPROPERTY(BlueprintReadOnly)
    float ExpediteFee = 0.f;
};

// 保险 UI Widget
UCLASS(BlueprintType)
class UInsuranceWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // —— 初始化 ——
    UFUNCTION(BlueprintCallable, Category = "InsuranceUI")
    void InitializeInsuranceUI(AController* Player);

    // —— 获取所有保单 ——
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "InsuranceUI")
    TArray<FPolicyDisplay> GetAllPolicies() const;

    // —— 获取待处理索赔 ——
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "InsuranceUI")
    TArray<FClaimDisplay> GetPendingClaims() const;

    // —— 提交索赔 ——
    UFUNCTION(BlueprintCallable, Category = "InsuranceUI")
    void FileClaim(const FName& PolicyID, const FString& Reason);

    // —— 加速索赔 ——
    UFUNCTION(BlueprintCallable, Category = "InsuranceUI")
    void ExpediteClaim(const FName& ClaimID);

    // —— 购买保险 ——
    UFUNCTION(BlueprintCallable, Category = "InsuranceUI")
    void PurchaseInsurance(const FName& VehicleID, uint8 Tier, uint8 Coverage, const FName& HangarID);

    // —— 升级保险 ——
    UFUNCTION(BlueprintCallable, Category = "InsuranceUI")
    void UpgradePolicy(const FName& PolicyID, uint8 NewTier);

    // —— 续保 ——
    UFUNCTION(BlueprintCallable, Category = "InsuranceUI")
    void RenewPolicy(const FName& PolicyID);

    // —— 取消保险 ——
    UFUNCTION(BlueprintCallable, Category = "InsuranceUI")
    void CancelPolicy(const FName& PolicyID);

    // —— 找回载具 ——
    UFUNCTION(BlueprintCallable, Category = "InsuranceUI")
    void RecoverVehicle(const FName& VehicleID, const FName& DestinationHangar);

    // —— 刷新 ——
    UFUNCTION(BlueprintCallable, Category = "InsuranceUI")
    void RefreshInsuranceData();

    // —— 获取价格预览 ——
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "InsuranceUI")
    float GetQuote(const FName& VehicleModelID, uint8 Tier, uint8 Coverage) const;

    // —— Tick ——
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // —— 事件 ——
    UPROPERTY(BlueprintAssignable, Category = "InsuranceUI|Events")
    FOnInsuranceDataChanged OnDataChanged;

    UPROPERTY(BlueprintAssignable, Category = "InsuranceUI|Events")
    FOnClaimStatusChanged OnClaimStatusChanged;

protected:
    // 绑定的玩家
    UPROPERTY()
    AController* BoundPlayer = nullptr;

    // 保险管理器
    UPROPERTY()
    UInsuranceManager* InsuranceMgr = nullptr;

    // 转换保单为显示格式
    FPolicyDisplay ConvertPolicyToDisplay(const FInsurancePolicy& Policy) const;

    // 转换索赔为显示格式
    FClaimDisplay ConvertClaimToDisplay(const FInsuranceClaim& Claim) const;

    // 刷新计时
    float LastRefreshTime = 0.f;
    const float RefreshInterval = 3.f;
};

// 保险数据变化事件
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInsuranceDataChanged);

// 索赔状态变化事件
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnClaimStatusChanged, FName, ClaimID, FString, NewStatus);
