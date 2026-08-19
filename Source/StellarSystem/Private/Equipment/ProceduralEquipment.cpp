// ProceduralEquipment.cpp
#include "Equipment/ProceduralEquipment.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/UnrealMathUtility.h"

// ============ 工具函数 ============
namespace
{
    void PushQuad(TArray<int32>& T, int32 a,int32 b,int32 c,int32 d)
    { T.Add(a);T.Add(b);T.Add(c);T.Add(c);T.Add(b);T.Add(d); }

    float SmoothStep(float a, float b, float x)
    { return FMath::Clamp((x-a)/(b-a),0.f,1.f); }

    FVector RandOffset(FRandomStream& R, float S)
    { return FVector(R.FRandRange(-S,S),R.FRandRange(-S,S),R.FRandRange(-S,S)); }
}

// ============ UArmorGenerator ============
FArmorParameters UArmorGenerator::GenerateArmor(int32 Seed, EArmorSlot Slot, EArmorType Type)
{
    FRandomStream R(Seed);
    FArmorParameters P;
    P.Slot = Slot; P.Type = Type;
    P.ArmorID = FName(*FString::Printf(TEXT("ARM_%d"), Seed));

    // 基础随机
    P.PlateThickness = R.FRandRange(0.3f,0.7f);
    P.SegmentCount  = R.FRandRange(0.2f,0.8f);
    P.Angularity    = R.FRandRange(0.2f,0.8f);
    P.SurfaceDetail = R.FRandRange(0.2f,0.8f);
    P.Vents         = R.FRandRange(0.1f,0.5f);
    P.Pauldrons    = (Slot==EArmorSlot::Chest) ? R.FRandRange(0.3f,0.8f) : 0.f;
    P.TrimWidth     = R.FRandRange(0.2f,0.6f);

    // 类型影响
    switch(Type)
    {
    case EArmorType::Light:  P.PlateThickness*=0.6f; P.MobilityPenalty=-5.f;  P.DefenseRating=25.f; break;
    case EArmorType::Medium: P.DefenseRating=50.f; P.MobilityPenalty=0.f; break;
    case EArmorType::Heavy:  P.PlateThickness=FMath::Min(P.PlateThickness*1.4f,1.f); P.DefenseRating=80.f; P.MobilityPenalty=10.f; break;
    case EArmorType::Powered:P.DefenseRating=70.f; P.EnergyGlow=0.6f; P.Vents*=1.3f; break;
    case EArmorType::Stealth:P.DefenseRating=40.f; P.Metallic=0.3f; P.Roughness=0.9f; P.PrimaryColor=FLinearColor(0.05,0.05,0.08,1); break;
    case EArmorType::Hazard: P.DefenseRating=60.f; P.EnergyResistance=30; P.ThermalResistance=30; P.RadiationResistance=40; P.ToxinResistance=30; break;
    }

    // 颜色
    P.PrimaryColor   = FLinearColor(R.FRandRange(0.1f,0.4f),R.FRandRange(0.1f,0.4f),R.FRandRange(0.12f,0.45f));
    P.SecondaryColor = FLinearColor(P.PrimaryColor.R*0.5f,P.PrimaryColor.G*0.5f,P.PrimaryColor.B*0.5f);
    P.AccentColor    = FLinearColor(R.FRandRange(0.6f,1.f),R.FRandRange(0.4f,0.8f),R.FRandRange(0.1f,0.4f));
    P.Metallic  = R.FRandRange(0.5f,0.9f);
    P.Roughness = R.FRandRange(0.2f,0.6f);
    P.WearAndTear = R.FRandRange(0.1f,0.4f);

    ApplyFactionTheme(P, R);
    return P;
}

FArmorParameters UArmorGenerator::MutateArmor(const FArmorParameters& Base, int32 Seed, float S)
{
    FRandomStream R(Seed); FArmorParameters P=Base;
    auto M=[&](float&v){ if(R.FRand()<0.5f) v=FMath::Clamp(v+R.FRandRange(-S,S),0.f,1.f); };
    M(P.PlateThickness);M(P.SegmentCount);M(P.Angularity);M(P.SurfaceDetail);
    M(P.Vents);M(P.Pauldrons);M(P.TrimWidth);M(P.Metallic);M(P.Roughness);
    M(P.WearAndTear);M(P.EnergyGlow);
    P.DefenseRating+=R.FRandRange(-S*20,S*20);
    return P;
}

void UArmorGenerator::ApplyRarityScaling(FArmorParameters& P, EItemRarity Rarity)
{
    float Mult=1.f;
    switch(Rarity)
    {
    case EItemRarity::Common:    Mult=1.f;   break;
    case EItemRarity::Uncommon:  Mult=1.2f;  P.EnergyGlow=FMath::Max(P.EnergyGlow,0.2f); break;
    case EItemRarity::Rare:     Mult=1.5f;  P.EnergyGlow=FMath::Max(P.EnergyGlow,0.4f); break;
    case EItemRarity::Epic:     Mult=2.f;   P.EnergyGlow=FMath::Max(P.EnergyGlow,0.6f); P.AccentColor=FLinearColor(0.6f,0.2f,0.9f); break;
    case EItemRarity::Legendary: Mult=3.f;   P.EnergyGlow=1.f; P.AccentColor=FLinearColor(1.f,0.7f,0.1f); break;
    case EItemRarity::Mythic:   Mult=5.f;   P.EnergyGlow=1.f; P.AccentColor=FLinearColor(0.2f,0.9f,1.f); P.WearAndTear=0.f; break;
    }
    P.DefenseRating*=Mult;
    P.EnergyResistance*=Mult; P.ThermalResistance*=Mult;
    P.RadiationResistance*=Mult; P.ToxinResistance*=Mult;
    P.Rarity=Rarity;
}

void UArmorGenerator::ApplyFactionTheme(FArmorParameters& P, FRandomStream& R)
{
    int32 Theme=R.RandRange(0,5);
    switch(Theme)
    {
    case 0: P.PrimaryColor=FLinearColor(0.15f,0.2f,0.35f); P.AccentColor=FLinearColor(0.8f,0.7f,0.1f); P.FactionTheme="Empire"; break;
    case 1: P.PrimaryColor=FLinearColor(0.3f,0.1f,0.1f); P.AccentColor=FLinearColor(0.9f,0.2f,0.2f); P.FactionTheme="Crimson"; break;
    case 2: P.PrimaryColor=FLinearColor(0.1f,0.3f,0.2f); P.AccentColor=FLinearColor(0.2f,0.9f,0.5f); P.FactionTheme="Verdant"; break;
    case 3: P.PrimaryColor=FLinearColor(0.2f,0.2f,0.3f); P.AccentColor=FLinearColor(0.6f,0.4f,0.9f); P.FactionTheme="Void"; break;
    case 4: P.PrimaryColor=FLinearColor(0.35f,0.3f,0.15f); P.AccentColor=FLinearColor(1.f,0.8f,0.3f); P.FactionTheme="Gold"; break;
    case 5: P.PrimaryColor=FLinearColor(0.1f,0.1f,0.1f); P.AccentColor=FLinearColor(0.3f,0.3f,0.3f); P.FactionTheme="Nomad"; break;
    }
}

void UArmorGenerator::BuildArmorMesh(UProceduralMeshComponent* M, const FArmorParameters& P)
{
    if(!M) return; M->ClearAllMeshSections();
    TArray<FVector> V; TArray<int32> T;
    TArray<FVector> N; TArray<FVector2D> UV; TArray<FColor> C; TArray<FProcMeshTangent> Tan;
    FRandomStream R(FMath::Rand());

    switch(P.Slot)
    {
    case EArmorSlot::Chest:   GenerateChestPlate(V,T,P); break;
    case EArmorSlot::Head:    GenerateHelmet(V,T,P); break;
    case EArmorSlot::Arms:    GenerateGauntlets(V,T,P); break;
    case EArmorSlot::Legs:    GenerateGreaves(V,T,P); break;
    case EArmorSlot::Feet:    GenerateBoots(V,T,P); break;
    case EArmorSlot::Shield:  // 盾牌：大圆盘+能量面
        for(int32 i=0;i<=16;++i){float A=(float)i/16*2*PI; V.Add(FVector(FMath::Cos(A)*25,FMath::Sin(A)*25,0));}
        V.Add(FVector::ZeroVector); int32 CIdx=V.Num()-1;
        for(int32 i=0;i<16;++i){T.Add(i);T.Add((i+1)%16);T.Add(CIdx);}
        break;
    }

    // 表面细节扰动
    AddSurfaceDetail(V,P,R);

    N.SetNum(V.Num()); UV.SetNum(V.Num()); C.SetNum(V.Num()); Tan.SetNum(V.Num());
    for(int32 i=0;i<V.Num();++i) C[i]=FColor(120,120,130,255);

    M->CreateMeshSection(0,V,T,N,UV,C,Tan,true);
}

void UArmorGenerator::GenerateChestPlate(TArray<FVector>& V,TArray<int32>& T,const FArmorParameters& P)
{
    float W=15.f*FMath::Lerp(0.7f,1.3f,P.PlateThickness);
    float H=25.f; float D=8.f*FMath::Lerp(0.6f,1.2f,P.PlateThickness);
    int32 S=FMath::RoundToInt(FMath::Lerp(2.f,6.f,P.SegmentCount));

    // 前板
    for(int32 i=0;i<=S;++i){float t=(float)i/S; float y=FMath::Lerp(-H*0.5f,H*0.5f,t);
        V.Add(FVector(-W,y,-D*0.5f)); V.Add(FVector(W,y,-D*0.5f));
    }
    for(int32 i=0;i<S;++i){int32 a=i*2,b=a+2; PushQuad(T,a,a+1,b+1,b);}
    // 后板
    int32 B=V.Num();
    for(int32 i=0;i<=S;++i){float t=(float)i/S; float y=FMath::Lerp(-H*0.5f,H*0.5f,t);
        V.Add(FVector(-W*0.9f,y,D*0.5f)); V.Add(FVector(W*0.9f,y,D*0.5f));
    }
    for(int32 i=0;i<S;++i){int32 a=B+i*2,b=a+2; PushQuad(T,a+1,a,b,b+1);}
    // 肩甲
    if(P.Pauldrons>0.3f){
        int32 L=V.Num();
        float PS=P.Pauldrons*10.f;
        V.Add(FVector(-W-2,y,-D*0.3f)); V.Add(FVector(-W-2-PS,0,-D*0.5f)); V.Add(FVector(-W-2,y*0.5f,D*0.2f));
        V.Add(FVector(-W-2-PS*0.5f,0,D*0.3f));
        T.Add(L);T.Add(L+1);T.Add(L+2);T.Add(L+2);T.Add(L+1);T.Add(L+3);
        // 右侧镜像省略（同理）
    }
}

void UArmorGenerator::GeneratePauldron(TArray<FVector>& V,TArray<int32>& T,const FArmorParameters& P,bool bLeft)
{
    // 简略：一个扇形板
    float Sign=bLeft?-1.f:1.f; int32 N=8;
    int32 B=V.Num();
    for(int32 i=0;i<=N;++i){float A=(float)i/N*PI*0.6f-FMath::PI*0.3f;
        V.Add(FVector(Sign*12.f,FMath::Sin(A)*10.f,FMath::Cos(A)*8.f));
    }
    V.Add(FVector(Sign*16.f,0,0)); int32 Tip=V.Num()-1;
    for(int32 i=0;i<N;++i){T.Add(B+i);T.Add(B+(i+1)%N);T.Add(Tip);}
}

void UArmorGenerator::GenerateHelmet(TArray<FVector>& V,TArray<int32>& T,const FArmorParameters& P)
{
    int32 Segs=12; float Rad=10.f*FMath::Lerp(0.8f,1.2f,P.PlateThickness);
    for(int32 i=0;i<=Segs;++i){float Vr=(float)i/Segs; float Phi=Vr*PI*0.9f+0.05f*PI;
        for(int32 j=0;j<=Segs;++j){float Ur=(float)j/Segs; float Th=Ur*2*PI;
            FVector S(FMath::Sin(Phi)*FMath::Cos(Th),FMath::Sin(Phi)*FMath::Sin(Th),FMath::Cos(Phi));
            V.Add(S*Rad);
        }
    }
    int32 Stride=Segs+1;
    for(int32 i=0;i<Segs;++i) for(int32 j=0;j<Segs;++j){
        int32 a=i*Stride+j,b=a+Stride;
        T.Add(a);T.Add(b);T.Add(a+1);T.Add(a+1);T.Add(b);T.Add(b+1);
    }
    // 眼部狭缝
    if(P.Type==EArmorType::Stealth||P.Type==EArmorType::Powered){
        int32 Slit=V.Num();
        V.Add(FVector(0,-Rad*0.95f,-2)); V.Add(FVector(0,-Rad*0.95f,2));
        V.Add(FVector(0,-Rad*0.95f+0.5f,0));
        T.Add(Slit);T.Add(Slit+1);T.Add(Slit+2);
    }
}

void UArmorGenerator::GenerateGreaves(TArray<FVector>& V,TArray<int32>& T,const FArmorParameters& P)
{
    float W=10.f*FMath::Lerp(0.7f,1.2f,P.PlateThickness); float H=22.f; float D=7.f;
    V.Add(FVector(-W,-H*0.5f,-D)); V.Add(FVector(W,-H*0.5f,-D));
    V.Add(FVector(-W,H*0.5f,-D));  V.Add(FVector(W,H*0.5f,-D));
    V.Add(FVector(-W*0.8f,0,D));   V.Add(FVector(W*0.8f,0,D));
    PushQuad(T,0,1,3,2); PushQuad(T,0,2,4,4); PushQuad(T,1,5,3,3); PushQuad(T,4,2,5,3);
}

void UArmorGenerator::GenerateBoots(TArray<FVector>& V,TArray<int32>& T,const FArmorParameters& P)
{
    float W=8.f; float L=14.f; float H=6.f;
    V.Add(FVector(-W,-L*0.5f,-H)); V.Add(FVector(W,-L*0.5f,-H));
    V.Add(FVector(-W,L*0.5f,-H));  V.Add(FVector(W,L*0.5f,-H));
    V.Add(FVector(-W*0.7f,L*0.5f,H)); V.Add(FVector(W*0.7f,L*0.5f,H));
    V.Add(FVector(-W*0.5f,-L*0.3f,H*0.5f)); V.Add(FVector(W*0.5f,-L*0.3f,H*0.5f));
    PushQuad(T,0,1,3,2); PushQuad(T,2,3,5,4); PushQuad(T,0,2,6,7); PushQuad(T,1,7,3,3);
}

void UArmorGenerator::GenerateGauntlets(TArray<FVector>& V,TArray<int32>& T,const FArmorParameters& P)
{
    float W=5.f; float L=12.f; float D=4.f;
    V.Add(FVector(-W,-L*0.5f,-D)); V.Add(FVector(W,-L*0.5f,-D));
    V.Add(FVector(-W,L*0.5f,-D));  V.Add(FVector(W,L*0.5f,-D));
    V.Add(FVector(-W*0.6f,L*0.5f,D)); V.Add(FVector(W*0.6f,L*0.5f,D));
    PushQuad(T,0,1,3,2); PushQuad(T,2,3,5,4);
}

void UArmorGenerator::AddSurfaceDetail(TArray<FVector>& V,const FArmorParameters& P,FRandomStream& R)
{
    float Amt=P.SurfaceDetail*2.f;
    for(FVector& v:V){
        v+=RandOffset(R,Amt);
    }
    // 散热口
    if(P.Vents>0.3f){
        // 简化：在背板加几个凹坑（略）
    }
}

// ============ UWeaponGenerator ============
FWeaponParameters UWeaponGenerator::GenerateWeapon(int32 Seed, EWeaponClass Class)
{
    FRandomStream R(Seed);
    FWeaponParameters P;
    P.WeaponID=FName(*FString::Printf(TEXT("WPN_%d"),Seed));
    P.Class=Class;

    // 几何
    P.BarrelLength  =R.FRandRange(0.3f,0.8f);
    P.BarrelThickness=R.FRandRange(0.3f,0.7f);
    P.ReceiverSize  =R.FRandRange(0.3f,0.7f);
    P.StockStyle    =R.FRandRange(0.2f,0.8f);
    P.GripAngle     =R.FRandRange(0.3f,0.7f);
    P.MagazineSize  =R.FRandRange(0.3f,0.8f);
    P.MagazineShape =R.FRandRange(0.2f,0.8f);
    P.ScopeSize     =R.FRandRange(0.1f,0.6f);
    P.RailCount     =R.FRandRange(0.1f,0.7f);
    P.MuzzleDevice  =R.FRandRange(0.1f,0.6f);

    // 类型影响
    switch(Class)
    {
    case EWeaponClass::Pistol:       P.Damage=20;P.FireRate=300;P.MagazineCapacity=12;P.Accuracy=0.85f;P.Stability=0.8f;P.Range=30000;P.WeaponMass=1.f;break;
    case EWeaponClass::Rifle:        P.Damage=35;P.FireRate=600;P.MagazineCapacity=30;P.Accuracy=0.8f;P.Stability=0.7f;P.Range=60000;P.WeaponMass=3.f;break;
    case EWeaponClass::SMG:          P.Damage=18;P.FireRate=900;P.MagazineCapacity=40;P.Accuracy=0.65f;P.Stability=0.5f;P.Range=35000;P.WeaponMass=2.f;break;
    case EWeaponClass::Shotgun:      P.Damage=60;P.FireRate=80;P.MagazineCapacity=6;P.Accuracy=0.4f;P.Stability=0.6f;P.Range=20000;P.WeaponMass=3.5f;break;
    case EWeaponClass::Sniper:       P.Damage=90;P.FireRate=40;P.MagazineCapacity=5;P.Accuracy=0.98f;P.Stability=0.9f;P.Range=120000;P.WeaponMass=5.f;P.CriticalChance=0.2f;P.CriticalMultiplier=2.5f;break;
    case EWeaponClass::EnergyPistol: P.Damage=25;P.FireRate=400;P.HeatPerShot=4;P.Accuracy=0.9f;P.WeaponMass=1.2f;P.GlowColor=FLinearColor(0,0.7f,1);break;
    case EWeaponClass::EnergyRifle:  P.Damage=40;P.FireRate=500;P.HeatPerShot=6;P.Accuracy=0.85f;P.WeaponMass=3.2f;P.GlowColor=FLinearColor(0,0.9f,0.8f);break;
    case EWeaponClass::PlasmaCaster: P.Damage=55;P.FireRate=120;P.HeatPerShot=12;P.ChargeTime=0.8f;P.Accuracy=0.75f;P.GlowColor=FLinearColor(0.9f,0.3f,0.9f);break;
    case EWeaponClass::Melee:        P.Damage=70;P.FireRate=60;P.MagazineCapacity=999;P.Range=200;P.WeaponMass=2.5f;break;
    case EWeaponClass::Throwable:    P.Damage=45;P.FireRate=30;P.MagazineCapacity=3;P.Range=15000;P.WeaponMass=0.5f;break;
    }
    // 火控模式
    switch(Class){
    case EWeaponClass::Sniper:case EWeaponClass::Melee: P.FireMode=EFireMode::Semi;break;
    case EWeaponClass::Shotgun:case EWeaponClass::PlasmaCaster: P.FireMode=EFireMode::Charge;break;
    case EWeaponClass::EnergyRifle:case EWeaponClass::Rifle: P.FireMode=EFireMode::Auto;break;
    default: P.FireMode=EFireMode::Semi;
    }

    // 元素伤害（30% 概率）
    if(R.FRand()<0.3f) P.FireDamage=R.FRandRange(5,15);
    if(R.FRand()<0.2f) P.ElectricDamage=R.FRandRange(3,12);
    if(R.FRand()<0.15f) P.FrostDamage=R.FRandRange(5,20);
    if(R.FRand()<0.15f) P.AcidDamage=R.FRandRange(4,10);
    if(R.FRand()<0.1f)  P.VoidDamage=R.FRandRange(8,25);

    // 颜色
    P.BodyColor=FLinearColor(R.FRandRange(0.1f,0.3f),R.FRandRange(0.1f,0.3f),R.FRandRange(0.12f,0.35f));
    P.TrimColor=FLinearColor(R.FRandRange(0.4f,0.8f),R.FRandRange(0.4f,0.8f),R.FRandRange(0.4f,0.8f));
    P.BodyMetallic=R.FRandRange(0.6f,0.95f);
    P.BodyRoughness=R.FRandRange(0.2f,0.5f);
    P.GripTexture=R.FRandRange(0.3f,0.8f);

    // 词条
    P.Affixes=RollAffixes(Seed,EItemRarity::Common,Class);
    return P;
}

FWeaponParameters UWeaponGenerator::MutateWeapon(const FWeaponParameters& Base,int32 Seed,float S)
{
    FRandomStream R(Seed); FWeaponParameters P=Base;
    auto M=[&](float&v){if(R.FRand()<0.5f) v=FMath::Clamp(v+R.FRandRange(-S,S),0.01f,999.f);};
    M(P.Damage);M(P.FireRate);M(P.Accuracy);M(P.Stability);M(P.Range);
    M(P.MagazineCapacity);M(P.ReloadTime);M(P.CriticalChance);M(P.CriticalMultiplier);
    M(P.BarrelLength);M(P.ReceiverSize);M(P.GripAngle);
    return P;
}

void UWeaponGenerator::ApplyRarityScaling(FWeaponParameters& P,EItemRarity Rarity)
{
    float DmgMult=1.f,AccMult=1.f;
    switch(Rarity){
    case EItemRarity::Common:break;
    case EItemRarity::Uncommon:DmgMult=1.15f;break;
    case EItemRarity::Rare:DmgMult=1.35f;AccMult=1.1f;break;
    case EItemRarity::Epic:DmgMult=1.6f;AccMult=1.2f;P.CriticalChance+=0.1f;break;
    case EItemRarity::Legendary:DmgMult=2.f;AccMult=1.3f;P.CriticalChance+=0.15f;P.CriticalMultiplier+=0.5f;break;
    case EItemRarity::Mythic:DmgMult=3.f;AccMult=1.5f;P.CriticalChance+=0.2f;P.CriticalMultiplier+=1.f;P.GlowColor=FLinearColor(0.2f,0.9f,1.f);break;
    }
    P.Damage*=DmgMult; P.Accuracy=FMath::Clamp(P.Accuracy*AccMult,0.f,1.f);
    P.Rarity=Rarity;
    if(Rarity>=EItemRarity::Rare) P.Affixes.Add(TEXT("Stability"));
    if(Rarity>=EItemRarity::Epic) P.Affixes.Add(TEXT("Penetration"));
    if(Rarity>=EItemRarity::Legendary) P.Affixes.Add(TEXT("Overcharge"));
}

TArray<FName> UWeaponGenerator::RollAffixes(int32 Seed,EItemRarity Rarity,EWeaponClass Class)
{
    FRandomStream R(Seed);
    TArray<FName> Pool={
        TEXT("ExtendedMag"),TEXT("QuickReload"),TEXT("RecoilComp"),TEXT("HairTrigger"),
        TEXT("ArmorPierce"),TEXT("Stability"),TEXT("Penetration"),TEXT("Overcharge"),
        TEXT("FireRate+"),TEXT("CritChance+"),TEXT("Elemental+"),TEXT("Range+"),
        TEXT("MeleeReach"),TEXT("ThrowDistance"),TEXT("AmmoEfficiency")
    };
    int32 Count=0;
    switch(Rarity){case EItemRarity::Common:Count=0;break;case EItemRarity::Uncommon:Count=1;break;
    case EItemRarity::Rare:Count=2;break;case EItemRarity::Epic:Count=3;break;
    case EItemRarity::Legendary:Count=4;break;case EItemRarity::Mythic:Count=6;break;}
    TArray<FName> Result; TArray<FName> Remaining=Pool;
    for(int32 i=0;i<Count&&Remaining.Num()>0;++i){
        int32 Idx=R.RandRange(0,Remaining.Num()-1);
        Result.Add(Remaining[Idx]); Remaining.RemoveAt(Idx);
    }
    return Result;
}

void UWeaponGenerator::BuildWeaponMesh(UProceduralMeshComponent* M,const FWeaponParameters& P)
{
    if(!M) return; M->ClearAllMeshSections();
    TArray<FVector> V; TArray<int32> T;
    TArray<FVector> N; TArray<FVector2D> UV; TArray<FColor> C; TArray<FProcMeshTangent> Tan;
    FRandomStream R(FMath::Rand());

    GenerateBarrel(V,T,P);
    GenerateReceiver(V,T,P);
    GenerateStock(V,T,P);
    GenerateGrip(V,T,P);
    GenerateMagazine(V,T,P);
    if(P.ScopeSize>0.2f) GenerateScope(V,T,P);
    if(P.MuzzleDevice>0.2f) GenerateMuzzle(V,T,P);
    AddWeaponDetail(V,P,R);

    N.SetNum(V.Num());UV.SetNum(V.Num());C.SetNum(V.Num());Tan.SetNum(V.Num());
    for(int32 i=0;i<V.Num();++i) C[i]=FColor(80,80,85,255);
    M->CreateMeshSection(0,V,T,N,UV,C,Tan,true);
}

void UWeaponGenerator::GenerateBarrel(TArray<FVector>& V,TArray<int32>& T,const FWeaponParameters& P)
{
    float L=P.BarrelLength*30.f; float R=P.BarrelThickness*2.f+0.5f;
    int32 Segs=8; int32 B=V.Num();
    for(int32 i=0;i<=Segs;++i){float A=(float)i/Segs*2*PI;
        V.Add(FVector(-L*0.8f,FMath::Cos(A)*R,FMath::Sin(A)*R));
        V.Add(FVector(-L*0.2f,FMath::Cos(A)*R*0.9f,FMath::Sin(A)*R*0.9f));
    }
    for(int32 i=0;i<Segs;++i){int32 a=B+i*2,b=B+(i+1)*2;
        T.Add(a);T.Add(b);T.Add(a+1);T.Add(a+1);T.Add(b);T.Add(b+1);}
}

void UWeaponGenerator::GenerateReceiver(TArray<FVector>& V,TArray<int32>& T,const FWeaponParameters& P)
{
    float W=P.ReceiverSize*8.f; float H=P.ReceiverSize*5.f; float D=P.ReceiverSize*4.f;
    int32 B=V.Num();
    V.Add(FVector(0,-H,-D));V.Add(FVector(W,-H,-D));V.Add(FVector(0,H,-D));V.Add(FVector(W,H,-D));
    V.Add(FVector(0,-H,D));V.Add(FVector(W,-H,D));V.Add(FVector(0,H,D));V.Add(FVector(W,H,D));
    PushQuad(T,0,1,3,2);PushQuad(T,4,6,7,5);PushQuad(T,0,2,6,4);PushQuad(T,1,5,7,3);
}

void UWeaponGenerator::GenerateStock(TArray<FVector>& V,TArray<int32>& T,const FWeaponParameters& P)
{
    float L=P.StockStyle*15.f; float W=4.f;
    int32 B=V.Num();
    V.Add(FVector(L,-W,-2));V.Add(FVector(L+5,-W*0.8f,-1.5f));
    V.Add(FVector(L,W,-2));V.Add(FVector(L+5,W*0.8f,-1.5f));
    V.Add(FVector(L,0,2));V.Add(FVector(L+5,0,1.5f));
    PushQuad(T,0,1,3,2); PushQuad(T,0,2,4,4); PushQuad(T,1,5,3,3);
}

void UWeaponGenerator::GenerateGrip(TArray<FVector>& V,TArray<int32>& T,const FWeaponParameters& P)
{
    float Ang=P.GripAngle*30.f-10.f; float L=8.f;
    int32 B=V.Num();
    V.Add(FVector(2,-2,-3));V.Add(FVector(2+3,-2+Ang*0.1f,-3.5f));
    V.Add(FVector(2,2,-3));V.Add(FVector(2+3,2+Ang*0.1f,-3.5f));
    V.Add(FVector(2,-1.5f,2));V.Add(FVector(2+3,-1.5f+Ang*0.1f,2.5f));
    PushQuad(T,0,1,3,2); PushQuad(T,0,2,4,4);
}

void UWeaponGenerator::GenerateMagazine(TArray<FVector>& V,TArray<int32>& T,const FWeaponParameters& P)
{
    float Sz=P.MagazineSize*6.f+2.f; float W=P.MagazineShape*3.f+1.5f;
    int32 B=V.Num();
    V.Add(FVector(-2,-W,-3));V.Add(FVector(-2,W,-3));
    V.Add(FVector(-2-Sz,-W*1.2f,-2));V.Add(FVector(-2-Sz,W*1.2f,-2));
    V.Add(FVector(-2,-W,1));V.Add(FVector(-2,W,1));
    PushQuad(T,0,1,3,2); PushQuad(T,0,4,1,1); PushQuad(T,4,5,1,3);
}

void UWeaponGenerator::GenerateScope(TArray<FVector>& V,TArray<int32>& T,const FWeaponParameters& P)
{
    float Sz=P.ScopeSize*5.f; float R=P.ScopeSize*2.f+0.5f;
    int32 B=V.Num();
    for(int32 i=0;i<6;++i){float A=(float)i/6*2*PI;
        V.Add(FVector(3,FMath::Cos(A)*R,FMath::Sin(A)*R));
        V.Add(FVector(3+Sz,FMath::Cos(A)*R*0.7f,FMath::Sin(A)*R*0.7f));
    }
    for(int32 i=0;i<5;++i){T.Add(B+i*2);T.Add(B+(i+1)*2);T.Add(B+i*2+1);T.Add(B+i*2+1);T.Add(B+(i+1)*2);T.Add(B+(i+1)*2+1);}
}

void UWeaponGenerator::GenerateMuzzle(TArray<FVector>& V,TArray<int32>& T,const FWeaponParameters& P)
{
    float Sz=P.MuzzleDevice*4.f; int32 B=V.Num();
    for(int32 i=0;i<6;++i){float A=(float)i/6*2*PI;
        V.Add(FVector(-P.BarrelLength*30.f-1,FMath::Cos(A)*2.5f,FMath::Sin(A)*2.5f));
        V.Add(FVector(-P.BarrelLength*30.f-1-Sz,FMath::Cos(A)*3.5f,FMath::Sin(A)*3.5f));
    }
    for(int32 i=0;i<5;++i){T.Add(B+i*2);T.Add(B+(i+1)*2);T.Add(B+i*2+1);T.Add(B+i*2+1);T.Add(B+(i+1)*2);T.Add(B+(i+1)*2+1);}
}

void UWeaponGenerator::AddWeaponDetail(TArray<FVector>& V,const FWeaponParameters& P,FRandomStream& R)
{
    float Amt=P.GripTexture*1.5f;
    for(FVector& v:V) if(R.FRand()<0.3f) v+=RandOffset(R,Amt);
}
