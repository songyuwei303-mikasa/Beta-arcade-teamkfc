using UnrealBuildTool;
public class BetaArcadeTeamkfcTarget : TargetRules
{
    public BetaArcadeTeamkfcTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        ExtraModuleNames.Add("BetaArcadeTeamkfc");
    }
}
