// RingManager.cpp
// Simplified ring manager implementation

#include "RingManager.h"
#include "RingData.h"
#include "SpellData.h"
#include "ItemData.h"
#include "BreakCalculator.h"
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

	// Unsubscribe from any previously-tracked crystals for this actor
	UnsubscribeFromActorCrystals(Actor);

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

	// Subscribe to OnCrystalBroken for every slotted crystal so we can auto-switch
	SubscribeToActorCrystals(Actor);

	// Diagnostic: warn if any of this actor's crystals are also slotted on another actor
	WarnIfCrystalSharedAcrossActors(Actor);
}

void URingManager::ClearRingState(AActor *Actor)
{
	if (!Actor)
		return;
	UnsubscribeFromActorCrystals(Actor);
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
	// Reject ring if missing, legacy-broken, or has no functional crystal
	if (!TargetRing || TargetRing->bIsBroken)
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

		if (Ring && !Ring->bIsBroken && Ring->SlottedCrystal && !Ring->SlottedCrystal->IsBroken())
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
		if (Ring && !Ring->bIsBroken && Ring->SlottedCrystal && !Ring->SlottedCrystal->IsBroken())
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

// ============================================================
// DURABILITY WEAR (new model — Phase 2c)
// ============================================================

int32 URingManager::ProcessPostCastWear(AActor *Actor, USpellData *SpellCast, int32 InfusionLevel)
{
	if (!Actor || !SpellCast)
	{
		return 0;
	}

	URingData *Ring = GetActiveRing(Actor);
	if (!Ring)
	{
		return 0;
	}

	UItemData *Crystal = Ring->SlottedCrystal;
	if (!Crystal || !Crystal->bIsRefined || Crystal->bImmuneToBreaking)
	{
		// Evolution crystals or unrefined crystals: no wear, nothing to do
		return 0;
	}

	if (Crystal->IsBroken())
	{
		// Already broken — should never reach here (ring should've been auto-switched)
		// but defensive: don't double-process
		UE_LOG(LogTemp, Warning,
			   TEXT("[RingManager] ProcessPostCastWear called on already-broken crystal '%s' for %s"),
			   *Crystal->GetFullItemName(), *Actor->GetName());
		return 0;
	}

	const int32 Wear = UBreakCalculator::CalculateDurabilityWear(
		Crystal->Tier,
		SpellCast->Tier,
		InfusionLevel,
		/*bIsSpell=*/true);

	if (Wear <= 0)
	{
		return 0;
	}

	UE_LOG(LogTemp, Verbose,
		   TEXT("[RingManager] %s casting %s applies %d wear to crystal '%s' (%d/%d)"),
		   *Actor->GetName(), *SpellCast->SpellName, Wear,
		   *Crystal->GetFullItemName(),
		   Crystal->CurrentDurability, Crystal->MaxDurability);

	Crystal->ApplyWear(Wear);
	// Note: ApplyWear fires OnCrystalBroken if it hits 0; we handle that in HandleCrystalBroken

	return Wear;
}

// ============================================================
// CRYSTAL SUBSCRIPTION & BREAK HANDLING
// ============================================================

void URingManager::SubscribeToActorCrystals(AActor *Actor)
{
	const TArray<URingData *> *Rings = EquippedRings.Find(Actor);
	if (!Rings)
	{
		return;
	}

	for (URingData *Ring : *Rings)
	{
		if (!Ring || !Ring->SlottedCrystal)
		{
			continue;
		}

		UItemData *Crystal = Ring->SlottedCrystal;
		// Only subscribe to refined, non-immune crystals (others can't fire OnCrystalBroken anyway)
		if (!Crystal->bIsRefined || Crystal->bImmuneToBreaking)
		{
			continue;
		}

		// Avoid duplicate bindings if the same crystal is on multiple rings of this actor
		if (!Crystal->OnCrystalBroken.IsAlreadyBound(this, &URingManager::HandleCrystalBroken))
		{
			Crystal->OnCrystalBroken.AddDynamic(this, &URingManager::HandleCrystalBroken);
		}
	}
}

void URingManager::UnsubscribeFromActorCrystals(AActor *Actor)
{
	const TArray<URingData *> *Rings = EquippedRings.Find(Actor);
	if (!Rings)
	{
		return;
	}

	for (URingData *Ring : *Rings)
	{
		if (!Ring || !Ring->SlottedCrystal)
		{
			continue;
		}

		UItemData *Crystal = Ring->SlottedCrystal;
		if (Crystal->OnCrystalBroken.IsAlreadyBound(this, &URingManager::HandleCrystalBroken))
		{
			Crystal->OnCrystalBroken.RemoveDynamic(this, &URingManager::HandleCrystalBroken);
		}
	}
}

void URingManager::HandleCrystalBroken(UItemData *BrokenCrystal)
{
	if (!BrokenCrystal)
	{
		return;
	}

	AActor *OwnerActor = nullptr;
	URingData *OwnerRing = nullptr;
	if (!FindOwnerOfCrystal(BrokenCrystal, OwnerActor, OwnerRing))
	{
		// Crystal isn't on any ring we're tracking — could be from an unrelated event
		return;
	}

	UE_LOG(LogTemp, Log,
		   TEXT("[RingManager] Crystal '%s' broke on %s's ring '%s' — auto-switching"),
		   *BrokenCrystal->GetFullItemName(),
		   *OwnerActor->GetName(),
		   *OwnerRing->RingName);

	// Broadcast the new event
	OnRingCrystalBroken.Broadcast(OwnerActor, OwnerRing, BrokenCrystal);

	// Also broadcast the legacy event so existing BP listeners still fire
	// (will be removed in Phase 2d once BPs are migrated)
	OnRingBroken.Broadcast(OwnerActor, OwnerRing);

	// Auto-switch to next ring with a functional crystal
	SwitchToNextRing(OwnerActor);
}

bool URingManager::FindOwnerOfCrystal(UItemData *Crystal, AActor *&OutActor, URingData *&OutRing) const
{
	OutActor = nullptr;
	OutRing = nullptr;

	if (!Crystal)
	{
		return false;
	}

	for (const auto &Pair : EquippedRings)
	{
		AActor *Actor = Pair.Key.Get();
		if (!Actor)
		{
			continue;
		}

		for (URingData *Ring : Pair.Value)
		{
			if (Ring && Ring->SlottedCrystal == Crystal)
			{
				OutActor = Actor;
				OutRing = Ring;
				return true;
			}
		}
	}

	return false;
}

void URingManager::WarnIfCrystalSharedAcrossActors(AActor *Actor) const
{
	const TArray<URingData *> *MyRings = EquippedRings.Find(Actor);
	if (!MyRings)
	{
		return;
	}

	for (URingData *MyRing : *MyRings)
	{
		if (!MyRing || !MyRing->SlottedCrystal)
		{
			continue;
		}

		UItemData *MyCrystal = MyRing->SlottedCrystal;

		// Check every other actor's rings for this same crystal asset
		for (const auto &Pair : EquippedRings)
		{
			AActor *OtherActor = Pair.Key.Get();
			if (!OtherActor || OtherActor == Actor)
			{
				continue;
			}

			for (URingData *OtherRing : Pair.Value)
			{
				if (OtherRing && OtherRing->SlottedCrystal == MyCrystal)
				{
					UE_LOG(LogTemp, Warning,
						   TEXT("[RingManager] Crystal asset '%s' is slotted on BOTH %s and %s. ")
							   TEXT("Wear/break state will be SHARED between these characters. ")
								   TEXT("Use unique crystal data assets per character to avoid this."),
						   *MyCrystal->GetFullItemName(),
						   *Actor->GetName(),
						   *OtherActor->GetName());
				}
			}
		}
	}
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
