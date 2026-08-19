# Mobile Performance Optimization

## SoC Detection & Scoring

The `UMobilePerformanceProfile` subsystem scores the device on 4 axes:

| Axis | Max Points | How Measured |
|---|---|---|
| RAM | 30 pts | `FPlatformMemory::GetPhysicalGBRam() * 1024` → min(RAM_MB/256, 30) |
| CPU Cores | 20 pts | `FPlatformMisc::NumberOfCores()` → min(cores*3, 20) |
| GPU | 20 pts | Estimated from vendor (Apple +15, Qualcomm +10, MediaTek +8, Google +12) |
| Form Factor | 5 pts | Tablet +5 (better thermals) |

**Total: 0-100 → mapped to 5 tiers**

## Quality Tier Reference

### UltraLow (Score 0-19)
```
ResolutionScale    = 0.50  (540p on 1080p screen)
ViewDistance      = 2,000 cm
ShadowQuality     = 0 (off)
TextureQuality    = 0 (256 MB pool)
EffectsQuality    = 0
FoliageDensity    = 0.2
AntiAliasing      = 0 (off)
TargetFrameRate   = 30
DynamicResolution = true (min 0.4)
ParticleQuality   = 0 (off)
PhysicsHz         = 20
AudioQuality      = 0 (low)
NetworkTickRate   = 10 Hz
```

### Low (Score 20-39)
```
ResolutionScale    = 0.60  (720p)
ViewDistance      = 3,500 cm
ShadowQuality     = 0 (off)
TextureQuality    = 1 (512 MB)
EffectsQuality    = 0
FoliageDensity    = 0.35
AntiAliasing      = 1 (FXAA)
TargetFrameRate   = 30
DynamicResolution = true (min 0.5)
ParticleQuality   = 0
PhysicsHz         = 25
AudioQuality      = 1
NetworkTickRate   = 15 Hz
```

### Medium (Score 40-59) ← **Default for most devices**
```
ResolutionScale    = 0.75  (810p)
ViewDistance      = 5,000 cm
ShadowQuality     = 1 (low)
TextureQuality    = 1 (512 MB)
EffectsQuality    = 1
FoliageDensity    = 0.50
AntiAliasing      = 1 (FXAA)
TargetFrameRate   = 45
DynamicResolution = true (min 0.6)
ParticleQuality   = 1 (low)
PhysicsHz         = 30
AudioQuality      = 1
NetworkTickRate   = 20 Hz
```

### High (Score 60-79)
```
ResolutionScale    = 0.85  (972p)
ViewDistance      = 8,000 cm
ShadowQuality     = 2 (med)
TextureQuality    = 2 (768 MB)
EffectsQuality    = 2
FoliageDensity    = 0.75
AntiAliasing      = 2 (TAA)
TargetFrameRate   = 60
DynamicResolution = true (min 0.7)
ParticleQuality   = 2 (med)
PhysicsHz         = 60
AudioQuality      = 2
NetworkTickRate   = 25 Hz
```

### Ultra (Score 80-100)
```
ResolutionScale    = 1.00  (1080p+)
ViewDistance      = 12,000 cm
ShadowQuality     = 3 (high)
TextureQuality    = 3 (1024 MB)
EffectsQuality    = 2
FoliageDensity    = 1.0
AntiAliasing      = 2 (TAA)
TargetFrameRate   = 60
DynamicResolution = false
ParticleQuality   = 2 (med)
PhysicsHz         = 60
AudioQuality      = 2
NetworkTickRate   = 30 Hz
```

## Dynamic Resolution

Runs every 1 second:

```pseudo
if (CurrentFPS < TargetFPS * 0.85)
    ResolutionScale = max(MinResolution, ResolutionScale - 0.05)
else if (CurrentFPS > TargetFPS * 0.98)
    ResolutionScale = min(MaxResolution, ResolutionScale + 0.02)
```

This means the game **self-tunes** — if it's running hot, it drops resolution; if it's cool, it creeps back up.

## Thermal Management

Every 5 seconds, check FPS trend:

| Condition | Action |
|---|---|
| Avg FPS < 60% of target **and** not already throttling | Set `bThermalThrottling=true`, downgrade 1 tier, fire `OnThermalWarning` |
| Avg FPS > 85% of target **and** throttling | Clear throttling flag |

**Downgrade path**: Ultra → High → Medium → Low → UltraLow (clamped at bottom).

## LOD Strategy Comparison

| Setting | Aggressive | Balanced | Quality |
|---|---|---|---|
| Max Static Mesh LOD | 3 | 2 | 1 |
| Max Skeletal LOD | 2 | 1 | 0 |
| Max Planet LOD | 3 | 2 | 1 |
| Max Ship LOD | 2 | 1 | 0 |
| Small Object Cull | 15m | 30m | 50m |
| Medium Object Cull | 40m | 80m | 120m |
| Large Object Cull | 100m | 200m | 300m |
| Particle Cull | 25m | 50m | 80m |
| Light Cull | 20m | 40m | 60m |
| Max Particles | 16 | 32 | 64 |
| Max Shadow Lights | 0 | 1 | 2 |
| Occlusion Aggression | 0.9 | 0.7 | 0.5 |
| Landscape LOD Bias | +2 | +1 | 0 |

## Network Optimization

### Tick Rate by Quality

| Tier | Tick Rate | Bandwidth (est.) |
|---|---|---|
| UltraLow | 10 Hz | ~50 KB/s |
| Low | 15 Hz | ~75 KB/s |
| Medium | 20 Hz | ~100 KB/s |
| High | 25 Hz | ~125 KB/s |
| Ultra | 30 Hz | ~150 KB/s |
| **Data Saver** | **5 Hz** | **<2 KB/s** |

### Data Saver Mode

When enabled:
- Network tick rate forced to 5-10 Hz
- Voice chat disabled
- Cosmetic replication skipped (`net.SkipCosmeticReplication=1`)
- Texture streaming pool reduced 50%
- Particle effects disabled
- Max bandwidth: 100 KB/minute

## Memory Budget

| Tier | Target RAM | Pool Sizes |
|---|---|---|
| UltraLow | 1.0 GB | Tex 256MB, Audio 32MB, Particles 16MB |
| Low | 1.5 GB | Tex 512MB, Audio 64MB, Particles 32MB |
| Medium | 2.0 GB | Tex 512MB, Audio 64MB, Particles 64MB |
| High | 3.0 GB | Tex 768MB, Audio 128MB, Particles 128MB |
| Ultra | 4.0 GB | Tex 1024MB, Audio 128MB, Particles 128MB |

## Profiling Tips

### Android
```bash
# Real-time GPU profiling
adb shell dumpsys gpu
adb shell cat /proc/gpufreq/gpufreq_table

# Memory
adb shell dumpsys meminfo com.yourstudio.stellarsystem

# Frame stats
adb shell dumpsys gfxinfo com.yourstudio.stellarsystem
```

### iOS
- Xcode → Debug → View Debugging → Capture GPU Frame
- Instruments → Game Performance → FPS / CPU / Memory
- Settings → Developer → Metal HUD (shows FPS + GPU time)

## Known Performance Hotspots

| Location | Cost | Mitigation |
|---|---|---|
| Planet LOD generation | 2-100ms spike | Frame budget (max 2 concurrent, yield every 32 rows) |
| Procedural buildings | 50-100ms spike | Incremental (3 buildings/frame) |
| Trade price calc (32 players) | O(n²) | O(1) cache, refresh 5s |
| Faction relation sync | All 36 pairs | Dirty-only incremental |
| Quest template gen | CPU heavy | Template cache by seed |
| LZ4 compression @ high BW | CPU > savings | Skip when BW > 500KB/s |

All six have been optimized in v7.1/v7.2.
