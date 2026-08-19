// ============================================================
// PlanetarySpaceport.cpp
// 行星地面太空港实现
// v7.6.2: 电梯仅连6区域 + 机库仅自己/队友 + Bug修复
// ============================================================

#include "Station/PlanetarySpaceport.h"
#include "Station/OrbitalStationPlacer.h"
#include "Character/MyCharacter.h"
#include "Ship/ShipPawn.h"
#include "Ship/InsuranceSystem.h"
#include "Core/StellarPlayerController.h"
#include "Core/PartySystem.h"
#include "Kismet/GameplayStatics.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Math/UnrealMathUtility.h"

APlanetarySpaceport::APlanetarySpaceport()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;

    SpaceportName = TEXT("New Babbage Interstellar Port");

    // 固定 10 个功能区
    Zones.SetNum(10);

    // 0: 电梯大厅（入口/出口）— 唯一电梯接入点
    Zones[0] = FSpaceportZoneDef{
        ESpaceportZone::ElevatorLobby, FName(TEXT("ElevatorLobby")),
        FVector(0, 0, 500), FRotator::ZeroRotator, FVector(800, 800, 600),
        true, NAME_None, 0, false, true /*bElevatorAccessible*/
    };

    // 1: 个人机库
    Zones[1] = FSpaceportZoneDef{
        ESpaceportZone::PersonalHangar, FName(TEXT("PersonalHangars")),
        FVector(4000, 1500, 300), FRotator::ZeroRotator, FVector(3000, 2000, 800),
        true, NAME_None, 0, false, true /*bElevatorAccessible*/
    };

    // 2: 美食街
    Zones[2] = FSpaceportZoneDef{
        ESpaceportZone::FoodCourt, FName(TEXT("FoodCourt")),
        FVector(0, 2000, 500), FRotator::ZeroRotator, FVector(1500, 1200, 600),
        true, NAME_None, 0, false, true /*bElevatorAccessible*/
    };

    // 3: 公共展厅
    Zones[3] = FSpaceportZoneDef{
        ESpaceportZone::PublicShowroom, FName(TEXT("ShipShowroom")),
        FVector(-4000, 1500, 500), FRotator::ZeroRotator, FVector(2000, 1500, 800),
        true, NAME_None, 100, false, true /*bElevatorAccessible*/
    };

    // 4: 商场
    Zones[4] = FSpaceportZoneDef{
        ESpaceportZone::ShoppingMall, FName(TEXT("ShoppingMall")),
        FVector(0, -2000, 500), FRotator::ZeroRotator, FVector(2000, 1500, 600),
        true, NAME_None, 0, false, true /*bElevatorAccessible*/
    };

    // 5: 医院
    Zones[5] = FSpaceportZoneDef{
        ESpaceportZone::Hospital, FName(TEXT("MedicalCenter")),
        FVector(-2000, 0, 500), FRotator::ZeroRotator, FVector(1200, 800, 600),
        true, NAME_None, 0, true, true /*bElevatorAccessible*/
    };

    // 6: 会议室
    Zones[6] = FSpaceportZoneDef{
        ESpaceportZone::ConferenceRoom, FName(TEXT("ConferenceHall")),
        FVector(4000, -1500, 500), FRotator::ZeroRotator, FVector(1000, 800, 600),
        true, NAME_None, 300, false, true /*bElevatorAccessible*/
    };

    // 7: 海关（地面入口直达，电梯不可达）
    Zones[7] = FSpaceportZoneDef{
        ESpaceportZone::Customs, FName(TEXT("CustomsCheckpoint")),
        FVector(1000, 0, 500), FRotator::ZeroRotator, FVector(600, 400, 600),
        true, NAME_None, 0, true, false /*bElevatorAccessible = false*/
    };

    // 8: 地面交通（外部入口直达，电梯不可达）
    Zones[8] = FSpaceportZoneDef{
        ESpaceportZone::GroundTransport, FName(TEXT("GroundTransportHub")),
        FVector(-1000, 2000, 300), FRotator::ZeroRotator, FVector(1200, 800, 400),
        true, NAME_None, 0, false, false /*bElevatorAccessible = false*/
    };

    // 9: 宿舍（从电梯大厅步行可达，电梯不可直达）
    Zones[9] = FSpaceportZoneDef{
        ESpaceportZone::Dormitory, FName(TEXT("DormitoryBlockA")),
        FVector(2000, 0, 500), FRotator::ZeroRotator, FVector(1500, 1000, 600),
        true, NAME_None, 0, false, false /*bElevatorAccessible = false*/
    };

    // 默认个人机库
    PersonalHangars.SetNum(8);
    for (int32 i = 0; i < 8; ++i)
    {
        PersonalHangars[i].HangarID = FName(*FString::Printf(TEXT("Hangar_%02d"), i + 1));
        PersonalHangars[i].OwnerPlayerID = NAME_None;
        PersonalHangars[i].HangarLocation = FVector(4000 + i * 350, 1500, 300);
        PersonalHangars[i].MaxShipSize = 3;
        PersonalHangars[i].bHasShipCallTerminal = true;
        PersonalHangars[i].CurrentParkedShip = NAME_None;
        PersonalHangars[i].bIsRentable = true;
        PersonalHangars[i].WeeklyRent = 500.f + i * 100.f;
    }

    // 初始化电梯目的地
    InitElevatorDestinations();
}

void APlanetarySpaceport::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        GenerateSpaceportMesh();
        GenerateZoneVolumes();
        GenerateLandingPads();
        InitElevatorDestinations();
    }
}

void APlanetarySpaceport::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    ProcessShipCallQueue(DeltaTime);
    ProcessElevatorQueue(DeltaTime);
}

// ========== 电梯目的地管理 ==========

void APlanetarySpaceport::InitElevatorDestinations()
{
    AvailableDestinations.Empty();
    // 严格限定 6 个电梯可达区域
    AvailableDestinations.Add(EElevatorDestination::Hangar);
    AvailableDestinations.Add(EElevatorDestination::FoodCourt);
    AvailableDestinations.Add(EElevatorDestination::Showroom);
    AvailableDestinations.Add(EElevatorDestination::ShoppingMall);
    AvailableDestinations.Add(EElevatorDestination::Hospital);
    AvailableDestinations.Add(EElevatorDestination::ConferenceRoom);
    AvailableDestinations.Add(EElevatorDestination::ElevatorLobby); // 返回大厅
}

bool APlanetarySpaceport::IsElevatorAccessible(ESpaceportZone ZoneType) const
{
    for (const FSpaceportZoneDef& Zone : Zones)
    {
        if (Zone.ZoneType == ZoneType)
        {
            return Zone.bElevatorAccessible;
        }
    }
    return false;
}

// ========== 电梯移动处理 ==========

void APlanetarySpaceport::ProcessElevatorQueue(float DeltaTime)
{
    ElevatorTimer += DeltaTime;
    if (ElevatorTimer < ElevatorProcessInterval) return;
    ElevatorTimer = 0.f;

    if (ElevatorQueue.Num() == 0) return;

    TArray<int32> CompletedIndices;
    for (int32 i = 0; i < ElevatorQueue.Num(); ++i)
    {
        FElevatorMoveRequest& Req = ElevatorQueue[i];
        if (!IsValid(Req.Character)) continue;

        Req.ElapsedTime += ElevatorProcessInterval;
        if (Req.ElapsedTime >= ElevatorTravelTime)
        {
            CompletedIndices.Add(i);

            // 到达目的地 → 传送玩家
            ESpaceportZone ZoneType;
            switch (Req.Destination)
            {
                case EElevatorDestination::Hangar:         ZoneType = ESpaceportZone::PersonalHangar; break;
                case EElevatorDestination::FoodCourt:      ZoneType = ESpaceportZone::FoodCourt; break;
                case EElevatorDestination::Showroom:       ZoneType = ESpaceportZone::PublicShowroom; break;
                case EElevatorDestination::ShoppingMall:   ZoneType = ESpaceportZone::ShoppingMall; break;
                case EElevatorDestination::Hospital:       ZoneType = ESpaceportZone::Hospital; break;
                case EElevatorDestination::ConferenceRoom: ZoneType = ESpaceportZone::ConferenceRoom; break;
                default: ZoneType = ESpaceportZone::ElevatorLobby; break;
            }

            FVector DestLoc = GetZoneEntrance(ZoneType);
            Req.Character->SetActorLocation(DestLoc);
            PlayerCurrentZone.Add(Req.Character, ZoneType);

            // 解锁输入
            AStellarPlayerController* PC = Cast<AStellarPlayerController>(Req.Character->GetController());
            if (PC) PC->SetGameInputMode();

            // 广播事件
            OnElevatorArrived.Broadcast(Req.Character, Req.Destination);
        }
    }

    // 移除已完成
    for (int32 idx = CompletedIndices.Num() - 1; idx >= 0; --idx)
    {
        ElevatorQueue.RemoveAt(CompletedIndices[idx]);
    }
}

// ========== RPC 实现 ==========

bool APlanetarySpaceport::Server_EnterSpaceport_Validate(AMyCharacter* Character) { return true; }
void APlanetarySpaceport::Server_EnterSpaceport_Implementation(AMyCharacter* Character)
{
    if (!Character || !HasAuthority()) return;

    PlayersInside.AddUnique(Character);

    // 设置玩家位置到电梯大厅
    FVector Entrance = GetActorLocation() + Zones[0].ZoneLocation;
    Character->SetActorLocation(Entrance);
    PlayerCurrentZone.Add(Character, ESpaceportZone::ElevatorLobby);

    OnPlayerEntered.Broadcast(Character);

    AStellarPlayerController* PC = Cast<AStellarPlayerController>(Character->GetController());
    if (PC)
    {
        PC->SwitchToCharacter(Character);
    }
}

bool APlanetarySpaceport::Server_ExitToOrbit_Validate(AMyCharacter* Character) { return true; }
void APlanetarySpaceport::Server_ExitToOrbit_Implementation(AMyCharacter* Character)
{
    if (!Character || !HasAuthority()) return;

    PlayersInside.Remove(Character);
    PlayerCurrentZone.Remove(Character);

    // 传送到轨道电梯出口点
    FVector ExitPoint = GetActorLocation() + OrbitalTransferPoint;
    Character->SetActorLocation(ExitPoint);

    // 通知轨道站（由 GameMode 处理）
    if (LinkedOrbitalStationID != NAME_None)
    {
        AOrbitalStationPlacer* OrbitalStation = GetLinkedOrbitalStation();
        if (OrbitalStation)
        {
            // 触发轨道站接入逻辑
        }
    }
}

bool APlanetarySpaceport::Server_UseElevator_Validate(AMyCharacter* Character, EElevatorDestination Destination) { return true; }
void APlanetarySpaceport::Server_UseElevator_Implementation(AMyCharacter* Character, EElevatorDestination Destination)
{
    if (!Character || !HasAuthority()) return;

    // 验证目的地合法性
    if (Destination == EElevatorDestination::MAX) return;

    // 检查玩家当前是否在电梯大厅
    ESpaceportZone* CurrentZone = PlayerCurrentZone.Find(Character);
    if (!CurrentZone || *CurrentZone != ESpaceportZone::ElevatorLobby)
    {
        // 不在电梯大厅，不能乘电梯
        return;
    }

    // 锁定玩家输入
    AStellarPlayerController* PC = Cast<AStellarPlayerController>(Character->GetController());
    if (PC)
    {
        PC->SetUIMode();
    }

    // 加入电梯队列
    FElevatorMoveRequest Req;
    Req.Character = Character;
    Req.Destination = Destination;
    Req.ElapsedTime = 0.f;
    Req.bIsMoving = true;
    ElevatorQueue.Add(Req);
}

bool APlanetarySpaceport::Server_EnterZone_Validate(AMyCharacter* Character, ESpaceportZone ZoneType) { return true; }
void APlanetarySpaceport::Server_EnterZone_Implementation(AMyCharacter* Character, ESpaceportZone ZoneType)
{
    if (!Character || !HasAuthority()) return;

    if (!CanEnterZone(Character, ZoneType))
    {
        return;
    }

    FVector Entrance = GetZoneEntrance(ZoneType);
    Character->SetActorLocation(Entrance);
    PlayerCurrentZone.Add(Character, ZoneType);
}

// ========== 机库权限检查 ==========

bool APlanetarySpaceport::CanAccessHangar(AMyCharacter* Character, FName HangarID) const
{
    if (!Character) return false;

    FName CharacterID = FName(*Character->GetName());

    for (const FPersonalHangarDef& Hangar : PersonalHangars)
    {
        if (Hangar.HangarID == HangarID)
        {
            // 1. 自己拥有 → 允许
            if (Hangar.OwnerPlayerID == CharacterID) return true;

            // 2. 队友共享 → 允许
            for (FName AuthUser : Hangar.AuthorizedUsers)
            {
                if (AuthUser == CharacterID) return true;
            }

            // 3. 检查是否同队伍
            if (ArePlayersInSameParty(CharacterID, Hangar.OwnerPlayerID))
            {
                return true;
            }

            return false;
        }
    }
    return false;
}

bool APlanetarySpaceport::ArePlayersInSameParty(FName PlayerA, FName PlayerB) const
{
    if (PlayerA == PlayerB) return true;
    if (PlayerA == NAME_None || PlayerB == NAME_None) return false;

    // 通过 World 查找 PartySystem
    UWorld* World = GetWorld();
    if (!World) return false;

    UPartySystem* PartySys = World->GetSubsystem<UPartySystem>();
    if (!PartySys) return false;

    FName PartyA = PartySys->GetPlayerPartyID(PlayerA);
    FName PartyB = PartySys->GetPlayerPartyID(PlayerB);
    return (PartyA != NAME_None && PartyA == PartyB);
}

FName APlanetarySpaceport::GetPlayerPartyID(FName PlayerID) const
{
    UWorld* World = GetWorld();
    if (!World) return NAME_None;

    UPartySystem* PartySys = World->GetSubsystem<UPartySystem>();
    if (!PartySys) return NAME_None;

    return PartySys->GetPlayerPartyID(PlayerID);
}

// ========== 呼船 ==========

bool APlanetarySpaceport::Server_CallShip_Validate(AMyCharacter* Character, FName ShipID, FName HangarID) { return true; }
void APlanetarySpaceport::Server_CallShip_Implementation(AMyCharacter* Character, FName ShipID, FName HangarID)
{
    if (!Character || !HasAuthority()) return;

    // 检查机库权限
    if (!CanAccessHangar(Character, HangarID))
    {
        OnHangarAccessDenied.Broadcast(Character, HangarID);
        return;
    }

    FShipCallRequest Req;
    Req.PlayerID = FName(*Character->GetName());
    Req.ShipID = ShipID;
    Req.TargetHangarID = HangarID;
    Req.EstimatedArrivalTime = 60.f;
    Req.bIsIncoming = true;
    Req.bIsPendingClaim = false;

    PendingShipCalls.Add(Req);
}

bool APlanetarySpaceport::Server_ParkShip_Validate(AShipPawn* Ship, FName HangarID) { return true; }
void APlanetarySpaceport::Server_ParkShip_Implementation(AShipPawn* Ship, FName HangarID)
{
    if (!Ship || !HasAuthority()) return;

    for (FPersonalHangarDef& Hangar : PersonalHangars)
    {
        if (Hangar.HangarID == HangarID)
        {
            Hangar.CurrentParkedShip = FName(*Ship->GetName());
            Ship->FlightMode = EShipFlightMode::Docked;
            break;
        }
    }
}

bool APlanetarySpaceport::Server_RetrieveShip_Validate(AMyCharacter* Character, FName ShipID) { return true; }
void APlanetarySpaceport::Server_RetrieveShip_Implementation(AMyCharacter* Character, FName ShipID)
{
    if (!Character || !HasAuthority()) return;

    AStellarPlayerController* PC = Cast<AStellarPlayerController>(Character->GetController());
    if (!PC) return;

    // 查找飞船并 Possess
    TArray<AActor*> Ships;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AShipPawn::StaticClass(), Ships);
    for (AActor* A : Ships)
    {
        AShipPawn* Ship = Cast<AShipPawn>(A);
        if (Ship && FName(*Ship->GetName()) == ShipID)
        {
            // 检查飞船是否停泊在当前太空港的某机库
            bool bShipInOurHangar = false;
            for (const FPersonalHangarDef& Hangar : PersonalHangars)
            {
                if (Hangar.CurrentParkedShip == FName(*Ship->GetName()))
                {
                    bShipInOurHangar = true;
                    break;
                }
            }

            if (!bShipInOurHangar)
            {
                // 飞船不在本太空港机库，拒绝
                return;
            }

            PC->SwitchToShip(Ship);
            break;
        }
    }
}

bool APlanetarySpaceport::Server_RentHangar_Validate(AMyCharacter* Character, FName HangarID, int32 Weeks) { return true; }
void APlanetarySpaceport::Server_RentHangar_Implementation(AMyCharacter* Character, FName HangarID, int32 Weeks)
{
    if (!Character || !HasAuthority()) return;

    FName CharID = FName(*Character->GetName());

    for (FPersonalHangarDef& Hangar : PersonalHangars)
    {
        if (Hangar.HangarID == HangarID && Hangar.bIsRentable)
        {
            // 只能租未出租的机库
            if (Hangar.OwnerPlayerID != NAME_None && Hangar.OwnerPlayerID != CharID)
            {
                return; // 已被别人租用
            }

            Hangar.OwnerPlayerID = CharID;
            Hangar.AuthorizedUsers.AddUnique(CharID);

            OnHangarRented.Broadcast(HangarID, Character);
            break;
        }
    }
}

// ========== 查询函数 ==========

TArray<FPersonalHangarDef> APlanetarySpaceport::GetAvailableHangars() const
{
    TArray<FPersonalHangarDef> Available;
    for (const FPersonalHangarDef& H : PersonalHangars)
    {
        if (H.OwnerPlayerID == NAME_None && H.bIsRentable)
        {
            Available.Add(H);
        }
    }
    return Available;
}

FVector APlanetarySpaceport::GetNearestLandingPad(const FVector& FromLocation) const
{
    FVector BaseLoc = GetActorLocation();
    return BaseLoc + FVector(4000, 1500, 300);
}

FVector APlanetarySpaceport::GetZoneEntrance(ESpaceportZone ZoneType) const
{
    for (const FSpaceportZoneDef& Zone : Zones)
    {
        if (Zone.ZoneType == ZoneType)
        {
            return GetActorLocation() + Zone.ZoneLocation;
        }
    }
    return GetActorLocation();
}

bool APlanetarySpaceport::CanEnterZone(AMyCharacter* Character, ESpaceportZone ZoneType) const
{
    if (!Character) return false;

    for (const FSpaceportZoneDef& Zone : Zones)
    {
        if (Zone.ZoneType != ZoneType) continue;

        if (!Zone.bIsAccessible) return false;

        // 电梯可达检查：只能通过电梯进入（已在 Server_UseElevator 中处理）
        // 此处仅做权限检查

        // 派系限制
        if (Zone.RequiredFaction != NAME_None)
        {
            // 查询 FactionManager（简化：允许通过）
            // TODO: 接入 FactionManager->GetPlayerFaction()
        }

        // 声望要求
        if (Zone.MinReputation > 0)
        {
            // TODO: 接入 FactionManager->GetReputation()
        }

        // 机库特殊处理：检查权限
        if (ZoneType == ESpaceportZone::PersonalHangar)
        {
            // 机库进入由电梯系统处理，此处检查在 CanAccessHangar 中
        }

        return true;
    }
    return false;
}

TArray<FShipCallRequest> APlanetarySpaceport::GetPendingShipCalls(FName PlayerID) const
{
    TArray<FShipCallRequest> Result;
    for (const FShipCallRequest& Req : PendingShipCalls)
    {
        if (Req.PlayerID == PlayerID)
        {
            Result.Add(Req);
        }
    }
    return Result;
}

AOrbitalStationPlacer* APlanetarySpaceport::GetLinkedOrbitalStation() const
{
    TArray<AActor*> Actors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AOrbitalStationPlacer::StaticClass(), Actors);
    for (AActor* A : Actors)
    {
        AOrbitalStationPlacer* Placer = Cast<AOrbitalStationPlacer>(A);
        if (Placer && Placer->GetName().Contains(LinkedOrbitalStationID.ToString()))
        {
            return Placer;
        }
    }
    return nullptr;
}

// ========== 呼船队列处理 ==========

void APlanetarySpaceport::ProcessShipCallQueue(float DeltaTime)
{
    ShipCallTimer += DeltaTime;
    if (ShipCallTimer < ShipCallProcessInterval) return;
    ShipCallTimer = 0.f;

    if (PendingShipCalls.Num() == 0) return;

    TArray<int32> CompletedIndices;
    for (int32 i = 0; i < PendingShipCalls.Num(); ++i)
    {
        FShipCallRequest& Req = PendingShipCalls[i];
        if (Req.bIsIncoming)
        {
            Req.EstimatedArrivalTime -= ShipCallProcessInterval;
            if (Req.EstimatedArrivalTime <= 0.f)
            {
                Req.bIsIncoming = false;
                Req.EstimatedArrivalTime = 0.f;
                CompletedIndices.Add(i);
                OnShipArrived.Broadcast(Req.ShipID, Req.TargetHangarID);
            }
        }
    }

    for (int32 idx = CompletedIndices.Num() - 1; idx >= 0; --idx)
    {
        PendingShipCalls.RemoveAt(CompletedIndices[idx]);
    }
}

// ========== 自动生成 ==========

void APlanetarySpaceport::AutoGenerateSpaceport(AProceduralPlanet* Planet, FName Faction)
{
    ParentPlanet = Planet;
    ControllingFaction = Faction;

    if (Planet)
    {
        SetActorLocation(Planet->GetActorLocation() + FVector(0, 0, Planet->PlanetRadius + 5000.f));
    }

    if (Faction == FName(TEXT("TerranEmpire")))
    {
        SecurityLevel = 0.9f;
    }
    else if (Faction == FName(TEXT("CrimsonPirates")))
    {
        SecurityLevel = 0.2f;
    }
    else
    {
        SecurityLevel = 0.6f;
    }
}

void APlanetarySpaceport::ApplyTemplate_MajorCity()
{
    SpaceportName = TEXT("Metropolis Interstellar Port");
    SecurityLevel = 0.85f;
    PublicHangarSlots = 40;

    for (FSpaceportZoneDef& Zone : Zones)
    {
        Zone.bIsAccessible = true;
    }
    PersonalHangars.SetNum(16);
    for (int32 i = 0; i < 16; ++i)
    {
        PersonalHangars[i].HangarID = FName(*FString::Printf(TEXT("CityHangar_%02d"), i + 1));
        PersonalHangars[i].OwnerPlayerID = NAME_None;
        PersonalHangars[i].HangarLocation = FVector(4000 + (i % 4) * 350, 1500 + (i / 4) * 400, 300);
        PersonalHangars[i].MaxShipSize = 4;
        PersonalHangars[i].bHasShipCallTerminal = true;
        PersonalHangars[i].CurrentParkedShip = NAME_None;
        PersonalHangars[i].bIsRentable = true;
        PersonalHangars[i].WeeklyRent = 800.f;
    }
}

void APlanetarySpaceport::ApplyTemplate_Outpost()
{
    SpaceportName = TEXT("Border Outpost");
    SecurityLevel = 0.4f;
    PublicHangarSlots = 8;

    // 仅电梯可达区域保持开放
    for (int32 i = 0; i < Zones.Num(); ++i)
    {
        if (Zones[i].bElevatorAccessible)
        {
            Zones[i].bIsAccessible = true;
        }
        else
        {
            Zones[i].bIsAccessible = false;
        }
    }

    // 边境前哨：只保留机库+医院
    Zones[1].bIsAccessible = true; // 机库
    Zones[5].bIsAccessible = true; // 医院

    PersonalHangars.SetNum(4);
    for (int32 i = 0; i < 4; ++i)
    {
        PersonalHangars[i].HangarID = FName(*FString::Printf(TEXT("OutpostHgr_%d"), i + 1));
        PersonalHangars[i].OwnerPlayerID = NAME_None;
        PersonalHangars[i].WeeklyRent = 200.f;
    }
}

void APlanetarySpaceport::ApplyTemplate_Resort()
{
    SpaceportName = TEXT("Paradise Resort Spaceport");
    SecurityLevel = 0.95f;

    Zones[2].ZoneExtent = FVector(2500, 2000, 800); // 大美食街
    Zones[4].ZoneExtent = FVector(3000, 2000, 800); // 大商场

    PersonalHangars.SetNum(12);
    for (int32 i = 0; i < 12; ++i)
    {
        PersonalHangars[i].OwnerPlayerID = NAME_None;
        PersonalHangars[i].WeeklyRent = 1200.f;
    }
}

void APlanetarySpaceport::ApplyTemplate_MilitaryBase()
{
    SpaceportName = TEXT("Fortress Command Spaceport");
    SecurityLevel = 1.0f;

    for (FSpaceportZoneDef& Zone : Zones)
    {
        Zone.bRequiresSecurityCheck = true;
    }

    Zones[6].ZoneExtent = FVector(1500, 1200, 800); // 军事会议室

    PersonalHangars.SetNum(20);
    for (int32 i = 0; i < 20; ++i)
    {
        PersonalHangars[i].OwnerPlayerID = NAME_None;
        PersonalHangars[i].MaxShipSize = 5;
        PersonalHangars[i].WeeklyRent = 300.f;
    }
}

void APlanetarySpaceport::ApplyTemplate_TradeHub()
{
    SpaceportName = TEXT("Trade Nexus Spaceport");
    SecurityLevel = 0.75f;

    Zones[4].ZoneExtent = FVector(3500, 2500, 800); // 大商场
    Zones[3].ZoneExtent = FVector(3000, 2000, 1000); // 大展厅
    Zones[3].MinReputation = 0; // 移除声望限制

    PublicHangarSlots = 50;
    PersonalHangars.SetNum(24);
    for (int32 i = 0; i < 24; ++i)
    {
        PersonalHangars[i].OwnerPlayerID = NAME_None;
        PersonalHangars[i].WeeklyRent = 600.f;
    }
}

// ========== Mesh/Volume 生成 ==========

void APlanetarySpaceport::GenerateSpaceportMesh()
{
    // 程序化几何体或蓝图资产替换
}

void APlanetarySpaceport::GenerateZoneVolumes()
{
    for (const FSpaceportZoneDef& Zone : Zones)
    {
        UBoxComponent* ZoneVolume = NewObject<UBoxComponent>(this);
        if (ZoneVolume)
        {
            ZoneVolume->RegisterComponent();
            ZoneVolume->SetBoxExtent(Zone.ZoneExtent);
            ZoneVolume->SetRelativeLocation(Zone.ZoneLocation);
            ZoneVolume->SetRelativeRotation(Zone.ZoneRotation);
            ZoneVolume->SetCollisionProfileName(TEXT("OverlapAll"));
        }
    }
}

void APlanetarySpaceport::GenerateLandingPads()
{
    // 生成停机坪位置
}

// ========== 网络复制 ==========

void APlanetarySpaceport::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(APlanetarySpaceport, SpaceportName);
    DOREPLIFETIME(APlanetarySpaceport, ControllingFaction);
    DOREPLIFETIME(APlanetarySpaceport, PersonalHangars);
    DOREPLIFETIME(APlanetarySpaceport, AvailableDestinations);
}
