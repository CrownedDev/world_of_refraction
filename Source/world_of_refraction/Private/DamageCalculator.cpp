// DamageCalculator.cpp
// Centralized damage calculation implementation

#include "DamageCalculator.h"
#include "CharacterData.h"
#include "CharacterDataComponent.h"
#include "SpellData.h"
#include "AbilityData.h"
#include "BaseAttackData.h"
#include "StatusEffectManager.h"
#include "BrokenDarknessManager.h"
#include "Engine/GameInstance.h"

void UDamageCalculator::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("[DamageCalculator] Initialized"));
}

// ==================== MAIN CALCULATION ====================

FDamageCalculationResult UDamageCalculator::CalculateDamage(
	AActor* Attacker,
	AActor* Defender,
	const FDamageCalculationInput& Input)
{
	FDamageCalculationResult Result;
	Result.EffectiveElement = Input.Element;

	if (Input.BaseDamage <= 0)
	{
		return Result;
	}

	float RunningDamage = static_cast<float>(Input.BaseDamage);

	// Step 1: Attacker's damage multiplier (Effect Damage or Raw Damage)
	float AttackerMult = GetAttackerDamageMultiplier(Attacker, Input.bIsElemental);
	Result.AttackerDamageMultiplier = AttackerMult;
	RunningDamage *= AttackerMult;

	// Step 2: Status effect modifiers (buffs/debuffs)
	float StatusMod = GetStatusEffectDamageModifier(Attacker, Defender, Input.bIsElemental);
	RunningDamage *= StatusMod;

	// Step 3: Element interaction (weakness/resistance)
	if (Input.bIsElemental && Defender)
	{
		UCharacterData* DefenderData = GetCharacterData(Defender);
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

	// Step 6: Defender's elemental resistance (only for elemental damage)
	if (!Input.bIgnoreResistance && Input.bIsElemental && Defender)
	{
		Result.DefenderResistance = GetDefenderResistance(Defender);
		int32 BeforeResist = FMath::RoundToInt(RunningDamage);
		RunningDamage *= (1.0f - Result.DefenderResistance);
		Result.DamageReducedByResistance = BeforeResist - FMath::RoundToInt(RunningDamage);
	}

	// Step 7: Ensure minimum damage
	Result.FinalDamage = FMath::Max(DamageConstants::MIN_DAMAGE, FMath::RoundToInt(RunningDamage));

	// Calculate status buildup if applicable
	// (Caller should handle this separately based on spell/ability data)

	return Result;
}

FDamageCalculationResult UDamageCalculator::CalculateSpellDamage(
	AActor* Caster,
	AActor* Target,
	USpellData* Spell,
	bool bUseElementalMode,
	int32 InfusionLevel)
{
	FDamageCalculationResult Result;

	if (!Spell)
	{
		return Result;
	}

	UCharacterData* CasterData = GetCharacterData(Caster);
	if (!CasterData)
	{
		return Result;
	}

	// Build input
	FDamageCalculationInput Input;
	Input.BaseDamage = Spell->BaseDamage;
	Input.bIsElemental = bUseElementalMode;
	Input.Element = Spell->Element;
	Input.bCanCrit = true;
	Input.bWasInfused = InfusionLevel > 0;
	Input.InfusionLevel = InfusionLevel;
	Input.bIsRawMode = !bUseElementalMode;

	// Apply spell size infusion (not damage, but tracking)
	// Actual size handled elsewhere

	// Calculate with main function
	Result = CalculateDamage(Caster, Target, Input);

	// Calculate status buildup (SpellData has a method, not a property)
	if (bUseElementalMode && CasterData)
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
	AActor* User,
	AActor* Target,
	UAbilityData* Ability,
	bool bIsInfused,
	int32 PowerInfusionLevel)
{
	FDamageCalculationResult Result;

	if (!Ability)
	{
		return Result;
	}

	UCharacterData* UserData = GetCharacterData(User);
	if (!UserData)
	{
		return Result;
	}

	// Build input
	FDamageCalculationInput Input;
	Input.BaseDamage = Ability->BaseDamage;
	
	// Abilities are physical unless infused
	Input.bIsElemental = bIsInfused;
	Input.Element = bIsInfused ? UserData->InnateElement : ESpellElement::Generic;
	
	Input.bCanCrit = true;
	Input.bWasInfused = bIsInfused || PowerInfusionLevel > 0;
	Input.InfusionLevel = PowerInfusionLevel;
	Input.HitCount = Ability->HitCount;

	// Apply element infusion penalty (casters pay 30% damage for adding element)
	if (bIsInfused)
	{
		Input.BaseDamage = FMath::RoundToInt(Input.BaseDamage * DamageConstants::ELEMENT_INFUSION_PENALTY);
	}

	// Apply power infusion multiplier (Generic characters)
	if (PowerInfusionLevel > 0)
	{
		float PowerMult = GetInfusionDamageMultiplier(PowerInfusionLevel);
		Input.BaseDamage = FMath::RoundToInt(Input.BaseDamage * PowerMult);
	}

	// Calculate with main function
	Result = CalculateDamage(User, Target, Input);

	return Result;
}

FDamageCalculationResult UDamageCalculator::CalculateAttackDamage(
	AActor* Attacker,
	AActor* Target,
	UBaseAttackData* Attack,
	bool bIsInfused)
{
	FDamageCalculationResult Result;

	if (!Attack)
	{
		return Result;
	}

	UCharacterData* AttackerData = GetCharacterData(Attacker);
	if (!AttackerData)
	{
		return Result;
	}

	// Build input
	FDamageCalculationInput Input;
	
	// Attacks use base 100 × RawDamageMultiplier (from ActionExecutor pattern)
	// The RawDamageMultiplier will be applied again in main calculation,
	// so we just use base 100 here
	Input.BaseDamage = 100;
	
	// Attacks are physical unless infused
	Input.bIsElemental = bIsInfused;
	Input.Element = bIsInfused ? AttackerData->InnateElement : ESpellElement::Generic;
	
	Input.bCanCrit = true;
	Input.bWasInfused = bIsInfused;
	Input.HitCount = Attack->HitCount;

	// Infusion penalty (30% damage reduction)
	if (bIsInfused)
	{
		Input.BaseDamage = FMath::RoundToInt(Input.BaseDamage * 0.7f);
	}

	// Calculate with main function
	Result = CalculateDamage(Attacker, Target, Input);

	return Result;
}

// ==================== COMPONENT CALCULATIONS ====================

float UDamageCalculator::GetAttackerDamageMultiplier(AActor* Attacker, bool bIsElemental) const
{
	UCharacterData* Data = GetCharacterData(Attacker);
	if (!Data)
	{
		return 1.0f;
	}

	if (bIsElemental)
	{
		return Data->CalculateEffectDamageMultiplier();
	}
	else
	{
		return Data->CalculateRawDamageMultiplier();
	}
}

int32 UDamageCalculator::GetDefenderFlatDefense(AActor* Defender) const
{
	UCharacterData* Data = GetCharacterData(Defender);
	if (!Data)
	{
		return 0;
	}

	int32 BaseDefense = Data->CalculateFlatDefense();

	// Apply status effect modifiers
	UStatusEffectManager* StatusManager = GetStatusEffectManager();
	if (StatusManager)
	{
		float DefenseBuff = StatusManager->GetTotalStatModifier(Defender, EAbilityEffectType::DefenseBuff);
		float DefenseDebuff = StatusManager->GetTotalStatModifier(Defender, EAbilityEffectType::DefenseDebuff);
		
		// Buffs/debuffs are percentage modifiers
		float Modifier = 1.0f + (DefenseBuff - DefenseDebuff) / 100.0f;
		BaseDefense = FMath::RoundToInt(BaseDefense * FMath::Max(0.0f, Modifier));
	}

	return BaseDefense;
}

float UDamageCalculator::GetDefenderResistance(AActor* Defender) const
{
	UCharacterData* Data = GetCharacterData(Defender);
	if (!Data)
	{
		return 0.0f;
	}

	float BaseResistance = Data->CalculateElementalResistance();

	// Apply status effect modifiers
	UStatusEffectManager* StatusManager = GetStatusEffectManager();
	if (StatusManager)
	{
		float ResBuff = StatusManager->GetTotalStatModifier(Defender, EAbilityEffectType::ResistanceBuff);
		float ResDebuff = StatusManager->GetTotalStatModifier(Defender, EAbilityEffectType::ResistanceDebuff);
		
		BaseResistance += (ResBuff - ResDebuff) / 100.0f;
	}

	return FMath::Clamp(BaseResistance, 0.0f, DamageConstants::MAX_RESISTANCE);
}

float UDamageCalculator::GetCriticalChance(AActor* Attacker) const
{
	UCharacterData* Data = GetCharacterData(Attacker);
	if (!Data)
	{
		return DamageConstants::BASE_CRIT_CHANCE;
	}

	float BaseCrit = Data->CalculateCritChance();

	// Apply status effect modifiers
	UStatusEffectManager* StatusManager = GetStatusEffectManager();
	if (StatusManager)
	{
		float CritBuff = StatusManager->GetTotalStatModifier(Attacker, EAbilityEffectType::CritChanceBuff);
		float CritDebuff = StatusManager->GetTotalStatModifier(Attacker, EAbilityEffectType::CritChanceDebuff);
		
		BaseCrit += (CritBuff - CritDebuff) / 100.0f;
	}

	return FMath::Clamp(BaseCrit, 0.0f, DamageConstants::MAX_CRIT_CHANCE);
}

bool UDamageCalculator::RollCriticalHit(AActor* Attacker, float OverrideChance) const
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
	AActor* Attacker,
	AActor* Target,
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

float UDamageCalculator::GetBDStackStatusMultiplier(AActor* Attacker, ESpellElement Element) const
{
	UBrokenDarknessManager* BDManager = GetBrokenDarknessManager(Attacker);
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
	AActor* Healer,
	AActor* Target,
	int32 BaseHealing)
{
	if (BaseHealing <= 0)
	{
		return 0;
	}

	float Healing = static_cast<float>(BaseHealing);

	// Apply healer's Effect Damage multiplier (healing scales with spell power)
	UCharacterData* HealerData = GetCharacterData(Healer);
	if (HealerData)
	{
		Healing *= HealerData->CalculateEffectDamageMultiplier();
	}

	// TODO: Add HealingReceivedBuff/Debuff to EAbilityEffectType when needed
	// Currently no healing modifiers in status effect system

	return FMath::Max(0, FMath::RoundToInt(Healing));
}

// ==================== UTILITY ====================

int32 UDamageCalculator::ApplyDefenseToValue(
	int32 Damage,
	int32 FlatDefense,
	float Resistance,
	bool bIsElemental) const
{
	float Result = static_cast<float>(Damage);

	// Flat defense (subtraction)
	Result -= FlatDefense;
	Result = FMath::Max(0.0f, Result);

	// Resistance (percentage, only for elemental)
	if (bIsElemental)
	{
		Result *= (1.0f - FMath::Clamp(Resistance, 0.0f, DamageConstants::MAX_RESISTANCE));
	}

	return FMath::Max(DamageConstants::MIN_DAMAGE, FMath::RoundToInt(Result));
}

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

void UDamageCalculator::DebugPrintCalculation(const FDamageCalculationResult& Result) const
{
	UE_LOG(LogTemp, Display, TEXT("=== DAMAGE CALCULATION ==="));
	UE_LOG(LogTemp, Display, TEXT("Damage Before Defense: %d"), Result.DamageBeforeDefense);
	UE_LOG(LogTemp, Display, TEXT("Attacker Multiplier: %.2fx"), Result.AttackerDamageMultiplier);
	UE_LOG(LogTemp, Display, TEXT("Element Multiplier: %.2fx"), Result.ElementMultiplier);
	UE_LOG(LogTemp, Display, TEXT("Critical: %s (%.2fx)"), Result.bWasCritical ? TEXT("YES") : TEXT("NO"), Result.CritMultiplier);
	UE_LOG(LogTemp, Display, TEXT("Flat Defense: %d (blocked %d)"), Result.DefenderFlatDefense, Result.DamageBlockedByDefense);
	UE_LOG(LogTemp, Display, TEXT("Resistance: %.1f%% (reduced %d)"), Result.DefenderResistance * 100.0f, Result.DamageReducedByResistance);
	UE_LOG(LogTemp, Display, TEXT("FINAL DAMAGE: %d"), Result.FinalDamage);
	UE_LOG(LogTemp, Display, TEXT("Status Buildup: %d"), Result.StatusBuildup);
	UE_LOG(LogTemp, Display, TEXT("=========================="));
}

// ==================== PRIVATE HELPERS ====================

UCharacterData* UDamageCalculator::GetCharacterData(AActor* Actor) const
{
	if (!Actor)
	{
		return nullptr;
	}

	UCharacterDataComponent* Comp = Actor->FindComponentByClass<UCharacterDataComponent>();
	return Comp ? Comp->CharacterData : nullptr;
}

UStatusEffectManager* UDamageCalculator::GetStatusEffectManager() const
{
	if (!CachedStatusManager)
	{
		// StatusEffectManager is a GameInstanceSubsystem like DamageCalculator
		if (UGameInstance* GI = GetGameInstance())
		{
			CachedStatusManager = GI->GetSubsystem<UStatusEffectManager>();
		}
	}
	return CachedStatusManager;
}

UBrokenDarknessManager* UDamageCalculator::GetBrokenDarknessManager(AActor* Actor) const
{
	if (!Actor)
	{
		return nullptr;
	}
	return Actor->FindComponentByClass<UBrokenDarknessManager>();
}

float UDamageCalculator::GetStatusEffectDamageModifier(AActor* Attacker, AActor* Defender, bool bIsElemental) const
{
	float Modifier = 1.0f;

	UStatusEffectManager* StatusManager = GetStatusEffectManager();
	if (!StatusManager)
	{
		return Modifier;
	}

	// Attacker damage buffs/debuffs
	if (Attacker)
	{
		float DamageBuff = StatusManager->GetTotalStatModifier(Attacker, EAbilityEffectType::DamageBuff);
		float DamageDebuff = StatusManager->GetTotalStatModifier(Attacker, EAbilityEffectType::DamageDebuff);
		Modifier *= (1.0f + (DamageBuff - DamageDebuff) / 100.0f);
	}

	// Defender defense modifiers affect incoming damage indirectly
	// (handled separately in defense calculations)
	// TODO: Add DamageTakenBuff/Debuff to EAbilityEffectType if needed

	return FMath::Max(0.0f, Modifier);
}
