// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class world_of_refraction : ModuleRules
{
	public world_of_refraction(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		// AIModule: ACombatAIController's base class (AAIController). Combat AI
		// decisions still live in UAIDecisionManager — the controller is only a
		// possession host so engine control queries (IsPlayerControlled /
		// IsBotControlled) report the truth.
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "Niagara", "UMG", "MotionWarping", "NetCore", "GameplayTags", "AIModule" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

		// Uncomment if you are using Slate UI
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
