using UnrealBuildTool;

public class world_of_refractionEditor : ModuleRules
{
	public world_of_refractionEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// B-573: the agent play tool (unreal-mcp). An Editor module is never built into a packaged game.
		// ToolsetRegistry is the editor plugin the project's AllToolsets plugin already turns on.
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "ToolsetRegistry" });
		PrivateDependencyModuleNames.AddRange(new string[] { "ApplicationCore", "Slate", "SlateCore", "UnrealEd", "ImageCore" });
	}
}
