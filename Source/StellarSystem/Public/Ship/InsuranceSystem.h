// ============================================================
// 路径: Source/StellarSystem/Public/Ship/InsuranceSystem.h
// 作用: 飞船与地面载具保险及索赔系统
// 修改于: v7.6 (索赔按配置重建/原船失效/货物转移)
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "InsuranceSystem.generated.h"

class AShipPawn;
class AMyCharacter;
class APlanetarySpaceport;
class UCargoComponent;
struct FShipSavedConfig;

// 保险等级
UENUM(BlueprintType)
enum class EInsuranceTier : uint8
{
    None          UMETA(DisplayName = "No Insurance"),
    Basic         UMETA(DisplayName = "Basic (90 day, 10% deductible)"),
    Standard      UMETA(DisplayName = "Standard (6 month, 5% deductible)"),
    Gold          UMETA(DisplayName = "Gold (1 year, 2% deductible)"),
    Platinum      UMETA(DisplayName = "Platinum (Lifetime, 0% deductible)"),
    Fleet         UMETA(DisplayName = "Fleet (Org-wide coverage)")
};

// 保险覆盖类型
UENUM(BlueprintType)
enum class EInsuranceCoverage : uint8
{
    HullOnly       UMETA(DisplayName = "Hull Only"),
    HullAndCargo   UMETA(DisplayName = "Hull + Cargo"),
    FullCoverage   UMETA(DisplayName = "Full (Hull + Cargo + Equipment)"),
    CombatCoverage UMETA(DisplayName = "Combat (PvP loss covered)"),
    PiracyLoss     UMETA(DisplayName = "Piracy Loss (PvP killed by pirate)")
};

// 载具类型
UENUM(BlueprintType)
enum class EVehicleType : uint8
{
    Spaceship      UMETA(DisplayName = "Spaceship"),
    GroundVehicle UMETA(DisplayName = "Ground Vehicle"),
    HoverBike    UMETA(DisplayName = "Hover Bike"),
    Rover         UMETA(DisplayName = "Rover"),
    DropShip      UMETA(DisplayName = "Drop Ship"),
    MAX
};

// 单艘载具的保险单(增强: 保存飞船配置引用)
USTRUCT(BlueprintType)
struct FInsurancePolicy
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FName PolicyID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FName VehicleID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    EVehicleType VehicleType = EVehicleType::Spaceship;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FString VehicleDisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FName VehicleModelID;

    // ★ 关键: 保存飞船配置(索赔时按此重建)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FShipSavedConfig SavedConfiguration;

    // 保险详情
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    EInsuranceTier Tier = EInsuranceTier::Basic;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    EInsuranceCoverage Coverage = EInsuranceCoverage::HullOnly;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float PolicyExpiryTime = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float DeductiblePercent = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float MonthlyPremium = 50.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float LastPaymentTime = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    int32 ClaimCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    int32 MaxClaims = 0;

    // 载具当前状态
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    bool bVehicleDestroyed = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    bool bVehicleStolen = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    bool bVehicleMissing = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FVector DestructionLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FString DestructionReason;

    // 关联的机库
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FName HomeHangarID = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    bool bAutoRenew = true;

    // ★ 新增: 货物保险
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    bool bCargoInsured = false; // 货物是否投保

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float CargoCoverageLimit = 10000.f; // 货物保险上限
};

// 索赔请求(增强: 新船生成参数)
USTRUCT(BlueprintType)
struct FInsuranceClaim
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName ClaimID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName PolicyID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName VehicleID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName PlayerID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ClaimReason;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ClaimStatus; // Pending/Approved/Denied/Processing/Delivered

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ProcessingTime = 120.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ElapsedTime = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DeductibleAmount = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ReplacementCost = 0.f;

    // ★ 新增: 新船生成信息
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName DeliveryHangarID = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector DeliveryLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FShipSavedConfig ReplacementConfig; // 新船配置

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bExpedited = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ExpediteFee = 0.f;

    // ★ 新增: 原船失效信息
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName OldShipID = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bOldShipInvalidated = false;
};

// 保险管理器(GameState 子系统)
UCLASS(BlueprintType)
class UInsuranceManager : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;

    // ========== 保单管理 ==========

    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Insurance")
    void Server_PurchaseInsurance(AController* Player, FName VehicleID, EInsuranceTier Tier, EInsuranceCoverage Coverage, FName HangarID);

    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Insurance")
    void Server_UpgradeInsurance(AController* Player, FName PolicyID, EInsuranceTier NewTier);

    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Insurance")
    void Server_RenewPolicy(AController* Player, FName PolicyID);

    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Insurance")
    void Server_CancelPolicy(AController* Player, FName PolicyID);

    // ★ 新增: 更新保单中的飞船配置(玩家改装后调用)
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Insurance|Config")
    void Server_UpdatePolicyConfig(AController* Player, FName PolicyID, const FShipSavedConfig& NewConfig);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Insurance")
    TArray<FInsurancePolicy> GetPlayerPolicies(AController* Player) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Insurance")
    FInsurancePolicy GetPolicyForVehicle(FName VehicleID) const;

    // ========== 索赔(增强) ==========

    // ★ 关键修复: 提交索赔时传入飞船配置
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Insurance|Claim")
    void Server_FileClaim(AController* Player, FName PolicyID, const FString& Reason, AShipPawn* DestroyedShip);

    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Insurance|Claim")
    void Server_FileExpeditedClaim(AController* Player, FName PolicyID, float ExpediteFee);

    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Insurance|Claim")
    void Server_RecoverVehicle(AController* Player, FName VehicleID, FName DestinationHangar);

    void ProcessClaims(float DeltaTime);

    // ★ 关键修复: 完成索赔 → 按配置生成新船
    void CompleteClaim(FInsuranceClaim& Claim);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Insurance|Claim")
    TArray<FInsuranceClaim> GetPlayerPendingClaims(AController* Player) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Insurance|Claim")
    TArray<FInsuranceClaim> GetAllPendingClaims() const;

    // ========== 费用计算 ==========

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Insurance|Pricing")
    float CalculatePremium(FName VehicleModelID, EInsuranceTier Tier, EInsuranceCoverage Coverage) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Insurance|Pricing")
    float CalculateDeductible(const FInsurancePolicy& Policy, float VehicleValue) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Insurance|Pricing")
    float CalculateClaimProcessingTime(const FInsurancePolicy& Policy, bool bExpedited) const;

    // ========== 载具注册 ==========

    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Insurance|Vehicle")
    void Server_RegisterVehicle(AController* Player, FName VehicleID, FName ModelID, EVehicleType Type, FName HangarID, const FShipSavedConfig& InitialConfig);

    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Insurance|Vehicle")
    void Server_UnregisterVehicle(AController* Player, FName VehicleID);

    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Insurance|Vehicle")
    void Server_TransferVehicle(AController* Player, FName VehicleID, FName NewOwnerID);

    // ========== 自动检测 ==========

    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Insurance|Auto")
    void Server_OnVehicleDestroyed(FName VehicleID, const FVector& Location, const FString& Reason);

    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Insurance|Auto")
    void Server_OnVehicleMissing(FName VehicleID, float MissingDuration);

    // ========== 定价表 ==========
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insurance|Pricing")
    TMap<EInsuranceTier, float> TierBasePrice;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insurance|Pricing")
    TMap<EInsuranceCoverage, float> CoverageMultiplier;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insurance|Pricing")
    TMap<EVehicleType, float> VehicleTypeMultiplier;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insurance|Pricing")
    float BaseProcessingTime = 120.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insurance|Pricing")
    float ExpediteProcessingTime = 30.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insurance|Pricing")
    float ExpediteFeeMultiplier = 0.5f;

    // ========== 事件 ==========
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPolicyPurchased, AController*, Player, FName, PolicyID);
    UPROPERTY(BlueprintAssignable, Category = "Insurance|Events")
    FOnPolicyPurchased OnPolicyPurchased;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnClaimFiled, AController*, Player, FName, ClaimID, FString, Status);
    UPROPERTY(BlueprintAssignable, Category = "Insurance|Events")
    FOnClaimFiled OnClaimFiled;

    // ★ 新增: 索赔完成 → 通知 GameMode 生成新船
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnClaimReadyToSpawn, FName, ClaimID, FName, PlayerID, FShipSavedConfig, Config, FVector, SpawnLocation);
    UPROPERTY(BlueprintAssignable, Category = "Insurance|Events")
    FOnClaimReadyToSpawn OnClaimReadyToSpawn;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnClaimCompleted, AController*, Player, FName, ClaimID, FName, VehicleID);
    UPROPERTY(BlueprintAssignable, Category = "Insurance|Events")
    FOnClaimCompleted OnClaimCompleted;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVehicleRecovered, AController*, Player, FName, VehicleID);
    UPROPERTY(BlueprintAssignable, Category = "Insurance|Events")
    FOnVehicleRecovered OnVehicleRecovered;

private:
    UPROPERTY()
    TMap<FName, FInsurancePolicy> AllPolicies;

    UPROPERTY()
    TArray<FInsuranceClaim> PendingClaims;

    UPROPERTY()
    TMap<FName, FName> VehicleOwners;

    UPROPERTY()
    TMap<FName, float> VehicleBaseValues;

    // ★ 新增: 待生成的新船队列
    UPROPERTY()
    TArray<FInsuranceClaim> ReadyToSpawnClaims;

    FName GeneratePolicyID() const;
    FName GenerateClaimID() const;

    bool IsPolicyValid(const FInsurancePolicy& Policy) const;
    bool IsEligibleForClaim(const FInsurancePolicy& Policy) const;

    void InitializeDefaultPricing();

    float GetVehicleValue(FName ModelID) const;

    // ★ 新增: 从飞船提取配置到保单
    void CaptureShipConfig(AShipPawn* Ship, FInsurancePolicy& Policy);

    // ★ 新增: 处理待生成队列
    void ProcessReadyToSpawnClaims();
};
