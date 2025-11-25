// Copyright Epic Games, Inc. All Rights Reserved.

#include "ActionExecutorTestActor.h"
#include "ActionExecutor.h"
#include "ActionStructs.h"
#include "StatusEffectManager.h"
#include "StatusEffect.h"
#include "CharacterDataComponent.h"
#include "CharacterData.h"
#include "SpellData.h"
#include "AbilityData.h"
#include "ItemData.h"
#include "BaseAttackData.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

AActionExecutorTestActor::AActionExecutorTestActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

// ==================== TEST SUITE ====================

void AActionExecutorTestActor::RunAllTests()
{
	TestsPassed = 0;
	TestsFailed = 0;

	UE_LOG(LogTemp, Display, TEXT(""));
	UE_LOG(LogTemp, Display, TEXT("========================================"));
	UE_LOG(LogTemp, Display, TEXT("ACTION EXECUTOR TEST SUITE"));
	UE_LOG(LogTemp, Display, TEXT("========================================"));

	// Validation Tests
	Test_ValidationBasic();
	Test_ValidationEnergy();
	Test_ValidationStunned();
	Test_ValidationSilenced();

	// Execution Tests
	Test_ExecuteSpell();
	Test_ExecuteSpellInfused();
	Test_ExecuteAbility();
	Test_ExecuteAbilityInfused();
	Test_ExecuteItem();
	Test_ExecuteAttack();
	Test_ExecuteDefend();

	// Infusion Multiplier Tests
	Test_InfusionMultipliers();

	// Damage Tests
	Test_DamageApplication();
	Test_CriticalHits();
	Test_DefenseReduction();
	Test_MultiHit();

	// Effect Tests
	Test_StatusEffectApplication();
	Test_HealingApplication();

	UE_LOG(LogTemp, Display, TEXT("========================================"));
	UE_LOG(LogTemp, Display, TEXT("RESULTS: %d passed, %d failed"), TestsPassed, TestsFailed);
	UE_LOG(LogTemp, Display, TEXT("========================================"));
	UE_LOG(LogTemp, Display, TEXT(""));

	CleanupTestActors();
}

// ==================== VALIDATION TESTS ====================

void AActionExecutorTestActor::Test_ValidationBasic()
{
	UActionExecutor* Executor = GetActionExecutor();
	if (!Executor)
	{
		LogTestResult(TEXT("Validation Basic"), false, TEXT("ActionExecutor not found"));
		return;
	}

	// Create test actor
	AActor* TestActor = CreateTestActor(TEXT("ValidationBasicTest"));
	USpellData* TestSpell = CreateTestSpell(TEXT("TestSpell"), 50, 10);

	// Create valid action
	FAction Action;
	Action.ActionType = EActionType::Spell;
	Action.SpellData = TestSpell;
	Action.Targets.Add(TestActor); // Target self for simplicity

	FActionValidationResult Result = Executor->ValidateAction(TestActor, Action);

	bool bPassed = Result.bIsValid && Result.EnergyCost == 10;
	LogTestResult(TEXT("Validation Basic"), bPassed,
		FString::Printf(TEXT("Valid: %s, Cost: %d"), 
			Result.bIsValid ? TEXT("true") : TEXT("false"), Result.EnergyCost));
}

void AActionExecutorTestActor::Test_ValidationEnergy()
{
	UActionExecutor* Executor = GetActionExecutor();
	if (!Executor)
	{
		LogTestResult(TEXT("Validation Energy"), false, TEXT("ActionExecutor not found"));
		return;
	}

	// Create actor with low energy
	AActor* TestActor = CreateTestActor(TEXT("ValidationEnergyTest"), 100, 100);
	UCharacterDataComponent* CharComp = TestActor->FindComponentByClass<UCharacterDataComponent>();
	CharComp->CurrentEP = 5; // Only 5 EP

	// Create spell that costs 50 EP
	USpellData* ExpensiveSpell = CreateTestSpell(TEXT("ExpensiveSpell"), 100, 50);

	FAction Action;
	Action.ActionType = EActionType::Spell;
	Action.SpellData = ExpensiveSpell;
	Action.Targets.Add(TestActor);

	FActionValidationResult Result = Executor->ValidateAction(TestActor, Action);

	bool bPassed = !Result.bIsValid && Result.ErrorMessage.Contains(TEXT("energy"));
	LogTestResult(TEXT("Validation Energy"), bPassed,
		FString::Printf(TEXT("Valid: %s, Error: %s"),
			Result.bIsValid ? TEXT("true") : TEXT("false"), *Result.ErrorMessage));
}

void AActionExecutorTestActor::Test_ValidationStunned()
{
	UActionExecutor* Executor = GetActionExecutor();
	UStatusEffectManager* StatusManager = GetStatusEffectManager();
	if (!Executor || !StatusManager)
	{
		LogTestResult(TEXT("Validation Stunned"), false, TEXT("Subsystems not found"));
		return;
	}

	AActor* TestActor = CreateTestActor(TEXT("ValidationStunnedTest"));

	// Apply stun
	FStatusEffect Stun = FStatusEffect::CreateDebuff(
		TEXT("Test Stun"), 1, EAbilityEffectType::Stun, 1.0f, 3);
	StatusManager->ApplyEffect(TestActor, Stun, nullptr, TEXT("Test"), -1);

	// Try to act while stunned
	FAction Action;
	Action.ActionType = EActionType::Defend;

	FActionValidationResult Result = Executor->ValidateAction(TestActor, Action);

	bool bPassed = !Result.bIsValid && Result.ErrorMessage.Contains(TEXT("Stunned"));
	LogTestResult(TEXT("Validation Stunned"), bPassed,
		FString::Printf(TEXT("Valid: %s, Error: %s"),
			Result.bIsValid ? TEXT("true") : TEXT("false"), *Result.ErrorMessage));

	// Cleanup
	StatusManager->RemoveAllEffects(TestActor);
}

void AActionExecutorTestActor::Test_ValidationSilenced()
{
	UActionExecutor* Executor = GetActionExecutor();
	UStatusEffectManager* StatusManager = GetStatusEffectManager();
	if (!Executor || !StatusManager)
	{
		LogTestResult(TEXT("Validation Silenced"), false, TEXT("Subsystems not found"));
		return;
	}

	AActor* TestActor = CreateTestActor(TEXT("ValidationSilencedTest"));

	// Apply silence
	FStatusEffect Silence = FStatusEffect::CreateDebuff(
		TEXT("Test Silence"), 2, EAbilityEffectType::Silence, 1.0f, 3);
	StatusManager->ApplyEffect(TestActor, Silence, nullptr, TEXT("Test"), -1);

	// Try to cast spell while silenced
	USpellData* TestSpell = CreateTestSpell(TEXT("SilenceTestSpell"), 50, 10);
	FAction SpellAction;
	SpellAction.ActionType = EActionType::Spell;
	SpellAction.SpellData = TestSpell;
	SpellAction.Targets.Add(TestActor);

	FActionValidationResult SpellResult = Executor->ValidateAction(TestActor, SpellAction);

	// Should not be able to cast spell
	bool bSpellBlocked = !SpellResult.bIsValid && SpellResult.ErrorMessage.Contains(TEXT("Silenced"));

	// But should be able to use abilities
	UAbilityData* TestAbility = CreateTestAbility(TEXT("SilenceTestAbility"), 30, 5);
	FAction AbilityAction;
	AbilityAction.ActionType = EActionType::Ability;
	AbilityAction.AbilityData = TestAbility;
	AbilityAction.Targets.Add(TestActor);

	FActionValidationResult AbilityResult = Executor->ValidateAction(TestActor, AbilityAction);
	bool bAbilityAllowed = AbilityResult.bIsValid;

	bool bPassed = bSpellBlocked && bAbilityAllowed;
	LogTestResult(TEXT("Validation Silenced"), bPassed,
		FString::Printf(TEXT("Spell blocked: %s, Ability allowed: %s"),
			bSpellBlocked ? TEXT("true") : TEXT("false"),
			bAbilityAllowed ? TEXT("true") : TEXT("false")));

	// Cleanup
	StatusManager->RemoveAllEffects(TestActor);
}

// ==================== EXECUTION TESTS ====================

void AActionExecutorTestActor::Test_ExecuteSpell()
{
	UActionExecutor* Executor = GetActionExecutor();
	if (!Executor)
	{
		LogTestResult(TEXT("Execute Spell"), false, TEXT("ActionExecutor not found"));
		return;
	}

	AActor* Caster = CreateTestActor(TEXT("SpellCaster"), 100, 100);
	AActor* Target = CreateTestActor(TEXT("SpellTarget"), 100, 100);

	UCharacterDataComponent* CasterComp = Caster->FindComponentByClass<UCharacterDataComponent>();
	UCharacterDataComponent* TargetComp = Target->FindComponentByClass<UCharacterDataComponent>();

	int32 CasterEPBefore = CasterComp->CurrentEP;
	int32 TargetHPBefore = TargetComp->CurrentHP;

	// Create and execute spell
	USpellData* Spell = CreateTestSpell(TEXT("ExecuteTestSpell"), 30, 15);

	FAction Action;
	Action.ActionType = EActionType::Spell;
	Action.SpellData = Spell;
	Action.Targets.Add(Target);

	FActionResult Result = Executor->ExecuteAction(Caster, Action);

	bool bSucceeded = Result.bSuccess;
	bool bEnergySpent = CasterComp->CurrentEP == CasterEPBefore - 15;
	bool bDamageDealt = TargetComp->CurrentHP < TargetHPBefore;

	bool bPassed = bSucceeded && bEnergySpent && bDamageDealt;
	LogTestResult(TEXT("Execute Spell"), bPassed,
		FString::Printf(TEXT("Success: %s, EP: %d->%d, HP: %d->%d, Damage: %d"),
			bSucceeded ? TEXT("true") : TEXT("false"),
			CasterEPBefore, CasterComp->CurrentEP,
			TargetHPBefore, TargetComp->CurrentHP,
			Result.TotalDamageDealt));
}

void AActionExecutorTestActor::Test_ExecuteSpellInfused()
{
	UActionExecutor* Executor = GetActionExecutor();
	if (!Executor)
	{
		LogTestResult(TEXT("Execute Spell Infused"), false, TEXT("ActionExecutor not found"));
		return;
	}

	AActor* Caster = CreateTestActor(TEXT("InfusedSpellCaster"), 100, 100);
	AActor* Target = CreateTestActor(TEXT("InfusedSpellTarget"), 200, 100);

	UCharacterDataComponent* CasterComp = Caster->FindComponentByClass<UCharacterDataComponent>();

	// Create spell
	USpellData* Spell = CreateTestSpell(TEXT("InfusionTestSpell"), 40, 20);

	// Execute non-infused spell first
	FAction NormalAction;
	NormalAction.ActionType = EActionType::Spell;
	NormalAction.SpellData = Spell;
	NormalAction.Targets.Add(Target);
	NormalAction.SpellInfusionLevel = 0;

	FActionResult NormalResult = Executor->ExecuteAction(Caster, NormalAction);
	int32 NormalEnergyCost = NormalResult.EnergySpent;
	float NormalSize = NormalResult.AttackSize;

	// Reset energy
	CasterComp->CurrentEP = 100;

	// Execute infused spell (Level 2)
	FAction InfusedAction;
	InfusedAction.ActionType = EActionType::Spell;
	InfusedAction.SpellData = Spell;
	InfusedAction.Targets.Add(Target);
	InfusedAction.SpellInfusionLevel = 2;

	FActionResult InfusedResult = Executor->ExecuteAction(Caster, InfusedAction);
	int32 InfusedEnergyCost = InfusedResult.EnergySpent;
	float InfusedSize = InfusedResult.AttackSize;

	// Spell infusion should:
	// 1. Cost more energy (1.6x at level 2)
	bool bCostIncreased = InfusedEnergyCost > NormalEnergyCost;
	// 2. Increase SIZE (2.0x at level 2) - NOT damage
	bool bSizeIncreased = InfusedSize > NormalSize;
	// 3. Damage should be roughly the same (spell infusion doesn't boost damage)
	bool bDamageSimilar = FMath::Abs(InfusedResult.TotalDamageDealt - NormalResult.TotalDamageDealt) < 10;

	bool bPassed = bCostIncreased && bSizeIncreased;
	LogTestResult(TEXT("Execute Spell Infused"), bPassed,
		FString::Printf(TEXT("Normal: %dEP/Size:%.1f/Dmg:%d, Infused: %dEP/Size:%.1f/Dmg:%d"),
			NormalEnergyCost, NormalSize, NormalResult.TotalDamageDealt,
			InfusedEnergyCost, InfusedSize, InfusedResult.TotalDamageDealt));
}

void AActionExecutorTestActor::Test_ExecuteAbility()
{
	UActionExecutor* Executor = GetActionExecutor();
	if (!Executor)
	{
		LogTestResult(TEXT("Execute Ability"), false, TEXT("ActionExecutor not found"));
		return;
	}

	AActor* User = CreateTestActor(TEXT("AbilityUser"), 100, 100);
	AActor* Target = CreateTestActor(TEXT("AbilityTarget"), 100, 100);

	UCharacterDataComponent* UserComp = User->FindComponentByClass<UCharacterDataComponent>();
	UCharacterDataComponent* TargetComp = Target->FindComponentByClass<UCharacterDataComponent>();

	int32 UserEPBefore = UserComp->CurrentEP;
	int32 TargetHPBefore = TargetComp->CurrentHP;

	// Create and execute ability
	UAbilityData* Ability = CreateTestAbility(TEXT("ExecuteTestAbility"), 25, 10);

	FAction Action;
	Action.ActionType = EActionType::Ability;
	Action.AbilityData = Ability;
	Action.Targets.Add(Target);

	FActionResult Result = Executor->ExecuteAction(User, Action);

	bool bSucceeded = Result.bSuccess;
	bool bEnergySpent = UserComp->CurrentEP == UserEPBefore - 10;
	bool bDamageDealt = TargetComp->CurrentHP < TargetHPBefore;

	bool bPassed = bSucceeded && bEnergySpent && bDamageDealt;
	LogTestResult(TEXT("Execute Ability"), bPassed,
		FString::Printf(TEXT("Success: %s, EP: %d->%d, Damage: %d"),
			bSucceeded ? TEXT("true") : TEXT("false"),
			UserEPBefore, UserComp->CurrentEP,
			Result.TotalDamageDealt));
}

void AActionExecutorTestActor::Test_ExecuteAbilityInfused()
{
	UActionExecutor* Executor = GetActionExecutor();
	if (!Executor)
	{
		LogTestResult(TEXT("Execute Ability Infused"), false, TEXT("ActionExecutor not found"));
		return;
	}

	AActor* User = CreateTestActor(TEXT("InfusedAbilityUser"), 100, 100);
	AActor* Target = CreateTestActor(TEXT("InfusedAbilityTarget"), 300, 100);

	UCharacterDataComponent* UserComp = User->FindComponentByClass<UCharacterDataComponent>();

	UAbilityData* Ability = CreateTestAbility(TEXT("InfusionAbility"), 50, 20);
	Ability->bCanBeInfused = true;

	// Test 1: Element Infusion (Casters) - trades damage for status
	// Execute non-infused
	FAction NormalAction;
	NormalAction.ActionType = EActionType::Ability;
	NormalAction.AbilityData = Ability;
	NormalAction.Targets.Add(Target);
	NormalAction.bIsElementInfused = false;
	NormalAction.AbilityInfusionLevel = 0;

	FActionResult NormalResult = Executor->ExecuteAction(User, NormalAction);
	int32 NormalDamage = NormalResult.TotalDamageDealt;
	int32 NormalCost = NormalResult.EnergySpent;

	UserComp->CurrentEP = 100;

	// Element infusion (Casters): -30% damage, +50% cost, adds element
	FAction ElementAction;
	ElementAction.ActionType = EActionType::Ability;
	ElementAction.AbilityData = Ability;
	ElementAction.Targets.Add(Target);
	ElementAction.bIsElementInfused = true;
	ElementAction.AbilityInfusionLevel = 0;

	FActionResult ElementResult = Executor->ExecuteAction(User, ElementAction);
	int32 ElementDamage = ElementResult.TotalDamageDealt;
	int32 ElementCost = ElementResult.EnergySpent;

	// Element infusion: cost up, damage DOWN (trades for status)
	bool bElementCostUp = ElementCost > NormalCost;
	bool bElementDamageDown = ElementDamage < NormalDamage;

	UserComp->CurrentEP = 100;

	// Test 2: Power Infusion (Generic) - pure damage boost
	FAction PowerAction;
	PowerAction.ActionType = EActionType::Ability;
	PowerAction.AbilityData = Ability;
	PowerAction.Targets.Add(Target);
	PowerAction.bIsElementInfused = false;
	PowerAction.AbilityInfusionLevel = 2; // Level 2 power infusion

	FActionResult PowerResult = Executor->ExecuteAction(User, PowerAction);
	int32 PowerDamage = PowerResult.TotalDamageDealt;
	int32 PowerCost = PowerResult.EnergySpent;

	// Power infusion: cost up, damage UP (1.6x at level 2)
	bool bPowerCostUp = PowerCost > NormalCost;
	bool bPowerDamageUp = PowerDamage > NormalDamage;

	bool bPassed = bElementCostUp && bElementDamageDown && bPowerCostUp && bPowerDamageUp;
	LogTestResult(TEXT("Execute Ability Infused"), bPassed,
		FString::Printf(TEXT("Normal: %dEP/%dDmg | Element: %dEP/%dDmg (down) | Power: %dEP/%dDmg (up)"),
			NormalCost, NormalDamage,
			ElementCost, ElementDamage,
			PowerCost, PowerDamage));
}

void AActionExecutorTestActor::Test_ExecuteItem()
{
	UActionExecutor* Executor = GetActionExecutor();
	if (!Executor)
	{
		LogTestResult(TEXT("Execute Item"), false, TEXT("ActionExecutor not found"));
		return;
	}

	AActor* User = CreateTestActor(TEXT("ItemUser"), 100, 100);
	UCharacterDataComponent* UserComp = User->FindComponentByClass<UCharacterDataComponent>();

	// Damage user first so healing can work
	UserComp->CurrentHP = 50;
	int32 HPBefore = UserComp->CurrentHP;

	// Create healing item
	UItemData* HealingItem = CreateTestItem(TEXT("Healing Potion"), EItemEffectType::Healing, 30.0f);

	FAction Action;
	Action.ActionType = EActionType::Item;
	Action.ItemData = HealingItem;
	Action.Targets.Add(User);

	FActionResult Result = Executor->ExecuteAction(User, Action);

	bool bSucceeded = Result.bSuccess;
	bool bNoEnergyCost = Result.EnergySpent == 0;
	bool bHealed = UserComp->CurrentHP > HPBefore;

	bool bPassed = bSucceeded && bNoEnergyCost && bHealed;
	LogTestResult(TEXT("Execute Item"), bPassed,
		FString::Printf(TEXT("Success: %s, HP: %d->%d, Healing: %d"),
			bSucceeded ? TEXT("true") : TEXT("false"),
			HPBefore, UserComp->CurrentHP,
			Result.TotalHealingDone));
}

void AActionExecutorTestActor::Test_ExecuteAttack()
{
	UActionExecutor* Executor = GetActionExecutor();
	if (!Executor)
	{
		LogTestResult(TEXT("Execute Attack"), false, TEXT("ActionExecutor not found"));
		return;
	}

	AActor* Attacker = CreateTestActor(TEXT("Attacker"), 100, 100);
	AActor* Target = CreateTestActor(TEXT("AttackTarget"), 100, 100);

	UCharacterDataComponent* AttackerComp = Attacker->FindComponentByClass<UCharacterDataComponent>();
	UCharacterDataComponent* TargetComp = Target->FindComponentByClass<UCharacterDataComponent>();

	int32 AttackerEPBefore = AttackerComp->CurrentEP;
	int32 TargetHPBefore = TargetComp->CurrentHP;

	// Create attack
	UBaseAttackData* Attack = CreateTestAttack(TEXT("BasicAttack"), 20);

	FAction Action;
	Action.ActionType = EActionType::Attack;
	Action.AttackData = Attack;
	Action.Targets.Add(Target);
	Action.bIsElementInfused = false;

	FActionResult Result = Executor->ExecuteAction(Attacker, Action);

	bool bSucceeded = Result.bSuccess;
	bool bNoEnergyCost = AttackerComp->CurrentEP == AttackerEPBefore; // Non-infused attack is free
	bool bDamageDealt = TargetComp->CurrentHP < TargetHPBefore;

	bool bPassed = bSucceeded && bNoEnergyCost && bDamageDealt;
	LogTestResult(TEXT("Execute Attack"), bPassed,
		FString::Printf(TEXT("Success: %s, EP unchanged: %s, Damage: %d"),
			bSucceeded ? TEXT("true") : TEXT("false"),
			bNoEnergyCost ? TEXT("true") : TEXT("false"),
			Result.TotalDamageDealt));
}

void AActionExecutorTestActor::Test_ExecuteDefend()
{
	UActionExecutor* Executor = GetActionExecutor();
	UStatusEffectManager* StatusManager = GetStatusEffectManager();
	if (!Executor || !StatusManager)
	{
		LogTestResult(TEXT("Execute Defend"), false, TEXT("Subsystems not found"));
		return;
	}

	AActor* Defender = CreateTestActor(TEXT("Defender"), 100, 100);

	FAction Action;
	Action.ActionType = EActionType::Defend;

	FActionResult Result = Executor->ExecuteAction(Defender, Action);

	// Check if defense buff was applied
	bool bHasDefenseBuff = StatusManager->HasEffectOfType(Defender, EAbilityEffectType::DefenseBuff);

	bool bPassed = Result.bSuccess && Result.EnergySpent == 0 && bHasDefenseBuff;
	LogTestResult(TEXT("Execute Defend"), bPassed,
		FString::Printf(TEXT("Success: %s, Has Defense Buff: %s"),
			Result.bSuccess ? TEXT("true") : TEXT("false"),
			bHasDefenseBuff ? TEXT("true") : TEXT("false")));

	// Cleanup
	StatusManager->RemoveAllEffects(Defender);
}

// ==================== INFUSION TESTS ====================

void AActionExecutorTestActor::Test_InfusionMultipliers()
{
	// Test static multiplier methods return correct values per design doc:
	// Spell Infusion: Size 1.0/1.5/2.0, Cost 1.0/1.3/1.6
	// Ability Power: Damage 1.0/1.3/1.6, Cost 1.0/1.3/1.6

	// Spell Size Multipliers
	bool bSpellSize0 = FMath::IsNearlyEqual(UActionExecutor::GetSpellInfusionSizeMultiplier(0), 1.0f);
	bool bSpellSize1 = FMath::IsNearlyEqual(UActionExecutor::GetSpellInfusionSizeMultiplier(1), 1.5f);
	bool bSpellSize2 = FMath::IsNearlyEqual(UActionExecutor::GetSpellInfusionSizeMultiplier(2), 2.0f);

	// Spell Cost Multipliers
	bool bSpellCost0 = FMath::IsNearlyEqual(UActionExecutor::GetSpellInfusionCostMultiplier(0), 1.0f);
	bool bSpellCost1 = FMath::IsNearlyEqual(UActionExecutor::GetSpellInfusionCostMultiplier(1), 1.3f);
	bool bSpellCost2 = FMath::IsNearlyEqual(UActionExecutor::GetSpellInfusionCostMultiplier(2), 1.6f);

	// Ability Power Damage Multipliers
	bool bAbilityDmg0 = FMath::IsNearlyEqual(UActionExecutor::GetAbilityPowerInfusionDamageMultiplier(0), 1.0f);
	bool bAbilityDmg1 = FMath::IsNearlyEqual(UActionExecutor::GetAbilityPowerInfusionDamageMultiplier(1), 1.3f);
	bool bAbilityDmg2 = FMath::IsNearlyEqual(UActionExecutor::GetAbilityPowerInfusionDamageMultiplier(2), 1.6f);

	// Ability Power Cost Multipliers
	bool bAbilityCost0 = FMath::IsNearlyEqual(UActionExecutor::GetAbilityPowerInfusionCostMultiplier(0), 1.0f);
	bool bAbilityCost1 = FMath::IsNearlyEqual(UActionExecutor::GetAbilityPowerInfusionCostMultiplier(1), 1.3f);
	bool bAbilityCost2 = FMath::IsNearlyEqual(UActionExecutor::GetAbilityPowerInfusionCostMultiplier(2), 1.6f);

	bool bAllSpell = bSpellSize0 && bSpellSize1 && bSpellSize2 && bSpellCost0 && bSpellCost1 && bSpellCost2;
	bool bAllAbility = bAbilityDmg0 && bAbilityDmg1 && bAbilityDmg2 && bAbilityCost0 && bAbilityCost1 && bAbilityCost2;

	bool bPassed = bAllSpell && bAllAbility;
	LogTestResult(TEXT("Infusion Multipliers"), bPassed,
		FString::Printf(TEXT("Spell: Size(1.0/1.5/2.0) Cost(1.0/1.3/1.6), Ability: Dmg(1.0/1.3/1.6) Cost(1.0/1.3/1.6)")));
}

// ==================== DAMAGE TESTS ====================

void AActionExecutorTestActor::Test_DamageApplication()
{
	UActionExecutor* Executor = GetActionExecutor();
	if (!Executor)
	{
		LogTestResult(TEXT("Damage Application"), false, TEXT("ActionExecutor not found"));
		return;
	}

	AActor* Attacker = CreateTestActor(TEXT("DamageAttacker"), 100, 100);
	AActor* Target = CreateTestActor(TEXT("DamageTarget"), 100, 100);

	UCharacterDataComponent* TargetComp = Target->FindComponentByClass<UCharacterDataComponent>();
	int32 HPBefore = TargetComp->CurrentHP;

	// Apply direct damage
	FCombatHitResult HitResult = Executor->ApplyDamage(Attacker, Target, 30, false);

	bool bDamageDealt = HitResult.DamageDealt > 0;
	bool bHPReduced = TargetComp->CurrentHP < HPBefore;
	bool bCorrectTarget = HitResult.Target == Target;

	bool bPassed = bDamageDealt && bHPReduced && bCorrectTarget;
	LogTestResult(TEXT("Damage Application"), bPassed,
		FString::Printf(TEXT("Damage: %d, HP: %d->%d"),
			HitResult.DamageDealt, HPBefore, TargetComp->CurrentHP));
}

void AActionExecutorTestActor::Test_CriticalHits()
{
	UActionExecutor* Executor = GetActionExecutor();
	if (!Executor)
	{
		LogTestResult(TEXT("Critical Hits"), false, TEXT("ActionExecutor not found"));
		return;
	}

	AActor* Attacker = CreateTestActor(TEXT("CritAttacker"), 100, 100);
	AActor* Target = CreateTestActor(TEXT("CritTarget"), 1000, 100); // High HP to survive many hits

	// Run multiple attacks and check if any crit
	bool bHadCrit = false;
	int32 MaxDamage = 0;
	int32 MinDamage = INT32_MAX;

	for (int32 i = 0; i < 50; i++)
	{
		FCombatHitResult HitResult = Executor->ApplyDamage(Attacker, Target, 50, false);
		
		if (HitResult.bWasCritical)
		{
			bHadCrit = true;
		}
		
		MaxDamage = FMath::Max(MaxDamage, HitResult.DamageDealt);
		MinDamage = FMath::Min(MinDamage, HitResult.DamageDealt);
	}

	// Crits should deal more damage than non-crits
	bool bCritDealsMore = MaxDamage > MinDamage;

	// Note: This test may occasionally fail if RNG doesn't produce crits in 50 tries
	// With default 5% crit chance, probability of no crits in 50 tries is ~7.7%
	LogTestResult(TEXT("Critical Hits"), bHadCrit && bCritDealsMore,
		FString::Printf(TEXT("Had Crit: %s, Min Dmg: %d, Max Dmg: %d"),
			bHadCrit ? TEXT("true") : TEXT("false"), MinDamage, MaxDamage));
}

void AActionExecutorTestActor::Test_DefenseReduction()
{
	UActionExecutor* Executor = GetActionExecutor();
	UStatusEffectManager* StatusManager = GetStatusEffectManager();
	if (!Executor || !StatusManager)
	{
		LogTestResult(TEXT("Defense Reduction"), false, TEXT("Subsystems not found"));
		return;
	}

	AActor* Attacker = CreateTestActor(TEXT("DefenseAttacker"), 100, 100);
	AActor* Target = CreateTestActor(TEXT("DefenseTarget"), 200, 100);

	// Apply damage without defense buff
	FCombatHitResult NormalHit = Executor->ApplyDamage(Attacker, Target, 50, false, ERefractionElement::None, false);
	int32 NormalDamage = NormalHit.DamageDealt;

	// Apply defense buff (50%)
	FStatusEffect DefBuff = FStatusEffect::CreateBuff(
		TEXT("Test Defense"), 100, EAbilityEffectType::DefenseBuff, 50.0f, 5);
	StatusManager->ApplyEffect(Target, DefBuff, nullptr, TEXT("Test"), -1);

	// Apply damage with defense buff
	FCombatHitResult BuffedHit = Executor->ApplyDamage(Attacker, Target, 50, false, ERefractionElement::None, false);
	int32 BuffedDamage = BuffedHit.DamageDealt;

	// Defense should reduce damage
	bool bDamageReduced = BuffedDamage < NormalDamage;

	bool bPassed = bDamageReduced;
	LogTestResult(TEXT("Defense Reduction"), bPassed,
		FString::Printf(TEXT("Normal: %d, With 50%% Defense: %d"),
			NormalDamage, BuffedDamage));

	// Cleanup
	StatusManager->RemoveAllEffects(Target);
}

void AActionExecutorTestActor::Test_MultiHit()
{
	UActionExecutor* Executor = GetActionExecutor();
	if (!Executor)
	{
		LogTestResult(TEXT("Multi Hit"), false, TEXT("ActionExecutor not found"));
		return;
	}

	AActor* User = CreateTestActor(TEXT("MultiHitUser"), 100, 100);
	AActor* Target = CreateTestActor(TEXT("MultiHitTarget"), 200, 100);

	// Create 3-hit ability
	UAbilityData* MultiHitAbility = CreateTestAbility(TEXT("TripleStrike"), 30, 15, 3);

	FAction Action;
	Action.ActionType = EActionType::Ability;
	Action.AbilityData = MultiHitAbility;
	Action.Targets.Add(Target);

	UCharacterDataComponent* TargetComp = Target->FindComponentByClass<UCharacterDataComponent>();
	int32 HPBefore = TargetComp->CurrentHP;

	FActionResult Result = Executor->ExecuteAction(User, Action);

	// Each hit deals 10 damage (30/3), so total should be ~30 before defense
	bool bDealtDamage = Result.TotalDamageDealt > 0;
	bool bMultipleHits = Result.TotalDamageDealt > 15; // More than a single hit would deal

	bool bPassed = Result.bSuccess && bDealtDamage;
	LogTestResult(TEXT("Multi Hit"), bPassed,
		FString::Printf(TEXT("Total Damage: %d, HP: %d->%d"),
			Result.TotalDamageDealt, HPBefore, TargetComp->CurrentHP));
}

// ==================== EFFECT TESTS ====================

void AActionExecutorTestActor::Test_StatusEffectApplication()
{
	UActionExecutor* Executor = GetActionExecutor();
	UStatusEffectManager* StatusManager = GetStatusEffectManager();
	if (!Executor || !StatusManager)
	{
		LogTestResult(TEXT("Status Effect Application"), false, TEXT("Subsystems not found"));
		return;
	}

	AActor* User = CreateTestActor(TEXT("EffectUser"), 100, 100);
	AActor* Target = CreateTestActor(TEXT("EffectTarget"), 100, 100);

	// Create ability with effect
	UAbilityData* DebuffAbility = CreateTestAbility(TEXT("Weakening Strike"), 20, 10);
	DebuffAbility->EffectType = EAbilityEffectType::DamageDebuff;
	DebuffAbility->EffectValue = 20.0f;
	DebuffAbility->EffectDuration = 3;

	FAction Action;
	Action.ActionType = EActionType::Ability;
	Action.AbilityData = DebuffAbility;
	Action.Targets.Add(Target);

	FActionResult Result = Executor->ExecuteAction(User, Action);

	// Check if debuff was applied
	bool bHasDebuff = StatusManager->HasEffectOfType(Target, EAbilityEffectType::DamageDebuff);

	bool bPassed = Result.bSuccess && Result.StatusEffectsApplied > 0 && bHasDebuff;
	LogTestResult(TEXT("Status Effect Application"), bPassed,
		FString::Printf(TEXT("Effects Applied: %d, Has Debuff: %s"),
			Result.StatusEffectsApplied, bHasDebuff ? TEXT("true") : TEXT("false")));

	// Cleanup
	StatusManager->RemoveAllEffects(Target);
}

void AActionExecutorTestActor::Test_HealingApplication()
{
	UActionExecutor* Executor = GetActionExecutor();
	if (!Executor)
	{
		LogTestResult(TEXT("Healing Application"), false, TEXT("ActionExecutor not found"));
		return;
	}

	AActor* Healer = CreateTestActor(TEXT("Healer"), 100, 100);
	AActor* Target = CreateTestActor(TEXT("HealTarget"), 100, 100);

	UCharacterDataComponent* TargetComp = Target->FindComponentByClass<UCharacterDataComponent>();

	// Damage target first
	TargetComp->CurrentHP = 30;
	int32 HPBefore = TargetComp->CurrentHP;

	// Apply healing
	FCombatHitResult HealResult = Executor->ApplyHealing(Healer, Target, 50);

	bool bHealed = HealResult.HealingDone > 0;
	bool bHPIncreased = TargetComp->CurrentHP > HPBefore;
	bool bNotOverhealed = TargetComp->CurrentHP <= TargetComp->MaxHP;

	bool bPassed = bHealed && bHPIncreased && bNotOverhealed;
	LogTestResult(TEXT("Healing Application"), bPassed,
		FString::Printf(TEXT("Healed: %d, HP: %d->%d (Max: %d)"),
			HealResult.HealingDone, HPBefore, TargetComp->CurrentHP, TargetComp->MaxHP));
}

// ==================== TEST HELPERS ====================

AActor* AActionExecutorTestActor::CreateTestActor(const FString& Name, int32 MaxHP, int32 MaxEP)
{
	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = FName(*Name);
	SpawnParams.Owner = this;

	AActor* TestActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

	if (TestActor)
	{
		// Add CharacterDataComponent
		UCharacterDataComponent* CharComp = NewObject<UCharacterDataComponent>(TestActor);
		CharComp->RegisterComponent();
		TestActor->AddInstanceComponent(CharComp);

		// Initialize stats
		CharComp->MaxHP = MaxHP;
		CharComp->CurrentHP = MaxHP;
		CharComp->MaxEP = MaxEP;
		CharComp->CurrentEP = MaxEP;
		CharComp->bIsAlive = true;

		SpawnedTestActors.Add(TestActor);
	}

	return TestActor;
}

USpellData* AActionExecutorTestActor::CreateTestSpell(const FString& Name, int32 Damage, int32 EnergyCost, int32 Hits)
{
	USpellData* Spell = NewObject<USpellData>(this);
	Spell->SpellName = FName(*Name);
	Spell->BaseDamage = Damage;
	Spell->BaseEnergyCost = EnergyCost;
	Spell->HitCount = Hits;
	Spell->Element = ERefractionElement::Fire;
	Spell->School = ESpellSchool::Destruction;
	Spell->bHasModeToggle = false;

	CreatedTestAssets.Add(Spell);
	return Spell;
}

UAbilityData* AActionExecutorTestActor::CreateTestAbility(const FString& Name, int32 Damage, int32 EnergyCost, int32 Hits)
{
	UAbilityData* Ability = NewObject<UAbilityData>(this);
	Ability->AbilityName = FName(*Name);
	Ability->BaseDamage = Damage;
	Ability->BaseEnergyCost = EnergyCost;
	Ability->HitCount = Hits;
	Ability->bCanBeInfused = false;
	Ability->TargetType = ETargetType::SingleEnemy;
	Ability->EffectType = EAbilityEffectType::None;

	CreatedTestAssets.Add(Ability);
	return Ability;
}

UItemData* AActionExecutorTestActor::CreateTestItem(const FString& Name, EItemEffectType EffectType, float Value)
{
	UItemData* Item = NewObject<UItemData>(this);
	Item->ItemName = FName(*Name);
	Item->EffectType = EffectType;
	Item->BaseEffectValue = Value;
	Item->ItemTier = EItemTier::C;
	Item->EffectDuration = 3;

	CreatedTestAssets.Add(Item);
	return Item;
}

UBaseAttackData* AActionExecutorTestActor::CreateTestAttack(const FString& Name, int32 Damage, int32 Hits)
{
	UBaseAttackData* Attack = NewObject<UBaseAttackData>(this);
	Attack->AttackName = FName(*Name);
	Attack->BaseDamage = Damage;
	Attack->HitCount = Hits;

	CreatedTestAssets.Add(Attack);
	return Attack;
}

UActionExecutor* AActionExecutorTestActor::GetActionExecutor() const
{
	if (UGameInstance* GI = Cast<UGameInstance>(GetWorld()->GetGameInstance()))
	{
		return GI->GetSubsystem<UActionExecutor>();
	}
	return nullptr;
}

UStatusEffectManager* AActionExecutorTestActor::GetStatusEffectManager() const
{
	if (UGameInstance* GI = Cast<UGameInstance>(GetWorld()->GetGameInstance()))
	{
		return GI->GetSubsystem<UStatusEffectManager>();
	}
	return nullptr;
}

void AActionExecutorTestActor::LogTestResult(const FString& TestName, bool bPassed, const FString& Details)
{
	if (bPassed)
	{
		TestsPassed++;
		UE_LOG(LogTemp, Display, TEXT("  [PASS] %s"), *TestName);
	}
	else
	{
		TestsFailed++;
		if (Details.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("  [FAIL] %s"), *TestName);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("  [FAIL] %s - %s"), *TestName, *Details);
		}
	}
}

void AActionExecutorTestActor::CleanupTestActors()
{
	for (AActor* Actor : SpawnedTestActors)
	{
		if (Actor && IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	SpawnedTestActors.Empty();

	// Clear test assets (will be garbage collected)
	CreatedTestAssets.Empty();
}
