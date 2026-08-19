// TutorialSystem.h
// 新手教程系统：10 分钟引导式垂直切片
// 玩家在教程中学会：走路→跳跃→起飞→轨道→跃迁→登船→驾驶→开火→PvP→采矿→交易→死亡→复活
// 所有步骤由事件驱动，可跳过，可重玩

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "TutorialSystem.generated.h"

class UUserWidget;
class UTextBlock;
class UImage;
class UProgressBar;
class AProceduralPlanet;
class AShipPawn;
class AMyCharacter;
class UInputAction;
class UInputMappingContext;

// —— 教程阶段枚举 ——
UENUM(BlueprintType)
enum class ETutorialPhase : uint8
{
    None,                   // 未开始
    Intro_Cinematic,        // 0. 开场过场：从太空俯瞰星球
    Move_Walk,              // 1. 学会走路（WASD）
    Move_Jump,              // 2. 学会跳跃（空格）
    Look_Mouse,             // 3. 学会转视角（鼠标）
    Flight_TakeOff,         // 4. 学会起飞（F）
    Flight_Orbit,           // 5. 学会轨道飞行（WASD+鼠标）
    Flight_Warp,            // 6. 学会跃迁（G）
    Ship_Approach,          // 7. 学会靠近飞船
    Ship_Board,             // 8. 学会登船（E）
    Ship_Thrust,            // 9. 学会推进（W/S）
    Ship_Steer,             // 10. 学会转向（鼠标）
    Ship_Roll,              // 11. 学会滚转（Q/E）
    Ship_Fire,              // 12. 学会开火（锁定+左键）
    Combat_PvP,             // 13. 第一次 PvP 遭遇
    Mining_Laser,           // 14. 学会采矿（对准+左键）
    Mining_Sell,            // 15. 学会卖矿石（进空间站）
    Shop_Buy,               // 16. 学会买装备
    Death_Experience,       // 17. 体验死亡（可选）
    Respawn_SetPoint,       // 18. 学会设置复活点
    Outro_Welcome,          // 19. 教程结束：欢迎来到宇宙
    Completed               // 教程完成
};

// —— 单个教程步骤数据 ——
USTRUCT(BlueprintType)
struct FTutorialStep
{
    GENERATED_BODY()

    // 阶段标识
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ETutorialPhase Phase = ETutorialPhase::None;

    // 显示给玩家的提示文本
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText PromptText;

    // 详细说��（按 Tab 展开）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText DetailText;

    // 触发条件类型
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName TriggerType;  // "InputPressed" / "ReachedLocation" / "Timer" / "Event" / "Distance"

    // 触发参数
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TriggerParam; // 输入动作名 / 位置名 / 秒数 / 事件名

    // 触发距离（用于 ReachedLocation）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TriggerDistance = 500.f;

    // 超时自动跳过（秒，0=不超时）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TimeoutSeconds = 0.f;

    // 是否显示箭头指向目标
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bShowArrow = true;

    // 箭头指向的目标位置（世界坐标或 Actor 名）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ArrowTarget;

    // 完成后获得的奖励描述
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText RewardText;

    // 完成后的提示音
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName CompletionSound = FName("Tutorial_Complete");

    // 是否暂停游戏等待输入
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bPauseForInput = false;

    // 是否在完成时播放过场
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bPlayCinematic = false;

    // 关联的 Input Action（高亮提示用）
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName HighlightInputAction;
};

// —— 教程保存数据（记录进度） ——
USTRUCT(BlueprintType)
struct FTutorialSaveData
{
    GENERATED_BODY()

    UPROPERTY()
    ETutorialPhase CurrentPhase = ETutorialPhase::Intro_Cinematic;

    UPROPERTY()
    ETutorialPhase LastCompletedPhase = ETutorialPhase::None;

    UPROPERTY()
    bool bTutorialCompleted = false;

    UPROPERTY()
    bool bTutorialSkipped = false;

    UPROPERTY()
    FDateTime StartTime;

    UPROPERTY()
    FDateTime CompletionTime;

    UPROPERTY()
    int32 StepsCompleted = 0;

    UPROPERTY()
    int32 TotalSteps = 20;
};

// —— 教程管理器（单例，挂载在 GameMode 上） ——
UCLASS(BlueprintType)
class STELLARSYSTEM_API ATutorialManager : public AActor
{
    GENERATED_BODY()

public:
    ATutorialManager();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // —— 生命周期 ——
    UFUNCTION(BlueprintCallable, Category = "Tutorial")
    void StartTutorial();

    UFUNCTION(BlueprintCallable, Category = "Tutorial")
    void SkipTutorial();

    UFUNCTION(BlueprintCallable, Category = "Tutorial")
    void RestartTutorial();

    UFUNCTION(BlueprintCallable, Category = "Tutorial")
    void CompleteTutorial();

    // —— 步骤控制 ——
    UFUNCTION(BlueprintCallable, Category = "Tutorial")
    void AdvancePhase(ETutorialPhase NextPhase);

    UFUNCTION(BlueprintCallable, Category = "Tutorial")
    void CompleteCurrentStep();

    // —— 事件触发（由其他系统调用） ——
    UFUNCTION(BlueprintCallable, Category = "Tutorial")
    void OnInputPressed(const FName& InputActionName);

    UFUNCTION(BlueprintCallable, Category = "Tutorial")
    void OnReachedLocation(const FString& LocationName, const FVector& PlayerLocation);

    UFUNCTION(BlueprintCallable, Category = "Tutorial")
    void OnEventTriggered(const FName& EventName);

    UFUNCTION(BlueprintCallable, Category = "Tutorial")
    void OnDistanceCheck(const FString& TargetName, float Distance);

    // —— 查询 ——
    UFUNCTION(BlueprintPure, Category = "Tutorial")
    ETutorialPhase GetCurrentPhase() const { return CurrentPhase; }

    UFUNCTION(BlueprintPure, Category = "Tutorial")
    bool IsTutorialActive() const { return bIsActive; }

    UFUNCTION(BlueprintPure, Category = "Tutorial")
    FTutorialStep GetCurrentStep() const;

    UFUNCTION(BlueprintPure, Category = "Tutorial")
    float GetStepProgress() const;

    UFUNCTION(BlueprintPure, Category = "Tutorial")
    FText GetCurrentPrompt() const;

    UFUNCTION(BlueprintPure, Category = "Tutorial")
    FText GetCurrentDetail() const;

    // —— 配置 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
    TArray<FTutorialStep> TutorialSteps;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
    bool bAutoStartOnNewGame = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
    bool bAllowSkip = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
    float PromptDisplayDuration = 5.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
    FString TutorialPlanetName = TEXT("TutorialPrime");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
    FString TutorialShipName = TEXT("TutorialShip");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
    FString TutorialStationName = TEXT("TutorialStation");

    // —— UI 引用 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial|UI")
    TSoftClassPtr<UUserWidget> TutorialPromptWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial|UI")
    TSoftClassPtr<UUserWidget> TutorialCinematicWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial|UI")
    TSoftClassPtr<UUserWidget> TutorialCompleteWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial|UI")
    TSoftClassPtr<UUserWidget> TutorialArrowWidgetClass;

    // —— 音效 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial|Audio")
    FName Audio_StepComplete = FName("Tutorial_StepComplete");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial|Audio")
    FName Audio_PhaseAdvance = FName("Tutorial_PhaseAdvance");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial|Audio")
    FName Audio_TutorialComplete = FName("Tutorial_AllComplete");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial|Audio")
    FName Audio_PromptShow = FName("Tutorial_PromptShow");

    // —— 奖励 ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial|Rewards")
    int32 CompletionCreditsReward = 5000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial|Rewards")
    int32 CompletionXP = 1000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial|Rewards")
    FName CompletionUnlockItem = FName("Weapon_LaserRifle_Common");

    // —— 调试 ——
    UFUNCTION(Exec, Category = "Tutorial|Debug")
    void Debug_JumpToPhase(int32 PhaseIndex);

    UFUNCTION(Exec, Category = "Tutorial|Debug")
    void Debug_CompleteAllSteps();

    UFUNCTION(Exec, Category = "Tutorial|Debug")
    void Debug_ShowTutorialState();

protected:
    // 当前阶段
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    ETutorialPhase CurrentPhase = ETutorialPhase::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 CurrentStepIndex = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    bool bIsActive = false;

    // 计时器
    float StepTimer = 0.f;
    float PromptTimer = 0.f;

    // UI 实例
    UPROPERTY()
    UUserWidget* ActivePromptWidget = nullptr;

    UPROPERTY()
    UUserWidget* ActiveCinematicWidget = nullptr;

    UPROPERTY()
    UUserWidget* ActiveArrowWidget = nullptr;

    // 保存数据
    UPROPERTY()
    FTutorialSaveData SaveData;

    // 内部方法
    void LoadTutorialData();
    void SaveTutorialData();
    void InitializeDefaultSteps();
    void ShowPrompt(const FTutorialStep& Step);
    void HidePrompt();
    void ShowCinematic(ETutorialPhase Phase);
    void HideCinematic();
    void ShowArrow(const FVector& TargetLocation);
    void HideArrow();
    void UpdateArrow(float DeltaTime);
    void CheckTriggerConditions(float DeltaTime);
    void OnStepCompleted(const FTutorialStep& Step);
    void PlayStepCompleteAudio();
    void GrantCompletionRewards();
    void SpawnTutorialEntities();
    void CleanupTutorialEntities();

    // 阶段-specific 逻辑
    void SetupIntroCinematic();
    void SetupWalkingTutorial();
    void SetupFlightTutorial();
    void SetupShipTutorial();
    void SetupCombatTutorial();
    void SetupMiningTutorial();
    void SetupRespawnTutorial();
    void SetupOutroCinematic();

    // 输入高亮
    void HighlightInputAction(FName ActionName);
    void ClearInputHighlight();

private:
    // 教程专用实体（临时生成，完成后销毁）
    UPROPERTY()
    AProceduralPlanet* TutorialPlanet = nullptr;

    UPROPERTY()
    AShipPawn* TutorialShip = nullptr;

    // 标记
    bool bWaitingForInput = false;
    FName WaitingForInputName;
    bool bCinematicActive = false;
    FVector ArrowTargetLocation;
    bool bArrowVisible = false;
};
