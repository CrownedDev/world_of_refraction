// RingManager.cpp
// Simplified ring manager implementation

#include "RingManager.h"
#include "RingData.h"
#include "SpellData.h"
#include "ElementColorDebugComponent.h"
#include "ElementColors.h"
#include "LoadoutComponent.h"
#include "FCombatLoadout.h"
#include "FRingLoadoutEntry.h"

void URingManager::Initialize(FSubsystemCollectionBase &Collection)
{
	Super::Initialize(Collection);
}

void URingManager::Deinitialize()
{
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

	// Reset all rings
	for (URingData *Ring : Rings)
	{
		if (Ring)
		{
			Ring->ResetRingState();
		}
	}

	// Find first valid ring
	for (int32 i = 0; i < Rings.Num(); ++i)
	{
		if (Rings[i])
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

// ==================== SPELL ACCESS ====================

TArray<USpellData *> URingManager::GetAvailableSpells(AActor *Actor) const
{
	URingData *Ring = GetActiveRing(Actor);
	return Ring ? Ring->GetAvailableSpells() : TArray<USpellData *>();
}

bool URingManager::CanCastSpell(AActor *Actor, USpellData *Spell) const
{
	URingData *Ring = GetActiveRing(Actor);
	return Ring ? Ring->CanCastSpell(Spell) : false;
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
	if (!TargetRing || TargetRing->bIsBroken)
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

	// Find next valid ring
	int32 StartIndex = *CurrentIndex;
	for (int32 i = 1; i <= Rings->Num(); ++i)
	{
		int32 CheckIndex = (StartIndex + i) % Rings->Num();
		URingData *Ring = (*Rings)[CheckIndex];

		if (Ring && !Ring->bIsBroken)
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
		if (Ring && !Ring->bIsBroken)
		{
			Count++;
		}
	}
	return Count;
}

// ==================== BREAK SYSTEM ====================

bool URingManager::ProcessPostCastBreakCheck(AActor *Actor, USpellData *SpellCast, bool bWasInfused)
{
	URingData *Ring = GetActiveRing(Actor);
	if (!Ring || Ring->bIsBroken)
		return false;

	float BreakChance = Ring->CalculateBreakChance(SpellCast, bWasInfused);
	bool bBroke = Ring->RollForBreak(BreakChance);

	if (bBroke)
	{
		OnRingBroken.Broadcast(Actor, Ring);

		// Auto-switch to next ring
		SwitchToNextRing(Actor);
	}

	return bBroke;
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