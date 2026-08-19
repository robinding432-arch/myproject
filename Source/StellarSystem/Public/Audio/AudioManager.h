// ============================================================
// AudioManager.h — 增强版
// 路径: Source/StellarSystem/Public/Audio/AudioManager.h
// 功能: 完整游戏音频管理（引擎/跃迁/武器/环境/UI/音乐/太空天气）
// ============================================================

#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundCue.h"
#include "Components/AudioComponent.h"
#include "AudioManager.generated.h"

// —— 音频分类 ——
UENUM(BlueprintType)
enum class EAudioCategory : uint8
{
    // 飞船
    EngineHum       UMETA(DisplayName = "Engine Hum (Looping)"),
    EngineThrust    UMETA(DisplayName = "Engine Thrust"),
    EngineOverload  UMETA(DisplayName = "Engine Overload Warning"),
    WarpCharge      UMETA(DisplayName = "Warp Charging"),
    WarpTravel      UMETA(DisplayName = "Warp Travel (Looping)"),
    WarpArrival     UMETA(DisplayName = "Warp Arrival"),
    ShieldHum       UMETA(DisplayName = "Shield Hum (Looping)"),
    ShieldHit       UMETA(DisplayName = "Shield Hit"),
    HullHit         UMETA(DisplayName = "Hull Hit"),
    HullAlarm       UMETA(DisplayName = "Hull Critical Alarm"),

    // 武器
    WeaponFire_Laser    UMETA(DisplayName = "Weapon: Laser"),
    WeaponFire_Plasma   UMETA(DisplayName = "Weapon: Plasma"),
    WeaponFire_Railgun  UMETA(DisplayName = "Weapon: Railgun"),
    WeaponFire_Missile  UMETA(DisplayName = "Weapon: Missile Launch"),
    WeaponFire_Beam     UMETA(DisplayName = "Weapon: Beam"),
    WeaponReload    UMETA(DisplayName = "Weapon Reload"),
    WeaponEmpty     UMETA(DisplayName = "Weapon Empty Click"),
    LockOn_Acquire  UMETA(DisplayName = "Target Lock Acquired"),
    LockOn_Lost     UMETA(DisplayName = "Target Lock Lost"),

    // 环境
    Ambient_Space       UMETA(DisplayName = "Ambient: Deep Space"),
    Ambient_Planet_Grass UMETA(DisplayName = "Ambient: Grassland"),
    Ambient_Planet_Desert UMETA(DisplayName = "Ambient: Desert"),
    Ambient_Planet_Forest UMETA(DisplayName = "Ambient: Forest"),
    Ambient_Planet_Tundra UMETA(DisplayName = "Ambient: Tundra"),
    Ambient_Planet_Snow   UMETA(DisplayName = "Ambient: Snow"),
    Ambient_Station       UMETA(DisplayName = "Ambient: Space Station"),
    Ambient_Nebula       UMETA(DisplayName = "Ambient: Nebula"),
    Wind_Howl             UMETA(DisplayName = "Wind (Exposed Surface)"),

    // 太空天气
    SolarWind      UMETA(DisplayName = "Solar Wind"),
    RadiationStorm UMETA(DisplayName = "Radiation Storm"),
    EMPBurst      UMETA(DisplayName = "EMP Burst"),
    AuroraHum     UMETA(DisplayName = "Aurora Resonance"),

    // 角色
    Footstep_Default UMETA(DisplayName = "Footstep: Default"),
    Footstep_Metal   UMETA(DisplayName = "Footstep: Metal"),
    Footstep_Grass   UMETA(DisplayName = "Footstep: Grass"),
    Footstep_Snow    UMETA(DisplayName = "Footstep: Snow"),
    Jump             UMETA(DisplayName = "Jump"),
    Landing          UMETA(DisplayName = "Landing"),
    Breathing        UMETA(DisplayName = "Breathing (Looping)"),
    Heartbeat        UMETA(DisplayName = "Heartbeat (Looping)"),
    Sprint_Grunt     UMETA(DisplayName = "Sprint Exhaustion"),

    // UI
    UI_Click        UMETA(DisplayName = "UI: Click"),
    UI_Hover        UMETA(DisplayName = "UI: Hover"),
    UI_Open         UMETA(DisplayName = "UI: Open Menu"),
    UI_Close        UMETA(DisplayName = "UI: Close Menu"),
    UI_Error        UMETA(DisplayName = "UI: Error Beep"),
    UI_Success      UMETA(DisplayName = "UI: Success Chime"),

    // 系统
    Explosion_Large UMETA(DisplayName = "Explosion: Large"),
    Explosion_Small UMETA(DisplayName = "Explosion: Small"),
    Pickup          UMETA(DisplayName = "Item Pickup"),
    Drop            UMETA(DisplayName = "Item Drop"),
    Consume         UMETA(DisplayName = "Consumable Use"),
    LevelUp         UMETA(DisplayName = "Level Up"),
    QuestComplete   UMETA(DisplayName = "Quest Complete"),
    QuestFail      UMETA(DisplayName = "Quest Failed"),
    Achievement    UMETA(DisplayName = "Achievement Unlocked"),
    Death           UMETA(DisplayName = "Death"),
    Respawn         UMETA(DisplayName = "Respawn"),

    // 交互
    Door_Open       UMETA(DisplayName = "Door Open"),
    Door_Close      UMETA(DisplayName = "Door Close"),
    Terminal_Use    UMETA(DisplayName = "Terminal Use"),
    Vendor_Trade    UMETA(DisplayName = "Vendor Trade"),

    // 音乐
    Music_Menu      UMETA(DisplayName = "Music: Main Menu"),
    Music_Combat    UMETA(DisplayName = "Music: Combat"),
    Music_Exploration UMETA(DisplayName = "Music: Exploration"),
    Music_Warp      UMETA(DisplayName = "Music: Warp Travel"),
    Music_Boss      UMETA(DisplayName = "Music: Boss Battle"),
    Music_Victory   UMETA(DisplayName = "Music: Victory")
};

// —— 音效参数（用于程序化生成）——
USTRUCT(BlueprintType)
struct FProceduralSoundParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BaseFrequency = 440.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FrequencyEnd = 440.f; // 扫频终点

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Duration = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Amplitude = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Roughness = 0.3f; // 0=纯音 1=噪声

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Resonance = 0.5f; // 共振强度

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AttackTime = 0.05f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DecayTime = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FilterCutoff = 5000.f;
};

// —— 音乐轨道配置 ——
USTRUCT(BlueprintType)
struct FMusicTrack
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EAudioCategory Category = EAudioCategory::Music_Exploration;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    USoundBase* Track = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FadeInTime = 2.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FadeOutTime = 2.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BaseVolume = 0.7f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bLoop = true;
};

// —— 3D 音频衰减设置 ——
USTRUCT(BlueprintType)
struct FAttenuationSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MinDistance = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxDistance = 100000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FalloffExponent = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LPFFreqAtMax = 2000.f; // 远处低通滤波

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableOcclusion = true;
};

// ============================================================
// UAudioManager — GameInstanceSubsystem
// ============================================================
UCLASS()
class UAudioManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ==================== 播放接口 ====================

    // 3D 世界空间音效
    UFUNCTION(BlueprintCallable, Category = "Audio|Play")
    UAudioComponent* PlaySoundAtLocation(EAudioCategory Category,
        const FVector& Location, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f);

    // 附加到 SceneComponent
    UFUNCTION(BlueprintCallable, Category = "Audio|Play")
    UAudioComponent* PlaySoundAttached(EAudioCategory Category,
        USceneComponent* AttachTo, float VolumeMultiplier = 1.f);

    // 2D（UI/音乐/全屏）
    UFUNCTION(BlueprintCallable, Category = "Audio|Play")
    UAudioComponent* PlaySound2D(EAudioCategory Category, float VolumeMultiplier = 1.f);

    // 程序化音效（无需预置资源）
    UFUNCTION(BlueprintCallable, Category = "Audio|Procedural")
    USoundBase* CreateProceduralSound(const FProceduralSoundParams& Params,
        const FString& SoundName = TEXT("ProceduralSound"));

    // ==================== 持续音效控制 ====================

    UFUNCTION(BlueprintCallable, Category = "Audio|Looping")
    void SetEngineHumVolume(float Volume);

    UFUNCTION(BlueprintCallable, Category = "Audio|Looping")
    void SetWarpVolume(float Volume);

    UFUNCTION(BlueprintCallable, Category = "Audio|Looping")
    void SetShieldHumVolume(float Volume);

    UFUNCTION(BlueprintCallable, Category = "Audio|Looping")
    void SetAmbientVolume(float Volume);

    UFUNCTION(BlueprintCallable, Category = "Audio|Looping")
    void StopAllLoopingSounds();

    UFUNCTION(BlueprintCallable, Category = "Audio|Looping")
    void StopLoopingSound(EAudioCategory Category);

    // ==================== 音乐控制 ====================

    UFUNCTION(BlueprintCallable, Category = "Audio|Music")
    void PlayMusic(EAudioCategory MusicCategory, bool bFade = true);

    UFUNCTION(BlueprintCallable, Category = "Audio|Music")
    void StopMusic(EAudioCategory MusicCategory, bool bFade = true);

    UFUNCTION(BlueprintCallable, Category = "Audio|Music")
    void SetMusicVolume(float Volume);

    UFUNCTION(BlueprintCallable, Category = "Audio|Music")
    void CrossfadeMusic(EAudioCategory FromCategory, EAudioCategory ToCategory,
        float FadeTime = 2.f);

    // ==================== 环境音自适应 ====================

    UFUNCTION(BlueprintCallable, Category = "Audio|Ambient")
    void SetAmbientForBiome(EBiomeType Biome, float FadeTime = 1.f);

    UFUNCTION(BlueprintCallable, Category = "Audio|Ambient")
    void SetAmbientForLocation(const FVector& WorldLocation);

    UFUNCTION(BlueprintCallable, Category = "Audio|Ambient")
    void StopAllAmbient(float FadeTime = 1.f);

    // ==================== 太空天气音效 ====================

    UFUNCTION(BlueprintCallable, Category = "Audio|Weather")
    void StartSolarWindSound(float Intensity = 1.f);

    UFUNCTION(BlueprintCallable, Category = "Audio|Weather")
    void StartRadiationStormSound(float Intensity = 1.f);

    UFUNCTION(BlueprintCallable, Category = "Audio|Weather")
    void StartEMPSound(float Intensity = 1.f);

    UFUNCTION(BlueprintCallable, Category = "Audio|Weather")
    void StopAllWeatherSounds(float FadeTime = 1.f);

    // ==================== 音量控制 ====================

    UFUNCTION(BlueprintCallable, Category = "Audio|Settings")
    void SetMasterVolume(float Volume);

    UFUNCTION(BlueprintCallable, Category = "Audio|Settings")
    void SetSFXVolume(float Volume);

    UFUNCTION(BlueprintCallable, Category = "Audio|Settings")
    void SetMusicVolumeSetting(float Volume);

    UFUNCTION(BlueprintCallable, Category = "Audio|Settings")
    void SetVOiceVolume(float Volume);

    UFUNCTION(BlueprintCallable, Category = "Audio|Settings")
    void SetAmbientVolumeSetting(float Volume);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Audio|Settings")
    float GetMasterVolume() const { return MasterVolume; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Audio|Settings")
    float GetSFXVolume() const { return SFXVolume; }

    // ==================== 配置 ====================

    // 音效资产映射（编辑器里填）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Assets")
    TMap<EAudioCategory, USoundBase*> SoundMap;

    // 音乐轨道配置
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Music")
    TArray<FMusicTrack> MusicTracks;

    // 衰减设置
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Spatial")
    FAttenuationSettings DefaultAttenuation;

    // 引擎音效参数（程序化）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Procedural")
    FProceduralSoundParams EngineHumParams;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Procedural")
    FProceduralSoundParams WarpSoundParams;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Procedural")
    FProceduralSoundParams WeaponSoundParams;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Procedural")
    FProceduralSoundParams ExplosionSoundParams;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Procedural")
    FProceduralSoundParams UIBeepParams;

    // 音量设置
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Settings")
    float MasterVolume = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Settings")
    float MusicVolume = 0.7f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Settings")
    float SFXVolume = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Settings")
    float VOiceVolume = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Settings")
    float AmbientVolume = 0.5f;

    // ==================== 跃迁音频接口 ====================

    UFUNCTION(BlueprintCallable, Category = "Audio|Warp")
    void StartWarpAudio(float ChargeDuration = 1.5f);

    UFUNCTION(BlueprintCallable, Category = "Audio|Warp")
    void UpdateWarpAudio(float Progress);

    UFUNCTION(BlueprintCallable, Category = "Audio|Warp")
    void StopWarpAudio();

    // ==================== 程序化生成 ====================

    UFUNCTION(BlueprintCallable, Category = "Audio|Procedural")
    USoundBase* GenerateEngineHum(float BaseFrequency = 80.f, float Roughness = 0.3f);

    UFUNCTION(BlueprintCallable, Category = "Audio|Procedural")
    USoundBase* GenerateWarpSound(float BaseFrequency = 200.f, float SweepRate = 2.f);

    UFUNCTION(BlueprintCallable, Category = "Audio|Procedural")
    USoundBase* GenerateWeaponSound(EAudioCategory WeaponType, float Intensity = 1.f);

    UFUNCTION(BlueprintCallable, Category = "Audio|Procedural")
    USoundBase* GenerateExplosionSound(float Size = 1.f);

    UFUNCTION(BlueprintCallable, Category = "Audio|Procedural")
    USoundBase* GenerateUIBeep(float Frequency = 1000.f, float Duration = 0.05f);

    UFUNCTION(BlueprintCallable, Category = "Audio|Procedural")
    USoundBase* GenerateFootstepSound(const FString& SurfaceType, float Intensity = 1.f);

    UFUNCTION(BlueprintCallable, Category = "Audio|Procedural")
    USoundBase* GenerateWindSound(float WindSpeed = 10.f);

    UFUNCTION(BlueprintCallable, Category = "Audio|Procedural")
    USoundBase* GenerateHeartbeatSound(float HeartRate = 70.f);

    // ==================== 内部 ====================

private:
    // 持续音效
    UPROPERTY()
    TMap<EAudioCategory, UAudioComponent*> LoopingSounds;

    // 当前音乐
    UPROPERTY()
    TMap<EAudioCategory, UAudioComponent*> ActiveMusic;

    // 当前环境音
    UPROPERTY()
    TMap<EAudioCategory, UAudioComponent*> ActiveAmbient;

    // 当前天气音
    UPROPERTY()
    TMap<EAudioCategory, UAudioComponent*> ActiveWeatherSounds;

    // 缓存
    float EngineHumVolume = 0.f;
    float WarpVolume = 0.f;
    float ShieldHumVolume = 0.f;
    float AmbientVol = 0.5f;
    EAudioCategory CurrentMusic = EAudioCategory::Music_Exploration;
    EBiomeType CurrentBiome = EBiomeType::Ocean;

    // 内部方法
    USoundBase* GetSound(EAudioCategory Category) const;
    float GetFinalVolume(float CategoryVolume) const;
    void ApplyVolumeToLooping(EAudioCategory Category, float Volume);
    void FadeOutAndStop(UAudioComponent* Comp, float FadeTime);
    void UpdateMusicDuck();

    // 跃迁状态
    bool bWarpAudioActive = false;
    float WarpChargeDuration = 1.5f;
    UAudioComponent* WarpChargeComp = nullptr;
    UAudioComponent* WarpTravelComp = nullptr;
};
