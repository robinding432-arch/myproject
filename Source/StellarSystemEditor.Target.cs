// StellarSystemEditor.Target.cs
// ============================================================
//  Editor Target — 编辑器编译目标（开发期使用）
//  用途：在 UE 编辑器里编译/运行/调试
//  特点：包含全部模块，方便开发
// ============================================================

using UnrealBuildTool;
using System.Collections.Generic;

public class StellarSystemEditor : TargetRules
{
    public StellarSystemEditor(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

        // ---- 编辑器宏 ----
        GlobalDefinitions.Add("IS_CLIENT=0");
        GlobalDefinitions.Add("IS_DEDICATED_SERVER=0");
        GlobalDefinitions.Add("IS_LISTEN_SERVER=0");
        GlobalDefinitions.Add("WITH_EDITOR=1");

        // 编辑器不开 LTO（编译太慢）
        bUseUnity = true;
        bUseParallelCompiler = true;
        OptimizeCode = CodeOptimization.InNonDebug;

        ExtraModuleNames.Add("StellarSystem");
    }
}
