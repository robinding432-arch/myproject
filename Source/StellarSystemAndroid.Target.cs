// StellarSystemAndroid.Target.cs
// v7.6.2 (fixed) — Android-specific build target

using UnrealBuildTool;
using System.Collections.Generic;

public class StellarSystemAndroid : TargetRules
{
    public StellarSystemAndroid(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

        // Android-specific settings
        bUseLoggingInShipping = false;
        bEnableExceptions = false;
        bUseUnityBuild = true;
        bUseAdaptiveUnityBuild = true;

        // Compile for ARM64 only
        AdditionalCompilerArguments = "-target-abi=arm64-v8a";

        // Strip debug symbols in shipping
        if (Target.Configuration == UnrealTargetConfiguration.Shipping)
        {
            AdditionalLinkerArguments = "-Wl,--strip-all";
        }

        ExtraModuleNames.AddRange(new string[] { "StellarSystem" });
    }
}