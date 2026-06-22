// BrokenDarknessManager.cpp
// Implementation for BrokenDarkness transformation and absorption system

#include "Combat/Mechanics/BrokenDarknessManager.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Definitions/AbilityData.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Combat/Defense/DefenseSystem.h"
#include "Skills/Definitions/WorldStatRequirements.h"
#include "Character/StatConstants.h"
#include "Combat/CombatConstants.h" // LUCK_BREAK_SKIP_MAX (strain luck-skip)
#include "Skills/Definitions/ElementHelpers.h"
#include "Skills/Effects/StatusBuildupManager.h"
#include "Skills/Definitions/EScalingTier.h" // GetScalingFraction (Efficiency scaling)
#include "Combat/CombatOrchestrator.h"        // WoR.AbsorptionSnapshot resolves the combat's BD actor
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

namespace BrokenDarknessConstants
{
	// Break System — tier-keyed, infusion-multiplied (L1 ×1.5, L2 ×2.0)
	// Per locked design (May 2026):
	// S=1.5%, A=1.0%, B=0.6%, C=0.3%, D=0.1%, E/F=0%.
	constexpr float BREAK_CHANCE_S_TIER = 0.015f;
	constexpr float BREAK_CHANCE_A_TIER = 0.010f;
	constexpr float BREAK_CHANCE_B_TIER = 0.006f;
	constexpr float BREAK_CHANCE_C_TIER = 0.003f;
	constexpr float BREAK_CHANCE_D_TIER = 0.001f;
	constexpr float BREAK_CHANCE_E_TIER = 0.0f;
	constexpr float BREAK_CHANCE_F_TIER = 0.0f;
	constexpr float BREAK_CHANCE_L1_MULTIPLIER = 1.5f;
	constexpr float BREAK_CHANCE_L2_MULTIPLIER = 2.0f;

	// Strain constants (deficit model) are header-homed in BrokenDarknessManager.h — BOTH
	// AddStrain and the caller (ActionExecutor) read them, so they can't be TU-local here.
	// See namespace BrokenDarknessConstants in the header.

	// Absorption — base rate (pre-Efficiency); Efficiency scales it UP via ABSORPTION_EFFICIENCY_K.
	constexpr float PARRY_BASE_RATE = 0.10f;        // base absorption rate (parry), pre-Efficiency
	constexpr float BLOCK_BASE_RATE = 0.05f;        // base absorption rate (block) — half of parry
	constexpr float ABSORPTION_EFFICIENCY_K = 8.0f; // Efficiency scaling: max-stat factor 0.5 → 5× the base rate
	constexpr float PERFECT_ABSORPTION_MULT = 2.0f; // perfect parry/block doubles absorbed energy

	// Stack Multipliers
	constexpr float STACK_0_MULT = 1.0f;
	constexpr float STACK_1_MULT = 1.0f;
	constexpr float STACK_2_MULT = 2.0f;
	constexpr float STACK_3_MULT = 4.0f;

	// Overload — CurrentEP may exceed MaxEP by this fraction of MaxEP before the
	// hard cap, so the overload window scales with the BD's stat-derived pool.
	constexpr float OVERLOAD_CAPACITY_FRACTION = 0.30f;

	// Aura Range (scales with MaxEnergy points)
	constexpr float AURA_RANGE_MIN = 2.0f; // 0 points - no coverage
	constexpr float AURA_RANGE_MAX = 4.5f; // 21 points - full 3x3 coverage
}

namespace
{
	/** Look up base break chance for a tier. Returns 0 for E/F. */
	float GetBaseBreakChance(EItemTier Tier)
	{
		switch (Tier)
		{
		case EItemTier::S_Tier: return BrokenDarknessConstants::BREAK_CHANCE_S_TIER;
		case EItemTier::A_Tier: return BrokenDarknessConstants::BREAK_CHANCE_A_TIER;
		case EItemTier::B_Tier: return BrokenDarknessConstants::BREAK_CHANCE_B_TIER;
		case EItemTier::C_Tier: return BrokenDarknessConstants::BREAK_CHANCE_C_TIER;
		case EItemTier::D_Tier: return BrokenDarknessConstants::BREAK_CHANCE_D_TIER;
		case EItemTier::E_Tier: return BrokenDarknessConstants::BREAK_CHANCE_E_TIER;
		case EItemTier::F_Tier: return BrokenDarknessConstants::BREAK_CHANCE_F_TIER;
		default: return 0.0f;
		}
	}

	/** Look up infusion-level multiplier. L0 = 1.0x, L1 = 1.5x, L2 = 2.0x. */
	float GetInfusionMultiplier(int32 InfusionLevel)
	{
		switch (InfusionLevel)
		{
		case 1: return BrokenDarknessConstants::BREAK_CHANCE_L1_MULTIPLIER;
		case 2: return BrokenDarknessConstants::BREAK_CHANCE_L2_MULTIPLIER;
		default: return 1.0f;
		}
	}
}

UBrokenDarknessManager::UBrokenDarknessManager()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBrokenDarknessManager::BeginPlay()
{
	Super::BeginPlay();

	// Initialize state
	bIsFlipped = false;
	bIsOverloaded = false;
	CurrentAlignmentElement = ESpellElement::Generic;
	CurrentAbsorptionStacks = 0;
	ConsecutiveAbsorptions = 0;

	if (AActor *Owner = GetOwner())
	{
		if (UCharacterDataComponent *CharComp = Owner->FindComponentByClass<UCharacterDataComponent>())
		{
			// Overload is re-evaluated on every owner energy change. Absorption
			// gain, cast spend, and overload drain all broadcast OnEPChanged,
			// so this binding is the single overload trigger point.
			CharComp->OnEPChanged.AddDynamic(this, &UBrokenDarknessManager::HandleOwnerEnergyChanged);

			// Character-created BD path: align the manager's internal flag with
			// CharacterDataComponent::IsBrokenDarkness(). Without this,
			// character-created BDs silently short-circuit absorption methods
			// (OnDefenseResolved, ProcessForbiddenCast, etc.)
			// because they all check bIsFlipped.
			if (CharComp->IsBrokenDarkness())
			{
				bIsFlipped = true;
				// Born-BD starts on the base Darkness pool (Model B), same as a runtime
				// transform. Element axis only — energy is set separately by CDC init.
				SeedBaseElement();
				UE_LOG(LogTemp, Log, TEXT("[BrokenDarkness] %s: auto-flipped bIsFlipped for character-created BD"),
					   *Owner->GetName());
			}
		}
	}
}

void UBrokenDarknessManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UCharacterDataComponent *CharComp = GetCharComp())
	{
		CharComp->OnEPChanged.RemoveDynamic(this, &UBrokenDarknessManager::HandleOwnerEnergyChanged);
	}

	Super::EndPlay(EndPlayReason);
}

// ==================== BREAK SYSTEM ====================

// Superseded by AddStrain (deterministic strain); retained for rollback, no production callers.
bool UBrokenDarknessManager::RollForBreak(EItemTier Tier, int32 InfusionLevel, const FString &TriggerReason)
{
	if (bIsFlipped)
	{
		return false; // Already transformed
	}

	const float BaseChance = GetBaseBreakChance(Tier);
	const float Multiplier = GetInfusionMultiplier(InfusionLevel);
	const float Chance = BaseChance * Multiplier;

	AActor *Owner = GetOwner();

	// E/F tier (or any unmapped tier) — chance is 0, skip the roll entirely
	if (Chance <= 0.0f)
	{
		UE_LOG(LogTemp, Verbose, TEXT("BrokenDarkness: %s skipped break check (Tier too low, Reason: %s)"),
			   Owner ? *Owner->GetName() : TEXT("Unknown"),
			   *TriggerReason);
		return false;
	}

	const float Roll = FMath::FRand();
	const bool bBreaks = Roll < Chance;

	if (bBreaks)
	{
		UE_LOG(LogTemp, Warning,
			   TEXT("BrokenDarkness: %s BROKE! (Reason: %s, Tier: %s, Level: L%d, Roll: %.4f < %.4f)"),
			   Owner ? *Owner->GetName() : TEXT("Unknown"),
			   *TriggerReason,
			   *UEnum::GetValueAsString(Tier),
			   InfusionLevel,
			   Roll, Chance);

		TriggerTransformation();
		return true;
	}
	else
	{
		UE_LOG(LogTemp, Display,
			   TEXT("BrokenDarkness: %s survived break check (Reason: %s, Tier: %s, Level: L%d, Roll: %.4f >= %.4f)"),
			   Owner ? *Owner->GetName() : TEXT("Unknown"),
			   *TriggerReason,
			   *UEnum::GetValueAsString(Tier),
			   InfusionLevel,
			   Roll, Chance);

		return false;
	}
}

void UBrokenDarknessManager::AddStrain(int32 Deficit, const FString &TriggerReason)
{
	if (bIsFlipped)
	{
		return; // Already transformed — mirror RollForBreak's guard.
	}

	if (Deficit <= 0)
	{
		return; // Qualified cast — no requirement deficit, no strain (the deficit self-zeroes).
	}

	AActor *Owner = GetOwner();

	// Composed combat stats (NOT asset-intrinsic — the values combat actually uses).
	UCharacterDataComponent *CharComp = GetCharComp();
	if (!CharComp)
	{
		return;
	}

	const int32 MaxEP = CharComp->MaxEP;
	if (MaxEP <= 0)
	{
		return; // Guard div0 — a zero/invalid pool can't take a percentage slice.
	}

	const float SpellDamageFrac = CharComp->GetEffectiveSpellDamage() - 1.0f; // >= 0
	const float DefenceFrac = CharComp->GetEvolutionModifiedFlatDefense();    // [0, 0.5]

	// Power amplifies, control dampens — both clamped (mirrors the durability-wear wrap).
	const float PowerFactor = FMath::Min(1.0f + SpellDamageFrac, BrokenDarknessConstants::STRAIN_POWER_FACTOR_MAX);
	const float ControlFactor = FMath::Max(1.0f - DefenceFrac, BrokenDarknessConstants::STRAIN_CONTROL_FACTOR_MIN);

	const float StrainPerCast = Deficit * BrokenDarknessConstants::STRAIN_PCT_PER_DEFICIT * PowerFactor * ControlFactor;
	float StrainAdded = (StrainPerCast / static_cast<float>(MaxEP)) * 100.0f;

	// Luck wear-skip parity: the wielder can skip the whole strain event (no accrual), exactly
	// as ProcessPostCastWear skips a durability event. Negative ("cursed") luck never skips.
	const float SkipChance = CharComp->GetLuckModifiedChance(0.0f, CombatConstants::LUCK_BREAK_SKIP_MAX);
	if (FMath::FRand() < SkipChance)
	{
		UE_LOG(LogTemp, Log,
			   TEXT("BrokenDarkness: %s LUCKY strain skip (would have added %.2f, skip chance %.2f, Reason: %s)"),
			   Owner ? *Owner->GetName() : TEXT("Unknown"),
			   StrainAdded, SkipChance, *TriggerReason);
		return;
	}

	// Min-floor: a real deficit that isn't luck-skipped always accrues at least the unmodulated
	// single-deficit-point amount, so control-stacking can't grind a real deficit down to ~0.
	const float MinFloor = (BrokenDarknessConstants::STRAIN_PCT_PER_DEFICIT / static_cast<float>(MaxEP)) * 100.0f;
	if (StrainAdded < MinFloor)
	{
		StrainAdded = MinFloor;
	}

	AccruedStrainPct += StrainAdded;

	UE_LOG(LogTemp, Display,
		   TEXT("BrokenDarkness: %s +%.2f strain (now %.2f / %.0f) (Reason: %s, Deficit: %d)"),
		   Owner ? *Owner->GetName() : TEXT("Unknown"),
		   StrainAdded, AccruedStrainPct, BrokenDarknessConstants::STRAIN_BREAK_THRESHOLD,
		   *TriggerReason, Deficit);

	if (AccruedStrainPct >= BrokenDarknessConstants::STRAIN_BREAK_THRESHOLD)
	{
		UE_LOG(LogTemp, Warning,
			   TEXT("BrokenDarkness: %s BROKE via strain! (%.2f >= %.0f, Reason: %s)"),
			   Owner ? *Owner->GetName() : TEXT("Unknown"),
			   AccruedStrainPct, BrokenDarknessConstants::STRAIN_BREAK_THRESHOLD,
			   *TriggerReason);

		TriggerTransformation();
	}
}

bool UBrokenDarknessManager::DoesSpellExceedRequirements(USpellData *Spell, UCharacterData *Character)
{
	if (!Spell || !Character)
	{
		return false;
	}

	// Uses FWorldStatRequirements - if there's a deficit, character doesn't meet requirements
	return Spell->Requirements.GetTotalDeficit(Character) > 0;
}

bool UBrokenDarknessManager::DoesAbilityExceedRequirements(UAbilityData *Ability, UCharacterData *Character)
{
	if (!Ability || !Character)
	{
		return false;
	}

	// Uses FWorldStatRequirements - if there's a deficit, character doesn't meet requirements
	return Ability->Requirements.GetTotalDeficit(Character) > 0;
}

void UBrokenDarknessManager::ForceTransformation()
{
	if (!bIsFlipped)
	{
		UE_LOG(LogTemp, Warning, TEXT("BrokenDarkness: Force transformation triggered"));
		TriggerTransformation();
	}
}

void UBrokenDarknessManager::TriggerTransformation()
{
	if (bIsFlipped)
	{
		return;
	}

	bIsFlipped = true;

	// Energy carries over: whatever CurrentEP the character held at the moment
	// of transformation becomes their starting absorption buffer.

	// Seed the base Darkness pool as active (Model B) and reset stacks. The seed
	// sets CurrentAlignmentElement to Darkness, superseding the old =Generic reset.
	SeedBaseElement();
	CurrentAbsorptionStacks = 0;
	ConsecutiveAbsorptions = 0;

	AActor *Owner = GetOwner();

	// Update the runtime flag on CharacterDataComponent. This zeros regular
	// EP and is the canonical "this character is now BD" signal that
	// IsBrokenDarkness() reads.
	if (Owner)
	{
		if (UCharacterDataComponent *CharComp = Owner->FindComponentByClass<UCharacterDataComponent>())
		{
			CharComp->ServerSetBrokenDarkness(true);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("BrokenDarkness: %s has TRANSFORMED into Broken Darkness!"),
		   Owner ? *Owner->GetName() : TEXT("Unknown"));

	// Broadcast event
	OnTransformed.Broadcast(Owner);

	// VFX/Audio trigger point
}

void UBrokenDarknessManager::RevertTransformation()
{
	// Mirror of TriggerTransformation's double-entry guard — if not BD, nothing to revert.
	if (!bIsFlipped)
		return;

	// 1. Flip the manager flag.
	bIsFlipped = false;

	// 2. Clear all BD runtime state.
	//    ExitOverload() is a no-op if not overloaded; fires OnOverloadStateChanged if it was.
	//    ResetStacks() zeros CurrentAbsorptionStacks + ConsecutiveAbsorptions.
	ExitOverload();
	ResetStacks();
	CurrentAlignmentElement = ESpellElement::Generic;
	AbsorbedElements.Empty();

	// 3. Clear the runtime BD flag on the component + reset EP→MaxEP. The EP guard is on
	//    bIsBrokenDarkness (CDC), which ServerSetBrokenDarkness(false) clears before resetting
	//    EP — so the EP reset is correctly ordered inside the CDC false branch.
	if (UCharacterDataComponent *CharComp = GetCharComp())
	{
		CharComp->ServerSetBrokenDarkness(false);
	}

	UE_LOG(LogTemp, Warning, TEXT("BrokenDarkness: %s has REVERTED to Darkness."),
		   GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));

	// 4. Fire the reverted event (VFX/UI hook — trigger binds here, not OnTransformed).
	OnReverted.Broadcast(GetOwner());
}

// ==================== FORBIDDEN ELEMENTS ====================

bool UBrokenDarknessManager::IsForbiddenElement(ESpellElement Element)
{
	return Element == ESpellElement::Light || Element == ESpellElement::Void;
}

bool UBrokenDarknessManager::CanAbsorbElement(ESpellElement Element)
{
	// Allowlist gate — reject the non-absorbable sentinels/values; everything that
	// passes (the 7 elements + Darkness) rotates the active pool on absorption.
	//   Generic — polymorphic placeholder, nothing concrete to absorb
	//   None    — non-elemental / physical attack, no element to absorb
	//   Reality — too powerful
	// Darkness is intentionally absorbable: it is the rotation target that points the
	// active pool back to the innate Darkness pool (a normal rotation, NOT the
	// out-of-combat RevertTransformation BD-exit).
	if (Element == ESpellElement::Generic ||
		Element == ESpellElement::None ||
		Element == ESpellElement::Reality)
	{
		return false;
	}

	return true;
}

float UBrokenDarknessManager::CalculateForbiddenCastDamage(float SpellBaseDamage) const
{
	// Spell-damage-scaled self-cost — mirrors the convention at
	// DamageCalculator::GetAttackerDamageMultiplier (:226-249) where
	// GetEvolutionModifiedSpellDamage is used as a direct multiplier on damage.
	// The multiplier returns 1.0 + (ModifiedMind × points × SPELL_DAMAGE_PER_POINT),
	// so an unleveled caster gets the flat percent and a stat-invested one scales up.
	float SpellDamageMult = 1.0f;
	if (const UCharacterDataComponent *CharComp = GetCharComp())
	{
		// Fully-layered SpellDamage (innate + equipment + stone + transient/gamble) —
		// the unified getter, so BD's forbidden-cast self-cost inherits all four layers
		// the spell pipeline applies, not just the innate term.
		SpellDamageMult = CharComp->GetEffectiveSpellDamage();
	}
	return SpellBaseDamage * ForbiddenCastSelfDamagePercent * SpellDamageMult;
}

bool UBrokenDarknessManager::ProcessForbiddenCast(ESpellElement SpellElement, float SpellBaseDamage)
{
	if (!bIsFlipped)
	{
		return false;
	}

	if (!IsForbiddenElement(SpellElement))
	{
		return false;
	}

	AActor *Owner = GetOwner();

	// 1) Self-DAMAGE — scales with caster's SpellDamage stat (locked design 4.2).
	const float SelfDamage = CalculateForbiddenCastDamage(SpellBaseDamage);
	ApplyDamageToActor(Owner, SelfDamage);

	// 2) Self-BUILDUP — in the forbidden element. Base amount only; AddStatusBuildup
	//    applies the caster's StatusMultiplier amplification AND the caster's own
	//    Resistance reduction internally (the BD is both Source and Target here).
	//    Routed directly to UStatusBuildupManager from here — the apply site sits
	//    adjacent to the self-damage call, both costs side by side.
	const float SelfBuildup = SpellBaseDamage * ForbiddenCastSelfBuildupPercent;
	if (SelfBuildup > 0.0f)
	{
		if (UWorld *World = GetWorld())
		{
			if (UGameInstance *GI = World->GetGameInstance())
			{
				if (UStatusBuildupManager *SBM = GI->GetSubsystem<UStatusBuildupManager>())
				{
					SBM->AddStatusBuildup(Owner, Owner, SelfBuildup, SpellElement, EPhysicalDamageType::None);
				}
			}
		}
	}

	UE_LOG(LogTemp, Warning,
		   TEXT("BrokenDarkness: Forbidden element %s self-cost on %s — damage %.1f, base buildup %.1f"),
		   *UEnum::GetValueAsString(SpellElement),
		   Owner ? *Owner->GetName() : TEXT("Unknown"),
		   SelfDamage, SelfBuildup);

	// Broadcast for VFX/UI feedback (HP-side; no subscribers in source today)
	OnOverloadDamage.Broadcast(Owner, Owner, SelfDamage);

	return true;
}

// ==================== ABSORPTION ====================

void UBrokenDarknessManager::GrantAbsorptionEnergy(float Amount, ESpellElement Element)
{
	// Public entry point for non-defense absorption sources (e.g. ItemExecutor
	// when a crystal is used on a BD). Mirrors the defense path (OnDefenseResolved):
	// energy + element rotation. Defense absorption reaches AddAbsorptionEnergy +
	// RecordAbsorbedElement the same way.
	if (!bIsFlipped)
	{
		return;
	}

	AddAbsorptionEnergy(Amount);
	RecordAbsorbedElement(Element); // self-guards via CanAbsorbElement — rotates the active
									// pool + fires OnAlignmentChanged for absorbable elements,
									// no-ops for Reality/Generic/None (energy still granted)
}

void UBrokenDarknessManager::DrainAndRevertToBase(float Amount)
{
	// Reality cleanse: drain the (tier-scaled) energy and revert the active pool to the
	// base Darkness pool — unmaking the stolen element. The character STAYS BD.
	if (!bIsFlipped)
	{
		return;
	}

	const ESpellElement OldActive = GetActivePool();

	if (UCharacterDataComponent *CharComp = GetCharComp())
	{
		CharComp->ServerSpendEnergy(FMath::RoundToInt(Amount)); // drain, clamps at 0, fires OnEPChanged
	}

	SeedBaseElement(); // clear AbsorbedElements -> {Darkness}, set alignment Darkness (stays BD)

	// SeedBaseElement is silent (sets CurrentAlignmentElement directly, no delegate), so the
	// bar won't re-tint on its own — broadcast the alignment change explicitly here.
	if (OldActive != ESpellElement::Darkness)
	{
		OnAlignmentChanged.Broadcast(GetOwner(), OldActive, ESpellElement::Darkness);
	}
}

void UBrokenDarknessManager::AddAbsorptionEnergy(float Amount)
{
	if (Amount <= 0.0f)
	{
		return;
	}

	UCharacterDataComponent *CharComp = GetCharComp();
	if (!CharComp)
	{
		return;
	}

	// BD energy is stored on CurrentEP. Absorption may push it above MaxEP into
	// overload — the ceiling is MaxEP + GetOverloadCapacity() (30% of MaxEP).
	// The OnEPChanged broadcast inside ServerGainBrokenDarknessEnergy drives
	// UpdateOverloadState via HandleOwnerEnergyChanged.
	const int32 AbsoluteMax = CharComp->MaxEP + FMath::RoundToInt(GetOverloadCapacity());
	CharComp->ServerGainBrokenDarknessEnergy(FMath::RoundToInt(Amount), AbsoluteMax);

	UE_LOG(LogTemp, Verbose, TEXT("BrokenDarkness: absorbed %.1f energy -> CurrentEP %d/%d (Overload: %s)"),
		   Amount, CharComp->CurrentEP, CharComp->MaxEP, bIsOverloaded ? TEXT("Yes") : TEXT("No"));
}

void UBrokenDarknessManager::RecordAbsorbedElement(ESpellElement Element)
{
	// Check if element can be absorbed
	if (!CanAbsorbElement(Element))
	{
		return;
	}

	// Single active slot: the most recent absorption is always Last(). Re-absorbing
	// a prior element moves it to the end rather than duplicating, so the array
	// stays a distinct, recency-ordered history and Last() is the active element.
	AbsorbedElements.Remove(Element);
	AbsorbedElements.Add(Element);

	// Process stacks and alignment
	ProcessElementAbsorption(Element);
}

void UBrokenDarknessManager::SeedBaseElement()
{
	// Model B: Darkness is the base pool, active immediately on transform so the BD
	// can use the base element without first parrying. RESET (not append) so a
	// re-transform after a revert starts clean on Darkness. Element axis ONLY — no
	// energy granted (CurrentEP untouched, AddAbsorptionEnergy not called) and no
	// delegates fired; this is transform-time seeding, not an absorption event.
	AbsorbedElements.Empty();
	AbsorbedElements.Add(ESpellElement::Darkness);
	CurrentAlignmentElement = ESpellElement::Darkness;
}

// ==================== OVERLOAD STATE ====================

float UBrokenDarknessManager::GetOverloadEnergy() const
{
	const UCharacterDataComponent *CharComp = GetCharComp();
	if (CharComp && CharComp->CurrentEP > CharComp->MaxEP)
	{
		return static_cast<float>(CharComp->CurrentEP - CharComp->MaxEP);
	}
	return 0.0f;
}

float UBrokenDarknessManager::GetOverloadCapacity() const
{
	// Derived: 30% of the stat-derived MaxEP — no stored field. The overload
	// window scales with the BD's energy pool rather than a flat buffer.
	const UCharacterDataComponent *CharComp = GetCharComp();
	const int32 MaxEP = CharComp ? CharComp->MaxEP : 0;
	return MaxEP * BrokenDarknessConstants::OVERLOAD_CAPACITY_FRACTION;
}

void UBrokenDarknessManager::UpdateOverloadState()
{
	const UCharacterDataComponent *CharComp = GetCharComp();
	if (!CharComp)
	{
		return;
	}

	const bool bShouldBeOverloaded = CharComp->CurrentEP > CharComp->MaxEP;

	if (bShouldBeOverloaded && !bIsOverloaded)
	{
		EnterOverload();
	}
	else if (!bShouldBeOverloaded && bIsOverloaded)
	{
		ExitOverload();
	}
}

void UBrokenDarknessManager::EnterOverload()
{
	if (bIsOverloaded)
	{
		return;
	}

	bIsOverloaded = true;

	AActor *Owner = GetOwner();
	const UCharacterDataComponent *CharComp = GetCharComp();
	UE_LOG(LogTemp, Warning, TEXT("BrokenDarkness: %s entered OVERLOAD! (Energy: %d/%d)"),
		   Owner ? *Owner->GetName() : TEXT("Unknown"),
		   CharComp ? CharComp->CurrentEP : 0, CharComp ? CharComp->MaxEP : 0);

	OnOverloadStateChanged.Broadcast(Owner, true);

	// VFX/Audio trigger point
}

void UBrokenDarknessManager::ExitOverload()
{
	if (!bIsOverloaded)
	{
		return;
	}

	bIsOverloaded = false;

	AActor *Owner = GetOwner();
	const UCharacterDataComponent *CharComp = GetCharComp();
	UE_LOG(LogTemp, Log, TEXT("BrokenDarkness: %s exited overload (Energy: %d/%d)"),
		   Owner ? *Owner->GetName() : TEXT("Unknown"),
		   CharComp ? CharComp->CurrentEP : 0, CharComp ? CharComp->MaxEP : 0);

	OnOverloadStateChanged.Broadcast(Owner, false);
}

void UBrokenDarknessManager::ProcessOverloadTick(const TArray<AActor *> &NearbyEnemies,
												 float StatusMultiplierBonus, float EfficiencyMult)
{
	if (!bIsOverloaded || !bIsFlipped)
	{
		return;
	}

	AActor *Owner = GetOwner();
	UCharacterDataComponent *CharComp = GetCharComp();

	// Unified HP-damage path: both aura (enemies) and self-damage scale by the BD's
	// fully-layered SpellDamage (innate + equipment + stone + transient/gamble) via the
	// shared GetEffectiveSpellDamage getter — the SAME layered value the spell pipeline
	// applies (minus the call-specific ActionMods/Grid/defender terms). ApplyDamageToActor
	// stays the shared apply primitive; only the SpellDamage scalar gained the layers.
	const float SpellDamageMult = CharComp ? CharComp->GetEffectiveSpellDamage() : 1.0f;

	// 1. Aura HP damage to combatants in range — same SpellDamage scaling as self.
	const float AuraDamage = BaseOverloadAuraDamage * SpellDamageMult;
	for (AActor *Enemy : NearbyEnemies)
	{
		if (Enemy && Enemy != Owner)
		{
			ApplyDamageToActor(Enemy, AuraDamage);
			OnOverloadDamage.Broadcast(Owner, Enemy, AuraDamage);

			UE_LOG(LogTemp, Log, TEXT("BrokenDarkness: Overload aura dealt %.1f damage to %s"),
				   AuraDamage, *Enemy->GetName());
		}
	}

	// 2. Self HP damage — same convention.
	const float SelfDamage = BaseOverloadSelfDamage * SpellDamageMult;
	ApplyDamageToActor(Owner, SelfDamage);
	OnOverloadDamage.Broadcast(Owner, Owner, SelfDamage);

	UE_LOG(LogTemp, Log, TEXT("BrokenDarkness: Overload self-damage %.1f"), SelfDamage);

	// 3. Coupled energy leak. ONE value drives both outflows — by design.
	//   released  = BaseEnergyRelease × StatusMultiplier↑ × Efficiency↓
	//             - StatusMultiplier raises release ("more flow")
	//             - Efficiency lowers release ("smaller hole")
	//             - StatusMultiplier here folds in BOTH the base-stat layer AND the
	//               transient skill-effect buff/debuff (step 5b), so both feed the
	//               drain — a buff burns faster, a debuff slower.
	//   drain     = ServerSpendEnergy(released) — the BD's absorption bleeds back
	//               toward MaxEP; the OnEPChanged broadcast re-evaluates overload
	//               via HandleOwnerEnergyChanged, so overload auto-exits when the
	//               pool drops to MaxEP.
	//   self-status = AddStatusBuildup(BD, BD, released, alignment, None,
	//                                  bSkipBaseStatAmp=true). Steps 5 (base-stat) AND
	//                 5b (transient buff/debuff) are both skipped because `released`
	//                 already includes both. Steps 5c (BD stack multiplier) and 6
	//                 (target resistance) still fire on this self-status.
	const float Released = BaseEnergyRelease * StatusMultiplierBonus * EfficiencyMult;

	if (CharComp)
	{
		const int32 DrainAmount = FMath::RoundToInt(Released);
		CharComp->ServerSpendEnergy(DrainAmount);

		UE_LOG(LogTemp, Log,
			   TEXT("BrokenDarkness: Overload leak released %.1f (StatusMult=%.2f, EffMult=%.2f), absorption now %d"),
			   Released, StatusMultiplierBonus, EfficiencyMult, CharComp->CurrentEP);
	}

	// Self-status — released energy becomes status buildup in the alignment element.
	// A flipped BD's alignment is always a real element (Darkness is seeded on transform,
	// then rotates on absorption — never Generic in the flipped path), so no Generic guard
	// is needed here.
	if (Released > 0.0f)
	{
		if (UWorld *World = GetWorld())
		{
			if (UGameInstance *GI = World->GetGameInstance())
			{
				if (UStatusBuildupManager *SBM = GI->GetSubsystem<UStatusBuildupManager>())
				{
					// bSkipBaseStatAmp=true: `Released` is ALREADY StatusMultiplier-amplified (pre-baked in
					// ProcessOverloadTick so the energy DRAIN sees the amplified value too — "one value, two
					// outflows"). Letting AddStatusBuildup re-apply the source amplification here would
					// double-count it on the self-status. Do NOT remove: the pre-bake is required by the
					// drain, so the amplification cannot be deferred to AddStatusBuildup.
					SBM->AddStatusBuildup(Owner, Owner, Released,
										  CurrentAlignmentElement, EPhysicalDamageType::None,
										  /*bSkipBaseStatAmp=*/true);
				}
			}
		}
	}
}

void UBrokenDarknessManager::ApplyDamageToActor(AActor *Target, float Damage)
{
	if (!Target)
	{
		return;
	}

	UCharacterDataComponent *CharComp = Target->FindComponentByClass<UCharacterDataComponent>();
	if (CharComp)
	{
		CharComp->ServerTakeDamage(FMath::RoundToInt(Damage));
	}
}

UCharacterDataComponent *UBrokenDarknessManager::GetCharComp() const
{
	AActor *Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UCharacterDataComponent>() : nullptr;
}

void UBrokenDarknessManager::HandleOwnerEnergyChanged(int32 InCurrentEP, int32 InMaxEP)
{
	// Overload is a Broken-Darkness mechanic — non-BD energy changes are ignored.
	if (!bIsFlipped)
	{
		return;
	}

	UpdateOverloadState();
}

// ==================== ABSORPTION STACKS ====================

float UBrokenDarknessManager::GetStackStatusMultiplier() const
{
	switch (CurrentAbsorptionStacks)
	{
	case 0:
		return BrokenDarknessConstants::STACK_0_MULT;
	case 1:
		return BrokenDarknessConstants::STACK_1_MULT;
	case 2:
		return BrokenDarknessConstants::STACK_2_MULT;
	case 3:
	default:
		return BrokenDarknessConstants::STACK_3_MULT;
	}
}

float UBrokenDarknessManager::GetElementStackStatusMultiplier(ESpellElement Element) const
{
	// Non-transformed BD (or pre-transform Caster) → no amplification.
	if (!bIsFlipped)
	{
		return 1.0f;
	}
	// Element gate — stacks amplify only matches against the current alignment.
	if (Element == CurrentAlignmentElement)
	{
		return GetStackStatusMultiplier();
	}
	return 1.0f;
}

void UBrokenDarknessManager::ProcessElementAbsorption(ESpellElement Element)
{
	AActor *Owner = GetOwner();
	ESpellElement OldAlignment = CurrentAlignmentElement;

	// Check if same element as current alignment. (Element here has already passed
	// CanAbsorbElement, so it is never Generic — matching the alignment implies a real element.)
	if (Element == CurrentAlignmentElement)
	{
		// Same element - increment toward next stack
		ConsecutiveAbsorptions++;

		// Stacks: Absorption 1 = Stack 0, Absorption 2 = Stack 1, etc.
		int32 NewStacks = FMath::Min(ConsecutiveAbsorptions - 1, MaxAbsorptionStacks);

		if (NewStacks != CurrentAbsorptionStacks)
		{
			CurrentAbsorptionStacks = NewStacks;
			OnStacksChanged.Broadcast(Owner, Element, CurrentAbsorptionStacks);

			UE_LOG(LogTemp, Log, TEXT("BrokenDarkness: %s stacks increased to %d (x%.1f status mult)"),
				   *UEnum::GetValueAsString(Element),
				   CurrentAbsorptionStacks,
				   GetStackStatusMultiplier());
		}
	}
	else
	{
		// Different element - reset stacks, change alignment
		ResetStacks();

		CurrentAlignmentElement = Element;
		ConsecutiveAbsorptions = 1;
		CurrentAbsorptionStacks = 0;

		OnAlignmentChanged.Broadcast(Owner, OldAlignment, Element);
		OnStacksChanged.Broadcast(Owner, Element, 0);

		UE_LOG(LogTemp, Log, TEXT("BrokenDarkness: Alignment changed %s -> %s (stacks reset)"),
			   *UEnum::GetValueAsString(OldAlignment),
			   *UEnum::GetValueAsString(Element));
	}
}

void UBrokenDarknessManager::ResetStacks()
{
	CurrentAbsorptionStacks = 0;
	ConsecutiveAbsorptions = 0;
}

// ==================== HYBRID SPELLS ====================

bool UBrokenDarknessManager::HasAbsorbedElement(ESpellElement Element) const
{
	// Single active slot: only the most recent absorption is "active". Earlier
	// entries in AbsorbedElements are historical and do not count here.
	return AbsorbedElements.Num() > 0 && AbsorbedElements.Last() == Element;
}

bool UBrokenDarknessManager::IsElementCastable(AActor *Actor,
											   UCharacterDataComponent *CharComp,
											   UBrokenDarknessManager *BDManager,
											   ESpellElement Element)
{
	// Generic is the polymorphic wildcard — always castable here. A Generic spell
	// resolves to a real element at cast (ResolveSpellCastElement reads the actual
	// source/pool), so resolution is the real gate; the element itself never blocks.
	if (Element == ESpellElement::Generic)
	{
		return true;
	}

	// Cannot resolve the character — do not block.
	if (!CharComp)
	{
		return true;
	}

	// Broken Darkness (Model B): the castable element is the SINGLE active pool
	// (GetActivePool() — the most recent absorption, Darkness when seeded). Darkness
	// is castable only when it IS the active pool: it is seeded as the base pool on
	// transform and rotates like any other element, so there is no always-on Darkness.
	// The equipped-crystal channel still unlocks an element regardless of the active pool.
	if (CharComp->IsBrokenDarkness() && BDManager)
	{
		return Element == BDManager->GetActivePool()
			|| ULoadoutComponent::HasEquippedSourceForElement(Actor, Element);
	}

	// Non-BD: innate-element match, an innate any-element source
	// (Reality / BrokenDarkness), or an equipped crystal channelling the element.
	const UCharacterData *CharData = CharComp->CharacterData;
	if (!CharData)
	{
		return true;
	}
	return Element == CharData->InnateElement
		|| ElementHelpers::IsAnySpellSource(CharData->InnateElement)
		|| ULoadoutComponent::HasEquippedSourceForElement(Actor, Element);
}

// ==================== DEFENSE SYSTEM INTEGRATION ====================

void UBrokenDarknessManager::OnDefenseResolved(EDefenseType DefenseType,
											   const FDefenseResult &DefenseResult, ESpellElement AttackElement, float AttackEnergyCost, bool bPerfect)
{
	if (!bIsFlipped)
	{
		return;
	}

	// Dodge doesn't absorb - you avoided it entirely
	if (DefenseType == EDefenseType::Dodge)
	{
		UE_LOG(LogTemp, Display, TEXT("BrokenDarkness: Dodge - nothing to absorb"));
		return;
	}

	// Only Block and Parry absorb
	if (DefenseType != EDefenseType::Block && DefenseType != EDefenseType::Parry)
	{
		return;
	}

	// Must be successful defense
	if (!DefenseResult.bSuccess && !DefenseResult.bWasInWindow)
	{
		UE_LOG(LogTemp, Display, TEXT("BrokenDarkness: Defense failed - no absorption"));
		return;
	}

	// Reality cannot be absorbed — it UNMAKES the BD instead. Parrying/blocking a Reality
	// attack DRAINS the would-be-gain energy and reverts the active pool to base Darkness
	// (the cleanse), mirroring the Reality item path. Must run BEFORE the generic
	// !CanAbsorbElement return below (CanAbsorbElement(Reality) is false, so the generic
	// check would otherwise swallow Reality with no drain). A Generic spell resolved to
	// Reality arrives here as AttackElement == Reality, so it is covered identically.
	if (AttackElement == ESpellElement::Reality)
	{
		const float WouldBeGain = CalculateAbsorptionEnergy(DefenseType, AttackEnergyCost, bPerfect);
		DrainAndRevertToBase(WouldBeGain); // drain that amount (clamped) + revert pool to Darkness
		UE_LOG(LogTemp, Log,
			   TEXT("BrokenDarkness: Reality parry/block — drained %.1f energy + reverted to base Darkness"),
			   WouldBeGain);
		return; // do NOT fall through to the normal absorb
	}

	// Check if element can be absorbed (other non-absorbable elements → nothing happens)
	if (!CanAbsorbElement(AttackElement))
	{
		UE_LOG(LogTemp, Display, TEXT("BrokenDarkness: Cannot absorb %s element"),
			   *UEnum::GetValueAsString(AttackElement));
		return;
	}

	// Calculate energy gained (perfect parry/block doubles it)
	float EnergyGained = CalculateAbsorptionEnergy(DefenseType, AttackEnergyCost, bPerfect);

	// Add energy
	AddAbsorptionEnergy(EnergyGained);

	// Record element (handles stacks and alignment)
	RecordAbsorbedElement(AttackElement);

	// Broadcast
	OnEnergyAbsorbed.Broadcast(GetOwner(), EnergyGained, AttackElement);

	UE_LOG(LogTemp, Log, TEXT("BrokenDarkness: %s%s absorbed %.1f energy from %s (Stacks: %d)"),
		   DefenseType == EDefenseType::Parry ? TEXT("Parry") : TEXT("Block"),
		   bPerfect ? TEXT(" (Perfect)") : TEXT(""),
		   EnergyGained,
		   *UEnum::GetValueAsString(AttackElement),
		   CurrentAbsorptionStacks);
}

float UBrokenDarknessManager::CalculateAbsorptionEnergy(EDefenseType DefenseType, float AttackEnergyCost, bool bPerfect) const
{
	float BaseRate = 0.0f;
	switch (DefenseType)
	{
	case EDefenseType::Parry:
		BaseRate = BrokenDarknessConstants::PARRY_BASE_RATE;
		break;
	case EDefenseType::Block:
		BaseRate = BrokenDarknessConstants::BLOCK_BASE_RATE;
		break;
	default:
		return 0.0f;
	}

	// Efficiency scales absorption UP (read INVESTMENT, not the inverted cost multiplier).
	// GetScalingFraction(Efficiency, …) bucket-D returns max(0, 1 − EffMult): 0 (no investment) →
	// 0.5 (max stat) → ~0.9 (gear). Rate = BaseRate × (1 + factor × K) → parry 10%→50% (×8), block 5%→25%.
	float EfficiencyFactor = 0.0f;
	if (AActor *Owner = GetOwner())
	{
		if (UCharacterDataComponent *CharComp = Owner->FindComponentByClass<UCharacterDataComponent>())
		{
			EfficiencyFactor = GetScalingFraction(ESubStat::Efficiency, CharComp->GetEffectiveEfficiencyMultiplier());
		}
	}

	const float AbsorptionRate = BaseRate * (1.0f + EfficiencyFactor * BrokenDarknessConstants::ABSORPTION_EFFICIENCY_K);

	// Perfect parry/block doubles the absorbed energy (timing-based, type-agnostic — applies to both).
	const float PerfectMult = bPerfect ? BrokenDarknessConstants::PERFECT_ABSORPTION_MULT : 1.0f;
	return AttackEnergyCost * AbsorptionRate * PerfectMult;
}

void UBrokenDarknessManager::DebugLogAbsorption(float AttackEnergyCost) const
{
	// Raw Efficiency multiplier (inverted: 1.0 neutral → 0.5 max stat → ~0.10 gear) and the
	// increasing factor it maps to via GetScalingFraction — shown so the curve is legible.
	float EffMult = 1.0f;
	if (const AActor *Owner = GetOwner())
	{
		if (const UCharacterDataComponent *CharComp = Owner->FindComponentByClass<UCharacterDataComponent>())
		{
			EffMult = CharComp->GetEffectiveEfficiencyMultiplier();
		}
	}
	const float EffFactor = GetScalingFraction(ESubStat::Efficiency, EffMult);

	UE_LOG(LogTemp, Display, TEXT("=== AbsorptionSnapshot: %s (effMult %.3f → effFactor %.3f, K %.1f, cost %.1f) ==="),
		   GetOwner() ? *GetOwner()->GetName() : TEXT("?"),
		   EffMult, EffFactor, BrokenDarknessConstants::ABSORPTION_EFFICIENCY_K, AttackEnergyCost);

	const EDefenseType Types[] = {EDefenseType::Parry, EDefenseType::Block};
	for (const EDefenseType Type : Types)
	{
		const float BaseRate = (Type == EDefenseType::Parry)
								   ? BrokenDarknessConstants::PARRY_BASE_RATE
								   : BrokenDarknessConstants::BLOCK_BASE_RATE;
		const float Rate = BaseRate * (1.0f + EffFactor * BrokenDarknessConstants::ABSORPTION_EFFICIENCY_K);
		const float NormalEnergy = CalculateAbsorptionEnergy(Type, AttackEnergyCost, /*bPerfect=*/false);
		const float PerfectEnergy = CalculateAbsorptionEnergy(Type, AttackEnergyCost, /*bPerfect=*/true);
		UE_LOG(LogTemp, Display, TEXT("  %s: base %.3f → rate %.3f → energy %.1f (normal) / %.1f (perfect ×%.1f)"),
			   Type == EDefenseType::Parry ? TEXT("Parry") : TEXT("Block"),
			   BaseRate, Rate, NormalEnergy, PerfectEnergy, BrokenDarknessConstants::PERFECT_ABSORPTION_MULT);
	}
}

// ========================================
// CONSOLE COMMAND — absorption-curve inspection
// ========================================

namespace
{
	// Resolve the active combat's first Broken Darkness actor (transformed or not — the curve is
	// inspectable regardless) and log its absorption breakdown across a couple of attack energy costs.
	// No exact parry needed; run mid-PIE-combat.
	void RunAbsorptionSnapshotCommand(UWorld *World)
	{
		if (!World)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BrokenDarkness] WoR.AbsorptionSnapshot: no world"));
			return;
		}

		ACombatOrchestrator *Orchestrator =
			Cast<ACombatOrchestrator>(UGameplayStatics::GetActorOfClass(World, ACombatOrchestrator::StaticClass()));
		if (!Orchestrator)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BrokenDarkness] WoR.AbsorptionSnapshot: no CombatOrchestrator (run during a PIE combat)"));
			return;
		}

		UBrokenDarknessManager *BDManager = nullptr;
		TArray<AActor *> Combatants = Orchestrator->GetTeam0();
		Combatants.Append(Orchestrator->GetTeam1());
		for (AActor *Actor : Combatants)
		{
			if (Actor)
			{
				if (UBrokenDarknessManager *Mgr = Actor->FindComponentByClass<UBrokenDarknessManager>())
				{
					BDManager = Mgr;
					break;
				}
			}
		}

		if (!BDManager)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BrokenDarkness] WoR.AbsorptionSnapshot: no BrokenDarknessManager among combatants"));
			return;
		}

		const float SampleCosts[] = {50.0f, 100.0f};
		for (const float Cost : SampleCosts)
		{
			BDManager->DebugLogAbsorption(Cost);
		}
	}

	// arc-2 verification: RevertTransformation has no production caller yet, so this fires it manually
	// on the active combat's first transformed Broken Darkness actor. Confirms IsBrokenDarkness() reads
	// false after, EP resets to MaxEP, the bar relabels, absorption/overload stop, and OnReverted fires.
	// Retained as BD debug tooling per CLAUDE.md (every system ships debug utilities).
	void RunBDRevertCommand(UWorld *World)
	{
		if (!World)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BrokenDarkness] WoR.TestBDRevert: no world"));
			return;
		}

		ACombatOrchestrator *Orchestrator =
			Cast<ACombatOrchestrator>(UGameplayStatics::GetActorOfClass(World, ACombatOrchestrator::StaticClass()));
		if (!Orchestrator)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BrokenDarkness] WoR.TestBDRevert: no CombatOrchestrator (run during a PIE combat)"));
			return;
		}

		UBrokenDarknessManager *BDManager = nullptr;
		TArray<AActor *> Combatants = Orchestrator->GetTeam0();
		Combatants.Append(Orchestrator->GetTeam1());
		for (AActor *Actor : Combatants)
		{
			if (Actor)
			{
				if (UBrokenDarknessManager *Mgr = Actor->FindComponentByClass<UBrokenDarknessManager>())
				{
					if (Mgr->IsTransformed())
					{
						BDManager = Mgr;
						break;
					}
				}
			}
		}

		if (!BDManager)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BrokenDarkness] WoR.TestBDRevert: no transformed Broken Darkness actor among combatants"));
			return;
		}

		UE_LOG(LogTemp, Warning, TEXT("[BrokenDarkness] WoR.TestBDRevert: reverting %s..."),
			   BDManager->GetOwner() ? *BDManager->GetOwner()->GetName() : TEXT("?"));
		BDManager->RevertTransformation();
	}
}

static FAutoConsoleCommandWithWorld GAbsorptionSnapshotCommand(
	TEXT("WoR.AbsorptionSnapshot"),
	TEXT("Log a Broken Darkness actor's absorption breakdown (base rate, Efficiency factor, rate, energy) ")
	TEXT("for sample attack energy costs across parry + block — inspect the Efficiency-scaled curve without ")
	TEXT("triggering an exact parry. Resolves the first BD among the active combat's combatants."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&RunAbsorptionSnapshotCommand));

static FAutoConsoleCommandWithWorld GTestBDRevertCommand(
	TEXT("WoR.TestBDRevert"),
	TEXT("Forcibly revert the active combat's first transformed Broken Darkness actor back to Darkness ")
	TEXT("(RevertTransformation). Arc-2 debug hook — confirms EP reset to MaxEP, bar relabel, OnReverted fire."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&RunBDRevertCommand));

float UBrokenDarknessManager::CalculateAuraRange() const
{
	if (!bIsOverloaded)
	{
		return 0.0f;
	}

	// Get character data for MaxEnergy points
	AActor *Owner = GetOwner();
	if (!Owner)
	{
		return BrokenDarknessConstants::AURA_RANGE_MIN;
	}

	UCharacterDataComponent *CharComp = Owner->FindComponentByClass<UCharacterDataComponent>();
	if (!CharComp || !CharComp->CharacterData)
	{
		return BrokenDarknessConstants::AURA_RANGE_MIN;
	}

	int32 MaxEnergyPoints = CharComp->CharacterData->GetTotalMaxEnergy();

	// Linear scaling: 0 points = MIN, 21 points = MAX
	float PointRatio = FMath::Clamp(
		(float)MaxEnergyPoints / (float)StatConstants::MAX_SUBSTAT_POINTS_PER_PILLAR,
		0.0f,
		1.0f);

	return BrokenDarknessConstants::AURA_RANGE_MIN +
		   (PointRatio * (BrokenDarknessConstants::AURA_RANGE_MAX - BrokenDarknessConstants::AURA_RANGE_MIN));
}