// GearRepair.h
// B-178 (D-093): paid repair in a run. Wear lives on the party's carried gear between fights
// (FPartyMember::CarriedGear, B-115); a repair at a rest spot brings every worn gem, fusion and
// evolution (a broken evolution included, Q-159c) back to full for Gold, priced by each item's
// tier times its missing share of durability (Q-178). Outside a run there is nothing to repair: the carried gear is cleared when a run starts
// and ends, so the next fight starts from the authored, unworn gear.
// Console: wor.Party.RepairQuote, wor.Party.Repair.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/ItemTier.h"

struct FRuntimeAttachedItem;
struct FEvolutionAttachment;
class UParty;
class UCurrencyComponent;

namespace GearRepair
{
	/** Gold to repair a fully broken item, by tier F..S; a worn one pays its missing share (Q-178).
	 *  TODO: tune. */
	constexpr int32 GOLD_BY_TIER[] = {5, 10, 15, 25, 40, 60, 90};
	static_assert(UE_ARRAY_COUNT(GOLD_BY_TIER) == static_cast<int32>(EItemTier::S_Tier) + 1, "One price per tier");

	WORLD_OF_REFRACTION_API int32 GetGoldForTier(EItemTier Tier);

	/** GetGoldForTier times the missing share of Max, rounded up; 0 when nothing is missing. */
	WORLD_OF_REFRACTION_API int32 GetRepairGold(EItemTier Tier, int32 Current, int32 Max);

	/** Gold to bring this attachment to full; 0 when it is empty, unworn or can't be repaired
	 *  (an augmented fusion has no gem half to repair). */
	WORLD_OF_REFRACTION_API int32 GetRepairCost(const FRuntimeAttachedItem &Item);
	WORLD_OF_REFRACTION_API int32 GetRepairCost(const FEvolutionAttachment &Evolution);

	/** Gold to repair every member's carried gear. */
	WORLD_OF_REFRACTION_API int32 GetPartyRepairCost(const UParty &Party);

	/** Debug: each member's worn items with tier, durability and cost, then the total. */
	WORLD_OF_REFRACTION_API FString GetRepairQuoteString(const UParty &Party);

	/** Pays GetPartyRepairCost in Gold from Wallet and repairs everything to full. False (nothing
	 *  paid or repaired) when there is nothing to repair or the wallet can't afford it. */
	WORLD_OF_REFRACTION_API bool RepairParty(UParty &Party, UCurrencyComponent &Wallet, int32 &OutGoldSpent);
}
