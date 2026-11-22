// BattleIntelView.h
// B-586 (D-261, Q-586; refine_final/13_intel.png): what the fight's Intel panel shows, as plain data (no widgets), so the
// rules run headless (spec WoR.UI.Intel). One page per combatant, party first then enemies:
// - a party page: heading (the name), Spirit of ... / Body of ..., the bars, Pillars, Stats (eleven, as the Character
//   sheet; one an effect changed reads "before -> now", green better, red worse) and Effects (name, amount, turns left);
// - an enemy page: heading, the bars (HP without its number, Q-391), Weak to and Resists by element ("?" for the
//   elements no hit has taught yet, Q-586: learned per enemy type for the run, UTrialRunSubsystem::MarkIntelLearned)
//   and Effects.
// Your own character's tab reads "You" (D-261); its heading keeps the name. UBattleIntelWidget draws a page.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Resistance/EWoRDamageType.h"
#include "Skills/Effects/ActiveSkillEffect.h"

class AActor;
class UCharacterDataComponent;
class UTrialRunSubsystem;
class UWorld;
struct FBattleHUDCard;

/** How a row's value is coloured: better (green), worse (red), not learned yet (grey "?"), or plain. */
enum class EBattleIntelTone : uint8
{
	Plain,
	Better,
	Worse,
	Unknown,
};

/** One line of a section: its label, the value ("now"), the value before the fight's effects when they changed it, and
 *  an effect's turns left. */
struct FBattleIntelRow
{
	FText Label;
	FText Before;
	FText Now;
	FText Turns;
	EBattleIntelTone Tone = EBattleIntelTone::Plain;
};

/** One tab's page. */
struct FBattleIntelPage
{
	TWeakObjectPtr<AActor> Actor;
	bool bEnemy = false;
	FText Tab;
	FText Heading;
	FText Subline;
	float HPFraction = 0.0f;
	FText HPLine; // empty for an enemy (Q-391: its HP number stays hidden)
	bool bShowEP = false;
	float EPFraction = 0.0f;
	FText EPLine;
	float BuildupFraction = 0.0f;
	FText BuildupLine;
	TArray<FBattleIntelRow> Pillars;
	TArray<FBattleIntelRow> Stats;
	TArray<FBattleIntelRow> Effects;
	TArray<FBattleIntelRow> WeakTo;
	TArray<FBattleIntelRow> Resists;
};

namespace BattleIntelView
{
	/** A stat's two values, "before" only when the fight's effects moved it. Every Intel stat reads better higher. */
	WORLD_OF_REFRACTION_API FBattleIntelRow MakeStatRow(const FText &Label, float Before, float Now, bool bPercent);

	/** The eleven Character-sheet stats in its order: "before" is the stat without the fight's effects (the layers with
	 *  TransientFactor 1; Resistance without its ModifyStatusResist bonus). */
	WORLD_OF_REFRACTION_API TArray<FBattleIntelRow> BuildStats(AActor *Actor);

	/** Mind, Body and Spirit: the levels the Character sheet shows. */
	WORLD_OF_REFRACTION_API TArray<FBattleIntelRow> BuildPillars(const UCharacterDataComponent &Character);

	/** One effect: its name, its amount ("+50%", "-7 HP a turn", "can't act") and its turns left ("2 turns", "until
	 *  removed"). Holder is who carries it (a damage-over-time tick is read against it). */
	WORLD_OF_REFRACTION_API FBattleIntelRow DescribeEffect(AActor *Holder, const FActiveSkillEffect &Effect);

	/** The enemy-type key learning is kept by (its character key, as Kit Sight's). NAME_None without character data. */
	WORLD_OF_REFRACTION_API FName GetEnemyTypeKey(const AActor *Enemy);

	/** Q-586: Weak to / Resists for Enemy by element, a "?" line while any element is still unlearned. */
	WORLD_OF_REFRACTION_API void BuildElementLines(AActor *Enemy, TFunctionRef<bool(EWoRDamageType)> IsLearned,
												   TArray<FBattleIntelRow> &OutWeakTo, TArray<FBattleIntelRow> &OutResists);

	/** D-261: this machine's own player character (a recruit or another co-op player's character is not "You"). */
	WORLD_OF_REFRACTION_API bool IsYou(const AActor *Actor);

	/** The whole page for one card. Run may be null (no run: nothing is learned). */
	WORLD_OF_REFRACTION_API FBattleIntelPage BuildPage(const FBattleHUDCard &Card, const TArray<FActiveSkillEffect> &Effects,
													   const UTrialRunSubsystem *Run);

	/** Debug (wor.UI.Print): a page as lines. */
	WORLD_OF_REFRACTION_API FString GetPageString(const FBattleIntelPage &Page);
}
