// WeaponSpellRules.h
// B-098 (D-027, Q-098): weapons never give spells. A crystal on a weapon makes its abilities elemental (the infusion
// source, B-080), it no longer offers its spells (the old Resonate Weapon spell list). Every read of a weapon's
// spells asks WeaponsGiveSpells() first: FWeaponLoadoutEntry::GetAllSpells / GetSpellSlots / ReplaceSpell,
// FCombatCapabilities::BuildFrom, ULoadoutComponent::GetWeaponResonateSpells.
// The spells a weapon already has in its data or a save stay where they are, unread, so old saves load and the
// switch can bring them back: wor.Loadout.WeaponSpells 1 is the old behaviour, for a PIE comparison only, removed
// with the weapon spell fields in the sweep (Sweep row 48).
// Debug: wor.Loadout.ListWeaponSpells lists every owned weapon that still holds spells (the B-098 survey).

#pragma once

#include "CoreMinimal.h"

namespace WeaponSpellRules
{
	/** D-027: false (weapons give no spells) unless wor.Loadout.WeaponSpells is 1 (the old behaviour, PIE only). */
	WORLD_OF_REFRACTION_API bool WeaponsGiveSpells();
}
