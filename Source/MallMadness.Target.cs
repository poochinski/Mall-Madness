using UnrealBuildTool;

public class MallMadnessTarget : TargetRules
{
	public MallMadnessTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("MallMadness");
	}
}
