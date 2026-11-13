// FieldSkillUnlockSubsystem.h
// B-214b (Design/FieldAbilities.md "Rank needed", Crown 2026-10-27): the account's field skill unlocks. It keeps, per
// aura (USpellData::FieldAura) and element, the best rank any character has reached with an aura spell of that element;
// a movement type is unlocked for an element once each of its auras has reached its rank there (FieldSkillRules), and
// then anyone with a source of that element can pick it. Ranks only go up: a spell levelled down or given away later
// doesn't lock a type again. Kept on the save (FSavedFieldUnlocks).
// Fed by UEconomyService::LevelUpSpell (a rank-up), the store's skill changes (a learn, a give) and a scan of the
// store's spells on load and on a pick. Per machine: in co-op the host's unlocks decide the host's checks.
// Debug: GetUnlocksString; wor.Field.Unlocks, wor.Field.Unlock <Type> <Element> (FieldMovementDebug).

#pragma once

#include "CoreMinimal.h"
#include "Field/FieldSkillTypes.h"
#include "Save/WoRSaveTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FieldSkillUnlockSubsystem.generated.h"

class USpellData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFieldSkillUnlocked, EFieldMovementType, Type, ESpellElement, Element);

UCLASS()
class WORLD_OF_REFRACTION_API UFieldSkillUnlockSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase &Collection) override;

	/** Records Spell at Tier when it's an aura spell of a real element. True when it raised a rank; each type that
	 *  became unlocked is logged and broadcast. */
	bool RecordSpellTier(const USpellData *Spell, EItemTier Tier);

	/** Records every spell in the store (each character's learned spells and the owned list). Returns how many ranks
	 *  rose. */
	int32 RecordFromPool();

	/** The best rank reached for Aura in Element (0 = never). */
	UFUNCTION(BlueprintPure, Category = "Field")
	int32 GetBestRank(EFieldAura Aura, ESpellElement Element) const;

	UFUNCTION(BlueprintPure, Category = "Field")
	bool IsUnlocked(EFieldMovementType Type, ESpellElement Element) const;

	/** Every type a source of Element may use, the dash first. */
	UFUNCTION(BlueprintPure, Category = "Field")
	TArray<EFieldMovementType> GetUnlockedTypes(ESpellElement Element) const;

	/** Debug: raises Type's auras in Element to its rank. False when that element can't unlock it (Teleport: Void). */
	bool UnlockForDebug(EFieldMovementType Type, ESpellElement Element);

	/** Save (D-059). Restore replaces the ranks, then scans the store (restored first) for spells already ranked. */
	FSavedFieldUnlocks Capture() const;
	void Restore(const FSavedFieldUnlocks &Saved);

	/** "Enhancement Lightning rank 3 | ... || unlocked: Lightning EnhancedSprint, ..." */
	FString GetUnlocksString() const;

	UPROPERTY(BlueprintAssignable, Category = "Field")
	FOnFieldSkillUnlocked OnFieldSkillUnlocked;

private:
	UFUNCTION()
	void HandlePoolSkillsChanged(FName CharacterKey);

	/** Sets the rank if higher; logs and broadcasts the types it unlocked. True when it rose. */
	bool RaiseRank(EFieldAura Aura, ESpellElement Element, int32 Rank);

	static uint16 MakeKey(EFieldAura Aura, ESpellElement Element);

	/** MakeKey(Aura, Element) -> best rank reached. */
	TMap<uint16, int32> BestRanks;
};
