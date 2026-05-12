// Copyright Epic Games, Inc. All Rights Reserved.

#include "WeaponManager.h"
#include "WeaponData.h"
#include "CharacterDataComponent.h"
#include "CharacterData.h"
#include "LoadoutComponent.h"
#include "ItemData.h"
#include "BreakCalculator.h"

void UWeaponManager::Initialize(FSubsystemCollectionBase &Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("[WeaponManager] Initialized"));
}

void UWeaponManager::Deinitialize()
{
	Super::Deinitialize();
}

// ========================================
// QUERIES
// ========================================

UWeaponData *UWeaponManager::GetActiveWeapon(AActor *Actor) const
{
	if (ULoadoutComponent *LoadoutComp = GetLoadoutComponent(Actor))
	{
		return LoadoutComp->GetActiveWeapon();
	}
	return nullptr;
}

UWeaponAttackData *UWeaponManager::GetActiveAttack(AActor *Actor) const
{
	if (ULoadoutComponent *Loadout = GetLoadoutComponent(Actor))
	{
		return Loadout->GetCurrentAttack();
	}
	return nullptr;
}

// ========================================
// HELPERS
// ========================================

ULoadoutComponent *UWeaponManager::GetLoadoutComponent(AActor *Actor) const
{
	if (!Actor)
		return nullptr;
	return Actor->FindComponentByClass<ULoadoutComponent>();
}

// ============================================================
// DURABILITY (Phase 4d)
// ============================================================

int32 UWeaponManager::ProcessPostCastWear(AActor *Actor, EItemTier ActionTier, int32 InfusionLevel, bool bIsSpell)
{
	if (!Actor)
	{
		return 0;
	}

	UWeaponData *Weapon = GetActiveWeapon(Actor);
	if (!Weapon || !Weapon->SlottedCrystal)
	{
		return 0;
	}

	UItemData *Crystal = Weapon->SlottedCrystal;
	if (!Crystal->bIsRefined || Crystal->bImmuneToBreaking)
	{
		// Evolution crystals or unrefined crystals: no wear, nothing to do
		return 0;
	}

	if (Crystal->IsBroken())
	{
		// Already broken — should never reach here (commit-time gate should have
		// excluded broken crystal from the source options). Defensive: don't double-process.
		UE_LOG(LogTemp, Warning,
			   TEXT("[WeaponManager] ProcessPostCastWear called on already-broken crystal '%s' for %s"),
			   *Crystal->GetFullItemName(), *Actor->GetName());
		return 0;
	}

	const int32 Wear = UBreakCalculator::CalculateDurabilityWear(
		Crystal->Tier,
		ActionTier,
		InfusionLevel,
		bIsSpell);

	if (Wear <= 0)
	{
		return 0;
	}

	// Luck-driven break skip. Roll the wielder's Luck before applying wear.
	// On success, skip the wear entirely (durability unchanged, no broadcast,
	// no break check). Linearly scaled from raw Luck to LUCK_BREAK_SKIP_MAX.
	if (UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>())
	{
		if (UCharacterData *CharData = CharComp->CharacterData)
		{
			const float RawLuck = CharData->CalculateLuck();
			const float SkipChance = (RawLuck / CombatConstants::LUCK_RAW_MAX) * CombatConstants::LUCK_BREAK_SKIP_MAX;
			if (FMath::FRand() < SkipChance)
			{
				UE_LOG(LogTemp, Log,
					   TEXT("[WeaponManager] %s LUCKY break skip on weapon crystal '%s' (would have applied %d wear, skip chance %.2f)"),
					   *Actor->GetName(), *Crystal->GetFullItemName(), Wear, SkipChance);
				return 0;
			}
		}
	}

	UE_LOG(LogTemp, Verbose,
		   TEXT("[WeaponManager] %s applies %d wear to weapon crystal '%s' (%d/%d) [ActionTier=%d L%d bIsSpell=%d]"),
		   *Actor->GetName(), Wear,
		   *Crystal->GetFullItemName(),
		   Crystal->CurrentDurability, Crystal->MaxDurability,
		   static_cast<int32>(ActionTier), InfusionLevel, bIsSpell ? 1 : 0);

	Crystal->ApplyWear(Wear);
	// Crystal->ApplyWear fires UItemData::OnCrystalBroken if durability hits 0.
	// UWeaponManager no longer subscribes to that event — broken weapon crystals
	// downgrade lazily via UWeaponData::GetWeaponElement returning Generic when
	// IsBroken(). URingManager still subscribes for ring-side auto-switch.

	// Broadcast post-wear durability for real-time UI updates. Fires whether
	// the crystal survived or just broke — UI updates either way.
	OnWeaponDurabilityChanged.Broadcast(Actor, Weapon, Crystal->CurrentDurability, Crystal->MaxDurability);

	return Wear;
}
