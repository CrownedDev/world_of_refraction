// WoRIconTable.h
// B-485 (Crown, 2026-11-14; mockups menus/loadout/icons): the UI's small icons in one data asset: an element badge per
// element (a crystal in a socket), the empty socket, a tier badge F to S, a weapon-type icon and an effect-kind icon.
// Every field is a soft reference (D-057): a texture loads when a screen first shows it. One asset is the active
// table (GetActive, a fixed path as UCombatTuningData does); with no asset, or an icon left unset, a getter returns
// null and the screen keeps its text. Filled by the editor step B-486.
// Crown, 2026-11-14: a crystal's badge is its element's (the diamond badges are placeholders). Ice has no crystal of
// its own: here Ice is a hit kind keyed to Water, not an ESpellElement, so an Ice crystal's badge is Water's. An effect
// keeps its own icon: by its type, and in an element's colour where the table has that variant.
// Debug: wor.UI.Icons.Print, and the Print icons button in the asset's Details panel.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Equipment/Weapons/EWeaponType.h"
#include "Inventory/ItemTier.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Skills/Effects/ESkillEffectType.h"
#include "UI/WoRIcon.h"
#include "WoRIconTable.generated.h"

class UTexture2D;
struct FCrystalId;
enum class EItemEffectType : uint8;

/** An effect kind's icon: one for any element, and its variants in an element's colour ("Fire Amp", "Water resistance"). */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FWoREffectIcons
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
	TSoftObjectPtr<UTexture2D> Default;

	/** Used for an effect keyed to that element; an element without one here takes Default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
	TMap<ESpellElement, TSoftObjectPtr<UTexture2D>> ByElement;
};

UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UWoRIconTable : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** A crystal (or evolution) of this element in a socket. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sockets")
	TMap<ESpellElement, TSoftObjectPtr<UTexture2D>> ElementBadges;

	/** B-584 (D-256): a crystal (or evolution) of this element in no socket: no frame. Unset: the element's badge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sockets")
	TMap<ESpellElement, TSoftObjectPtr<UTexture2D>> LooseGems;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sockets")
	TSoftObjectPtr<UTexture2D> EmptySocket;

	/** An augment stone in a socket (it has no element). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sockets")
	TSoftObjectPtr<UTexture2D> StoneSocket;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tiers")
	TMap<EItemTier, TSoftObjectPtr<UTexture2D>> TierBadges;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapons")
	TMap<EWeaponType, TSoftObjectPtr<UTexture2D>> WeaponTypes;

	/** The icon beside an effect line, by the effect's first payload's type (and its element, where there is a variant). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
	TMap<ESkillEffectType, FWoREffectIcons> EffectKinds;

	/** The active table (the asset at the fixed path), loaded on first use; null when there is none. */
	static const UWoRIconTable *GetActive();

	// Each getter loads its texture on demand; null when the icon isn't set (the caller keeps its text).
	UTexture2D *GetElementBadge(ESpellElement Element) const;
	UTexture2D *GetLooseGem(ESpellElement Element) const;
	UTexture2D *GetEmptySocket() const;
	UTexture2D *GetStoneSocket() const;
	UTexture2D *GetTierBadge(EItemTier Tier) const;
	UTexture2D *GetWeaponType(EWeaponType Type) const;
	UTexture2D *GetEffectKind(ESkillEffectType Type, ESpellElement Element = ESpellElement::None) const;

	/** Debug: which icons are set, one line per group ("Tiers: 5 of 7 set: F E D C B; missing: A S"). */
	FString GetIconTableString() const;

	UFUNCTION(CallInEditor, Category = "Debug")
	void PrintIcons() const;
};

/** The active table's icons, null-safe: each returns null with no table or no icon. */
namespace WoRIcons
{
	WORLD_OF_REFRACTION_API UTexture2D *GetElementBadge(ESpellElement Element);
	WORLD_OF_REFRACTION_API UTexture2D *GetLooseGem(ESpellElement Element);
	WORLD_OF_REFRACTION_API UTexture2D *GetEmptySocket();
	WORLD_OF_REFRACTION_API UTexture2D *GetStoneSocket();
	WORLD_OF_REFRACTION_API UTexture2D *GetTierBadge(EItemTier Tier);
	WORLD_OF_REFRACTION_API UTexture2D *GetWeaponType(EWeaponType Type);
	WORLD_OF_REFRACTION_API UTexture2D *GetEffectKind(ESkillEffectType Type, ESpellElement Element = ESpellElement::None);

	/** B-487: the texture for what Icon stands for; null for none (the caller keeps its text). An adapter's is its frame. */
	WORLD_OF_REFRACTION_API UTexture2D *Find(const FWoRIcon &Icon);

	/** B-530 (D-229): what sits in an icon's centre, over Find's texture: an adapter's crystal badge; null otherwise. */
	WORLD_OF_REFRACTION_API UTexture2D *FindCentre(const FWoRIcon &Icon);

	/** B-530 (D-229): an adapter's icon: the frame, with SocketElement's badge inside (None: the centre stays empty). */
	WORLD_OF_REFRACTION_API FWoRIcon MakeAdapterIcon(ESpellElement SocketElement, const FText &Name);

	/** B-584 (D-256): Icon as drawn out of a socket: a crystal's badge becomes the gem alone; any other icon unchanged. */
	WORLD_OF_REFRACTION_API FWoRIcon AsLoose(FWoRIcon Icon);

	/** B-487: the effect kind an item's use counts as, for the icon beside its effect sentence (a Garnet's burn is
	 *  damage over time, a Sapphire's is the last stand); None for a use with no icon kind. */
	WORLD_OF_REFRACTION_API ESkillEffectType GetItemEffectKind(EItemEffectType Type);

	/** B-487: the icon of a crystal's or stone's effect: its kind, in the item's element. */
	WORLD_OF_REFRACTION_API FWoRIcon MakeItemEffectIcon(const FCrystalId &Item);

	/** Specs: use Table as the active one (null: back to the asset). */
	WORLD_OF_REFRACTION_API void SetActiveForTests(const UWoRIconTable *Table);
}
