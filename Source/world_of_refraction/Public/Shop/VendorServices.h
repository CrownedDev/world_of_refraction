// VendorServices.h
// B-445 (D-177, D-193; changes D-131): selling and dismantling are done at a vendor, not from the inventory.
//   Sell       every vendor with a shop screen (Blacksmith, Jeweler, Spiritualist) buys a member's weapons, adapters and
//              pouch crystals / stones for half the tier's buy price (Q-445, the duplicate convert's rule), in the
//              place's currency.
//   Dismantle  the Blacksmith takes apart gear and crystals; the Spiritualist evolutions. Hub only, never during a run;
//              what is dismantled is gone until it is found again.
// Plain functions, so the placeholder (wor.UI.Vendor, UVendorServiceWidget), the vendor screen to come (B-456) and the
// specs route through the same code. They act through UEconomyService on the member's spawned actor.
// Service names: WoRGameNames::GetVendorServiceName (D-062). Debug: GetRowsString, wor.Vendor.PrintServices.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Inventory/ItemTier.h"
#include "Merchant/MerchantData.h"

class UWorld;

enum class EVendorService : uint8
{
	Sell,
	Dismantle
};

namespace WoRVendorServices
{
	/** One thing a vendor can take: a weapon or adapter (GearID), an owned evolution (EvolutionID), or a pouch stack. */
	struct WORLD_OF_REFRACTION_API FVendorRow
	{
		FGuid GearID;
		FGuid EvolutionID;
		FCrystalId Item;
		int32 Count = 0;
		EItemTier Tier = EItemTier::F_Tier;
		FText Label;

		bool IsGear() const { return GearID.IsValid(); }
		bool IsEvolution() const { return EvolutionID.IsValid(); }
	};

	/** A trial run is under way (its gear layer is open or the run is active): dismantling is refused then (D-193). */
	WORLD_OF_REFRACTION_API bool IsInRun(const UWorld *World);

	/** D-193: whether Vendor does Service at all. */
	WORLD_OF_REFRACTION_API bool Offers(EMerchantType Vendor, EVendorService Service);

	/** What Vendor takes from Member for Service: gear and pouch stacks, or (the Spiritualist's Dismantle) evolutions. */
	WORLD_OF_REFRACTION_API TArray<FVendorRow> GatherRows(UWorld *World, EMerchantType Vendor, EVendorService Service, FName Member);

	/** Why Service can't run on Row, for the player; empty when it can. */
	WORLD_OF_REFRACTION_API FText GetRefusal(UWorld *World, EMerchantType Vendor, EVendorService Service, FName Member, const FVendorRow &Row);

	/** What one of Row gives: "100 Gold", "30 Gear Essence". */
	WORLD_OF_REFRACTION_API FText GetPayText(UWorld *World, EVendorService Service, const FVendorRow &Row);

	/** What the player confirms before Service runs (the pay, that a dismantle can't be undone, D-016's warning for a
	 *  worn piece). */
	WORLD_OF_REFRACTION_API FText GetConfirmText(UWorld *World, EVendorService Service, FName Member, const FVendorRow &Row);

	/** Runs Service on Row (Count for stacks, clamped to the row's). Empty on success; else the reason, for the player. */
	WORLD_OF_REFRACTION_API FText Run(UWorld *World, EMerchantType Vendor, EVendorService Service, FName Member, const FVendorRow &Row, int32 Count);

	/** Debug: Vendor's services and, for each, Member's rows with what they give or why they are refused. */
	WORLD_OF_REFRACTION_API FString GetRowsString(UWorld *World, EMerchantType Vendor, FName Member);
}
