// StellarSystemIOS.Target.cs
// v7.2 — iOS-specific build target

using UnrealBuildTool;
using System.Collections.Generic;

public class StellarSystemIOS : TargetRules
{
    public StellarSystemIOS(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

        // iOS-specific settings
        bUseLoggingInShipping = false;
        bEnableExceptions = false;
        bUseUnityBuild = true;
        bUseAdaptiveUnityBuild = true;

        // Metal is the only supported renderer on iOS
        // A8+ devices (64-bit only)
        AdditionalCompilerArguments = "-arch arm64";

        // Strip symbols in shipping
        if (Configuration == UnrealTargetConfiguration.Shipping)
        {
            AdditionalLinkerArguments = "-dead_strip";
        }

        // Disable JIT (iOS doesn't allow)
        GlobalDefinitions.Add("UE_ENABLE_ICU=0");

        ExtraModuleNames.AddRange(new string[] { "StellarSystem" });
    }
}
