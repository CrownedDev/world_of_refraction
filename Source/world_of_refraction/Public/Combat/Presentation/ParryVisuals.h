// ParryVisuals.h
// B-393 (D-170 corrected 2026-11-09, Q-393; Design/VisualGameplayPlan.md "Effects and parry rules"): what a landed
// parry (a successful Guard, Perfect or not) plays, decided by one resolver:
//   - the weapon guard always sparks (the defender's UCosmeticsData::GuardSparkVFX); a Perfect Guard plays its flash
//     instead (UDefenseSystem::PlayPerfectGuardFeedback), as before. The unarmed guard has no spark;
//   - on top, the guard's parry-effect cosmetic (UCosmeticsData::WeaponGuardParryEffectVFX / UnarmedGuardParryEffectVFX,
//     e.g. a magic circle) plays tinted by the element behind it. The sources: the weapon's crystal (weapon guard only:
//     an unarmed guard has put the weapon away) and the character's own magic (a Refractor's element, else its adapter's
//     crystal: a Resonator's active one, another class's primary one). When both have an element, the loadout setting
//     (ULoadoutComponent::GetParryElementPriority) picks; the other is used when the first has none;
//   - with no element behind it (None, Generic, a broken crystal), no parry effect plays: a weapon sparks only and the
//     unarmed guard shows nothing.
// With no parry-effect cosmetic authored, play is the spark as before (B-369), minus the unarmed guard's spark (D-170).
// B-503s (D-218, Q-503s C): a member's field attack pick replaces the guard and priority settings: a swing guards with
// the weapon in the option's element (plain: the spark only); a pulse guards unarmed in its element (Ice in Ice's
// colour; "(R) Fire" and "Dark Fire" are Fire). Never picked (an AI seat): the settings, as above.
// Spawned at the defender's HitPoint socket (ImpactVisuals::GetSpawnLocation); the burst is sent to clients too.
// Debug: wor.Guard.Print (each combatant's parry pick), wor.ImpactVFX.Log 1 (each landed parry).

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"
#include "Combat/Defense/EGuardStyle.h"
#include "Combat/Defense/EParryElementPriority.h"
#include "Skills/Definitions/ESpellElement.h"

class AActor;
class UNiagaraSystem;
struct FFieldAttackEntry;

/** Which source coloured the parry effect (debug and specs). */
enum class EParryElementSource : uint8
{
	/** No element behind the parry: no parry effect plays. */
	None,
	/** The weapon's crystal (weapon guard only). */
	Weapon,
	/** The character's own magic: a Refractor's element, else its adapter's crystal. */
	Magic
};

/** What a landed parry needs to know to pick its visuals; GatherInputs reads it from the defender. */
struct WORLD_OF_REFRACTION_API FParryVisualInputs
{
	EGuardStyle Guard = EGuardStyle::Unarmed;
	/** A Perfect Guard: its flash replaces the spark (DefenseSystem); the parry effect still plays. */
	bool bPerfect = false;
	/** The in-hand weapon's crystal element; None with no weapon, no crystal or a broken one. */
	ESpellElement WeaponElement = ESpellElement::None;
	/** A Refractor's element, else its adapter's crystal element; None with neither. */
	ESpellElement MagicElement = ESpellElement::None;
	EParryElementPriority Priority = EParryElementPriority::Weapon;
	/** B-503s: the field pick's Ice option: the Water effect takes Ice's colour. */
	bool bIce = false;
	/** The defender's cosmetic slots (UCosmeticsData). */
	TSoftObjectPtr<UNiagaraSystem> Spark;
	TSoftObjectPtr<UNiagaraSystem> WeaponGuardEffect;
	TSoftObjectPtr<UNiagaraSystem> UnarmedGuardEffect;
};

struct WORLD_OF_REFRACTION_API FParryVisualChoice
{
	/** The weapon guard's spark; null for the unarmed guard and a Perfect Guard. */
	TSoftObjectPtr<UNiagaraSystem> Spark;
	/** The guard's parry-effect cosmetic; null with no element behind it or none authored. */
	TSoftObjectPtr<UNiagaraSystem> Effect;
	EParryElementSource ElementSource = EParryElementSource::None;
	ESpellElement Element = ESpellElement::None;
	/** Element's colour (ElementColors), the effect's tint. */
	FLinearColor Tint = FLinearColor::White;

	bool ShouldSpawnSpark() const { return !Spark.IsNull(); }
	bool ShouldSpawnEffect() const { return !Effect.IsNull(); }
};

namespace ParryVisuals
{
	/** The Niagara user parameters the tint is written to (as the skill VFX tint, CombatPresentationComponent). */
	inline const FName TINT_CORE_PARAM(TEXT("CoreColor"));
	inline const FName TINT_EDGE_PARAM(TEXT("EdgeColor"));

	/** An element that can colour a parry effect: not None and not Generic (Generic means "inherit", no colour). */
	WORLD_OF_REFRACTION_API bool IsColouringElement(ESpellElement Element);

	/** D-170's rule only: which source colours the effect (no asset lookups). The weapon counts for the weapon guard
	 *  only; Priority picks when both have an element. */
	WORLD_OF_REFRACTION_API EParryElementSource PickElementSource(const FParryVisualInputs &Inputs);

	/** B-503s (D-218): sets the guard, both elements and the priority from the member's field attack pick.
	 *  bWeaponInHand false makes a swing (an unarmed Generic's) guard unarmed in its element. */
	WORLD_OF_REFRACTION_API void ApplyFieldPick(FParryVisualInputs &Inputs, const FFieldAttackEntry &Pick, bool bWeaponInHand);

	/** The spark, the effect and its tint. */
	WORLD_OF_REFRACTION_API FParryVisualChoice Resolve(const FParryVisualInputs &Inputs);

	/** The defender's guard (ULoadoutComponent::GetGuardStyle), weapon crystal, own magic, priority setting and
	 *  cosmetic slots. A defender with no loadout reads as unarmed with no element. */
	WORLD_OF_REFRACTION_API FParryVisualInputs GatherInputs(const AActor *Defender, bool bPerfect);

	/** GatherInputs + Resolve. */
	WORLD_OF_REFRACTION_API FParryVisualChoice ResolveForDefender(const AActor *Defender, bool bPerfect);

	/** Resolves and spawns a landed parry's visuals at the defender (and sends them to clients). Returns the choice. */
	WORLD_OF_REFRACTION_API FParryVisualChoice SpawnForLandedParry(AActor *Defender, bool bPerfect);

	/** "guard Weapon | weapon Fire, magic Water, priority Weapon". */
	WORLD_OF_REFRACTION_API FString GetInputsString(const FParryVisualInputs &Inputs);

	/** "spark NS_Spark + effect NS_Circle tinted Fire (Weapon)" / "nothing". */
	WORLD_OF_REFRACTION_API FString GetChoiceString(const FParryVisualChoice &Choice);
}
