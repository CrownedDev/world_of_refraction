// RingManager.cpp
// Simplified ring manager implementation

#include "RingManager.h"
#include "RingData.h"
#include "ItemData.h"
#include "ElementColorDebugComponent.h"
#include "ElementColors.h"
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

	ActiveRingIndex.Empty();
	EquippedRings.Empty();
	Super::Deinitialize();
}

// ==================== RING STATE ====================

void URingManager::SetEquippedRings(AActor *Actor, const TArray<URingData *> &Rings)
{
	if (!Actor)
		return;

	EquippedRings.Add(Actor, Rings);
	ActiveRingIndex.Add(Actor, 0);

	// Find first valid ring (has crystal, crystal not broken)
	for (int32 i = 0; i < Rings.Num(); ++i)
	{
		URingData *Ring = Rings[i];
		if (Ring && Ring->SlottedCrystal && !Ring->SlottedCrystal->IsBroken())
		{
			ActiveRingIndex[Actor] = i;
			break;
		}
	}
}

void URingManager::ClearRingState(AActor *Actor)
{
	if (!Actor)
		return;
	ActiveRingIndex.Remove(Actor);
	EquippedRings.Remove(Actor);
}

URingData *URingManager::GetActiveRing(AActor *Actor) const
{
	if (!Actor)
		return nullptr;

	// Check local state first (runtime tracking with break state)
	const int32 *Index = ActiveRingIndex.Find(Actor);
	const TArray<URingData *> *Rings = EquippedRings.Find(Actor);

	if (Index && Rings && Rings->IsValidIndex(*Index))
	{
		return (*Rings)[*Index];
	}

	// Fallback to LoadoutComponent (if not yet initialized locally)
	ULoadoutComponent *LoadoutComp = Actor->FindComponentByClass<ULoadoutComponent>();
	if (LoadoutComp)
	{
		return LoadoutComp->GetActiveRing();
	}

	return nullptr;
}

ESpellElement URingManager::GetActiveElement(AActor *Actor) const
{
	URingData *Ring = GetActiveRing(Actor);
	return Ring ? Ring->GetRingElement() : ESpellElement::Generic;
}

TArray<URingData *> URingManager::GetEquippedRings(AActor *Actor) const
{
	const TArray<URingData *> *Rings = EquippedRings.Find(Actor);
	return Rings ? *Rings : TArray<URingData *>();
}

// ==================== RING SWITCHING ====================

bool URingManager::SwitchToRing(AActor *Actor, int32 RingIndex)
{
	if (!Actor)
		return false;

	TArray<URingData *> *Rings = EquippedRings.Find(Actor);
	if (!Rings || !Rings->IsValidIndex(RingIndex))
		return false;

	URingData *TargetRing = (*Rings)[RingIndex];
	// Reject ring if missing or has no functional crystal
	if (!TargetRing)
		return false;
	if (!TargetRing->SlottedCrystal || TargetRing->SlottedCrystal->IsBroken())
		return false;

	ActiveRingIndex[Actor] = RingIndex;
	if (UElementColorDebugComponent *ColorComp = Actor->FindComponentByClass<UElementColorDebugComponent>())
	{
		ESpellElement NewElement = TargetRing->GetRingElement(); // or from inventory entry
		FLinearColor ElementColor = ElementColors::GetColorForElement(NewElement);
		ColorComp->SetColorDirect(ElementColor);
	}
	return true;
}

bool URingManager::SwitchToNextRing(AActor *Actor)
{
	if (!Actor)
		return false;

	const int32 *CurrentIndex = ActiveRingIndex.Find(Actor);
	TArray<URingData *> *Rings = EquippedRings.Find(Actor);

	if (!CurrentIndex || !Rings)
		return false;

	// Find next valid ring (has functional crystal)
	int32 StartIndex = *CurrentIndex;
	for (int32 i = 1; i <= Rings->Num(); ++i)
	{
		int32 CheckIndex = (StartIndex + i) % Rings->Num();
		URingData *Ring = (*Rings)[CheckIndex];

		if (Ring && Ring->SlottedCrystal && !Ring->SlottedCrystal->IsBroken())
		{
			ActiveRingIndex[Actor] = CheckIndex;
			return true;
		}
	}

	return false;
}

int32 URingManager::GetWorkingRingCount(AActor *Actor) const
{
	const TArray<URingData *> *Rings = EquippedRings.Find(Actor);
	if (!Rings)
		return 0;

	int32 Count = 0;
	for (URingData *Ring : *Rings)
	{
		if (Ring && Ring->SlottedCrystal && !Ring->SlottedCrystal->IsBroken())
		{
			Count++;
		}
	}
	return Count;
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

void URingManager::InitializeFromLoadout(AActor *Actor, ULoadoutComponent *LoadoutComp)
{
	if (!Actor || !LoadoutComp)
		return;

	// Only Resonators use ring loadout
	if (LoadoutComp->CharacterClass != ECharacterClass::Resonator)
	{
		UE_LOG(LogTemp, Log, TEXT("[RingManager] %s is not a Resonator, skipping ring init"),
			   *Actor->GetName());
		return;
	}

	FCombatLoadout Loadout = LoadoutComp->GetActiveLoadout();

	// Extract rings from ring loadout
	TArray<URingData *> Rings;
	for (const FRingLoadoutEntry &Entry : Loadout.RingLoadout)
	{
		if (Entry.IsValid())
		{
			Rings.Add(Entry.RingEntry.Ring);
		}
	}

	if (Rings.Num() > 0)
	{
		SetEquippedRings(Actor, Rings);

		// Sync active index
		if (ActiveRingIndex.Contains(Actor))
		{
			ActiveRingIndex[Actor] = Loadout.ActiveRingIndex;
		}

		UE_LOG(LogTemp, Log, TEXT("[RingManager] Initialized %d rings for %s from LoadoutComponent"),
			   Rings.Num(), *Actor->GetName());
	}
}

URingData *URingManager::GetPrimaryRing(AActor *Actor) const
{
	if (!Actor)
		return nullptr;

	// Primary ring is LoadoutComponent only
	ULoadoutComponent *LoadoutComp = Actor->FindComponentByClass<ULoadoutComponent>();
	if (LoadoutComp)
	{
		return LoadoutComp->GetPrimaryRing();
	}

	return nullptr;
}
