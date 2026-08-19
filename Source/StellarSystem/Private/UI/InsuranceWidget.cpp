// ============================================================
// InsuranceWidget.cpp
// 保险管理 UI 实现
// ============================================================

#include "UI/InsuranceWidget.h"
#include "UI/PartyDelegates.h"
#include "Ship/InsuranceSystem.h"
#include "Character/MyCharacter.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Controller.h"

void UInsuranceWidget::InitializeInsuranceUI(AController* Player)
{
    BoundPlayer = Player;

    if (GetWorld())
    {
        InsuranceMgr = GetWorld()->GetSubsystem<UInsuranceManager>();
    }

    RefreshInsuranceData();
}

TArray<FPolicyDisplay> UInsuranceWidget::GetAllPolicies() const
{
    TArray<FPolicyDisplay> Result;

    if (!InsuranceMgr || !BoundPlayer) return Result;

    TArray<FInsurancePolicy> Policies = InsuranceMgr->GetPlayerPolicies(BoundPlayer);

    for (const FInsurancePolicy& Policy : Policies)
    {
        Result.Add(ConvertPolicyToDisplay(Policy));
    }

    return Result;
}

TArray<FClaimDisplay> UInsuranceWidget::GetPendingClaims() const
{
    TArray<FClaimDisplay> Result;

    if (!InsuranceMgr || !BoundPlayer) return Result;

    TArray<FInsuranceClaim> Claims = InsuranceMgr->GetPlayerPendingClaims(BoundPlayer);

    for (const FInsuranceClaim& Claim : Claims)
    {
        Result.Add(ConvertClaimToDisplay(Claim));
    }

    return Result;
}

void UInsuranceWidget::FileClaim(const FName& PolicyID, const FString& Reason)
{
    if (!InsuranceMgr || !BoundPlayer) return;

    InsuranceMgr->Server_FileClaim(BoundPlayer, PolicyID, Reason);
    RefreshInsuranceData();
}

void UInsuranceWidget::ExpediteClaim(const FName& ClaimID)
{
    if (!InsuranceMgr || !BoundPlayer) return;

    // 查找对应保单
    TArray<FInsurancePolicy> Policies = InsuranceMgr->GetPlayerPolicies(BoundPlayer);
    for (const FInsurancePolicy& Policy : Policies)
    {
        // 简化：加速最近的索赔
        TArray<FInsuranceClaim> Claims = InsuranceMgr->GetPlayerPendingClaims(BoundPlayer);
        if (Claims.Num() > 0)
        {
            float Fee = Claims[0].ReplacementCost * 0.1f; // 10% 加速费
            InsuranceMgr->Server_FileExpeditedClaim(BoundPlayer, Policy.PolicyID, Fee);
        }
        break;
    }

    RefreshInsuranceData();
}

void UInsuranceWidget::PurchaseInsurance(const FName& VehicleID, uint8 Tier, uint8 Coverage, const FName& HangarID)
{
    if (!InsuranceMgr || !BoundPlayer) return;

    EInsuranceTier T = (EInsuranceTier)FMath::Clamp((int32)Tier, 0, 5);
    EInsuranceCoverage C = (EInsuranceCoverage)FMath::Clamp((int32)Coverage, 0, 4);

    InsuranceMgr->Server_PurchaseInsurance(BoundPlayer, VehicleID, T, C, HangarID);
    RefreshInsuranceData();
}

void UInsuranceWidget::UpgradePolicy(const FName& PolicyID, uint8 NewTier)
{
    if (!InsuranceMgr || !BoundPlayer) return;

    EInsuranceTier T = (EInsuranceTier)FMath::Clamp((int32)NewTier, 0, 5);
    InsuranceMgr->Server_UpgradeInsurance(BoundPlayer, PolicyID, T);
    RefreshInsuranceData();
}

void UInsuranceWidget::RenewPolicy(const FName& PolicyID)
{
    if (!InsuranceMgr || !BoundPlayer) return;

    InsuranceMgr->Server_RenewPolicy(BoundPlayer, PolicyID);
    RefreshInsuranceData();
}

void UInsuranceWidget::CancelPolicy(const FName& PolicyID)
{
    if (!InsuranceMgr || !BoundPlayer) return;

    InsuranceMgr->Server_CancelPolicy(BoundPlayer, PolicyID);
    RefreshInsuranceData();
}

void UInsuranceWidget::RecoverVehicle(const FName& VehicleID, const FName& DestinationHangar)
{
    if (!InsuranceMgr || !BoundPlayer) return;

    InsuranceMgr->Server_RecoverVehicle(BoundPlayer, VehicleID, DestinationHangar);
    RefreshInsuranceData();
}

float UInsuranceWidget::GetQuote(const FName& VehicleModelID, uint8 Tier, uint8 Coverage) const
{
    if (!InsuranceMgr) return 0.f;

    EInsuranceTier T = (EInsuranceTier)FMath::Clamp((int32)Tier, 0, 5);
    EInsuranceCoverage C = (EInsuranceCoverage)FMath::Clamp((int32)Coverage, 0, 4);

    return InsuranceMgr->CalculatePremium(VehicleModelID, T, C);
}

void UInsuranceWidget::RefreshInsuranceData()
{
    LastRefreshTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    OnDataChanged.Broadcast();
}

void UInsuranceWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!GetWorld()) return;

    float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LastRefreshTime >= RefreshInterval)
    {
        RefreshInsuranceData();
    }
}

FPolicyDisplay UInsuranceWidget::ConvertPolicyToDisplay(const FInsurancePolicy& Policy) const
{
    FPolicyDisplay Display;
    Display.PolicyID = Policy.PolicyID;
    Display.VehicleName = Policy.VehicleDisplayName;
    Display.bVehicleLost = Policy.bVehicleDestroyed || Policy.bVehicleMissing || Policy.bVehicleStolen;

    // 等级文本
    switch (Policy.Tier)
    {
    case EInsuranceTier::None:       Display.TierText = TEXT("No Insurance"); break;
    case EInsuranceTier::Basic:      Display.TierText = TEXT("Basic (90 days)"); break;
    case EInsuranceTier::Standard:   Display.TierText = TEXT("Standard (6 months)"); break;
    case EInsuranceTier::Gold:       Display.TierText = TEXT("Gold (1 year)"); break;
    case EInsuranceTier::Platinum:   Display.TierText = TEXT("Platinum (Lifetime)"); break;
    case EInsuranceTier::Fleet:      Display.TierText = TEXT("Fleet (Org-wide)"); break;
    }

    // 覆盖范围
    switch (Policy.Coverage)
    {
    case EInsuranceCoverage::HullOnly:     Display.CoverageText = TEXT("Hull Only"); break;
    case EInsuranceCoverage::HullAndCargo: Display.CoverageText = TEXT("Hull + Cargo"); break;
    case EInsuranceCoverage::FullCoverage: Display.CoverageText = TEXT("Full Coverage"); break;
    case EInsuranceCoverage::CombatCoverage: Display.CoverageText = TEXT("Combat Coverage"); break;
    case EInsuranceCoverage::PiracyLoss:   Display.CoverageText = TEXT("Piracy Loss"); break;
    }

    // 过期文本
    if (Policy.PolicyExpiryTime == 0.f)
    {
        Display.ExpiryText = TEXT("Never Expires");
        Display.bIsExpired = false;
    }
    else if (GetWorld() && GetWorld()->GetTimeSeconds() >= Policy.PolicyExpiryTime)
    {
        Display.ExpiryText = TEXT("EXPIRED");
        Display.bIsExpired = true;
    }
    else
    {
        float Remaining = Policy.PolicyExpiryTime - (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f);
        Display.ExpiryText = FString::Printf(TEXT("%.0f days left"), Remaining / (24.f * 3600.f));
        Display.bIsExpired = false;
    }

    // 免赔额
    float VehicleValue = 10000.f; // 默认值
    if (InsuranceMgr)
    {
        // 通过保单查询
    }
    Display.DeductibleText = FString::Printf(TEXT("%.0f Credits (%.0f%%)"),
        Policy.DeductiblePercent * VehicleValue, Policy.DeductiblePercent * 100.f);

    // 保费
    Display.PremiumText = FString::Printf(TEXT("%.0f Credits/month"), Policy.MonthlyPremium);

    // 索赔次数
    Display.ClaimsUsed = Policy.ClaimCount;
    Display.ClaimsMax = Policy.MaxClaims;

    return Display;
}

FClaimDisplay UInsuranceWidget::ConvertClaimToDisplay(const FInsuranceClaim& Claim) const
{
    FClaimDisplay Display;
    Display.ClaimID = Claim.ClaimID;

    // 状态
    if (Claim.ClaimStatus == TEXT("Pending"))
    {
        Display.StatusText = TEXT("Pending Review");
        Display.StatusColor = FLinearColor(1.f, 0.8f, 0.f); // 橙黄
    }
    else if (Claim.ClaimStatus == TEXT("Processing"))
    {
        Display.StatusText = TEXT("Processing");
        Display.StatusColor = FLinearColor(0.2f, 0.6f, 1.f); // 蓝
    }
    else if (Claim.ClaimStatus == TEXT("Approved"))
    {
        Display.StatusText = TEXT("Approved - Dispatching");
        Display.StatusColor = FLinearColor(0.2f, 1.f, 0.2f); // 绿
    }
    else if (Claim.ClaimStatus == TEXT("Denied"))
    {
        Display.StatusText = TEXT("DENIED");
        Display.StatusColor = FLinearColor(1.f, 0.2f, 0.2f); // 红
    }
    else if (Claim.ClaimStatus == TEXT("Completed"))
    {
        Display.StatusText = TEXT("Delivered");
        Display.StatusColor = FLinearColor(0.5f, 1.f, 0.5f); // 亮绿
    }

    // 进度
    if (Claim.ProcessingTime > 0.f)
    {
        Display.ProgressPercent = FMath::Clamp(Claim.ElapsedTime / Claim.ProcessingTime, 0.f, 1.f);
        Display.ETA = FMath::Max(0.f, Claim.ProcessingTime - Claim.ElapsedTime);
    }

    // 免赔额
    Display.DeductibleText = FString::Printf(TEXT("%.0f Credits"), Claim.DeductibleAmount);

    // 加速
    Display.bCanExpedite = !Claim.bExpedited && (Claim.ClaimStatus == TEXT("Pending") || Claim.ClaimStatus == TEXT("Processing"));
    Display.ExpediteFee = Claim.ExpediteFee;

    return Display;
}
