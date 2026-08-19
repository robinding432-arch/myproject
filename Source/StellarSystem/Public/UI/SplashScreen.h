#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SplashScreen.generated.h"

// —— 启动画面阶段 ——
UENUM(BlueprintType)
enum class ESplashPhase : uint8
{
    CompanyLogo,       // 公司 Logo
    LegalDisclaimer,    // 法律声明
    LoadingTip,         // 加载提示
    FadingOut          // 淡出
};

// —— 启动画面 Widget ——
UCLASS()
class STELLARSYSTEM_API USplashScreen : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // —— 可配置参数（蓝图里设） ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splash")
    float CompanyLogoDuration = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splash")
    float LegalDisclaimerDuration = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splash")
    float LoadingTipDuration = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splash")
    float FadeOutDuration = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splash")
    FString CompanyName = TEXT("STELLAR FORGE STUDIOS");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splash")
    FString CompanyTagline = TEXT("Crafting Worlds Beyond Imagination");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splash")
    FString LegalText = TEXT(
        "Copyright (c) 2025 Stellar Forge Studios. All Rights Reserved.\n\n"
        "This game is a work of fiction. Names, characters, places, and events are products of the developers' imagination.\n"
        "Any resemblance to actual persons, living or dead, is purely coincidental.\n\n"
        "Unauthorized copying, distribution, or modification of this software is strictly prohibited.\n"
        "Uses Unreal Engine. Copyright Epic Games, Inc. All Rights Reserved.\n"
        "Powered by Steamworks SDK. Copyright Valve Corporation.\n\n"
        "For support: support@stellarforge.example.com");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splash")
    TArray<FString> LoadingTips = {
        TEXT("Tip: Press F to take off from any planet surface."),
        TEXT("Tip: Hold Right Mouse to lock onto targets before firing."),
        TEXT("Tip: Different biomes require different armor types."),
        TEXT("Tip: Asteroid belts are great sources of rare minerals."),
        TEXT("Tip: Your ship's AI can auto-warp to nearby star systems."),
        TEXT("Tip: Oxygen depletes in vacuum - watch your vitals!"),
        TEXT("Tip: Solar storms can disable electronics - seek shelter!"),
        TEXT("Tip: Each planet seed generates a unique world."),
    };

    // —— 事件 ——
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSplashComplete);
    UPROPERTY(BlueprintAssignable, Category = "Splash|Events")
    FOnSplashComplete OnSplashComplete;

    // 跳过
    UFUNCTION(BlueprintCallable, Category = "Splash")
    void SkipSplash();

    // 当前阶段
    UFUNCTION(BlueprintPure, Category = "Splash")
    ESplashPhase GetCurrentPhase() const { return CurrentPhase; }

    UFUNCTION(BlueprintPure, Category = "Splash")
    float GetPhaseProgress() const;

    UFUNCTION(BlueprintPure, Category = "Splash")
    FString GetCurrentTip() const;

    UFUNCTION(BlueprintPure, Category = "Splash")
    float GetGlobalAlpha() const;

private:
    ESplashPhase CurrentPhase = ESplashPhase::CompanyLogo;
    float PhaseTimer = 0.f;
    bool bSkipped = false;
    int32 CurrentTipIndex = 0;

    void AdvancePhase();
    void OnAllPhasesComplete();
};
