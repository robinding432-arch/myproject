// PauseMenuWidget.h
// 暂停菜单：存档/读档/任务/成就/设置/退出
// 死亡界面：复活点选择/观战/返回主菜单

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PauseMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;
class UHorizontalBox;
class UBorder;
class UProgressBar;
class UImage;

UENUM(BlueprintType)
enum class EPauseMenuTab : uint8
{
    SaveLoad,      // 存档/读档
    Quests,        // 任务列表
    Achievements,  // 成就
    Settings,      // 设置
    Respawn,       // 复活点（死亡时显示）
    Spectate,      // 观战（死亡时显示）
    ConfirmQuit     // 确认退出
};

UENUM(BlueprintType)
enum class ERespawnResult : uint8
{
    Respawned,     // 成功复活
    Spectating,    // 进入观战
    ReturnToMenu,  // 返回主菜单
    QuitGame       // 退出游戏
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPauseTabChanged, EPauseMenuTab, NewTab);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRespawnSelected, FString, RespawnPointID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnReturnToMainMenu);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuitConfirmed, bool, bSaveFirst);

UCLASS(Blueprintable, BlueprintType)
class UPauseMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPauseMenuWidget(const FObjectInitializer& ObjectInitializer);

    // —— 状态 ——
    UPROPERTY(BlueprintReadOnly, Category = "Pause")
    EPauseMenuTab CurrentTab = EPauseMenuTab::SaveLoad;

    UPROPERTY(BlueprintReadOnly, Category = "Pause")
    bool bIsDead = false;

    UPROPERTY(BlueprintReadOnly, Category = "Pause")
    float RespawnCountdown = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Pause")
    FString LastDeathCause;

    // —— 事件 ——
    UPROPERTY(BlueprintAssignable, Category = "Pause|Events")
    FOnPauseTabChanged OnTabChanged;

    UPROPERTY(BlueprintAssignable, Category = "Pause|Events")
    FOnRespawnSelected OnRespawnSelected;

    UPROPERTY(BlueprintAssignable, Category = "Pause|Events")
    FOnReturnToMainMenu OnReturnToMainMenu;

    UPROPERTY(BlueprintAssignable, Category = "Pause|Events")
    FOnQuitConfirmed OnQuitConfirmed;

    // —— 接口 ——
    UFUNCTION(BlueprintCallable, Category = "Pause")
    void SwitchTab(EPauseMenuTab NewTab);

    UFUNCTION(BlueprintCallable, Category = "Pause")
    void SaveGame(const FString& SlotName);

    UFUNCTION(BlueprintCallable, Category = "Pause")
    void LoadGame(const FString& SlotName);

    UFUNCTION(BlueprintCallable, Category = "Pause")
    TArray<FString> GetSaveSlotList() const;

    UFUNCTION(BlueprintCallable, Category = "Pause")
    FString GetSaveInfo(const FString& SlotName) const;

    UFUNCTION(BlueprintCallable, Category = "Pause")
    void DeleteSave(const FString& SlotName);

    // 快速保存（自动命名）
    UFUNCTION(BlueprintCallable, Category = "Pause")
    void QuickSave();

    // 快速读取（最近存档）
    UFUNCTION(BlueprintCallable, Category = "Pause")
    void QuickLoad();

    // —— 复活系统 ——
    UFUNCTION(BlueprintCallable, Category = "Respawn")
    void SetDeathState(const FString& DeathCause, float CountdownTime);

    UFUNCTION(BlueprintCallable, Category = "Respawn")
    void RespawnAtPoint(const FString& RespawnPointID);

    UFUNCTION(BlueprintCallable, Category = "Respawn")
    void EnterSpectatorMode();

    UFUNCTION(BlueprintCallable, Category = "Respawn")
    void SetRespawnPoints(const TArray<FString>& PointNames,
        const TArray<FString>& PointIDs);

    UFUNCTION(BlueprintCallable, Category = "Respawn")
    FString GetNearestRespawnPoint() const;

    // —— 退出 ——
    UFUNCTION(BlueprintCallable, Category = "Pause")
    void ConfirmQuit(bool bSaveBeforeQuit);

    UFUNCTION(BlueprintCallable, Category = "Pause")
    void ReturnToMainMenu();

    // —— 任务/成就（数据显示） ——
    UFUNCTION(BlueprintCallable, Category = "Quests")
    TArray<FString> GetActiveQuests() const;

    UFUNCTION(BlueprintCallable, Category = "Quests")
    FString GetQuestDescription(const FString& QuestID) const;

    UFUNCTION(BlueprintCallable, Category = "Achievements")
    TArray<FString> GetUnlockedAchievements() const;

    UFUNCTION(BlueprintCallable, Category = "Achievements")
    TArray<FString> GetLockedAchievements() const;

    UFUNCTION(BlueprintCallable, Category = "Achievements")
    float GetAchievementProgress(const FString& AchievementID) const;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // 倒计时
    UPROPERTY(BlueprintReadOnly, Category = "Respawn")
    float CountdownTimer = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Respawn")
    float DefaultRespawnDelay = 10.f;

    // 复活点列表
    UPROPERTY(BlueprintReadOnly, Category = "Respawn")
    TArray<FString> RespawnPointNames;

    UPROPERTY(BlueprintReadOnly, Category = "Respawn")
    TArray<FString> RespawnPointIDs;

    // 自动复活
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Respawn")
    bool bAutoRespawn = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Respawn")
    float AutoRespawnDelay = 15.f;

private:
    void TickCountdown(float DeltaTime);
    void AutoRespawnIfEnabled();
    void PopulateSaveList();
    void PopulateQuestList();
    void PopulateAchievementList();

    // 上次选中的复活点
    FString SelectedRespawnPoint;
};
