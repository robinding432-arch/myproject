// PauseMenu.h
// 暂停菜单：继续 / 设置 / 存档 / 读档 / 返回主菜单
//
// v6.5 重要修正：
//   本菜单在多人游戏中只做"本地 UI 暂停"，绝不调用 PC->SetPause(true)。
//   SetPause(true) 在多人对战里会暂停整个 GameWorld（包括其他玩家的模拟），
//   导致 PvP 进程被冻结，这是 UE 经典坑。
//   正确做法：自己维护 bLocallyPaused 标志，只控制本地输入/UI/Tick，
//   游戏世界（物理/AI/网络复制）继续运行。
//
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PauseMenu.generated.h"

class UButton;
class UTextBlock;
class UWidgetSwitcher;

// 暂停菜单模式
UENUM(BlueprintType)
enum class EPauseMenuMode : uint8
{
    FullPause,       // 单人游戏：可暂停整个游戏
    LocalOnly,       // 多人游戏：仅本地 UI 暂停，不暂停世界
    Disabled         // 某些 PvP 模式完全禁止暂停
};

UCLASS(Blueprintable)
class STELLARSYSTEM_API UPauseMenu : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // ---- UI 绑定 ----
    UPROPERTY(meta = (BindWidget))
    UButton* Button_Resume = nullptr;

    UPROPERTY(meta = (BindWidget))
    UButton* Button_Settings = nullptr;

    UPROPERTY(meta = (BindWidget))
    UButton* Button_Save = nullptr;

    UPROPERTY(meta = (BindWidget))
    UButton* Button_Load = nullptr;

    UPROPERTY(meta = (BindWidget))
    UButton* Button_MainMenu = nullptr;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* Text_Status = nullptr;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* Text_Mode = nullptr; // 显示当前暂停模式提示

    // ---- 按钮回调 ----
    UFUNCTION()
    void OnResumeClicked();

    UFUNCTION()
    void OnSettingsClicked();

    UFUNCTION()
    void OnSaveClicked();

    UFUNCTION()
    void OnLoadClicked();

    UFUNCTION()
    void OnMainMenuClicked();

    // ---- 打开/关闭 ----
    UFUNCTION(BlueprintCallable, Category = "Pause")
    void OpenPauseMenu();

    UFUNCTION(BlueprintCallable, Category = "Pause")
    void ClosePauseMenu();

    // ---- 多人安全控制 ----
    // 由 GameMode 在 BeginPlay 时调用，告诉菜单当前是什么模式
    UFUNCTION(BlueprintCallable, Category = "Pause")
    void SetPauseMode(EPauseMenuMode NewMode);

    UFUNCTION(BlueprintPure, Category = "Pause")
    EPauseMenuMode GetPauseMode() const { return CurrentPauseMode; }

    // 查询：当前是否处于本地暂停状态（UI 打开 + 输入被拦截）
    UFUNCTION(BlueprintPure, Category = "Pause")
    bool IsLocallyPaused() const { return bLocallyPaused; }

    // 由 PlayerController 每帧查询：是否应该忽略输入
    UFUNCTION(BlueprintPure, Category = "Pause")
    bool ShouldBlockGameInput() const { return bLocallyPaused; }

private:
    // ---- 核心：本地暂停状态 ----
    // 这个标志替代了原来的 PC->SetPause(true)
    // 它只影响本地 UI 和输入，不影响其他玩家
    bool bLocallyPaused = false;

    // 暂停模式（由服务器/GameMode 决定）
    UPROPERTY()
    EPauseMenuMode CurrentPauseMode = EPauseMenuMode::LocalOnly;

    // 打开菜单前的输入模式（用于恢复）
    bool bHadMouseCursor = false;
    bool bWasGameInputMode = true;

    // 更新状态文本
    void UpdateStatusText();

    // 更新模式提示文本
    void UpdateModeText();

    // 执行真正的暂停/恢复（根据当前模式选择策略）
    void ApplyPauseState(bool bPause);

    // 在多人游戏中，暂停时只暂停本地模拟（不影响网络复制）
    void PauseLocalOnly();
    void ResumeLocalOnly();
};
