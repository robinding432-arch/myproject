// ============================================================
// OrbitalStationPlacer.cpp
// 行星轨道空间站定位系统实现
// ============================================================

#include "Station/OrbitalStationPlacer.h"
#include "Station/ProceduralStation.h"
#include "Planet/ProceduralPlanet.h"
#include "Core/SolarSystem.h"
#include "Engine/World.h"
#include "Math/UnrealMathUtility.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

AOrbitalStationPlacer::AOrbitalStationPlacer()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;

    // 默认5个轨道站位
    StationSlots.SetNum(5);
    StationSlots[0].SlotType = EOrbitalStationSlot::LowOrbit;
    StationSlots[0].SlotName = FName(TEXT("LowOrbit_01"));
    StationSlots[0].OrbitalRadius = 30000.f;
    StationSlots[0].OrbitalPeriod = 1800.f;
    StationSlots[0].MaxDockingPorts = 12;

    StationSlots[1].SlotType = EOrbitalStationSlot::Geosynchronous;
    StationSlots[1].SlotName = FName(TEXT("GeoSync_01"));
    StationSlots[1].OrbitalRadius = 60000.f;
    StationSlots[1].OrbitalPeriod = 3600.f;
    StationSlots[1].bHasQuantumGate = true;
    StationSlots[1].MaxDockingPorts = 16;

    StationSlots[2].SlotType = EOrbitalStationSlot::LagrangeL4;
    StationSlots[2].SlotName = FName(TEXT("L4_TradingPost"));
    StationSlots[2].OrbitalRadius = 80000.f;
    StationSlots[2].OrbitalPhase = 60.f;
    StationSlots[2].OrbitalPeriod = 7200.f;
    StationSlots[2].MaxDockingPorts = 8;

    StationSlots[3].SlotType = EOrbitalStationSlot::LagrangeL5;
    StationSlots[3].SlotName = FName(TEXT("L5_Outpost"));
    StationSlots[3].OrbitalRadius = 80000.f;
    StationSlots[3].OrbitalPhase = 300.f;
    StationSlots[3].OrbitalPeriod = 7200.f;
    StationSlots[3].MaxDockingPorts = 6;

    StationSlots[4].SlotType = EOrbitalStationSlot::TransferOrbit;
    StationSlots[4].SlotName = FName(TEXT("Transfer_Hub"));
    StationSlots[4].OrbitalRadius = 45000.f;
    StationSlots[4].OrbitalInclination = 15.f;
    StationSlots[4].OrbitalPeriod = 2400.f;
    StationSlots[4].MaxDockingPorts = 20;
}

void AOrbitalStationPlacer::BeginPlay()
{
    Super::BeginPlay();

    if (ParentPlanet == nullptr)
    {
        // 自动查找场景中的行星
        TArray<AActor*> FoundPlanets;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), AProceduralPlanet::StaticClass(), FoundPlanets);
        if (FoundPlanets.Num() > 0)
        {
            ParentPlanet = Cast<AProceduralPlanet>(FoundPlanets[0]);
        }
    }

    if (HasAuthority() && ParentPlanet)
    {
        GenerateOrbitalStations();
    }
}

void AOrbitalStationPlacer::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    OrbitalTimeAccumulator += DeltaTime;

    if (OrbitalTimeAccumulator - LastOrbitalUpdate >= OrbitalUpdateInterval)
    {
        UpdateOrbitalMotion(DeltaTime);
        LastOrbitalUpdate = OrbitalTimeAccumulator;
    }
}

void AOrbitalStationPlacer::GenerateOrbitalStations()
{
    if (!HasAuthority() || !ParentPlanet) return;

    ActiveStations.Empty();

    for (const FOrbitalStationSlotDef& Slot : StationSlots)
    {
        AProceduralStation* Station = SpawnStationAtSlot(Slot);
        if (Station)
        {
            ActiveStations.Add(Slot.SlotType, Station);
        }
    }
}

AProceduralStation* AOrbitalStationPlacer::SpawnStationAtSlot(const FOrbitalStationSlotDef& Slot)
{
    if (!ParentPlanet) return nullptr;

    // 计算初始世界位置
    FVector PlanetLoc = ParentPlanet->GetActorLocation();
    FRotator OrbitRot = FRotator(0.f, Slot.OrbitalPhase, Slot.OrbitalInclination);
    FVector Offset = OrbitRot.RotateVector(FVector(Slot.OrbitalRadius, 0.f, 0.f));
    FVector WorldLoc = PlanetLoc + Offset;

    // 生成空间站 Actor
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    AProceduralStation* Station = GetWorld()->SpawnActor<AProceduralStation>(WorldLoc, OrbitRot, SpawnParams);

    if (Station)
    {
        // 配置空间站类型
        switch (Slot.SlotType)
        {
        case EOrbitalStationSlot::LowOrbit:
        case EOrbitalStationSlot::Geosynchronous:
            Station->StationType = EStationType::Ring;
            Station->StationRadius = 8000.f;
            Station->ModuleCount = 12;
            break;
        case EOrbitalStationSlot::LagrangeL4:
        case EOrbitalStationSlot::LagrangeL5:
            Station->StationType = EStationType::Spherical;
            Station->StationRadius = 3000.f;
            Station->ModuleCount = 6;
            break;
        case EOrbitalStationSlot::TransferOrbit:
            Station->StationType = EStationType::Cylindrical;
            Station->StationRadius = 6000.f;
            Station->ModuleCount = 10;
            break;
        default:
            Station->StationType = EStationType::Cluster;
            Station->StationRadius = 5000.f;
            Station->ModuleCount = 8;
            break;
        }

        Station->FactionID = Slot.FactionOwner;
        Station->GenerateStation();

        // 标记太空港
        if (Slot.bHasSpaceport)
        {
            Station->AvailableServices.Add(EStationService::Shop);
            Station->AvailableServices.Add(EStationService::Repair);
            Station->AvailableServices.Add(EStationService::Refuel);
            Station->AvailableServices.Add(EStationService::Storage);
        }
        if (Slot.bHasShipyard)
        {
            Station->AvailableServices.Add(EStationService::Upgrade);
            Station->AvailableServices.Add(EStationService::Repair);
        }
    }

    return Station;
}

void AOrbitalStationPlacer::UpdateOrbitalMotion(float DeltaTime)
{
    if (!ParentPlanet) return;

    FVector PlanetLoc = ParentPlanet->GetActorLocation();

    for (auto& Pair : ActiveStations)
    {
        EOrbitalStationSlot SlotType = Pair.Key;
        AProceduralStation* Station = Pair.Value;
        if (!IsValid(Station)) continue;

        // 找到对应的 Slot 定义
        for (const FOrbitalStationSlotDef& Slot : StationSlots)
        {
            if (Slot.SlotType == SlotType)
            {
                // 计算当前相位
                float Phase = Slot.OrbitalPhase + (OrbitalTimeAccumulator / Slot.OrbitalPeriod) * 360.f;
                Phase = FMath::Fmod(Phase, 360.f);

                FRotator OrbitRot = FRotator(0.f, Phase, Slot.OrbitalInclination);
                FVector Offset = OrbitRot.RotateVector(FVector(Slot.OrbitalRadius, 0.f, 0.f));
                FVector NewLoc = PlanetLoc + Offset;

                Station->SetActorLocation(NewLoc);
                Station->SetActorRotation(OrbitRot);

                // 空间站缓慢自转
                float SpinRate = 2.f; // 度/秒
                FRotator CurrentRot = Station->GetActorRotation();
                CurrentRot.Roll += SpinRate * OrbitalUpdateInterval;
                Station->SetActorRotation(CurrentRot);
                break;
            }
        }
    }
}

FVector AOrbitalStationPlacer::GetStationWorldLocation(EOrbitalStationSlot Slot) const
{
    if (ActiveStations.Contains(Slot) && IsValid(ActiveStations[Slot]))
    {
        return ActiveStations[Slot]->GetActorLocation();
    }
    return FVector::ZeroVector;
}

FRotator AOrbitalStationPlacer::GetStationWorldRotation(EOrbitalStationSlot Slot) const
{
    if (ActiveStations.Contains(Slot) && IsValid(ActiveStations[Slot]))
    {
        return ActiveStations[Slot]->GetActorRotation();
    }
    return FRotator::ZeroRotator;
}

AProceduralStation* AOrbitalStationPlacer::FindNearestStation(const FVector& FromLocation) const
{
    AProceduralStation* Nearest = nullptr;
    float MinDist = TNumericLimits<float>::Max();

    for (const auto& Pair : ActiveStations)
    {
        if (!IsValid(Pair.Value)) continue;
        float Dist = FVector::Dist(FromLocation, Pair.Value->GetActorLocation());
        if (Dist < MinDist)
        {
            MinDist = Dist;
            Nearest = Pair.Value;
        }
    }
    return Nearest;
}

AProceduralStation* AOrbitalStationPlacer::FindNearestShipyard(const FVector& FromLocation) const
{
    for (const auto& Pair : ActiveStations)
    {
        if (!IsValid(Pair.Value)) continue;
        for (const FOrbitalStationSlotDef& Slot : StationSlots)
        {
            if (Slot.SlotType == Pair.Key && Slot.bHasShipyard)
            {
                return Pair.Value;
            }
        }
    }
    return FindNearestStation(FromLocation);
}

AProceduralStation* AOrbitalStationPlacer::FindNearestQuantumGate(const FVector& FromLocation) const
{
    for (const auto& Pair : ActiveStations)
    {
        if (!IsValid(Pair.Value)) continue;
        for (const FOrbitalStationSlotDef& Slot : StationSlots)
        {
            if (Slot.SlotType == Pair.Key && Slot.bHasQuantumGate)
            {
                return Pair.Value;
            }
        }
    }
    return nullptr;
}

void AOrbitalStationPlacer::AutoConfigureForPlanet(AProceduralPlanet* Planet)
{
    if (!Planet) return;

    ParentPlanet = Planet;

    // 根据行星类型智能配置
    FString PlanetType = Planet->GetName(); // 简化判断

    // 宜居星球：多太空港 + 商场
    if (PlanetType.Contains(TEXT("Lush")) || PlanetType.Contains(TEXT("Temperate")))
    {
        ApplyTemplate_LushParadise();
    }
    // 工业星球：造船厂 + 货运
    else if (PlanetType.Contains(TEXT("Industrial")) || PlanetType.Contains(TEXT("Barren")))
    {
        ApplyTemplate_Industrial();
    }
    // 军事星球
    else if (PlanetType.Contains(TEXT("Military")) || PlanetType.Contains(TEXT("Fortress")))
    {
        ApplyTemplate_Military();
    }
    // 采矿前哨
    else if (PlanetType.Contains(TEXT("Mining")) || PlanetType.Contains(TEXT("Arid")))
    {
        ApplyTemplate_MiningOutpost();
    }
    else
    {
        // 默认：量子枢纽
        ApplyTemplate_QuantumHub();
    }
}

void AOrbitalStationPlacer::ApplyTemplate_LushParadise()
{
    StationSlots.SetNum(6);

    // L1: 主要太空港
    StationSlots[0] = FOrbitalStationSlotDef{
        EOrbitalStationSlot::LagrangeL1, FName(TEXT("L1_ParadisePort")), 40000.f, 0.f, 0.f, 3600.f, true, false, false, 16
    };
    // GEO: 量子门枢纽
    StationSlots[1] = FOrbitalStationSlotDef{
        EOrbitalStationSlot::Geosynchronous, FName(TEXT("GEO_QuantumGate")), 65000.f, 0.f, 0.f, 3600.f, true, false, true, 8
    };
    // L4: 贸易站
    StationSlots[2] = FOrbitalStationSlotDef{
        EOrbitalStationSlot::LagrangeL4, FName(TEXT("L4_TradeHub")), 80000.f, 0.f, 60.f, 7200.f, true, false, false, 12
    };
    // L5: 度假村站
    StationSlots[3] = FOrbitalStationSlotDef{
        EOrbitalStationSlot::LagrangeL5, FName(TEXT("L5_Resort")), 80000.f, 0.f, 300.f, 7200.f, true, false, false, 6
    };
    // LowOrbit: 飞船展示
    StationSlots[4] = FOrbitalStationSlotDef{
        EOrbitalStationSlot::LowOrbit, FName(TEXT("LowOrbit_Showroom")), 25000.f, 0.f, 0.f, 1200.f, true, true, false, 10
    };
    // Transfer: 物流枢纽
    StationSlots[5] = FOrbitalStationSlotDef{
        EOrbitalStationSlot::TransferOrbit, FName(TEXT("Transfer_Logistics")), 50000.f, 10.f, 180.f, 2400.f, true, false, false, 20
    };
}

void AOrbitalStationPlacer::ApplyTemplate_Industrial()
{
    StationSlots.SetNum(4);
    StationSlots[0] = FOrbitalStationSlotDef{
        EOrbitalStationSlot::LowOrbit, FName(TEXT("LowOrbit_Shipyard")), 20000.f, 0.f, 0.f, 900.f, true, true, false, 24
    };
    StationSlots[1] = FOrbitalStationSlotDef{
        EOrbitalStationSlot::Geosynchronous, FName(TEXT("GEO_CargoHub")), 55000.f, 0.f, 0.f, 3600.f, true, false, true, 30
    };
    StationSlots[2] = FOrbitalStationSlotDef{
        EOrbitalStationSlot::TransferOrbit, FName(TEXT("Transfer_Export")), 40000.f, 0.f, 90.f, 1800.f, true, false, false, 16
    };
    StationSlots[3] = FOrbitalStationSlotDef{
        EOrbitalStationSlot::PolarOrbit, FName(TEXT("Polar_Survey")), 35000.f, 90.f, 0.f, 1500.f, false, false, false, 4
    };
}

void AOrbitalStationPlacer::ApplyTemplate_Military()
{
    StationSlots.SetNum(5);
    StationSlots[0] = FOrbitalStationSlotDef{
        EOrbitalStationSlot::LowOrbit, FName(TEXT("LowOrbit_Command")), 22000.f, 0.f, 0.f, 1000.f, true, true, false, 20
    };
    StationSlots[1] = FOrbitalStationSlotDef{
        EOrbitalStationSlot::Geosynchronous, FName(TEXT("GEO_Defense")), 60000.f, 0.f, 0.f, 3600.f, true, false, true, 8
    };
    StationSlots[2] = FOrbitalStationSlotDef{
        EOrbitalStationSlot::LagrangeL1, FName(TEXT("L1_Picket")), 35000.f, 0.f, 45.f, 3000.f, false, false, false, 4
    };
    StationSlots[3] = FOrbitalStationSlotDef{
        EOrbitalStationSlot::PolarOrbit, FName(TEXT("Polar_Recon")), 30000.f, 90.f, 0.f, 1200.f, false, false, false, 2
    };
    StationSlots[4] = FOrbitalStationSlotDef{
        EOrbitalStationSlot::TransferOrbit, FName(TEXT("Transfer_Reinforce")), 45000.f, 0.f, 180.f, 2000.f, true, true, false, 12
    };
}

void AOrbitalStationPlacer::ApplyTemplate_MiningOutpost()
{
    StationSlots.SetNum(3);
    StationSlots[0] = FOrbitalStationSlotDef{
        EOrbitalStationSlot::LowOrbit, FName(TEXT("LowOrbit_OreProcessing")), 18000.f, 0.f, 0.f, 800.f, true, true, false, 16
    };
    StationSlots[1] = FOrbitalStationSlotDef{
        EOrbitalStationSlot::TransferOrbit, FName(TEXT("Transfer_OreTransport")), 35000.f, 0.f, 0.f, 1600.f, true, false, false, 24
    };
    StationSlots[2] = FOrbitalStationSlotDef{
        EOrbitalStationSlot::Geosynchronous, FName(TEXT("GEO_SurveyBase")), 50000.f, 0.f, 0.f, 3600.f, true, false, false, 6
    };
}

void AOrbitalStationPlacer::ApplyTemplate_QuantumHub()
{
    StationSlots.SetNum(3);
    StationSlots[0] = FOrbitalStationSlotDef{
        EOrbitalStationSlot::Geosynchronous, FName(TEXT("GEO_QuantumPrime")), 70000.f, 0.f, 0.f, 3600.f, true, false, true, 32
    };
    StationSlots[1] = FOrbitalStationSlotDef{
        EOrbitalStationSlot::LagrangeL4, FName(TEXT("L4_QuantumRelay")), 90000.f, 0.f, 60.f, 7200.f, true, false, true, 16
    };
    StationSlots[2] = FOrbitalStationSlotDef{
        EOrbitalStationSlot::LagrangeL5, FName(TEXT("L5_QuantumRelay")), 90000.f, 0.f, 300.f, 7200.f, true, false, true, 16
    };
}

void AOrbitalStationPlacer::CalculateOrbitalParams(FOrbitalStationSlotDef& Slot, float PlanetRadius, float PlanetMass)
{
    // 根据开普勒第三定律估算轨道周期
    // T = 2π * sqrt(r³/GM)
    float G = 6.674e-11f; // 引力常数（游戏内缩放）
    float ScaledMass = PlanetMass * 1000.f; // 游戏内质量缩放
    float R = FMath::Max(Slot.OrbitalRadius, PlanetRadius * 1.5f);

    Slot.OrbitalRadius = R;

    if (ScaledMass > 0.f)
    {
        float T = 2.f * PI * FMath::Sqrt((R * R * R) / (G * ScaledMass));
        Slot.OrbitalPeriod = FMath::Max(T, 600.f); // 最少10分钟
    }
}

void AOrbitalStationPlacer::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(AOrbitalStationPlacer, ParentPlanet);
}
