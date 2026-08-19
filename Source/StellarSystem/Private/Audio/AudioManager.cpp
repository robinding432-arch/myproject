// ============================================================
// AudioManager.cpp — 增强版完整实现
// 路径: Source/StellarSystem/Private/Audio/AudioManager.cpp
// ============================================================

#include "Audio/AudioManager.h"
#include "Planet/ProceduralPlanet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundAttenuation.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Math/UnrealMathUtility.h"

// ======================== 初始化 ========================

void UAudioManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // 初始化默认值
    EngineHumVolume = 0.f;
    WarpVolume = 0.f;
    ShieldHumVolume = 0.f;
    AmbientVol = AmbientVolume;
    CurrentMusic = EudioCategory::Music_Exploration;
    CurrentBiome = EBiomeType::Ocean;
    bWarpAudioActive = false;

    // 默认衰减
    DefaultAttenuation.MinDistance = 100.f;
    DefaultAttenuation.MaxDistance = 100000.f;
    DefaultAttenuation.FalloffExponent = 1.5f;
    DefaultAttenuation.LPFFreqAtMax = 2000.f;
    DefaultAttenuation.bEnableOcclusion = true;

    // 默认程序化参数
    EngineHumParams.BaseFrequency = 80.f;
    EngineHumParams.FrequencyEnd = 120.f;
    EngineHumParams.Duration = 2.f;
    EngineHumParams.Amplitude = 0.5f;
    EngineHumParams.Roughness = 0.3f;
    EngineHumParams.Resonance = 0.6f;
    EngineHumParams.AttackTime = 0.5f;
    EngineHumParams.DecayTime = 1.f;
    EngineHumParams.FilterCutoff = 800.f;

    WarpSoundParams.BaseFrequency = 200.f;
    WarpSoundParams.FrequencyEnd = 2000.f;
    WarpSoundParams.Duration = 3.f;
    WarpSoundParams.Amplitude = 0.7f;
    WarpSoundParams.Roughness = 0.5f;
    WarpSoundParams.Resonance = 0.8f;
    WarpSoundParams.AttackTime = 0.2f;
    WarpSoundParams.DecayTime = 2.f;
    WarpSoundParams.FilterCutoff = 5000.f;

    WeaponSoundParams.BaseFrequency = 440.f;
    WeaponSoundParams.FrequencyEnd = 100.f;
    WeaponSoundParams.Duration = 0.3f;
    WeaponSoundParams.Amplitude = 0.8f;
    WeaponSoundParams.Roughness = 0.7f;
    WeaponSoundParams.Resonance = 0.4f;
    WeaponSoundParams.AttackTime = 0.01f;
    WeaponSoundParams.DecayTime = 0.2f;
    WeaponSoundParams.FilterCutoff = 8000.f;

    ExplosionSoundParams.BaseFrequency = 50.f;
    ExplosionSoundParams.FrequencyEnd = 10.f;
    ExplosionSoundParams.Duration = 1.5f;
    ExplosionSoundParams.Amplitude = 1.f;
    ExplosionSoundParams.Roughness = 0.9f;
    ExplosionSoundParams.Resonance = 0.3f;
    ExplosionSoundParams.AttackTime = 0.05f;
    ExplosionSoundParams.DecayTime = 1.2f;
    ExplosionSoundParams.FilterCutoff = 3000.f;

    UE_LOG(LogTemp, Log, TEXT("[Audio] Manager initialized"));
}

void UAudioManager::Deinitialize()
{
    StopAllLoopingSounds();
    StopAllAmbient(0.f);
    StopAllWeatherSounds(0.f);

    // 停所有音乐
    for (auto& Pair : ActiveMusic)
    {
        if (Pair.Value && Pair.Value->IsPlaying())
        {
            Pair.Value->Stop();
        }
    }
    ActiveMusic.Empty();

    Super::Deinitialize();
}

// ======================== 内部工具 ========================

USoundBase* UAudioManager::GetSound(EAudioCategory Category) const
{
    if (const USoundBase* const* Found = SoundMap.Find(Category))
    {
        return const_cast<USoundBase*>(*Found);
    }
    return nullptr;
}

float UAudioManager::GetFinalVolume(float CategoryVolume) const
{
    return CategoryVolume * MasterVolume;
}

void UAudioManager::ApplyVolumeToLooping(EAudioCategory Category, float Volume)
{
    if (UAudioComponent* const* Found = LoopingSounds.Find(Category))
    {
        if (*Found && (*Found)->IsPlaying())
        {
            (*Found)->SetVolumeMultiplier(Volume * MasterVolume);
        }
    }
}

void UAudioManager::FadeOutAndStop(UAudioComponent* Comp, float FadeTime)
{
    if (!Comp || !Comp->IsPlaying()) return;

    // 简化淡出：线性降低音量后停止
    float StartVol = Comp->VolumeMultiplier;
    float FadeRate = StartVol / FMath::Max(FadeTime, 0.01f);

    // 用定时器实现淡出
    FTimerHandle TimerHandle;
    GetWorld()->GetTimerManager().SetTimer(TimerHandle, [Comp, FadeRate]()
    {
        if (!Comp || !Comp->IsPlaying()) return;

        float NewVol = Comp->VolumeMultiplier - FadeRate * 0.1f;
        if (NewVol <= 0.01f)
        {
            Comp->Stop();
            Comp->VolumeMultiplier = 0.f;
        }
        else
        {
            Comp->SetVolumeMultiplier(NewVol);
        }
    }, 0.1f, true, 0.f);
}

// ======================== 播放接口 ========================

UAudioComponent* UAudioManager::PlaySoundAtLocation(EAudioCategory Category,
    const FVector& Location, float VolumeMultiplier, float PitchMultiplier)
{
    USoundBase* Sound = GetSound(Category);
    if (!Sound) return nullptr;

    float FinalVolume = VolumeMultiplier * SFXVolume * MasterVolume;

    UAudioComponent* Comp = UGameplayStatics::SpawnSoundAtLocation(
        GetWorld(), Sound, Location,
        FRotator::ZeroRotator, FinalVolume, PitchMultiplier,
        0.f, nullptr);

    return Comp;
}

UAudioComponent* UAudioManager::PlaySoundAttached(EAudioCategory Category,
    USceneComponent* AttachTo, float VolumeMultiplier)
{
    if (!AttachTo) return nullptr;
    USoundBase* Sound = GetSound(Category);
    if (!Sound) return nullptr;

    float FinalVolume = VolumeMultiplier * SFXVolume * MasterVolume;

    UAudioComponent* Comp = UGameplayStatics::SpawnSoundAttached(
        Sound, AttachTo, NAME_None, FVector::ZeroVector,
        EAttachLocation::SnapToTarget, true, FinalVolume, 1.f, 0.f, nullptr);

    return Comp;
}

UAudioComponent* UAudioManager::PlaySound2D(EAudioCategory Category, float VolumeMultiplier)
{
    USoundBase* Sound = GetSound(Category);
    if (!Sound) return nullptr;

    float FinalVolume = VolumeMultiplier * SFXVolume * MasterVolume;

    UAudioComponent* Comp = UGameplayStatics::SpawnSound2D(
        GetWorld(), Sound, FinalVolume, 1.f, 0.f, nullptr, true);

    return Comp;
}

// ======================== 持续音效 ========================

void UAudioManager::SetEngineHumVolume(float Volume)
{
    EngineHumVolume = FMath::Clamp(Volume, 0.f, 1.f);

    if (UAudioComponent* const* Found = LoopingSounds.Find(EAudioCategory::EngineHum))
    {
        if (*Found)
        {
            (*Found)->SetVolumeMultiplier(EngineHumVolume * SFXVolume * MasterVolume);
        }
    }
    else if (EngineHumVolume > 0.01f)
    {
        USoundBase* Sound = GetSound(EAudioCategory::EngineHum);
        if (!Sound) Sound = GenerateEngineHum(EngineHumParams.BaseFrequency, EngineHumParams.Roughness);
        if (Sound)
        {
            UAudioComponent* Comp = PlaySound2D(EAudioCategory::EngineHum, EngineHumVolume);
            if (Comp)
            {
                Comp->bIsLooping = true;
                Comp->Play();
                LoopingSounds.Add(EAudioCategory::EngineHum, Comp);
            }
        }
    }
}

void UAudioManager::SetWarpVolume(float Volume)
{
    WarpVolume = FMath::Clamp(Volume, 0.f, 1.f);

    if (UAudioComponent* const* Found = LoopingSounds.Find(EAudioCategory::WarpTravel))
    {
        if (*Found)
        {
            (*Found)->SetVolumeMultiplier(WarpVolume * SFXVolume * MasterVolume);
        }
    }
    else if (WarpVolume > 0.01f)
    {
        USoundBase* Sound = GetSound(EAudioCategory::WarpTravel);
        if (!Sound) Sound = GenerateWarpSound(WarpSoundParams.BaseFrequency, 2.f);
        if (Sound)
        {
            UAudioComponent* Comp = PlaySound2D(EAudioCategory::WarpTravel, WarpVolume);
            if (Comp)
            {
                Comp->bIsLooping = true;
                Comp->Play();
                LoopingSounds.Add(EAudioCategory::WarpTravel, Comp);
            }
        }
    }
}

void UAudioManager::SetShieldHumVolume(float Volume)
{
    ShieldHumVolume = FMath::Clamp(Volume, 0.f, 1.f);

    if (UAudioComponent* const* Found = LoopingSounds.Find(EAudioCategory::ShieldHum))
    {
        if (*Found)
        {
            (*Found)->SetVolumeMultiplier(ShieldHumVolume * SFXVolume * MasterVolume);
        }
    }
    else if (ShieldHumVolume > 0.01f)
    {
        USoundBase* Sound = GetSound(EAudioCategory::ShieldHum);
        if (Sound)
        {
            UAudioComponent* Comp = PlaySound2D(EAudioCategory::ShieldHum, ShieldHumVolume);
            if (Comp)
            {
                Comp->bIsLooping = true;
                Comp->Play();
                LoopingSounds.Add(EAudioCategory::ShieldHum, Comp);
            }
        }
    }
}

void UAudioManager::SetAmbientVolume(float Volume)
{
    AmbientVol = FMath::Clamp(Volume, 0.f, 1.f);
    AmbientVolume = AmbientVol;

    for (auto& Pair : ActiveAmbient)
    {
        if (Pair.Value && Pair.Value->IsPlaying())
        {
            Pair.Value->SetVolumeMultiplier(AmbientVol * MasterVolume);
        }
    }
}

void UAudioManager::StopAllLoopingSounds()
{
    for (auto& Pair : LoopingSounds)
    {
        if (Pair.Value && Pair.Value->IsPlaying())
        {
            Pair.Value->Stop();
        }
    }
    LoopingSounds.Empty();
    EngineHumVolume = 0.f;
    WarpVolume = 0.f;
    ShieldHumVolume = 0.f;
}

void UAudioManager::StopLoopingSound(EAudioCategory Category)
{
    if (UAudioComponent* const* Found = LoopingSounds.Find(Category))
    {
        if (*Found && (*Found)->IsPlaying())
        {
            (*Found)->Stop();
        }
        LoopingSounds.Remove(Category);
    }
}

// ======================== 音乐控制 ========================

void UAudioManager::PlayMusic(EAudioCategory MusicCategory, bool bFade)
{
    // 找到对应轨道
    USoundBase* Track = nullptr;
    float BaseVol = 0.7f;
    bool bShouldLoop = true;

    for (const FMusicTrack& MT : MusicTracks)
    {
        if (MT.Category == MusicCategory)
        {
            Track = MT.Track;
            BaseVol = MT.BaseVolume;
            bShouldLoop = MT.bLoop;
            break;
        }
    }

    if (!Track) Track = GetSound(MusicCategory);
    if (!Track) return;

    float FinalVol = BaseVol * MusicVolume * MasterVolume;

    if (bFade)
    {
        // 先淡入
        UAudioComponent* Comp = UGameplayStatics::SpawnSound2D(
            GetWorld(), Track, 0.f, 1.f, 0.f, nullptr, bShouldLoop);
        if (Comp)
        {
            // 简单淡入
            float TargetVol = FinalVol;
            FTimerHandle FH;
            float Elapsed = 0.f;
            float FadeTime = 2.f;
            GetWorld()->GetTimerManager().SetTimer(FH, [Comp, TargetVol, &Elapsed, FadeTime]()
            {
                if (!Comp) return;
                Elapsed += 0.1f;
                float Alpha = FMath::Min(Elapsed / FadeTime, 1.f);
                Comp->SetVolumeMultiplier(TargetVol * Alpha);
                if (Alpha >= 1.f)
                {
                    // 停止定时器（通过销毁）
                }
            }, 0.1f, true, 0.f);

            ActiveMusic.Add(MusicCategory, Comp);
            CurrentMusic = MusicCategory;
        }
    }
    else
    {
        UAudioComponent* Comp = UGameplayStatics::SpawnSound2D(
            GetWorld(), Track, FinalVol, 1.f, 0.f, nullptr, bShouldLoop);
        if (Comp)
        {
            ActiveMusic.Add(MusicCategory, Comp);
            CurrentMusic = MusicCategory;
        }
    }
}

void UAudioManager::StopMusic(EAudioCategory MusicCategory, bool bFade)
{
    if (UAudioComponent* const* Found = ActiveMusic.Find(MusicCategory))
    {
        if (*Found && (*Found)->IsPlaying())
        {
            if (bFade)
            {
                FadeOutAndStop(*Found, 2.f);
            }
            else
            {
                (*Found)->Stop();
            }
        }
        ActiveMusic.Remove(MusicCategory);
    }
}

void UAudioManager::SetMusicVolume(float Volume)
{
    MusicVolume = FMath::Clamp(Volume, 0.f, 1.f);

    for (auto& Pair : ActiveMusic)
    {
        if (Pair.Value && Pair.Value->IsPlaying())
        {
            Pair.Value->SetVolumeMultiplier(MusicVolume * MasterVolume);
        }
    }
}

void UAudioManager::CrossfadeMusic(EAudioCategory FromCategory, EAudioCategory ToCategory, float FadeTime)
{
    StopMusic(FromCategory, true);
    PlayMusic(ToCategory, true);
}

void UAudioManager::UpdateMusicDuck()
{
    // 战斗时降低音乐音量（ducking）
    float DuckFactor = 0.4f; // 战斗时音乐降到 40%
    for (auto& Pair : ActiveMusic)
    {
        if (Pair.Value && Pair.Value->IsPlaying())
        {
            Pair.Value->SetVolumeMultiplier(MusicVolume * MasterVolume * DuckFactor);
        }
    }
}

// ======================== 环境音自适应 ========================

void UAudioManager::SetAmbientForBiome(EBiomeType Biome, float FadeTime)
{
    if (CurrentBiome == Biome) return;

    // 停掉旧环境音
    StopAllAmbient(FadeTime);

    // 选新环境音
    EAudioCategory NewAmbient = EAudioCategory::Ambient_Space;
    switch (Biome)
    {
    case EBiomeType::Grassland:
    case EBiomeType::Forest:
        NewAmbient = (Biome == EBiomeType::Forest) ?
            EAudioCategory::Ambient_Planet_Forest :
            EAudioCategory::Ambient_Planet_Grass;
        break;
    case EBiomeType::Desert:
        NewAmbient = EAudioCategory::Ambient_Planet_Desert;
        break;
    case EBiomeType::Tundra:
    case EBiomeType::Snow:
        NewAmbient = EAudioCategory::Ambient_Planet_Tundra;
        break;
    case EBiomeType::Ocean:
    case EBiomeType::Beach:
        NewAmbient = EAudioCategory::Ambient_Space; // 海浪声可在行星处单独处理
        break;
    default:
        NewAmbient = EAudioCategory::Ambient_Space;
        break;
    }

    USoundBase* Sound = GetSound(NewAmbient);
    if (Sound)
    {
        UAudioComponent* Comp = UGameplayStatics::SpawnSound2D(
            GetWorld(), Sound, AmbientVol * MasterVolume, 1.f, 0.f, nullptr, true);
        if (Comp)
        {
            ActiveAmbient.Add(NewAmbient, Comp);
        }
    }

    CurrentBiome = Biome;
}

void UAudioManager::SetAmbientForLocation(const FVector& WorldLocation)
{
    // 查附近行星 → 获取 Biome → 设置环境音
    // 简化：扫描所有行星找最近
    TArray<AActor*> Planets;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AProceduralPlanet::StaticClass(), Planets);

    AProceduralPlanet* Nearest = nullptr;
    float NearestDist = TNumericLimits<float>::Max();
    for (AActor* A : Planets)
    {
        float D = FVector::DistSquared(WorldLocation, A->GetActorLocation());
        if (D < NearestDist)
        {
            NearestDist = D;
            Nearest = Cast<AProceduralPlanet>(A);
        }
    }

    if (Nearest)
    {
        EBiomeType Biome = Nearest->GetBiomeAtWorldPos(WorldLocation);
        SetAmbientForBiome(Biome, 1.f);
    }
    else
    {
        // 深空
        SetAmbientForBiome(EBiomeType::Ocean, 1.f); // Ocean 映射为太空
    }
}

void UAudioManager::StopAllAmbient(float FadeTime)
{
    for (auto& Pair : ActiveAmbient)
    {
        if (Pair.Value && Pair.Value->IsPlaying())
        {
            if (FadeTime > 0.f)
            {
                FadeOutAndStop(Pair.Value, FadeTime);
            }
            else
            {
                Pair.Value->Stop();
            }
        }
    }
    ActiveAmbient.Empty();
}

// ======================== 太空天气音效 ========================

void UAudioManager::StartSolarWindSound(float Intensity)
{
    StopAllWeatherSounds(0.5f);

    EAudioCategory Cat = EAudioCategory::SolarWind;
    USoundBase* Sound = GetSound(Cat);
    if (!Sound) return;

    float Vol = FMath::Clamp(Intensity, 0.f, 3.f) * 0.5f;
    UAudioComponent* Comp = UGameplayStatics::SpawnSound2D(
        GetWorld(), Sound, Vol * SFXVolume * MasterVolume, 1.f, 0.f, nullptr, true);
    if (Comp)
    {
        Comp->bIsLooping = true;
        Comp->Play();
        ActiveWeatherSounds.Add(Cat, Comp);
    }
}

void UAudioManager::StartRadiationStormSound(float Intensity)
{
    EAudioCategory Cat = EAudioCategory::RadiationStorm;
    USoundBase* Sound = GetSound(Cat);
    if (!Sound) return;

    float Vol = FMath::Clamp(Intensity, 0.f, 3.f) * 0.6f;
    UAudioComponent* Comp = UGameplayStatics::SpawnSound2D(
        GetWorld(), Sound, Vol * SFXVolume * MasterVolume, 1.f + Intensity * 0.5f, 0.f, nullptr, true);
    if (Comp)
    {
        Comp->bIsLooping = true;
        Comp->Play();
        ActiveWeatherSounds.Add(Cat, Comp);
    }
}

void UAudioManager::StartEMPSound(float Intensity)
{
    EAudioCategory Cat = EAudioCategory::EMPBurst;
    USoundBase* Sound = GetSound(Cat);
    if (Sound)
    {
        PlaySound2D(Cat, FMath::Clamp(Intensity, 0.f, 2.f));
    }

    // EMP 还有持续的静电噪声
    USoundBase* StaticSound = GetSound(EAudioCategory::SolarWind); // 复用
    if (StaticSound)
    {
        float Vol = Intensity * 0.3f;
        UAudioComponent* Comp = UGameplayStatics::SpawnSound2D(
            GetWorld(), StaticSound, Vol * SFXVolume * MasterVolume, 2.f, 0.f, nullptr, true);
        if (Comp)
        {
            Comp->bIsLooping = true;
            Comp->Play();
            ActiveWeatherSounds.Add(EAudioCategory::EMPBurst, Comp);
        }
    }
}

void UAudioManager::StopAllWeatherSounds(float FadeTime)
{
    for (auto& Pair : ActiveWeatherSounds)
    {
        if (Pair.Value && Pair.Value->IsPlaying())
        {
            if (FadeTime > 0.f)
            {
                FadeOutAndStop(Pair.Value, FadeTime);
            }
            else
            {
                Pair.Value->Stop();
            }
        }
    }
    ActiveWeatherSounds.Empty();
}

// ======================== 音量控制 ========================

void UAudioManager::SetMasterVolume(float Volume)
{
    MasterVolume = FMath::Clamp(Volume, 0.f, 1.f);

    // 更新所有活跃音频
    for (auto& Pair : LoopingSounds)
    {
        if (Pair.Value) Pair.Value->SetVolumeMultiplier(MasterVolume);
    }
    for (auto& Pair : ActiveMusic)
    {
        if (Pair.Value) Pair.Value->SetVolumeMultiplier(MusicVolume * MasterVolume);
    }
    for (auto& Pair : ActiveAmbient)
    {
        if (Pair.Value) Pair.Value->SetVolumeMultiplier(AmbientVol * MasterVolume);
    }
}

void UAudioManager::SetSFXVolume(float Volume)
{
    SFXVolume = FMath::Clamp(Volume, 0.f, 1.f);
}

void UAudioManager::SetMusicVolumeSetting(float Volume)
{
    SetMusicVolume(Volume);
}

void UAudioManager::SetVOiceVolume(float Volume)
{
    VOiceVolume = FMath::Clamp(Volume, 0.f, 1.f);
}

void UAudioManager::SetAmbientVolumeSetting(float Volume)
{
    SetAmbientVolume(Volume);
}

// ======================== 跃迁音频 ========================

void UAudioManager::StartWarpAudio(float ChargeDuration)
{
    bWarpAudioActive = true;
    WarpChargeDuration = ChargeDuration;
    WarpVolume = 0.f;

    // 蓄能音：频率上升
    USoundBase* ChargeSound = GetSound(EAudioCategory::WarpCharge);
    if (ChargeSound)
    {
        WarpChargeComp = UGameplayStatics::SpawnSound2D(
            GetWorld(), ChargeSound, 0.3f * SFXVolume * MasterVolume, 1.f, 0.f, nullptr, false);
        if (WarpChargeComp)
        {
            WarpChargeComp->bIsLooping = true;
            WarpChargeComp->Play();
        }
    }

    // 启动跃迁持续音
    SetWarpVolume(0.5f);
}

void UAudioManager::UpdateWarpAudio(float Progress)
{
    if (!bWarpAudioActive) return;

    // 蓄能阶段：音量渐大 + 音调渐高
    if (Progress < 0.2f)
    {
        float ChargeVol = FMath::Lerp(0.3f, 0.8f, Progress / 0.2f);
        if (WarpChargeComp)
        {
            WarpChargeComp->SetVolumeMultiplier(ChargeVol * SFXVolume * MasterVolume);
            WarpChargeComp->SetPitchMultiplier(FMath::Lerp(0.5f, 2.f, Progress / 0.2f));
        }
    }
    else
    {
        // 巡航阶段：关闭蓄能音，跃迁嗡嗡全开
        if (WarpChargeComp && WarpChargeComp->IsPlaying())
        {
            WarpChargeComp->Stop();
            WarpChargeComp = nullptr;
        }

        float CruiseVol = FMath::Lerp(0.5f, 1.f, (Progress - 0.2f) / 0.8f);
        SetWarpVolume(CruiseVol);
    }
}

void UAudioManager::StopWarpAudio()
{
    bWarpAudioActive = false;

    if (WarpChargeComp && WarpChargeComp->IsPlaying())
    {
        WarpChargeComp->Stop();
        WarpChargeComp = nullptr;
    }

    SetWarpVolume(0.f);

    // 播放到达音
    USoundBase* ArrivalSound = GetSound(EAudioCategory::WarpArrival);
    if (ArrivalSound)
    {
        PlaySound2D(EAudioCategory::WarpArrival, 0.8f);
    }
}

// ======================== 程序化音效生成 ========================

USoundBase* UAudioManager::GenerateEngineHum(float BaseFrequency, float Roughness)
{
    // 优先返回预置资源
    USoundBase* Existing = GetSound(EAudioCategory::EngineHum);
    if (Existing) return Existing;

    // UE5 推荐用 MetaSound 做程序化
    // 这里创建提示日志，实际应在编辑器中创建 MetaSound Source
    UE_LOG(LogTemp, Warning,
        TEXT("[Audio] GenerateEngineHum: Create a MetaSound asset with params: Freq=%.0f, Rough=%.2f. "
             "See MetaSound documentation for runtime sound synthesis."),
        BaseFrequency, Roughness);

    return nullptr;
}

USoundBase* UAudioManager::GenerateWarpSound(float BaseFrequency, float SweepRate)
{
    USoundBase* Existing = GetSound(EAudioCategory::WarpTravel);
    if (Existing) return Existing;

    UE_LOG(LogTemp, Warning,
        TEXT("[Audio] GenerateWarpSound: Create a MetaSound asset with frequency sweep: "
             "Start=%.0f, SweepRate=%.1f. Use MetaSound 'Pitch Shift' node."),
        BaseFrequency, SweepRate);

    return nullptr;
}

USoundBase* UAudioManager::GenerateWeaponSound(EAudioCategory WeaponType, float Intensity)
{
    USoundBase* Sound = GetSound(WeaponType);
    if (!Sound) Sound = GetSound(EAudioCategory::WeaponFire_Laser);
    if (!Sound)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Audio] GenerateWeaponSound: Type=%d Intensity=%.2f — create MetaSound with ADSR"),
            (int32)WeaponType, Intensity);
    }
    return Sound;
}

USoundBase* UAudioManager::GenerateExplosionSound(float Size)
{
    USoundBase* Sound = GetSound(EAudioCategory::Explosion_Large);
    if (!Sound) Sound = GetSound(EAudioCategory::Explosion_Small);
    if (!Sound)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Audio] GenerateExplosionSound: Size=%.1f — use MetaSound with noise burst + lowpass sweep"),
            Size);
    }
    return Sound;
}

USoundBase* UAudioManager::GenerateUIBeep(float Frequency, float Duration)
{
    USoundBase* Sound = GetSound(EAudioCategory::UI_Click);
    if (!Sound)
    {
        UE_LOG(LogTemp, Log,
            TEXT("[Audio] UI Beep: Freq=%.0f Dur=%.2f — create MetaSound oscillator"),
            Frequency, Duration);
    }
    return Sound;
}

USoundBase* UAudioManager::GenerateFootstepSound(const FString& SurfaceType, float Intensity)
{
    EAudioCategory Cat = EAudioCategory::Footstep_Default;
    if (SurfaceType == TEXT("Metal")) Cat = EAudioCategory::Footstep_Metal;
    else if (SurfaceType == TEXT("Grass")) Cat = EAudioCategory::Footstep_Grass;
    else if (SurfaceType == TEXT("Snow")) Cat = EAudioCategory::Footstep_Snow;

    USoundBase* Sound = GetSound(Cat);
    if (!Sound) Sound = GetSound(EAudioCategory::Footstep_Default);
    return Sound;
}

USoundBase* UAudioManager::GenerateWindSound(float WindSpeed)
{
    USoundBase* Sound = GetSound(EAudioCategory::Wind_Howl);
    if (!Sound)
    {
        UE_LOG(LogTemp, Log,
            TEXT("[Audio] Wind: Speed=%.1f — create MetaSound with filtered noise + LFO modulation"),
            WindSpeed);
    }
    return Sound;
}

USoundBase* UAudioManager::GenerateHeartbeatSound(float HeartRate)
{
    USoundBase* Sound = GetSound(EAudioCategory::Heartbeat);
    if (!Sound)
    {
        UE_LOG(LogTemp, Log,
            TEXT("[Audio] Heartbeat: BPM=%.0f — create MetaSound with two-thump pattern at interval %.2fs"),
            HeartRate, 60.f / HeartRate);
    }
    return Sound;
}

USoundBase* UAudioManager::CreateProceduralSound(const FProceduralSoundParams& Params, const FString& SoundName)
{
    UE_LOG(LogTemp, Warning,
        TEXT("[Audio] CreateProceduralSound '%s': Freq=%.0f→%.0f, Dur=%.2f, Rough=%.2f — "
             "UE5 MetaSound is the recommended approach for runtime synthesis."),
        *SoundName, Params.BaseFrequency, Params.FrequencyEnd,
        Params.Duration, Params.Roughness);

    return nullptr;
}
