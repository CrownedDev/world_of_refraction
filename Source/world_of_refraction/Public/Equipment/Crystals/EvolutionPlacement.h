// EvolutionPlacement.h
// B-357b part 1 (D-147, Q-357b): one resolve path for "an evolution attached to X": the character's own slot (the old
// standalone primary), a weapon (its element socket, D-141) or an adapter. ULoadoutComponent::GatherAttachedEvolutions
// lists them; these rules say what each place gives. On the character an evolution gives everything; on gear it gives
// only its passive skills (its effects), and only to an action that item powers at infusion charge level 2 (nothing
// at L0 / L1). Fight-start effects are gathered at charge 0, so gear evolutions add none there (as before).
// B-357c part 1: gear evolutions stop giving stats and resistance (GivesStats; the pillar layer and the resistance
// sum read the resolve path). Left (B-357d): the L2 action turning the gear evolution's passives on for that action.
// Debug: ULoadoutComponent::GetAttachedEvolutionsString, wor.Loadout.PrintEvolutions.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Crystals/FEvolutionAttachment.h"
#include "EvolutionPlacement.generated.h"

struct FRuntimeAttachedItem;

/** Where an evolution is attached (D-147). */
UENUM(BlueprintType)
enum class EEvolutionPlace : uint8
{
	Character,
	Weapon,
	Adapter
};

/** One evolution and the place it is attached to. */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FAttachedEvolution
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Evolution")
	FEvolutionAttachment Attachment;

	UPROPERTY(BlueprintReadOnly, Category = "Evolution")
	EEvolutionPlace Place = EEvolutionPlace::Character;
};

namespace WoREvolutionPlacement
{
	/** D-147 (Q-357b): the infusion charge level at which a gear evolution's passive skills act. */
	constexpr int32 GEAR_PASSIVES_CHARGE_LEVEL = 2;

	/** Whether an evolution at Place gives its passive skills to an action charged at ChargeLevel (0 = not infused,
	 *  and every fight-start read). The character's own slot always does; gear only at GEAR_PASSIVES_CHARGE_LEVEL. */
	WORLD_OF_REFRACTION_API bool GivesPassives(EEvolutionPlace Place, int32 ChargeLevel);

	/** B-357c (D-147): whether an evolution at Place gives its stats (pillar modifiers) and resistance: only on the
	 *  character's own slot; on gear it gives only its passive skills. */
	WORLD_OF_REFRACTION_API bool GivesStats(EEvolutionPlace Place);

	/** Adds Item's evolution, if it holds one, to Out at Place. False when it holds something else or nothing. */
	WORLD_OF_REFRACTION_API bool AppendFrom(const FRuntimeAttachedItem &Item, EEvolutionPlace Place, TArray<FAttachedEvolution> &Out);

	/** Adds Attachment (the character's own slot) to Out when it has an item. */
	WORLD_OF_REFRACTION_API bool AppendCharacterSlot(const FEvolutionAttachment &Attachment, TArray<FAttachedEvolution> &Out);

	/** The ones in Attached whose passive skills act at ChargeLevel (GivesPassives). */
	WORLD_OF_REFRACTION_API TArray<FAttachedEvolution> FilterPassives(const TArray<FAttachedEvolution> &Attached, int32 ChargeLevel);

	/** "character" / "weapon" / "adapter", for debug lines. */
	WORLD_OF_REFRACTION_API const TCHAR *GetPlaceName(EEvolutionPlace Place);
}
