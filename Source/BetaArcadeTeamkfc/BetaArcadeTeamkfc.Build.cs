using UnrealBuildTool;
public class BetaArcadeTeamkfc : ModuleRules
{
    public BetaArcadeTeamkfc(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "UMG" });
        PrivateDependencyModuleNames.AddRange(new[] { "Slate", "SlateCore", "ApplicationCore" });
        if (Target.Platform == UnrealTargetPlatform.Win64)
            PublicSystemLibraries.AddRange(new[] { "Xinput9_1_0.lib", "dinput8.lib", "dxguid.lib" });
    }
}
