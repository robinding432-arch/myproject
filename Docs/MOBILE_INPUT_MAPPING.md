# Mobile Input Mapping

## Touch Gesture → Input Action Table

| Gesture | Input Action | Desktop Equivalent | Use Case |
|---|---|---|---|
| **Left Joystick** (drag) | `MoveForward` / `MoveRight` | WASD | Walking / flying thrust |
| **Right Joystick** (drag) | `LookUp` / `LookRight` | Mouse XY | Camera / aiming |
| **Single Tap** (anywhere) | `Jump` | Space | Jump / boost |
| **Double Tap** (target) | `Interact` | E | Interact / board ship |
| **Long Press** (1+ sec) | `Aim` | Right Mouse | ADS / precision mode |
| **Swipe Left** | `PrevWeapon` | Mouse Wheel ↓ | Previous weapon |
| **Swipe Right** | `NextWeapon` | Mouse Wheel ↑ | Next weapon |
| **Swipe Up** | `ThrustUp` | Shift+W | Vertical thrust up |
| **Swipe Down** | `ThrustDown` | Shift+S | Vertical thrust down |
| **Pinch In** (2-finger) | `ZoomIn` | Scroll Down | Zoom camera in |
| **Pinch Out** (2-finger) | `ZoomOut` | Scroll Up | Zoom camera out |
| **Two-Finger Tap** | `Map` | M | Toggle star map |
| **Edge Swipe (left)** | `PreviousTab` | Tab | Previous UI tab |
| **Edge Swipe (right)** | `NextTab` | Shift+Tab | Next UI tab |
| **3-Finger Tap** | `Screenshot` | F12 | Take screenshot |
| **Shake** (if supported) | `QuickMenu` | Q | Quick radial menu |

## On-Screen Buttons (always visible)

| Button | Position | Action | Input Action |
|---|---|---|---|
| **Fire** | Bottom-right | Primary fire | `Fire` |
| **Boost** | Bottom-center | Thrust boost | `Boost` |
| **Interact** | Center-right | Context action | `Interact` |
| **Menu** | Top-left | Open pause menu | `Menu` |
| **Map** | Top-right | Open star map | `Map` |
| **Jump** | Bottom-left (small) | Jump / toggle flight | `Jump` |
| **Target** | Right of fire button | Lock target | `TargetLock` |
| **Voice** | Top-right (small) | Push-to-talk | `VoiceChat` |

## Adaptive Joystick Behavior

| Setting | Default | Range | Description |
|---|---|---|---|
| `bAdaptiveJoystick` | true | bool | Joystick recenters where finger touches |
| `LeftDeadZone` | 0.15 | 0.05-0.3 | Left stick dead zone |
| `RightDeadZone` | 0.10 | 0.02-0.2 | Right stick dead zone |
| `DoubleTapTime` | 0.3s | 0.1-0.5s | Window for double-tap |
| `LongPressTime` | 0.6s | 0.3-1.0s | Duration for long-press |
| `SwipeMinDistance` | 50px | 20-100px | Min swipe length |
| `PinchMinDelta` | 20px | 10-50px | Min pinch change |
| `bHapticFeedback` | true | bool | Vibrate on joystick engage |

## HUD Layout (Landscape)

```
┌──────────────────────────────────────────┐
│  ╔═══╗              📶  🔋    [≡]  │  ← Top bar: signal, battery, menu
│  ║██║  [Map]                    │
│  ╚═══╝                              │
│                                      │
│                                      │
│         ┌──────────────┐             │
│         │  COMPASS     │             │
│         └──────────────┘             │
│                                      │
│  HP ████████░░  SH ██████░░░░    │  ← Vital bars
│  O₂ ██████████  EN ████████░░    │
│                                      │
│  Spd: 320 km/h  Alt: 1,250 m      │  ← Telemetry
│  Tgt: Bandit 2.3km  Hull 78%      │
│                                      │
│                                      │
│                                      │
│  [🎤]                    [🔫] [👁]  │  ← Voice / Fire / Target
│  [⬆]                              │  ← Jump
│  [💨]                              │  ← Boost
│ ╔══╗                                │
│ ║L║  ← Left Joystick (movement)    │
│ ╚══╝                                │
│       ╔══╗                          │
│       ║R ║ ← Right Joystick (look) │
│       ╚══╝                          │
└──────────────────────────────────────────┘
```

## HUD Layout (Portrait — fallback)

```
┌──────────────────┐
│ ╔═══╗       [≡] │
│ ║██║  [Map]    │
│ ╚═══╝           │
│                  │
│  HP ████████░░ │
│  SH ██████░░░░ │
│  O₂ ██████████ │
│  EN ████████░░ │
│                  │
│  Spd: 320  Alt:1250│
│                  │
│                  │
│ ╔══╗            │
│ ║L ║  [🔫][👁]│
│ ╚══╝  [💨][🎤]│
│       ╔══╗      │
│       ║R ║     │
│       ╚══╝     │
└──────────────────┘
```

## Safe Area Handling

| Device Feature | Top Inset | Bottom Inset | Left/Right |
|---|---|---|---|
| iPhone (notch) | 60px | 40px | 0 |
| iPhone (Dynamic Island) | 55px | 35px | 0 |
| Android (punch-hole L) | 50px | 30px | 0/40px |
| Android (punch-hole C) | 50px | 30px | 0 |
| Android (waterdrop) | 45px | 30px | 0 |
| iPad (no notch) | 20px | 20px | 0 |
| Foldable (open) | 30px | 30px | 0 |

→ All computed automatically by `UMobileUIScaler::ComputeSafeArea()`

## Customizing the Layout

In Blueprint, subclass `WMobileHUD` and override:

```blueprint
Event Construct:
  → Get Owning Player Pawn
  → Bind "OnFirePressed" → Your custom logic
  → Bind "OnTargetChanged" → Update reticle
  → Set HUD Mode (Combat/Nav/Mining/Trade/Social/Minimal)
```

Or in C++:

```cpp
// In your PlayerController
if (UMobileHUDWidget* HUD = GetMobileHUD())
{
    HUD->SetHUDMode(EHUDMode::Combat);
    HUD->OnFirePressed.AddDynamic(this, &AMyPC::HandleFire);
    HUD->OnBoostPressed.AddDynamic(this, &AMyPC::HandleBoost);
}
```

## Gesture Binding API

```cpp
// Bind a gesture to any Input Action at runtime
UTouchInputManager* Touch = GetTouchManager();
Touch->BindGestureToAction(ETouchGesture::SwipeLeft, FName("PrevWeapon"));
Touch->BindGestureToAction(ETouchGesture::PinchOut, FName("ZoomOut"));

// Query binding
FName Action = Touch->GetGestureBinding(ETouchGesture::DoubleTap);

// Disable touch (e.g., when in text input)
Touch->SetTouchEnabled(false);
```

## Testing Touch Input

### Android (ADB)
```bash
# Simulate touch at (500, 800)
adb shell input tap 500 800

# Simulate swipe
adb shell input swipe 500 800 800 600 300

# Simulate pinch (two-finger)
adb shell input swipe 500 500 400 400 200 & \
adb shell input swipe 500 500 600 600 200
```

### iOS (Xcode)
- Window → Devices → Open Console
- Use Automation Instrument for gesture recording
- Simulate Location for GPS-based features
