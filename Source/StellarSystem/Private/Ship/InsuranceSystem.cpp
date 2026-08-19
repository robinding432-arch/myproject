// ============================================================
// InsuranceSystem.cpp
// 飞船及地面载具保险索赔系统实现
// 修改于: v7.6 (索赔按配置重建/原船失效/货物转移/新船生成)
// ============================================================

#include "Ship/InsuranceSystem.h"
#include "Ship/ShipPawn.h"
#include "Ship/ShipCargoComponent.h"
#include "Station/PlanetarySpaceport.h"
#include "Character/MyCharacter.h"
#include "Core/StellarGameMode.h"
#include "Death/ShipInvalidationSystem.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Math/UnrealMathUtility.h"
#include "Kismet/GameplayStatics.h"

UInsuranceManager::UInsuranceManager()
{
    InitializeDefaultPricing();
}

void UInsuranceManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (GetWorld() && GetWorld()->GetNetMode() != NM_Client)
    {
        InitializeDefaultPricing();
    }
}

void UInsuranceManager::Deinitialize()
{
    AllPolicies.Empty();
    PendingClaims.Empty();
    VehicleOwners.Empty();
    VehicleBaseValues.Empty();
    ReadyToSpawnClaims.Empty();

    Super::Deinitialize();
}

void UInsuranceManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (GetWorld() == nullptr) return;

    // 处理索赔进度
    ProcessClaims(DeltaTime);

    // 处理待生成队列
    ProcessReadyToSpawnClaims();

    // 自动续保检查
    float CurrentTime = GetWorld()->GetTimeSeconds();
    for (auto& Pair : AllPolicies)
    {
        FInsurancePolicy& Policy = Pair.Value;
        if (Policy.bAutoRenew && Policy.PolicyExpiryTime > 0.f)
        {
            if (CurrentTime >= Policy.PolicyExpiryTime)
            {
                Policy.PolicyExpiryTime = CurrentTime + 30.f * 24.f * 3600.f;
                Policy.LastPaymentTime = CurrentTime;
            }
        }
    }
}

void UInsuranceManager::InitializeDefaultPricing()
{
    TierBasePrice.Empty();
    TierBasePrice.Add(EInsuranceTier::None, 0.f);
    TierBasePrice.Add(EInsuranceTier::Basic, 100.f);
    TierBasePrice.Add(EInsuranceTier::Standard, 250.f);
    TierBasePrice.Add(EInsuranceTier::Gold, 500.f);
    TierBasePrice.Add(EInsuranceTier::Platinum, 1000.f);
    TierBasePrice.Add(EInsuranceTier::Fleet, 2000.f);

    CoverageMultiplier.Empty();
    CoverageMultiplier.Add(EInsuranceCoverage::HullOnly, 1.0f);
    CoverageMultiplier.Add(EInsuranceCoverage::HullAndCargo, 1.3f);
    CoverageMultiplier.Add(EInsuranceCoverage::FullCoverage, 1.6f);
    CoverageMultiplier.Add(EInsuranceCoverage::CombatCoverage, 2.0f);
    CoverageMultiplier.Add(EInsuranceCoverage::PiracyLoss, 2.5f);

    VehicleTypeMultiplier.Empty();
    VehicleTypeMultiplier.Add(EVehicleType::Spaceship, 1.0f);
    VehicleTypeMultiplier.Add(EVehicleType::GroundVehicle, 0.5f);
    VehicleTypeMultiplier.Add(EVehicleType::HoverBike, 0.3f);
    VehicleTypeMultiplier.Add(EVehicleType::Rover, 0.4f);
    VehicleTypeMultiplier.Add(EVehicleType::DropShip, 1.5f);

    VehicleBaseValues.Add(FName(TEXT("Ship_Scout")), 5000.f);
    VehicleBaseValues.Add(FName(TEXT("Ship_Fighter")), 12000.f);
    VehicleBaseValues.Add(FName(TEXT("Ship_Hauler")), 18000.f);
    VehicleBaseValues.Add(FName(TEXT("Ship_Explorer")), 25000.f);
    VehicleBaseValues.Add(FName(TEXT("Ship_Capital")), 100000.f);
    VehicleBaseValues.Add(FName(TEXT("Rover_Standard")), 2000.f);
    VehicleBaseValues.Add(FName(TEXT("HoverBike")), 800.f);
    VehicleBaseValues.Add(FName(TEXT("DropShip_Assault")), 35000.f);
}

// ========== 保单管理 ==========

void UInsuranceManager::Server_PurchaseInsurance_Implementation(AController* Player, FName VehicleID, EInsuranceTier Tier, EInsuranceCoverage Coverage, FName HangarID)
{
    if (!Player || Tier == EInsuranceTier::None) return;

    FString PlayerID = Player->GetName();

    for (const auto& Pair : AllPolicies)
    {
        if (Pair.Value.VehicleID == VehicleID) return; // 已有保单
    }

    FInsurancePolicy NewPolicy;
    NewPolicy.PolicyID = GeneratePolicyID();
    NewPolicy.VehicleID = VehicleID;
    NewPolicy.VehicleType = EVehicleType::Spaceship;
    NewPolicy.VehicleDisplayName = VehicleID.ToString();
    NewPolicy.VehicleModelID = VehicleID;
    NewPolicy.Tier = Tier;
    NewPolicy.Coverage = Coverage;
    NewPolicy.PolicyExpiryTime = GetWorld() ? GetWorld()->GetTimeSeconds() + 90.f * 24.f * 3600.f : 7776000.f;
    NewPolicy.DeductiblePercent = (Tier == EInsuranceTier::Platinum) ? 0.f :
                                 (Tier == EInsuranceTier::Gold) ? 0.02f :
                                 (Tier == EInsuranceTier::Standard) ? 0.05f : 0.1f;
    NewPolicy.MonthlyPremium = CalculatePremium(VehicleID, Tier, Coverage);
    NewPolicy.LastPaymentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    NewPolicy.ClaimCount = 0;
    NewPolicy.MaxClaims = (Tier == EInsuranceTier::Platinum || Tier == EInsuranceTier::Fleet) ? 0 : 5;
    NewPolicy.bVehicleDestroyed = false;
    NewPolicy.HomeHangarID = HangarID;
    NewPolicy.bAutoRenew = true;
    NewPolicy.bCargoInsured = (Coverage == EInsuranceCoverage::HullAndCargo ||
                              Coverage == EInsuranceCoverage::FullCoverage);

    // ★ 尝试从当前飞船获取配置
    if (AShipPawn* Ship = Cast<AShipPawn>(Player->GetPawn()))
    {
        NewPolicy.SavedConfiguration = Ship->GetCurrentConfig();
    }

    AllPolicies.Add(NewPolicy.PolicyID, NewPolicy);

    OnPolicyPurchased.Broadcast(Player, NewPolicy.PolicyID);
}

void UInsuranceManager::Server_UpgradeInsurance_Implementation(AController* Player, FName PolicyID, EInsuranceTier NewTier)
{
    if (!AllPolicies.Contains(PolicyID)) return;

    FInsurancePolicy& Policy = AllPolicies[PolicyID];
    Policy.Tier = NewTier;

    switch (NewTier)
    {
    case EInsuranceTier::Platinum: Policy.DeductiblePercent = 0.f; break;
    case EInsuranceTier::Gold:     Policy.DeductiblePercent = 0.02f; break;
    case EInsuranceTier::Standard: Policy.DeductiblePercent = 0.05f; break;
    case EInsuranceTier::Basic:    Policy.DeductiblePercent = 0.1f; break;
    default: break;
    }

    Policy.MonthlyPremium = CalculatePremium(Policy.VehicleModelID, NewTier, Policy.Coverage);
}

void UInsuranceManager::Server_RenewPolicy_Implementation(AController* Player, FName PolicyID)
{
    if (!AllPolicies.Contains(PolicyID) || !GetWorld()) return;

    FInsurancePolicy& Policy = AllPolicies[PolicyID];
    float CurrentTime = GetWorld()->GetTimeSeconds();
    Policy.PolicyExpiryTime = CurrentTime + 30.f * 24.f * 3600.f;
    Policy.LastPaymentTime = CurrentTime;
}

void UInsuranceManager::Server_CancelPolicy_Implementation(AController* Player, FName PolicyID)
{
    AllPolicies.Remove(PolicyID);
}

void UInsuranceManager::Server_UpdatePolicyConfig_Implementation(AController* Player, FName PolicyID, const FShipSavedConfig& NewConfig)
{
    if (!AllPolicies.Contains(PolicyID)) return;
    AllPolicies[PolicyID].SavedConfiguration = NewConfig;
}

TArray<FInsurancePolicy> UInsuranceManager::GetPlayerPolicies(AController* Player) const
{
    TArray<FInsurancePolicy> Result;
    if (!Player) return Result;

    FString PlayerID = Player->GetName();

    for (const auto& Pair : AllPolicies)
    {
        if (VehicleOwners.Contains(Pair.Value.VehicleID))
        {
            if (VehicleOwners[Pair.Value.VehicleID] == FName(*PlayerID))
            {
                Result.Add(Pair.Value);
            }
        }
    }
    return Result;
}

FInsurancePolicy UInsuranceManager::GetPolicyForVehicle(FName VehicleID) const
{
    for (const auto& Pair : AllPolicies)
    {
        if (Pair.Value.VehicleID == VehicleID)
        {
            return Pair.Value;
        }
    }
    return FInsurancePolicy();
}

// ========== 索赔(关键修复) ==========

void UInsuranceManager::Server_FileClaim_Implementation(AController* Player, FName PolicyID, const FString& Reason, AShipPawn* DestroyedShip)
{
    if (!Player || !AllPolicies.Contains(PolicyID)) return;

    FInsurancePolicy& Policy = AllPolicies[PolicyID];

    if (!IsEligibleForClaim(Policy)) return;

    // ★ 从被毁飞船提取最新配置
    if (DestroyedShip)
    {
        Policy.SavedConfiguration = DestroyedShip->GetCurrentConfig();
    }

    FInsuranceClaim Claim;
    Claim.ClaimID = GenerateClaimID();
    Claim.PolicyID = PolicyID;
    Claim.VehicleID = Policy.VehicleID;
    Claim.PlayerID = FName(*Player->GetName());
    Claim.ClaimReason = Reason;
    Claim.ClaimStatus = TEXT("Pending");
    Claim.ProcessingTime = CalculateClaimProcessingTime(Policy, false);
    Claim.ElapsedTime = 0.f;
    Claim.DeductibleAmount = CalculateDeductible(Policy, GetVehicleValue(Policy.VehicleModelID));
    Claim.ReplacementCost = GetVehicleValue(Policy.VehicleModelID);
    Claim.DeliveryHangarID = Policy.HomeHangarID;
    Claim.bExpedited = false;
    Claim.ExpediteFee = 0.f;

    // ★ 保存配置到索赔(用于生成新船)
    Claim.ReplacementConfig = Policy.SavedConfiguration;

    // ★ 记录原船信息(用于失效)
    if (DestroyedShip)
    {
        Claim.OldShipID = FName(*DestroyedShip->GetName());
    }
    Claim.bOldShipInvalidated = false;

    Policy.bVehicleDestroyed = true;
    Policy.ClaimCount++;

    PendingClaims.Add(Claim);

    OnClaimFiled.Broadcast(Player, Claim.ClaimID, TEXT("Pending"));
}

void UInsuranceManager::Server_FileExpeditedClaim_Implementation(AController* Player, FName PolicyID, float ExpediteFee)
{
    if (!Player || !AllPolicies.Contains(PolicyID)) return;

    // 先提交普通索赔(传入nullptr, 后续会查找)
    Server_FileClaim_Implementation(Player, PolicyID, TEXT("Expedited Claim"), nullptr);

    for (FInsuranceClaim& Claim : PendingClaims)
    {
        if (Claim.PolicyID == PolicyID && Claim.ClaimStatus == TEXT("Pending"))
        {
            Claim.bExpedited = true;
            Claim.ExpediteFee = ExpediteFee;
            Claim.ProcessingTime = CalculateClaimProcessingTime(GetPolicyForVehicle(Claim.VehicleID), true);
            break;
        }
    }
}

void UInsuranceManager::Server_RecoverVehicle_Implementation(AController* Player, FName VehicleID, FName DestinationHangar)
{
    if (!Player) return;

    for (auto& Pair : AllPolicies)
    {
        if (Pair.Value.VehicleID == VehicleID)
        {
            Pair.Value.bVehicleMissing = false;
            Pair.Value.bVehicleDestroyed = false;
            Pair.Value.bVehicleStolen = false;
            break;
        }
    }

    OnVehicleRecovered.Broadcast(Player, VehicleID);
}

void UInsuranceManager::ProcessClaims(float DeltaTime)
{
    if (PendingClaims.Num() == 0) return;

    TArray<int32> CompletedIndices;

    for (int32 i = 0; i < PendingClaims.Num(); ++i)
    {
        FInsuranceClaim& Claim = PendingClaims[i];
        Claim.ElapsedTime += DeltaTime;

        float Progress = Claim.ElapsedTime / Claim.ProcessingTime;
        if (Progress >= 1.f)
        {
            CompleteClaim(Claim);
            CompletedIndices.Add(i);
        }
    }

    for (int32 idx = CompletedIndices.Num() - 1; idx >= 0; --idx)
    {
        PendingClaims.RemoveAt(CompletedIndices[idx]);
    }
}

void UInsuranceManager::CompleteClaim(FInsuranceClaim& Claim)
{
    Claim.ClaimStatus = TEXT("Completed");

    // ★ 关键: 将索赔加入待生成队列(由 GameMode 处理生成新船)
    ReadyToSpawnClaims.Add(Claim);

    // 重置保单状态
    if (AllPolicies.Contains(Claim.PolicyID))
    {
        FInsurancePolicy& Policy = AllPolicies[Claim.PolicyID];
        Policy.bVehicleDestroyed = false;
    }

    OnClaimCompleted.Broadcast(nullptr, Claim.ClaimID, Claim.VehicleID);
}

void UInsuranceManager::ProcessReadyToSpawnClaims()
{
    if (ReadyToSpawnClaims.Num() == 0) return;

    AStellarGameMode* GM = Cast<AStellarGameMode>(GetWorld()->GetAuthGameMode());
    if (!GM) return;

    TArray<int32> ProcessedIndices;

    for (int32 i = 0; i < ReadyToSpawnClaims.Num(); ++i)
    {
        FInsuranceClaim& Claim = ReadyToSpawnClaims[i];

        // 通知 GameMode 生成新船
        OnClaimReadyToSpawn.Broadcast(
            Claim.ClaimID,
            Claim.PlayerID,
            Claim.ReplacementConfig,
            Claim.DeliveryLocation
        );

        ProcessedIndices.Add(i);
    }

    for (int32 idx = ProcessedIndices.Num() - 1; idx >= 0; --idx)
    {
        ReadyToSpawnClaims.RemoveAt(ProcessedIndices[idx]);
    }
}

TArray<FInsuranceClaim> UInsuranceManager::GetPlayerPendingClaims(AController* Player) const
{
    TArray<FInsuranceClaim> Result;
    if (!Player) return Result;

    FName PlayerID(*Player->GetName());

    for (const FInsuranceClaim& Claim : PendingClaims)
    {
        if (Claim.PlayerID == PlayerID)
        {
            Result.Add(Claim);
        }
    }
    return Result;
}

TArray<FInsuranceClaim> UInsuranceManager::GetAllPendingClaims() const
{
    return PendingClaims;
}

// ========== 载具注册(增强: 保存初始配置) ==========

void UInsuranceManager::Server_RegisterVehicle_Implementation(AController* Player, FName VehicleID, FName ModelID, EVehicleType Type, FName HangarID, const FShipSavedConfig& InitialConfig)
{
    if (!Player) return;

    FString PlayerID = Player->GetName();
    VehicleOwners.Add(VehicleID, FName(*PlayerID));

    float BaseValue = 5000.f;
    switch (Type)
    {
    case EVehicleType::Spaceship:    BaseValue = 10000.f; break;
    case EVehicleType::GroundVehicle: BaseValue = 3000.f; break;
    case EVehicleType::HoverBike:    BaseValue = 1000.f; break;
    case EVehicleType::Rover:        BaseValue = 2500.f; break;
    case EVehicleType::DropShip:     BaseValue = 30000.f; break;
    default: break;
    }
    VehicleBaseValues.Add(VehicleID, BaseValue);

    // 创建默认保单(如果不存在)
    bool bHasPolicy = false;
    for (const auto& Pair : AllPolicies)
    {
        if (Pair.Value.VehicleID == VehicleID) { bHasPolicy = true; break; }
    }

    if (!bHasPolicy)
    {
        FInsurancePolicy DefaultPolicy;
        DefaultPolicy.PolicyID = GeneratePolicyID();
        DefaultPolicy.VehicleID = VehicleID;
        DefaultPolicy.VehicleType = Type;
        DefaultPolicy.VehicleModelID = ModelID;
        DefaultPolicy.VehicleDisplayName = VehicleID.ToString();
        DefaultPolicy.Tier = EInsuranceTier::Basic;
        DefaultPolicy.Coverage = EInsuranceCoverage::HullOnly;
        DefaultPolicy.SavedConfiguration = InitialConfig;
        DefaultPolicy.HomeHangarID = HangarID;
        DefaultPolicy.bAutoRenew = true;

        AllPolicies.Add(DefaultPolicy.PolicyID, DefaultPolicy);
    }
}

void UInsuranceManager::Server_UnregisterVehicle_Implementation(AController* Player, FName VehicleID)
{
    VehicleOwners.Remove(VehicleID);
    VehicleBaseValues.Remove(VehicleID);

    TArray<FName> ToRemove;
    for (const auto& Pair : AllPolicies)
    {
        if (Pair.Value.VehicleID == VehicleID)
        {
            ToRemove.Add(Pair.Key);
        }
    }
    for (FName PID : ToRemove)
    {
        AllPolicies.Remove(PID);
    }
}

void UInsuranceManager::Server_TransferVehicle_Implementation(AController* Player, FName VehicleID, FName NewOwnerID)
{
    if (VehicleOwners.Contains(VehicleID))
    {
        VehicleOwners[VehicleID] = NewOwnerID;
    }
}

// ========== 自动检测 ==========

void UInsuranceManager::Server_OnVehicleDestroyed_Implementation(FName VehicleID, const FVector& Location, const FString& Reason)
{
    for (auto& Pair : AllPolicies)
    {
        if (Pair.Value.VehicleID == VehicleID)
        {
            Pair.Value.bVehicleDestroyed = true;
            Pair.Value.DestructionLocation = Location;
            Pair.Value.DestructionReason = Reason;

            // 自动提交索赔
            if (VehicleOwners.Contains(VehicleID))
            {
                FName OwnerID = VehicleOwners[VehicleID];

                FInsuranceClaim AutoClaim;
                AutoClaim.ClaimID = GenerateClaimID();
                AutoClaim.PolicyID = Pair.Key;
                AutoClaim.VehicleID = VehicleID;
                AutoClaim.PlayerID = OwnerID;
                AutoClaim.ClaimReason = Reason;
                AutoClaim.ClaimStatus = TEXT("Processing");
                AutoClaim.ProcessingTime = CalculateClaimProcessingTime(Pair.Value, false);
                AutoClaim.DeductibleAmount = CalculateDeductible(Pair.Value, GetVehicleValue(Pair.Value.VehicleModelID));
                AutoClaim.ReplacementCost = GetVehicleValue(Pair.Value.VehicleModelID);
                AutoClaim.DeliveryHangarID = Pair.Value.HomeHangarID;
                AutoClaim.ElapsedTime = 0.f;
                // ★ 保存配置(即使飞船已毁, 配置在保单中)
                AutoClaim.ReplacementConfig = Pair.Value.SavedConfiguration;

                PendingClaims.Add(AutoClaim);
                Pair.Value.ClaimCount++;
            }
            break;
        }
    }
}

void UInsuranceManager::Server_OnVehicleMissing_Implementation(FName VehicleID, float MissingDuration)
{
    const float MissingThreshold = 300.f;
    if (MissingDuration >= MissingThreshold)
    {
        for (auto& Pair : AllPolicies)
        {
            if (Pair.Value.VehicleID == VehicleID)
            {
                Pair.Value.bVehicleMissing = true;
                break;
            }
        }
    }
}

// ========== 费用计算 ==========

float UInsuranceManager::CalculatePremium(FName VehicleModelID, EInsuranceTier Tier, EInsuranceCoverage Coverage) const
{
    float BasePrice = 100.f;
    if (TierBasePrice.Contains(Tier)) BasePrice = TierBasePrice[Tier];

    float CoverageMult = 1.f;
    if (CoverageMultiplier.Contains(Coverage)) CoverageMult = CoverageMultiplier[Coverage];

    float VehicleValue = GetVehicleValue(VehicleModelID);
    float ValueMult = FMath::Sqrt(VehicleValue / 10000.f);

    return BasePrice * CoverageMult * ValueMult;
}

float UInsuranceManager::CalculateDeductible(const FInsurancePolicy& Policy, float VehicleValue) const
{
    return VehicleValue * Policy.DeductiblePercent;
}

float UInsuranceManager::CalculateClaimProcessingTime(const FInsurancePolicy& Policy, bool bExpedited) const
{
    if (bExpedited) return ExpediteProcessingTime;

    switch (Policy.Tier)
    {
    case EInsuranceTier::Platinum: return BaseProcessingTime * 0.3f;
    case EInsuranceTier::Gold:     return BaseProcessingTime * 0.5f;
    case EInsuranceTier::Standard: return BaseProcessingTime * 0.8f;
    case EInsuranceTier::Fleet:    return BaseProcessingTime * 0.4f;
    default:                       return BaseProcessingTime;
    }
}

// ========== 辅助函数 ==========

FName UInsuranceManager::GeneratePolicyID() const
{
    static int32 Counter = 0;
    Counter++;
    return FName(*FString::Printf(TEXT("POL_%08d_%d"), FMath::RandRange(10000000, 99999999), Counter));
}

FName UInsuranceManager::GenerateClaimID() const
{
    static int32 Counter = 0;
    Counter++;
    return FName(*FString::Printf(TEXT("CLM_%08d_%d"), FMath::RandRange(10000000, 99999999), Counter));
}

bool UInsuranceManager::IsPolicyValid(const FInsurancePolicy& Policy) const
{
    if (GetWorld() == nullptr) return true;
    float CurrentTime = GetWorld()->GetTimeSeconds();
    return (Policy.PolicyExpiryTime == 0.f) || (CurrentTime < Policy.PolicyExpiryTime);
}

bool UInsuranceManager::IsEligibleForClaim(const FInsurancePolicy& Policy) const
{
    if (!IsPolicyValid(Policy)) return false;
    if (Policy.bVehicleDestroyed) return false;
    if (Policy.MaxClaims > 0 && Policy.ClaimCount >= Policy.MaxClaims) return false;
    return true;
}

float UInsuranceManager::GetVehicleValue(FName ModelID) const
{
    if (VehicleBaseValues.Contains(ModelID)) return VehicleBaseValues[ModelID];
    return 10000.f;
}
