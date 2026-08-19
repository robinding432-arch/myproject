// WeGameAntiAddictionWidget.cpp
// v6.9 — 防沉迷提示 Widget 实现

#include "WeGame/WeGameAntiAddictionWidget.h"
#include "WeGame/WeGameIntegration.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

DEFINE_LOG_CATEGORY_STATIC(LogWeGameAAUI, Log, All);

void UWeGameAntiAddictionWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 自动绑定到 WeGameIntegration 的防沉迷事件
    if (UWeGameIntegration* WG = GEngine->GetEngineSubsystem<UWeGameIntegration>())
    {
        WG->OnAntiAddictionAction.AddDynamic(this, &UWeGameAntiAddictionWidget::HandleAntiAddictionEvent);
    }

    UE_LOG(LogWeGameAAUI, Log, TEXT("AntiAddictionWidget constructed"));
}

void UWeGameAntiAddictionWidget::NativeDestruct()
{
    if (UWeGameIntegration* WG = GEngine->GetEngineSubsystem<UWeGameIntegration>())
    {
        WG->OnAntiAddictionAction.RemoveDynamic(this, &UWeGameAntiAddictionWidget::HandleAntiAddictionEvent);
    }

    // 清除计时器
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DismissTimerHandle);
    }

    Super::NativeDestruct();
}

// ============================================================
//  显示方法
// ============================================================

void UWeGameAntiAddictionWidget::ShowTips(const FString& Title, const FString& Content, int32 DurationSeconds)
{
    bPendingForceExit = false;

    // 设置 UI 文本（蓝图子类需实现 BP_SetTitle/BP_SetContent）
    // 这里调用蓝图可重写的方法
    UE_LOG(LogWeGameAAUI, Log, TEXT("AntiAddiction Tips [%s]: %s (duration=%ds)"),
        *Title, *Content, DurationSeconds);

    // 设置自动关闭计时器
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            DismissTimerHandle,
            this,
            &UWeGameAntiAddictionWidget::OnAutoDismiss,
            FMath::Max(1.0f, (float)DurationSeconds),
            false
        );
    }
}

void UWeGameAntiAddictionWidget::ShowForceExitWarning(const FString& Content)
{
    bPendingForceExit = true;

    UE_LOG(LogWeGameAAUI, Warning, TEXT("AntiAddiction FORCE EXIT WARNING: %s"), *Content);

    // 显示警告，10 秒后自动退出
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            DismissTimerHandle,
            []()
            {
                // 请求退出
                if (GEngine && GEngine->GetCurrentPlayWorld())
                {
                    GEngine->GetCurrentPlayWorld()->GetAuthGameMode()->ReturnToMainMenuHost();
                }
                FPlatformMisc::RequestExit(false);
            },
            10.0f,
            false
        );
    }
}

void UWeGameAntiAddictionWidget::ShowCurfewNotice(const FString& Content)
{
    bPendingForceExit = false;

    UE_LOG(LogWeGameAAUI, Log, TEXT("AntiAddiction Curfew: %s"), *Content);

    // 宵禁通知，30 秒后自动关闭
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            DismissTimerHandle,
            this,
            &UWeGameAntiAddictionWidget::OnAutoDismiss,
            30.0f,
            false
        );
    }
}

void UWeGameAntiAddictionWidget::Dismiss()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DismissTimerHandle);
    }

    RemoveFromParent();
    OnDismissed.Broadcast();

    UE_LOG(LogWeGameAAUI, Log, TEXT("AntiAddiction widget dismissed"));
}

// ============================================================
//  静态工具：解析防沉迷 JSON
// ============================================================

void UWeGameAntiAddictionWidget::ParseAntiAddictionJSON(const FString& JSONString,
                                                        FString& OutTitle,
                                                        FString& OutContent,
                                                        int32& OutDuration,
                                                        bool& bOutIsForceExit)
{
    OutTitle = TEXT("防沉迷提示");
    OutContent = TEXT("");
    OutDuration = 60;
    bOutIsForceExit = false;

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JSONString);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        UE_LOG(LogWeGameAAUI, Warning, TEXT("Failed to parse anti-addiction JSON"));
        return;
    }

    // 解析 actions 数组
    const TArray<TSharedPtr<FJsonValue>>* Actions;
    if (JsonObject->TryGetArrayField(TEXT("actions"), Actions))
    {
        for (const TSharedPtr<FJsonValue>& ActionValue : *Actions)
        {
            const TSharedPtr<FJsonObject>* ActionObj;
            if (!ActionValue->TryGetObject(ActionObj) || !ActionObj) continue;

            // 获取 action.type
            const TSharedPtr<FJsonObject>* TypeObj;
            FString ActionName;
            if ((*ActionObj)->TryGetObjectField(TEXT("action"), TypeObj) && TypeObj)
            {
                (*TypeObj)->TryGetStringField(TEXT("name"), ActionName);
            }

            if (ActionName == TEXT("kRailAntiAddictionActionShowTips"))
            {
                (*ActionObj)->TryGetStringField(TEXT("title"), OutTitle);
                (*ActionObj)->TryGetStringField(TEXT("content"), OutContent);
                (*ActionObj)->TryGetNumberField(TEXT("display_duration_seconds"), OutDuration);
                bOutIsForceExit = false;
            }
            else if (ActionName == TEXT("kRailAntiAddictionActionHalt"))
            {
                bOutIsForceExit = true;
                if (OutContent.IsEmpty())
                {
                    OutContent = TEXT("根据防沉迷规定，您当前无法继续游戏。");
                }
            }
        }
    }
}

// ============================================================
//  内部
// ============================================================

void UWeGameAntiAddictionWidget::OnAutoDismiss()
{
    Dismiss();
}

// 事件处理（由蓝图子类实现 UI 更新）
void UWeGameAntiAddictionWidget::HandleAntiAddictionEvent(const FAntiAddictionEvent& Event)
{
    switch (Event.Action)
    {
        case EAntiAddictionAction::ShowTips:
            ShowTips(Event.Title, Event.Content, Event.DisplayDurationSeconds);
            break;

        case EAntiAddictionAction::ForceExit:
            ShowForceExitWarning(Event.Content);
            break;

        case EAntiAddictionAction::CurfewActive:
            ShowCurfewNotice(Event.Content);
            break;

        case EAntiAddictionAction::TimeWarning:
            ShowTips(Event.Title, Event.Content, Event.DisplayDurationSeconds);
            break;

        default:
            break;
    }
}
