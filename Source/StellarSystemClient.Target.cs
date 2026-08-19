// StellarSystemClient.Target.cs
// v7.6.2 (fixed) — Client Target

using UnrealBuildTool;
using System.Collections.Generic;

public class StellarSystemClient : TargetRules
{
    public StellarSystemClient(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = TargetType.Client;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

        // Client macros
        GlobalDefinitions.Add("IS_CLIENT=1");
        GlobalDefinitions.Add("IS_DEDICATED_SERVER=0");
        GlobalDefinitions.Add("IS_LISTEN_SERVER=0");

        // Client optimizations
        bUseUnity = true;
        bUseParallelCompiler = true;
        bEnableExceptions = false;
        bUseStaticCRT = false;

        if (Target.Configuration == UnrealTargetConfiguration.Shipping)
        {
            OptimizeCode = CodeOptimization.Speed;
            bUseLTO = true;
            bUsePCHFiles = true;
        }
        else if (Target.Configuration == UnrealTargetConfiguration.Development)
        {
            OptimizeCode = CodeOptimization.Speed;
        }

        ExtraModuleNames.Add("StellarSystem");
    }
}