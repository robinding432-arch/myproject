// MyCharacter.h
// 玩家角色：球面重力 + 地面行走 + 登船/离船 + 状态机
// v5.0：集成维生 Tick + 天气感知 + 主菜单入口
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MyCharacter.generated.h"

class UInputMappingContext;
class UInputAction;
class USpringArmComponent;
class UCameraComponent;
class UCharacterCustomizationComponent;
class UVitalsComponent;
class UInventoryComponent;
class UAmmoInventoryComponent;
class UConsumableInventoryComponent;
class UCurrencyComponent;
class UAudioManager;
class AProceduralPlanet;
class AShipPawn;
class AProceduralShip;
class ASpaceWeather;

UENUM(BlueprintType)
enum class ECharacterMode : uint8
{
    Walking,
    Orbiting,
    Transition
};

UCLASS()
class AMyCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AMyCharacter();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual float TakeDamage(float Damage, const FDamageEvent& DamageEvent,
        AController* EventInstigator, AActor* DamageCauser) override;

    // ---- 行星引用 ----
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet")
    AProceduralPlanet* PlanetActor = nullptr;

    // v5.0：天气引用
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
    TSoftObjectPtr<ASpaceWeather> WeatherActor;

    // ---- 模式 ----
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    ECharacterMode CurrentMode = ECharacterMode::Walking;

    // ---- 组件 ----
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    USpringArmComponent* CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UCameraComponent* FollowCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UCharacterCustomizationComponent* Customization;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UVitalsComponent* Vitals;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UInventoryComponent* Inventory;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UAmmoInventoryComponent* AmmoInv;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UConsumableInventoryComponent* ConsumableInv;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UCurrencyComponent* Currency;

    // ---- 输入资产 ----
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputMappingContext* DefaultMappingContext = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* MoveAction = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* LookAction = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* JumpAction = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* InteractAction = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* ToggleFlightAction = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* WarpAction = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* SprintAction = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* CycleConsumableAction = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* PauseAction = nullptr; // v5.0

    // ---- 飞船交互 ----
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    AShipPawn* NearbyShip = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    AProceduralShip* NearbyProceduralShip = nullptr;

    UFUNCTION(BlueprintCallable)
    void SetNearbyShip(AShipPawn* Ship);

    UFUNCTION(BlueprintCallable)
    void SetNearbyProceduralShip(AProceduralShip* Ship);

    // ---- 模式切换 ----
    UFUNCTION(BlueprintCallable)
    void EnterOrbitMode();

    UFUNCTION(BlueprintCallable)
    void ExitOrbitMode();

    // v5.0：打开主菜单
    UFUNCTION(BlueprintCallable)
    void OpenMainMenu();

    // ---- 网络 ----
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

protected:
    // ---- 输入回调 ----
    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void Interact(const FInputActionValue& Value);
    void ToggleFlight(const FInputActionValue& Value);
    void StartWarp(const FInputActionValue& Value);
    void SprintPressed(const FInputActionValue& Value);
    void SprintReleased(const FInputActionValue& Value);
    void CycleConsumable(const FInputActionValue& Value);
    void PausePressed(const FInputActionValue& Value); // v5.0

    // ---- 球面重力 ----
    void UpdatePlanetGravity();
    FVector PlanetUp = FVector::UpVector;
    FQuat LastPlanetRotation = FQuat::Identity;
    bool bPlanetRotInitialized = false;

    // ---- 维生 Tick ----
    void TickVitals(float DT);

    // ---- 过渡 ----
    bool bTransitioning = false;
    float TransitionAlpha = 0.f;
    FVector TransitionStart = FVector::ZeroVector;
    FVector TransitionTarget = FVector::ZeroVector;
    float TransitionSpeed = 800.f;
    float OrbitDistance = 1.5f;

    void UpdateTransition(float Dt);
    void StartTransitionToOrbit();
    void StartTransitionToSurface();
    FVector FindLandingPosition() const;

    // ---- 轨道飞行 ----
    void UpdateOrbitFlight(float Dt);

    // ---- 冲刺 ----
    bool bSprinting = false;

    // ---- 暂停 ----
    bool bPaused = false;
};
