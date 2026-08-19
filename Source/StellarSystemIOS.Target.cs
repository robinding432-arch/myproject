// StellarSystemIOS.Target.cs
// v7.6.2 (fixed) — iOS-specific build target

using UnrealBuildTool;
using System.Collections.Generic;

public class StellarSystemIOS : TargetRules
{
    public StellarSystemIOS(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

        // iOS-specific settings
        bUseLoggingInShipping = false;
        bEnableExceptions = false;
        bUseUnityBuild = true;
        bUseAdaptiveUnityBuild = true;

        // ARM64 only (A8+)
        AdditionalCompilerArguments = "-arch arm64";

        // Strip symbols in shipping
        if (Target.Configuration == UnrealTargetConfiguration.Shipping)
        {
            AdditionalLinkerArguments = "-dead_strip";
        }

        // Disable JIT (iOS doesn't allow)
        GlobalDefinitions.Add("UE_ENABLE_ICU=0");

        ExtraModuleNames.AddRange(new string[] { "StellarSystem" });
    }
}