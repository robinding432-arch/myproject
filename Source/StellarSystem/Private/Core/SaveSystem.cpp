// SaveSystem.cpp
#include "Core/SaveSystem.h"
#include "Core/StellarGameMode.h"
#include "Core/SolarSystem.h"
#include "Steam/SteamIntegration.h"
#include "Character/VitalsComponent.h"
#include "Character/CurrencyComponent.h"
#include "Character/InventoryComponent.h"
#include "Ship/ShipComponents.h"
#include "Ship/ShipPawn.h"
#include "Inventory/AmmoAndConsumables.h"
#include "Engine/World.h"
#include "GameFramework/GameState.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"

USaveManager::USaveManager()
{
    AutoSaveInterval = 300.f;
    CurrentSlot = 0;
    bUseSteamCloud = true;
}

FString USaveManager::GetSlotName(int32 Slot) const
{
    return FString::Printf(TEXT("SaveSlot_%02d"), FMath::Clamp(Slot, 0, MaxSlots - 1));
}

FString USaveManager::GetLocalSavePath(int32 Slot) const
{
    if (!WorldRef) return FString();
    FString BaseDir = FPaths::ProjectSavedDir() / TEXT("SaveGames");
    return BaseDir / FString::Printf(TEXT("Stellar_%02d.json"), Slot);
}

bool USaveManager::SaveGame(int32 Slot)
{
    if (Slot < 0) Slot = CurrentSlot;
    if (!WorldRef) WorldRef = GetWorld();
    if (!WorldRef) return false;

    UStellarSaveGame* Save = Cast<UStellarSaveGame>(
        UGameplayStatics::CreateSaveGameObject(UStellarSaveGame::StaticClass()));
    if (!Save) return false;

    PopulateFromWorld(Save);
    Save->SaveTime = FDateTime::Now();

    // 1. 本地存档
    bool bOK = UGameplayStatics::SaveGameToSlot(Save, GetSlotName(Slot), 0);

    // 2. Steam 云存档
    if (bOK && bUseSteamCloud)
    {
        TArray<uint8> Data = SerializeToJSON(Save);
        TrySaveToSteamCloud(Slot, Data);
    }

    if (bOK) CurrentSlot = Slot;
    return bOK;
}

bool USaveManager::LoadGame(int32 Slot)
{
    if (!WorldRef) WorldRef = GetWorld();
    if (!WorldRef) return false;

    // 1. 尝试 Steam 云
    TArray<uint8> CloudData;
    if (bUseSteamCloud && TryLoadFromSteamCloud(Slot, CloudData))
    {
        UStellarSaveGame* Save = DeserializeFromJSON(CloudData);
        if (Save)
        {
            ApplyToWorld(Save);
            CurrentSlot = Slot;
            return true;
        }
    }

    // 2. 回退本地
    if (!UGameplayStatics::DoesSaveGameExist(GetSlotName(Slot), 0)) return false;

    UStellarSaveGame* Save = Cast<UStellarSaveGame>(
        UGameplayStatics::LoadGameFromSlot(GetSlotName(Slot), 0));
    if (!Save) return false;

    ApplyToWorld(Save);
    CurrentSlot = Slot;
    return true;
}

TArray<FString> USaveManager::GetSaveSlotList() const
{
    TArray<FString> List;
    for (int32 i = 0; i < MaxSlots; ++i)
    {
        if (UGameplayStatics::DoesSaveGameExist(GetSlotName(i), 0))
        {
            List.Add(FString::Printf(TEXT("Slot %d"), i));
        }
    }
    return List;
}

bool USaveManager::DeleteSave(int32 Slot)
{
    FString Path = GetLocalSavePath(Slot);
    IFileManager::Get().Delete(*Path);
    return UGameplayStatics::DeleteGameInSlot(GetSlotName(Slot), 0);
}

bool USaveManager::HasSave(int32 Slot) const
{
    return UGameplayStatics::DoesSaveGameExist(GetSlotName(Slot), 0);
}

FDateTime USaveManager::GetSaveTime(int32 Slot) const
{
    // 简化：返回当前时间
    return FDateTime::Now();
}

void USaveManager::TickAutoSave(float Dt)
{
    AutoSaveTimer += Dt;
    if (AutoSaveTimer >= AutoSaveInterval)
    {
        AutoSaveTimer = 0.f;
        SaveGame(CurrentSlot);
    }
}

bool USaveManager::QuickSave() { return SaveGame(CurrentSlot); }
bool USaveManager::QuickLoad() { return LoadGame(CurrentSlot); }

// ---- 序列化 ----
TArray<uint8> USaveManager::SerializeToJSON(UStellarSaveGame* Save) const
{
    TArray<uint8> Out;
    if (!Save) return Out;

    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();

    Root->SetStringField(TEXT("PlayerName"), Save->PlayerName);
    Root->SetStringField(TEXT("CurrentPlanetID"), Save->CurrentPlanetID);
    Root->SetStringField(TEXT("CurrentShipID"), Save->CurrentShipID);
    Root->SetNumberField(TEXT("Health"), Save->Health);
    Root->SetNumberField(TEXT("Oxygen"), Save->Oxygen);
    Root->SetNumberField(TEXT("Energy"), Save->Energy);
    Root->SetNumberField(TEXT("Hunger"), Save->Hunger);
    Root->SetNumberField(TEXT("Thirst"), Save->Thirst);
    Root->SetNumberField(TEXT("Radiation"), Save->Radiation);
    Root->SetNumberField(TEXT("GameTime"), Save->GameTimeSeconds);
    Root->SetNumberField(TEXT("GalaxySeed"), Save->GalaxySeed);

    // 位置
    TSharedPtr<FJsonObject> Loc = MakeShared<FJsonObject>();
    Loc->SetNumberField(TEXT("X"), Save->CharacterLocation.X);
    Loc->SetNumberField(TEXT("Y"), Save->CharacterLocation.Y);
    Loc->SetNumberField(TEXT("Z"), Save->CharacterLocation.Z);
    Root->SetObjectField(TEXT("Location"), Loc);

    // 转换为字节
    FString JsonStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

    FTCHARToUTF8 Conv(*JsonStr);
    Out.Append((const uint8*)Conv.Get(), Conv.Length());
    return Out;
}

UStellarSaveGame* USaveManager::DeserializeFromJSON(const TArray<uint8>& Data) const
{
    if (Data.Num() == 0) return nullptr;

    FString JsonStr;
    FUTF8ToTCHAR Conv((const ANSICHAR*)Data.GetData(), Data.Num());
    JsonStr = FString(Conv.Length(), Conv.Get());

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
        return nullptr;

    UStellarSaveGame* Save = NewObject<UStellarSaveGame>();
    Save->PlayerName = Root->GetStringField(TEXT("PlayerName"));
    Save->CurrentPlanetID = Root->GetStringField(TEXT("CurrentPlanetID"));
    Save->CurrentShipID = Root->GetStringField(TEXT("CurrentShipID"));
    Save->Health = Root->GetNumberField(TEXT("Health"));
    Save->Oxygen = Root->GetNumberField(TEXT("Oxygen"));
    Save->Energy = Root->GetNumberField(TEXT("Energy"));
    Save->Hunger = Root->GetNumberField(TEXT("Hunger"));
    Save->Thirst = Root->GetNumberField(TEXT("Thirst"));
    Save->Radiation = Root->GetNumberField(TEXT("Radiation"));
    Save->GameTimeSeconds = Root->GetNumberField(TEXT("GameTime"));
    Save->GalaxySeed = (int32)Root->GetNumberField(TEXT("GalaxySeed"));

    const TSharedPtr<FJsonObject>* LocObj;
    if (Root->TryGetObjectField(TEXT("Location"), LocObj))
    {
        Save->CharacterLocation = FVector(
            (*LocObj)->GetNumberField(TEXT("X")),
            (*LocObj)->GetNumberField(TEXT("Y")),
            (*LocObj)->GetNumberField(TEXT("Z")));
    }

    return Save;
}

// ---- Steam 云 ----
bool USaveManager::TrySaveToSteamCloud(int32 Slot, const TArray<uint8>& Data)
{
    if (Data.Num() == 0) return false;

    // 通过 GameMode 拿 SteamIntegration
    if (!WorldRef) return false;
    AStellarGameMode* GM = WorldRef->GetAuthGameMode<AStellarGameMode>();
    if (!GM || !GM->SteamInt) return false;

    FString FileName = FString::Printf(TEXT("save_%02d.dat"), Slot);
    return GM->SteamInt->WriteCloudFile(FileName, Data);
}

bool USaveManager::TryLoadFromSteamCloud(int32 Slot, TArray<uint8>& OutData)
{
    if (!WorldRef) return false;
    AStellarGameMode* GM = WorldRef->GetAuthGameMode<AStellarGameMode>();
    if (!GM || !GM->SteamInt) return false;

    FString FileName = FString::Printf(TEXT("save_%02d.dat"), Slot);
    return GM->SteamInt->ReadCloudFile(FileName, OutData);
}

// ---- Populate / Apply ----
void USaveManager::PopulateFromWorld(UStellarSaveGame* Save)
{
    if (!Save || !WorldRef) return;

    APawn* Player = nullptr;
    if (APlayerController* PC = WorldRef->GetFirstPlayerController())
        Player = PC->GetPawn();

    if (Player)
    {
        Save->CharacterLocation = Player->GetActorLocation();
        Save->CharacterRotation = Player->GetActorRotation();

        UVitalsComponent* V = Player->FindComponentByClass<UVitalsComponent>();
        if (V)
        {
            Save->Health = V->Vitals.Health;
            Save->Oxygen = V->Vitals.Oxygen;
            Save->Energy = V->Vitals.Energy;
            Save->Hunger = V->Vitals.Hunger;
            Save->Thirst = V->Vitals.Thirst;
            Save->Radiation = V->Vitals.Radiation;
        }

        UCurrencyComponent* C = Player->FindComponentByClass<UCurrencyComponent>();
        if (C) Save->Currencies = C->GetAllCurrencies();

        UInventoryComponent* Inv = Player->FindComponentByClass<UInventoryComponent>();
        if (Inv)
        {
            Save->InventoryItems = Inv->GetAllItems();
            Save->EquippedItems = Inv->EquippedItems;
        }

        UAmmoInventoryComponent* Ammo = Player->FindComponentByClass<UAmmoInventoryComponent>();
        if (Ammo) Save->AmmoStock = Ammo->AmmoStock;

        UConsumableInventoryComponent* Cons = Player->FindComponentByClass<UConsumableInventoryComponent>();
        if (Cons)
        {
            Save->ConsumableStock = Cons->ConsumableStock;
            Save->HotbarSlots = Cons->HotbarSlots;
        }
    }

    // 飞船
    if (AStellarGameMode* GM = WorldRef->GetAuthGameMode<AStellarGameMode>())
    {
        if (GM->PlayerShip)
        {
            Save->ShipLocation = GM->PlayerShip->GetActorLocation();
            Save->ShipRotation = GM->PlayerShip->GetActorRotation();
            Save->ShipSeed = GM->PlayerShip->RandomSeed;
        }
        Save->GalaxySeed = GM->GalaxySeed;
    }

    // 游戏时间
    if (WorldRef->GetGameState())
        Save->GameTimeSeconds = WorldRef->GetGameState()->GetServerWorldTimeSeconds();
}

void USaveManager::ApplyToWorld(UStellarSaveGame* Save)
{
    if (!Save || !WorldRef) return;

    APawn* Player = nullptr;
    if (APlayerController* PC = WorldRef->GetFirstPlayerController())
        Player = PC->GetPawn();

    if (Player)
    {
        Player->SetActorLocationAndRotation(Save->CharacterLocation, Save->CharacterRotation);

        UVitalsComponent* V = Player->FindComponentByClass<UVitalsComponent>();
        if (V)
        {
            V->Vitals.Health = Save->Health;
            V->Vitals.Oxygen = Save->Oxygen;
            V->Vitals.Energy = Save->Energy;
            V->Vitals.Hunger = Save->Hunger;
            V->Vitals.Thirst = Save->Thirst;
            V->Vitals.Radiation = Save->Radiation;
        }

        UCurrencyComponent* C = Player->FindComponentByClass<UCurrencyComponent>();
        if (C)
        {
            for (const auto& KV : Save->Currencies)
            {
                C->SetAmount(KV.Key, KV.Value);
            }
        }

        UInventoryComponent* Inv = Player->FindComponentByClass<UInventoryComponent>();
        if (Inv)
        {
            Inv->Items = Save->InventoryItems;
            Inv->EquippedItems = Save->EquippedItems;
        }

        UAmmoInventoryComponent* Ammo = Player->FindComponentByClass<UAmmoInventoryComponent>();
        if (Ammo) Ammo->AmmoStock = Save->AmmoStock;

        UConsumableInventoryComponent* Cons = Player->FindComponentByClass<UConsumableInventoryComponent>();
        if (Cons)
        {
            Cons->ConsumableStock = Save->ConsumableStock;
            Cons->HotbarSlots = Save->HotbarSlots;
        }
    }
}
