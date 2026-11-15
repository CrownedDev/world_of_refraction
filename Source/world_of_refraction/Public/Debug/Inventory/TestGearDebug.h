// TestGearDebug.h
// B-503n (Q-503n): test gear for the field wheel and the v2 loadout screen, given through the real API.
// wor.Debug.TestGear [fill] [refractor|resonator|bd|reality]
//   (always) the played character gets a plain sword (AddWeapon), a ring (AddRing) and one Fire, Water and
//            Lightning crystal in its pouch (CarryOrStore); what it already owns is not given again.
//   fill     builds the loadout as the v2 screen does: the sword and the ring go in as entries (TryChangeGear +
//            ApplyDraft), then the crystals go in the empty sockets (ApplySocketChoice).
//   variant  sets the character's class and innate on its CharacterData in memory (restored when PIE ends) and
//            takes every entry but the first out first. bd also turns Broken Darkness on.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Crystals/CrystalType.h"
#include "Skills/Definitions/ESpellElement.h"
#include "UI/Menus/LoadoutScreenModel.h"

namespace WoRTestGear
{
	/** The crystal the quick fill puts in a row's empty crystal socket: a weapon takes Fire, then Lightning, then Water; an
	 *  adapter takes Water first. The first of those that is Offered and whose element no socket of the loadout has yet
	 *  (Taken); None when nothing fits. */
	WORLD_OF_REFRACTION_API ECrystalType PickTestCrystal(ELoadoutScreenRowKind Kind, const TArray<ECrystalType> &Offered, const TSet<ESpellElement> &Taken);

	/** Runs wor.Debug.TestGear on the played character; returns what was given and filled, one line per step. */
	WORLD_OF_REFRACTION_API FString GrantTestGear(UWorld *World, bool bFill, const FString &Variant);
}
