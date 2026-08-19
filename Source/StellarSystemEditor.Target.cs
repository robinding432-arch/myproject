// StellarSystemEditor.Target.cs
// v7.6.2 (fixed) — Editor Target

using UnrealBuildTool;
using System.Collections.Generic;

public class StellarSystemEditor : TargetRules
{
    public StellarSystemEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

        // Editor macros
        GlobalDefinitions.Add("IS_CLIENT=0");
        GlobalDefinitions.Add("IS_DEDICATED_SERVER=0");
        GlobalDefinitions.Add("IS_LISTEN_SERVER=0");
        GlobalDefinitions.Add("WITH_EDITOR=1");

        // Editor: no LTO (too slow to compile)
        bUseUnity = true;
        bUseParallelCompiler = true;
        OptimizeCode = CodeOptimization.InNonDebug;

        ExtraModuleNames.Add("StellarSystem");
    }
}