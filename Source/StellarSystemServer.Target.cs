// StellarSystemServer.Target.cs
// v7.6.2 (fixed) — Dedicated Server Target

using UnrealBuildTool;
using System.Collections.Generic;

public class StellarSystemServer : TargetRules
{
    public StellarSystemServer(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = TargetType.Server;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

        // Server macros
        GlobalDefinitions.Add("IS_DEDICATED_SERVER=1");
        GlobalDefinitions.Add("IS_CLIENT=0");
        GlobalDefinitions.Add("IS_LISTEN_SERVER=0");

        // Server optimizations
        bUseUnity = true;
        bUseParallelCompiler = true;
        bEnableExceptions = false;
        bUseStaticCRT = true;
        bBuildEditor = false;
        bBuildWithEditorOnlyData = false;
        bCompileAgainstEngine = true;
        bCompileAgainstCoreUObject = true;

        if (Target.Configuration == UnrealTargetConfiguration.Shipping)
        {
            OptimizeCode = CodeOptimization.SizeAndSpeed;
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