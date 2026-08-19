// ObjectPool.cpp
// 通用对象池完整实现

#include "Core/ObjectPool.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogObjectPool, Log, All);

// =====================================================================
// UObjectPool
// =====================================================================

UObjectPool::UObjectPool()
{
    MaxPoolSize = 200;
    bAutoExpand = true;
    bRecycleOnRelease = true;
    AutoShrinkInterval = 60.f;
    InactivityTimeout = 120.f;
}

void UObjectPool::Initialize(TSubclassOf<AActor> ActorClass, int32 PrewarmCount,
                             int32 InMaxPoolSize, UWorld* World, AActor* Owner)
{
    PooledClass = ActorClass;
    MaxPoolSize = FMath::Max(InMaxPoolSize, 1);
    PoolWorld = World;
    PoolOwner = Owner;

    Stats.Reset();
    Stats.PoolName = ActorClass ? FName(*ActorClass->GetName()) : NAME_None;

    if (PrewarmCount > 0)
    {
        Prewarm(FMath::Min(PrewarmCount, MaxPoolSize));
    }

    UE_LOG(LogObjectPool, Log, TEXT("[Pool] Initialized: %s, prewarm=%d, max=%d"),
        *Stats.PoolName.ToString(), PrewarmCount, MaxPoolSize);
}

AActor* UObjectPool::Acquire(const FTransform& SpawnTransform)
{
    AActor* Result = nullptr;

    // 1. 尝试从空闲队列取
    while (InactiveObjects.Num() > 0)
    {
        TWeakObjectPtr<AActor> Weak = InactiveObjects.Pop(/*bAllowShrinking=*/false);
        if (Weak.IsValid())
        {
            Result = Weak.Get();
            break;
        }
        // 无效引用，继续取下一个
    }

    // 2. 池为空，创建新对象
    if (!Result)
    {
        if (bAutoExpand && Stats.TotalCreated < MaxPoolSize)
        {
            Result = CreateNewObject(SpawnTransform);
        }
        else
        {
            // 池满，尝试复用最久未使用的
            TWeakObjectPtr<AActor> Oldest;
            float OldestTime = TNumericLimits<float>::Max();

            for (const TPair<TWeakObjectPtr<AActor>, float>& Pair : LastUseTime)
            {
                if (Pair.Key.IsValid() && Pair.Value < OldestTime)
                {
                    OldestTime = Pair.Value;
                    Oldest = Pair.Key;
                }
            }

            if (Oldest.IsValid())
            {
                Result = Oldest.Get();
                ActiveObjects.Add(Result);
                LastUseTime[Result] = 0.f;
                UE_LOG(LogObjectPool, Warning, TEXT("[Pool] Pool full, recycling oldest: %s"), *Stats.PoolName.ToString());
            }
        }
    }

    if (Result)
    {
        // 设置变换
        Result->SetActorTransform(SpawnTransform);
        Result->SetActorHiddenInGame(false);
        Result->SetActorEnableCollision(true);
        Result->SetActorTickEnabled(true);

        // 从 Inactive 移到 Active
        ActiveObjects.Add(Result);
        LastUseTime[Result] = 0.f;

        // 通知对象
        if (Result->GetClass()->ImplementsInterface(UPoolableInterface::StaticClass()))
        {
            IPoolableInterface::Execute_OnAcquiredFromPool(Result);
        }

        // 更新统计
        Stats.ActiveCount = ActiveObjects.Num();
        Stats.InactiveCount = InactiveObjects.Num();
        Stats.TotalAcquired++;
        Stats.PeakActive = FMath::Max(Stats.PeakActive, Stats.ActiveCount);

        // 计算命中率
        int32 TotalRequests = Stats.TotalAcquired;
        if (TotalRequests > 0)
        {
            Stats.HitRate = (float)(Stats.TotalAcquired - Stats.TotalCreated) / (float)TotalRequests;
            Stats.HitRate = FMath::Clamp(Stats.HitRate, 0.f, 1.f);
        }
    }

    return Result;
}

void UObjectPool::Release(AActor* Actor)
{
    if (!Actor) return;

    // 从 Active 移除
    int32 Removed = ActiveObjects.Remove(Actor);
    if (Removed == 0) return;  // 不在池中

    // 通知对象
    if (Actor->GetClass()->ImplementsInterface(UPoolableInterface::StaticClass()))
    {
        IPoolableInterface::Execute_OnReleasedToPool(Actor);
    }

    // 重置状态
    if (bRecycleOnRelease)
    {
        ResetObject(Actor);
    }

    // 隐藏并禁用
    Actor->SetActorHiddenInGame(true);
    Actor->SetActorEnableCollision(false);
    Actor->SetActorTickEnabled(false);

    // 放回空闲队列
    if (Stats.TotalCreated <= MaxPoolSize)
    {
        InactiveObjects.Add(Actor);
        LastUseTime[Actor] = 0.f;  // 刚归还，最近使用
    }
    else
    {
        // 超出池大小，直接销毁
        Actor->Destroy();
        LastUseTime.Remove(Actor);
        Stats.TotalCreated--;
    }

    // 更新统计
    Stats.ActiveCount = ActiveObjects.Num();
    Stats.InactiveCount = InactiveObjects.Num();
    Stats.TotalReleased++;

    // 清理无效引用
    if (InactiveObjects.Num() > MaxPoolSize * 1.5f)
    {
        CleanupInvalidReferences();
    }
}

void UObjectPool::Prewarm(int32 Count)
{
    if (!PoolWorld || !PooledClass) return;

    Count = FMath::Clamp(Count, 0, MaxPoolSize);

    for (int32 i = 0; i < Count; ++i)
    {
        if (Stats.TotalCreated >= MaxPoolSize) break;

        FTransform ZeroTransform;
        AActor* NewActor = CreateNewObject(ZeroTransform);
        if (NewActor)
        {
            // 立即归还到池中
            NewActor->SetActorHiddenInGame(true);
            NewActor->SetActorEnableCollision(false);
            NewActor->SetActorTickEnabled(false);
            InactiveObjects.Add(NewActor);
            LastUseTime[NewActor] = -9999.f;  // 最久未使用
        }
    }

    Stats.InactiveCount = InactiveObjects.Num();

    UE_LOG(LogObjectPool, Log, TEXT("[Pool] Prewarmed %d objects (%s)"), Stats.InactiveCount, *Stats.PoolName.ToString());
}

void UObjectPool::Shrink(int32 TargetSize)
{
    TargetSize = FMath::Max(TargetSize, 0);

    while (InactiveObjects.Num() > TargetSize)
    {
        TWeakObjectPtr<AActor> Weak = InactiveObjects.Pop(/*bAllowShrinking=*/false);
        if (Weak.IsValid())
        {
            AActor* Actor = Weak.Get();
            LastUseTime.Remove(Actor);
            Actor->Destroy();
            Stats.TotalCreated--;
        }
    }

    Stats.InactiveCount = InactiveObjects.Num();

    UE_LOG(LogObjectPool, Log, TEXT("[Pool] Shrunk to %d (%s)"), Stats.InactiveCount, *Stats.PoolName.ToString());
}

void UObjectPool::Clear()
{
    // 销毁所有活跃对象
    for (TWeakObjectPtr<AActor> Weak : ActiveObjects)
    {
        if (Weak.IsValid())
        {
            Weak->Destroy();
        }
    }
    ActiveObjects.Empty();

    // 销毁所有空闲对象
    for (TWeakObjectPtr<AActor> Weak : InactiveObjects)
    {
        if (Weak.IsValid())
        {
            Weak->Destroy();
        }
    }
    InactiveObjects.Empty();
    LastUseTime.Empty();

    Stats.Reset();

    UE_LOG(LogObjectPool, Log, TEXT("[Pool] Cleared (%s)"), *Stats.PoolName.ToString());
}

// ---- Private ----

AActor* UObjectPool::CreateNewObject(const FTransform& Transform)
{
    if (!PoolWorld || !PooledClass) return nullptr;

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Params.Owner = PoolOwner;

    AActor* NewActor = PoolWorld->SpawnActor<AActor>(
        PooledClass, Transform, Params);

    if (NewActor)
    {
        Stats.TotalCreated++;
        Stats.ActiveCount = ActiveObjects.Num() + 1;  // 即将加入
    }

    return NewActor;
}

void UObjectPool::ResetObject(AActor* Actor)
{
    if (!Actor) return;

    // 重置常见组件状态
    TArray<USceneComponent*> Components;
    Actor->GetComponents<USceneComponent>(Components);

    for (USceneComponent* Comp : Components)
    {
        if (!Comp) continue;

        // 重置物理
        if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Comp))
        {
            Prim->SetPhysicsLinearVelocity(FVector::ZeroVector);
            Prim->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
            Prim->ClearAccumulatedForces();
        }
    }

    // 重置生命周期
    if (Actor->GetClass()->ImplementsInterface(UPoolableInterface::StaticClass()))
    {
        // 接口方法已调用
    }
}

void UObjectPool::CleanupInvalidReferences()
{
    for (int32 i = InactiveObjects.Num() - 1; i >= 0; --i)
    {
        if (!InactiveObjects[i].IsValid())
        {
            InactiveObjects.RemoveAt(i, /*bAllowShrinking=*/false);
        }
    }
}

// =====================================================================
// AObjectPoolManager
// =====================================================================

AObjectPoolManager::AObjectPoolManager()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 5.f;  // 每 5 秒检查一次
}

void AObjectPoolManager::BeginPlay()
{
    Super::BeginPlay();

    UWorld* World = GetWorld();
    if (!World) return;

    // 自动注册配置的池
    for (const TPair<FName, TSubclassOf<AActor>>& Pair : PoolRegistrations)
    {
        FName PoolName = Pair.Key;
        TSubclassOf<AActor> ActorClass = Pair.Value;

        int32 Prewarm = 10;
        if (int32* Found = PoolPrewarmCounts.Find(PoolName))
        {
            Prewarm = *Found;
        }

        int32 MaxSize = 100;
        if (int32* Found = PoolMaxSizes.Find(PoolName))
        {
            MaxSize = *Found;
        }

        GetOrCreatePool(PoolName, ActorClass, Prewarm, MaxSize);
    }

    UE_LOG(LogObjectPool, Log, TEXT("[PoolManager] %d pools registered"), Pools.Num());
}

void AObjectPoolManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    ShrinkTimer += DeltaTime;
    if (ShrinkTimer >= SHRINK_CHECK_INTERVAL)
    {
        ShrinkTimer = 0.f;
        ShrinkAllPools();
    }

    // 更新每个池的使用时间
    for (TPair<FName, UObjectPool*>& Pair : Pools)
    {
        if (!Pair.Value) continue;

        // 这里可以检查超时对象并回收
        // 实际超时检查在 Pool 内部做
    }
}

UObjectPool* AObjectPoolManager::GetOrCreatePool(FName PoolName,
                                                 TSubclassOf<AActor> ActorClass,
                                                 int32 PrewarmCount,
                                                 int32 MaxPoolSize)
{
    UObjectPool** Found = Pools.Find(PoolName);
    if (Found && *Found)
    {
        return *Found;
    }

    UObjectPool* NewPool = NewObject<UObjectPool>(this);
    if (NewPool)
    {
        NewPool->Initialize(ActorClass, PrewarmCount, MaxPoolSize, GetWorld(), this);
        Pools.Add(PoolName, NewPool);
    }

    return NewPool;
}

AActor* AObjectPoolManager::AcquireFromPool(FName PoolName, const FTransform& Transform)
{
    UObjectPool** Found = Pools.Find(PoolName);
    if (!Found || !*Found) return nullptr;

    return (*Found)->Acquire(Transform);
}

void AObjectPoolManager::ReleaseToPool(FName PoolName, AActor* Actor)
{
    UObjectPool** Found = Pools.Find(PoolName);
    if (!Found || !*Found || !Actor) return;

    (*Found)->Release(Actor);
}

TArray<FPoolStats> AObjectPoolManager::GetAllPoolStats() const
{
    TArray<FPoolStats> Result;
    for (const TPair<FName, UObjectPool*>& Pair : Pools)
    {
        if (Pair.Value)
        {
            Result.Add(Pair.Value->GetStats());
        }
    }
    return Result;
}

void AObjectPoolManager::ShrinkAllPools()
{
    for (TPair<FName, UObjectPool*>& Pair : Pools)
    {
        if (!Pair.Value) continue;

        FPoolStats Stats = Pair.Value->GetStats();
        // 如果空闲对象远超活跃对象，收缩到活跃数的 2 倍
        int32 TargetActive = Stats.ActiveCount;
        int32 TargetSize = FMath::Max(TargetActive * 2, 10);

        if (Stats.InactiveCount > TargetSize * 1.5f)
        {
            Pair.Value->Shrink(TargetSize);
        }
    }
}
