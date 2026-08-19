// StellarSystemWeGame.Target.cs
// v7.6.2 (fixed) — WeGame platform-specific build target

using UnrealBuildTool;
using System.Collections.Generic;

public class StellarSystemWeGame : TargetRules
{
    public StellarSystemWeGame(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

        // WeGame: force Shipping via command line (-configuration=Shipping)
        // Do NOT assign Configuration here — UBT will set it

        // LTO for smaller binary + faster runtime
        bUseLTO = true;
        bUsePCHFiles = true;

        // No editor modules
        bBuildEditor = false;
        bBuildWithEditorOnlyData = false;

        // WeGame = client only
        GlobalDefinitions.Add("IS_CLIENT=1");
        GlobalDefinitions.Add("IS_DEDICATED_SERVER=0");
        GlobalDefinitions.Add("WITH_WEGAME=1");
        GlobalDefinitions.Add("WITH_STEAMWORKS=0");

        // Shipping flags (only apply when actually building Shipping)
        if (Target.Configuration == UnrealTargetConfiguration.Shipping)
        {
            GlobalDefinitions.Add("UE_BUILD_SHIPPING=1");
            GlobalDefinitions.Add("WITH_LOGGING=0");
        }
        else
        {
            GlobalDefinitions.Add("UE_BUILD_SHIPPING=0");
            GlobalDefinitions.Add("WITH_LOGGING=1");
        }

        ExtraModuleNames.Add("StellarSystem");
    }
}