// RingManager.cpp
// All ring state lives in ULoadoutComponent (single source of truth);
// URingManager is a stateless query/switching surface plus a
// crystal-break consumer.

#include "RingManager.h"
#include "RingData.h"
#include "ItemData.h"
#include "LoadoutComponent.h"
#include "FCombatLoadout.h"
#include "FRingLoadoutEntry.h"
#include "CrystalManager.h"

void URingManager::Initialize(FSubsystemCollectionBase &Collection)
{
	Super::Initialize(Collection);

	// Bind to UCrystalManager's unified break broadcast. Filter for rings
	// in the handler; ignore weapon-crystal breaks.
	if (UCrystalManager *CrystalMgr = Collection.InitializeDependency<UCrystalManager>())
	{
		CrystalMgr->OnCrystalBroken.AddDynamic(this, &URingManager::HandleCrystalBroken);
	}
}

void URingManager::Deinitialize()
{
	if (UGameInstance *GI = GetGameInstance())
	{
		if (UCrystalManager *CrystalMgr = GI->GetSubsystem<UCrystalManager>())
		{
			CrystalMgr->OnCrystalBroken.RemoveDynamic(this, &URingManager::HandleCrystalBroken);
		}
	}

	Super::Deinitialize();
}

// ==================== RING QUERIES ====================

URingData *URingManager::GetActiveRing(AActor *Actor) const
{
	if (!Actor)
		return nullptr;

	ULoadoutComponent *LoadoutComp = Actor->FindComponentByClass<ULoadoutComponent>();
	return LoadoutComp ? LoadoutComp->GetActiveRing() : nullptr;
}

ESpellElement URingManager::GetActiveElement(AActor *Actor) const
{
	URingData *Ring = GetActiveRing(Actor);
	return Ring ? Ring->GetRingElement() : ESpellElement::Generic;
}

URingData *URingManager::GetPrimaryRing(AActor *Actor) const
{
	if (!Actor)
		return nullptr;

	ULoadoutComponent *LoadoutComp = Actor->FindComponentByClass<ULoadoutComponent>();
	return LoadoutComp ? LoadoutComp->GetPrimaryRing() : nullptr;
}

// ==================== RING SWITCHING ====================

bool URingManager::SwitchToNextRing(AActor *Actor)
{
	if (!Actor)
		return false;

	ULoadoutComponent *LoadoutComp = Actor->FindComponentByClass<ULoadoutComponent>();
	if (!LoadoutComp)
		return false;

	const FCombatLoadout Loadout = LoadoutComp->GetActiveLoadout();
	const int32 RingCount = Loadout.RingLoadout.Num();
	if (RingCount == 0)
		return false;

	const int32 StartIndex = Loadout.ActiveRingIndex;

	// Find next valid ring (has functional crystal). Wrap around; skip current.
	for (int32 i = 1; i <= RingCount; ++i)
	{
		const int32 CheckIndex = (StartIndex + i) % RingCount;
		const FRingLoadoutEntry &Entry = Loadout.RingLoadout[CheckIndex];

		const FCrystalInventoryEntry &CrystalEntry = Entry.RingEntry.AttachedCrystal;
		if (CrystalEntry.Crystal && !CrystalEntry.IsBroken())
		{
			LoadoutComp->SetActiveRingIndex(CheckIndex);
			return true;
		}
	}

	return false;
}

// ==================== CRYSTAL BREAK CONSUMER ====================

void URingManager::HandleCrystalBroken(AActor *Actor, UObject *Holder, UItemData *Crystal)
{
	// Filter: only handle ring crystal breaks. Weapon crystals are not our concern.
	URingData *BrokenRing = Cast<URingData>(Holder);
	if (!BrokenRing || !Actor || !Crystal)
	{
		return;
	}

	UE_LOG(LogTemp, Log,
		   TEXT("[RingManager] Ring crystal '%s' broke on %s — auto-switching"),
		   *Crystal->GetFullItemName(), *Actor->GetName());

	SwitchToNextRing(Actor);
}
