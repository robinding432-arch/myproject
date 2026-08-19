// StellarSystemAndroid.Target.cs
// v7.2 — Android-specific build target

using UnrealBuildTool;
using System.Collections.Generic;

public class StellarSystemAndroid : TargetRules
{
    public StellarSystemAndroid(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

        // Android-specific settings
        bUseLoggingInShipping = false;
        bEnableExceptions = false;
        bUseUnityBuild = true;
        bUseAdaptiveUnityBuild = true;

        // Compile for ARM64 only (dropped 32-bit)
        AdditionalCompilerArguments = "-target-abi=arm64-v8a";

        // Strip debug symbols in shipping
        if (Configuration == UnrealTargetConfiguration.Shipping)
        {
            AdditionalLinkerArguments = "-Wl,--strip-all";
        }

        ExtraModuleNames.AddRange(new string[] { "StellarSystem" });
    }
}
