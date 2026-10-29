// SourceBreakRules.h
// D-083: what a skill can still do when its source's crystal breaks mid-fight. Broken skills stay
// in the menu, greyed out with one of these reasons, and the executor refuses them.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/ItemTier.h"

namespace SourceBreakRules
{
	/** A ring whose crystal broke still casts spells of this tier. */
	constexpr EItemTier BROKEN_RING_SPELL_TIER = EItemTier::F_Tier;

	inline bool IsRingSpellUsableWhenBroken(EItemTier SpellTier)
	{
		return SpellTier == BROKEN_RING_SPELL_TIER;
	}

	inline FText BrokenWeaponCrystalReason()
	{
		return NSLOCTEXT("SourceBreak", "BrokenWeaponCrystal", "Source broken: the weapon's crystal is broken.");
	}

	inline FText BrokenEvolutionReason()
	{
		return NSLOCTEXT("SourceBreak", "BrokenEvolution", "Source broken: the evolution crystal is broken.");
	}

	inline FText BrokenRingCrystalReason()
	{
		return NSLOCTEXT("SourceBreak", "BrokenAdapterCrystal", "Source broken: this adapter casts F-tier spells only.");
	}
}
