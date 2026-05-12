// Copyright Epic Games, Inc. All Rights Reserved.

#include "WeaponManager.h"
#include "WeaponData.h"
#include "WeaponAttackData.h"
#include "AbilityData.h"
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
// STATE MANAGEMENT
// ========================================

void UWeaponManager::InitializeWeaponState(AActor *Actor)
{
	if (!Actor)
		return;

	SubscribeToActorWeaponCrystals(Actor);

	UE_LOG(LogTemp, Log, TEXT("[WeaponManager] Registered crystal subscriptions for %s"),
		   *Actor->GetName());
}

void UWeaponManager::ClearWeaponState(AActor *Actor)
{
	UnsubscribeFromActorWeaponCrystals(Actor);
	UE_LOG(LogTemp, Log, TEXT("[WeaponManager] Cleared crystal subscriptions for %s"),
		   Actor ? *Actor->GetName() : TEXT("Unknown"));
}

UWeaponData *UWeaponManager::GetActiveWeapon(AActor *Actor) const
{
	ULoadoutComponent *LoadoutComp = GetLoadoutComponent(Actor);
	if (LoadoutComp)
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

UCharacterDataComponent *UWeaponManager::GetCharacterDataComponent(AActor *Actor) const
{
	if (!Actor)
		return nullptr;
	return Actor->FindComponentByClass<UCharacterDataComponent>();
}

UCharacterData *UWeaponManager::GetCharacterData(AActor *Actor) const
{
	UCharacterDataComponent *Comp = GetCharacterDataComponent(Actor);
	return Comp ? Comp->CharacterData : nullptr;
}
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
	// Note: ApplyWear fires OnCrystalBroken if it hits 0; we handle that in HandleWeaponCrystalBroken

	// Broadcast post-wear durability for real-time UI updates. Fires whether
	// the crystal survived or just broke — UI updates either way.
	OnWeaponDurabilityChanged.Broadcast(Actor, Weapon, Crystal->CurrentDurability, Crystal->MaxDurability);

	return Wear;
}

// ============================================================
// CRYSTAL SUBSCRIPTION & BREAK HANDLING
// ============================================================

void UWeaponManager::SubscribeToActorWeaponCrystals(AActor *Actor)
{
	if (!Actor)
	{
		return;
	}

	ULoadoutComponent *LoadoutComp = GetLoadoutComponent(Actor);
	if (!LoadoutComp)
	{
		return;
	}

	// Iterate every weapon in the loadout — subscribe to crystals on each.
	// Currently uses UWeaponData::SlottedCrystal (asset-side); see TODO note in header.
	auto SubscribeToWeapon = [this](UWeaponData *Weapon)
	{
		if (!Weapon || !Weapon->SlottedCrystal)
		{
			return;
		}

		UItemData *Crystal = Weapon->SlottedCrystal;
		if (!Crystal->bIsRefined || Crystal->bImmuneToBreaking)
		{
			return;
		}

		if (!Crystal->OnCrystalBroken.IsAlreadyBound(this, &UWeaponManager::HandleWeaponCrystalBroken))
		{
			Crystal->OnCrystalBroken.AddDynamic(this, &UWeaponManager::HandleWeaponCrystalBroken);
		}
	};

	// Primary, Secondary, and conjured weapons all need monitoring.
	// LoadoutComponent's accessors are the source of truth.
	if (UWeaponData *Primary = LoadoutComp->GetPrimaryWeapon())
	{
		SubscribeToWeapon(Primary);
	}
	if (UWeaponData *Secondary = LoadoutComp->GetSecondaryWeapon())
	{
		SubscribeToWeapon(Secondary);
	}

	// Conjured weapon is runtime-spawned during combat; if a Caster has one
	// active we'd subscribe to it then. For now, conjured-time subscription
	// is a TODO when the conjure pipeline lands.
}

void UWeaponManager::UnsubscribeFromActorWeaponCrystals(AActor *Actor)
{
	if (!Actor)
	{
		return;
	}

	ULoadoutComponent *LoadoutComp = GetLoadoutComponent(Actor);
	if (!LoadoutComp)
	{
		return;
	}

	auto UnsubscribeFromWeapon = [this](UWeaponData *Weapon)
	{
		if (!Weapon || !Weapon->SlottedCrystal)
		{
			return;
		}

		UItemData *Crystal = Weapon->SlottedCrystal;
		if (Crystal->OnCrystalBroken.IsAlreadyBound(this, &UWeaponManager::HandleWeaponCrystalBroken))
		{
			Crystal->OnCrystalBroken.RemoveDynamic(this, &UWeaponManager::HandleWeaponCrystalBroken);
		}
	};

	if (UWeaponData *Primary = LoadoutComp->GetPrimaryWeapon())
	{
		UnsubscribeFromWeapon(Primary);
	}
	if (UWeaponData *Secondary = LoadoutComp->GetSecondaryWeapon())
	{
		UnsubscribeFromWeapon(Secondary);
	}

	// Conjured: same TODO as in subscribe.
}

void UWeaponManager::HandleWeaponCrystalBroken(UItemData *BrokenCrystal)
{
	if (!BrokenCrystal)
	{
		return;
	}

	AActor *OwnerActor = nullptr;
	UWeaponData *OwnerWeapon = nullptr;
	if (!ULoadoutComponent::FindOwnerOfWeaponCrystal(BrokenCrystal, OwnerActor, OwnerWeapon))
	{
		// Crystal isn't on any weapon we're tracking — could be from a ring
		// (which has its own handler in URingManager) or an unrelated event
		return;
	}

	UE_LOG(LogTemp, Log,
		   TEXT("[WeaponManager] Crystal '%s' broke on %s's weapon '%s' — weapon stays equipped, downgrades to physical"),
		   *BrokenCrystal->GetFullItemName(),
		   *OwnerActor->GetName(),
		   *OwnerWeapon->WeaponName);

	// Broadcast the crystal-broken event for any listener (UI, VFX, audio)
	OnWeaponCrystalBroken.Broadcast(OwnerActor, OwnerWeapon, BrokenCrystal);

	// Per locked design: NO auto-switch. Weapon stays equipped, just loses
	// crystal-derived effects (element, Iolite enhancement, Quartz absorption).
	// WeaponData::GetWeaponElement already returns Generic when crystal IsBroken.
}

