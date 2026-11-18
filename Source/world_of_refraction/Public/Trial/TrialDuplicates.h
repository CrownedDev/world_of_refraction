// TrialDuplicates.h
// B-570 (D-225, TrialPoolDesign §6, stage 6): a dropped weapon the player already has one of.
//   - The drop is added first: Add (keep both) is the no-loss default (Q-565), so nothing waits on the prompt.
//   - The prompt (UDuplicateDropWidget) opens back on the trial map, where the root layout is.
//   - Replace: the new one takes the held one's loadout places; the held one stays in the inventory, unworn
//     (taking gear apart is the Blacksmith's, D-177).
//   - Convert: the new one is dismantled for Gear essence.
// A known skill can't be held twice: it converts to Skill essence at the drop (Q-570).
// Debug: wor.Trial.PrintDuplicates, wor.Trial.NextDuplicate.

#pragma once

#include "CoreMinimal.h"
#include "Trial/TrialPoolLook.h"

class AActor;
class APlayerController;
class UInventoryComponent;
class UPoolSubsystem;
class USkillDataBase;
class UTrialRunSubsystem;
class UWeaponData;
class UWorld;

/** A duplicate waiting for its choice: the copy just dropped and the one the character had, by owned-instance id. */
struct WORLD_OF_REFRACTION_API FTrialDuplicate
{
	FName CharacterKey;
	FGuid NewID;
	FGuid HeldID;

	bool IsSet() const { return !CharacterKey.IsNone() && NewID.IsValid() && HeldID.IsValid(); }
};

namespace TrialDuplicates
{
	/** Inventory's copy of Weapon that isn't ExceptID (the copy just dropped); invalid when it has no other. */
	WORLD_OF_REFRACTION_API FGuid FindHeldWeapon(const UInventoryComponent &Inventory, const UWeaponData *Weapon, const FGuid &ExceptID);

	/** What the prompt shows, read from the store's two pieces. False when either is gone (given away, dismantled). */
	WORLD_OF_REFRACTION_API bool MakeView(const UPoolSubsystem &Pool, const FTrialDuplicate &Duplicate, FDuplicateDropView &OutView);

	/** Acts on Choice through the member's spawned actor. Empty on success (Add always is); else the reason, for the player. */
	WORLD_OF_REFRACTION_API FText Resolve(UWorld *World, const FTrialDuplicate &Duplicate, EDuplicateChoice Choice);

	/** Pays Receiver the Skill essence a dropped skill it already knows converts to; 0 when it can't be paid. */
	WORLD_OF_REFRACTION_API int32 ConvertKnownSkill(AActor *Receiver, const USkillDataBase *Skill);
	/** "Dropped: Lunge (E): already known, 12 Skill essence". */
	WORLD_OF_REFRACTION_API FText GetConvertedLine(const FTrialDropView &Drop, int32 Essence);

	/** Opens the prompt for the run's next waiting duplicate on PC's root layout; the choice resolves it and opens the
	 *  next. A duplicate whose pieces are gone is skipped. False when none is waiting or the prompt can't open. */
	WORLD_OF_REFRACTION_API bool OpenNextPrompt(APlayerController *PC);

	WORLD_OF_REFRACTION_API FString GetDuplicatesString(const UTrialRunSubsystem &TrialRun, const UPoolSubsystem *Pool);
}
