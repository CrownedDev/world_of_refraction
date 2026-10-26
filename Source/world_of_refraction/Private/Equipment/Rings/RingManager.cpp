// RingManager.cpp
// OBSOLETE (B-024): every query and the ring switch now live on ULoadoutComponent, and the
// crystal-break auto-switch moved there too (ULoadoutComponent::HandleCrystalBroken).
// URingManager only forwards, for Blueprints that may still call it, until Crown's PIE pass.

#include "Equipment/Rings/RingManager.h"
#include "Loadout/LoadoutComponent.h"

namespace
{
	ULoadoutComponent *RingManagerFindLoadout(AActor *Actor)
	{
		return Actor ? Actor->FindComponentByClass<ULoadoutComponent>() : nullptr;
	}
}

void URingManager::Initialize(FSubsystemCollectionBase &Collection)
{
	Super::Initialize(Collection);
	// No OnCrystalBroken binding: the loadout component owns the auto-switch, and a
	// second handler here would switch rings twice.
}

void URingManager::Deinitialize()
{
	Super::Deinitialize();
}

// ==================== RING QUERIES ====================

URingData *URingManager::GetActiveRing(AActor *Actor) const
{
	ULoadoutComponent *LoadoutComp = RingManagerFindLoadout(Actor);
	return LoadoutComp ? LoadoutComp->GetActiveRing() : nullptr;
}

ESpellElement URingManager::GetActiveElement(AActor *Actor) const
{
	ULoadoutComponent *LoadoutComp = RingManagerFindLoadout(Actor);
	return LoadoutComp ? LoadoutComp->GetActiveRingElement() : ESpellElement::None;
}

URingData *URingManager::GetPrimaryRing(AActor *Actor) const
{
	ULoadoutComponent *LoadoutComp = RingManagerFindLoadout(Actor);
	return LoadoutComp ? LoadoutComp->GetPrimaryRing() : nullptr;
}

bool URingManager::SwitchToNextRing(AActor *Actor)
{
	ULoadoutComponent *LoadoutComp = RingManagerFindLoadout(Actor);
	return LoadoutComp && LoadoutComp->SwitchToNextUsableRing();
}

// ==================== CRYSTAL BREAK CONSUMER ====================

void URingManager::HandleCrystalBroken(AActor *Actor, UObject *Holder, FBrokenCrystalPayload Payload)
{
	// Unbound since B-024 (see Initialize); ULoadoutComponent::HandleCrystalBroken handles it.
}
