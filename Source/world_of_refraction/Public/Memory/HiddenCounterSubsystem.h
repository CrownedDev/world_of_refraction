// HiddenCounterSubsystem.h
// B-379 (MemoryCatalogue §3.9; SLF's hidden stats): this player's hidden running totals, kept on the save across runs
// ("parried 50 times as Sapphire"). Each counter keeps a total per character and one for all characters. A memory's
// CounterReached condition reads them (FWoRConditionContext::Counters); reaching a milestone opens it at the next
// memory event. Counted today: Parries (a Perfect Guard) and Kills, from ABattleGameMode; a co-op friend's own
// character's count goes to their machine (AWoRPlayerState::Client_AddCounter).
// Debug: GetCountersString; wor.Counter.Print, wor.Counter.Add <Counter> <Amount>.

#pragma once

#include "CoreMinimal.h"
#include "Save/WoRSaveTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HiddenCounterSubsystem.generated.h"

class UCharacterData;

namespace HiddenCounters
{
	/** A Perfect Guard (a parry, D-084) by the character. */
	inline constexpr const TCHAR *PARRIES = TEXT("Parries"); // TCHAR, not FName: no FName at static init
	/** A finishing blow by the character. */
	inline constexpr const TCHAR *KILLS = TEXT("Kills");
}

UCLASS()
class WORLD_OF_REFRACTION_API UHiddenCounterSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Adds Amount to Counter for Character (and to the all-characters total). Null Character: the total only. */
	void Add(FName Counter, const UCharacterData *Character, int32 Amount = 1);

	/** Counter's total for Character; null Character: for all characters together. */
	int32 Get(FName Counter, const UCharacterData *Character) const;

	/** Save (D-059). */
	FSavedCounters Capture() const;
	void Restore(const FSavedCounters &Saved);

	FString GetCountersString() const;

	/** The key a character's count is kept under (its asset name; None: all characters). */
	static FName KeyFor(const UCharacterData *Character);

private:
	/** Counter -> character key (None: all characters) -> count. */
	TMap<FName, TMap<FName, int32>> Counts;
};
