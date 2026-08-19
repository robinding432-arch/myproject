// StellarSystemMobile.Target.cs
// v7.2 — Shared mobile target (Android + iOS common settings)

using UnrealBuildTool;
using System.Collections.Generic;

public class StellarSystemMobile : TargetRules
{
    public StellarSystemMobile(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

        // Shared mobile optimizations
        bUseLoggingInShipping = false;
        bEnableExceptions = false;
        bUseUnityBuild = true;
        bUseAdaptiveUnityBuild = true;

        // Mobile-specific engine definitions
        GlobalDefinitions.Add("PLATFORM_MOBILE=1");
        GlobalDefinitions.Add("MOBILE_TOUCH_INPUT=1");
        GlobalDefinitions.Add("WITH_STEAMWORKS=0");
        GlobalDefinitions.Add("WITH_WEGAME=0");

        // No Steam/WeGame on mobile
        // Anti-cheat still active (server-side)

        ExtraModuleNames.AddRange(new string[] { "StellarSystem" });
    }
}
