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
class UTexture;
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

/** B-590 (D-264): one hexagon of the turn order. */
struct FBattleTurnHex
{
	TWeakObjectPtr<AActor> Actor;
	/** The character's face, captured at the fight's start (Q-590, Crown 2026-11-23); null: the plain placeholder. */
	TWeakObjectPtr<UTexture> Portrait;
	FLinearColor Border = FLinearColor::White;
	/** Width and height of the hexagon's box, Slate units. */
	float Size = 0.0f;
	bool bCurrent = false;
	bool bParty = false;
};

namespace BattleHUDView
{
	/** OBSOLETE (B-590, D-264): the turns shown come from the Turns shown setting (GetTurnsShown); delete after PIE. */
	constexpr int32 UPCOMING_TURNS_SHOWN = 6;

	// ---- B-590 (D-264): the turn order as hexagon portraits ----

	/** Turns shown, the current one included: the Turns shown setting's range and default. */
	constexpr int32 MIN_TURNS_SHOWN = 1;
	constexpr int32 MAX_TURNS_SHOWN = 6;
	constexpr int32 DEFAULT_TURNS_SHOWN = 6;

	// TODO: tune in PIE. Slate units: 1080p draws them as pixels, 720p at two thirds (D-264's 56 px and 40 px).
	constexpr float CURRENT_TURN_HEX_SIZE = 84.0f;
	constexpr float NEXT_TURN_HEX_SIZE = 60.0f;
	constexpr float TURN_HEX_GAP = 6.0f;
	constexpr float CURRENT_TURN_HEX_BORDER = 4.0f; // B-590 QA: the next turns' edges were lost on a bright sky
	constexpr float NEXT_TURN_HEX_BORDER = 3.0f;
	inline const FLinearColor CURRENT_TURN_HEX_COLOUR(0.72f, 0.55f, 1.0f); // violet
	inline const FLinearColor PARTY_TURN_HEX_COLOUR(0.3f, 0.85f, 0.95f);   // cyan
	inline const FLinearColor ENEMY_TURN_HEX_COLOUR(0.92f, 0.3f, 0.5f);    // rose
	inline const FLinearColor TURN_HEX_PLACEHOLDER_COLOUR(0.14f, 0.15f, 0.2f, 0.9f);

	/** A saved Turns shown: 0 (never set) is DEFAULT_TURNS_SHOWN; the rest clamped to MIN..MAX. */
	WORLD_OF_REFRACTION_API int32 ReadTurnsShown(int32 Saved);

	/** The hexagons for Strip's first TurnsShown slots (empty slots skipped): the current one CURRENT_TURN_HEX_SIZE with
	 *  the violet edge, the next ones NEXT_TURN_HEX_SIZE, cyan for the party (IsParty) and rose for the rest; each one's
	 *  picture from FindPortrait (null: the placeholder). */
	WORLD_OF_REFRACTION_API TArray<FBattleTurnHex> MakeTurnHexes(const TArray<FBattleTurnSlot> &Strip, int32 TurnsShown,
																 TFunctionRef<bool(const AActor *)> IsParty,
																 TFunctionRef<UTexture *(const AActor *)> FindPortrait);

	/** The six corners of a flat-topped hexagon filling a Size x Size box, clockwise from the left point. */
	WORLD_OF_REFRACTION_API TArray<FVector2f> GetTurnHexCorners(float Size);

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

	// ---- B-436b (BattleHUDRefinePlan decision 2): the party card's low HP. No tier by the name (Crown, 2026-11-23) ----

	/** Below this share of max HP a living party member's HP bar turns LowHPColor and pulses. */
	constexpr float LOW_HP_FRACTION = 0.3f;
	// TODO: tune LOW_HP_FRACTION and the three pulse values below in PIE.
	/** One pulse (full, dim, full) per this many seconds. */
	constexpr double LOW_HP_PULSE_PERIOD_SECONDS = 1.0;
	/** The HP bar's opacity at the pulse's dimmest. */
	constexpr float LOW_HP_PULSE_MIN_OPACITY = 0.45f;
	/** How often the pulse steps while a card is low (a timer, no Tick). */
	constexpr float LOW_HP_PULSE_STEP_SECONDS = 0.05f;

	/** Alive and under LOW_HP_FRACTION (a downed card never pulses). */
	WORLD_OF_REFRACTION_API bool IsLowHP(bool bAlive, float HPFraction);

	/** The low HP bar's opacity Seconds into the pulse: 1 at each whole period, LOW_HP_PULSE_MIN_OPACITY half-way. */
	WORLD_OF_REFRACTION_API float GetLowHPPulseOpacity(double Seconds);

	/** One status as a short chip (FeelGuide 5: small, on the character): its shared name (D-062), the turns left
	 *  unless permanent, and "xN" for two or more stacks. "Fire DoT 2", "Haste x3", "Regen". */
	WORLD_OF_REFRACTION_API FText MakeStatusChip(const FActiveSkillEffect &Effect);

	/** Every chip joined with " · "; empty for none. */
	WORLD_OF_REFRACTION_API FText JoinStatusChips(const TArray<FActiveSkillEffect> &Effects);

	/** One status in full, for the view-all panel: name, element, turns, stacks, then the authored description. */
	WORLD_OF_REFRACTION_API FText DescribeStatus(const FActiveSkillEffect &Effect);

	/** "[1 / A] Attack"; "[1] Attack" when Gamepad is not valid; Label alone when Keyboard is not valid. */
	WORLD_OF_REFRACTION_API FText WithKeyHint(const FText &Label, const FKey &Keyboard, const FKey &Gamepad);

	/** For each menu row (Categories[i], Enabled[i]), the index into ACombatPlayerController::GetMenuKeys() of the key
	 *  that presses it, or INDEX_NONE. Mirrors UCombatCommandMenuSubsystem::PressMenuKey: a key goes to the first
	 *  enabled row it presses; the category keys (CommandMenuRules::IsMainMenuKey, Defend among them since D-258c) only
	 *  when bAtMainMenu; Back and the infusion keys wherever their rows show. A row two keys press shows the first in the
	 *  key table (B-436g: Back shows Backspace, not Esc). B-588: in the Position picker the Nth MoveRow row takes the
	 *  key CommandMenuRules::PositionPickerRowForKey gives slot N (1 / A, 2 / X, 3 / Y). */
	WORLD_OF_REFRACTION_API TArray<int32> AssignMenuKeys(const TArray<EPieMenuCategory> &Categories, const TArray<bool> &Enabled,
														  bool bAtMainMenu, bool bPositionPicker = false);

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

	// OBSOLETE (B-585, D-257): the bar has no word; delete after Crown's checkpoint 6.
	WORLD_OF_REFRACTION_API FText GetStatusLabel();

	/** "40/100", on the bar (D-257; whole numbers, truncated as the old panel). */
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
