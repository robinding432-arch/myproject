// WeGameAntiAddictionWidget.h
// v6.9 — 防沉迷提示 Widget
// 当收到 kRailEventAntiAddictionCustomizeAntiAddictionActions 回调时显示

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WeGameAntiAddictionWidget.generated.h"

#if WITH_WEGAME
#include "rail/sdk/rail_api.h"
#endif

UCLASS()
class STELLARSYSTEM_API UWeGameAntiAddictionWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // ---- 显示防沉迷提示 ----
    UFUNCTION(BlueprintCallable, Category = "WeGame|AntiAddiction|UI")
    void ShowTips(const FString& Title, const FString& Content, int32 DurationSeconds = 60);

    // ---- 显示强退警告 ----
    UFUNCTION(BlueprintCallable, Category = "WeGame|AntiAddiction|UI")
    void ShowForceExitWarning(const FString& Content);

    // ---- 显示宵禁提示 ----
    UFUNCTION(BlueprintCallable, Category = "WeGame|AntiAddiction|UI")
    void ShowCurfewNotice(const FString& Content);

    // ---- 隐藏 ----
    UFUNCTION(BlueprintCallable, Category = "WeGame|AntiAddiction|UI")
    void Dismiss();

    // ---- 静态工具：解析防沉迷 JSON ----
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WeGame|AntiAddiction")
    static void ParseAntiAddictionJSON(const FString& JSONString,
                                       FString& OutTitle,
                                       FString& OutContent,
                                       int32& OutDuration,
                                       bool& bOutIsForceExit);

    // ---- 委托 ----
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDismissed);
    UPROPERTY(BlueprintAssignable, Category = "WeGame|AntiAddiction|UI|Events")
    FOnDismissed OnDismissed;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // 倒计时句柄
    FTimerHandle DismissTimerHandle;

    // 是否强制退出
    bool bPendingForceExit = false;

    // 自动关闭
    void OnAutoDismiss();
};
