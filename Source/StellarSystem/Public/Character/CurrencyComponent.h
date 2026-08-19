// CurrencyComponent.h
// 多货币管理（被 Shop/Inventory 引用）
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CurrencyComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UCurrencyComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCurrencyComponent();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

    // —— 6 种货币 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
    int32 Credits = 1000;       // 通用货币

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
    int32 Premium = 0;         // 付费货币

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
    int32 FactionTokens = 0;   // 阵营声望币

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
    int32 Salvage = 0;         // 拆解废料

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
    int32 SciencePoints = 0;   // 科研点

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
    int32 Reputation = 0;      // 声望值

    // —— 查询 ——
    UFUNCTION(BlueprintCallable, BlueprintPure)
    int32 GetAmount(ECurrencyType Type) const;

    UFUNCTION(BlueprintCallable, BlueprintPure)
    bool HasEnough(ECurrencyType Type, int32 Amount) const;

    // —— 修改（Server RPC）——
    UFUNCTION(BlueprintCallable, Server, Reliable)
    void ServerAdd(ECurrencyType Type, int32 Amount);

    UFUNCTION(BlueprintCallable, Server, Reliable)
    bool ServerSpend(ECurrencyType Type, int32 Amount);

    UFUNCTION(BlueprintCallable, Server, Reliable)
    bool ServerTransfer(ECurrencyType Type, int32 Amount, UCurrencyComponent* Target);

    // —— 批量 ——
    UFUNCTION(BlueprintCallable)
    TMap<ECurrencyType, int32> GetAllCurrencies() const;

    // —— 事件 ——
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnCurrencyChanged, ECurrencyType, Type, int32, NewAmount, int32, Delta);
    UPROPERTY(BlueprintAssignable)
    FOnCurrencyChanged OnCurrencyChanged;

private:
    int32& GetRef(ECurrencyType Type);
    const int32& GetRef(ECurrencyType Type) const;
};
