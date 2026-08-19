// ============================================================
// 路径: Source/StellarSystem/Public/Ship/ShipTractorBeam.h
// 模块: Ship (飞船武器)
// 类型: 头文件
// 作用: 牵引光束武器 — 发射可锁定目标的牵引波束，
//       将友方物体拉近（回收货物/牵引队友）或
//       将敌方飞船减速并拉近（战术控制）
// 新增于: v7.6.1
// 依赖: ShipWeaponBase.h, ShipPawn.h
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "ShipWeapons.h"
#include "ShipTractorBeam.generated.h"

class AShipPawn;
class UParticleSystem;
class USoundBase;

// 牵引光束模式
UENUM(BlueprintType)
enum class ETractorMode : uint8
{
    Pull        UMETA(DisplayName = "牵引拉回 (Pull)"),
    Push        UMETA(DisplayName = "反向推离 (Push)"),
    Stabilize   UMETA(DisplayName = "稳定悬停 (Stabilize)"),
    Tow         UMETA(DisplayName = "拖曳牵引 (Tow)")
};

// 牵引光束伤害/效果属性
USTRUCT(BlueprintType)
struct FTractorEffectProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam")
    float PullForce = 8000.f;              // 牵引力 (cm/s²)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam")
    float PushForce = 5000.f;              // 推力 (cm/s²)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam")
    float MaxPullSpeed = 1500.f;           // 最大牵引速度 (cm/s)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam")
    float StabilizeDamping = 0.95f;        // 稳定模式阻尼系数

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam")
    float TowBreakDistance = 800.f;         // 拖曳断裂距离 (cm)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam")
    float MinTargetMass = 0.f;             // 最小可牵引质量 (kg)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam")
    float MaxTargetMass = 500000.f;         // 最大可牵引质量 (kg)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam")
    bool bAffectsShips = true;             // 是否可牵引飞船

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam")
    bool bAffectsCargo = true;             // 是否可牵引货物/碎片

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam")
    bool bAffectsDebris = true;            // 是否可牵引残骸

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam")
    float EnemySlowFactor = 0.4f;          // 对敌方的减速系数 (0~1)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam")
    bool bCanGrapEnemyShips = false;       // 能否牵引敌方飞船 (false=仅友方)
};

// 牵引光束运行参数
USTRUCT(BlueprintType)
struct FTractorBeamRuntime
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tractor Beam")
    bool bBeamActive = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tractor Beam")
    AActor* TargetActor = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tractor Beam")
    float BeamDuration = 0.f;              // 当前波束持续时间

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tractor Beam")
    float CurrentPullDistance = 0.f;       // 当前牵引距离

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tractor Beam")
    ETractorMode CurrentMode = ETractorMode::Pull;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tractor Beam")
    float EnergyConsumptionRate = 0.f;     // 当前能耗速率

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tractor Beam")
    float HeatAccumulation = 0.f;          // 热量积累
};

/**
 * 牵引光束武器组件
 * 
 * 功能：
 * - 持续波束（非弹道），按住开火持续牵引
 * - 4 种模式：Pull / Push / Stabilize / Tow
 * - 质量限制：超出目标质量范围则无效
 * - 友方/敌方区分：默认仅友方可被牵引
 * - 能耗+过热管理（同能量武器）
 * - 货物回收：牵引散落货物到货舱
 * - 战术控制：减速敌方飞船
 */
UCLASS(ClassGroup=(Ship|Weapons), meta=(BlueprintSpawnableComponent))
class UShipTractorBeamComponent : public UShipWeaponBaseComponent
{
    GENERATED_BODY()

public:
    UShipTractorBeamComponent();

    virtual void TickComponent(float Dt, ELevelTick Tick, FActorComponentTickFunction* Fn) override;
    virtual void BeginPlay() override;

    // ========== 基础属性 ==========

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam")
    float BeamRange = 30000.f;             // 波束最大射程 (cm) = 300m

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam")
    float BeamWidth = 80.f;                // 波束锥角宽度 (cm)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam")
    float MaxBeamLength = 30000.f;         // 最大波束长度

    // ========== 能量管理 ==========

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam|Energy")
    float MaxEnergy = 300.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tractor Beam|Energy")
    float CurrentEnergy = 300.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam|Energy")
    float EnergyDrainPerSecond = 18.f;     // 持续开火能耗

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam|Energy")
    float EnergyRegenPerSecond = 22.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam|Energy")
    float OverheatThreshold = 100.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tractor Beam|Energy")
    float CurrentHeat = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam|Energy")
    float HeatPerSecond = 10.f;            // 持续开火产热

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam|Energy")
    float HeatDissipationRate = 15.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam|Energy")
    bool bOverheated = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam|Energy")
    float OverheatCooldown = 4.f;

    // ========== 效果配置 ==========

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam|Effects")
    FTractorEffectProfile EffectProfile;

    // ========== 视觉效果 ==========

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam|VFX")
    TSoftObjectPtr<UParticleSystem> BeamParticle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam|VFX")
    TSoftObjectPtr<UParticleSystem> ConnectParticle;    // 目标连接点特效

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam|VFX")
    TSoftObjectPtr<UParticleSystem> ImpactParticle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam|VFX")
    FLinearColor BeamColor = FLinearColor(0.1f, 0.9f, 0.3f, 0.7f);  // 绿色牵引光束

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam|VFX")
    FLinearColor PushBeamColor = FLinearColor(0.9f, 0.3f, 0.1f, 0.7f); // 红色推斥光束

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam|VFX")
    TSoftObjectPtr<USoundBase> BeamActiveSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam|VFX")
    TSoftObjectPtr<USoundBase> BeamEndSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam|VFX")
    TSoftObjectPtr<USoundBase> OverheatSound;

    // ========== 运行实例 ==========

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tractor Beam|Runtime")
    FTractorBeamRuntime Runtime;

    // ========== 模式控制 ==========

    UFUNCTION(BlueprintCallable, Category = "Tractor Beam")
    void SetTractorMode(ETractorMode NewMode);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Tractor Beam")
    ETractorMode GetCurrentMode() const { return Runtime.CurrentMode; }

    // ========== 开火接口 ==========

    /** 开始发射牵引光束（持续） */
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Tractor Beam")
    void Server_StartTractorBeam(ETractorMode Mode);

    /** 停止发射 */
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Tractor Beam")
    void Server_StopTractorBeam();

    /** 尝试锁定目标（自动选择最近可牵引物体） */
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Tractor Beam")
    void Server_AcquireTractorTarget();

    /** 释放当前目标 */
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Tractor Beam")
    void Server_ReleaseTractorTarget();

    // ========== 查询 ==========

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Tractor Beam")
    bool IsBeamActive() const { return Runtime.bBeamActive; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Tractor Beam")
    AActor* GetTractorTarget() const { return Runtime.TargetActor; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Tractor Beam")
    float GetBeamEnergyPercent() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Tractor Beam")
    float GetBeamHeatPercent() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Tractor Beam")
    bool CanActivateBeam() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Tractor Beam")
    TArray<AActor*> GetValidTractorTargets() const;

    // ========== 过载模式 ==========

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam|Overcharge")
    bool bOverchargeMode = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam|Overcharge")
    float OverchargePullMultiplier = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam|Overcharge")
    float OverchargeEnergyMultiplier = 2.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tractor Beam|Overcharge")
    float OverchargeHeatMultiplier = 3.0f;

    UFUNCTION(BlueprintCallable, Category = "Tractor Beam")
    void SetOvercharge(bool bEnabled);

    // ========== 货物回收 ==========

    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Tractor Beam|Cargo")
    void Server_TractorRetrieveCargo(AActor* CargoActor);

    /** 自动搜索并回收范围内的散落货物 */
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Tractor Beam|Cargo")
    void Server_AutoRetrieveNearbyCargo();

    // ========== 网络复制 ==========

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

private:
    // 内部更新
    void UpdateBeam(float Dt);
    void UpdateEnergy(float Dt);
    void UpdateHeat(float Dt);
    void ProcessTractorEffect(float Dt);
    void ApplyPullForce(float Dt, AActor* Target);
    void ApplyPushForce(float Dt, AActor* Target);
    void ApplyStabilize(float Dt, AActor* Target);
    void ApplyTow(float Dt, AActor* Target);
    void CheckBeamRange();
    void SpawnBeamEffects();
    void DestroyBeamEffects();
    bool ValidateTarget(AActor* Target) const;
    float CalculateMassScaling(AActor* Target) const;

    // 计时器
    FTimerHandle OverheatTimerHandle;
    FTimerHandle AutoRetrieveTimerHandle;

    // 特效实例
    UPROPERTY()
    class UParticleSystemComponent* ActiveBeamPSC = nullptr;

    UPROPERTY()
    class UAudioComponent* ActiveBeamAudio = nullptr;
};
