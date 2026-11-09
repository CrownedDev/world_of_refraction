// WoRSkillAbilities.h
// B-043b (GAS stage 4 part 2, Q-043b): a combatant's skills as granted UWoRGameplayAbility specs, and the
// orchestrator's hand-off that starts an action through its skill's ability.
// - Granting: the orchestrator grants each combatant's loadout skills at fight start (GrantLoadoutSkills:
//   weapon attacks, abilities, spells) and clears them at fight end (ClearSkills, which cancels a running
//   one). A skill the loadout gains mid-fight (weapon switch, a Broken Darkness pool) is granted when first
//   used. Granting changes nothing in play.
// - `wor.Abilities.UseGAS` (default 0): at 1, TryExecuteThroughAbility starts a spell or ability action through
//   TryActivateAbility on its skill's spec; the ability runs the same UActionExecutor::ExecuteActionAsync.
//   At 0, or for an action with no skill (Defend, Item, Flee, Switch Weapon), a deferred ritual fire (B-043g),
//   an actor with no ASC, or an activation GAS refuses, the caller runs the old call.
// - Impacts (B-043c, Q-043c): while an ability runs a non-spell action, the executor hands each impact to
//   SendImpactEvent, which sends it to the ability's WaitGameplayEvent(Event.Combat.Impact) task; with no such
//   ability the executor resolves the impact itself, as before.
// - Projectiles (B-043d, Q-043d): while an ability runs a projectile action, the executor sends each arrival and
//   dodge to it (SendProjectileEvent) and hands it the burst stagger (StartBurstThroughAbility); with no such
//   ability the executor resolves them and runs its own timer, as before.
// Debug: Debug/GAS/WoRAbilityDebug.h (wor.Abilities.Print). Spec: WoR.GAS.Abilities.

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagContainer.h"
#include "Combat/Actions/ActionExecutor.h"

class AActor;
class UAbilitySystemComponent;
class USkillDataBase;
class UWoRGameplayAbility;
struct FGameplayAbilitySpec;

namespace WoRSkillAbilities
{
	/** The value of `wor.Abilities.UseGAS`; tests flip it with SetUsesAbilities. */
	WORLD_OF_REFRACTION_API bool UsesAbilities();
	WORLD_OF_REFRACTION_API void SetUsesAbilities(bool bEnabled);

	/** Ability.Shape.<execution type>, plus Ability.Shape.<delivery> for each cast entry's delivery (a spell
	 *  with no cast entries: its DeliveryType, as the orchestrator's async check reads it). */
	WORLD_OF_REFRACTION_API FGameplayTagContainer GetShapeTags(const USkillDataBase &Skill);

	/** The skill an action runs: the spell for a Spell action, the skill for an Ability action, else null. */
	WORLD_OF_REFRACTION_API USkillDataBase *GetActionSkill(const FAction &Action);

	/** The skill's plain cast (no infusion, innate source): what CheckCost and the debug print price
	 *  a spec at when no action is being activated. */
	WORLD_OF_REFRACTION_API FAction MakePlainAction(USkillDataBase *Skill);

	/** The ASC skills are granted on: Actor's, authority only. Null otherwise. */
	WORLD_OF_REFRACTION_API UAbilitySystemComponent *GetGrantingASC(AActor *Actor);

	/** Actor's spec for Skill (a UWoRGameplayAbility spec whose source object is Skill), or null. */
	WORLD_OF_REFRACTION_API FGameplayAbilitySpec *FindSkillSpec(AActor *Actor, const USkillDataBase *Skill);

	/** Grants Skill unless Actor already has its spec; returns the spec's handle (invalid without an ASC). */
	WORLD_OF_REFRACTION_API FGameplayAbilitySpecHandle GrantSkill(AActor *Actor, USkillDataBase *Skill);

	/** Grants each skill (nulls and repeats skipped); returns how many new specs were given. */
	WORLD_OF_REFRACTION_API int32 GrantSkills(AActor *Actor, const TArray<USkillDataBase *> &Skills);

	/** The skills Actor's loadout can use now: weapon attacks, abilities, spells (ULoadoutComponent). */
	WORLD_OF_REFRACTION_API TArray<USkillDataBase *> GetLoadoutSkills(const AActor *Actor);

	/** GrantSkills(Actor, GetLoadoutSkills(Actor)). */
	WORLD_OF_REFRACTION_API int32 GrantLoadoutSkills(AActor *Actor);

	/** Cancels and removes every skill spec on Actor; returns how many were removed. */
	WORLD_OF_REFRACTION_API int32 ClearSkills(AActor *Actor);

	/** Every skill spec on Actor (copies, for inspection). */
	WORLD_OF_REFRACTION_API TArray<FGameplayAbilitySpec> GetSkillSpecs(AActor *Actor);

	/** With `wor.Abilities.UseGAS` 1: starts Action through its skill's ability (granted now if missing) and
	 *  returns true; OnComplete runs when the action completes, after the ability has ended. False, with
	 *  nothing started, when the caller must run UActionExecutor::ExecuteActionAsync itself. */
	WORLD_OF_REFRACTION_API bool TryExecuteThroughAbility(AActor *Actor, const FAction &Action, const FOnActionComplete &OnComplete);

	/** B-043c: Actor's skill ability that is running an action and waiting for its impacts, or null. */
	WORLD_OF_REFRACTION_API UWoRGameplayAbility *FindImpactListener(AActor *Actor);

	/** B-043c: with an impact listener on Actor, sends Event.Combat.Impact (EventMagnitude = Index) to Actor and
	 *  returns true once the listener has resolved it. False, with nothing resolved, when the caller must resolve
	 *  the impact itself (no listener, or the event did not reach it). */
	WORLD_OF_REFRACTION_API bool SendImpactEvent(AActor *Actor, int32 Index);

	/** B-043d: Actor's skill ability that is running an action and waiting for its projectiles, or null. */
	WORLD_OF_REFRACTION_API UWoRGameplayAbility *FindProjectileListener(AActor *Actor);

	/** B-043d: with a projectile listener on Actor (the caster), sends Event (Event.Combat.Projectile.Arrived or
	 *  .Dodged; Target = Target, EventMagnitude = CastEntryIndex) and returns true once the listener has resolved it.
	 *  False, with nothing resolved, when the caller must resolve the arrival itself. */
	WORLD_OF_REFRACTION_API bool SendProjectileEvent(AActor *Actor, const FGameplayTag &Event, AActor *Target, int32 CastEntryIndex);

	/** B-043d: hands a burst's stagger (one spawn per Interval) to Actor's projectile listener. False when there
	 *  is none: the caller runs its own timer. */
	WORLD_OF_REFRACTION_API bool StartBurstThroughAbility(AActor *Actor, float Interval);
}
