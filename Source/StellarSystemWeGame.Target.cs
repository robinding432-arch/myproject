// StellarSystemWeGame.Target.cs
// v6.9 — WeGame 平台专用编译目标
// 编译方式：
//   Windows: RunUAT.bat BuildGame -targetplatform=Win64 -configuration=Shipping -target=StellarSystemWeGame
//   Linux:   RunUAT.sh  BuildGame -targetplatform=Linux -configuration=Shipping -target=StellarSystemWeGame

using UnrealBuildTool;
using System.Collections.Generic;

public class StellarSystemWeGame : TargetRules
{
    public StellarSystemWeGame(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;

        // WeGame 版本始终为 Shipping
        Configuration = UnrealTargetConfiguration.Shipping;

        // 启用 LTCG 优化（减小包体 + 提升运行速度）
        bUseLTCG = true;
        bUsePCHFiles = true;

        // 禁用编辑器模块
        bBuildEditor = false;
        bBuildWithEditorOnlyData = false;

        // 强制定义为客户端（WeGame 版是纯客户端）
        GlobalDefinitions.Add("IS_CLIENT=1");
        GlobalDefinitions.Add("IS_DEDICATED_SERVER=0");
        GlobalDefinitions.Add("WITH_WEGAME=1");
        GlobalDefinitions.Add("WITH_STEAMWORKS=0"); // WeGame 版不含 Steam

        // Shipping 优化
        GlobalDefinitions.Add("UE_BUILD_SHIPPING=1");
        GlobalDefinitions.Add("WITH_LOGGING=0"); // 关闭日志（可改为 1 保留）
    }
}
