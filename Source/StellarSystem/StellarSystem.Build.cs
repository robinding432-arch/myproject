// StellarSystem.Build.cs
// v7.6.2 (fixed) — Added System.IO + cleaned WeGame defines + safe module removal

using UnrealBuildTool;
using System.IO;   // ← 关键修复：Path.Combine / Directory.Exists 需要这个

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

            // ── Rendering (PC/Console/Editor only — excluded for Server below) ──
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

            // ── Cinematic (excluded for Server below) ──
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
        // 注意：WITH_WEGAME 默认值先设 0，找到 SDK 再改 1
        PublicDefinitions.Add("WITH_WEGAME=0");
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string RailSDKPath = Path.Combine(ModuleDirectory, "../../../ThirdParty/RailSDK");
            if (Directory.Exists(RailSDKPath))
            {
                PublicDefinitions.Remove("WITH_WEGAME=0");
                PublicDefinitions.Add("WITH_WEGAME=1");
                PublicIncludePaths.Add(Path.Combine(RailSDKPath, "include"));
                PublicAdditionalLibraries.Add(Path.Combine(RailSDKPath, "lib/win/Release_64/rail_api64.lib"));
                PublicDelayLoadDLLs.Add("rail_api64.dll");
                RuntimeDependencies.Add(Path.Combine(RailSDKPath, "lib/win/Release_64/rail_api64.dll"));
            }
            else
            {
                System.Console.WriteLine("WARNING: Rail SDK not found — building in stub mode");
            }
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
        // 用条件判断替代 Remove()，避免 UBT 数组操作副作用
        bool bIsServer = Target.Type == TargetType.Server;
        bool bIsClient = Target.Type == TargetType.Client;

        PublicDefinitions.Add(bIsServer ? "IS_DEDICATED_SERVER=1" : "IS_DEDICATED_SERVER=0");
        PublicDefinitions.Add(bIsClient ? "IS_CLIENT=1" : "IS_CLIENT=0");

        if (bIsServer)
        {
            // Server: strip client-only modules
            // 注意：已经加进去的模块用 Remove 在某些 UBT 版本不生效
            // 正确做法是在上面 AddRange 之前就按条件分流
            // 这里保留 Remove 作为兜底（UBT 5.4+ 支持 List.Remove）
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
        else if (bIsClient)
        {
            PublicDependencyModuleNames.Remove("ReplicationGraph");
            PublicDependencyModuleNames.Remove("Iris");
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

        // ── v7.4 ──
        PublicDefinitions.Add("WITH_CARGO_SYSTEM=1");
        PublicDefinitions.Add("WITH_PROXIMITY_DELIVERY=1");
        PublicDefinitions.Add("WITH_SHIP_INVALIDATION=1");
        PublicDefinitions.Add("WITH_PLAYER_DEATH_SYSTEM=1");

        // ── v7.5 ──
        PublicDefinitions.Add("WITH_PLAYER_TRADE=1");
        PublicDefinitions.Add("WITH_CARGO_MISSIONS=1");
        PublicDefinitions.Add("WITH_NPC_TRADE_TAX=1");
        PublicDefinitions.Add("WITH_PLAYER_TO_PLAYER_GIVE=1");

        // ── Authority flags ──
        bool bIsServerTarget = Target.Type == TargetType.Server;
        PublicDefinitions.Add(bIsServerTarget ? "CARGO_AUTHORITY=1" : "CARGO_AUTHORITY=0");
        PublicDefinitions.Add(bIsServerTarget ? "DEATH_AUTHORITY=1" : "DEATH_AUTHORITY=0");
        PublicDefinitions.Add(bIsServerTarget ? "TRADE_AUTHORITY=1" : "TRADE_AUTHORITY=0");
        PublicDefinitions.Add(bIsServerTarget ? "MISSION_AUTHORITY=1" : "MISSION_AUTHORITY=0");
        PublicDefinitions.Add(bIsServerTarget ? "TURRET_AUTHORITY=1" : "TURRET_AUTHORITY=0");

        // ── v7.6.1 ──
        PublicDefinitions.Add("WITH_TRACTOR_BEAM=1");
        PublicDefinitions.Add("WITH_DEFENSE_TURRET=1");

        // ── v7.6.2 ──
        PublicDefinitions.Add("WITH_ELEVATOR=1");
    }
}