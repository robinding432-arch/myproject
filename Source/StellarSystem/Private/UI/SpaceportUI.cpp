// ============================================================
// SpaceportUI.cpp
// 太空港总控 UI 实现
// ============================================================

#include "UI/SpaceportUI.h"
#include "Station/PlanetarySpaceport.h"
#include "Character/MyCharacter.h"
#include "Core/StellarPlayerController.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

void USpaceportUI::InitializeSpaceportUI(APlanetarySpaceport* Spaceport)
{
    BoundSpaceport = Spaceport;
    RefreshUI();
}

TArray<FSpaceportZoneDisplay> USpaceportUI::GetAllZones() const
{
    TArray<FSpaceportZoneDisplay> Result;

    if (!BoundSpaceport) return Result;

    for (const FSpaceportZoneDef& Zone : BoundSpaceport->Zones)
    {
        FSpaceportZoneDisplay Display;
        Display.ZoneName = Zone.ZoneName.ToString();
        Display.bIsAccessible = Zone.bIsAccessible;
        Display.bRequiresSecurity = Zone.bRequiresSecurityCheck;

        switch (Zone.ZoneType)
        {
        case ESpaceportZone::ElevatorLobby:  Display.ZoneType = TEXT("Elevator Lobby"); break;
        case ESpaceportZone::Dormitory:      Display.ZoneType = TEXT("Dormitory"); break;
        case ESpaceportZone::Hospital:       Display.ZoneType = TEXT("Hospital"); break;
        case ESpaceportZone::FoodCourt:     Display.ZoneType = TEXT("Food Court"); break;
        case ESpaceportZone::ShoppingMall:   Display.ZoneType = TEXT("Shopping Mall"); break;
        case ESpaceportZone::PersonalHangar: Display.ZoneType = TEXT("Personal Hangar"); break;
        case ESpaceportZone::PublicShowroom: Display.ZoneType = TEXT("Ship Showroom"); break;
        case ESpaceportZone::ConferenceRoom: Display.ZoneType = TEXT("Conference Room"); break;
        case ESpaceportZone::Customs:        Display.ZoneType = TEXT("Customs Checkpoint"); break;
        case ESpaceportZone::GroundTransport:Display.ZoneType = TEXT("Ground Transport"); break;
        default: Display.ZoneType = TEXT("Unknown"); break;
        }

        if (Zone.RequiredFaction != NAME_None)
        {
            Display.RestrictionText = FString::Printf(TEXT("Requires: %s"), *Zone.RequiredFaction.ToString());
        }
        else if (Zone.MinReputation > 0)
        {
            Display.RestrictionText = FString::Printf(TEXT("Reputation: %d+"), Zone.MinReputation);
        }
        else
        {
            Display.RestrictionText = TEXT("Public Access");
        }

        Result.Add(Display);
    }

    return Result;
}

void USpaceportUI::NavigateToZone(int32 ZoneIndex)
{
    if (!BoundSpaceport) return;

    AController* PC = nullptr;
    TArray<AActor*> Controllers;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AController::StaticClass(), Controllers);
    for (AActor* A : Controllers)
    {
        AController* C = Cast<AController>(A);
        if (C && C->GetPawn())
        {
            PC = C;
            break;
        }
    }

    if (PC && PC->GetPawn())
    {
        AMyCharacter* Char = Cast<AMyCharacter>(PC->GetPawn());
        if (Char)
        {
            ESpaceportZone ZoneType = ESpaceportZone::ElevatorLobby;
            if (BoundSpaceport->Zones.IsValidIndex(ZoneIndex))
            {
                ZoneType = BoundSpaceport->Zones[ZoneIndex].ZoneType;
            }
            BoundSpaceport->Server_EnterZone(Char, ZoneType);
        }
    }
}

void USpaceportUI::UseElevatorToOrbit()
{
    if (!BoundSpaceport) return;

    AController* PC = nullptr;
    TArray<AActor*> Controllers;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AController::StaticClass(), Controllers);
    for (AActor* A : Controllers)
    {
        AController* C = Cast<AController>(A);
        if (C && C->GetPawn())
        {
            PC = C;
            break;
        }
    }

    if (PC && PC->GetPawn())
    {
        AMyCharacter* Char = Cast<AMyCharacter>(PC->GetPawn());
        if (Char)
        {
            BoundSpaceport->Server_UseElevator(Char, true);
        }
    }
}

void USpaceportUI::UseElevatorToSurface()
{
    if (!BoundSpaceport) return;

    AController* PC = nullptr;
    TArray<AActor*> Controllers;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AController::StaticClass(), Controllers);
    for (AActor* A : Controllers)
    {
        AController* C = Cast<AController>(A);
        if (C && C->GetPawn())
        {
            PC = C;
            break;
        }
    }

    if (PC && PC->GetPawn())
    {
        AMyCharacter* Char = Cast<AMyCharacter>(PC->GetPawn());
        if (Char)
        {
            BoundSpaceport->Server_UseElevator(Char, false);
        }
    }
}

TArray<FString> USpaceportUI::GetHangarList() const
{
    TArray<FString> Result;
    if (!BoundSpaceport) return Result;

    for (const FPersonalHangarDef& H : BoundSpaceport->PersonalHangars)
    {
        FString Entry = FString::Printf(TEXT("%s - %s - %.0f creds/week"),
            *H.HangarID.ToString(),
            H.OwnerPlayerID == NAME_None ? TEXT("Available") : TEXT("Occupied"),
            H.WeeklyRent);
        Result.Add(Entry);
    }
    return Result;
}

void USpaceportUI::RentHangar(int32 HangarIndex, int32 Weeks)
{
    if (!BoundSpaceport) return;

    AController* PC = nullptr;
    TArray<AActor*> Controllers;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AController::StaticClass(), Controllers);
    for (AActor* A : Controllers)
    {
        AController* C = Cast<AController>(A);
        if (C && C->GetPawn())
        {
            PC = C;
            break;
        }
    }

    if (PC && PC->GetPawn() && BoundSpaceport->PersonalHangars.IsValidIndex(HangarIndex))
    {
        AMyCharacter* Char = Cast<AMyCharacter>(PC->GetPawn());
        if (Char)
        {
            BoundSpaceport->Server_RentHangar(Char, BoundSpaceport->PersonalHangars[HangarIndex].HangarID, Weeks);
        }
    }
}

FString USpaceportUI::GetSpaceportName() const
{
    if (BoundSpaceport) return BoundSpaceport->SpaceportName;
    return TEXT("Unknown Spaceport");
}

FString USpaceportUI::GetSecurityLevelText() const
{
    if (!BoundSpaceport) return TEXT("Unknown");

    float Sec = BoundSpaceport->SecurityLevel;
    if (Sec >= 0.9f) return TEXT("Maximum Security");
    if (Sec >= 0.7f) return TEXT("High Security");
    if (Sec >= 0.4f) return TEXT("Moderate Security");
    if (Sec >= 0.2f) return TEXT("Low Security");
    return TEXT("Lawless Zone");
}

FString USpaceportUI::GetFactionText() const
{
    if (!BoundSpaceport) return TEXT("Independent");
    if (BoundSpaceport->ControllingFaction == NAME_None) return TEXT("Independent");
    return BoundSpaceport->ControllingFaction.ToString();
}

void USpaceportUI::RefreshUI()
{
    LastRefreshTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

void USpaceportUI::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!GetWorld()) return;

    float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LastRefreshTime >= RefreshInterval)
    {
        RefreshUI();
    }
}
