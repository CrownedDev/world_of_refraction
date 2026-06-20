// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class world_of_refraction : ModuleRules
{
	public world_of_refraction(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "Niagara", "UMG", "MotionWarping" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

		// Uncomment if you are using Slate UI
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// F2 migration tooling (SkillEffectMigrationDebug): asset enumeration (runtime+editor)
		// + editor-only re-save via UEditorAssetLibrary. The tool itself is WITH_EDITOR-guarded.
		PrivateDependencyModuleNames.Add("AssetRegistry");
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("EditorScriptingUtilities");
		}
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
