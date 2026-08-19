// StellarSystemServer.Target.cs
// ============================================================
//  Dedicated Server Target — 纯服务端编译目标
//  用途：打包 Linux/Windows 专用游戏服务器（无渲染、无音频、无 UI）
//  特点：无头模式（Headless）、最小内存占用、最大网络吞吐
// ============================================================

using UnrealBuildTool;
using System.Collections.Generic;

public class StellarSystemServer : TargetRules
{
    public StellarSystemServer(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Server;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

        // ---- 服务器专用宏 ----
        GlobalDefinitions.Add("IS_DEDICATED_SERVER=1");
        GlobalDefinitions.Add("IS_CLIENT=0");
        GlobalDefinitions.Add("IS_LISTEN_SERVER=0");

        // ---- 服务器关键优化 ----
        bUseUnity = true;
        bUseParallelCompiler = true;
        bEnableExceptions = false;
        bUseStaticCRT = true;                // 服务端静态链接 CRT → 部署简单
        bBuildEditor = false;                // 服务器不编编辑器
        bBuildWithEditorOnlyData = false;    // 不打包编辑器数据
        bCompileAgainstEngine = true;
        bCompileAgainstCoreUObject = true;

        // Shipping：极致性能 + 最小体积
        if (Configuration == UnrealTargetConfiguration.Shipping)
        {
            OptimizeCode = CodeOptimization.SizeAndSpeed;
            bUseLTO = true;
            bUsePCHFiles = true;
        }
        else if (Configuration == UnrealTargetConfiguration.Development)
        {
            OptimizeCode = CodeOptimization.Speed;
        }

        // ---- 服务器不需要的模块（大幅裁剪）----
        // 无渲染：去掉 Slate/UMG/Cinematic/ Niagara/ShaderCore/Renderer
        // 无音频：去掉 AudioMixer/SoundModulation/SignalProcessing
        // 无物理（可选）：服务端用简单碰撞即可

        ExtraModuleNames.Add("StellarSystem");
    }
}
