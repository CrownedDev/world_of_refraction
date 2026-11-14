// LoadoutApparelModel.h
// B-440f (D-164, CL-50): the loadout screen's apparel rows, without widgets so the specs read it. A (Change) on an
// apparel row lists the member's owned apparel pieces (UPoolSubsystem::GetOwnedApparel, owner = the member) that this
// loadout doesn't already wear; A again wears the lit one in that row's place. X takes the row's piece off (it stays
// owned). Apparel is outside the budget. A lore loadout's apparel waits for B-380e (it isn't saved yet).

#pragma once

#include "CoreMinimal.h"
#include "UI/Menus/LoadoutScreenModel.h"

struct FApparelEntry;

namespace WoRLoadoutApparel
{
	/** Row is an apparel row (worn or empty). */
	WORLD_OF_REFRACTION_API bool IsApparelRow(const FLoadoutScreenRow &Row);

	/** What can go in Row's place: each of Owned not already worn in Loadout (the row's own piece aside); a piece whose
	 *  socket holds what apparel can't take (a stone) is listed with the refusal. Choice.Row.GearID is the piece. */
	WORLD_OF_REFRACTION_API TArray<FLoadoutScreenChoice> BuildApparelChoices(const FCombatLoadout &Loadout, const FLoadoutScreenRow &Row,
																			   const TArray<FApparelEntry> &Owned);

	/** Wears Piece in Row's place: in place of the row's piece, or in a free slot. False when it can't go. */
	WORLD_OF_REFRACTION_API bool PutApparel(FCombatLoadout &Loadout, const FLoadoutScreenRow &Row, const FApparelEntry &Piece);

	/** Takes the row's piece off. False when the row wears nothing. */
	WORLD_OF_REFRACTION_API bool TakeOffApparel(FCombatLoadout &Loadout, const FLoadoutScreenRow &Row);

	/** The member's owned pieces (the shared store's apparel whose owner is the member). */
	WORLD_OF_REFRACTION_API TArray<FApparelEntry> GetMemberApparel(UWorld *World, const FLoadoutScreenMember &Member);

	WORLD_OF_REFRACTION_API TArray<FLoadoutScreenChoice> BuildApparelChoices(UWorld *World, const FLoadoutScreenMember &Member, const FLoadoutScreenRow &Row);

	/** Wears the choice through ULoadoutComponent::TryChangeGear; returns the refusal, empty when it went on. */
	WORLD_OF_REFRACTION_API FText ApplyApparelChoice(UWorld *World, const FLoadoutScreenMember &Member, const FLoadoutScreenRow &Row,
													  const FLoadoutScreenChoice &Choice);

	/** Takes the row's piece off; returns the refusal, empty when it came off. */
	WORLD_OF_REFRACTION_API FText TakeOffRow(UWorld *World, const FLoadoutScreenMember &Member, const FLoadoutScreenRow &Row);

	/** Debug: the member's owned apparel and what the active loadout wears. */
	WORLD_OF_REFRACTION_API FString GetMemberApparelString(UWorld *World, const FLoadoutScreenMember &Member);
}
