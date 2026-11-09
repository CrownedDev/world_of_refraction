// CastVisuals.h
// B-394 (D-170, Q-394; Design/VisualGameplayPlan.md "Effects and parry rules"): which cast visual (the launch flash at
// the caster, the skill's Muzzle VFX) plays, decided by one resolver:
//   - a weapon's attack, ability or weapon-crystal spell plays the weapon's fixed CastVFX (UWeaponData);
//   - a spell from an adapter plays its adapter type's default (UImpactVisualDefaults::AdapterCastVFX by
//     EAdapterForm); there is no per-adapter override. A Refractor's innate spell and an evolution's spell use the
//     same default: the worn adapter's form, else Ring (Q-394);
//   - a Generic wearing both a weapon and an adapter: the class preference (UImpactVisualDefaults::
//     GenericCastVFXPreference) picks Weapon or Adapter for every spell it casts;
//   - a spell item plays its own.
// Fallback: when the picked source has no asset authored, the skill's own Muzzle VFX plays (today's visual), so play
// only changes once the new data is set.
// Debug: wor.VFX.PrintCastSource (every combatant's pick for each kind of cast).

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"
#include "Character/ECharacterClass.h"
#include "Equipment/Rings/EAdapterForm.h"
#include "Skills/Definitions/ESpellSource.h"
#include "CastVisuals.generated.h"

class AActor;
class UImpactVisualDefaults;
class UNiagaraSystem;
class URingData;
class UWeaponData;
struct FAction;

/** D-170: which VFX a Generic wearing both a weapon and an adapter shows for its spells (class data, Q-394). */
UENUM(BlueprintType)
enum class ECastVFXPreference : uint8
{
	Adapter UMETA(DisplayName = "Adapter"),
	Weapon UMETA(DisplayName = "Weapon")
};

/** Where a cast visual comes from (debug and specs). */
enum class ECastVFXSource : uint8
{
	/** The active weapon's fixed CastVFX. */
	Weapon,
	/** The adapter type's default (Ring when no adapter is worn). */
	AdapterDefault,
	/** The skill's own Muzzle VFX (a spell item, or no action context). */
	Skill
};

/** What a cast needs to know to pick its visual; GatherCastVisualInputs reads it from a combatant. */
struct WORLD_OF_REFRACTION_API FCastVisualInputs
{
	/** False when the cast has no action (nothing to read the source from): the skill's own VFX plays. */
	bool bHasAction = true;
	ECharacterClass CharacterClass = ECharacterClass::Generic;
	/** A spell (USpellData); false for weapon attacks and abilities. */
	bool bIsSpell = false;
	ESpellSource SpellSource = ESpellSource::Innate;
	/** The active weapon (CL-17); null with none. */
	const UWeaponData *Weapon = nullptr;
	/** The worn adapter: a Resonator's active one, else the primary one; null with none. */
	const URingData *Adapter = nullptr;
	/** The skill's own Muzzle VFX: today's visual and the fallback. */
	TSoftObjectPtr<UNiagaraSystem> SkillVFX;
};

struct WORLD_OF_REFRACTION_API FCastVisualChoice
{
	/** The source D-170's rule picks. */
	ECastVFXSource Source = ECastVFXSource::Skill;
	/** The picked source had no asset, so VFX is the skill's own. */
	bool bFellBackToSkill = false;
	/** For AdapterDefault: the adapter type whose default was read. */
	EAdapterForm AdapterForm = EAdapterForm::Ring;
	TSoftObjectPtr<UNiagaraSystem> VFX;
};

namespace CastVisuals
{
	/** The adapter type a cast without a worn adapter uses (D-170: rings are the default). */
	constexpr EAdapterForm DEFAULT_ADAPTER_FORM = EAdapterForm::Ring;

	/** The Generic preference with no defaults asset authored (Q-394). */
	constexpr ECastVFXPreference FALLBACK_GENERIC_PREFERENCE = ECastVFXPreference::Adapter;

	/** D-170's rule only: which source a cast reads (no asset lookups). */
	WORLD_OF_REFRACTION_API ECastVFXSource PickSource(const FCastVisualInputs &Inputs, ECastVFXPreference GenericPreference);

	/** The source and the asset to play. Defaults may be null (no adapter defaults, Generic preference
	 *  FALLBACK_GENERIC_PREFERENCE). */
	WORLD_OF_REFRACTION_API FCastVisualChoice Resolve(const FCastVisualInputs &Inputs, const UImpactVisualDefaults *Defaults);

	/** The caster's class, active weapon and worn adapter (its ULoadoutComponent), the action's spell source, and
	 *  SkillVFX. Action may be null (no action context): bHasAction is false and the skill's own VFX plays. */
	WORLD_OF_REFRACTION_API FCastVisualInputs GatherInputs(const AActor *Caster, const FAction *Action, bool bIsSpell,
															 const TSoftObjectPtr<UNiagaraSystem> &SkillVFX);

	/** GatherInputs + Resolve with UImpactVisualDefaults::Get: the one call the executor makes. */
	WORLD_OF_REFRACTION_API FCastVisualChoice ResolveForCaster(const AActor *Caster, const FAction *Action, bool bIsSpell,
															   const TSoftObjectPtr<UNiagaraSystem> &SkillVFX);

	/** "AdapterDefault (Ring): NS_Magic_Circle1" / "Weapon -> skill fallback: NS_Muzzle". */
	WORLD_OF_REFRACTION_API FString GetChoiceString(const FCastVisualChoice &Choice);
}
