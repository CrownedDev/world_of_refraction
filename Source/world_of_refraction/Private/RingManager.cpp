// RingManager.cpp
// Simplified ring manager implementation

#include "RingManager.h"
#include "RingData.h"
#include "SpellData.h"

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

	const int32 *Index = ActiveRingIndex.Find(Actor);
	const TArray<URingData *> *Rings = EquippedRings.Find(Actor);

	if (!Index || !Rings)
		return nullptr;
	if (!Rings->IsValidIndex(*Index))
		return nullptr;

	return (*Rings)[*Index];
}

ESpellElement URingManager::GetActiveElement(AActor *Actor) const
{
	URingData *Ring = GetActiveRing(Actor);
	return Ring ? Ring->Element : ESpellElement::Generic;
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
