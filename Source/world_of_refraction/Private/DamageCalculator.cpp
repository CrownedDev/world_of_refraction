// DamageCalculator.cpp
// Centralized damage calculation implementation

#include "DamageCalculator.h"
#include "CharacterData.h"
#include "CharacterDataComponent.h"
#include "SpellData.h"
#include "AbilityData.h"
#include "WeaponAttackData.h"
#include "StatusEffectManager.h"
#include "BrokenDarknessManager.h"
#include "Engine/GameInstance.h"
#include "WeaponData.h"
#include "WeaponManager.h"
#include "CombatGridSubsystem.h"

void UDamageCalculator::Initialize(FSubsystemCollectionBase &Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("[DamageCalculator] Initialized"));
}

// ==================== MAIN CALCULATION ====================

FDamageCalculationResult UDamageCalculator::CalculateDamage(
	AActor *Attacker,
	AActor *Defender,
	const FDamageCalculationInput &Input)
{
	FDamageCalculationResult Result;
	Result.EffectiveElement = Input.Element;

	if (Input.BaseDamage <= 0)
	{
		return Result;
	}

	float RunningDamage = static_cast<float>(Input.BaseDamage);

	// Step 1: Attacker's damage multiplier — branched on EActionType.
	// Spell → SpellDamage. Ability/Attack/None → RawDamage. Per-action ActionMods
	// boost the matching sub-stat. ActionMods carries Reality + Evolution + any
	// future per-action stat modifier sources.
	float AttackerMult = GetAttackerDamageMultiplier(Attacker, Input.ActionType);
	const ESubStat AttackerStat = (Input.ActionType == EActionType::Spell) ? ESubStat::SpellDamage : ESubStat::RawDamage;
	AttackerMult = Input.ActionMods.ApplyTo(AttackerMult, AttackerStat);
	Result.AttackerDamageMultiplier = AttackerMult;
	RunningDamage *= AttackerMult;

	// Step 1.5: Grid position damage modifier (attacker)
	UCombatGridSubsystem *Grid = GetCombatGridSubsystem();
	if (Grid)
	{
		float GridDamageMod = Grid->GetDamageModifier(Attacker);
		RunningDamage *= GridDamageMod;
	}

	// Step 2: Status effect modifiers (buffs/debuffs)
	float StatusMod = GetStatusEffectDamageModifier(Attacker, Defender);
	RunningDamage *= StatusMod;

	// Step 3: Element interaction (weakness/resistance)
	if (Input.Element != ESpellElement::Generic && Defender)
	{
		UCharacterData *DefenderData = GetCharacterData(Defender);
		if (DefenderData)
		{
			Result.ElementMultiplier = GetElementInteractionMultiplier(Input.Element, DefenderData->InnateElement);
			RunningDamage *= Result.ElementMultiplier;
		}
	}

	// Step 4: Critical hit
	if (Input.bCanCrit)
	{
		float CritChance = Input.OverrideCritChance >= 0.0f ? Input.OverrideCritChance : GetCriticalChance(Attacker);
		// ActionMods.CritChance applies whether crit chance came from override
		// or computed default. No re-clamp here — preserves prior bool-path
		// behaviour (Reality boost was deliberately uncapped at this site).
		CritChance = Input.ActionMods.ApplyTo(CritChance, ESubStat::CritChance);

		// Luck-driven crit bonus. Linearly scaled from raw Luck (0.0-LUCK_RAW_MAX)
		// to consumer cap LUCK_CRIT_BONUS_MAX. Additive on top — matches locked
		// design where Luck grants extra crit chance ON TOP OF CritChance.
		UCharacterData *AttackerData = GetCharacterData(Attacker);
		if (AttackerData)
		{
			const float RawLuck = AttackerData->CalculateLuck();
			const float LuckCritBonus = (RawLuck / CombatConstants::LUCK_RAW_MAX) * CombatConstants::LUCK_CRIT_BONUS_MAX;
			CritChance += LuckCritBonus;
		}

		Result.bWasCritical = FMath::FRand() < CritChance;

		if (Result.bWasCritical)
		{
			Result.CritMultiplier = DamageConstants::CRIT_MULTIPLIER;
			RunningDamage *= DamageConstants::CRIT_MULTIPLIER;
		}
	}

	// Store damage before defense
	Result.DamageBeforeDefense = FMath::RoundToInt(RunningDamage);

	// Step 5: Defender's flat defense
	if (!Input.bIgnoreDefense && Defender)
	{
		Result.DefenderFlatDefense = GetDefenderFlatDefense(Defender);
		int32 Blocked = FMath::Min(Result.DefenderFlatDefense, FMath::RoundToInt(RunningDamage));
		Result.DamageBlockedByDefense = Blocked;
		RunningDamage -= Blocked;
	}

	// Step 6.5: Grid position defense modifier (defender)
	if (Grid && Defender)
	{
		float GridDefenseMod = Grid->GetDefenseModifier(Defender);
		if (GridDefenseMod > 0.0f)
		{
			RunningDamage /= GridDefenseMod;
		}
	}

	// Step 7: Ensure minimum damage
	Result.FinalDamage = FMath::Max(DamageConstants::MIN_DAMAGE, FMath::RoundToInt(RunningDamage));

	// Step 7: Ensure minimum damage
	Result.FinalDamage = FMath::Max(DamageConstants::MIN_DAMAGE, FMath::RoundToInt(RunningDamage));

	// Calculate status buildup if applicable
	// (Caller should handle this separately based on spell/ability data)

	return Result;
}

FDamageCalculationResult UDamageCalculator::CalculateSpellDamage(
	AActor *Caster,
	AActor *Target,
	USpellData *Spell,
	int32 InfusionLevel)
{
	FDamageCalculationResult Result;

	if (!Spell)
	{
		return Result;
	}

	UCharacterData *CasterData = GetCharacterData(Caster);
	if (!CasterData)
	{
		return Result;
	}

	// Build input
	FDamageCalculationInput Input;
	Input.BaseDamage = Spell->Damage;
	Input.ActionType = EActionType::Spell;
	Input.Element = Spell->Element;
	Input.bCanCrit = true;
	Input.bWasInfused = InfusionLevel > 0;
	Input.InfusionLevel = InfusionLevel;
	Input.bIsRawMode = Spell->bIsRawMode;

	// Apply spell size infusion (not damage, but tracking)
	// Actual size handled elsewhere

	// Calculate with main function
	Result = CalculateDamage(Caster, Target, Input);

	// Calculate status buildup (SpellData has a method, not a property)
	if (!Spell->bIsRawMode && CasterData)
	{
		int32 BaseBuildup = Spell->CalculateStatusBuildup(CasterData);
		if (BaseBuildup > 0)
		{
			Result.StatusBuildup = CalculateStatusBuildup(Caster, Target, BaseBuildup, Spell->Element);
		}
	}

	return Result;
}

FDamageCalculationResult UDamageCalculator::CalculateAbilityDamage(
	AActor *User,
	AActor *Target,
	UAbilityData *Ability,
	bool bIsInfused)
{
	FDamageCalculationResult Result;

	if (!Ability)
	{
		return Result;
	}

	UCharacterData *UserData = GetCharacterData(User);
	if (!UserData)
	{
		return Result;
	}

	// Build input
	FDamageCalculationInput Input;
	Input.BaseDamage = Ability->BaseDamage;
	Input.ActionType = EActionType::Ability;

	// Abilities are physical unless infused. Per locked design, infused abilities
	// still scale by RawDamage — the element only affects status routing.
	Input.Element = bIsInfused ? UserData->InnateElement : ESpellElement::Generic;

	Input.bCanCrit = true;
	Input.bWasInfused = bIsInfused;
	Input.InfusionLevel = 0;
	Input.HitCount = Ability->HitCount;

	// Element-infusion damage penalty removed per locked cost matrix.
	// Calculate with main function
	Result = CalculateDamage(User, Target, Input);

	return Result;
}

FDamageCalculationResult UDamageCalculator::CalculateAttackDamage(
	AActor *Attacker,
	AActor *Target,
	UWeaponAttackData *Attack,
	bool bIsInfused)
{
	FDamageCalculationResult Result;

	if (!Attack)
	{
		return Result;
	}

	UCharacterData *AttackerData = GetCharacterData(Attacker);
	if (!AttackerData)
	{
		return Result;
	}

	// Build input
	FDamageCalculationInput Input;

	// Base damage
	Input.BaseDamage = 100;
	Input.ActionType = EActionType::Attack;

	// Attacks are physical unless infused. Per locked design, infused attacks
	// still scale by RawDamage — the element only affects status routing.
	Input.Element = bIsInfused ? AttackerData->InnateElement : ESpellElement::Generic;

	Input.bCanCrit = true;
	Input.bWasInfused = bIsInfused;
	Input.HitCount = Attack->HitCount;

	// Apply weapon stats only if not infused (None = weapon stats apply)
	if (!bIsInfused)
	{
		// Get active weapon
		UWeaponManager *WM = Cast<UWeaponManager>(GetGameInstance()->GetSubsystem<UWeaponManager>());
		if (WM)
		{
			UWeaponData *Weapon = WM->GetActiveWeapon(Attacker);
			if (Weapon)
			{
				// Apply weapon damage bonus
				Input.BaseDamage += Weapon->BonusAttack;

				// Store crit bonuses for later application
				Input.OverrideCritChance = GetCriticalChance(Attacker) + (Weapon->BonusCritChance / 100.0f);
			}
		}
	}
	// Infused branch no longer applies a flat damage penalty (removed per locked
	// cost matrix; cost is paid via durability/HP/status/energy mechanics).

	// Calculate with main function
	Result = CalculateDamage(Attacker, Target, Input);

	// Apply weapon crit damage bonus if not infused and was critical
	if (!bIsInfused && Result.bWasCritical)
	{
		UWeaponManager *WM = Cast<UWeaponManager>(GetGameInstance()->GetSubsystem<UWeaponManager>());
		if (WM)
		{
			UWeaponData *Weapon = WM->GetActiveWeapon(Attacker);
			if (Weapon && Weapon->BonusCritDamage != 0.0f)
			{
				float BonusCritMult = Weapon->BonusCritDamage / 100.0f;
				Result.FinalDamage = FMath::RoundToInt(Result.FinalDamage * (1.0f + BonusCritMult));
			}
		}
	}

	return Result;
}

// ==================== COMPONENT CALCULATIONS ====================

float UDamageCalculator::GetAttackerDamageMultiplier(AActor *Attacker, EActionType ActionType) const
{
	UCharacterData *Data = GetCharacterData(Attacker);
	if (!Data)
	{
		return 1.0f;
	}

	if (ActionType == EActionType::Spell)
	{
		return Data->CalculateSpellDamage();
	}
	else
	{
		return Data->CalculateRawDamage();
	}
}

int32 UDamageCalculator::GetDefenderFlatDefense(AActor *Defender) const
{
	UCharacterData *Data = GetCharacterData(Defender);
	if (!Data)
	{
		return 0;
	}

	int32 BaseDefense = Data->CalculateFlatDefense();

	// Apply status effect modifiers
	UStatusEffectManager *StatusManager = GetStatusEffectManager();
	if (StatusManager)
	{
		float DefenseBuff = StatusManager->GetTotalStatModifier(Defender, EStatusType::DefenseBuff);
		float DefenseDebuff = StatusManager->GetTotalStatModifier(Defender, EStatusType::DefenseDebuff);

		// Buffs/debuffs are percentage modifiers
		float Modifier = 1.0f + (DefenseBuff - DefenseDebuff) / 100.0f;
		BaseDefense = FMath::RoundToInt(BaseDefense * FMath::Max(0.0f, Modifier));
	}

	return BaseDefense;
}

float UDamageCalculator::GetCriticalChance(AActor *Attacker) const
{
	UCharacterData *Data = GetCharacterData(Attacker);
	if (!Data)
	{
		return DamageConstants::BASE_CRIT_CHANCE;
	}

	float BaseCrit = Data->CalculateCritChance();

	// Apply status effect modifiers
	UStatusEffectManager *StatusManager = GetStatusEffectManager();
	if (StatusManager)
	{
		float CritBuff = StatusManager->GetTotalStatModifier(Attacker, EStatusType::CritChanceBuff);
		float CritDebuff = StatusManager->GetTotalStatModifier(Attacker, EStatusType::CritChanceDebuff);

		BaseCrit += (CritBuff - CritDebuff) / 100.0f;
	}

	return FMath::Clamp(BaseCrit, 0.0f, DamageConstants::MAX_CRIT_CHANCE);
}

bool UDamageCalculator::RollCriticalHit(AActor *Attacker, float OverrideChance) const
{
	float Chance = OverrideChance >= 0.0f ? OverrideChance : GetCriticalChance(Attacker);
	return FMath::FRand() < Chance;
}

float UDamageCalculator::GetElementInteractionMultiplier(ESpellElement AttackElement, ESpellElement DefenderElement) const
{
	if (IsWeakTo(DefenderElement, AttackElement))
	{
		return DamageConstants::WEAKNESS_MULTIPLIER;
	}

	if (ResistsElement(DefenderElement, AttackElement))
	{
		return DamageConstants::RESISTANCE_MULTIPLIER;
	}

	return DamageConstants::NEUTRAL_MULTIPLIER;
}

// ==================== STATUS EFFECT CALCULATIONS ====================

int32 UDamageCalculator::CalculateStatusBuildup(
	AActor *Attacker,
	AActor *Target,
	int32 BaseBuildup,
	ESpellElement Element)
{
	if (BaseBuildup <= 0)
	{
		return 0;
	}

	float Buildup = static_cast<float>(BaseBuildup);

	// Apply BD stack multiplier
	float BDMult = GetBDStackStatusMultiplier(Attacker, Element);
	Buildup *= BDMult;

	// TODO: Apply status effect modifiers (status potency buffs/debuffs)

	return FMath::RoundToInt(Buildup);
}

float UDamageCalculator::GetBDStackStatusMultiplier(AActor *Attacker, ESpellElement Element) const
{
	UBrokenDarknessManager *BDManager = GetBrokenDarknessManager(Attacker);
	if (!BDManager || !BDManager->IsTransformed())
	{
		return 1.0f;
	}

	// Only apply multiplier if spell element matches BD alignment
	if (Element == BDManager->GetCurrentAlignment())
	{
		return BDManager->GetStackStatusMultiplier();
	}

	return 1.0f;
}

// ==================== HEALING CALCULATIONS ====================

int32 UDamageCalculator::CalculateHealing(
	AActor *Healer,
	AActor *Target,
	int32 BaseHealing)
{
	if (BaseHealing <= 0)
	{
		return 0;
	}

	float Healing = static_cast<float>(BaseHealing);

	// Apply healer's SpellDamage multiplier. Healing is a spell-class effect — it
	// scales with the caster's spell power (Mind), not status-buildup amplification.
	UCharacterData *HealerData = GetCharacterData(Healer);
	if (HealerData)
	{
		Healing *= HealerData->CalculateSpellDamage();
	}

	// TODO: Add HealingReceivedBuff/Debuff to EStatusType when needed
	// Currently no healing modifiers in status effect system

	return FMath::Max(0, FMath::RoundToInt(Healing));
}

// ==================== UTILITY ====================

float UDamageCalculator::GetInfusionDamageMultiplier(int32 InfusionLevel)
{
	switch (InfusionLevel)
	{
	case 1:
		return DamageConstants::POWER_INFUSION_L1_MULT;
	case 2:
		return DamageConstants::POWER_INFUSION_L2_MULT;
	default:
		return 1.0f;
	}
}

bool UDamageCalculator::IsWeakTo(ESpellElement Defender, ESpellElement Attacker)
{
	// No element weakness system - all elements are neutral
	return false;
}

bool UDamageCalculator::ResistsElement(ESpellElement Defender, ESpellElement Attacker)
{
	// No element resistance system - all elements are neutral
	return false;
}

// ==================== DEBUG ====================

void UDamageCalculator::DebugPrintCalculation(const FDamageCalculationResult &Result) const
{
	UE_LOG(LogTemp, Display, TEXT("=== DAMAGE CALCULATION ==="));
	UE_LOG(LogTemp, Display, TEXT("Damage Before Defense: %d"), Result.DamageBeforeDefense);
	UE_LOG(LogTemp, Display, TEXT("Attacker Multiplier: %.2fx"), Result.AttackerDamageMultiplier);
	UE_LOG(LogTemp, Display, TEXT("Element Multiplier: %.2fx"), Result.ElementMultiplier);
	UE_LOG(LogTemp, Display, TEXT("Critical: %s (%.2fx)"), Result.bWasCritical ? TEXT("YES") : TEXT("NO"), Result.CritMultiplier);
	UE_LOG(LogTemp, Display, TEXT("Flat Defense: %d (blocked %d)"), Result.DefenderFlatDefense, Result.DamageBlockedByDefense);
	UE_LOG(LogTemp, Display, TEXT("FINAL DAMAGE: %d"), Result.FinalDamage);
	UE_LOG(LogTemp, Display, TEXT("Status Buildup: %d"), Result.StatusBuildup);
	UE_LOG(LogTemp, Display, TEXT("=========================="));
}

// ==================== PRIVATE HELPERS ====================

UCharacterData *UDamageCalculator::GetCharacterData(AActor *Actor) const
{
	if (!Actor)
	{
		return nullptr;
	}

	UCharacterDataComponent *Comp = Actor->FindComponentByClass<UCharacterDataComponent>();
	return Comp ? Comp->CharacterData : nullptr;
}

UStatusEffectManager *UDamageCalculator::GetStatusEffectManager() const
{
	if (!CachedStatusManager)
	{
		// StatusEffectManager is a GameInstanceSubsystem like DamageCalculator
		if (UGameInstance *GI = GetGameInstance())
		{
			CachedStatusManager = GI->GetSubsystem<UStatusEffectManager>();
		}
	}
	return CachedStatusManager;
}

UBrokenDarknessManager *UDamageCalculator::GetBrokenDarknessManager(AActor *Actor) const
{
	if (!Actor)
	{
		return nullptr;
	}
	return Actor->FindComponentByClass<UBrokenDarknessManager>();
}

float UDamageCalculator::GetStatusEffectDamageModifier(AActor *Attacker, AActor *Defender) const
{
	float Modifier = 1.0f;

	UStatusEffectManager *StatusManager = GetStatusEffectManager();
	if (!StatusManager)
	{
		return Modifier;
	}

	// Attacker damage buffs/debuffs
	if (Attacker)
	{
		float DamageBuff = StatusManager->GetTotalStatModifier(Attacker, EStatusType::DamageBuff);
		float DamageDebuff = StatusManager->GetTotalStatModifier(Attacker, EStatusType::DamageDebuff);
		Modifier *= (1.0f + (DamageBuff - DamageDebuff) / 100.0f);
	}

	// Defender defense modifiers affect incoming damage indirectly
	// (handled separately in defense calculations)
	// TODO: Add DamageTakenBuff/Debuff to EStatusType if needed

	return FMath::Max(0.0f, Modifier);
}

UCombatGridSubsystem *UDamageCalculator::GetCombatGridSubsystem() const
{
	if (!CachedCombatGridSubsystem)
	{
		if (UGameInstance *GI = GetGameInstance())
		{
			CachedCombatGridSubsystem = GI->GetSubsystem<UCombatGridSubsystem>();
		}
	}
	return CachedCombatGridSubsystem;
}