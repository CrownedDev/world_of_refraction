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
	WeaponStates.Empty();
	Super::Deinitialize();
}

// ========================================
// STATE MANAGEMENT
// ========================================

void UWeaponManager::InitializeWeaponState(AActor *Actor)
{
	if (!Actor)
		return;

	ULoadoutComponent *LoadoutComp = GetLoadoutComponent(Actor);
	UCharacterData *CharData = GetCharacterData(Actor);

	if (!LoadoutComp && !CharData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WeaponManager] Cannot initialize - no LoadoutComponent or CharacterData for %s"),
			   *Actor->GetName());
		return;
	}

	FWeaponState State;

	// Determine initial state from LoadoutComponent or CharacterData
	bool bIsGeneric = CharData ? CharData->IsGeneric() : (LoadoutComp && LoadoutComp->CharacterClass == ECharacterClass::Generic);

	if (bIsGeneric)
	{
		// Generic: bShowPrimary is the gameplay toggle between equipped weapons.
		bool bShowPrimary = LoadoutComp ? LoadoutComp->IsShowingPrimary() : true;
		State.ActiveSlot = bShowPrimary ? EWeaponSlot::Primary : EWeaponSlot::Secondary;
	}
	else
	{
		// Caster/Resonator: combat layer ignores bShowPrimary. Active iff primary weapon slotted.
		State.ActiveSlot = (LoadoutComp && LoadoutComp->GetPrimaryWeapon())
			? EWeaponSlot::Primary
			: EWeaponSlot::Unarmed;
	}

	WeaponStates.Add(Actor, State);

	// Subscribe to OnCrystalBroken for every slotted crystal on the actor's weapons
	SubscribeToActorWeaponCrystals(Actor);

	UE_LOG(LogTemp, Log, TEXT("[WeaponManager] Initialized weapon state for %s: Slot=%d"),
		   *Actor->GetName(), static_cast<int32>(State.ActiveSlot));
}

void UWeaponManager::ClearWeaponState(AActor *Actor)
{
	// Unsubscribe BEFORE removing state so the helper can iterate the loadout
	UnsubscribeFromActorWeaponCrystals(Actor);

	WeaponStates.Remove(Actor);
	UE_LOG(LogTemp, Log, TEXT("[WeaponManager] Cleared weapon state for %s"),
		   Actor ? *Actor->GetName() : TEXT("Unknown"));
}

FWeaponState UWeaponManager::GetWeaponState(AActor *Actor) const
{
	const FWeaponState *State = WeaponStates.Find(Actor);
	return State ? *State : FWeaponState();
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

TArray<UAbilityData *> UWeaponManager::GetActiveAbilities(AActor *Actor) const
{
	const FWeaponState *State = WeaponStates.Find(Actor);
	if (!State)
	{
		return TArray<UAbilityData *>();
	}

	UWeaponData *Weapon = GetWeaponInSlot(Actor, State->ActiveSlot);
	if (Weapon)
	{
		return Weapon->PresetAbilities;
	}

	return TArray<UAbilityData *>();
}

// ========================================
// WEAPON SWITCHING
// ========================================

bool UWeaponManager::SwitchWeapon(AActor *Actor, EWeaponSlot TargetSlot)
{
	if (!CanSwitchWeapon(Actor, TargetSlot))
	{
		return false;
	}

	FWeaponState *State = WeaponStates.Find(Actor);
	if (!State)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WeaponManager] No weapon state for %s"),
			   *Actor->GetName());
		return false;
	}

	EWeaponSlot OldSlot = State->ActiveSlot;
	State->ActiveSlot = TargetSlot;

	OnWeaponSwitched.Broadcast(Actor, OldSlot, TargetSlot);

	UE_LOG(LogTemp, Log, TEXT("[WeaponManager] %s switched from %d to %d"),
		   *Actor->GetName(), static_cast<int32>(OldSlot), static_cast<int32>(TargetSlot));

	return true;
}

bool UWeaponManager::CanSwitchWeapon(AActor *Actor, EWeaponSlot TargetSlot) const
{
	const FWeaponState *State = WeaponStates.Find(Actor);
	if (!State)
		return false;

	// Can't switch while conjured (must dispel first)
	if (State->ActiveSlot == EWeaponSlot::Conjured)
	{
		return false;
	}

	// Conjured slot is reserved enum-side; no caller can switch into it
	// directly (the conjuration runtime was removed in WeaponManager teardown).
	if (TargetSlot == EWeaponSlot::Conjured)
	{
		return false;
	}

	// Already in target slot
	if (State->ActiveSlot == TargetSlot)
	{
		return false;
	}

	TArray<EWeaponSlot> Available = GetAvailableSlots(Actor);
	return Available.Contains(TargetSlot);
}

TArray<EWeaponSlot> UWeaponManager::GetAvailableSlots(AActor *Actor) const
{
	TArray<EWeaponSlot> Slots;

	ULoadoutComponent *LoadoutComp = GetLoadoutComponent(Actor);
	UCharacterData *CharData = GetCharacterData(Actor);

	if (!LoadoutComp && !CharData)
		return Slots;

	Slots.Add(EWeaponSlot::Unarmed);

	// Check primary weapon availability
	if (LoadoutComp)
	{
		if (LoadoutComp->GetPrimaryWeapon())
			Slots.Add(EWeaponSlot::Primary);

		if (LoadoutComp->GetSecondaryWeapon())
			Slots.Add(EWeaponSlot::Secondary);
	}

	return Slots;
}

// ========================================
// ATTACK EXECUTION
// ========================================
// UWeaponManager::ExecuteAttack and ExecuteAttackWithInfusion deleted in C2.
// Their only in-source caller (UActionExecutor::ExecuteAttack's nullptr-fallback
// branch) was unreachable in production — fallback never fired in any
// orchestrator, AI, or input flow. BP audit confirmed zero Blueprint references
// for both functions before deletion. The async attack path
// (ExecuteAttackAsync -> OpenDefenseWindowsForTargets -> ApplyDamageAfterDefense
// -> ApplyHit) handles all production weapon attacks.

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

UWeaponData *UWeaponManager::GetWeaponInSlot(AActor *Actor, EWeaponSlot Slot) const
{
	switch (Slot)
	{
	case EWeaponSlot::Unarmed:
		return nullptr;

	case EWeaponSlot::Primary:
	{
		ULoadoutComponent *LoadoutComp = GetLoadoutComponent(Actor);
		if (LoadoutComp)
		{
			return LoadoutComp->GetPrimaryWeapon();
		}
		return nullptr;
	}

	case EWeaponSlot::Secondary:
	{
		ULoadoutComponent *LoadoutComp = GetLoadoutComponent(Actor);
		if (LoadoutComp)
		{
			return LoadoutComp->GetSecondaryWeapon();
		}
		return nullptr;
	}

	case EWeaponSlot::Conjured:
	default:
		return nullptr;
	}
}

// ========================================
// DEBUG
// ========================================

void UWeaponManager::DebugPrintWeaponStates() const
{
	UE_LOG(LogTemp, Display, TEXT("=== WEAPON STATES ==="));

	for (const auto &Pair : WeaponStates)
	{
		AActor *Actor = Pair.Key.Get();
		const FWeaponState &State = Pair.Value;

		UE_LOG(LogTemp, Display, TEXT("Actor: %s"),
			   Actor ? *Actor->GetName() : TEXT("Invalid"));
		UE_LOG(LogTemp, Display, TEXT("  Slot: %d"),
			   static_cast<int32>(State.ActiveSlot));
	}

	UE_LOG(LogTemp, Display, TEXT("====================="));
}

void UWeaponManager::DebugPrintActorWeaponState(AActor *Actor) const
{
	if (!Actor)
	{
		UE_LOG(LogTemp, Display, TEXT("Invalid actor"));
		return;
	}

	const FWeaponState *State = WeaponStates.Find(Actor);
	if (!State)
	{
		UE_LOG(LogTemp, Display, TEXT("No weapon state for %s"), *Actor->GetName());
		return;
	}

	UE_LOG(LogTemp, Display, TEXT("=== WEAPON STATE: %s ==="), *Actor->GetName());
	UE_LOG(LogTemp, Display, TEXT("Active Slot: %d"), static_cast<int32>(State->ActiveSlot));

	UWeaponData *ActiveWeapon = GetActiveWeapon(Actor);
	if (ActiveWeapon)
	{
		UE_LOG(LogTemp, Display, TEXT("Active Weapon: %s"), *ActiveWeapon->WeaponName);
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("Active Weapon: Unarmed"));
	}

	UWeaponAttackData *Attack = GetActiveAttack(Actor);
	if (Attack)
	{
		UE_LOG(LogTemp, Display, TEXT("Active Attack: %s"), *Attack->AttackName);
	}

	TArray<UAbilityData *> Abilities = GetActiveAbilities(Actor);
	UE_LOG(LogTemp, Display, TEXT("Active Abilities: %d"), Abilities.Num());
	for (UAbilityData *Ability : Abilities)
	{
		if (Ability)
		{
			UE_LOG(LogTemp, Display, TEXT("  - %s"), *Ability->AbilityName);
		}
	}

	UE_LOG(LogTemp, Display, TEXT("================================"));
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

