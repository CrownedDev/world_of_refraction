// FApparelEntry.h
// B-380 (D-164): one apparel piece in a loadout: the asset, its owned identity and level, its own crystal socket and
// its instance stat roll. Apparel sits outside the loadout budget (ApparelRules), in at most MAX_APPAREL_SLOTS slots
// (Crown, D-164: the deckbuilder's 2). Its socket takes a refined crystal or an evolution (Crown; no augment stone):
// the crystal keys the piece (GetElement), adds resistance to its element (Crown) and powers no skill; an evolution may sit there too
// (Crown), giving its passive effects. The asset's
// effects count (Crown). The owned store and save are B-380b.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/FEquipmentStatBonus.h"
#include "Equipment/FResistanceBonus.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Inventory/ItemQuality.h"
#include "Inventory/ItemTier.h"
#include "Skills/Definitions/ESpellElement.h"
#include "FApparelEntry.generated.h"

class UApparelData;
class UEvolutionItemData;
struct FCombatLoadout;

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FApparelEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Apparel")
	TObjectPtr<UApparelData> Apparel = nullptr;

	/** The owned instance (B-380b mints it in the owned store); invalid: not an owned piece. */
	UPROPERTY(BlueprintReadOnly, Category = "Identity")
	FGuid PersistentID;

	UPROPERTY(BlueprintReadOnly, Category = "Identity")
	EItemTier Tier = EItemTier::F_Tier;

	UPROPERTY(BlueprintReadOnly, Category = "Identity")
	EItemQuality Quality = EItemQuality::F_Quality;

	/** Its own socket (separate from weapon / adapter crystals): a refined crystal or an evolution. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Apparel")
	FRuntimeAttachedItem AttachedItem;

	/** The instance roll on top of the asset's lines; only HP and Defence count. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Apparel")
	FEquipmentStatBonus StatBonus;

	/** A new entry for Apparel at its authored tier. */
	static FApparelEntry CreateFromApparel(UApparelData *InApparel);

	/** The socketed crystal's or evolution's element, None with nothing (or a broken one) in the socket. */
	ESpellElement GetElement() const;

	/** D-164 (Crown): an evolution in the socket (it gives its passive effects, keys the piece and adds its element's
	 *  resistance like a crystal; it powers no skill); null otherwise. */
	const UEvolutionItemData *GetEvolution() const;

	/** The asset's HP and Defence plus the instance roll's (nothing else counts). */
	FEquipmentStatBonus GetStatLines() const;

	/** D-164 (Crown): the asset's resistance, plus ApparelRules::CRYSTAL_ELEMENT_RESISTANCE to its crystal's element. */
	FResistanceBonus GetResistance() const;
};

namespace ApparelRules
{
	/** Q-380 (the deckbuilder caps apparel at 2). */
	constexpr int32 MAX_APPAREL_SLOTS = 2;

	/** Apparel never takes loadout budget or an item limit (D-164). */
	constexpr int32 BUDGET_COST = 0;

	/** D-164 (Crown, 2026-11-08): a crystal on apparel adds resistance to its element (status buildup, percent like
	 *  FResistanceBonus). TODO: tune with the resistance pass; a third of the per-column cap for now. */
	constexpr float CRYSTAL_ELEMENT_RESISTANCE = 5.0f;

	/** A refined crystal or (Crown, D-164) an evolution sits in an apparel socket; not an augment stone. */
	WORLD_OF_REFRACTION_API bool CanSocket(const FRuntimeAttachedItem &Attachment);

	/** Adds Entry to Loadout's apparel when there is a free slot, it has an asset and (when owned) the same piece is
	 *  not already worn. */
	WORLD_OF_REFRACTION_API bool AddApparel(FCombatLoadout &Loadout, const FApparelEntry &Entry);

	/** Debug: one line per apparel piece in Loadout (asset, element, HP, Defence). */
	WORLD_OF_REFRACTION_API FString GetApparelString(const FCombatLoadout &Loadout);
}
