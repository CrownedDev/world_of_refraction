// BattleHUDView.h
// B-391 (VisualGameplayPlan step 1): what the new placeholder battle HUD shows, as plain data and FText, with no
// widgets, so specs check it headless (WoR.UI.BattleHUD). The widgets (UBattleHUDWidget, UBattleCommandMenuWidget)
// only copy these into text blocks.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Skills/Definitions/ESpellElement.h"
#include "UI/Combat/PieMenuButtonData.h"

class AActor;
struct FActiveSkillEffect;

/** B-391c: what a party card names of its gear (the loadout component's active loadout). Empty = none. */
struct FBattleGearNames
{
	FString LoadoutName;
	FString WeaponName;
	FString RingName;
};

/** B-391c: the acting character's durability resources, as the old menu header found them. */
struct FBattleDurability
{
	/** A usable (slotted, unbroken) ring anywhere in the active loadout. */
	bool bHasRing = false;
	int32 RingCurrent = 0;
	int32 RingMax = 0;
	/** The active weapon has a slotted, unbroken crystal. */
	bool bHasWeapon = false;
	int32 WeaponCurrent = 0;
	int32 WeaponMax = 0;
};

/** One slot of the turn order strip. */
struct FBattleTurnSlot
{
	TWeakObjectPtr<AActor> Actor;
	int32 TurnNumber = 0;
	bool bCurrent = false;
	bool bBonus = false;
};

/** One upcoming turn as the turn state reports it (input to MakeTurnStrip). */
struct FBattleUpcomingTurn
{
	AActor *Actor = nullptr;
	bool bBonus = false;
};

namespace BattleHUDView
{
	/** Turns shown after the current one (Expedition 33's left column shows about eight in all; Q-391). */
	constexpr int32 UPCOMING_TURNS_SHOWN = 6;

	/** The strip: the current actor first, then Upcoming in order (at most MaxUpcoming), numbered on from TurnNumber.
	 *  Null actors are kept as slots so the numbering stays true. */
	WORLD_OF_REFRACTION_API TArray<FBattleTurnSlot> MakeTurnStrip(AActor *Current, bool bCurrentBonus, int32 TurnNumber,
																   const TArray<FBattleUpcomingTurn> &Upcoming, int32 MaxUpcoming);

	/** The strip from the fight: the replicated turn state when there is one (B-228, works on clients), else the
	 *  server's TurnManager (standalone, headless). Empty when neither exists. */
	WORLD_OF_REFRACTION_API TArray<FBattleTurnSlot> ReadTurnStrip(const UObject *Context, int32 TurnNumber, int32 MaxUpcoming);

	/** The fight's turn number now (turn state, else TurnManager); 1 when neither has one. */
	WORLD_OF_REFRACTION_API int32 ReadTurnNumber(const UObject *Context);

	/** "HP 120 / 150". */
	WORLD_OF_REFRACTION_API FText FormatPool(const FText &Label, int32 Current, int32 Max);

	/** Current / Max clamped to [0, 1]; 0 when Max is not positive. */
	WORLD_OF_REFRACTION_API float PoolFraction(int32 Current, int32 Max);

	WORLD_OF_REFRACTION_API FText GetHPLabel();
	WORLD_OF_REFRACTION_API FText GetEPLabel();

	/** B-391b: a Broken Darkness character's EP is its absorption pool (the old panel's "Abs"). */
	WORLD_OF_REFRACTION_API FText GetAbsorbLabel();

	/** B-391b: Current above a positive Max (Broken Darkness overload; capped at Max + 30% by the manager). */
	WORLD_OF_REFRACTION_API bool IsOverloaded(int32 Current, int32 Max);

	/** B-391b: "Absorb 80 / 100"; overloaded, "Absorb 130 / 100, overload 130%". */
	WORLD_OF_REFRACTION_API FText FormatAbsorbLine(int32 Current, int32 Max);

	/** B-391b: the absorb line's colour, as the old panel: white, then yellow / orange / red past the
	 *  CombatConstants::OVERLOAD_*_THRESHOLD ratios. */
	WORLD_OF_REFRACTION_API FLinearColor GetOverloadColour(int32 Current, int32 Max);

	/** The character's name from its data asset, else the actor's name; empty for null. */
	WORLD_OF_REFRACTION_API FText GetCombatantName(const AActor *Actor);

	/** One status as a short chip (FeelGuide 5: small, on the character): its shared name (D-062), the turns left
	 *  unless permanent, and "xN" for two or more stacks. "Burn 2", "Haste x3", "Regen". */
	WORLD_OF_REFRACTION_API FText MakeStatusChip(const FActiveSkillEffect &Effect);

	/** Every chip joined with " · "; empty for none. */
	WORLD_OF_REFRACTION_API FText JoinStatusChips(const TArray<FActiveSkillEffect> &Effects);

	/** One status in full, for the view-all panel: name, element, turns, stacks, then the authored description. */
	WORLD_OF_REFRACTION_API FText DescribeStatus(const FActiveSkillEffect &Effect);

	/** "[1 / A] Attack"; "[1] Attack" when Gamepad is not valid; Label alone when Keyboard is not valid. */
	WORLD_OF_REFRACTION_API FText WithKeyHint(const FText &Label, const FKey &Keyboard, const FKey &Gamepad);

	/** For each menu row (Categories[i], Enabled[i]), the index into ACombatPlayerController::GetMenuKeys() of the key
	 *  that presses it, or INDEX_NONE. Mirrors UCombatCommandMenuSubsystem::PressMenuKey: a key goes to the first
	 *  enabled row it presses; the category keys (CommandMenuRules::IsMainMenuKey) only when bAtMainMenu; Defend and the
	 *  infusion keys wherever their rows show. */
	WORLD_OF_REFRACTION_API TArray<int32> AssignMenuKeys(const TArray<EPieMenuCategory> &Categories, const TArray<bool> &Enabled,
														  bool bAtMainMenu);

	// ---- B-391c: the status buildup bar (the old panel's bar, with Q-111 A's cracks) ----

	// TODO: tune in PIE (placeholder until the CommonUI HUD, D-024).
	/** Q-111 A: the bar stays clean below this share of its cap, then cracks more and more toward the break. */
	constexpr float CRACK_START_FRACTION = 0.5f;
	/** Cracks between CRACK_START_FRACTION and the cap, evenly spaced (the first at CRACK_START_FRACTION). */
	constexpr int32 CRACK_COUNT = 4;

	/** Current / Max clamped to [0, 1]; 0 when Max is not positive. */
	WORLD_OF_REFRACTION_API float BuildupFraction(float Current, float Max);

	/** Where each crack sits along the bar (0..1), in order: 0.5, 0.625, 0.75, 0.875 at the defaults. */
	WORLD_OF_REFRACTION_API TArray<float> GetCrackPositions();

	/** How many cracks a bar this full shows: every crack at or below Fraction. */
	WORLD_OF_REFRACTION_API int32 CountCracksShown(float Fraction);

	WORLD_OF_REFRACTION_API FText GetStatusLabel();

	/** "Status 40 / 100" (whole numbers, truncated as the old panel). */
	WORLD_OF_REFRACTION_API FText FormatBuildupLine(float Current, float Max);

	/** The bar's fill, as the old panel's ApplyStatusBarTint: the pending element's colour (None / Generic the neutral
	 *  colour); a Broken Darkness attacker darkens an element to its hybrid colour. */
	WORLD_OF_REFRACTION_API FLinearColor GetBuildupColour(ESpellElement PendingElement, bool bSourceInBrokenDarkness);

	// ---- B-391c: the party card's gear line and tooltip ----

	/** The active loadout's name, weapon and ring (the Resonator's active ring, else the primary ring). Empty for an
	 *  actor without a loadout component. */
	WORLD_OF_REFRACTION_API FBattleGearNames ReadGearNames(const AActor *Actor);

	/** "Iron Sword · Ember Ring", one of them alone, or empty for no gear. */
	WORLD_OF_REFRACTION_API FText FormatGearLine(const FBattleGearNames &Gear);

	/** The hover tooltip: "Loadout: ...", "Weapon: ...", "Ring: ..." on three lines ("none" for an empty one). */
	WORLD_OF_REFRACTION_API FText FormatGearTooltip(const FBattleGearNames &Gear);

	// ---- B-391c: the command menu's durability header (the old UDurabilityHeaderWidget) ----

	/** The acting character's ring and weapon crystal durability, read from its loadout component as the old header. */
	WORLD_OF_REFRACTION_API FBattleDurability ReadDurability(const AActor *Actor);

	/** The header's lines, by the old header's slot rule: the ring first if it has one, then the weapon; none when it
	 *  has neither. "AD 3 / 5" (adapter durability, D-107), "WD 2 / 4". */
	WORLD_OF_REFRACTION_API TArray<FText> MakeDurabilityLines(const FBattleDurability &Durability);
}
