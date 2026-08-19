// StellarSystem.Build.cs
// v7.6.1 — Added TractorBeam + StationDefenseTurret modules

using UnrealBuildTool;

public class StellarSystem : ModuleRules
{
    public StellarSystem(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            // ── Core ──
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",

            // ── Input ──
            "EnhancedInput",

            // ── Rendering (PC/Console/Editor only) ──
            "RenderCore",
            "Renderer",
            "RHI",
            "ShaderCore",
            "ComputeFramework",

            // ── Procedural Mesh ──
            "ProceduralMeshComponent",

            // ── Networking ──
            "Networking",
            "Sockets",
            "OnlineSubsystem",
            "OnlineSubsystemUtils",
            "PacketHandlers",
            "ReplicationGraph",
            "Iris",
            "NetCore",
            "NetcodeUnitTest",

            // ── Physics ──
            "PhysicsCore",
            "Chaos",
            "ChaosVehicles",

            // ── Audio ──
            "AudioMixer",
            "EngineSettings",

            // ── UI ──
            "UMG",
            "Slate",
            "SlateCore",
            "InputCore",
            "HeadMountedDisplay",

            // ── Particles ──
            "Niagara",
            "NiagaraCore",

            // ── Serialization / Data ──
            "Json",
            "JsonUtilities",
            "Serialization",
            "Compression",

            // ── Gameplay ──
            "GameplayTags",
            "GameplayTasks",
            "AIModule",
            "NavigationSystem",
            "BehaviorTree",

            // ── Stats / Profiling ──
            "Stats",
            "Profiler",

            // ── Cinematic ──
            "CinematicCamera",
            "LevelSequence",
            "MovieScene",

            // ── Projects / Config ──
            "Projects",
            "Config",

            // ── Web / HTTP ──
            "HTTP",
            "WebSockets",
            "InternetBrowser",

            // ── Platform ──
            "ApplicationCore",
            "HeadMountedDisplay",

            // ── Threading ──
            "Async",
            "Threading",
        });

        // ── Conditional modules ──
        if (Target.bBuildEditor)
        {
            PublicDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd",
                "EditorStyle",
                "PropertyEditor",
                "Kismet",
                "BlueprintGraph",
            });
        }

        // ── Steam (PC only) ──
        if (Target.Platform == UnrealTargetPlatform.Win64 ||
            Target.Platform == UnrealTargetPlatform.Linux)
        {
            PublicDependencyModuleNames.Add("Steamworks");
            PublicDefinitions.Add("WITH_STEAMWORKS=1");
        }
        else
        {
            PublicDefinitions.Add("WITH_STEAMWORKS=0");
        }

        // ── WeGame Rail SDK (Windows only) ──
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicDefinitions.Add("WITH_WEGAME=1");
            string RailSDKPath = Path.Combine(ModuleDirectory, "../../../ThirdParty/RailSDK");
            if (Directory.Exists(RailSDKPath))
            {
                PublicIncludePaths.Add(Path.Combine(RailSDKPath, "include"));
                PublicAdditionalLibraries.Add(Path.Combine(RailSDKPath, "lib/win/Release_64/rail_api64.lib"));
                PublicDelayLoadDLLs.Add("rail_api64.dll");
                RuntimeDependencies.Add(Path.Combine(RailSDKPath, "lib/win/Release_64/rail_api64.dll"));
            }
            else
            {
                PublicDefinitions.Add("WITH_WEGAME=0");
                System.Console.WriteLine("WARNING: Rail SDK not found — building in stub mode");
            }
        }
        else
        {
            PublicDefinitions.Add("WITH_WEGAME=0");
        }

        // ── Mobile platform definitions ──
        if (Target.Platform == UnrealTargetPlatform.Android ||
            Target.Platform == UnrealTargetPlatform.IOS)
        {
            PublicDefinitions.Add("PLATFORM_MOBILE=1");
            PublicDefinitions.Add("MOBILE_TOUCH_INPUT=1");
            PublicDefinitions.Add("WITH_STEAMWORKS=0");
            PublicDefinitions.Add("WITH_WEGAME=0");

            PublicDependencyModuleNames.Add("Launch");
            PublicDependencyModuleNames.Add("ApplePlatform");
            PublicDependencyModuleNames.Add("AndroidPlatform");

            PublicDefinitions.Add("MOBILE_NETWORK_OPTIMISATIONS=1");
            PublicDefinitions.Add("WITH_MOD_SUPPORT=0");
        }
        else
        {
            PublicDefinitions.Add("PLATFORM_MOBILE=0");
            PublicDefinitions.Add("MOBILE_TOUCH_INPUT=0");
            PublicDefinitions.Add("WITH_MOD_SUPPORT=1");
        }

        // ── Android-specific ──
        if (Target.Platform == UnrealTargetPlatform.Android)
        {
            PublicDefinitions.Add("PLATFORM_ANDROID=1");
            PublicDependencyModuleNames.Add("VulkanRHI");
        }
        else
        {
            PublicDefinitions.Add("PLATFORM_ANDROID=0");
        }

        // ── iOS-specific ──
        if (Target.Platform == UnrealTargetPlatform.IOS)
        {
            PublicDefinitions.Add("PLATFORM_IOS=1");
            PublicDependencyModuleNames.Add("MetalRHI");
        }
        else
        {
            PublicDefinitions.Add("PLATFORM_IOS=0");
        }

        // ── Client/Server target differentiation ──
        if (Target.Type == TargetType.Server)
        {
            PublicDefinitions.Add("IS_DEDICATED_SERVER=1");
            PublicDefinitions.Add("IS_CLIENT=0");
            PublicDependencyModuleNames.Remove("RenderCore");
            PublicDependencyModuleNames.Remove("Renderer");
            PublicDependencyModuleNames.Remove("RHI");
            PublicDependencyModuleNames.Remove("Niagara");
            PublicDependencyModuleNames.Remove("NiagaraCore");
            PublicDependencyModuleNames.Remove("UMG");
            PublicDependencyModuleNames.Remove("Slate");
            PublicDependencyModuleNames.Remove("SlateCore");
            PublicDependencyModuleNames.Remove("CinematicCamera");
            PublicDependencyModuleNames.Remove("LevelSequence");
            PublicDependencyModuleNames.Remove("MovieScene");
            PublicDependencyModuleNames.Remove("AudioMixer");
            PublicDefinitions.Add("PLATFORM_MOBILE=0");
            PublicDefinitions.Add("MOBILE_TOUCH_INPUT=0");
        }
        else if (Target.Type == TargetType.Client)
        {
            PublicDefinitions.Add("IS_DEDICATED_SERVER=0");
            PublicDefinitions.Add("IS_CLIENT=1");
            PublicDependencyModuleNames.Remove("ReplicationGraph");
            PublicDependencyModuleNames.Remove("Iris");
        }
        else
        {
            PublicDefinitions.Add("IS_DEDICATED_SERVER=0");
            PublicDefinitions.Add("IS_CLIENT=0");
        }

        // ── Shipping optimizations ──
        if (Target.Configuration == UnrealTargetConfiguration.Shipping)
        {
            PublicDefinitions.Add("WITH_LOGGING=0");
            PublicDefinitions.Add("UE_BUILD_SHIPPING=1");

            if (Target.Platform == UnrealTargetPlatform.Android)
            {
                AdditionalLinkerArguments = "-Wl,--strip-all";
            }
            else if (Target.Platform == UnrealTargetPlatform.IOS)
            {
                AdditionalLinkerArguments = "-dead_strip";
            }
        }

        // ── Compression library ──
        PublicDefinitions.Add("USE_LZ4=1");
        PublicDefinitions.Add("USE_OODLE=0");
        PublicDefinitions.Add("USE_SNAPPY=0");

        // ── v7.4: Cargo / Delivery / Death systems ──
        PublicDefinitions.Add("WITH_CARGO_SYSTEM=1");
        PublicDefinitions.Add("WITH_PROXIMITY_DELIVERY=1");
        PublicDefinitions.Add("WITH_SHIP_INVALIDATION=1");
        PublicDefinitions.Add("WITH_PLAYER_DEATH_SYSTEM=1");

        // ── v7.5: Player Trade / Cargo Missions / NPC Station Tax ──
        PublicDefinitions.Add("WITH_PLAYER_TRADE=1");
        PublicDefinitions.Add("WITH_CARGO_MISSIONS=1");
        PublicDefinitions.Add("WITH_NPC_TRADE_TAX=1");
        PublicDefinitions.Add("WITH_PLAYER_TO_PLAYER_GIVE=1");

        if (Target.Type == TargetType.Server)
        {
            PublicDefinitions.Add("CARGO_AUTHORITY=1");
            PublicDefinitions.Add("DEATH_AUTHORITY=1");
            PublicDefinitions.Add("TRADE_AUTHORITY=1");
            PublicDefinitions.Add("MISSION_AUTHORITY=1");
            PublicDefinitions.Add("TURRET_AUTHORITY=1");
        }
        else
        {
            PublicDefinitions.Add("CARGO_AUTHORITY=0");
            PublicDefinitions.Add("DEATH_AUTHORITY=0");
            PublicDefinitions.Add("TRADE_AUTHORITY=0");
            PublicDefinitions.Add("MISSION_AUTHORITY=0");
            PublicDefinitions.Add("TURRET_AUTHORITY=0");
        }

        // ── v7.6.1: Tractor Beam + Defense Turret ──
        PublicDefinitions.Add("WITH_TRACTOR_BEAM=1");
        PublicDefinitions.Add("WITH_DEFENSE_TURRET=1");

        // ── v7.6.2: Elevator logic fix + Hangar permission ──
        PublicDefinitions.Add("WITH_ELEVATOR=1");
    }
}
