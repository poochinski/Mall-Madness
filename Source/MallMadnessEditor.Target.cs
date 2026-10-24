using UnrealBuildTool;

public class MallMadnessEditorTarget : TargetRules
{
	public MallMadnessEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("MallMadness");
	}
}
