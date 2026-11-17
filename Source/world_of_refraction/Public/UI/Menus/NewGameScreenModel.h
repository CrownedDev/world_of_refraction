// NewGameScreenModel.h
// B-469 (UIScreens #11, mockups screen_mockups/front/new_game/): what the new game screen shows and what Begin does,
// apart from the widget so specs can check it headless.
//
// The pick is a typed name, a body (Manny / Quinn), a class and the class's own pick (D-228): a Refractor picks one
// element (Broken Darkness and Reality among them), a Generic two weapons (a sword and a shield are two picks), a
// Resonator two crystal types (3 of each, for two empty adapters). Every pick starts at F tier.
// B-546 (stage 1, look only): the lists are this stub class table.
// B-554 (stage 3): Begin builds the player from the pick (WoRPlayerBuild) into the first free character slot; the
// Generic and Resonator picks set the gear from stage 4 (OnePlayerCharacterPlan.md).

#pragma once

#include "CoreMinimal.h"
#include "Character/PlayerPicks.h"

class APlayerController;
class UCharacterData;

/** B-554: the screen's pick is the saved picks struct (Character/PlayerPicks.h). */
using FNewGamePick = FPlayerPicks;

/** B-546: one entry of a Refractor's list: an element, or Broken Darkness (Darkness, broken). */
struct FNewGameRefractorChoice
{
	ESpellElement Element = ESpellElement::Darkness;
	bool bBrokenDarkness = false;
};

namespace WoRNewGame
{
	/** D-228: a Generic picks two weapons, a Resonator two crystal types. */
	constexpr int32 MULTI_PICK_COUNT = 2;
	/** D-228: crystals a Resonator starts with of each picked type (a tunable). */
	constexpr int32 STARTING_CRYSTALS_PER_TYPE = 3;
	constexpr int32 MAX_NAME_LENGTH = 16;

	/** Generic, Refractor, Resonator: the order the cards stand in. */
	WORLD_OF_REFRACTION_API const TArray<ECharacterClass> &GetClasses();
	/** The nine elements, in the game's order (Fire first, Reality last). */
	WORLD_OF_REFRACTION_API const TArray<ESpellElement> &GetElements();
	/** B-546: a Refractor's list: the nine elements with Broken Darkness after Darkness. */
	WORLD_OF_REFRACTION_API const TArray<FNewGameRefractorChoice> &GetRefractorChoices();
	/** B-546: every weapon type (Shield included). */
	WORLD_OF_REFRACTION_API const TArray<EWeaponType> &GetWeaponChoices();
	/** B-546: the nine element crystals (Garnet to Iolite). */
	WORLD_OF_REFRACTION_API const TArray<ECrystalType> &GetCrystalChoices();
	/** B-546 (D-229): Generic, Resonator and each Refractor choice: the default most character slots. */
	WORLD_OF_REFRACTION_API int32 GetClassChoiceCount();

	/** B-546: the class's own pick list: its length, an entry's name, and its element when it has one (for the mark). */
	WORLD_OF_REFRACTION_API int32 GetPickChoiceCount(ECharacterClass Class);
	WORLD_OF_REFRACTION_API FText GetPickChoiceName(ECharacterClass Class, int32 Index);
	WORLD_OF_REFRACTION_API bool GetPickChoiceElement(ECharacterClass Class, int32 Index, ESpellElement &OutElement);
	/** B-546: how many the class picks: one for a Refractor, MULTI_PICK_COUNT for the others. */
	WORLD_OF_REFRACTION_API int32 GetPickCount(ECharacterClass Class);
	WORLD_OF_REFRACTION_API bool IsPicked(const FNewGamePick &Pick, int32 Index);
	/** B-546: a Refractor takes the entry; the others toggle it, and a pick past the count drops the oldest. */
	WORLD_OF_REFRACTION_API void TogglePick(FNewGamePick &Pick, int32 Index);
	/** "Element", "Two weapons", "Two crystal types". */
	WORLD_OF_REFRACTION_API FText GetPicksLabel(ECharacterClass Class);
	/** A Generic's or a Resonator's picks in words; empty for a Refractor. */
	WORLD_OF_REFRACTION_API FText GetPicksLine(const FNewGamePick &Pick);
	/** "Broken Darkness", or the element's name ("Unknown" for Reality, D-095). */
	WORLD_OF_REFRACTION_API FText GetRefractorChoiceName(const FNewGameRefractorChoice &Choice);

	/** "Weapons", "Innate magic", "Adapters": the card's corner word. */
	WORLD_OF_REFRACTION_API FText GetClassKind(ECharacterClass Class);
	/** The card's one line, ending with the class's budget ("... Budget 5."). */
	WORLD_OF_REFRACTION_API FText GetClassLine(ECharacterClass Class);
	/** The side panel's title: "Darkness Refractor", "Broken Darkness Refractor", "Generic". */
	WORLD_OF_REFRACTION_API FText GetPanelTitle(const FNewGamePick &Pick);
	/** The side panel's first paragraph. */
	WORLD_OF_REFRACTION_API FText GetAbout(const FNewGamePick &Pick);
	/** What that element's full status bar does, in a line (Rules SB-19 to SB-28). Empty for a non-element. */
	WORLD_OF_REFRACTION_API FText GetElementLine(ESpellElement Element);
	/** The side panel's last line. */
	WORLD_OF_REFRACTION_API FText GetPanelFootnote(const FNewGamePick &Pick);
	/** "Begin as Darkness Refractor", "Begin as a Generic". */
	WORLD_OF_REFRACTION_API FText GetBeginLabel(const FNewGamePick &Pick);
	/** B-546: why Begin can't run yet (no name; a Generic's or Resonator's picks short); empty when it can. */
	WORLD_OF_REFRACTION_API FText GetBeginRefusal(const FNewGamePick &Pick);

	/** The first of Choices that this pick starts as: its class matches and, for a Refractor, its innate element and
	 *  Broken Darkness; a born Broken Darkness lord is only a Broken Darkness Refractor's start. Loads the candidates.
	 *  Null when none matches. */
	WORLD_OF_REFRACTION_API UCharacterData *FindStartCharacter(const TArray<TSoftObjectPtr<UCharacterData>> &Choices,
															  const FNewGamePick &Pick);

	/** Starts the new game as Pick for PC's player: the party is the picked character alone, everything else as a new
	 *  game's, then the hub opens. Empty on success; else why not (nothing changed). */
	WORLD_OF_REFRACTION_API FText Begin(APlayerController *PC, const FNewGamePick &Pick);
}
