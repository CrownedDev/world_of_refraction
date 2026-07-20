// BattleConfigComponent.h
// Per-encounter runtime placement/ownership for one combatant (Encounter
// Composition, Arc 1).
//
// Set by ABattleGameMode at spawn, read by combat systems. Deliberately NOT on
// UCharacterData: that asset is design-time and immutable, while everything here
// is per-run — the same character in two encounters gets two different configs.
//
// No init-cascade dependency. This component reads nothing from its siblings
// during BeginPlay, so it is order-free and appends last in ACombatCharacter's
// constructor. See docs/Architecture/CombatCharacter.md for why that ordering
// rule exists.

#pragma once

#include "Combat/Grid/FCombatGridPosition.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "BattleConfigComponent.generated.h"

class UParty;

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class WORLD_OF_REFRACTION_API UBattleConfigComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBattleConfigComponent();

	/** Grid cell this combatant occupies. Note FCombatGridPosition carries its own
	 *  TeamIndex — SetGridPosition stamps this component's TeamIndex into it so the
	 *  two cannot disagree. Assign via SetGridPosition, not directly. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle")
	FCombatGridPosition GridPosition;

	/** WEAK: the party is owned by UPartySessionSubsystem and outlives this pawn.
	 *  Null for opposing-side combatants, which belong to no player party. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle")
	TWeakObjectPtr<UParty> OwningParty;

	/** 0 = local-perspective ally, 1 = local-perspective opposing. Authoritative —
	 *  GridPosition.TeamIndex mirrors it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle")
	int32 TeamIndex = 0;

	/** Human-readable provenance for UI, e.g. "Crown's Party — Slot 2". */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle")
	FText DisplayContext;

	/** Stamp ownership + side. Called by ABattleGameMode immediately after spawn,
	 *  before FinishSpawning, so anything reading this at BeginPlay sees it set. */
	UFUNCTION(BlueprintCallable, Category = "Battle")
	void SetBattleContext(UParty *InOwningParty, int32 InTeamIndex, const FText &InDisplayContext);

	/** Assign the grid cell, forcing its TeamIndex to match this component's so the
	 *  two representations cannot drift. */
	UFUNCTION(BlueprintCallable, Category = "Battle")
	void SetGridPosition(const FCombatGridPosition &InGridPosition);
};
