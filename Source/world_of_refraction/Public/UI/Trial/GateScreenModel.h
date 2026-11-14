// GateScreenModel.h
// B-466 (D-191, D-192; UIScreens #21, mockups screen_mockups/run/gate/): what the Gate screen shows and what Enter does,
// with no widget in it, so the specs read it headless. UGateScreenWidget draws it.
//
// Words: a "gate" is a UTrialData (Stone Gate, Silver Gate, the crystal-named gates), a "trial" is one of its
// FTrialVariation (D-192's mapping). Every gate is open to everyone and the hub has one gate (D-192): the screen lists
// every trial asset with a level. A trial row shows its name and line, never its boss; a locked one shows "???" and how
// it unlocks (D-191). The "Going in" panel lists the party; an AI seat shows nothing for who plays it (D-191).
// Debug: wor.UI.PrintGate logs GetGateScreenString.

#pragma once

#include "CoreMinimal.h"
#include "Trial/TrialGate.h"

class UGameInstance;
class UTrialData;
class UWorld;

struct FGateTrialRow
{
	/** The trial's name, or "???" while locked. */
	FText Name;
	/** Its line, or how it unlocks while locked. */
	FText Line;
	bool bLocked = false;
};

struct FGateDifficultyCard
{
	FText Name;
	/** "Enemy damage x1.2". */
	FText DamageLine;
	/** "Payout x1.5". */
	FText PayoutLine;
	/** Pays more than Medium (drawn in the good colour). */
	bool bPaysMore = false;
};

struct FGateMember
{
	FText Name;
	FText ClassName;
	/** "You", or empty for an AI seat (D-191: no level text). */
	FText PlayedBy;
};

struct FGateScreenPage
{
	/** One tab per gate, by name. */
	TArray<FText> Gates;
	TArray<FGateTrialRow> Trials;
	TArray<FGateDifficultyCard> Difficulties;
	TArray<FGateMember> Members;
	/** "5  (3 in the final fight)". */
	FText WipesLine;
	/** The difficulty's rule, and the run's length and save rule (the mockup's two lines). */
	FText DifficultyNote;
	FText RunNote;
};

namespace WoRGateScreen
{
	/** Every trial asset with a level, in a fixed order (by asset path): the gates. */
	WORLD_OF_REFRACTION_API TArray<UTrialData *> GatherGates();

	/** A trial is open unless it names an unlock quest that isn't finished. */
	WORLD_OF_REFRACTION_API bool IsUnlocked(const UGameInstance *GameInstance, const FTrialVariation &Trial);

	/** The first open trial of Gate; INDEX_NONE when it has none (or no trials at all). */
	WORLD_OF_REFRACTION_API int32 GetFirstOpenTrial(const UGameInstance *GameInstance, const UTrialData *Gate);

	/** The page for Gate (one of Gates) in World. */
	WORLD_OF_REFRACTION_API FGateScreenPage BuildPage(UWorld *World, const TArray<UTrialData *> &Gates, const UTrialData *Gate);

	/** Why Enter would refuse (for the player), or empty when it would go: a locked or missing trial, a gate with
	 *  no level. */
	WORLD_OF_REFRACTION_API FText GetEnterRefusal(const UGameInstance *GameInstance, const UTrialData *Gate, int32 TrialIndex);

	/** Enter: keeps the choice on the run and opens the gate's level. Empty when it went; else the refusal. */
	WORLD_OF_REFRACTION_API FText Enter(UGameInstance *GameInstance, UTrialData *Gate, int32 TrialIndex, ETrialDifficulty Difficulty);

	WORLD_OF_REFRACTION_API FString GetGateScreenString(UWorld *World);
}
