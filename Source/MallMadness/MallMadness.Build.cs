using UnrealBuildTool;

public class MallMadness : ModuleRules
{
	public MallMadness(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "AIModule", "Slate", "SlateCore", "RenderCore", "RHI"
		});
	}
}
