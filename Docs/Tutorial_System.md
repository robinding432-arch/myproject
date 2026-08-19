# 新手教程系统（10 分钟垂直切片）

## 概述

教程系统同时是**新手引导**和**游戏垂直切片**——玩家在前 10 分钟里学会的所有操作，恰好就是游戏的核心循环。

## 设计原则

1. **事件驱动**：每步由玩家输入/到达位置/系统事件触发，不靠计时器硬卡
2. **可跳过**：任何时候按 Esc → 跳过教程
3. **可重玩**：主菜单有"重玩教程"选项
4. **固定种子**：教程星球用固定 Seed=2024，每次体验完全一致
5. **零阻塞**：教程期间不弹模态对话框，提示在屏幕下方常驻

## 20 个步骤

| # | 阶段 | 玩家学到 | 触发方式 |
|---|---|---|---|
| 0 | 开场过场 | 宇宙氛围 | 4 秒计时 |
| 1 | WASD 行走 | 基础移动 | 走到标记点（距离<800） |
| 2 | 空格跳跃 | 跳跃 | 按空格 |
| 3 | 鼠标视角 | 环顾 | 3 秒计时 |
| 4 | F 起飞 | 进入轨道 | 按 F |
| 5 | 轨道飞行 | WASD+鼠标 | 5 秒计时 |
| 6 | G 跃迁 | 超光速 | 按 G |
| 7 | 靠近飞船 | 导航 | 距离飞船<3000 |
| 8 | E 登船 | 进入飞船 | 按 E |
| 9 | W/S 推进 | 飞船加速 | 按 W 或 S |
| 10 | 鼠标转向 | 飞船操控 | 5 秒计时 |
| 11 | Q/E 滚转 | 战斗机动 | 按 Q 或 E |
| 12 | 右键锁定+左键开火 | 武器 | 击毁训练靶 |
| 13 | PvP 遭遇 | 玩家战斗 | 击败训练 AI |
| 14 | 采矿 | 资源收集 | 采集第一块矿石 |
| 15 | 进空间站 | 交易 | 进入空间站 |
| 16 | 买装备 | 商店 | 完成首次购买 |
| 17 | 体验死亡 | 死亡机制 | 血量归零 |
| 18 | H 设复活点 | 复活系统 | 设置复活点 |
| 19 | 结尾过场 | 自由探索 | 5 秒计时 |

## 架构

```
ATutorialManager (Actor, 挂载在 GameMode 上)
 ├── TArray<FTutorialStep> TutorialSteps  ← 20 个步骤数据
 ├── ETutorialPhase CurrentPhase          ← 当前阶段
 ├── int32 CurrentStepIndex              ← 当前步骤索引
 ├── FTutorialSaveData SaveData          ← 进度存档
 │
 ├── UTutorialPromptWidget*               ← 底部提示条
 ├── UTutorialCinematicWidget*           ← 全屏过场
 └── UTutorialArrowWidget*               ← 屏幕边缘箭头

事件流：
  Player 按 W → InputComponent → OnInputPressed("IA_Move")
  → TutorialManager 检查当前步骤的 TriggerType
  → 匹配 → CompleteCurrentStep()
  → ShowPrompt(NextStep)
```

## 接入方式

### 在 GameMode 中创建

```cpp
// StellarGameMode.cpp
void AStellarGameMode::BeginPlay()
{
    Super::BeginPlay();

    // 创建教程管理器
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    TutorialManager = GetWorld()->SpawnActor<ATutorialManager>(
        ATutorialManager::StaticClass(),
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        Params);

    if (TutorialManager && bIsNewGame)
    {
        TutorialManager->StartTutorial();
    }
}
```

### 在其他系统中触发教程事件

```cpp
// 在 MiningSystem.cpp 中：
if (ATutorialManager* TM = GetTutorialManager())
{
    if (TM->GetCurrentPhase() == ETutorialPhase::Mining_Laser)
    {
        TM->OnEventTriggered(FName("FirstOreMined"));
    }
}

// 在 ShipWeapons.cpp 中：
if (ATutorialManager* TM = GetTutorialManager())
{
    if (TM->GetCurrentPhase() == ETutorialPhase::Ship_Fire)
    {
        TM->OnEventTriggered(FName("FirstKill"));
    }
}

// 在 PvPSystem.cpp 中：
if (ATutorialManager* TM = GetTutorialManager())
{
    if (TM->GetCurrentPhase() == ETutorialPhase::Combat_PvP)
    {
        TM->OnEventTriggered(FName("PvPKill"));
    }
}
```

### 在角色 InputComponent 中通知

```cpp
// MyCharacter.cpp
void AMyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    // ... 其他绑定 ...

    // 通知教程系统
    EIC->BindAction(InteractAction, ETriggerEvent::Started,
        this, &AMyCharacter::OnAnyInput);

    // 或者用 APlayerController 的 Input 事件
}

void AMyCharacter::OnAnyInput(const FInputActionValue& Value)
{
    if (AStellarGameMode* GM = Cast<AStellarGameMode>(GetWorld()->GetAuthGameMode()))
    {
        if (GM->TutorialManager)
        {
            GM->TutorialManager->OnInputPressed(FName("IA_Interact"));
        }
    }
}
```

## UI Widget 设置

### TutorialPromptWidget（底部提示条）

在编辑器中创建蓝图子类 `WBP_TutorialPrompt`，布局：

```
┌─────────────────────────────────────────────────────┐
│  [KeyIcon] 移动：使用 W A S D 行走              ▶  │
│  ─────────────────────────────────────────────────  │
│  你的角色会自动贴合星球表面。试试走到前方标记点。    │
│  ┌────────────────────────────┐ 奖励: + 学会了行走 │
│  │████████████░░░░░░░░░░░░│  [跳过教程]        │
│  └────────────────────────────┘                     │
└─────────────────────────────────────────────────────┘
```

控件命名（必须与 C++ 中的 BindWidget 匹配）：
- `PromptText` (TextBlock) — 主提示
- `DetailText` (TextBlock) — 详细说明
- `RewardText` (TextBlock) — 奖励描述
- `ProgressBar` (ProgressBar) — 进度 0~1
- `SkipButton` (Button) — 跳过教程
- `BackgroundPanel` (Image) — 半透明背景
- `KeyHighlightIcon` (Image) — 按键高亮图标

### TutorialCinematicWidget（全屏过场）

```
┌─────────────────────────────────────────────────────┐
│                                                     │
│              ╔══════════════════════╗              │
│              ║   STELLARSYSTEM      ║              │
│              ║   程序化宇宙引擎      ║              │
│              ╚══════════════════════╝              │
│                                                     │
│           在程序生成的无限宇宙中                    │
│           探索 · 贸易 · 战斗 · 生存               │
│                                                     │
│                  [ 继续 ]                           │
│                                                     │
└─────────────────────────────────────────────────────┘
```

控件命名：
- `BackgroundImage` (Image) — 星空背景
- `LogoImage` (Image) — 游戏 Logo
- `TitleText` (TextBlock)
- `SubtitleText` (TextBlock)
- `BodyText` (TextBlock)
- `ContinueButton` (Button)
- `ScanlineOverlay` (Image) — CRT 扫描线效果
- `VignetteOverlay` (Image) — 暗角效果

### TutorialArrowWidget（屏幕边缘箭头）

```
                    ↑
              ┌──────────┐
              │          │
              │  ARROW   │  ← 指向目标方向
              │   ➤      │
              │          │
              └──────────┘
```

控件命名：
- `ArrowImage` (Image) — 箭头图标（朝右的三角形）
- `ArrowCanvas` (CanvasPanel) — 用于定位

## 调试命令

在控制台（`~` 键）中输入：

```
# 跳到指定步骤
Debug_JumpToPhase 7

# 完成所有步骤
Debug_CompleteAllSteps

# 显示当前教程状态
Debug_ShowTutorialState
```

输出示例：
```
=== TUTORIAL STATE ===
Active: YES
Phase: Ship_Approach
Step: 7 / 20
Progress: 35.0%
Completed: NO
Skipped: NO
=========================
```

## 配置文件位置

教程步骤数据可以外部化为 JSON，方便策划调整：

```json
// Content/Data/TutorialSteps.json
{
    "Steps": [
        {
            "Phase": "Intro_Cinematic",
            "PromptText": "欢迎来到 StellarSystem",
            "DetailText": "你正从深空中俯瞰一颗新发现的星球。",
            "TriggerType": "Timer",
            "TriggerParam": "4.0",
            "RewardText": "+ 探索开始",
            "bPlayCinematic": true
        },
        ...
    ]
}
```

加载：
```cpp
void ATutorialManager::LoadTutorialFromJSON()
{
    FString FilePath = FPaths::ProjectContentDir() + TEXT("Data/TutorialSteps.json");
    FString JsonString;
    if (FFileHelper::LoadFileToString(JsonString, *FilePath))
    {
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
        if (FJsonSerializer::Deserialize(Reader, JsonObject))
        {
            // 解析并填充 TutorialSteps
        }
    }
}
```

## 已知限制

1. **教程飞船是临时的**：教程完成后销毁，不影响玩家后续获得的飞船
2. **教程星球固定 Seed**：确保每次体验一致，但缺乏惊喜感
3. **PvP 训练 AI 简单**：只做基础追踪+开火，后续可升级
4. **没有语音**：当前只有文本+音效，后续可加 TTS 或录音
5. **跳过教程后不可恢复进度**：跳过 = 全部完成，后续可改为"从跳过处继续"

## 后续增强

| 功能 | 说明 |
|---|---|
| 语音旁白 | MetaSounds 参数化 TTS |
| 分支教程 | 根据玩家选择（战斗/贸易/探索）走不同路径 |
| 多语言 | 所有 PromptText 走 Localization Dashboard |
| 成就解锁 | 完成教程 → Steam 成就 "First Steps" |
| 数据统计 | 每步耗时上传到 Analytics，优化引导流程 |
| A/B 测试 | 两种引导方案随机分配，看哪种留存高 |
