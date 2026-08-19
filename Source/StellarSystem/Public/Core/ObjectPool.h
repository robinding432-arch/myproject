// ObjectPool.h
// 通用对象池：消除 Spawn/Destroy 开销，减少 GC 压力
// v6.6 性能优化

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ObjectPool.generated.h"

// 池化对象接口
UINTERFACE(Blueprintable, MinimalAPI)
class UPoolableInterface : public UInterface
{
    GENERATED_BODY()
};

class IPoolableInterface
{
    GENERATED_BODY()

public:
    // 从池中取出时调用
    virtual void OnAcquiredFromPool() = 0;

    // 归还池中时调用
    virtual void OnReleasedToPool() = 0;

    // 是否可被回收（例如：正在执行关键逻辑时返回 false）
    virtual bool CanBePooled() const { return true; }
};

// 池统计
USTRUCT(BlueprintType)
struct FPoolStats
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FName PoolName;

    UPROPERTY(BlueprintReadOnly)
    int32 ActiveCount = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 InactiveCount = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 TotalCreated = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 TotalAcquired = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 TotalReleased = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 PeakActive = 0;

    UPROPERTY(BlueprintReadOnly)
    float HitRate = 1.f;  // 命中率（无需新建的比例）

    void Reset()
    {
        ActiveCount = 0;
        InactiveCount = 0;
        TotalCreated = 0;
        TotalAcquired = 0;
        TotalReleased = 0;
        PeakActive = 0;
        HitRate = 1.f;
    }
};

// 单个对象池
UCLASS()
class STELLARSYSTEM_API UObjectPool : public UObject
{
    GENERATED_BODY()

public:
    UObjectPool();

    // 初始化池
    UFUNCTION(BlueprintCallable, Category = "ObjectPool")
    void Initialize(TSubclassOf<AActor> ActorClass, int32 PrewarmCount,
                    int32 MaxPoolSize, UWorld* World, AActor* Owner = nullptr);

    // 获取对象（从池中取或新建）
    UFUNCTION(BlueprintCallable, Category = "ObjectPool")
    AActor* Acquire(const FTransform& SpawnTransform);

    // 归还对象
    UFUNCTION(BlueprintCallable, Category = "ObjectPool")
    void Release(AActor* Actor);

    // 预分配
    UFUNCTION(BlueprintCallable, Category = "ObjectPool")
    void Prewarm(int32 Count);

    // 收缩池（释放多余对象）
    UFUNCTION(BlueprintCallable, Category = "ObjectPool")
    void Shrink(int32 TargetSize);

    // 清空池
    UFUNCTION(BlueprintCallable, Category = "ObjectPool")
    void Clear();

    // 获取统计
    UFUNCTION(BlueprintCallable, Category = "ObjectPool")
    FPoolStats GetStats() const { return Stats; }

    // 配置
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxPoolSize = 200;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bAutoExpand = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bRecycleOnRelease = true;  // 归还时自动 Reset

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AutoShrinkInterval = 60.f;  // 每 60 秒检查收缩

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float InactivityTimeout = 120.f;  // 2 分钟不活跃则回收

private:
    TSubclassOf<AActor> PooledClass;
    UWorld* PoolWorld = nullptr;
    AActor* PoolOwner = nullptr;

    // 空闲对象队列（栈式，O(1) 取放）
    TArray<TWeakObjectPtr<AActor>> InactiveObjects;

    // 活跃对象集合（快速查找）
    TSet<TWeakObjectPtr<AActor>> ActiveObjects;

    // 最后使用时间（用于超时回收）
    TMap<TWeakObjectPtr<AActor>, float> LastUseTime;

    FPoolStats Stats;

    // 创建新对象
    AActor* CreateNewObject(const FTransform& Transform);

    // 重置对象状态
    void ResetObject(AActor* Actor);

    // 清理无效引用
    void CleanupInvalidReferences();
};

// 全局池管理器（Actor，挂在世界中）
UCLASS(BlueprintType)
class STELLARSYSTEM_API AObjectPoolManager : public AActor
{
    GENERATED_BODY()

public:
    AObjectPoolManager();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // 创建/获取指定类型的池
    UFUNCTION(BlueprintCallable, Category = "ObjectPool")
    UObjectPool* GetOrCreatePool(FName PoolName, TSubclassOf<AActor> ActorClass,
                                  int32 PrewarmCount = 10, int32 MaxPoolSize = 100);

    // 便捷方法：获取对象
    UFUNCTION(BlueprintCallable, Category = "ObjectPool")
    AActor* AcquireFromPool(FName PoolName, const FTransform& Transform);

    // 便捷方法：归还对象
    UFUNCTION(BlueprintCallable, Category = "ObjectPool")
    void ReleaseToPool(FName PoolName, AActor* Actor);

    // 获取所有池统计
    UFUNCTION(BlueprintCallable, Category = "ObjectPool")
    TArray<FPoolStats> GetAllPoolStats() const;

    // 全局收缩（释放所有池的多余对象）
    UFUNCTION(BlueprintCallable, Category = "ObjectPool")
    void ShrinkAllPools();

    // 注册需要池化的类（在 BeginPlay 前调用）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ObjectPool")
    TMap<FName, TSubclassOf<AActor>> PoolRegistrations;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ObjectPool")
    TMap<FName, int32> PoolPrewarmCounts;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ObjectPool")
    TMap<FName, int32> PoolMaxSizes;

private:
    TMap<FName, UObjectPool*> Pools;

    float ShrinkTimer = 0.f;
    static constexpr float SHRINK_CHECK_INTERVAL = 30.f;
};
