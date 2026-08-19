// ============================================================
// 路径: Source/StellarSystem/Public/Station/PlanetarySpaceport.h
// 作用: 行星地面太空港（电梯仅连6区域: 机库/美食街/展厅/商场/医院/会议室）
//       机库仅自己+队友可进入
// 依赖: Station/OrbitalStationPlacer.h, Ship/ShipPawn.h, Core/PartySystem.h
// 版本: v7.6.2 (电梯逻辑修复 + 机库权限)
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "PlanetarySpaceport.generated.h"

class AProceduralPlanet;
class AMyCharacter;
class AShipPawn;
class UUserWidget;

// 太空港功能区类型 —— 电梯 ONLY 连接以下 6 类区域
UENUM(BlueprintType)
enum class ESpaceportZone : uint8
{
    ElevatorLobby,     // 0: 电梯大厅（入口/出口）— 唯一电梯接入点
    PersonalHangar,    // 1: 个人机库（仅自己+队友）
    FoodCourt,         // 2: 美食街/餐厅
    PublicShowroom,    // 3: 公共展厅/飞船展示
    ShoppingMall,      // 4: 商场/商店街
    Hospital,          // 5: 医院/医疗中心
    ConferenceRoom,    // 6: 会议室/派系大厅
    // 以下区域 NOT served by elevator (via other access points only)
    Customs,           // 7: 海关/安全检查（地面入口直达）
    GroundTransport,   // 8: 地面交通枢纽（外部入口直达）
    Dormitory,         // 9: 宿舍/休息区（从电梯大厅步行）
    MAX
};

// 电梯目标 —— 严格限定为 6 个可乘电梯到达的区域
UENUM(BlueprintType)
enum class EElevatorDestination : uint8
{
    Hangar           UMETA(DisplayName = "个人机库"),
    FoodCourt        UMETA(DisplayName = "美食街"),
    Showroom         UMETA(DisplayName = "展厅"),
    ShoppingMall     UMETA(DisplayName = "商场"),
    Hospital         UMETA(DisplayName = "医院"),
    ConferenceRoom   UMETA(DisplayName = "会议室"),
    ElevatorLobby    UMETA(DisplayName = "电梯大厅(返回)"),
    MAX
};

// 单个功能区定义
USTRUCT(BlueprintType)
struct FSpaceportZoneDef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ESpaceportZone ZoneType = ESpaceportZone::ElevatorLobby;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName ZoneName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector ZoneLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRotator ZoneRotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector ZoneExtent = FVector(500, 500, 300);

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsAccessible = true;

    // 是否可由电梯到达（仅 6 区域为 true）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bElevatorAccessible = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName RequiredFaction = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MinReputation = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bRequiresSecurityCheck = false;
};

// 个人机库定义
USTRUCT(BlueprintType)
struct FPersonalHangarDef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName HangarID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName OwnerPlayerID;          // 机库主人

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector HangarLocation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxShipSize = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bHasShipCallTerminal = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName CurrentParkedShip = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsRentable = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float WeeklyRent = 500.f;

    // 当前正在使用机库的玩家（用于队友共享）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FName> AuthorizedUsers; // Owner + party members
};

// 呼船终端数据
USTRUCT(BlueprintType)
struct FShipCallRequest
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName PlayerID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName ShipID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName TargetHangarID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector LandingPadLocation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EstimatedArrivalTime = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsIncoming = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsPendingClaim = false;
};

// 电梯移动状态
USTRUCT(BlueprintType)
struct FElevatorMoveRequest
{
    GENERATED_BODY()

    UPROPERTY()
    AMyCharacter* Character = nullptr;

    UPROPERTY()
    EElevatorDestination Destination;

    UPROPERTY()
    float ElapsedTime = 0.f;

    UPROPERTY()
    bool bIsMoving = false;
};

// 太空港主类
UCLASS(BlueprintType)
class APlanetarySpaceport : public AActor
{
    GENERATED_BODY()

public:
    APlanetarySpaceport();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // —— 绑定的行星 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spaceport")
    AProceduralPlanet* ParentPlanet = nullptr;

    // —— 太空港名称 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Spaceport")
    FString SpaceportName = TEXT("New Babbage Interstellar Port");

    // —— 所属派系 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Spaceport")
    FName ControllingFaction = NAME_None;

    // —— 安全等级 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spaceport")
    float SecurityLevel = 0.8f;

    // —— 所有功能区（固定 10 个，索引对应 ESpaceportZone）——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spaceport")
    TArray<FSpaceportZoneDef> Zones;

    // —— 个人机库列表 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Spaceport|Hangars")
    TArray<FPersonalHangarDef> PersonalHangars;

    // —— 公共机库（游客/临时停泊）——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spaceport|Hangars")
    int32 PublicHangarSlots = 20;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spaceport|Hangars")
    TArray<FName> OccupiedPublicSlots;

    // —— 电梯系统 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spaceport|Elevator")
    FVector OrbitalTransferPoint = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spaceport|Elevator")
    float ElevatorTravelTime = 30.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spaceport|Elevator")
    bool bHasOrbitalElevator = true;

    // 电梯可到达的目的地列表（UI 用）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spaceport|Elevator")
    TArray<EElevatorDestination> AvailableDestinations;

    // ========== 核心功能 ==========

    // —— 玩家进入太空港（轨道站 → 电梯大厅）——
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Spaceport")
    void Server_EnterSpaceport(AMyCharacter* Character);

    // —— 玩家离开太空港（回轨道）——
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Spaceport")
    void Server_ExitToOrbit(AMyCharacter* Character);

    // —— 使用电梯（电梯大厅 → 6 个区域之一）——
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Spaceport|Elevator")
    void Server_UseElevator(AMyCharacter* Character, EElevatorDestination Destination);

    // —— 从电梯大厅进入指定功能区（电梯到达后调用）——
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Spaceport|Zones")
    void Server_EnterZone(AMyCharacter* Character, ESpaceportZone ZoneType);

    // —— 呼船 ——
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Spaceport|Hangar")
    void Server_CallShip(AMyCharacter* Character, FName ShipID, FName HangarID);

    // —— 飞船到达后停泊 ——
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Spaceport|Hangar")
    void Server_ParkShip(AShipPawn* Ship, FName HangarID);

    // —— 从机库取出飞船（登船准备）——
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Spaceport|Hangar")
    void Server_RetrieveShip(AMyCharacter* Character, FName ShipID);

    // —— 租/买个人机库 ——
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Spaceport|Hangar")
    void Server_RentHangar(AMyCharacter* Character, FName HangarID, int32 Weeks);

    // —— 查询机库可用性 ——
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Spaceport|Hangar")
    TArray<FPersonalHangarDef> GetAvailableHangars() const;

    // —— 获取最近的着陆台位置 ——
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Spaceport|Hangar")
    FVector GetNearestLandingPad(const FVector& FromLocation) const;

    // —— 获取功能区入口位置 ——
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Spaceport|Zones")
    FVector GetZoneEntrance(ESpaceportZone ZoneType) const;

    // —— 检查玩家是否有权进入某区域 ——
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Spaceport|Zones")
    bool CanEnterZone(AMyCharacter* Character, ESpaceportZone ZoneType) const;

    // —— 呼船终端 UI 数据 ——
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Spaceport|Hangar")
    TArray<FShipCallRequest> GetPendingShipCalls(FName PlayerID) const;

    // —— 自动生成太空港布局 ——
    UFUNCTION(BlueprintCallable, Category = "Spaceport")
    void AutoGenerateSpaceport(AProceduralPlanet* Planet, FName Faction);

    // —— 自动生成模板 ——
    UFUNCTION(BlueprintCallable, Category = "Spaceport")
    void ApplyTemplate_MajorCity();

    UFUNCTION(BlueprintCallable, Category = "Spaceport")
    void ApplyTemplate_Outpost();

    UFUNCTION(BlueprintCallable, Category = "Spaceport")
    void ApplyTemplate_Resort();

    UFUNCTION(BlueprintCallable, Category = "Spaceport")
    void ApplyTemplate_MilitaryBase();

    UFUNCTION(BlueprintCallable, Category = "Spaceport")
    void ApplyTemplate_TradeHub();

    // —— 太空港到轨道站的转移 ——
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Spaceport")
    AOrbitalStationPlacer* GetLinkedOrbitalStation() const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spaceport")
    FName LinkedOrbitalStationID = NAME_None;

    // —— 获取电梯可用目的地列表（UI 调用）——
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Spaceport|Elevator")
    TArray<EElevatorDestination> GetElevatorDestinations() const { return AvailableDestinations; }

    // —— 检查电梯是否可到达某区域 ——
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Spaceport|Elevator")
    bool IsElevatorAccessible(ESpaceportZone ZoneType) const;

    // —— 检查玩家是否能进入某机库（自己或队友）——
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Spaceport|Hangar")
    bool CanAccessHangar(AMyCharacter* Character, FName HangarID) const;

    // —— 事件 ——
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerEnteredSpaceport, AMyCharacter*, Character);
    UPROPERTY(BlueprintAssignable, Category = "Spaceport|Events")
    FOnPlayerEnteredSpaceport OnPlayerEntered;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnShipArrived, FName, ShipID, FName, HangarID);
    UPROPERTY(BlueprintAssignable, Category = "Spaceport|Events")
    FOnShipArrived OnShipArrived;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHangarRented, FName, HangarID, AMyCharacter*, Renter);
    UPROPERTY(BlueprintAssignable, Category = "Spaceport|Events")
    FOnHangarRented OnHangarRented;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnElevatorArrived, AMyCharacter*, Character, EElevatorDestination, Destination);
    UPROPERTY(BlueprintAssignable, Category = "Spaceport|Events")
    FOnElevatorArrived OnElevatorArrived;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHangarAccessDenied, AMyCharacter*, Character, FName, HangarID);
    UPROPERTY(BlueprintAssignable, Category = "Spaceport|Events")
    FOnHangarAccessDenied OnHangarAccessDenied;

    // —— 网络 ——
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

private:
    // 生成太空港建筑 Mesh
    void GenerateSpaceportMesh();

    // 生成功能区碰撞体
    void GenerateZoneVolumes();

    // 生成停机坪
    void GenerateLandingPads();

    // 处理呼船队列
    void ProcessShipCallQueue(float DeltaTime);

    // 处理电梯移动
    void ProcessElevatorQueue(float DeltaTime);

    // 初始化电梯可用目的地
    void InitElevatorDestinations();

    // 检查两玩家是否队友
    bool ArePlayersInSameParty(FName PlayerA, FName PlayerB) const;

    // 获取玩家 PartyID
    FName GetPlayerPartyID(FName PlayerID) const;

    // 呼船队列
    UPROPERTY()
    TArray<FShipCallRequest> PendingShipCalls;

    // 电梯移动队列
    UPROPERTY()
    TArray<FElevatorMoveRequest> ElevatorQueue;

    // 当前太空港内的玩家
    UPROPERTY()
    TArray<AMyCharacter*> PlayersInside;

    // 玩家当前所在区域
    UPROPERTY()
    TMap<AMyCharacter*, ESpaceportZone> PlayerCurrentZone;

    // 呼船处理定时器
    float ShipCallTimer = 0.f;
    const float ShipCallProcessInterval = 1.f;

    // 电梯处理定时器
    float ElevatorTimer = 0.f;
    const float ElevatorProcessInterval = 0.1f; // 100ms tick
};
