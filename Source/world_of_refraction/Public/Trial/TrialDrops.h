// TrialDrops.h
// B-569 (D-225, TrialPoolDesign §6 and §9 stage 5, drops): a won fight in a trial drops one thing for each player character.
//   - The candidates: the fight's pool (TrialLoot::GatherPool, what TrialLoot::CanGrant gives) and one crystal entry
//     (TrialPool::PickCrystal: every element's, leaning to the trial's).
//   - What the fight's enemies wore or knew weighs USED_WEIGHT against UNUSED_WEIGHT for the rest (a crystal: when any
//     enemy socketed one).
//   - A piece an enemy wore drops at the tier it wore; anything else at a tier from the zone's range (a skill: its own).
//   - It arrives empty and skips the pickup grade roll (rule ER-38); a prebuilt find arrives as set up (D-231).
// Shortcuts (TODO): apparel isn't given yet (TrialLoot::CanGrant); a boss's first-defeat extra drop from its always-has
// list isn't built; a duplicate takes today's path until stage 6.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Trial/TrialPool.h"

class AActor;
struct FRolledEnemyKit;
struct FTrialDropView;
enum class ESpellElement : uint8;

/** One thing a fight's enemies used: a piece at the tier worn, a skill, or (Item null) a socketed crystal. */
struct FTrialDropUse
{
	const UObject *Item = nullptr;
	EItemTier Tier = EItemTier::F_Tier;
};

struct FTrialDrop
{
	/** Null for a crystal. */
	UObject *Item = nullptr;
	FCrystalId Crystal;
	EItemTier Tier = EItemTier::F_Tier;
	/** An enemy in the fight used it (the weighted part). */
	bool bUsed = false;

	bool IsSet() const { return Item != nullptr || Crystal.Type != ECrystalType::None; }
};

namespace TrialDrops
{
	// TODO: tunables (TrialPoolDesign §6: "3 times likelier, a named number").
	inline constexpr float USED_WEIGHT = 3.0f;
	inline constexpr float UNUSED_WEIGHT = 1.0f;

	/** Each kit's pieces at their tiers, its skills, and its socketed crystals (Item null). */
	WORLD_OF_REFRACTION_API TArray<FTrialDropUse> GatherUsed(TConstArrayView<FRolledEnemyKit> Kits);
	/** USED_WEIGHT when Item is among Used, else UNUSED_WEIGHT (Item null: the crystal entry). */
	WORLD_OF_REFRACTION_API float GetWeight(const UObject *Item, TConstArrayView<FTrialDropUse> Used);
	/** One drop from Pool's grantable things and the crystal entry, by GetWeight; unset when Range is empty and Pool has
	 *  nothing. */
	WORLD_OF_REFRACTION_API FTrialDrop Pick(TConstArrayView<UObject *> Pool, TConstArrayView<FTrialDropUse> Used, ESpellElement TrialElement,
											const FTrialTierRange &Range, FRandomStream &Roll);
	/** Into Owner's inventory: a weapon or adapter at its tier, empty, no grade roll; a crystal to the stock; a skill
	 *  learned; an evolution or prebuilt find as TrialLoot gives it. */
	WORLD_OF_REFRACTION_API bool Grant(AActor *Owner, const FTrialDrop &Drop);
	/** For the results line (WoRTrialPoolLook::GetDroppedLine). */
	WORLD_OF_REFRACTION_API FTrialDropView MakeView(const FTrialDrop &Drop);
	/** "Arming Sword D (used)" with the asset name, for logs and the console. */
	WORLD_OF_REFRACTION_API FString GetDropString(const FTrialDrop &Drop);
}
