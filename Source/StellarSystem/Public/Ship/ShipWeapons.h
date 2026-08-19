// ShipWeapons.h
// 飞船武器系统：开火/锁定/弹药消耗/伤害结算（引用弹药系统）
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShipWeapons.generated.h"

class AShipPawn;
class UAmmoInventoryComponent;
struct FAmmoParameters;

UENUM(BlueprintType)
enum class EShipWeaponType : uint8
{
    Laser, Plasma, RailGun, Missile, Torpedo,
    Flak, Beam, Pulse, MineLayer,
    TractorBeam  UMETA(DisplayName = "牵引光束 (Tractor Beam)")
};

USTRUCT(BlueprintType)
struct FShipWeaponSlot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName WeaponID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EShipWeaponType Type = EShipWeaponType::Laser;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName LinkedComponentID; // 关联 ShipComponent

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Damage = 50.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FireRate = 200.f; // 发/分钟

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Range = 50000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EnergyPerShot = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HeatPerShot = 5.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bRequiresLock = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LockTime = 2.f; // 锁定所需时间

    // 运行时
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    float CooldownRemaining = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    AActor* CurrentTarget = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    float CurrentLockProgress = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    bool bLocked = false;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UShipWeaponsComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UShipWeaponsComponent();

    virtual void TickComponent(float Dt, ELevelTick Tick, FActorComponentTickFunction* Fn) override;
    virtual void BeginPlay() override;

    // —— 武器槽位 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
    TArray<FShipWeaponSlot> WeaponSlots;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxWeaponSlots = 6;

    // —— 开火控制 ——
    UFUNCTION(BlueprintCallable, Server, Reliable)
    void ServerStartFiring(int32 SlotIndex = 0);

    UFUNCTION(BlueprintCallable, Server, Reliable)
    void ServerStopFiring(int32 SlotIndex = 0);

    UFUNCTION(BlueprintCallable, Server, Reliable)
    void ServerFireSlot(int32 SlotIndex);

    // —— 锁定 ——
    UFUNCTION(BlueprintCallable, Server, Reliable)
    void ServerAcquireTarget(int32 SlotIndex, AActor* Target);

    UFUNCTION(BlueprintCallable, Server, Reliable)
    void ServerReleaseTarget(int32 SlotIndex);

    UFUNCTION(BlueprintCallable, BlueprintPure)
    AActor* GetCurrentTarget(int32 SlotIndex = 0) const;

    UFUNCTION(BlueprintCallable, BlueprintPure)
    float GetLockProgress(int32 SlotIndex = 0) const;

    // —— 安装/卸载 ——
    UFUNCTION(BlueprintCallable, Server, Reliable)
    void ServerInstallWeapon(const FShipWeaponSlot& Weapon);

    UFUNCTION(BlueprintCallable, Server, Reliable)
    void ServerRemoveWeapon(int32 SlotIndex);

    // —— 查询 ——
    UFUNCTION(BlueprintCallable, BlueprintPure)
    bool CanFire(int32 SlotIndex) const;

    UFUNCTION(BlueprintCallable, BlueprintPure)
    float GetDPS(int32 SlotIndex = 0) const;

    UFUNCTION(BlueprintCallable, BlueprintPure)
    TArray<AActor*> GetEnemiesInRange(int32 SlotIndex = 0) const;

    // —— 事件 ——
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeaponFired, int32, SlotIndex);
    UPROPERTY(BlueprintAssignable)
    FOnWeaponFired OnWeaponFired;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTargetLocked, int32, SlotIndex, AActor*, Target);
    UPROPERTY(BlueprintAssignable)
    FOnTargetLocked OnTargetLocked;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLockBroken, int32, SlotIndex);
    UPROPERTY(BlueprintAssignable)
    FOnLockBroken OnLockBroken;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

private:
    // 开火状态
    UPROPERTY(Replicated)
    TArray<bool> FiringState;

    // 内部
    void ProcessFiring(float Dt);
    void ProcessLocking(float Dt);
    void SpawnProjectile(int32 SlotIndex);
    bool CheckLineOfSight(AActor* Target) const;
    float CalculateDamageFalloff(int32 SlotIndex, float Distance) const;
};
