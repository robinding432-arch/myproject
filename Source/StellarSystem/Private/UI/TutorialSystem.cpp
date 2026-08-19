// TutorialSystem.cpp
// 10 分钟新手教程完整实现

#include "UI/TutorialSystem.h"
#include "UI/TutorialPromptWidget.h"
#include "UI/TutorialCinematicWidget.h"
#include "UI/TutorialArrowWidget.h"
#include "Planet/ProceduralPlanet.h"
#include "Character/MyCharacter.h"
#include "Ship/ShipPawn.h"
#include "Ship/ProceduralShip.h"
#include "Station/ProceduralStation.h"
#include "Core/StellarGameMode.h"
#include "Economy/MiningSystem.h"
#include "Audio/AudioManager.h"
#include "Combat/RespawnSystem.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogTutorial, Log, All);

ATutorialManager::ATutorialManager()
{
    PrimaryActorTick.bCanEverTick = true;
    bIsActive = false;
    CurrentPhase = ETutorialPhase::None;
    CurrentStepIndex = 0;
}

void ATutorialManager::BeginPlay()
{
    Super::BeginPlay();

    LoadTutorialData();

    if (bAutoStartOnNewGame && !SaveData.bTutorialCompleted && !SaveData.bTutorialSkipped)
    {
        // 延迟 2 秒开始（等关卡加载完成）
        GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
        {
            StartTutorial();
        });
    }
}

void ATutorialManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bIsActive) return;

    StepTimer += DeltaTime;
    PromptTimer += DeltaTime;

    // 检查当前步骤的触发条件
    CheckTriggerConditions(DeltaTime);

    // 更新箭头方向
    if (bArrowVisible)
    {
        UpdateArrow(DeltaTime);
    }

    // 超时检测
    if (CurrentStepIndex < TutorialSteps.Num())
    {
        const FTutorialStep& Step = TutorialSteps[CurrentStepIndex];
        if (Step.TimeoutSeconds > 0.f && StepTimer > Step.TimeoutSeconds)
        {
            // 超时自动跳过当前步骤
            UE_LOG(LogTutorial, Warning, TEXT("Tutorial step %d timed out, auto-advancing"),
                (int32)Step.Phase);
            CompleteCurrentStep();
        }
    }
}

// ==================== 生命周期 ====================

void ATutorialManager::StartTutorial()
{
    if (bIsActive) return;

    UE_LOG(LogTutorial, Log, TEXT("=== TUTORIAL STARTED ==="));

    bIsActive = true;
    CurrentStepIndex = 0;
    StepTimer = 0.f;
    PromptTimer = 0.f;

    SaveData.StartTime = FDateTime::Now();
    SaveData.StepsCompleted = 0;
    SaveData.TotalSteps = TutorialSteps.Num();

    // 生成教程专用实体
    SpawnTutorialEntities();

    // 初始化步骤（如果为空则填充默认）
    if (TutorialSteps.Num() == 0)
    {
        InitializeDefaultSteps();
    }

    // 开始第一个阶段
    if (TutorialSteps.Num() > 0)
    {
        CurrentPhase = TutorialSteps[0].Phase;
        ShowPrompt(TutorialSteps[0]);
        SetupIntroCinematic();
    }

    SaveTutorialData();
}

void ATutorialManager::SkipTutorial()
{
    if (!bIsActive) return;

    UE_LOG(LogTutorial, Log, TEXT("=== TUTORIAL SKIPPED ==="));

    SaveData.bTutorialSkipped = true;
    CompleteTutorial();
}

void ATutorialManager::RestartTutorial()
{
    UE_LOG(LogTutorial, Log, TEXT("=== TUTORIAL RESTARTED ==="));

    // 清理当前状态
    HidePrompt();
    HideCinematic();
    HideArrow();
    CleanupTutorialEntities();

    // 重新开始
    bIsActive = false;
    StartTutorial();
}

void ATutorialManager::CompleteTutorial()
{
    if (!bIsActive && SaveData.bTutorialCompleted) return;

    UE_LOG(LogTutorial, Log, TEXT("=== TUTORIAL COMPLETED ==="));

    bIsActive = false;
    CurrentPhase = ETutorialPhase::Completed;
    SaveData.bTutorialCompleted = true;
    SaveData.CompletionTime = FDateTime::Now();

    // 计算用时
    FTimespan Duration = SaveData.CompletionTime - SaveData.StartTime;
    UE_LOG(LogTutorial, Log, TEXT("Tutorial duration: %s"), *Duration.ToString());

    // 发放奖励
    GrantCompletionRewards();

    // 播放完成音效
    if (UAudioManager* Audio = GetWorld()->GetSubsystem<UAudioManager>())
    {
        Audio->PlayUISound(Audio_TutorialComplete);
    }

    // 显示完成 UI
    if (TutorialCompleteWidgetClass.IsValid())
    {
        if (UClass* WidgetClass = TutorialCompleteWidgetClass.LoadSynchronous())
        {
            if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
            {
                UUserWidget* CompleteWidget = CreateWidget<UUserWidget>(PC, WidgetClass);
                if (CompleteWidget)
                {
                    CompleteWidget->AddToViewport(100);
                }
            }
        }
    }

    // 清理教程实体
    CleanupTutorialEntities();

    SaveTutorialData();
}

// ==================== 步骤控制 ====================

void ATutorialManager::AdvancePhase(ETutorialPhase NextPhase)
{
    // 查找下一个匹配的步骤
    for (int32 i = CurrentStepIndex + 1; i < TutorialSteps.Num(); ++i)
    {
        if (TutorialSteps[i].Phase == NextPhase)
        {
            CompleteCurrentStep();
            return;
        }
    }

    // 没找到就直接完成当前
    CompleteCurrentStep();
}

void ATutorialManager::CompleteCurrentStep()
{
    if (CurrentStepIndex >= TutorialSteps.Num()) return;

    const FTutorialStep& Step = TutorialSteps[CurrentStepIndex];

    UE_LOG(LogTutorial, Log, TEXT("Tutorial step completed: %s"),
        *UEnum::GetValueAsString(Step.Phase));

    OnStepCompleted(Step);

    SaveData.StepsCompleted++;
    CurrentStepIndex++;

    // 播放完成音效
    PlayStepCompleteAudio();

    // 隐藏当前 UI
    HidePrompt();
    HideArrow();

    // 检查是否全部完成
    if (CurrentStepIndex >= TutorialSteps.Num())
    {
        CompleteTutorial();
        return;
    }

    // 进入下一步
    CurrentPhase = TutorialSteps[CurrentStepIndex].Phase;
    StepTimer = 0.f;

    // 显示下一步提示
    ShowPrompt(TutorialSteps[CurrentStepIndex]);

    // 阶段-specific 设置
    switch (CurrentPhase)
    {
    case ETutorialPhase::Intro_Cinematic:    SetupIntroCinematic(); break;
    case ETutorialPhase::Move_Walk:         SetupWalkingTutorial(); break;
    case ETutorialPhase::Flight_TakeOff:    SetupFlightTutorial(); break;
    case ETutorialPhase::Ship_Approach:     SetupShipTutorial(); break;
    case ETutorialPhase::Ship_Fire:         SetupCombatTutorial(); break;
    case ETutorialPhase::Mining_Laser:      SetupMiningTutorial(); break;
    case ETutorialPhase::Respawn_SetPoint:  SetupRespawnTutorial(); break;
    case ETutorialPhase::Outro_Welcome:     SetupOutroCinematic(); break;
    default: break;
    }

    SaveTutorialData();
}

// ==================== 事件触发 ====================

void ATutorialManager::OnInputPressed(const FName& InputActionName)
{
    if (!bIsActive) return;

    if (CurrentStepIndex >= TutorialSteps.Num()) return;

    const FTutorialStep& Step = TutorialSteps[CurrentStepIndex];

    if (Step.TriggerType == FName("InputPressed") && Step.TriggerParam == InputActionName.ToString())
    {
        UE_LOG(LogTutorial, Log, TEXT("Tutorial: Input '%s' matched step trigger"),
            *InputActionName.ToString());
        CompleteCurrentStep();
    }
}

void ATutorialManager::OnReachedLocation(const FString& LocationName, const FVector& PlayerLocation)
{
    if (!bIsActive) return;

    if (CurrentStepIndex >= TutorialSteps.Num()) return;

    const FTutorialStep& Step = TutorialSteps[CurrentStepIndex];

    if (Step.TriggerType == FName("ReachedLocation") && Step.TriggerParam == LocationName)
    {
        float Dist = FVector::Dist(PlayerLocation, GetActorLocation());
        if (Dist < Step.TriggerDistance)
        {
            CompleteCurrentStep();
        }
    }
}

void ATutorialManager::OnEventTriggered(const FName& EventName)
{
    if (!bIsActive) return;

    if (CurrentStepIndex >= TutorialSteps.Num()) return;

    const FTutorialStep& Step = TutorialSteps[CurrentStepIndex];

    if (Step.TriggerType == FName("Event") && Step.TriggerParam == EventName.ToString())
    {
        CompleteCurrentStep();
    }
}

void ATutorialManager::OnDistanceCheck(const FString& TargetName, float Distance)
{
    if (!bIsActive) return;

    if (CurrentStepIndex >= TutorialSteps.Num()) return;

    const FTutorialStep& Step = TutorialSteps[CurrentStepIndex];

    if (Step.TriggerType == FName("Distance") && Step.TriggerParam == TargetName)
    {
        if (Distance < Step.TriggerDistance)
        {
            CompleteCurrentStep();
        }
    }
}

// ==================== 查询 ====================

FTutorialStep ATutorialManager::GetCurrentStep() const
{
    if (CurrentStepIndex < TutorialSteps.Num())
    {
        return TutorialSteps[CurrentStepIndex];
    }
    return FTutorialStep();
}

float ATutorialManager::GetStepProgress() const
{
    if (SaveData.TotalSteps <= 0) return 0.f;
    return (float)SaveData.StepsCompleted / (float)SaveData.TotalSteps;
}

FText ATutorialManager::GetCurrentPrompt() const
{
    if (CurrentStepIndex < TutorialSteps.Num())
    {
        return TutorialSteps[CurrentStepIndex].PromptText;
    }
    return FText::GetEmpty();
}

FText ATutorialManager::GetCurrentDetail() const
{
    if (CurrentStepIndex < TutorialSteps.Num())
    {
        return TutorialSteps[CurrentStepIndex].DetailText;
    }
    return FText::GetEmpty();
}

// ==================== UI ====================

void ATutorialManager::ShowPrompt(const FTutorialStep& Step)
{
    if (!PromptTimer > 0.5f && ActivePromptWidget) return; // 防抖

    PromptTimer = 0.f;

    if (TutorialPromptWidgetClass.IsValid())
    {
        if (UClass* WidgetClass = TutorialPromptWidgetClass.LoadSynchronous())
        {
            if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
            {
                // 移除旧的
                if (ActivePromptWidget)
                {
                    ActivePromptWidget->RemoveFromParent();
                    ActivePromptWidget = nullptr;
                }

                ActivePromptWidget = CreateWidget<UUserWidget>(PC, WidgetClass);
                if (ActivePromptWidget)
                {
                    // 设置文本
                    if (UTextBlock* PromptText = Cast<UTextBlock>(
                        ActivePromptWidget->GetWidgetFromName(FName("PromptText"))))
                    {
                        PromptText->SetText(Step.PromptText);
                    }

                    if (UTextBlock* DetailText = Cast<UTextBlock>(
                        ActivePromptWidget->GetWidgetFromName(FName("DetailText"))))
                    {
                        DetailText->SetText(Step.DetailText);
                    }

                    // 设置进度条
                    if (UProgressBar* ProgressBar = Cast<UProgressBar>(
                        ActivePromptWidget->GetWidgetFromName(FName("ProgressBar"))))
                    {
                        ProgressBar->SetPercent(GetStepProgress());
                    }

                    ActivePromptWidget->AddToViewport(50);
                }
            }
        }
    }

    // 播放提示音
    if (UAudioManager* Audio = GetWorld()->GetSubsystem<UAudioManager>())
    {
        Audio->PlayUISound(Audio_PromptShow);
    }

    UE_LOG(LogTutorial, Log, TEXT("Prompt shown: %s"), *Step.PromptText.ToString());
}

void ATutorialManager::HidePrompt()
{
    if (ActivePromptWidget)
    {
        ActivePromptWidget->RemoveFromParent();
        ActivePromptWidget = nullptr;
    }
}

void ATutorialManager::ShowCinematic(ETutorialPhase Phase)
{
    if (TutorialCinematicWidgetClass.IsValid())
    {
        if (UClass* WidgetClass = TutorialCinematicWidgetClass.LoadSynchronous())
        {
            if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
            {
                HideCinematic();

                ActiveCinematicWidget = CreateWidget<UUserWidget>(PC, WidgetClass);
                if (ActiveCinematicWidget)
                {
                    ActiveCinematicWidget->AddToViewport(200);
                    bCinematicActive = true;

                    // 暂停输入
                    PC->SetIgnoreMoveInput(true);
                    PC->SetIgnoreLookInput(true);
                }
            }
        }
    }
}

void ATutorialManager::HideCinematic()
{
    if (ActiveCinematicWidget)
    {
        ActiveCinematicWidget->RemoveFromParent();
        ActiveCinematicWidget = nullptr;
    }
    bCinematicActive = false;

    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
    {
        PC->SetIgnoreMoveInput(false);
        PC->SetIgnoreLookInput(false);
    }
}

void ATutorialManager::ShowArrow(const FVector& TargetLocation)
{
    ArrowTargetLocation = TargetLocation;
    bArrowVisible = true;

    if (TutorialArrowWidgetClass.IsValid())
    {
        if (UClass* WidgetClass = TutorialArrowWidgetClass.LoadSynchronous())
        {
            if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
            {
                if (ActiveArrowWidget)
                {
                    ActiveArrowWidget->RemoveFromParent();
                }

                ActiveArrowWidget = CreateWidget<UUserWidget>(PC, WidgetClass);
                if (ActiveArrowWidget)
                {
                    ActiveArrowWidget->AddToViewport(75);
                }
            }
        }
    }
}

void ATutorialManager::HideArrow()
{
    bArrowVisible = false;
    if (ActiveArrowWidget)
    {
        ActiveArrowWidget->RemoveFromParent();
        ActiveArrowWidget = nullptr;
    }
}

void ATutorialManager::UpdateArrow(float DeltaTime)
{
    if (!bArrowVisible) return;

    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC) return;

    // 计算从玩家到目标的屏幕方向
    FVector PlayerLoc;
    FRotator PlayerRot;
    PC->GetPlayerViewPoint(PlayerLoc, PlayerRot);

    FVector Direction = (ArrowTargetLocation - PlayerLoc).GetSafeNormal();
    FVector ScreenPos;
    PC->ProjectWorldLocationToScreen(ArrowTargetLocation, ScreenPos);

    // 更新箭头 widget 位置（屏幕边缘吸附）
    if (ActiveArrowWidget)
    {
        // 计算屏幕中心偏移
        int32 ViewportX, ViewportY;
        PC->GetViewportSize(ViewportX, ViewportY);

        float CenterX = ViewportX * 0.5f;
        float CenterY = ViewportY * 0.5f;

        // 限制在屏幕内（边缘 padding 50px）
        float Padding = 50.f;
        ScreenPos.X = FMath::Clamp(ScreenPos.X, Padding, ViewportX - Padding);
        ScreenPos.Y = FMath::Clamp(ScreenPos.Y, Padding, ViewportY - Padding);

        ActiveArrowWidget->SetPositionInViewport(FVector2D(ScreenPos.X, ScreenPos.Y));
    }
}

// ==================== 触发条件检查 ====================

void ATutorialManager::CheckTriggerConditions(float DeltaTime)
{
    if (CurrentStepIndex >= TutorialSteps.Num()) return;

    const FTutorialStep& Step = TutorialSteps[CurrentStepIndex];

    // Timer 触发
    if (Step.TriggerType == FName("Timer"))
    {
        float TimerDuration = FCString::Atof(*Step.TriggerParam);
        if (StepTimer >= TimerDuration)
        {
            CompleteCurrentStep();
        }
    }

    // Distance 触发（自动检测玩家到目标的距离）
    if (Step.TriggerType == FName("Distance") && !Step.ArrowTarget.IsEmpty())
    {
        APawn* PlayerPawn = GetWorld()->GetFirstPlayerController()->GetPawn();
        if (PlayerPawn)
        {
            float Dist = FVector::Dist(PlayerPawn->GetActorLocation(), ArrowTargetLocation);
            if (Dist < Step.TriggerDistance)
            {
                CompleteCurrentStep();
            }
        }
    }
}

// ==================== 步骤完成回调 ====================

void ATutorialManager::OnStepCompleted(const FTutorialStep& Step)
{
    UE_LOG(LogTutorial, Log, TEXT("[Tutorial] ✓ Step done: %s | Reward: %s"),
        *UEnum::GetValueAsString(Step.Phase),
        *Step.RewardText.ToString());

    // 高亮输入
    if (!Step.HighlightInputAction.IsNone())
    {
        ClearInputHighlight();
    }

    // 如果是距离触发，隐藏箭头
    if (Step.TriggerType == FName("Distance") || Step.TriggerType == FName("ReachedLocation"))
    {
        HideArrow();
    }
}

void ATutorialManager::PlayStepCompleteAudio()
{
    if (UAudioManager* Audio = GetWorld()->GetSubsystem<UAudioManager>())
    {
        Audio->PlayUISound(Audio_StepComplete);
    }
}

// ==================== 奖励 ====================

void ATutorialManager::GrantCompletionRewards()
{
    // 给玩家发钱
    if (AStellarGameMode* GM = Cast<AStellarGameMode>(GetWorld()->GetAuthGameMode()))
    {
        if (APawn* PlayerPawn = GetWorld()->GetFirstPlayerController()->GetPawn())
        {
            // 加货币
            // GM->AddCurrency(PlayerPawn, ECurrencyType::Credits, CompletionCreditsReward);

            UE_LOG(LogTutorial, Log, TEXT("Tutorial reward: %d Credits + %d XP + Item: %s"),
                CompletionCreditsReward, CompletionXP, *CompletionUnlockItem.ToString());
        }
    }
}

// ==================== 教程实体生成 ====================

void ATutorialManager::SpawnTutorialEntities()
{
    UWorld* World = GetWorld();
    if (!World) return;

    // 生成教程星球（小一号，加载快）
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // 教程星球
    TutorialPlanet = World->SpawnActor<AProceduralPlanet>(
        AProceduralPlanet::StaticClass(),
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        SpawnParams);

    if (TutorialPlanet)
    {
        TutorialPlanet->SetActorLabel(TutorialPlanetName);
        TutorialPlanet->RandomSeed = 2024; // 固定种子 → 每次相同
        TutorialPlanet->PlanetRadius = 50000.f; // 小星球，适合新手
        TutorialPlanet->bGenerateOcean = true;
        TutorialPlanet->bGenerateFoliage = true;
        TutorialPlanet->bGenerateBuildings = false; // 教程不需要建筑
        // TutorialPlanet->GeneratePlanet(); // 由 BeginPlay 自动调用
    }

    // 教程飞船（放在星球旁边）
    FVector ShipLocation = FVector(60000.f, 0, 0); // 低轨道
    TutorialShip = World->SpawnActor<AShipPawn>(
        AShipPawn::StaticClass(),
        ShipLocation,
        FRotator::ZeroRotator,
        SpawnParams);

    if (TutorialShip)
    {
        TutorialShip->SetActorLabel(TutorialShipName);
    }

    UE_LOG(LogTutorial, Log, TEXT("Tutorial entities spawned: Planet + Ship"));
}

void ATutorialManager::CleanupTutorialEntities()
{
    if (TutorialPlanet)
    {
        TutorialPlanet->Destroy();
        TutorialPlanet = nullptr;
    }
    if (TutorialShip)
    {
        TutorialShip->Destroy();
        TutorialShip = nullptr;
    }
}

// ==================== 阶段-specific 设置 ====================

void ATutorialManager::SetupIntroCinematic()
{
    UE_LOG(LogTutorial, Log, TEXT("[Tutorial] Phase: Intro Cinematic - 从太空俯瞰星球"));

    ShowCinematic(ETutorialPhase::Intro_Cinematic);

    // 3 秒后自动进入下一步
    GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
    {
        // 设置摄像机到太空视角
        if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
        {
            // 摄像机动画：从远处缓缓靠近星球
            PC->SetControlRotation(FRotator(-30.f, 0.f, 0.f));
        }

        // 3 秒后完成
        GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
        {
            // 等待 3 秒（实际用 Timer 触发）
        });

        // 用延迟实现
        FTimerHandle TimerHandle;
        GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this]()
        {
            if (CurrentPhase == ETutorialPhase::Intro_Cinematic)
            {
                CompleteCurrentStep();
            }
        }, 4.f, false);
    });
}

void ATutorialManager::SetupWalkingTutorial()
{
    UE_LOG(LogTutorial, Log, TEXT("[Tutorial] Phase: Walking - 学会 WASD + 空格 + 鼠标"));

    // 确保玩家在星球表面
    if (APawn* PlayerPawn = GetWorld()->GetFirstPlayerController()->GetPawn())
    {
        // 放到星球表面
        FVector SurfacePos = FVector(0, 0, 50000.f + 200.f); // 半径 + 身高
        PlayerPawn->SetActorLocation(SurfacePos);
    }

    // 高亮 WASD 输入
    HighlightInputAction(FName("IA_Move"));
}

void ATutorialManager::SetupFlightTutorial()
{
    UE_LOG(LogTutorial, Log, TEXT("[Tutorial] Phase: Flight - 起飞→轨道→跃迁"));

    HighlightInputAction(FName("IA_ToggleFlight"));
}

void ATutorialManager::SetupShipTutorial()
{
    UE_LOG(LogTutorial, Log, TEXT("[Tutorial] Phase: Ship - 靠近→登船→驾驶"));

    // 箭头指向飞船
    if (TutorialShip)
    {
        ShowArrow(TutorialShip->GetActorLocation());
    }

    HighlightInputAction(FName("IA_Interact"));
}

void ATutorialManager::SetupCombatTutorial()
{
    UE_LOG(LogTutorial, Log, TEXT("[Tutorial] Phase: Combat - 锁定→开火"));

    HighlightInputAction(FName("IA_LockOn"));
}

void ATutorialManager::SetupMiningTutorial()
{
    UE_LOG(LogTutorial, Log, TEXT("[Tutorial] Phase: Mining - 采矿→卖钱"));

    HighlightInputAction(FName("IA_Fire")); // 采矿用左键
}

void ATutorialManager::SetupRespawnTutorial()
{
    UE_LOG(LogTutorial, Log, TEXT("[Tutorial] Phase: Respawn - 设置复活点"));

    // 箭头指向安全区域
    ShowArrow(FVector(0, 0, 52000.f));
}

void ATutorialManager::SetupOutroCinematic()
{
    UE_LOG(LogTutorial, Log, TEXT("[Tutorial] Phase: Outro - 欢迎来到宇宙"));

    ShowCinematic(ETutorialPhase::Outro_Welcome);

    // 5 秒后完成教程
    FTimerHandle TimerHandle;
    GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this]()
    {
        CompleteTutorial();
    }, 5.f, false);
}

// ==================== 输入高亮 ====================

void ATutorialManager::HighlightInputAction(FName ActionName)
{
    WaitingForInput = true;
    WaitingForInputName = ActionName;

    UE_LOG(LogTutorial, Log, TEXT("Highlighting input: %s"), *ActionName.ToString());

    // 通知 UI 高亮对应按键提示
    if (ActivePromptWidget)
    {
        // 通过事件通知 Widget 高亮
        // ActivePromptWidget->HighlightKey(ActionName);
    }
}

void ATutorialManager::ClearInputHighlight()
{
    WaitingForInput = false;
    WaitingForInputName = FName();
}

// ==================== 存档 ====================

void ATutorialManager::LoadTutorialData()
{
    // 从 GameMode/存档系统读取
    if (AStellarGameMode* GM = Cast<AStellarGameMode>(GetWorld()->GetAuthGameMode()))
    {
        // 从 SaveManager 读取教程进度
        // FTutorialSaveData* Saved = GM->GetSaveManager()->LoadTutorialData();
        // if (Saved) SaveData = *Saved;
    }
}

void ATutorialManager::SaveTutorialData()
{
    if (AStellarGameMode* GM = Cast<AStellarGameMode>(GetWorld()->GetAuthGameMode()))
    {
        // GM->GetSaveManager()->SaveTutorialData(SaveData);
    }
}

// ==================== 默认步骤初始化 ====================

void ATutorialManager::InitializeDefaultSteps()
{
    TutorialSteps.Empty();

    // 0. 开场过场
    FTutorialStep Step0;
    Step0.Phase = ETutorialPhase::Intro_Cinematic;
    Step0.PromptText = FText::FromString(TEXT("欢迎来到 StellarSystem"));
    Step0.DetailText = FText::FromString(TEXT("你正从深空中俯瞰一颗新发现的星球。\n按任意键继续..."));
    Step0.TriggerType = FName("Timer");
    Step0.TriggerParam = TEXT("4.0");
    Step0.bPlayCinematic = true;
    Step0.RewardText = FText::FromString(TEXT("+ 探索开始"));
    TutorialSteps.Add(Step0);

    // 1. 走路
    FTutorialStep Step1;
    Step1.Phase = ETutorialPhase::Move_Walk;
    Step1.PromptText = FText::FromString(TEXT("移动：使用 W A S D 行走"));
    Step1.DetailText = FText::FromString(TEXT("你的角色会自动贴合星球表面。\n试试走到前方那个发光的标记点。"));
    Step1.TriggerType = FName("Distance");
    Step1.TriggerParam = TEXT("Waypoint_01");
    Step1.TriggerDistance = 800.f;
    Step1.bShowArrow = true;
    Step1.ArrowTarget = TEXT("Waypoint_01");
    Step1.HighlightInputAction = FName("IA_Move");
    Step1.RewardText = FText::FromString(TEXT("+ 学会了行走"));
    TutorialSteps.Add(Step1);

    // 2. 跳跃
    FTutorialStep Step2;
    Step2.Phase = ETutorialPhase::Move_Jump;
    Step2.PromptText = FText::FromString(TEXT("跳跃：按 空格键"));
    Step2.DetailText = FText::FromString(TEXT("在低重力环境下，你的跳跃会更高更远。\n试试跳到那个平台上。"));
    Step2.TriggerType = FName("InputPressed");
    Step2.TriggerParam = TEXT("IA_Jump");
    Step2.HighlightInputAction = FName("IA_Jump");
    Step2.RewardText = FText::FromString(TEXT("+ 学会了跳跃"));
    TutorialSteps.Add(Step2);

    // 3. 转视角
    FTutorialStep Step3;
    Step3.Phase = ETutorialPhase::Look_Mouse;
    Step3.PromptText = FText::FromString(TEXT("视角：移动鼠标环顾四周"));
    Step3.DetailText = FText::FromString(TEXT("转动鼠标可以自由观察周围环境。\n看看你周围的星球地貌。"));
    Step3.TriggerType = FName("Timer");
    Step3.TriggerParam = TEXT("3.0");
    Step3.RewardText = FText::FromString(TEXT("+ 学会了观察"));
    TutorialSteps.Add(Step3);

    // 4. 起飞
    FTutorialStep Step4;
    Step4.Phase = ETutorialPhase::Flight_TakeOff;
    Step4.PromptText = FText::FromString(TEXT("起飞：按 F 键离开星球表面"));
    Step4.DetailText = FText::FromString(TEXT("你的太空服内置推进器。\n按 F 起飞进入轨道，再按一次 F 降落。"));
    Step4.TriggerType = FName("InputPressed");
    Step4.TriggerParam = TEXT("IA_ToggleFlight");
    Step4.HighlightInputAction = FName("IA_ToggleFlight");
    Step4.RewardText = FText::FromString(TEXT("+ 进入轨道"));
    TutorialSteps.Add(Step4);

    // 5. 轨道飞行
    FTutorialStep Step5;
    Step5.Phase = ETutorialPhase::Flight_Orbit;
    Step5.PromptText = FText::FromString(TEXT("轨道飞行：WASD 平移 + 鼠标旋转"));
    Step5.DetailText = FText::FromString(TEXT("在轨道上你可以自由移动。\n用 W 向前推进，鼠标控制朝向。"));
    Step5.TriggerType = FName("Timer");
    Step5.TriggerParam = TEXT("5.0");
    Step5.RewardText = FText::FromString(TEXT("+ 掌握轨道机动"));
    TutorialSteps.Add(Step5);

    // 6. 跃迁
    FTutorialStep Step6;
    Step6.Phase = ETutorialPhase::Flight_Warp;
    Step6.PromptText = FText::FromString(TEXT("跃迁：按 G 进行超光速跃迁"));
    Step6.DetailText = FText::FromString(TEXT("G 键会自动锁定最近的星球并跃迁。\n感受一下超光速的震撼吧！"));
    Step6.TriggerType = FName("InputPressed");
    Step6.TriggerParam = TEXT("IA_Warp");
    Step6.HighlightInputAction = FName("IA_Warp");
    Step6.RewardText = FText::FromString(TEXT("+ 首次跃迁完成"));
    TutorialSteps.Add(Step6);

    // 7. 靠近飞船
    FTutorialStep Step7;
    Step7.Phase = ETutorialPhase::Ship_Approach;
    Step7.PromptText = FText::FromString(TEXT("接近飞船：飞向那艘闪烁的飞船"));
    Step7.DetailText = FText::FromString(TEXT("那就是你的第一艘飞船。\n飞近它，准备登船。"));
    Step7.TriggerType = FName("Distance");
    Step7.TriggerParam = TEXT("TutorialShip");
    Step7.TriggerDistance = 3000.f;
    Step7.bShowArrow = true;
    Step7.ArrowTarget = TEXT("TutorialShip");
    Step7.RewardText = FText::FromString(TEXT("+ 接近飞船"));
    TutorialSteps.Add(Step7);

    // 8. 登船
    FTutorialStep Step8;
    Step8.Phase = ETutorialPhase::Ship_Board;
    Step8.PromptText = FText::FromString(TEXT("登船：靠近飞船后按 E"));
    Step8.DetailText = FText::FromString(TEXT("E 键让你进入飞船并接管驾驶。\n准备体验太空飞行！"));
    Step8.TriggerType = FName("InputPressed");
    Step8.TriggerParam = TEXT("IA_Interact");
    Step8.HighlightInputAction = FName("IA_Interact");
    Step8.RewardText = FText::FromString(TEXT("+ 登船成功"));
    TutorialSteps.Add(Step8);

    // 9. 推进
    FTutorialStep Step9;
    Step9.Phase = ETutorialPhase::Ship_Thrust;
    Step9.PromptText = FText::FromString(TEXT("推进：W 加速 / S 减速"));
    Step9.DetailText = FText::FromString(TEXT("飞船有惯性，松手后还会滑行一段。\n感受一下太空飞行的真实手感。"));
    Step9.TriggerType = FName("InputPressed");
    Step9.TriggerParam = TEXT("IA_Thrust");
    Step9.HighlightInputAction = FName("IA_Thrust");
    Step9.RewardText = FText::FromString(TEXT("+ 掌握推进"));
    TutorialSteps.Add(Step9);

    // 10. 转向
    FTutorialStep Step10;
    Step10.Phase = ETutorialPhase::Ship_Steer;
    Step10.PromptText = FText::FromString(TEXT("转向：移动鼠标控制飞船朝向"));
    Step10.DetailText = FText::FromString(TEXT("飞船的朝向就是你的前进方向。\n试试绕星球飞一圈。"));
    Step10.TriggerType = FName("Timer");
    Step10.TriggerParam = TEXT("5.0");
    Step10.RewardText = FText::FromString(TEXT("+ 掌握转向"));
    TutorialSteps.Add(Step10);

    // 11. 滚转
    FTutorialStep Step11;
    Step11.Phase = ETutorialPhase::Ship_Roll;
    Step11.PromptText = FText::FromString(TEXT("滚转：Q / E 让飞船翻滚"));
    Step11.DetailText = FText::FromString(TEXT("在战斗中滚转可以躲避敌方火力。\n试试连续按 Q 和 E。"));
    Step11.TriggerType = FName("InputPressed");
    Step11.TriggerParam = TEXT("IA_Roll");
    Step11.HighlightInputAction = FName("IA_Roll");
    Step11.RewardText = FText::FromString(TEXT("+ 掌握滚转"));
    TutorialSteps.Add(Step11);

    // 12. 开火
    FTutorialStep Step12;
    Step12.Phase = ETutorialPhase::Ship_Fire;
    Step12.PromptText = FText::FromString(TEXT("开火：右键锁定目标，左键开火"));
    Step12.DetailText = FText::FromString(TEXT("远处有一个训练靶标。\n先右键锁定它，再左键开火摧毁。"));
    Step12.TriggerType = FName("Event");
    Step12.TriggerParam = TEXT("FirstKill");
    Step12.HighlightInputAction = FName("IA_Fire");
    Step12.RewardText = FText::FromString(TEXT("+ 首次击杀"));
    TutorialSteps.Add(Step12);

    // 13. PvP 遭遇
    FTutorialStep Step13;
    Step13.Phase = ETutorialPhase::Combat_PvP;
    Step13.PromptText = FText::FromString(TEXT("PvP：另一艘飞船正在接近！"));
    Step13.DetailText = FText::FromString(TEXT("这是一场训练战斗。\n用你学到的技能击败对手！"));
    Step13.TriggerType = FName("Event");
    Step13.TriggerParam = TEXT("PvPKill");
    Step13.RewardText = FText::FromString(TEXT("+ PvP 首胜"));
    TutorialSteps.Add(Step13);

    // 14. 采矿
    FTutorialStep Step14;
    Step14.Phase = ETutorialPhase::Mining_Laser;
    Step14.PromptText = FText::FromString(TEXT("采矿：飞到矿脉上方，左键开火"));
    Step14.DetailText = FText::FromString(TEXT("那些发光的岩石就是矿脉。\n用武器轰击它们收集矿石。"));
    Step14.TriggerType = FName("Event");
    Step14.TriggerParam = TEXT("FirstOreMined");
    Step14.RewardText = FText::FromString(TEXT("+ 首次采矿"));
    TutorialSteps.Add(Step14);

    // 15. 卖矿石
    FTutorialStep Step15;
    Step15.Phase = ETutorialPhase::Mining_Sell;
    Step15.PromptText = FText::FromString(TEXT("交易：飞向空间站并按 E 进入"));
    Step15.DetailText = FText::FromString(TEXT("前方有一个贸易空间站。\n进去把矿石卖掉换取 Credits。"));
    Step15.TriggerType = FName("Event");
    Step15.TriggerParam = TEXT("FirstSale");
    Step15.bShowArrow = true;
    Step15.ArrowTarget = TEXT("TutorialStation");
    Step15.RewardText = FText::FromString(TEXT("+ 首次交易"));
    TutorialSteps.Add(Step15);

    // 16. 买装备
    FTutorialStep Step16;
    Step16.Phase = ETutorialPhase::Shop_Buy;
    Step16.PromptText = FText::FromString(TEXT("装备：在商店里购买一把新武器"));
    Step16.DetailText = FText::FromString(TEXT("你有足够的 Credits 了。\n打开商店（按 B），买一把更好的武器。"));
    Step16.TriggerType = FName("Event");
    Step16.TriggerParam = TEXT("FirstPurchase");
    Step16.HighlightInputAction = FName("IA_Interact");
    Step16.RewardText = FText::FromString(TEXT("+ 首次购买"));
    TutorialSteps.Add(Step16);

    // 17. 体验死亡
    FTutorialStep Step17;
    Step17.Phase = ETutorialPhase::Death_Experience;
    Step17.PromptText = FText::FromString(TEXT("危险：你的护盾正在衰竭！"));
    Step17.DetailText = FText::FromString(TEXT("这是一次安全的训练死亡。\n看看死亡后会发生什么——别担心，你可以复活。"));
    Step17.TriggerType = FName("Event");
    Step17.TriggerParam = TEXT("PlayerDied");
    Step17.RewardText = FText::FromString(TEXT("+ 体验了死亡"));
    TutorialSteps.Add(Step17);

    // 18. 设置复活点
    FTutorialStep Step18;
    Step18.Phase = ETutorialPhase::Respawn_SetPoint;
    Step18.PromptText = FText::FromString(TEXT("复活点：按 H 设置当前位置为复活点"));
    Step18.DetailText = FText::FromString(TEXT("设置复活点后，你死亡时会在那里重生。\n找一个安全的地方设置它。"));
    Step18.TriggerType = FName("Event");
    Step18.TriggerParam = TEXT("RespawnPointSet");
    Step18.HighlightInputAction = FName("IA_Sprint"); // 复用或新增 IA_SetRespawn
    Step18.RewardText = FText::FromString(TEXT("+ 复活点已设置"));
    TutorialSteps.Add(Step18);

    // 19. 教程结束
    FTutorialStep Step19;
    Step19.Phase = ETutorialPhase::Outro_Welcome;
    Step19.PromptText = FText::FromString(TEXT("教程完成！欢迎来到宇宙"));
    Step19.DetailText = FText::FromString(TEXT("你已经掌握了生存的基本技能。\n现在，这个无限宇宙任你探索。\n祝你好运，飞行员。"));
    Step19.TriggerType = FName("Timer");
    Step19.TriggerParam = TEXT("5.0");
    Step19.bPlayCinematic = true;
    Step19.RewardText = FText::FromString(TEXT("+5000 Credits +1000 XP + 新手激光步枪"));
    TutorialSteps.Add(Step19);

    UE_LOG(LogTutorial, Log, TEXT("Initialized %d default tutorial steps"), TutorialSteps.Num());
}

// ==================== 调试命令 ====================

void ATutorialManager::Debug_JumpToPhase(int32 PhaseIndex)
{
    if (PhaseIndex < 0 || PhaseIndex >= TutorialSteps.Num()) return;

    UE_LOG(LogTutorial, Warning, TEXT("DEBUG: Jumping to phase %d"), PhaseIndex);

    CurrentStepIndex = PhaseIndex;
    CurrentPhase = TutorialSteps[PhaseIndex].Phase;
    StepTimer = 0.f;

    HidePrompt();
    ShowPrompt(TutorialSteps[PhaseIndex]);
}

void ATutorialManager::Debug_CompleteAllSteps()
{
    UE_LOG(LogTutorial, Warning, TEXT("DEBUG: Completing all tutorial steps"));

    while (CurrentStepIndex < TutorialSteps.Num())
    {
        CompleteCurrentStep();
    }
}

void ATutorialManager::Debug_ShowTutorialState()
{
    UE_LOG(LogTutorial, Warning, TEXT("=== TUTORIAL STATE ==="));
    UE_LOG(LogTutorial, Warning, TEXT("Active: %s"), bIsActive ? TEXT("YES") : TEXT("NO"));
    UE_LOG(LogTutorial, Warning, TEXT("Phase: %s"), *UEnum::GetValueAsString(CurrentPhase));
    UE_LOG(LogTutorial, Warning, TEXT("Step: %d / %d"), CurrentStepIndex, TutorialSteps.Num());
    UE_LOG(LogTutorial, Warning, TEXT("Progress: %.1f%%"), GetStepProgress() * 100.f);
    UE_LOG(LogTutorial, Warning, TEXT("Completed: %s"), SaveData.bTutorialCompleted ? TEXT("YES") : TEXT("NO"));
    UE_LOG(LogTutorial, Warning, TEXT("Skipped: %s"), SaveData.bTutorialSkipped ? TEXT("YES") : TEXT("NO"));
    UE_LOG(LogTutorial, Warning, TEXT("========================="));
}
