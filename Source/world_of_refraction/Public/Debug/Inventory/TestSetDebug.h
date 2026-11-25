// TestSetDebug.h
// B-634 (Crown, 2026-11-25: "i need the save to reflect everything that i need rather than everything as a whole"): the
// current test set, the pieces being worked on, in one list (Config/DefaultGame.ini, [/Script/world_of_refraction.WoRTestSet]).
// - The home shops stock their slice of it (UTrialRunSubsystem::GetMerchantStock): Blacksmith the weapons, Jeweler the
//   adapters, Combat Master the abilities, Spell Shop the spells, Spiritualist the evolutions and gems, Stone Smith the
//   stones. A slice left empty keeps the vendor's own stock; a trial vendor still rolls its own.
// - wor.Debug.TestSet (typed by Crown, never run by an agent on Crown's save) makes the played character's bag, worn gear
//   and learned skills exactly the set, tops every currency to 9999, saves, and logs what it replaced. Gear and items
//   stay in the pool (replaced gear to the home chest, items to storage, owned set pieces used again); learned skills
//   outside the set are overwritten (their names logged; a set skill already known keeps its tier).
// - A Generic wears three infusion sources of three elements (Crown, 2026-11-25): the evolution as its primary (Wind), the
//   Arming Sword and Round Shield with a crystal as its second weapon (Fire), and the adapter with a crystal (Water).
// Names are asset names, found anywhere in the asset registry, so a moved asset still resolves. wor.Debug.PrintTestSet
// lists the set and any name that doesn't resolve (TestSetSpec checks every name).

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Merchant/MerchantData.h"
#include "UObject/Object.h"
#include "TestSetDebug.generated.h"

struct FCombatLoadout;
struct FEvolutionInventoryEntry;
struct FRingInventoryEntry;
struct FWeaponInventoryEntry;
class UWorld;

/** One pouch stack of the set. */
USTRUCT()
struct WORLD_OF_REFRACTION_API FWoRTestSetItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Config, Category = "Test set")
	FCrystalId Crystal;

	UPROPERTY(EditAnywhere, Config, Category = "Test set", meta = (ClampMin = 1))
	int32 Count = 1;
};

UCLASS(Config = Game)
class WORLD_OF_REFRACTION_API UWoRTestSet : public UObject
{
	GENERATED_BODY()

public:
	/** Blacksmith stock and the bag's weapons. */
	UPROPERTY(EditAnywhere, Config, Category = "Test set")
	TArray<FName> Weapons;

	/** Jeweler stock and the bag's spare adapters. */
	UPROPERTY(EditAnywhere, Config, Category = "Test set")
	TArray<FName> Adapters;

	/** Spiritualist stock and the bag's spare evolutions. */
	UPROPERTY(EditAnywhere, Config, Category = "Test set")
	TArray<FName> Evolutions;

	/** Combat Master stock and the learned abilities. */
	UPROPERTY(EditAnywhere, Config, Category = "Test set")
	TArray<FName> Abilities;

	/** Spell Shop stock and the learned spells. */
	UPROPERTY(EditAnywhere, Config, Category = "Test set")
	TArray<FName> Spells;

	/** The pouch; gems also stock the Spiritualist, stones the Stone Smith. */
	UPROPERTY(EditAnywhere, Config, Category = "Test set")
	TArray<FWoRTestSetItem> Items;

	// A Generic's three infusion sources, each a different element.
	UPROPERTY(EditAnywhere, Config, Category = "Generic")
	FName GenericEvolution;

	UPROPERTY(EditAnywhere, Config, Category = "Generic")
	FName GenericWeapon;

	UPROPERTY(EditAnywhere, Config, Category = "Generic")
	FName GenericOffHand;

	UPROPERTY(EditAnywhere, Config, Category = "Generic")
	FCrystalId GenericWeaponCrystal;

	UPROPERTY(EditAnywhere, Config, Category = "Generic")
	FName GenericAdapter;

	UPROPERTY(EditAnywhere, Config, Category = "Generic")
	FCrystalId GenericAdapterCrystal;
};

namespace WoRTestSet
{
	/** The asset of Class named AssetName, or null (logged) when none is, or more than one is. */
	WORLD_OF_REFRACTION_API UObject *FindAsset(const UClass *Class, FName AssetName);

	/** Every name of the set that doesn't resolve to one asset of its kind ("Weapons: DA_X"). */
	WORLD_OF_REFRACTION_API TArray<FString> FindUnresolvedNames(const UWoRTestSet &Set);

	/** The home shop's slice; empty for a vendor the set doesn't stock. */
	WORLD_OF_REFRACTION_API TArray<FMerchantStockEntry> GetStock(const UWoRTestSet &Set, EMerchantType Vendor);

	/** A Generic's three sources through the loadout screen's own drafts and checks: every entry out, the evolution as
	 *  the primary, Weapon (with OffHand, its own sockets) as the second weapon, Adapter in the adapter list. False with
	 *  the rule's reason at the first refusal (Loadout part-changed: call it on a copy, or inside TryChangeGear). */
	WORLD_OF_REFRACTION_API bool WearGenericKit(FCombatLoadout &Loadout, const FEvolutionInventoryEntry &Evolution, const FWeaponInventoryEntry &Weapon,
												const FWeaponInventoryEntry &OffHand, const FRingInventoryEntry &Adapter, FText &OutRefusal);

	/** wor.Debug.TestSet: the played character becomes the set. Returns the report (what it replaced, what it gave). */
	WORLD_OF_REFRACTION_API FString Apply(UWorld *World);

	/** wor.Debug.PrintTestSet. */
	WORLD_OF_REFRACTION_API FString GetTestSetString(const UWoRTestSet &Set);
}
