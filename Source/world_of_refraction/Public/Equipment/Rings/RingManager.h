// RingManager.h
// Simplified ring management for Resonators.
// Stateless query/switching surface backed by ULoadoutComponent
// (single source of truth for ring loadout + active index).
// The crystal-break auto-switch moved to ULoadoutComponent (B-024).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Equipment/Crystals/FBrokenCrystalPayload.h"
#include "RingManager.generated.h"

class URingData;
class UEvolutionItemData;
class UCrystalManager;

/**
 * OBSOLETE (B-024): forwards to the actor's ULoadoutComponent (GetActiveRing,
 * GetActiveRingElement, GetPrimaryRing, SwitchToNextUsableRing), which also owns the
 * crystal-break auto-switch now. No C++ caller left; kept for Blueprints until Crown's
 * PIE pass, then removed (B-154).
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
	/** Unbound since B-024: UCrystalManager::NotifyCrystalBroken calls
	 *  ULoadoutComponent::HandleCrystalBroken instead. Removed with this class (B-154). */
	UFUNCTION()
	void HandleCrystalBroken(AActor *Actor, UObject *Holder, FBrokenCrystalPayload Payload);
};
