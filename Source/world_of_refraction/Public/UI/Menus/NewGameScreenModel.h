// NewGameScreenModel.h
// B-469 (UIScreens #11, mockups screen_mockups/front/new_game/): what the new game screen shows and what Begin does,
// apart from the widget so specs can check it headless.
//
// The pick is a body (Manny / Quinn), a class and, for a Refractor, one of the nine elements. It starts as one of the
// lords (UPartySessionSubsystem::NewGameCharacters): the one whose class, and for a Refractor its innate element,
// match. Begin starts the fresh game in place (USaveSubsystem::StartFreshGame) and goes to the hub.

#pragma once

#include "CoreMinimal.h"
#include "Character/ECharacterClass.h"
#include "Character/Forms/CharacterFormData.h"
#include "Skills/Definitions/ESpellElement.h"

class APlayerController;
class UCharacterData;

struct FNewGamePick
{
	EPlayerBaseForm Body = EPlayerBaseForm::Manny;
	ECharacterClass Class = ECharacterClass::Caster;
	/** Used only by a Refractor. The mockup's default: Darkness. */
	ESpellElement Element = ESpellElement::Darkness;
};

namespace WoRNewGame
{
	/** Generic, Refractor, Resonator: the order the cards stand in. */
	WORLD_OF_REFRACTION_API const TArray<ECharacterClass> &GetClasses();
	/** The nine elements a Refractor can start with, in the game's order (Fire first, Reality last). */
	WORLD_OF_REFRACTION_API const TArray<ESpellElement> &GetElements();

	/** "Weapons", "Innate magic", "Adapters": the card's corner word. */
	WORLD_OF_REFRACTION_API FText GetClassKind(ECharacterClass Class);
	/** The card's one line, ending with the class's budget ("... Budget 5."). */
	WORLD_OF_REFRACTION_API FText GetClassLine(ECharacterClass Class);
	/** The side panel's title: "Darkness Refractor", "Generic". */
	WORLD_OF_REFRACTION_API FText GetPanelTitle(const FNewGamePick &Pick);
	/** The side panel's first paragraph. */
	WORLD_OF_REFRACTION_API FText GetAbout(const FNewGamePick &Pick);
	/** What that element's full status bar does, in a line (Rules SB-19 to SB-28). Empty for a non-element. */
	WORLD_OF_REFRACTION_API FText GetElementLine(ESpellElement Element);
	/** The side panel's last line. */
	WORLD_OF_REFRACTION_API FText GetPanelFootnote(const FNewGamePick &Pick);
	/** "Begin as Darkness Refractor", "Begin as a Generic". */
	WORLD_OF_REFRACTION_API FText GetBeginLabel(const FNewGamePick &Pick);

	/** The first of Choices that this pick starts as: its class matches and, for a Refractor, its innate element (a
	 *  born Broken Darkness lord is never a start). Loads the candidates. Null when none matches. */
	WORLD_OF_REFRACTION_API UCharacterData *FindStartCharacter(const TArray<TSoftObjectPtr<UCharacterData>> &Choices,
															  const FNewGamePick &Pick);

	/** Starts the new game as Pick for PC's player: the party is the picked character alone, everything else as a new
	 *  game's, then the hub opens. Empty on success; else why not (nothing changed). */
	WORLD_OF_REFRACTION_API FText Begin(APlayerController *PC, const FNewGamePick &Pick);
}
