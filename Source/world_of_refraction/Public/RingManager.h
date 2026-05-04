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
class ULoadoutComponent;
class UItemData;

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

	// ==================== DURABILITY ====================

	/** Apply wear to the slotted crystal of the active ring after a spell cast.
	 *  Returns the wear amount applied. If wear breaks the crystal, fires
	 *  OnCrystalBroken (handled internally to auto-switch ring). */
	UFUNCTION(BlueprintCallable, Category = "Ring Manager")
	int32 ProcessPostCastWear(AActor *Actor, USpellData *SpellCast, int32 InfusionLevel);

	// ==================== DELEGATES ====================

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnRingCrystalBroken, AActor *, Actor, URingData *, Ring, UItemData *, Crystal);

	/** Fires when a ring's slotted crystal hits 0 durability and breaks. */
	UPROPERTY(BlueprintAssignable, Category = "Ring Manager|Events")
	FOnRingCrystalBroken OnRingCrystalBroken;

	/** Initialize rings from LoadoutComponent */
	UFUNCTION(BlueprintCallable, Category = "Ring Manager")
	void InitializeFromLoadout(AActor *Actor, ULoadoutComponent *LoadoutComp);

	/** Get primary ring (Generic/Caster with ring in primary slot) */
	UFUNCTION(BlueprintPure, Category = "Ring Manager")
	URingData *GetPrimaryRing(AActor *Actor) const;

private:
	/** Active ring index per actor */
	TMap<TWeakObjectPtr<AActor>, int32> ActiveRingIndex;

	/** Equipped rings per actor */
	TMap<TWeakObjectPtr<AActor>, TArray<URingData *>> EquippedRings;

	/** Subscribe to OnCrystalBroken for every slotted crystal of an actor's rings. */
	void SubscribeToActorCrystals(AActor *Actor);

	/** Unsubscribe from OnCrystalBroken for every slotted crystal of an actor's rings. */
	void UnsubscribeFromActorCrystals(AActor *Actor);

	/** Handler bound to UItemData::OnCrystalBroken delegate. */
	UFUNCTION()
	void HandleCrystalBroken(UItemData *BrokenCrystal);

	/** Find which actor and ring own a given crystal (for the broadcast). */
	bool FindOwnerOfCrystal(UItemData *Crystal, AActor *&OutActor, URingData *&OutRing) const;

	/** Check if any other actor in the EquippedRings map has the same crystal asset.
	 *  Logs a warning if found. Called from SetEquippedRings. */
	void WarnIfCrystalSharedAcrossActors(AActor *Actor) const;
};