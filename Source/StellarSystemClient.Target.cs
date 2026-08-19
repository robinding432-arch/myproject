// StellarSystemClient.Target.cs
// ============================================================
//  Client Target — 纯客户端编译目标
//  用途：打包玩家电脑上运行的游戏客户端
//  特点：不包含 DedicatedServer 专用逻辑，开启客户端优化
// ============================================================

using UnrealBuildTool;
using System.Collections.Generic;

public class StellarSystemClient : TargetRules
{
    public StellarSystemClient(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Client;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

        // ---- 客户端专用宏 ----
        GlobalDefinitions.Add("IS_CLIENT=1");
        GlobalDefinitions.Add("IS_DEDICATED_SERVER=0");
        GlobalDefinitions.Add("IS_LISTEN_SERVER=0");

        // ---- 客户端优化 ----
        bUseUnity = true;                     // 客户端开 Unity Build 加速编译
        bUseParallelCompiler = true;
        bEnableExceptions = false;            // 关闭异常 → 更小更快
        bUseStaticCRT = false;

        // Shipping 极致优化
        if (Configuration == UnrealTargetConfiguration.Shipping)
        {
            OptimizeCode = CodeOptimization.Speed;
            bUseLTO = true;                   // 链接时优化
            bUsePCHFiles = true;
        }
        else if (Configuration == UnrealTargetConfiguration.Development)
        {
            OptimizeCode = CodeOptimization.Speed;
        }

        // ---- 客户端不需要的模块（裁剪体积）----
        // 服务器专用的管理/GM 命令/服务端验证日志不放客户端
        // （通过模块依赖控制，不链接到 Client target）

        ExtraModuleNames.Add("StellarSystem");
    }
}
