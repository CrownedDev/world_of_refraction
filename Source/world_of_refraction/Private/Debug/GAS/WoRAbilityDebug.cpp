// WoRAbilityDebug.cpp

#include "Debug/GAS/WoRAbilityDebug.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/WoRFightSubsystem.h"
#include "GAS/Abilities/WoRGameplayAbility.h"
#include "GAS/Abilities/WoRSkillAbilities.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	constexpr float ABILITY_DEBUG_SCREEN_SECONDS = 8.0f;

	FAutoConsoleCommandWithWorld GPrintAbilitiesCmd(
		TEXT("wor.Abilities.Print"),
		TEXT("DEBUG: each combatant's granted skill abilities: source skill, shape tags, active or idle, whether impacts, projectiles and AOE / instant / support resolutions route through GAS, plain EP cost (B-043b..e)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World) { UWoRAbilityDebug::PrintAbilities(World); }));
}

FString UWoRAbilityDebug::GetAbilitiesString(AActor *Actor)
{
	if (!Actor)
	{
		return TEXT("(no actor)");
	}
	UAbilitySystemComponent *ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	const TArray<FGameplayAbilitySpec> Specs = WoRSkillAbilities::GetSkillSpecs(Actor);
	FString Out = FString::Printf(TEXT("%s: %d skill ability(ies) (wor.Abilities.UseGAS %d, ASC %s, can grant %s, loadout skills %d)"),
								  *Actor->GetName(), Specs.Num(), WoRSkillAbilities::UsesAbilities() ? 1 : 0,
								  ASC ? TEXT("yes") : TEXT("no"), WoRSkillAbilities::GetGrantingASC(Actor) ? TEXT("yes") : TEXT("no"),
								  WoRSkillAbilities::GetLoadoutSkills(Actor).Num());
	UActionExecutor *Executor = WoRFight::Get<UActionExecutor>(Actor);
	const bool bPriceable = Executor && Actor->FindComponentByClass<UCharacterDataComponent>();
	for (const FGameplayAbilitySpec &Spec : Specs)
	{
		USkillDataBase *Skill = Cast<USkillDataBase>(Spec.SourceObject.Get());
		const UWoRGameplayAbility *Instance = Cast<UWoRGameplayAbility>(Spec.GetPrimaryInstance());
		const FAction *Running = Instance ? Instance->GetRunningAction() : nullptr;
		Out += FString::Printf(TEXT("\n  - %s [%s] %s"), *GetNameSafe(Skill), *Spec.GetDynamicSpecSourceTags().ToStringSimple(),
							   Spec.IsActive() ? TEXT("ACTIVE") : TEXT("idle"));
		if (Running)
		{
			Out += FString::Printf(TEXT(", running %s"), *Running->GetActionName());
		}
		// B-043c: whether this action's impacts arrive as Event.Combat.Impact, and how many have so far.
		if (Instance)
		{
			Out += FString::Printf(TEXT(", impacts via GAS: %s (%d resolved)"),
								   Instance->IsListeningForImpacts() ? TEXT("listening") : TEXT("not listening"),
								   Instance->GetImpactEventsResolved());
			// B-043d: projectile arrivals / dodges through events, and burst spawns its WaitDelay chain drove.
			Out += FString::Printf(TEXT(", projectiles via GAS: %s (%d resolved, %d burst spawns)"),
								   Instance->IsListeningForProjectiles() ? TEXT("listening") : TEXT("not listening"),
								   Instance->GetProjectileEventsResolved(), Instance->GetBurstSpawnsDriven());
			// B-043e: whether this action's AOE / Instant / support resolutions arrive as Event.Combat.SpellResolve.*.
			Out += FString::Printf(TEXT(", spell resolves via GAS: %s (%d AOE, %d instant, %d support)"),
								   Instance->IsListeningForSpellResolves() ? TEXT("listening") : TEXT("not listening"),
								   Instance->GetAOEResolves(), Instance->GetInstantResolves(), Instance->GetSupportResolves());
		}
		if (bPriceable && Skill)
		{
			Out += FString::Printf(TEXT(", plain cast %d EP"),
								   Executor->CalculateActionEnergyCost(Actor, WoRSkillAbilities::MakePlainAction(Skill)));
		}
	}
	return Out;
}

int32 UWoRAbilityDebug::PrintAbilities(const UObject *WorldContextObject)
{
	UWorld *World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return 0;
	}
	int32 SpecCount = 0;
	int32 Combatants = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (!It->FindComponentByClass<UCharacterDataComponent>())
		{
			continue;
		}
		++Combatants;
		SpecCount += WoRSkillAbilities::GetSkillSpecs(*It).Num();
		UE_LOG(LogWoR, Display, TEXT("[AbilityDebug] %s"), *GetAbilitiesString(*It));
	}
	const FString Summary = FString::Printf(TEXT("wor.Abilities.Print: %d skill ability(ies) on %d combatant(s), wor.Abilities.UseGAS %d"),
											SpecCount, Combatants, WoRSkillAbilities::UsesAbilities() ? 1 : 0);
	UE_LOG(LogWoR, Display, TEXT("[AbilityDebug] %s"), *Summary);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, ABILITY_DEBUG_SCREEN_SECONDS, FColor::Cyan, Summary);
	}
	return SpecCount;
}
