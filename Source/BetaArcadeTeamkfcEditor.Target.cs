using UnrealBuildTool;
public class BetaArcadeTeamkfcEditorTarget : TargetRules
{
    public BetaArcadeTeamkfcEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        ExtraModuleNames.Add("BetaArcadeTeamkfc");
    }
}
