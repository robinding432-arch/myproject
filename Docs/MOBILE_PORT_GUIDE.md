# Mobile Port Guide

## Overview

StellarSystem v7.2 compiles to **four platforms** from a single codebase:

| Platform | Architecture | Renderer | Build Target |
|---|---|---|---|
| Windows x64 | x86_64 | DirectX 12 | `StellarSystemClient` / `StellarSystemServer` / `StellarSystemWeGame` |
| Linux x64 | x86_64 | Vulkan | `StellarSystemClient` / `StellarSystemServer` |
| **Android** | **ARM64** | **Vulkan** | **`StellarSystemAndroid`** |
| **iOS** | **ARM64** | **Metal** | **`StellarSystemIOS`** |

## Architecture Decisions

### What We Cut on Mobile

| Feature | Desktop | Mobile | Reason |
|---|---|---|---|
| Steam Integration | ✅ | ❌ | Not available on mobile |
| WeGame Integration | ✅ (Win64) | ❌ | Rail SDK is Windows-only |
| Mod Support (Lua) | ✅ | ❌ | Sandbox prevents script loading |
| Ray Tracing | ✅ (DX12) | ❌ | Mobile GPUs don't support it |
| Volumetric Fog | ✅ | Simplified | Too expensive on mobile |
| Screen-Space Reflections | ✅ | ❌ | Replaced with cubemaps |
| Unlimited View Distance | ✅ | ❌ | Capped at 12km (Ultra) |
| 60Hz Physics | ✅ | 20-60Hz | Thermal constraint |
| Unlimited Particles | ✅ | 16-64 | GPU bandwidth |
| Voice Chat | ✅ | Optional | Data saver can disable |

### What We Added for Mobile

| Feature | Implementation |
|---|---|
| Touch Input Manager | Multi-touch, gestures, virtual joysticks |
| UI Scaler | DPI-aware, safe area, form factor detection |
| Performance Profile | Auto SoC detection → 5 quality tiers |
| LOD Manager | Aggressive culling, distance-based visibility |
| Network Adapter | WiFi/4G/5G detection, data saver, reconnect |
| Mobile Main Menu | Large buttons, touch-optimized layout |
| Mobile HUD | Collapsible panels, compact readouts |
| Background Handling | Keep-alive, timeout, auto-reconnect |

## Build Process

### Android (Linux/macOS)
```bash
# 1. Set UE root
export UE_ROOT=/path/to/UnrealEngine

# 2. Run packager
cd StellarSystem_v7.2
chmod +x Mobile/Build/PackageAndroid.sh
./Mobile/Build/PackageAndroid.sh Shipping StellarSystemAndroid

# 3. Output
# Binaries/Android/StellarSystem-Shipping-arm64.apk
```

### Android (Windows)
```cmd
set UE_ROOT=C:\Path\To\UnrealEngine
cd StellarSystem_v7.2
Mobile\Build\PackageAndroid.bat Shipping StellarSystemAndroid
```

### iOS (macOS only)
```bash
export UE_ROOT=/Applications/Epic\ Games/UE_5.3
cd StellarSystem_v7.2
chmod +x Mobile/Build/PackageIOS.sh
./Mobile/Build/PackageIOS.sh Shipping StellarSystemIOS
# Then open Xcode → Organizer → Distribute → TestFlight
```

## Quality Tiers

The `UMobilePerformanceProfile` subsystem auto-detects the device and applies the appropriate tier:

| Tier | SoC Examples | Resolution | Shadows | Textures | FPS |
|---|---|---|---|---|---|
| UltraLow | Snapdragon 6 Gen 1, Dimensity 700, A12 | 50% | Off | Low | 30 |
| Low | Snapdragon 7 Gen 1, Dimensity 900, A14 | 60% | Off | Med | 30 |
| **Medium** | **Snapdragon 7+ Gen 2, Dimensity 8200, A15** | **75%** | **Low** | **Med** | **45** |
| High | Snapdragon 8 Gen 2, Dimensity 9200, A16 | 85% | Med | High | 60 |
| Ultra | Snapdragon 8 Gen 3, Dimensity 9300, A17 Pro | 100% | High | Ultra | 60 |

Dynamic resolution scaling can further adjust within the tier's range.

## Touch Mapping

| Gesture | Desktop Equivalent | Game Action |
|---|---|---|
| Left joystick drag | WASD | Movement / Thrust |
| Right joystick drag | Mouse | Look / Aim |
| Single tap | Left click | Interact / Fire |
| Double tap | Double click | Sprint / Boost |
| Long press | Hold right click | Aim down sights |
| Swipe left/right | Mouse wheel | Previous/Next weapon |
| Swipe up/down | Shift+W/S | Thrust up/down |
| Pinch in/out | Scroll wheel | Zoom in/out |
| Two-finger tap | M key | Map toggle |

## Network Behavior

| Connection | Tick Rate | Compression | Voice | Notes |
|---|---|---|---|---|
| WiFi (Excellent) | 25-30 Hz | LZ4 | ✅ | Full quality |
| WiFi (Good) | 20 Hz | LZ4 | ✅ | Standard |
| 5G | 25 Hz | LZ4 | ✅ | Near-WiFi quality |
| 4G LTE | 15 Hz | LZ4 | Optional | Reduced cosmetics |
| 3G | 10 Hz | LZ4 | ❌ | Minimal, may be unplayable |
| **Data Saver** | **5-10 Hz** | **LZ4** | **❌** | **<100 KB/min** |

## Troubleshooting

| Problem | Solution |
|---|---|
| APK won't install | Enable "Unknown Sources" in Android settings |
| App crashes on launch | Check Vulkan support (`adb shell dumpsys gpu`) |
| Low FPS on flagship | First launch compiles shaders; play 5 min to warm up |
| Overheating after 15 min | Normal — game auto-downgrades quality |
| Can't connect to server | Check firewall; mobile uses port 7777 UDP |
| Background → foreground disconnect | Increase `BackgroundTimeout` in `DefaultMobile.ini` |
| iOS build fails | Ensure Xcode 15+, iOS 15+ deployment target |
| Android NDK errors | Use NDK r25b exactly (not newer) |

## Testing Checklist

### Functional
- [ ] Launch on low-end device (Snapdragon 6 series)
- [ ] Launch on flagship (Snapdragon 8 Gen 3 / iPhone 15 Pro)
- [ ] Touch controls responsive (no input lag)
- [ ] Virtual joysticks appear at touch point
- [ ] HUD readable (text not too small)
- [ ] Notch/punch-hole doesn't obscure UI
- [ ] Rotate device (landscape ↔ portrait)
- [ ] Background app → return (should reconnect)

### Performance
- [ ] Sustained 30+ FPS on low-end
- [ ] Sustained 45+ FPS on mid-range
- [ ] Sustained 60 FPS on flagship
- [ ] Memory <1.5GB on low-end
- [ ] Memory <2.5GB on flagship
- [ ] No thermal shutdown after 30 min
- [ ] Battery drain <15%/hour

### Network
- [ ] WiFi connection stable
- [ ] 4G connection playable
- [ ] Background timeout works (60s default)
- [ ] Reconnect after background <3s
- [ ] Data saver mode <100KB/min
- [ ] No desync after 10 min play

### Platform Submission
- [ ] Google Play: data safety disclosure complete
- [ ] Google Play: content rating assigned
- [ ] App Store: privacy manifest included
- [ ] App Store: NSSupportsOpeningDocumentsInPlace=false
- [ ] App Store: minimum iOS 15.0
- [ ] Both: age rating (PEGI 16 / ESRB T)
