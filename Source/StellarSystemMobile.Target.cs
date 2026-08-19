// StellarSystemMobile.Target.cs
// v7.6.2 (fixed) — Shared mobile target (Android + iOS common settings)

using UnrealBuildTool;
using System.Collections.Generic;

public class StellarSystemMobile : TargetRules
{
    public StellarSystemMobile(ReadOnlyTargetRules Target) : base(Target)
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

        ExtraModuleNames.AddRange(new string[] { "StellarSystem" });
    }
}