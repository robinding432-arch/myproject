// MobileNetworkAdapter.cpp
// v7.2 — Mobile network detection and adaptation

#include "Mobile/MobileNetworkAdapter.h"
#include "Engine/Engine.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineConnectionInterface.h"
#include "TimerManager.h"
#include "HAL/PlatformTime.h"

void UMobileNetworkAdapter::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Default settings
    Settings.NetworkTickRate = 15;
    Settings.NonCriticalReplicationRate = 5;
    Settings.bEnableCompression = true;
    Settings.bBatchPackets = true;
    Settings.MaxBatchSize = 1024;
    Settings.BackgroundKeepAlive = 30.f;
    Settings.BackgroundTimeout = 60.f;
    Settings.MaxReconnectAttempts = 5;
    Settings.ReconnectDelay = 3.f;
    Settings.bDataSaverMode = false;
    Settings.DataSaverKbPerMin = 100;
    Settings.bDisableVoiceInDataSaver = true;
    Settings.bSkipCosmeticReplication = true;

    DetectNetworkType();
    AssessQuality();
    ApplySettingsToEngine();

    // Listen for reachability changes
    if (GEngine)
    {
        // Bind delegate (best effort)
    }
}

void UMobileNetworkAdapter::Deinitialize()
{
    StopBackgroundTimer();
    Super::Deinitialize();
}

UMobileNetworkAdapter* UMobileNetworkAdapter::Get(const UObject* WorldContextObject)
{
    if (!WorldContextObject) return nullptr;
    UWorld* World = WorldContextObject->GetWorld();
    if (!World) return nullptr;
    UGameInstance* GI = World->GetGameInstance();
    if (!GI) return nullptr;
    return GI->GetSubsystem<UMobileNetworkAdapter>();
}

void UMobileNetworkAdapter::DetectNetworkType()
{
#if PLATFORM_ANDROID
    // On Android, query ConnectivityManager via JNI
    // For now, use console variable or default
    NetType = ENetworkType::WiFi; // Default assumption

    // Could check:
    // - ConnectivityManager.getActiveNetworkInfo().getType()
    // - TelephonyManager.getNetworkType() for cellular
    // - WifiManager.getConnectionInfo().getLinkSpeed() for WiFi speed

#elif PLATFORM_IOS
    // On iOS, use SystemConfiguration/CaptiveNetwork
    // or Network framework (iOS 12+)
    NetType = ENetworkType::WiFi;
#else
    // Desktop testing
    NetType = ENetworkType::Ethernet;
#endif

    OnNetworkTypeChanged.Broadcast(NetType);
}

void UMobileNetworkAdapter::UpdateBandwidth()
{
    // Estimate from engine stats
    // In production: read from NetDriver stats
    float DeltaTime = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
    BandwidthTimer += DeltaTime;

    if (BandwidthTimer >= 1.f)
    {
        BandwidthTimer = 0.f;
        // Rough estimate: assume 10KB per tick at 20Hz = 200KB/s baseline
        // Adjusted by tick rate
        float TickRate = FMath::Max(Settings.NetworkTickRate, 1);
        CurrentBandwidth = (TickRate * 10.f); // KB/s rough estimate

        // Data saver caps
        if (Settings.bDataSaverMode)
        {
            float MaxKBps = Settings.DataSaverKbPerMin / 60.f;
            CurrentBandwidth = FMath::Min(CurrentBandwidth, MaxKBps);
        }
    }
}

void UMobileNetworkAdapter::AssessQuality()
{
    ENetworkQuality OldQuality = Quality;

    if (NetType == ENetworkType::NoConnection)
    {
        Quality = ENetworkQuality::Unusable;
    }
    else if (NetType == ENetworkType::Cellular3G)
    {
        Quality = ENetworkQuality::Poor;
    }
    else if (NetType == ENetworkType::Cellular4G)
    {
        Quality = CurrentBandwidth > 500.f ? ENetworkQuality::Good : ENetworkQuality::Fair;
    }
    else if (NetType == ENetworkType::Cellular5G)
    {
        Quality = ENetworkQuality::Excellent;
    }
    else if (NetType == ENetworkType::WiFi)
    {
        Quality = CurrentBandwidth > 1000.f ? ENetworkQuality::Excellent : ENetworkQuality::Good;
    }
    else
    {
        Quality = ENetworkQuality::Good;
    }

    if (OldQuality != Quality)
    {
        OnQualityChanged.Broadcast(Quality);
        ApplySettingsToEngine();
    }
}

void UMobileNetworkAdapter::ApplySettingsToEngine()
{
    // Set network tick rate via console variable
    IConsoleVariable* NetTickVar = IConsoleManager::Get().FindConsoleVariable(TEXT("net.TickRate"));
    if (NetTickVar)
    {
        int32 TickRate = Settings.NetworkTickRate;
        // Reduce in poor quality
        if (Quality == ENetworkQuality::Poor) TickRate = FMath::Max(5, TickRate / 3);
        else if (Quality == ENetworkQuality::Fair) TickRate = FMath::Max(8, TickRate / 2);
        NetTickVar->Set(TickRate);
    }

    // Compression
    IConsoleVariable* CompressVar = IConsoleManager::Get().FindConsoleVariable(TEXT("net.UseCompression"));
    if (CompressVar) CompressVar->Set(Settings.bEnableCompression ? 1 : 0);

    // Data saver: skip cosmetic replication
    IConsoleVariable* CosmeticVar = IConsoleManager::Get().FindConsoleVariable(TEXT("net.SkipCosmeticReplication"));
    if (CosmeticVar) CosmeticVar->Set(Settings.bSkipCosmeticReplication || Settings.bDataSaverMode ? 1 : 0);
}

void UMobileNetworkAdapter::SetSettings(const FMobileNetworkSettings& NewSettings)
{
    Settings = NewSettings;
    ApplySettingsToEngine();
}

void UMobileNetworkAdapter::SetDataSaverMode(bool bEnabled)
{
    if (Settings.bDataSaverMode == bEnabled) return;
    Settings.bDataSaverMode = bEnabled;
    ApplySettingsToEngine();
    OnDataSaverChanged.Broadcast(bEnabled);
}

void UMobileNetworkAdapter::OnAppBackgrounded()
{
    bIsBackgrounded = true;
    StartBackgroundTimer();
}

void UMobileNetworkAdapter::OnAppForegrounded()
{
    bIsBackgrounded = false;
    StopBackgroundTimer();

    // Check if we need to reconnect
    if (ReconnectAttempts > 0 || !bConnectionHealthy)
    {
        Reconnect();
    }
}

void UMobileNetworkAdapter::StartBackgroundTimer()
{
    if (!GetWorld()) return;
    GetWorld()->GetTimerManager().ClearTimer(BackgroundTimerHandle);
    GetWorld()->GetTimerManager().SetTimer(BackgroundTimerHandle, this, &UMobileNetworkAdapter::OnBackgroundTimeout, Settings.BackgroundTimeout, false);
}

void UMobileNetworkAdapter::StopBackgroundTimer()
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(BackgroundTimerHandle);
}

void UMobileNetworkAdapter::OnBackgroundTimeout()
{
    // Connection timed out while backgrounded
    bConnectionHealthy = false;
    OnConnectionLost.Broadcast();
}

void UMobileNetworkAdapter::Reconnect()
{
    if (ReconnectAttempts >= Settings.MaxReconnectAttempts)
    {
        bConnectionHealthy = false;
        return;
    }

    ReconnectAttempts++;
    bConnectionHealthy = false;

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
        {
            // Attempt reconnect via engine
            // GEngine->GameViewport->Viewport->...
            // Or via OnlineSubsystem

            // For now, simulate: 70% success
            bool bSuccess = (FMath::RandRange(0, 100) < 70);
            if (bSuccess)
            {
                bConnectionHealthy = true;
                ReconnectAttempts = 0;
                OnReconnected.Broadcast();
            }
            else
            {
                // Retry after delay
                if (GetWorld())
                {
                    GetWorld()->GetTimerManager().SetTimer(ReconnectTimerHandle, this, &UMobileNetworkAdapter::Reconnect, Settings.ReconnectDelay, false);
                }
            }
        });
    }
}

void UMobileNetworkAdapter::OnNetworkReachabilityChanged(ENetworkReachability Reachability)
{
    switch (Reachability)
    {
    case ENetworkReachability::NotReachable:
        NetType = ENetworkType::NoConnection;
        bConnectionHealthy = false;
        OnConnectionLost.Broadcast();
        break;
    case ENetworkReachability::ReachableViaWiFi:
        NetType = ENetworkType::WiFi;
        if (!bConnectionHealthy) Reconnect();
        break;
    case ENetworkReachability::ReachableViaWWAN:
        NetType = ENetworkType::Cellular4G; // Assume 4G
        if (!bConnectionHealthy) Reconnect();
        break;
    }
    OnNetworkTypeChanged.Broadcast(NetType);
    AssessQuality();
}
