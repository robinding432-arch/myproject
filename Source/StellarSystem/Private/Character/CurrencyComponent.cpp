// CurrencyComponent.cpp
#include "Character/CurrencyComponent.h"
#include "Net/UnrealNetwork.h"

UCurrencyComponent::UCurrencyComponent(){SetIsReplicatedByDefault(true);}

void UCurrencyComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    DOREPLIFETIME(UCurrencyComponent, Credits);
    DOREPLIFETIME(UCurrencyComponent, Premium);
    DOREPLIFETIME(UCurrencyComponent, FactionTokens);
    DOREPLIFETIME(UCurrencyComponent, Salvage);
    DOREPLIFETIME(UCurrencyComponent, SciencePoints);
    DOREPLIFETIME(UCurrencyComponent, Reputation);
}

int32& UCurrencyComponent::GetRef(ECurrencyType Type)
{
    switch(Type)
    {
    case ECurrencyType::Credits:   return Credits;
    case ECurrencyType::Premium:   return Premium;
    case ECurrencyType::Faction:   return FactionTokens;
    case ECurrencyType::Salvage:   return Salvage;
    }
    // SciencePoints/Reputation 复用 Credits 槽位或扩展
    return Credits;
}

const int32& UCurrencyComponent::GetRef(ECurrencyType Type) const
{
    switch(Type)
    {
    case ECurrencyType::Credits:   return Credits;
    case ECurrencyType::Premium:   return Premium;
    case ECurrencyType::Faction:   return FactionTokens;
    case ECurrencyType::Salvage:   return Salvage;
    }
    return Credits;
}

int32 UCurrencyComponent::GetAmount(ECurrencyType Type) const { return GetRef(Type); }

bool UCurrencyComponent::HasEnough(ECurrencyType Type, int32 Amount) const
{ return GetRef(Type) >= Amount; }

void UCurrencyComponent::ServerAdd_Implementation(ECurrencyType Type, int32 Amount)
{
    if (Amount <= 0) return;
    GetRef(Type) += Amount;
    OnCurrencyChanged.Broadcast(Type, GetRef(Type), Amount);
}

bool UCurrencyComponent::ServerSpend_Implementation(ECurrencyType Type, int32 Amount)
{
    if (!HasEnough(Type, Amount)) return false;
    GetRef(Type) -= Amount;
    OnCurrencyChanged.Broadcast(Type, GetRef(Type), -Amount);
    return true;
}

bool UCurrencyComponent::ServerTransfer_Implementation(ECurrencyType Type, int32 Amount, UCurrencyComponent* Target)
{
    if (!Target || !ServerSpend(Type, Amount)) return false;
    Target->ServerAdd(Type, Amount);
    return true;
}

TMap<ECurrencyType, int32> UCurrencyComponent::GetAllCurrencies() const
{
    TMap<ECurrencyType, int32> M;
    M.Add(ECurrencyType::Credits, Credits);
    M.Add(ECurrencyType::Premium, Premium);
    M.Add(ECurrencyType::Faction, FactionTokens);
    M.Add(ECurrencyType::Salvage, Salvage);
    return M;
}
