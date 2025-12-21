// RingManager.h
// Simplified ring management for Resonators
// Tracks active ring, provides spell list, handles break checks

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ESpellElement.h"
#include "RingManager.generated.h"

class URingData;
class USpellData;

/**
 * Simplified Ring Manager
 * - Tracks which ring is active per actor
 * - Provides spell list from active ring
 * - Handles post-cast break checks
 */
UCLASS()
class WORLD_OF_REFRACTION_API URingManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ==================== LIFECYCLE ====================

	virtual void Initialize(FSubsystemCollectionBase &Collection) override;
	virtual void Deinitialize() override;

	// ==================== RING STATE ====================

	/** Set equipped rings for an actor (call at combat start) */
	UFUNCTION(BlueprintCallable, Category = "Ring Manager")
	void SetEquippedRings(AActor *Actor, const TArray<URingData *> &Rings);

	/** Clear ring state (call at combat end) */
	UFUNCTION(BlueprintCallable, Category = "Ring Manager")
	void ClearRingState(AActor *Actor);

	/** Get active ring */
	UFUNCTION(BlueprintPure, Category = "Ring Manager")
	URingData *GetActiveRing(AActor *Actor) const;

	/** Get active ring's element */
	UFUNCTION(BlueprintPure, Category = "Ring Manager")
	ESpellElement GetActiveElement(AActor *Actor) const;

	/** Get all equipped rings */
	UFUNCTION(BlueprintPure, Category = "Ring Manager")
	TArray<URingData *> GetEquippedRings(AActor *Actor) const;

	// ==================== SPELL ACCESS ====================

	/** Get available spells from active ring */
	UFUNCTION(BlueprintPure, Category = "Ring Manager")
	TArray<USpellData *> GetAvailableSpells(AActor *Actor) const;

	/** Can the active ring cast this spell? */
	UFUNCTION(BlueprintPure, Category = "Ring Manager")
	bool CanCastSpell(AActor *Actor, USpellData *Spell) const;

	// ==================== RING SWITCHING ====================

	/** Switch to ring at index */
	UFUNCTION(BlueprintCallable, Category = "Ring Manager")
	bool SwitchToRing(AActor *Actor, int32 RingIndex);

	/** Switch to next non-broken ring */
	UFUNCTION(BlueprintCallable, Category = "Ring Manager")
	bool SwitchToNextRing(AActor *Actor);

	/** Get count of working (non-broken) rings */
	UFUNCTION(BlueprintPure, Category = "Ring Manager")
	int32 GetWorkingRingCount(AActor *Actor) const;

	// ==================== BREAK SYSTEM ====================

	/** Process break check after casting - returns true if ring broke */
	UFUNCTION(BlueprintCallable, Category = "Ring Manager")
	bool ProcessPostCastBreakCheck(AActor *Actor, USpellData *SpellCast, bool bWasInfused);

	// ==================== DELEGATES ====================

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRingBroken, AActor *, Actor, URingData *, BrokenRing);

	UPROPERTY(BlueprintAssignable, Category = "Ring Manager|Events")
	FOnRingBroken OnRingBroken;

private:
	/** Active ring index per actor */
	TMap<TWeakObjectPtr<AActor>, int32> ActiveRingIndex;

	/** Equipped rings per actor */
	TMap<TWeakObjectPtr<AActor>, TArray<URingData *>> EquippedRings;
};
