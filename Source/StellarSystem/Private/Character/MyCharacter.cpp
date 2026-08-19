// MyCharacter.cpp
#include "Character/MyCharacter.h"
#include "Character/CharacterStates.h"
#include "Character/Customization/CharacterCustomization.h"
#include "Character/VitalsComponent.h"
#include "Character/CurrencyComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/AmmoAndConsumables.h"
#include "Planet/ProceduralPlanet.h"
#include "Ship/ProceduralShip.h"
#include "Ship/ShipPawn.h"
#include "Core/SpaceWeather.h"
#include "Core/StellarGameMode.h"
#include "Components/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

AMyCharacter::AMyCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 400.f;
    CameraBoom->bUsePawnControlRotation = true;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom);
    FollowCamera->bUsePawnControlRotation = false;

    Customization = CreateDefaultSubobject<UCharacterCustomizationComponent>(TEXT("Customization"));
    Vitals = CreateDefaultSubobject<UVitalsComponent>(TEXT("Vitals"));
    Inventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("Inventory"));
    AmmoInv = CreateDefaultSubobject<UAmmoInventoryComponent>(TEXT("AmmoInv"));
    ConsumableInv = CreateDefaultSubobject<UConsumableInventoryComponent>(TEXT("ConsumableInv"));
    Currency = CreateDefaultSubobject<UCurrencyComponent>(TEXT("Currency"));
    StateMachine = CreateDefaultSubobject<UCharacterStateMachine>(TEXT("StateMachine"));

    GetCharacterMovement()->GravityScale = 1.f;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f);

    // 默认速度
    GetCharacterMovement()->MaxWalkSpeed = 600.f;
}

void AMyCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (APlayerController* PC = Cast<APlayerController>(Controller))
    {
        if (ULocalPlayer* LP = PC->GetLocalPlayer())
        {
            if (UEnhancedInputLocalPlayerSubsystem* Subsys =
                LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            {
                if (DefaultMappingContext)
                    Subsys->AddMappingContext(DefaultMappingContext, 0);
            }
        }
    }

    // 初始化重力
    if (PlanetActor)
    {
        PlanetUp = (GetActorLocation() - PlanetActor->GetActorLocation()).GetSafeNormal();
        LastPlanetRotation = PlanetActor->GetCurrentRotation();
        bPlanetRotInitialized = true;
    }

    // 初始化维生
    if (Vitals) Vitals->Init(this);

    // v5.0：加载天气引用
    if (WeatherActor.IsValid())
    {
        // 已设置
    }
    else if (AStellarGameMode* GM = GetWorld()->GetAuthGameMode<AStellarGameMode>())
    {
        if (GM->WeatherSystem)
        {
            WeatherActor = GM->WeatherSystem;
        }
    }
}

void AMyCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bTransitioning)
    {
        UpdateTransition(DeltaTime);
        return;
    }

    if (CurrentMode == ECharacterMode::Walking)
    {
        UpdatePlanetGravity();

        // v5.0：Tick 维生
        if (Vitals)
        {
            Vitals->TickVitals(DeltaTime, PlanetActor);
        }
    }
    else if (CurrentMode == ECharacterMode::Orbiting)
    {
        UpdateOrbitFlight(DeltaTime);
    }
}

float AMyCharacter::TakeDamage(float Damage, const FDamageEvent& DamageEvent,
    AController* EventInstigator, AActor* DamageCauser)
{
    float Actual = Super::TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);
    if (Vitals)
    {
        Vitals->Health.Current = FMath::Max(0.f, Vitals->Health.Current - Actual);
        if (Vitals->Health.Current <= 0.f)
        {
            Vitals->OnHealthCritical.Broadcast(EVitalStatus::Depleted);
        }
    }
    return Actual;
}

void AMyCharacter::UpdatePlanetGravity()
{
    if (!PlanetActor) return;

    FVector Center = PlanetActor->GetActorLocation();
    PlanetUp = (GetActorLocation() - Center).GetSafeNormal();

    FQuat TargetQuat = FQuat::FindBetweenVectors(GetActorUpVector(), PlanetUp);
    AddActorWorldRotation(TargetQuat);

    GetCharacterMovement()->SetGravityDirection(-PlanetUp);

    FQuat CurrentPlanetRot = PlanetActor->GetCurrentRotation();
    FQuat DeltaRot = CurrentPlanetRot * LastPlanetRotation.Inverse();
    SetActorRotation(DeltaRot * GetActorQuat());
    LastPlanetRotation = CurrentPlanetRot;

    FRotator CamRot = CameraBoom->GetRelativeRotation();
    CameraBoom->bUsePawnControlRotation = false;
    CameraBoom->SetRelativeRotation(FRotationMatrix::MakeFromZ(PlanetUp).Rotator() + CamRot);
    CameraBoom->bUsePawnControlRotation = true;
}

void AMyCharacter::Move(const FInputActionValue& Value)
{
    FVector2D MV = Value.Get<FVector2D>();
    if (Controller && MV.SizeSquared() > 0.f)
    {
        FVector Fwd = FVector::CrossProduct(GetActorRightVector(), PlanetUp).GetSafeNormal();
        FVector Right = FVector::CrossProduct(PlanetUp, Fwd).GetSafeNormal();
        AddMovementInput(Fwd, MV.Y);
        AddMovementInput(Right, MV.X);
    }
}

void AMyCharacter::Look(const FInputActionValue& Value)
{
    FVector2D LV = Value.Get<FVector2D>();
    if (Controller)
    {
        AddControllerYawInput(LV.X);
        AddControllerPitchInput(LV.Y);
    }
}

void AMyCharacter::Interact(const FInputActionValue& Value)
{
    if (NearbyProceduralShip && !NearbyProceduralShip->IsPlayerInside())
    {
        NearbyProceduralShip->EnterShip(this);
    }
}

void AMyCharacter::ToggleFlight(const FInputActionValue& Value)
{
    if (CurrentMode == ECharacterMode::Walking)
        EnterOrbitMode();
    else if (CurrentMode == ECharacterMode::Orbiting)
        ExitOrbitMode();
}

void AMyCharacter::EnterOrbitMode()
{
    if (!PlanetActor) return;
    TransitionStart = GetActorLocation();
    FVector Dir = (GetActorLocation() - PlanetActor->GetActorLocation()).GetSafeNormal();
    float TargetDist = PlanetActor->PlanetRadius * OrbitDistance;
    TransitionTarget = PlanetActor->GetActorLocation() + Dir * TargetDist;
    TransitionAlpha = 0.f;
    bTransitioning = true;
    CurrentMode = ECharacterMode::Transition;

    GetCharacterMovement()->SetMovementMode(MOVE_Flying);
    GetCharacterMovement()->GravityScale = 0.f;
}

void AMyCharacter::ExitOrbitMode()
{
    if (!PlanetActor) return;
    TransitionStart = GetActorLocation();
    TransitionTarget = FindLandingPosition();
    TransitionAlpha = 0.f;
    bTransitioning = true;
    CurrentMode = ECharacterMode::Transition;
}

void AMyCharacter::UpdateTransition(float Dt)
{
    TransitionAlpha += Dt * 1.5f;
    if (TransitionAlpha >= 1.f)
    {
        bTransitioning = false;
        SetActorLocation(TransitionTarget);

        if (PlanetActor && FVector::Dist(GetActorLocation(), PlanetActor->GetActorLocation()) >
            PlanetActor->PlanetRadius * 1.1f)
        {
            CurrentMode = ECharacterMode::Orbiting;
        }
        else
        {
            CurrentMode = ECharacterMode::Walking;
            GetCharacterMovement()->SetMovementMode(MOVE_Walking);
            GetCharacterMovement()->GravityScale = 1.f;
        }
        return;
    }

    float t = FMath::SmoothStep(0.f, 1.f, TransitionAlpha);
    SetActorLocation(FMath::Lerp(TransitionStart, TransitionTarget, t));
}

FVector AMyCharacter::FindLandingPosition() const
{
    if (!PlanetActor) return GetActorLocation();
    FVector Dir = (GetActorLocation() - PlanetActor->GetActorLocation()).GetSafeNormal();
    float SurfaceDist = PlanetActor->PlanetRadius + 200.f;
    return PlanetActor->GetActorLocation() + Dir * SurfaceDist;
}

void AMyCharacter::UpdateOrbitFlight(float Dt)
{
    if (!PlanetActor || !Controller) return;

    APlayerController* PC = Cast<APlayerController>(Controller);
    if (!PC) return;

    float FwdInput = PC->GetInputAxisValue(FName("MoveForward"));
    float RightInput = PC->GetInputAxisValue(FName("MoveRight"));

    FVector OrbitUp = (GetActorLocation() - PlanetActor->GetActorLocation()).GetSafeNormal();
    FVector OrbitFwd = FVector::CrossProduct(GetActorRightVector(), OrbitUp).GetSafeNormal();
    FVector OrbitRight = FVector::CrossProduct(OrbitUp, OrbitFwd).GetSafeNormal();

    FVector MoveDelta = (OrbitFwd * FwdInput + OrbitRight * RightInput) * 3000.f * Dt;
    AddActorWorldOffset(MoveDelta, true);

    float LookX = PC->GetInputAxisValue(FName("Turn"));
    float LookY = PC->GetInputAxisValue(FName("LookUp"));

    FQuat YawQuat = FQuat(OrbitUp, FMath::DegreesToRadians(-LookX * 0.5f));
    SetActorLocation(YawQuat.RotateVector(GetActorLocation() - PlanetActor->GetActorLocation())
        + PlanetActor->GetActorLocation());

    FVector RightAxis = GetActorRightVector();
    FQuat PitchQuat = FQuat(RightAxis, FMath::DegreesToRadians(-LookY * 0.3f));
    SetActorLocation(PitchQuat.RotateVector(GetActorLocation() - PlanetActor->GetActorLocation())
        + PlanetActor->GetActorLocation());

    FVector ToCenter = (PlanetActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
    SetActorRotation(FRotationMatrix::MakeFromZ(ToCenter).Rotator());
}

void AMyCharacter::StartWarp(const FInputActionValue& Value)
{
    // 通知 GameMode 跃迁到最近星球
    if (AStellarGameMode* GM = GetWorld()->GetAuthGameMode<AStellarGameMode>())
    {
        if (PlanetActor)
        {
            // 找下一个星球
            TArray<AProceduralPlanet*> Others = GM->GetPlanetsInRange(
                GetActorLocation(), 1e10f);
            for (AProceduralPlanet* P : Others)
            {
                if (P && P != PlanetActor)
                {
                    if (GM->PlayerShip) GM->PlayerShip->WarpToPlanet(P);
                    break;
                }
            }
        }
    }
}

void AMyCharacter::SprintPressed(const FInputActionValue& Value)
{
    bSprinting = true;
    GetCharacterMovement()->MaxWalkSpeed = 1200.f;
    if (Vitals) Vitals->Energy.DrainRate *= 3.f;
}

void AMyCharacter::SprintReleased(const FInputActionValue& Value)
{
    bSprinting = false;
    GetCharacterMovement()->MaxWalkSpeed = 600.f;
    if (Vitals) Vitals->Energy.DrainRate /= 3.f;
}

void AMyCharacter::CycleConsumable(const FInputActionValue& Value)
{
    float Val = Value.Get<float>();
    int32 Direction = Val > 0 ? 1 : -1;
    ActiveHotbarSlot = FMath::Clamp(ActiveHotbarSlot + Direction, 1, 9);
}

void AMyCharacter::PausePressed(const FInputActionValue& Value)
{
    if (bPaused) return;
    bPaused = true;

    if (APlayerController* PC = Cast<APlayerController>(Controller))
    {
        PC->SetPause(true);
        PC->bShowMouseCursor = true;
        PC->SetInputMode(FInputModeUIOnly());

        // 打开暂停菜单 Widget
        // 简化：直接打开主菜单地图
        UGameplayStatics::OpenLevel(this, FName("MainMenuMap"), false);
    }
}

void AMyCharacter::OpenMainMenu()
{
    PausePressed(FInputActionValue());
}

void AMyCharacter::SetNearbyShip(AShipPawn* Ship)
{
    NearbyShip = Ship;
}

void AMyCharacter::SetNearbyProceduralShip(AProceduralShip* Ship)
{
    NearbyProceduralShip = Ship;
}

void AMyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (MoveAction)
            EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMyCharacter::Move);
        if (LookAction)
            EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMyCharacter::Look);
        if (JumpAction)
            EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
        if (InteractAction)
            EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &AMyCharacter::Interact);
        if (ToggleFlightAction)
            EIC->BindAction(ToggleFlightAction, ETriggerEvent::Started, this, &AMyCharacter::ToggleFlight);
        if (WarpAction)
            EIC->BindAction(WarpAction, ETriggerEvent::Started, this, &AMyCharacter::StartWarp);
        if (SprintAction)
        {
            EIC->BindAction(SprintAction, ETriggerEvent::Started, this, &AMyCharacter::SprintPressed);
            EIC->BindAction(SprintAction, ETriggerEvent::Completed, this, &AMyCharacter::SprintReleased);
        }
        if (CycleConsumableAction)
            EIC->BindAction(CycleConsumableAction, ETriggerEvent::Triggered, this, &AMyCharacter::CycleConsumable);
        if (PauseAction)
            EIC->BindAction(PauseAction, ETriggerEvent::Started, this, &AMyCharacter::PausePressed);
    }
}

void AMyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(AMyCharacter, CurrentMode);
    DOREPLIFETIME(AMyCharacter, PlanetActor);
}
