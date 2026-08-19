// AmmoAndConsumables.cpp
#include "Inventory/AmmoAndConsumables.h"
#include "Character/VitalsSystem.h"
#include "Character/InventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"

// ========== UAmmoGenerator ==========
FAmmoParameters UAmmoGenerator::GenerateAmmo(int32 Seed, EAmmoType Type)
{
    FRandomStream R(Seed);
    FAmmoParameters P; P.Type=Type;
    P.AmmoID=FName(*FString::Printf(TEXT("AMMO_%d_%d"),(int)Type,Seed));

    switch(Type)
    {
    case EAmmoType::PistolRound:
        P.Caliber=9.f;P.Mass=8.f;P.MuzzleVelocity=350.f;P.BaseDamage=18.f;
        P.ArmorPierce=0.1f;P.Range=25000.f;P.StackSize=15;break;
    case EAmmoType::SMGRound:
        P.Caliber=5.7f;P.Mass=5.f;P.MuzzleVelocity=400.f;P.BaseDamage=12.f;
        P.ArmorPierce=0.05f;P.Range=20000.f;P.StackSize=30;break;
    case EAmmoType::LightCaliber:
        P.Caliber=5.56f;P.Mass=4.f;P.MuzzleVelocity=950.f;P.BaseDamage=22.f;
        P.ArmorPierce=0.3f;P.Range=40000.f;P.StackSize=30;break;
    case EAmmoType::MediumCaliber:
        P.Caliber=7.62f;P.Mass=10.f;P.MuzzleVelocity=850.f;P.BaseDamage=35.f;
        P.ArmorPierce=0.25f;P.Range=50000.f;P.StackSize=20;break;
    case EAmmoType::HeavyCaliber:
        P.Caliber=12.7f;P.Mass=40.f;P.MuzzleVelocity=800.f;P.BaseDamage=65.f;
        P.ArmorPierce=0.6f;P.Range=60000.f;P.StackSize=10;break;
    case EAmmoType::SniperRound:
        P.Caliber=8.6f;P.Mass=12.f;P.MuzzleVelocity=900.f;P.BaseDamage=90.f;
        P.ArmorPierce=0.7f;P.Range=120000.f;P.DropoffStart=0.9f;P.StackSize=5;break;
    case EAmmoType::ShotgunShell:
        P.Caliber=18.5f;P.Mass=28.f;P.MuzzleVelocity=400.f;P.BaseDamage=12.f*8/*pellets*/;
        P.ArmorPierce=0.05f;P.Range=15000.f;P.DropoffStart=0.3f;P.StackSize=8;break;
    case EAmmoType::EnergyCell:
        P.Caliber=0.f;P.Mass=3.f;P.MuzzleVelocity=200000.f/*光速*/;P.BaseDamage=28.f;
        P.ArmorPierce=0.15f;P.Range=45000.f;P.StackSize=25;P.bTracer=true;break;
    case EAmmoType::PlasmaPack:
        P.Caliber=0.f;P.Mass=5.f;P.MuzzleVelocity=150000.f;P.BaseDamage=45.f;
        P.ArmorPierce=0.2f;P.Range=35000.f;P.FireChance=0.4f;P.FireDuration=3.f;
        P.StackSize=15;P.bTracer=true;break;
    case EAmmoType::RailSlug:
        P.Caliber=6.f;P.Mass=20.f;P.MuzzleVelocity=3000.f;P.BaseDamage=55.f;
        P.ArmorPierce=0.85f;P.Range=80000.f;P.DragCoefficient=0.05f;P.StackSize=10;break;
    case EAmmoType::Rocket:
        P.Caliber=84.f;P.Mass=500.f;P.MuzzleVelocity=300.f;P.BaseDamage=120.f;
        P.ArmorPierce=0.3f;P.Range=30000.f;P.bExplosive=true;P.ExplosionRadius=600.f;
        P.StackSize=3;break;
    case EAmmoType::Grenade:
        P.Caliber=40.f;P.Mass=200.f;P.MuzzleVelocity=80.f;P.BaseDamage=80.f;
        P.bExplosive=true;P.ExplosionRadius=400.f;P.FireChance=0.2f;P.FireDuration=5.f;
        P.StackSize=4;break;
    case EAmmoType::HomingMissile:
        P.Caliber=120.f;P.Mass=800.f;P.MuzzleVelocity=500.f;P.BaseDamage=200.f;
        P.bExplosive=true;P.ExplosionRadius=1000.f;P.StackSize=2;break;
    case EAmmoType::Mine:
        P.Caliber=0.f;P.Mass=300.f;P.BaseDamage=150.f;
        P.bExplosive=true;P.ExplosionRadius=800.f;P.StackSize=3;break;
    case EAmmoType::Flare:
        P.Caliber=25.f;P.Mass=50.f;P.MuzzleVelocity=60.f;P.BaseDamage=5.f;
        P.FireChance=1.f;P.FireDuration=30.f;P.StackSize=6;break;
    }

    // 随机元素附加
    if(R.FRand()<0.15f) P.FireChance=FMath::Max(P.FireChance,R.FRandRange(0.1f,0.4f));
    if(R.FRand()<0.1f)  P.ElectricChance=R.FRandRange(0.1f,0.3f);
    if(R.FRand()<0.08f) P.FrostChance=R.FRandRange(0.1f,0.3f);
    if(R.FRand()<0.08f) P.AcidChance=R.FRandRange(0.1f,0.25f);
    if(R.FRand()<0.05f) P.VoidChance=R.FRandRange(0.05f,0.15f);

    // 特殊标记
    if(R.FRand()<0.2f) P.bTracer=true;
    if(Type==EAmmoType::MediumCaliber && R.FRand()<0.3f) P.bArmorPiercing=true;
    if(Type==EAmmoType::PistolRound && R.FRand()<0.25f) P.bHollowPoint=true;
    if(Type==EAmmoType::SMGRound && R.FRand()<0.2f) P.bSubsonic=true;
    if(Type==EAmmoType::PlasmaPack && R.FRand()<0.3f) P.bIncendiary=true;

    // 视觉
    P.CasingColor=FLinearColor(R.FRandRange(0.5f,0.9f),R.FRandRange(0.4f,0.7f),R.FRandRange(0.2f,0.5f));
    P.CasingLength=R.FRandRange(0.3f,0.8f);
    P.CasingWidth=R.FRandRange(0.3f,0.7f);
    P.TracerColor=FLinearColor(R.FRandRange(0.5f,1.f),R.FRandRange(0.3f,0.9f),R.FRandRange(0.1f,0.6f));

    return P;
}

FAmmoParameters UAmmoGenerator::MutateAmmo(const FAmmoParameters& Base,int32 Seed,float S)
{
    FRandomStream R(Seed); FAmmoParameters P=Base;
    auto M=[&](float&v){if(R.FRand()<0.4f) v=FMath::Clamp(v+R.FRandRange(-S,S),0.01f,9999.f);};
    M(P.Mass);M(P.MuzzleVelocity);M(P.BaseDamage);M(P.ArmorPierce);M(P.Range);
    M(P.FireChance);M(P.ElectricChance);M(P.FrostChance);M(P.AcidChance);
    return P;
}

void UAmmoGenerator::ApplyRarityToAmmo(FAmmoParameters& P,EItemRarity Rarity)
{
    float DMult=1.f,PMult=1.f;
    switch(Rarity)
    {
    case EItemRarity::Common:break;
    case EItemRarity::Uncommon:DMult=1.15f;break;
    case EItemRarity::Rare:DMult=1.35f;PMult=1.2f;P.bTracer=true;break;
    case EItemRarity::Epic:DMult=1.6f;PMult=1.4f;P.FireChance=FMath::Max(P.FireChance,0.2f);break;
    case EItemRarity::Legendary:DMult=2.f;PMult=1.6f;P.bTracer=true;P.bIncendiary=true;break;
    case EItemRarity::Mythic:DMult=3.f;PMult=2.f;P.FireChance=FMath::Max(P.FireChance,0.5f);P.ElectricChance=FMath::Max(P.ElectricChance,0.3f);break;
    }
    P.BaseDamage*=DMult; P.ArmorPierce=FMath::Min(P.ArmorPierce*PMult,0.95f);
    P.Rarity=Rarity;
}

TArray<EAmmoType> UAmmoGenerator::GetCompatibleAmmoTypes(EWeaponClass WeaponClass)
{
    TArray<EAmmoType> T;
    switch(WeaponClass)
    {
    case EWeaponClass::Pistol:       T={EAmmoType::PistolRound,EAmmoType::LightCaliber}; break;
    case EWeaponClass::Rifle:        T={EAmmoType::LightCaliber,EAmmoType::MediumCaliber,EAmmoType::ArmorPiercing?EAmmoType::HeavyCaliber:EAmmoType::MediumCaliber}; break;
    case EWeaponClass::SMG:          T={EAmmoType::SMGRound,EAmmoType::PistolRound}; break;
    case EWeaponClass::Shotgun:      T={EAmmoType::ShotgunShell}; break;
    case EWeaponClass::Sniper:       T={EAmmoType::SniperRound,EAmmoType::RailSlug}; break;
    case EWeaponClass::EnergyPistol: T={EAmmoType::EnergyCell}; break;
    case EWeaponClass::EnergyRifle:  T={EAmmoType::EnergyCell,EAmmoType::PlasmaPack}; break;
    case EWeaponClass::PlasmaCaster: T={EAmmoType::PlasmaPack}; break;
    case EWeaponClass::Melee:        break; // 无弹药
    case EWeaponClass::Throwable:    T={EAmmoType::Grenade,EAmmoType::Flare}; break;
    }
    return T;
}

FVector UAmmoGenerator::CalculateTrajectory(const FAmmoParameters& Ammo,FVector Start,FVector Dir,float Gravity,float TimeStep)
{
    // 简化弹道：重力+阻力
    FVector Vel=Dir.GetSafeNormal()*Ammo.MuzzleVelocity*100.f; // cm/s
    FVector Pos=Start; float Drag=Ammo.DragCoefficient;
    for(int32 i=0;i<600;++i) // 最多 10 秒
    {
        Vel+=FVector(0,0,-Gravity*100*TimeStep); // 重力（cm）
        Vel*=FMath::Max(1.f-Drag*TimeStep,0.f);
        Pos+=Vel*TimeStep;
        if(Pos.Z<0) break;
    }
    return Pos;
}

float UAmmoGenerator::CalculateDamageAtRange(const FAmmoParameters& Ammo,float Distance)
{
    float DropStart=Ammo.DropoffStart*Ammo.Range;
    if(Distance<=DropStart) return Ammo.BaseDamage;
    float Falloff=1.f-(Distance-DropStart)/(Ammo.Range-DropStart);
    return Ammo.BaseDamage*FMath::Max(Falloff,0.2f);
}

// ========== UConsumableGenerator ==========
FConsumableParameters UConsumableGenerator::GenerateConsumable(int32 Seed,EConsumableType Type)
{
    FRandomStream R(Seed); FConsumableParameters P; P.Type=Type;
    P.ConsumableID=FName(*FString::Printf(TEXT("CON_%d_%d"),(int)Type,Seed));

    switch(Type)
    {
    // 医疗
    case EConsumableType::Medkit:
        P.HealthRestore=50.f;P.UseTime=2.f;P.Description=NSLOCTEXT("Consumable","MedkitDesc","治疗 50 点生命值");break;
    case EConsumableType::AdvancedMedkit:
        P.HealthRestore=100.f;P.RadiationCure=5.f;P.UseTime=3.f;P.Effectiveness=1.5f;break;
    case EConsumableType::TraumaKit:
        P.HealthRestore=30.f;P.BuffDuration=10.f;P.RegenBuff=5.f;P.UseTime=1.5f;break;
    case EConsumableType::StimPack:
        P.HealthRestore=15.f;P.EnergyRestore=40.f;P.SpeedBuff=0.2f;P.BuffDuration=15.f;P.UseTime=0.5f;break;
    case EConsumableType::Adrenaline:
        P.SpeedBuff=0.4f;P.DamageBuff=0.2f;P.BuffDuration=8.f;P.HealthRestore=-5.f/*副作用*/;P.UseTime=0.3f;break;
    // 食物
    case EConsumableType::Ration:
        P.HungerRestore=40.f;P.ThirstRestore=10.f;P.UseTime=2.f;break;
    case EConsumableType::MRE:
        P.HungerRestore=70.f;P.ThirstRestore=20.f;P.EnergyRestore=15.f;P.UseTime=3.f;break;
    case EConsumableType::FreshFood:
        P.HungerRestore=50.f;P.ThirstRestore=30.f;P.HealthRestore=5.f;P.UseTime=1.f;break;
    case EConsumableType::AlienFruit:
        P.HungerRestore=60.f;P.ThirstRestore=50.f;P.HealthRestore=10.f;P.SpeedBuff=0.1f;P.BuffDuration=20.f;P.UseTime=1.f;break;
    // 饮料
    case EConsumableType::Water:
        P.ThirstRestore=60.f;P.UseTime=1.f;break;
    case EConsumableType::EnergyDrink:
        P.ThirstRestore=20.f;P.EnergyRestore=50.f;P.SpeedBuff=0.15f;P.BuffDuration=12.f;P.UseTime=0.5f;break;
    case EConsumableType::AlienBrew:
        P.ThirstRestore=40.f;P.HealthRestore=15.f;P.DamageBuff=0.15f;P.BuffDuration=30.f;P.UseTime=1.f;break;
    case EConsumableType::Coffee:
        P.EnergyRestore=30.f;P.SpeedBuff=0.1f;P.BuffDuration=20.f;P.UseTime=0.5f;break;
    // 氧气
    case EConsumableType::OxygenTank:
        P.OxygenRestore=100.f;P.UseTime=1.f;break;
    case EConsumableType::OxygenCanister:
        P.OxygenRestore=40.f;P.UseTime=0.5f;break;
    case EConsumableType::EVAKit:
        P.OxygenRestore=80.f;P.SuitRepair=50.f;P.UseTime=2.f;break;
    // 能量
    case EConsumableType::Battery:
        P.EnergyRestore=60.f;P.UseTime=0.5f;break;
    case EConsumableType::PowerCell:
        P.EnergyRestore=100.f;P.UseTime=1.f;P.Effectiveness=1.5f;break;
    case EConsumableType::FusionCell:
        P.EnergyRestore=200.f;P.UseTime=1.5f;P.Effectiveness=2.f;break;
    // 工具
    case EConsumableType::RepairKit:
        P.SuitRepair=80.f;P.UseTime=3.f;break;
    case EConsumableType::Multitool:
        P.UseTime=0.1f;P.StackSize=1;break; // 非消耗但归类于此
    case EConsumableType::Welder:
        P.UseTime=0.1f;P.StackSize=1;break;
    case EConsumableType::Scanner:
        P.UseTime=0.1f;P.StackSize=1;break;
    // 特殊
    case EConsumableType::AntiRad:
        P.RadiationCure=10.f;P.UseTime=1.f;break;
    case EConsumableType::Antidote:
        P.ToxinCure=15.f;P.UseTime=1.f;break;
    case EConsumableType::Steroid:
        P.DamageBuff=0.3f;P.DefenseBuff=-0.1f/*副作用*/;P.BuffDuration=25.f;P.UseTime=0.5f;break;
    case EConsumableType::NeuralBooster:
        P.SpeedBuff=0.25f;P.DamageBuff=0.15f;P.StealthBuff=-0.2f/*副作用*/;P.BuffDuration=15.f;P.UseTime=0.5f;break;
    // 信号
    case EConsumableType::Flare:
        P.UseTime=0.5f;P.StackSize=3;break;
    case EConsumableType::SmokeGrenade:
        P.StealthBuff=0.5f;P.BuffDuration=10.f;P.UseTime=0.5f;P.StackSize=2;break;
    case EConsumableType::EMPGrenade:
        P.UseTime=0.5f;P.StackSize=2;break;
    case EConsumableType::DistressBeacon:
        P.UseTime=2.f;P.StackSize=1;break;
    }

    // 显示名
    static const TMap<EConsumableType,FString> Names={
        {EConsumableType::Medkit,TEXT("医疗包")},{EConsumableType::AdvancedMedkit,TEXT("高级医疗包")},
        {EConsumableType::TraumaKit,TEXT("创伤包")},{EConsumableType::StimPack,TEXT("兴奋剂")},
        {EConsumableType::Adrenaline,TEXT("肾上腺素")},{EConsumableType::Ration,TEXT("军粮")},
        {EConsumableType::MRE,TEXT("即食餐")},{EConsumableType::FreshFood,TEXT("新鲜食物")},
        {EConsumableType::AlienFruit,TEXT("外星果实")},{EConsumableType::Water,TEXT("水")},
        {EConsumableType::EnergyDrink,TEXT("能量饮料")},{EConsumableType::AlienBrew,TEXT("外星酿造")},
        {EConsumableType::Coffee,TEXT("咖啡")},{EConsumableType::OxygenTank,TEXT("氧气瓶")},
        {EConsumableType::OxygenCanister,TEXT("氧气罐")},{EConsumableType::EVAKit,TEXT("舱外活动套件")},
        {EConsumableType::Battery,TEXT("电池")},{EConsumableType::PowerCell,TEXT("能量电池")},
        {EConsumableType::FusionCell,TEXT("聚变电池")},{EConsumableType::RepairKit,TEXT("修理包")},
        {EConsumableType::AntiRad,TEXT("防辐射药")},{EConsumableType::Antidote,TEXT("解毒剂")},
        {EConsumableType::Steroid,TEXT("类固醇")},{EConsumableType::NeuralBooster,TEXT("神经增强剂")},
        {EConsumableType::Flare,TEXT("信号弹")},{EConsumableType::SmokeGrenade,TEXT("烟雾弹")},
        {EConsumableType::EMPGrenade,TEXT("电磁脉冲弹")},{EConsumableType::DistressBeacon,TEXT("求救信标")}
    };
    if(const FString* N=Names.Find(Type)) P.DisplayName=FText::FromString(*N);

    // 颜色
    P.ItemColor=FLinearColor(R.FRandRange(0.1f,0.9f),R.FRandRange(0.1f,0.9f),R.FRandRange(0.1f,0.9f));
    return P;
}

FConsumableParameters UConsumableGenerator::GenerateRandomConsumable(int32 Seed)
{
    FRandomStream R(Seed);
    int32 Count=(int32)EConsumableType::DistressBeacon+1;
    EConsumableType T=(EConsumableType)R.RandRange(0,Count-1);
    return GenerateConsumable(Seed,T);
}

void UConsumableGenerator::ApplyRarityToConsumable(FConsumableParameters& P,EItemRarity Rarity)
{
    float Mult=1.f;
    switch(Rarity)
    {
    case EItemRarity::Common:break;
    case EItemRarity::Uncommon:Mult=1.2f;break;
    case EItemRarity::Rare:Mult=1.5f;P.BuffDuration*=1.3f;break;
    case EItemRarity::Epic:Mult=2.f;P.BuffDuration*=1.5f;P.UseTime*=0.7f;break;
    case EItemRarity::Legendary:Mult=3.f;P.BuffDuration*=2.f;P.UseTime*=0.5f;break;
    case EItemRarity::Mythic:Mult=5.f;P.BuffDuration*=3.f;P.UseTime*=0.3f;break;
    }
    P.HealthRestore*=Mult;P.OxygenRestore*=Mult;P.EnergyRestore*=Mult;
    P.HungerRestore*=Mult;P.ThirstRestore*=Mult;P.RadiationCure*=Mult;
    P.ToxinCure*=Mult;P.SuitRepair*=Mult;P.Effectiveness*=Mult;
    P.Rarity=Rarity;
}

bool UConsumableGenerator::UseConsumable(APawn* User,const FConsumableParameters& C)
{
    if(!User) return false;
    UVitalsComponent* Vitals=User->FindComponentByClass<UVitalsComponent>();
    if(!Vitals) return false;

    if(C.HealthRestore!=0)   Vitals->ServerHeal(C.HealthRestore*C.Effectiveness);
    if(C.OxygenRestore!=0)   Vitals->ServerConsumeOxygen(C.OxygenRestore*C.Effectiveness);
    if(C.EnergyRestore!=0)   Vitals->ServerConsumeEnergy(C.EnergyRestore*C.Effectiveness);
    if(C.RadiationCure!=0)  Vitals->ServerCureRadiation(C.RadiationCure*C.Effectiveness);
    if(C.ToxinCure!=0)      Vitals->ServerCureToxin(C.ToxinCure*C.Effectiveness);
    if(C.SuitRepair!=0)     Vitals->ServerRepairSuit(C.SuitRepair*C.Effectiveness);
    if(C.HungerRestore>0||C.ThirstRestore>0)
        Vitals->ServerFeed(C.HungerRestore*C.Effectiveness,C.ThirstRestore*C.Effectiveness);

    // Buff 处理（由外部系统管理 ActiveBuffs）
    // if(C.BuffDuration>0) AddBuff(...)
    return true;
}

// ========== UAmmoInventoryComponent ==========
UAmmoInventoryComponent::UAmmoInventoryComponent(){SetIsReplicatedByDefault(true);}

void UAmmoInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{DOREPLIFETIME(UAmmoInventoryComponent,AmmoStock);}

int32 UAmmoInventoryComponent::GetAmmoCount(FName AmmoID) const
{return AmmoStock.FindRef(AmmoID);}

int32 UAmmoInventoryComponent::GetAmmoCountByType(EAmmoType Type) const
{int32 Total=0;for(const auto& KV:AmmoStock){/*按类型筛选*/Total+=KV.Value;}return Total;}

void UAmmoInventoryComponent::ServerAddAmmo_Implementation(FName AmmoID,int32 Amount)
{AmmoStock.FindOrAdd(AmmoID)+=Amount;}

bool UAmmoInventoryComponent::ServerConsumeAmmo_Implementation(FName AmmoID,int32 Amount)
{
    int32& Count=AmmoStock.FindOrAdd(AmmoID);
    if(Count<Amount) return false;
    Count-=Amount;
    if(Count<=0) AmmoStock.Remove(AmmoID);
    return true;
}

void UAmmoInventoryComponent::ServerAddAmmoOfType_Implementation(EAmmoType Type,int32 Amount,int32 Seed)
{
    if(Seed==0) Seed=FMath::Rand();
    FAmmoParameters P=UAmmoGenerator::GenerateAmmo(Seed,Type);
    AmmoStock.FindOrAdd(P.AmmoID)+=Amount;
}

bool UAmmoInventoryComponent::IsCompatibleWithWeapon(EAmmoType AmmoType,EWeaponClass WeaponClass) const
{
    TArray<EAmmoType> Compat=UAmmoGenerator::GetCompatibleAmmoTypes(WeaponClass);
    return Compat.Contains(AmmoType);
}

FName UAmmoInventoryComponent::GetBestAmmoForWeapon(EWeaponClass WeaponClass) const
{
    TArray<EAmmoType> Compat=UAmmoGenerator::GetCompatibleAmmoTypes(WeaponClass);
    FName Best=TEXT("None"); int32 BestCount=0;
    for(const auto& KV:AmmoStock)
    {
        // 解析 AmmoID 找类型（简化：遍历兼容列表比对）
        for(EAmmoType T:Compat)
        {
            FString Prefix=UAmmoTypeToString(T); // 需要辅助函数
            if(KV.Key.ToString().Contains(Prefix)&&KV.Value>BestCount)
            {Best=KV.Key;BestCount=KV.Value;}
        }
    }
    return Best;
}

FAmmoParameters UAmmoInventoryComponent::GetAmmoParams(FName AmmoID) const
{
    // 从 GameMode 的弹药注册表查询（简化）
    return FAmmoParameters();
}

// ========== UConsumableInventoryComponent ==========
UConsumableInventoryComponent::UConsumableInventoryComponent(){SetIsReplicatedByDefault(true);}

void UConsumableInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{DOREPLIFETIME(UConsumableInventoryComponent,ConsumableStock);DOREPLIFETIME(UConsumableInventoryComponent,HotbarSlots);DOREPLIFETIME(UConsumableInventoryComponent,ActiveBuffs);}

int32 UConsumableInventoryComponent::GetCount(FName ConsumableID) const
{return ConsumableStock.FindRef(ConsumableID);}

TArray<FName> UConsumableInventoryComponent::GetAllConsumables() const
{
    TArray<FName> Out;
    for(const auto& KV:ConsumableStock) if(KV.Value>0) Out.Add(KV.Key);
    return Out;
}

void UConsumableInventoryComponent::ServerAddConsumable_Implementation(FName ConsumableID,int32 Amount)
{ConsumableStock.FindOrAdd(ConsumableID)+=Amount;}

bool UConsumableInventoryComponent::ServerUseConsumable_Implementation(FName ConsumableID,APawn* User)
{
    int32& Count=ConsumableStock.FindOrAdd(ConsumableID);
    if(Count<=0) return false;
    // 获取参数并应用
    FConsumableParameters Params; // 从注册表查
    if(UConsumableGenerator::UseConsumable(User,Params))
    {Count--;if(Count<=0)ConsumableStock.Remove(ConsumableID);return true;}
    return false;
}

bool UConsumableInventoryComponent::ServerUseFromHotbar_Implementation(int32 Slot,APawn* User)
{
    if(FName* ID=HotbarSlots.Find(Slot)) return ServerUseConsumable(*ID,User);
    return false;
}

void UConsumableInventoryComponent::ServerAssignHotbar_Implementation(int32 Slot,FName ConsumableID)
{
    if(Slot<0||Slot>9) return;
    HotbarSlots.Add(Slot,ConsumableID);
}

void UConsumableInventoryComponent::ServerClearHotbar_Implementation(int32 Slot)
{HotbarSlots.Remove(Slot);}
