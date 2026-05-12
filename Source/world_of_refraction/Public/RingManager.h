// RingManager.h
// Simplified ring management for Resonators
// Tracks active ring, provides spell list, performs auto-switch on break

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ESpellElement.h"
#include "RingManager.generated.h"

class URingData;
class USpellData;
class ULoadoutComponent;
class UItemData;
class UCrystalManager;

/**
 * Simplified Ring Manager
 * - Tracks which ring is active per actor
 * - Provides spell list from active ring
 * - Consumes UCrystalManager::OnCrystalBroken, performs auto-switch
 *   when the broken crystal is on a ring this manager tracks
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

	/** Consumer of UCrystalManager::OnCrystalBroken. Filters by
	 *  Cast<URingData>(Holder); performs SwitchToNextRing on match. */
	UFUNCTION()
	void HandleCrystalBroken(AActor *Actor, UObject *Holder, UItemData *Crystal);
};
