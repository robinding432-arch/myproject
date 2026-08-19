# 🎮 InputSetup.md — UE5 输入系统完整配置

> 最后更新: v6.0 | 共 22 个 Input Action + 1 个 IMC

---

## 一、创建 Input Actions（共 22 个）

### 方式 A: 编辑器创建（推荐新手）

Content Browser → 右键 → **Input** → **Input Action** → 命名如下：

### 方式 B: 自动创建（复制粘贴）

打开 `Edit → Project Settings → Input`，手动添加。

---

## 二、完整 Input Action 列表

### 🚶 角色移动（地面 + 飞船通用）

| # | 名称 | 值类型 | 用途 |
|---|---|---|---|
| 1 | `IA_Move` | Axis2D (Vector2D) | WASD 移动 |
| 2 | `IA_Look` | Axis2D (Vector2D) | 鼠标视角 |
| 3 | `IA_Jump` | Bool (Digital) | 空格跳跃 |
| 4 | `IA_Sprint` | Bool (Digital) | Shift 奔跑 |
| 5 | `IA_Crouch` | Bool (Digital) | Ctrl 蹲伏 |

### 🚀 飞船控制

| # | 名称 | 值类型 | 用途 |
|---|---|---|---|
| 6 | `IA_Thrust` | Axis1D (Float) | W/S 推进/减速 |
| 7 | `IA_Strafe` | Axis1D (Float) | A/D 平移 |
| 8 | `IA_Pitch` | Axis1D (Float) | 鼠标 Y 俯仰 |
| 9 | `IA_Yaw` | Axis1D (Float) | 鼠标 X 偏航 |
| 10 | `IA_Roll` | Axis1D (Float) | Q/E 滚转 |

### 🔫 战斗

| # | 名称 | 值类型 | 用途 |
|---|---|---|---|
| 11 | `IA_Fire` | Bool (Digital) | 左键开火 |
| 12 | `IA_LockOn` | Bool (Digital) | 右键锁定 |
| 13 | `IA_Reload` | Bool (Digital) | R 换弹 |
| 14 | `IA_NextWeapon` | Bool (Digital) | 滚轮上切武器 |
| 15 | `IA_PrevWeapon` | Bool (Digital) | 滚轮下切武器 |

### 🌍 星球交互

| # | 名称 | 值类型 | 用途 |
|---|---|---|---|
| 16 | `IA_Interact` | Bool (Digital) | E 交互/登船 |
| 17 | `IA_ToggleFlight` | Bool (Digital) | F 起飞/降落 |
| 18 | `IA_Warp` | Bool (Digital) | G 跃迁 |
| 19 | `IA_AutoWarp` | Bool (Digital) | R 自动跃迁（飞船内） |
| 20 | `IA_Starmap` | Bool (Digital) | T 星图 |

### 🎒 系统

| # | 名称 | 值类型 | 用途 |
|---|---|---|---|
| 21 | `IA_Inventory` | Bool (Digital) | I 背包 |
| 22 | `IA_QuestLog` | Bool (Digital) | J 任务日志 |
| 23 | `IA_Pause` | Bool (Digital) | Esc 暂停 |
| 24 | `IA_CycleConsumable` | Axis1D (Float) | 鼠标滚轮 |

---

## 三、创建 Input Mapping Context

### 文件名: `IMC_Default`

Content Browser → 右键 → **Input** → **Input Mapping Context**

双击打开，按以下表格添加映射：

### 3.1 IA_Move

| 键 | 轴 | 修饰符 |
|---|---|---|
| **W** → IA_Move | Y 轴 +1 | 无 |
| **S** → IA_Move | Y 轴 -1 | 无 |
| **A** → IA_Move | X 轴 -1 | 无 |
| **D** → IA_Move | X 轴 +1 | 无 |

> ⚠️ 如果需要，对 A/D 添加 **Negate** 修饰符来翻转方向

### 3.2 IA_Look

| 键 | 轴 |
|---|---|
| **Mouse XY** → IA_Look | XY 直接映射 |

### 3.3 动作映射

| Action | 键 | 备注 |
|---|---|---|
| IA_Jump | **Space** | |
| IA_Sprint | **Left Shift** | |
| IA_Crouch | **Left Ctrl** | |
| IA_Fire | **Left Mouse** | |
| IA_LockOn | **Right Mouse** | |
| IA_Reload | **R** | |
| IA_Interact | **E** | |
| IA_ToggleFlight | **F** | |
| IA_Warp | **G** | |
| IA_AutoWarp | **R**（飞船内） | 与 Reload 共享键，按上下文区分 |
| IA_Starmap | **T** | |
| IA_Inventory | **I** | |
| IA_QuestLog | **J** | |
| IA_Pause | **Esc** | |
| IA_NextWeapon | **Mouse Wheel Up** | |
| IA_PrevWeapon | **Mouse Wheel Down** | |

### 3.4 飞船专用（建议新建 `IMC_Ship`）

| Action | 轴/键 |
|---|---|
| IA_Thrust | W(+1) / S(-1) → Axis1D |
| IA_Strafe | A(-1) / D(+1) → Axis1D |
| IA_Pitch | Mouse Y → Axis1D |
| IA_Yaw | Mouse X → Axis1D |
| IA_Roll | Q(-1) / E(+1) → Axis1D |
| IA_Fire | Left Mouse |
| IA_LockOn | Right Mouse |
| IA_AutoWarp | R |
| IA_Warp | G |
| IA_ToggleFlight | F（降落） |

> 进入飞船时切换 `IMC_Ship`，离开时切回 `IMC_Default`

---

## 四、在蓝图中指认

### 4.1 BP_MyCharacter

打开 `BP_MyCharacter` → **Class Defaults** → Details 面板：

| 变量 | 类型 | 指认 |
|---|---|---|
| `Default Mapping Context` | UInputMappingContext* | `IMC_Default` |
| `Move Action` | UInputAction* | `IA_Move` |
| `Look Action` | UInputAction* | `IA_Look` |
| `Jump Action` | UInputAction* | `IA_Jump` |
| `Sprint Action` | UInputAction* | `IA_Sprint` |
| `Interact Action` | UInputAction* | `IA_Interact` |
| `ToggleFlight Action` | UInputAction* | `IA_ToggleFlight` |
| `Warp Action` | UInputAction* | `IA_Warp` |
| `AutoWarp Action` | UInputAction* | `IA_AutoWarp` |
| `Starmap Action` | UInputAction* | `IA_Starmap` |
| `Inventory Action` | UInputAction* | `IA_Inventory` |
| `QuestLog Action` | UInputAction* | `IA_QuestLog` |
| `Pause Action` | UInputAction* | `IA_Pause` |
| `CycleConsumable Action` | UInputAction* | `IA_CycleConsumable` |

### 4.2 BP_ShipPawn

打开 `BP_ShipPawn` → Class Defaults：

| 变量 | 指认 |
|---|---|
| `Default Mapping Context` | `IMC_Ship` |
| `Thrust Action` | `IA_Thrust` |
| `Strafe Action` | `IA_Strafe` |
| `Pitch Action` | `IA_Pitch` |
| `Yaw Action` | `IA_Yaw` |
| `Roll Action` | `IA_Roll` |
| `Fire Action` | `IA_Fire` |
| `LockOn Action` | `IA_LockOn` |
| `AutoWarp Action` | `IA_AutoWarp` |
| `Warp Action` | `IA_Warp` |
| `ToggleFlight Action` | `IA_ToggleFlight` |

---

## 五、C++ 端绑定代码参考

```cpp
// MyCharacter.cpp → SetupPlayerInputComponent
void AMyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        // 移动
        if (MoveAction)
            EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMyCharacter::Move);
        if (LookAction)
            EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMyCharacter::Look);
        if (JumpAction)
        {
            EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
            EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
        }
        if (SprintAction)
            EIC->BindAction(SprintAction, ETriggerEvent::Triggered, this, &AMyCharacter::Sprint);
        if (InteractAction)
            EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &AMyCharacter::Interact);
        if (ToggleFlightAction)
            EIC->BindAction(ToggleFlightAction, ETriggerEvent::Started, this, &AMyCharacter::ToggleFlight);
        if (WarpAction)
            EIC->BindAction(WarpAction, ETriggerEvent::Started, this, &AMyCharacter::WarpToNearest);
        if (AutoWarpAction)
            EIC->BindAction(AutoWarpAction, ETriggerEvent::Started, this, &AMyCharacter::AutoWarp);
        if (StarmapAction)
            EIC->BindAction(StarmapAction, ETriggerEvent::Started, this, &AMyCharacter::ToggleStarmap);
        if (PauseAction)
            EIC->BindAction(PauseAction, ETriggerEvent::Started, this, &AMyCharacter::OpenPauseMenu);
    }
}
```

---

## 六、常见坑

### ❌ 按了没反应
→ 检查：① IMC 是否指认到 BP ② Input Action 资产是否拖入 ③ BeginPlay 是否调用了 `AddMappingContext`

### ❌ WASD 方向反了
→ IMC 里给对应键加 **Negate** 修饰符

### ❌ 鼠标不转视角
→ 检查 `bUsePawnControlRotation = true` 和 `bUseControllerDesiredRotation`

### ❌ 飞船里还响应 WASD 走路
→ 登船时 `SetMappingContext(IMC_Ship, 1)` 切换，离船时切回

### ❌ 滚轮没反应
→ 确认 IA_CycleConsumable 是 **Axis1D (Float)** 不是 Bool

---

## 七、测试清单

- [ ] WASD 角色移动正常
- [ ] 鼠标转视角正常
- [ ] 空格跳跃
- [ ] Shift 奔跑（速度变快）
- [ ] E 靠近飞船后登船
- [ ] 登船后 W/S 推进/减速
- [ ] 鼠标控制俯仰/偏航
- [ ] Q/E 滚转
- [ ] 左键开火
- [ ] 右键锁定目标
- [ ] F 起飞/降落
- [ ] G 跃迁
- [ ] T 星图
- [ ] Esc 暂停
- [ ] I 背包
- [ ] J 任务日志
- [ ] 滚轮切换消耗品
- [ ] 离船后地面控制恢复

---

**按本指南操作，22 个 Input Action + 2 个 IMC 全部配置完成。**
