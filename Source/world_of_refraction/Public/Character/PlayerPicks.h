// PlayerPicks.h
// B-554 (D-228, one-player stage 3): what a player chose at the new-game screen, saved with the player's party member and
// locked for the game (D-227). The player's character data is built from these (WoRPlayerBuild), not from a character
// asset of its own: the typed name, the Starting spirit (D-234), the class, a Refractor's element (Broken Darkness and
// Reality among them), a Generic's two weapons (a sword and a shield are two picks), a Resonator's two crystal types.
// The Generic's and Resonator's picks set the starting gear from stage 4 (OnePlayerCharacterPlan.md section 3).

#pragma once

#include "CoreMinimal.h"
#include "Character/ECharacterClass.h"
#include "Character/Forms/CharacterFormData.h"
#include "Equipment/Crystals/CrystalType.h"
#include "Equipment/Weapons/EWeaponType.h"
#include "Skills/Definitions/ESpellElement.h"
#include "PlayerPicks.generated.h"

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FPlayerPicks
{
	GENERATED_BODY()

	/** Typed at the new-game screen (D-228 Q6). Empty: no picks (a character that has its own asset). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Picks")
	FString Name;

	/** The Starting spirit: Male Nobody (Manny) or Female Nobody (Quinn), D-234. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Picks")
	EPlayerBaseForm Body = EPlayerBaseForm::Manny;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Picks")
	ECharacterClass Class = ECharacterClass::Caster;

	/** Used only by a Refractor. The mockup's default: Darkness. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Picks")
	ESpellElement Element = ESpellElement::Darkness;

	/** A Refractor's Broken Darkness (Element is then Darkness). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Picks")
	bool bBrokenDarkness = false;

	/** A Generic's two weapons, oldest first. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Picks")
	TArray<EWeaponType> Weapons;

	/** A Resonator's two crystal types, oldest first. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Picks")
	TArray<ECrystalType> Crystals;

	/** B-557 (D-238): a hub recruit's fixed picks (UClassDefaultLoadoutData::Recruits), built under its own name. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Picks")
	bool bRecruit = false;

	/** B-561 (stage 5): a co-op guest's character, built on the host under a guest name of its own (never the host's). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Picks")
	bool bGuest = false;

	/** A built player: these picks make its character data. */
	bool IsSet() const { return !Name.IsEmpty(); }
};
