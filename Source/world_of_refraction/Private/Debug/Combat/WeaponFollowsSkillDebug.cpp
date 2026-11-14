// WeaponFollowsSkillDebug.cpp

#include "Debug/Combat/WeaponFollowsSkillDebug.h"
#include "Loadout/LoadoutComponent.h"
#include "Save/SaveSubsystem.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Skills/Definitions/AbilityData.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	constexpr float WEAPON_FOLLOWS_SKILL_SCREEN_SECONDS = 10.0f;

	FAutoConsoleCommandWithWorldAndArgs GSwitchToSkillWeaponCmd(
		TEXT("wor.Settings.SwitchToSkillWeapon"),
		TEXT("Setting (B-474, D-207; until the settings screen lists it): 1 = using the other weapon's skill makes it active and ")
			TEXT("the battle menu offers both weapons' skills (default), 0 = off (L3 / 6 switches). Saved with the next save. No argument: prints it."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString> &Args, UWorld *World)
		{
			UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
			USaveSubsystem *Save = GameInstance ? GameInstance->GetSubsystem<USaveSubsystem>() : nullptr;
			if (!Save)
			{
				UE_LOG(LogWoRLoadout, Warning, TEXT("[WeaponFollowsSkill] wor.Settings.SwitchToSkillWeapon: no save subsystem."));
				return;
			}
			if (Args.Num() > 0)
			{
				if (!Args[0].IsNumeric())
				{
					UE_LOG(LogWoRLoadout, Warning, TEXT("[WeaponFollowsSkill] Usage: wor.Settings.SwitchToSkillWeapon <0|1>"));
					return;
				}
				Save->SetSwitchToSkillWeapon(FCString::Atoi(*Args[0]) != 0);
			}
			UE_LOG(LogWoRLoadout, Display, TEXT("[WeaponFollowsSkill] Switch to the weapon of the skill you use: %s"),
				   Save->IsSwitchToSkillWeaponOn() ? TEXT("on") : TEXT("off"));
		}));

	FAutoConsoleCommandWithWorld GPrintOtherWeaponSkillsCmd(
		TEXT("wor.Loadout.PrintOtherWeaponSkills"),
		TEXT("DEBUG (B-474): per character, the weapon in hand and the skills of the weapon not in hand (what the menu adds)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World) { WeaponFollowsSkillDebug::PrintOtherWeaponSkills(World); }));
}

FString WeaponFollowsSkillDebug::GetOtherWeaponSkillsString(const ULoadoutComponent &Loadout)
{
	const UWeaponData *InHand = Loadout.GetActiveWeapon();
	const TArray<UAbilityData *> Other = Loadout.GetOtherWeaponSkills();
	if (Other.Num() == 0)
	{
		return FString::Printf(TEXT("%s in hand; no other weapon's skills"), *GetNameSafe(InHand));
	}
	TArray<FString> Names;
	for (const UAbilityData *Ability : Other)
	{
		Names.Add(Ability ? Ability->Name : FString(TEXT("-")));
	}
	return FString::Printf(TEXT("%s in hand; the other weapon's skills: %s"), *GetNameSafe(InHand), *FString::Join(Names, TEXT(", ")));
}

void WeaponFollowsSkillDebug::PrintOtherWeaponSkills(UWorld *World)
{
	if (!World)
	{
		return;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const ULoadoutComponent *Loadout = It->FindComponentByClass<ULoadoutComponent>();
		if (!Loadout)
		{
			continue;
		}
		const FString Line = FString::Printf(TEXT("%s: %s"), *GetNameSafe(*It), *GetOtherWeaponSkillsString(*Loadout));
		UE_LOG(LogWoRLoadout, Display, TEXT("[WeaponFollowsSkill] %s"), *Line);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, WEAPON_FOLLOWS_SKILL_SCREEN_SECONDS, FColor::Cyan, Line);
		}
	}
}
