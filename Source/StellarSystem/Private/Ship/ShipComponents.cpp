// ShipComponents.cpp
#include "Ship/ShipComponents.h"
#include "ProceduralMeshComponent.h"
#include "Net/UnrealNetwork.h"

namespace
{
    void PushQuad(TArray<int32>& T,int32 a,int32 b,int32 c,int32 d)
    { T.Add(a);T.Add(b);T.Add(c);T.Add(c);T.Add(b);T.Add(d); }
    FVector RO(FRandomStream& R,float S){return FVector(R.FRandRange(-S,S),R.FRandRange(-S,S),R.FRandRange(-S,S));}
}

// ========== UShipComponentGenerator ==========
FShipComponentParams UShipComponentGenerator::GenerateComponent(int32 Seed, EShipComponentSlot Slot)
{
    FRandomStream R(Seed); FShipComponentParams P; P.Slot=Slot;
    P.ComponentID=FName(*FString::Printf(TEXT("CMP_%d_%d"),(int)Slot,Seed));
    P.Size=R.FRandRange(0.3f,0.8f); P.Complexity=R.FRandRange(0.2f,0.9f);
    P.Symmetry=R.FRandRange(0.5f,1.f); P.Angularity=R.FRandRange(0.2f,0.8f);
    P.VentCount=R.FRandRange(0.1f,0.6f); P.GlowIntensity=R.FRandRange(0.3f,0.8f);
    P.BaseColor=FLinearColor(R.FRandRange(0.1f,0.3f),R.FRandRange(0.12f,0.32f),R.FRandRange(0.15f,0.35f));
    P.EnergyColor=FLinearColor(R.FRandRange(0,1),R.FRandRange(0.3f,1),R.FRandRange(0.5f,1));
    P.Metallic=R.FRandRange(0.6f,0.95f); P.Roughness=R.FRandRange(0.2f,0.5f); P.Wear=R.FRandRange(0.05f,0.3f);
    P.PerformanceValue=R.FRandRange(80.f,150.f); P.PowerDraw=R.FRandRange(30.f,80.f);
    P.HeatOutput=R.FRandRange(20.f,60.f); P.Mass=R.FRandRange(50.f,200.f);

    switch(Slot)
    {
    case EShipComponentSlot::Engine:
        P.Thrust=P.PerformanceValue*10.f; P.WarpCapable=R.FRand()<0.3f?1.f:0.f;
        P.Mass*=1.5f; break;
    case EShipComponentSlot::Weapon:
        P.Damage=P.PerformanceValue*0.6f; P.FireRate=R.FRandRange(100.f,400.f);
        P.Range=R.FRandRange(20000.f,80000.f); P.EnergyPerShot=R.FRandRange(5.f,20.f);
        break;
    case EShipComponentSlot::Shield:
        P.ShieldHP=P.PerformanceValue*2.5f; P.RegenRate=P.PerformanceValue*0.05f;
        P.Mass*=0.8f; break;
    case EShipComponentSlot::Sensor:
        P.ScanRange=P.PerformanceValue*300.f; P.TargetLockSpeed=R.FRandRange(0.5f,2.f);
        P.Mass*=0.4f; P.PowerDraw*=0.5f; break;
    case EShipComponentSlot::Cargo:
        P.Capacity=P.PerformanceValue*1.5f; P.Mass*=1.2f; break;
    case EShipComponentSlot::WarpCore:
        P.WarpRange=P.PerformanceValue*50000.f; P.WarpChargeTime=R.FRandRange(3.f,8.f);
        P.PowerDraw*=2.f; P.Mass*=2.f; break;
    case EShipComponentSlot::Reactor:
        P.PowerOutput=P.PerformanceValue*2.f; P.Efficiency=R.FRandRange(0.7f,0.95f);
        P.Mass*=1.8f; break;
    case EShipComponentSlot::Cooler:
        P.CoolingRate=P.PerformanceValue*0.6f; P.Mass*=0.6f; P.PowerDraw*=0.4f; break;
    case EShipComponentSlot::Armor:
        P.ArmorHP=P.PerformanceValue*3.f; P.DamageReduction=R.FRandRange(0.1f,0.3f);
        P.Mass*=2.5f; break;
    case EShipComponentSlot::Utility:
        P.UtilityStrength=R.FRandRange(0.5f,2.f);
        {static const FName U[]={TEXT("Cloak"),TEXT("Jammer"),TEXT("Repair"),TEXT("Boost"),TEXT("Tractor")};
         P.UtilityEffect=U[R.RandRange(0,4)];}
        break;
    }
    return P;
}

TArray<FShipComponentParams> UShipComponentGenerator::GenerateFullSet(int32 Seed,int32 EngineCount,int32 WeaponCount)
{
    TArray<FShipComponentParams> Set;
    Set.Add(GenerateComponent(Seed+1,EShipComponentSlot::WarpCore));
    Set.Add(GenerateComponent(Seed+2,EShipComponentSlot::Reactor));
    Set.Add(GenerateComponent(Seed+3,EShipComponentSlot::Shield));
    Set.Add(GenerateComponent(Seed+4,EShipComponentSlot::Sensor));
    Set.Add(GenerateComponent(Seed+5,EShipComponentSlot::Cargo));
    Set.Add(GenerateComponent(Seed+6,EShipComponentSlot::Cooler));
    Set.Add(GenerateComponent(Seed+7,EShipComponentSlot::Armor));
    Set.Add(GenerateComponent(Seed+8,EShipComponentSlot::Utility));
    for(int32 i=0;i<EngineCount;++i) Set.Add(GenerateComponent(Seed+100+i,EShipComponentSlot::Engine));
    for(int32 i=0;i<WeaponCount;++i) Set.Add(GenerateComponent(Seed+200+i,EShipComponentSlot::Weapon));
    return Set;
}

FShipComponentParams UShipComponentGenerator::MutateComponent(const FShipComponentParams& Base,int32 Seed,float S)
{
    FRandomStream R(Seed); FShipComponentParams P=Base;
    auto M=[&](float&v){if(R.FRand()<0.5f) v=FMath::Clamp(v+R.FRandRange(-S,S),0.01f,9999.f);};
    M(P.Size);M(P.Complexity);M(P.Angularity);M(P.VentCount);M(P.GlowIntensity);
    M(P.PerformanceValue);M(P.PowerDraw);M(P.HeatOutput);M(P.Mass);
    M(P.Thrust);M(P.Damage);M(P.FireRate);M(P.Range);M(P.ShieldHP);M(P.ScanRange);M(P.Capacity);M(P.WarpRange);
    return P;
}

void UShipComponentGenerator::ApplyRarity(FShipComponentParams& P,EComponentRarity Rarity)
{
    float Mult=1.f;
    switch(Rarity)
    {
    case EComponentRarity::Standard:Mult=1.f;break;
    case EComponentRarity::Improved:Mult=1.25f;P.GlowIntensity=FMath::Max(P.GlowIntensity,0.4f);break;
    case EComponentRarity::Advanced:Mult=1.6f;P.GlowIntensity=FMath::Max(P.GlowIntensity,0.6f);P.Wear*=0.5f;break;
    case EComponentRarity::Prototype:Mult=2.2f;P.GlowIntensity=FMath::Max(P.GlowIntensity,0.8f);P.EnergyColor=FLinearColor(0.2f,1.f,0.5f);break;
    case EComponentRarity::Alien:Mult=3.5f;P.GlowIntensity=1.f;P.EnergyColor=FLinearColor(0.8f,0.3f,1.f);P.Angularity=FMath::Min(P.Angularity*1.3f,1.f);P.Wear=0.f;break;
    }
    P.PerformanceValue*=Mult; P.Thrust*=Mult; P.Damage*=Mult; P.ShieldHP*=Mult;
    P.ScanRange*=Mult; P.Capacity*=Mult; P.WarpRange*=Mult; P.ArmorHP*=Mult;
    P.PowerOutput*=Mult; P.CoolingRate*=Mult; P.Rarity=Rarity;
}

void UShipComponentGenerator::BuildComponentMesh(UProceduralMeshComponent* M,const FShipComponentParams& P)
{
    if(!M) return; M->ClearAllMeshSections();
    TArray<FVector> V; TArray<int32> T;
    TArray<FVector> N;TArray<FVector2D> UV;TArray<FColor> C;TArray<FProcMeshTangent> Tan;
    FRandomStream R(FMath::Rand());

    switch(P.Slot)
    {
    case EShipComponentSlot::Engine:GenEngineMesh(V,T,P);break;
    case EShipComponentSlot::Weapon:GenWeaponMesh(V,T,P);break;
    case EShipComponentSlot::Shield:GenShieldMesh(V,T,P);break;
    case EShipComponentSlot::Sensor:GenSensorMesh(V,T,P);break;
    case EShipComponentSlot::Cargo:GenCargoMesh(V,T,P);break;
    case EShipComponentSlot::WarpCore:GenWarpCoreMesh(V,T,P);break;
    case EShipComponentSlot::Reactor:GenReactorMesh(V,T,P);break;
    case EShipComponentSlot::Cooler:GenCoolerMesh(V,T,P);break;
    case EShipComponentSlot::Armor:GenArmorPlate(V,T,P);break;
    case EShipComponentSlot::Utility:GenUtilityMesh(V,T,P);break;
    }
    AddVents(V,P,R); AddGlowStrips(V,P,R);

    N.SetNum(V.Num());UV.SetNum(V.Num());C.SetNum(V.Num());Tan.SetNum(V.Num());
    for(int32 i=0;i<V.Num();++i) C[i]=FColor(100,105,115,255);
    M->CreateMeshSection(0,V,T,N,UV,C,Tan,true);
}

bool UShipComponentGenerator::IsCompatible(const FShipComponentParams& A,const FShipComponentParams& B)
{
    // 功率平衡：总消耗 < 总输出*1.2
    // 热平衡：总散热 > 总发热*0.8
    // 此处只做占位示例
    return true;
}

FShipComponentParams UShipComponentGenerator::UpgradeComponent(const FShipComponentParams& Base,int32 Seed)
{
    FRandomStream R(Seed);
    FShipComponentParams P=MutateComponent(Base,Seed,0.1f);
    EComponentRarity Next=Base.Rarity;
    switch(Base.Rarity)
    {
    case EComponentRarity::Standard:Next=EComponentRarity::Improved;break;
    case EComponentRarity::Improved:Next=EComponentRarity::Advanced;break;
    case EComponentRarity::Advanced:Next=EComponentRarity::Prototype;break;
    case EComponentRarity::Prototype:Next=EComponentRarity::Alien;break;
    case EComponentRarity::Alien:break; // 已满级
    }
    ApplyRarity(P,Next);
    return P;
}

// ---------- Mesh 生成 ----------
void UShipComponentGenerator::GenEngineMesh(TArray<FVector>& V,TArray<int32>& T,const FShipComponentParams& P)
{
    float L=P.Size*20.f; float R=P.Size*3.f; int32 Segs=10; int32 B=V.Num();
    for(int32 i=0;i<=Segs;++i){float A=(float)i/Segs*2*PI;
        V.Add(FVector(-L*0.8f,FMath::Cos(A)*R,FMath::Sin(A)*R));
        V.Add(FVector(L*0.2f,FMath::Cos(A)*R*0.7f,FMath::Sin(A)*R*0.7f));
    }
    for(int32 i=0;i<Segs;++i){int32 a=B+i*2,b=B+(i+1)*2;
        T.Add(a);T.Add(b);T.Add(a+1);T.Add(a+1);T.Add(b);T.Add(b+1);}
    // 喷口环
    int32 R2=V.Num();
    for(int32 i=0;i<6;++i){float A=(float)i/6*2*PI;V.Add(FVector(-L*0.85f,FMath::Cos(A)*R*1.1f,FMath::Sin(A)*R*1.1f));}
    V.Add(FVector(-L*0.9f,0,0));int32 Tip=V.Num()-1;
    for(int32 i=0;i<6;++i)T.Add(R2+i);T.Add(R2+(i+1)%6);T.Add(Tip);
}

void UShipComponentGenerator::GenWeaponMesh(TArray<FVector>& V,TArray<int32>& T,const FShipComponentParams& P)
{
    float L=P.Size*15.f; float R=P.Size*2.f;
    int32 B=V.Num();
    // 炮管
    for(int32 i=0;i<6;++i){float A=(float)i/6*2*PI;
        V.Add(FVector(-L*0.9f,FMath::Cos(A)*R,FMath::Sin(A)*R));
        V.Add(FVector(L*0.1f,FMath::Cos(A)*R*0.8f,FMath::Sin(A)*R*0.8f));
    }
    for(int32 i=0;i<5;++i){T.Add(B+i*2);T.Add(B+(i+1)*2);T.Add(B+i*2+1);T.Add(B+i*2+1);T.Add(B+(i+1)*2);T.Add(B+(i+1)*2+1);}
    // 底座
    int32 B2=V.Num();
    V.Add(FVector(L*0.15f,-3,-3));V.Add(FVector(L*0.15f,3,-3));V.Add(FVector(L*0.15f,-3,3));V.Add(FVector(L*0.15f,3,3));
    V.Add(FVector(L*0.5f,-2,-2));V.Add(FVector(L*0.5f,2,-2));V.Add(FVector(L*0.5f,-2,2));V.Add(FVector(L*0.5f,2,2));
    PushQuad(T,0,1,3,2);PushQuad(T,4,6,7,5);PushQuad(T,0,4,2,6);PushQuad(T,1,5,3,7);
}

void UShipComponentGenerator::GenShieldMesh(TArray<FVector>& V,TArray<int32>& T,const FShipComponentParams& P)
{
    int32 Segs=12; int32 B=V.Num();
    for(int32 i=0;i<=Segs;++i){float A=(float)i/Segs*2*PI;
        V.Add(FVector(FMath::Cos(A)*8,FMath::Sin(A)*8,0));
    }
    V.Add(FVector(0,0,0));int32 C=V.Num()-1;
    for(int32 i=0;i<Segs;++i)T.Add(B+i);T.Add(B+(i+1)%Segs);T.Add(C);
    // 发光环
    int32 B2=V.Num();
    for(int32 i=0;i<=Segs;++i){float A=(float)i/Segs*2*PI;
        V.Add(FVector(FMath::Cos(A)*9,FMath::Sin(A)*9,0.5f));
        V.Add(FVector(FMath::Cos(A)*9,FMath::Sin(A)*9,-0.5f));
    }
    for(int32 i=0;i<Segs;++i){int32 a=B2+i*2,b=B2+(i+1)*2;T.Add(a);T.Add(b);T.Add(a+1);T.Add(a+1);T.Add(b);T.Add(b+1);}
}

void UShipComponentGenerator::GenSensorMesh(TArray<FVector>& V,TArray<int32>& T,const FShipComponentParams& P)
{
    // 碟形
    int32 Segs=10; int32 B=V.Num();
    for(int32 i=0;i<=Segs;++i){float A=(float)i/Segs*2*PI;
        float R=6.f+sinf(A*3)*1.5f;
        V.Add(FVector(FMath::Cos(A)*R,FMath::Sin(A)*R,1.f));
        V.Add(FVector(FMath::Cos(A)*R*0.9f,FMath::Sin(A)*R*0.9f,-0.5f));
    }
    for(int32 i=0;i<Segs;++i){int32 a=B+i*2,b=B+(i+1)*2;T.Add(a);T.Add(b);T.Add(a+1);T.Add(a+1);T.Add(b);T.Add(b+1);}
}

void UShipComponentGenerator::GenCargoMesh(TArray<FVector>& V,TArray<int32>& T,const FShipComponentParams& P)
{
    float W=P.Size*10.f,H=P.Size*8.f,D=P.Size*10.f;
    V.Add(FVector(-W,-H,-D));V.Add(FVector(W,-H,-D));V.Add(FVector(-W,H,-D));V.Add(FVector(W,H,-D));
    V.Add(FVector(-W,-H,D));V.Add(FVector(W,-H,D));V.Add(FVector(-W,H,D));V.Add(FVector(W,H,D));
    PushQuad(T,0,1,3,2);PushQuad(T,4,6,7,5);PushQuad(T,0,2,6,4);PushQuad(T,1,5,7,3);PushQuad(T,0,4,5,1);PushQuad(T,2,3,7,6);
    // 分隔线
    int32 B2=V.Num();
    V.Add(FVector(-W*0.95f,0,-D));V.Add(FVector(W*0.95f,0,-D));V.Add(FVector(-W*0.95f,0,D));V.Add(FVector(W*0.95f,0,D));
    T.Add(B2);T.Add(B2+1);T.Add(B2+3);T.Add(B2);T.Add(B2+3);T.Add(B2+2);
}

void UShipComponentGenerator::GenWarpCoreMesh(TArray<FVector>& V,TArray<int32>& T,const FShipComponentParams& P)
{
    int32 Segs=8; float H=P.Size*25.f; float R=P.Size*4.f;
    int32 B=V.Num();
    for(int32 i=0;i<=Segs;++i){float A=(float)i/Segs*2*PI;
        float Y=FMath::Lerp(-H*0.5f,H*0.5f,(float)i/Segs);
        V.Add(FVector(FMath::Cos(A)*R*(1+sinf(Y*0.3f)*0.2f),Y,FMath::Sin(A)*R*(1+sinf(Y*0.3f)*0.2f)));
    }
    for(int32 i=0;i<Segs;++i){T.Add(B+i);T.Add(B+(i+1)%Segs);T.Add(B+(i+1)%Segs+Segs+1);}
    // 顶部发光球
    int32 B2=V.Num();
    for(int32 i=0;i<=Segs;++i){float A=(float)i/Segs*2*PI;
        V.Add(FVector(FMath::Cos(A)*R*0.6f,H*0.5f+2,FMath::Sin(A)*R*0.6f));
    }
    V.Add(FVector(0,H*0.5f+4,0));int32 Tip=V.Num()-1;
    for(int32 i=0;i<Segs;++i)T.Add(B2+i);T.Add(B2+(i+1)%Segs);T.Add(Tip);
}

void UShipComponentGenerator::GenReactorMesh(TArray<FVector>& V,TArray<int32>& T,const FShipComponentParams& P)
{
    int32 Segs=10; float R=P.Size*5.f; float H=P.Size*12.f;
    int32 B=V.Num();
    for(int32 i=0;i<=Segs;++i){float A=(float)i/Segs*2*PI;
        V.Add(FVector(FMath::Cos(A)*R,-H*0.5f,FMath::Sin(A)*R));
        V.Add(FVector(FMath::Cos(A)*R*0.7f,H*0.5f,FMath::Sin(A)*R*0.7f));
    }
    for(int32 i=0;i<Segs;++i){int32 a=B+i*2,b=B+(i+1)*2;T.Add(a);T.Add(b);T.Add(a+1);T.Add(a+1);T.Add(b);T.Add(b+1);}
    // 散热片
    for(int32 s=0;s<4;++s){float Y=FMath::Lerp(-H*0.3f,H*0.3f,(float)s/3);
        int32 BB=V.Num();
        for(int32 i=0;i<6;++i){float A=(float)i/6*2*PI+0.5f;
            V.Add(FVector(FMath::Cos(A)*(R+1.f),Y,FMath::Sin(A)*(R+1.f)));
        }
        V.Add(FVector(0,Y,R+1.5f));int32 Tip=V.Num()-1;
        for(int32 i=0;i<6;++i)T.Add(BB+i);T.Add(BB+(i+1)%6);T.Add(Tip);
    }
}

void UShipComponentGenerator::GenCoolerMesh(TArray<FVector>& V,TArray<int32>& T,const FShipComponentParams& P)
{
    // 散热栅格
    int32 Grills=4; float W=P.Size*8.f; float H=P.Size*6.f;
    for(int32 g=0;g<Grills;++g){float Z=FMath::Lerp(-H*0.4f,H*0.4f,(float)g/(Grills-1));
        int32 B=V.Num();
        V.Add(FVector(-W*0.5f,-1,Z-0.5f));V.Add(FVector(W*0.5f,-1,Z-0.5f));
        V.Add(FVector(-W*0.5f,1,Z-0.5f));V.Add(FVector(W*0.5f,1,Z-0.5f));
        V.Add(FVector(-W*0.5f,-1,Z+0.5f));V.Add(FVector(W*0.5f,-1,Z+0.5f));
        V.Add(FVector(-W*0.5f,1,Z+0.5f));V.Add(FVector(W*0.5f,1,Z+0.5f));
        PushQuad(T,0,1,3,2);PushQuad(T,4,6,7,5);PushQuad(T,0,2,6,4);PushQuad(T,1,5,7,3);
    }
}

void UShipComponentGenerator::GenArmorPlate(TArray<FVector>& V,TArray<int32>& T,const FShipComponentParams& P)
{
    float W=P.Size*12.f; float H=P.Size*10.f; float D=P.Size*2.f;
    int32 Ang=ROUND(P.Angularity*4);
    // 主甲板
    V.Add(FVector(-W,-H,-D));V.Add(FVector(W,-H,-D));V.Add(FVector(-W*0.8f,H,-D*0.8f));V.Add(FVector(W*0.8f,H,-D*0.8f));
    V.Add(FVector(-W,-H,D));V.Add(FVector(W,-H,D));V.Add(FVector(-W*0.8f,H,D*0.8f));V.Add(FVector(W*0.8f,H,D*0.8f));
    PushQuad(T,0,1,3,2);PushQuad(T,4,6,7,5);PushQuad(T,0,2,6,4);PushQuad(T,1,5,7,3);
    // 棱线
    if(Ang>0){int32 B=V.Num();
        V.Add(FVector(0,H*0.5f,-D*1.2f));V.Add(FVector(0,H*0.5f,D*1.2f));
        T.Add(B);T.Add(B+1);T.Add(3);T.Add(B);T.Add(3);T.Add(2);
    }
}

void UShipComponentGenerator::GenUtilityMesh(TArray<FVector>& V,TArray<int32>& T,const FShipComponentParams& P)
{
    // 通用：小盒子+天线
    V.Add(FVector(-3,-3,-3));V.Add(FVector(3,-3,-3));V.Add(FVector(-3,3,-3));V.Add(FVector(3,3,-3));
    V.Add(FVector(-3,-3,3));V.Add(FVector(3,-3,3));V.Add(FVector(-3,3,3));V.Add(FVector(3,3,3));
    PushQuad(T,0,1,3,2);PushQuad(T,4,6,7,5);PushQuad(T,0,2,6,4);PushQuad(T,1,5,7,3);PushQuad(T,0,4,5,1);PushQuad(T,2,3,7,6);
    // 天线
    int32 B=V.Num();
    V.Add(FVector(0,0,3));V.Add(FVector(0,0,8));V.Add(FVector(0.5f,0,8));V.Add(FVector(-0.5f,0,8));
    T.Add(B);T.Add(B+1);T.Add(B+2);T.Add(B);T.Add(B+3);T.Add(B+1);
}

void UShipComponentGenerator::AddVents(TArray<FVector>& V,const FShipComponentParams& P,FRandomStream& R)
{
    int32 Count=ROUND(P.VentCount*8);
    for(int32 i=0;i<Count;++i){FVector Ofs=RO(V,5.f);V.Add(Ofs);V.Add(Ofs+FVector(0,0,0.5f));}
}

void UShipComponentGenerator::AddGlowStrips(TArray<FVector>& V,const FShipComponentParams& P,FRandomStream& R)
{
    int32 Count=ROUND(P.GlowIntensity*6);
    for(int32 i=0;i<Count;++i){FVector Ofs=RO(V,8.f);V.Add(Ofs);V.Add(Ofs+FVector(0.3f,0,0));}
}

// ========== UShipLoadoutComponent ==========
UShipLoadoutComponent::UShipLoadoutComponent(){SetIsReplicatedByDefault(true);}

void UShipLoadoutComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{DOREPLIFETIME(UShipLoadoutComponent,InstalledComponents);DOREPLIFETIME(UShipLoadoutComponent,CurrentTotalMass);}

void UShipLoadoutComponent::ServerInstallComponent_Implementation(const FShipComponentParams& Comp)
{
    if(!CanInstall(Comp)) return;
    InstalledComponents.Add(Comp);
    RecalculateStats();
}

void UShipLoadoutComponent::ServerRemoveComponent_Implementation(EShipComponentSlot Slot,int32 Index)
{
    for(int32 i=0;i<InstalledComponents.Num();++i){
        if(InstalledComponents[i].Slot==Slot){
            if(Index<=0){InstalledComponents.RemoveAt(i);break;}
            Index--;
        }
    }
    RecalculateStats();
}

float UShipLoadoutComponent::GetTotalThrust() const
{float T=0;for(const auto&C:InstalledComponents)if(C.Slot==EShipComponentSlot::Engine)T+=C.Thrust;return T;}

float UShipLoadoutComponent::GetTotalShieldHP() const
{float S=0;for(const auto&C:InstalledComponents)if(C.Slot==EShipComponentSlot::Shield)S+=C.ShieldHP;return S;}

float UShipLoadoutComponent::GetTotalPowerOutput() const
{float P=0;for(const auto&C:InstalledComponents){if(C.Slot==EShipComponentSlot::Reactor)P+=C.PowerOutput;if(C.Slot==EShipComponentSlot::Engine)P-=C.PowerDraw;if(C.Slot==EShipComponentSlot::Weapon)P-=C.EnergyPerShot*C.FireRate/60.f;}
return P;}

float UShipLoadoutComponent::GetTotalMass() const
{float M=0;for(const auto&C:InstalledComponents)M+=C.Mass;return M;}

float UShipLoadoutComponent::GetMaxWarpRange() const
{float R=0;for(const auto&C:InstalledComponents)if(C.Slot==EShipComponentSlot::WarpCore)R=C.WarpRange;return R;}

float UShipLoadoutComponent::GetHeatGeneration(float Throttle) const
{float H=0;for(const auto&C:InstalledComponents){if(C.Slot==EShipComponentSlot::Engine)H+=C.HeatOutput*Throttle;if(C.Slot==EShipComponentSlot::Weapon)H+=C.EnergyPerShot*C.FireRate/60.f*0.3f;}
return H;}

float UShipLoadoutComponent::GetHeatDissipation() const
{float C=0;for(const auto&C:InstalledComponents)if(C.Slot==EShipComponentSlot::Cooler)C+=C.CoolingRate;return C;}

bool UShipLoadoutComponent::CanInstall(const FShipComponentParams& Comp) const
{
    // 检查功率平衡
    float NetPower=GetTotalPowerOutput();
    if(Comp.Slot==EShipComponentSlot::Reactor) NetPower+=Comp.PowerOutput;
    else NetPower-=Comp.PowerDraw;
    return NetPower>-50.f; // 允许轻微亏电
}

void UShipLoadoutComponent::RecalculateStats(){CurrentTotalMass=GetTotalMass();}
