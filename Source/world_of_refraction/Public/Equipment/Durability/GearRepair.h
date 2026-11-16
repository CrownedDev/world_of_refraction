// GearRepair.h
// B-178 (D-093): paid repair in a run. Wear lives on the party's carried gear between fights
// (FPartyMember::CarriedGear, B-115); a repair at a rest spot brings every worn gem, fusion and
// evolution (a broken evolution included, Q-159c) back to full for Gold, priced by each item's
// tier times its missing share of durability (Q-178). Outside a run there is nothing to repair: the carried gear is cleared when a run starts
// and ends, so the next fight starts from the authored, unworn gear.
// Console: wor.Party.RepairQuote, wor.Party.Repair.
// B-534 (D-231, Q-534): Restore, a locked prebuilt's broken gem brought back with crystals of its own type.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Inventory/ItemTier.h"

struct FRuntimeAttachedItem;
struct FEvolutionAttachment;
class UParty;
class UCurrencyComponent;
class UCrystalInventoryComponent;

namespace GearRepair
{
	/** B-534: what a Restore would spend and the durability the gem would end on. Uses is empty when nothing can be spent. */
	struct FRestorePlan
	{
		TArray<FCrystalId> Uses;
		int32 NewDurability = 0;
		int32 MaxDurability = 0;
	};

	/** B-534 (D-231 "Breaking, restoring and the word"): Restore tops up any socketed gem below full (not an evolution,
	 *  which is repaired, Q-159); at 0 only a gem locked to its piece (bLocked: GearSockets::IsCrystalFixed), since an
	 *  unlocked one is destroyed at fight end. */
	WORLD_OF_REFRACTION_API bool CanRestore(const FRuntimeAttachedItem &Gem, bool bLocked);

	/** B-534 (D-231 "Restore amounts", Q-534): crystals of the gem's type from Pouch, cheapest first, until it is full.
	 *  One crystal restores value(its tier) / value(the gem's tier) of full durability (EconomyYield::GetCrystalValue,
	 *  the merge values): the same tier or higher fills it, the extra is lost; one tier down a half (a third below S).
	 *  Not enough: as far as they go. No money is charged. */
	WORLD_OF_REFRACTION_API FRestorePlan PlanRestore(const FRuntimeAttachedItem &Gem, const UCrystalInventoryComponent &Pouch);

	/** Debug: the plan in one line ("Garnet C: F x3, E x1 -> 42 / 60"). */
	WORLD_OF_REFRACTION_API FString GetRestorePlanString(const FRuntimeAttachedItem &Gem, const FRestorePlan &Plan);

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

	/** B-456d: one worn piece of the party's carried gear, as the Spiritualist's Repair tab lists it. */
	struct FWornPiece
	{
		int32 MemberIndex = INDEX_NONE;
		/** Its place in the member's CarriedGear.Attachments; INDEX_NONE is the primary evolution. */
		int32 AttachmentIndex = INDEX_NONE;
		int32 Cost = 0;
	};

	/** Every carried piece a repair charges for (GetRepairCost above 0), in member order. */
	WORLD_OF_REFRACTION_API TArray<FWornPiece> GatherWornPieces(const UParty &Party);

	/** Gold to repair every member's carried gear. */
	WORLD_OF_REFRACTION_API int32 GetPartyRepairCost(const UParty &Party);

	/** B-456d: why Wallet can't pay Cost for a repair, in words for the player; empty when it can. The payment's
	 *  own checks (RepairParty and RepairPiece run them), so a greyed row can't disagree with the repair. */
	WORLD_OF_REFRACTION_API FText GetRefusal(const UCurrencyComponent *Wallet, int32 Cost);

	/** Debug: each member's worn items with tier, durability and cost, then the total. */
	WORLD_OF_REFRACTION_API FString GetRepairQuoteString(const UParty &Party);

	/** Pays GetPartyRepairCost from Wallet and repairs everything to full. B-366e (Q-366 extended): paid in the
	 *  currency of the place (UEconomyService::GetPlaceCurrency: hub money at home, run money in a run). False
	 *  (nothing paid or repaired) when there is nothing to repair or that balance can't afford it. */
	WORLD_OF_REFRACTION_API bool RepairParty(UParty &Party, UCurrencyComponent &Wallet, int32 &OutGoldSpent);

	/** B-456d: pays for and repairs the one piece, as RepairParty does all of them; its price is read fresh. False
	 *  (nothing paid or repaired) when it isn't worn any more or GetRefusal refuses. */
	WORLD_OF_REFRACTION_API bool RepairPiece(UParty &Party, const FWornPiece &Piece, UCurrencyComponent &Wallet, int32 &OutGoldSpent);
}
