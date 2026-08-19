// MobileNetworkAdapter.h
// v7.2 — Mobile-specific network optimizations

#pragma once

#include "CoreMinimal.h"
#include "MobileNetworkAdapter.generated.h"

/** Network connection type */
UENUM(BlueprintType)
enum class ENetworkType : uint8
{
    Unknown     UMETA(DisplayName = "Unknown"),
    WiFi        UMETA(DisplayName = "WiFi"),
    Cellular4G  UMETA(DisplayName = "4G LTE"),
    Cellular5G  UMETA(DisplayName = "5G"),
    Cellular3G  UMETA(DisplayName = "3G (Slow)"),
    Ethernet    UMETA(DisplayName = "Ethernet"),
    NoConnection UMETA(DisplayName = "No Connection"),
};

/** Network quality assessment */
UENUM(BlueprintType)
enum class ENetworkQuality : uint8
{
    Excellent   UMETA(DisplayName = "Excellent (>50Mbps)"),
    Good        UMETA(DisplayName = "Good (10-50Mbps)"),
    Fair        UMETA(DisplayName = "Fair (2-10Mbps)"),
    Poor        UMETA(DisplayName = "Poor (<2Mbps)"),
    Unusable    UMETA(DisplayName = "Unusable"),
};

/** Mobile network settings */
USTRUCT(BlueprintType)
struct FMobileNetworkSettings
{
    GENERATED_BODY()

    /** Network tick rate (Hz) — lower = less battery */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 NetworkTickRate = 15;

    /** Replication rate for non-critical actors (Hz) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 NonCriticalReplicationRate = 5;

    /** Enable data compression (LZ4) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableCompression = true;

    /** Aggressive packet batching */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bBatchPackets = true;

    /** Max batch size (bytes) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxBatchSize = 1024;

    /** Background connection keep-alive (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BackgroundKeepAlive = 30.f;

    /** Background timeout before disconnect (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BackgroundTimeout = 60.f;

    /** Auto-reconnect attempts */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxReconnectAttempts = 5;

    /** Reconnect delay (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ReconnectDelay = 3.f;

    /** Data saver mode (ultra-low bandwidth) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bDataSaverMode = false;

    /** Data saver: max KB per minute */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 DataSaverKbPerMin = 100;

    /** Skip voice chat in data saver */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bDisableVoiceInDataSaver = true;

    /** Skip non-essential replication */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bSkipCosmeticReplication = true;
};

/**
 * UMobileNetworkAdapter — monitors network and adapts
 * Auto-detects WiFi/4G/5G, adjusts tick rate, handles
 * backgrounding, reconnects, and data saver mode.
 */
UCLASS(BlueprintType)
class STELLARSYSTEM_API UMobileNetworkAdapter : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Get singleton */
    UFUNCTION(BlueprintPure, Category = "Mobile Net", meta = (WorldContext = "WorldContextObject"))
    static UMobileNetworkAdapter* Get(const UObject* WorldContextObject);

    /** Get current network type */
    UFUNCTION(BlueprintPure, Category = "Mobile Net")
    ENetworkType GetNetworkType() const { return NetType; }

    /** Get current quality */
    UFUNCTION(BlueprintPure, Category = "Mobile Net")
    ENetworkQuality GetQuality() const { return Quality; }

    /** Get current settings */
    UFUNCTION(BlueprintPure, Category = "Mobile Net")
    FMobileNetworkSettings GetSettings() const { return Settings; }

    /** Override settings */
    UFUNCTION(BlueprintCallable, Category = "Mobile Net")
    void SetSettings(const FMobileNetworkSettings& NewSettings);

    /** Enable/disable data saver */
    UFUNCTION(BlueprintCallable, Category = "Mobile Net")
    void SetDataSaverMode(bool bEnabled);

    /** Is data saver active */
    UFUNCTION(BlueprintPure, Category = "Mobile Net")
    bool IsDataSaverActive() const { return Settings.bDataSaverMode; }

    /** App going to background */
    UFUNCTION(BlueprintCallable, Category = "Mobile Net")
    void OnAppBackgrounded();

    /** App returning to foreground */
    UFUNCTION(BlueprintCallable, Category = "Mobile Net")
    void OnAppForegrounded();

    /** Force reconnect */
    UFUNCTION(BlueprintCallable, Category = "Mobile Net")
    void Reconnect();

    /** Get bytes sent since start */
    UFUNCTION(BlueprintPure, Category = "Mobile Net")
    int64 GetBytesSent() const { return BytesSent; }

    /** Get bytes received since start */
    UFUNCTION(BlueprintPure, Category = "Mobile Net")
    int64 GetBytesReceived() const { return BytesReceived; }

    /** Get current bandwidth estimate (KB/s) */
    UFUNCTION(BlueprintPure, Category = "Mobile Net")
    float GetCurrentBandwidthKBps() const { return CurrentBandwidth; }

    /** Is connection healthy */
    UFUNCTION(BlueprintPure, Category = "Mobile Net")
    bool IsConnectionHealthy() const { return bConnectionHealthy; }

    /** Get reconnect attempt count */
    UFUNCTION(BlueprintPure, Category = "Mobile Net")
    int32 GetReconnectAttempts() const { return ReconnectAttempts; }

    /** Event: network type changed */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNetworkTypeChanged, ENetworkType, NewType);
    UPROPERTY(BlueprintAssignable, Category = "Mobile Net")
    FOnNetworkTypeChanged OnNetworkTypeChanged;

    /** Event: quality changed */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQualityChanged, ENetworkQuality, NewQuality);
    UPROPERTY(BlueprintAssignable, Category = "Mobile Net")
    FOnQualityChanged OnQualityChanged;

    /** Event: connection lost */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnConnectionLost);
    UPROPERTY(BlueprintAssignable, Category = "Mobile Net")
    FOnConnectionLost OnConnectionLost;

    /** Event: reconnected */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnReconnected);
    UPROPERTY(BlueprintAssignable, Category = "Mobile Net")
    FOnReconnected OnReconnected;

    /** Event: data saver toggled */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDataSaverChanged, bool, bEnabled);
    UPROPERTY(BlueprintAssignable, Category = "Mobile Net")
    FOnDataSaverChanged OnDataSaverChanged;

private:
    void DetectNetworkType();
    void UpdateBandwidth();
    void AssessQuality();
    void ApplySettingsToEngine();
    void StartBackgroundTimer();
    void StopBackgroundTimer();
    void OnBackgroundTimeout();
    void AttemptReconnect();
    void OnNetworkReachabilityChanged(ENetworkReachability Reachability);

    ENetworkType NetType = ENetworkType::Unknown;
    ENetworkQuality Quality = ENetworkQuality::Good;
    FMobileNetworkSettings Settings;

    /** Bandwidth tracking */
    float CurrentBandwidth = 0.f;
    int64 BytesSent = 0;
    int64 BytesReceived = 0;
    float BandwidthTimer = 0.f;

    /** Background state */
    bool bIsBackgrounded = false;
    FTimerHandle BackgroundTimerHandle;
    FTimerHandle ReconnectTimerHandle;
    int32 ReconnectAttempts = 0;
    bool bConnectionHealthy = true;

    /** Data saver accumulator */
    float DataSaverAccumulator = 0.f;
    int32 DataSaverBytesThisMinute = 0;
};
