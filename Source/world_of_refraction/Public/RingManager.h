// RingManager.h
// Simplified ring management for Resonators.
// Stateless query/switching surface backed by ULoadoutComponent
// (single source of truth for ring loadout + active index).
// Consumes UCrystalManager::OnCrystalBroken, performs auto-switch.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ESpellElement.h"
#include "RingManager.generated.h"

class URingData;
class UEvolutionItemData;
class UCrystalManager;

/**
 * Simplified Ring Manager
 * - Provides active-ring queries sourced from LoadoutComponent
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

	// ==================== RING QUERIES ====================

	/** Get active ring */
	UFUNCTION(BlueprintPure, Category = "Ring Manager")
	URingData *GetActiveRing(AActor *Actor) const;

	/** Get active ring's element */
	UFUNCTION(BlueprintPure, Category = "Ring Manager")
	ESpellElement GetActiveElement(AActor *Actor) const;

	/** Get primary ring (Generic/Caster with ring in primary slot) */
	UFUNCTION(BlueprintPure, Category = "Ring Manager")
	URingData *GetPrimaryRing(AActor *Actor) const;

	// ==================== RING SWITCHING ====================

	/** Switch to next non-broken ring */
	UFUNCTION(BlueprintCallable, Category = "Ring Manager")
	bool SwitchToNextRing(AActor *Actor);

private:
	/** Consumer of UCrystalManager::OnCrystalBroken. Filters by
	 *  Cast<URingData>(Holder); performs SwitchToNextRing on match. */
	UFUNCTION()
	void HandleCrystalBroken(AActor *Actor, UObject *Holder, UEvolutionItemData *Crystal);
};
