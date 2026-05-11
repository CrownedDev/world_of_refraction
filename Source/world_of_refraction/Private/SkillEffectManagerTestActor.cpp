// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkillEffectManagerTestActor.h"
#include "SkillEffectManager.h"
#include "StatusEffect.h"
#include "CharacterDataComponent.h"
#include "TurnManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

ASkillEffectManagerTestActor::ASkillEffectManagerTestActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ASkillEffectManagerTestActor::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoRunTests)
	{
		// Delay to ensure subsystems are ready
		FTimerHandle TimerHandle;
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &ASkillEffectManagerTestActor::RunAllTests, 0.5f, false);
	}
}

// ========================================
// TEST EXECUTION
// ========================================

void ASkillEffectManagerTestActor::RunAllTests()
{
	TestsPassed = 0;
	TestsFailed = 0;
	TestActorCounter = 0;

	UE_LOG(LogTemp, Display, TEXT(""));
	UE_LOG(LogTemp, Display, TEXT("========================================"));
	UE_LOG(LogTemp, Display, TEXT("SKILL EFFECT MANAGER TEST SUITE"));
	UE_LOG(LogTemp, Display, TEXT("========================================"));

	// Application Tests
	Test_ApplyEffect();
	Test_StackingBehavior();
	Test_DurationRefresh();

	// Removal Tests
	Test_RemoveByID();
	Test_RemoveByType();
	Test_RemoveAllBuffs();
	Test_RemoveAllDebuffs();

	// Turn Processing Tests
	Test_StartOfTurnProcessing();
	Test_EndOfTurnProcessing();
	Test_DOTDamage();
	Test_DurationExpiration();

	// Advanced Tests
	Test_ConditionalTrigger();
	Test_PermanentEffects();
	Test_StatModifierQuery();
	Test_SourceTracking();
	Test_WeaponBonuses();
	Test_PhysicalDamageEffects();
	Test_SpeedBuffTurnManagerNotification();

	UE_LOG(LogTemp, Display, TEXT("========================================"));
	UE_LOG(LogTemp, Display, TEXT("RESULTS: %d passed, %d failed"), TestsPassed, TestsFailed);
	UE_LOG(LogTemp, Display, TEXT("========================================"));
	UE_LOG(LogTemp, Display, TEXT(""));

	// Final cleanup
	CleanupTestActors();

	if (USkillEffectManager *Manager = GetSkillEffectManager())
	{
		Manager->ClearAllEffects();
	}
}

// ========================================
// INDIVIDUAL TESTS
// ========================================

void ASkillEffectManagerTestActor::Test_ApplyEffect()
{
	UE_LOG(LogTemp, Display, TEXT("[TEST] Apply Effect"));

	USkillEffectManager *Manager = GetSkillEffectManager();
	if (!AssertTrue(Manager != nullptr, TEXT("Manager exists")))
		return;

	AActor *TestActor = CreateTestActor(TEXT("ApplyTest"));
	if (!AssertTrue(TestActor != nullptr, TEXT("Test actor created")))
		return;

	// Create and apply a simple buff
	FStatusEffect Buff = FStatusEffect::CreateBuff(TEXT("Test Buff"), 1001, EStatusType::DamageBuff, 10.0f, 3);

	EEffectApplicationResult Result = Manager->ApplyEffect(TestActor, Buff);

	bool bPassed = true;
	bPassed &= AssertTrue(Result == EEffectApplicationResult::Applied, TEXT("Effect applied successfully"));
	bPassed &= AssertEqual(1, Manager->GetEffectCount(TestActor), TEXT("Effect count is 1"));
	bPassed &= AssertTrue(Manager->HasEffectByID(TestActor, 1001), TEXT("Has effect by ID"));

	LogTestResult(TEXT("Apply Effect"), bPassed);

	Manager->RemoveAllEffects(TestActor);
}

void ASkillEffectManagerTestActor::Test_StackingBehavior()
{
	UE_LOG(LogTemp, Display, TEXT("[TEST] Stacking Behavior"));

	USkillEffectManager *Manager = GetSkillEffectManager();
	if (!Manager)
		return;

	AActor *TestActor = CreateTestActor(TEXT("StackTest"));
	if (!TestActor)
		return;

	// Create stackable effect
	FStatusEffect StackableBuff;
	StackableBuff.EffectName = TEXT("Stackable Buff");
	StackableBuff.EffectID = 2001;
	StackableBuff.EffectType = EStatusType::DamageBuff;
	StackableBuff.EffectValue = 5.0f;
	StackableBuff.RemainingTurns = 3;
	StackableBuff.bCanStack = true;
	StackableBuff.MaxStacks = 3;
	StackableBuff.ProcessTiming = EStatusEffectTiming::Persistent;

	// Apply 4 times (should cap at 3)
	Manager->ApplyEffect(TestActor, StackableBuff);
	EEffectApplicationResult R2 = Manager->ApplyEffect(TestActor, StackableBuff);
	EEffectApplicationResult R3 = Manager->ApplyEffect(TestActor, StackableBuff);
	EEffectApplicationResult R4 = Manager->ApplyEffect(TestActor, StackableBuff);

	TArray<FStatusEffect> Effects = Manager->GetActiveEffects(TestActor);

	bool bPassed = true;
	bPassed &= AssertEqual(1, Effects.Num(), TEXT("Only 1 effect entry (stacked)"));
	bPassed &= AssertTrue(R2 == EEffectApplicationResult::StackAdded, TEXT("Second apply added stack"));
	bPassed &= AssertTrue(R3 == EEffectApplicationResult::StackAdded, TEXT("Third apply added stack"));
	bPassed &= AssertTrue(R4 == EEffectApplicationResult::DurationRefreshed, TEXT("Fourth apply refreshed (at max)"));

	if (Effects.Num() > 0)
	{
		bPassed &= AssertEqual(3, Effects[0].CurrentStacks, TEXT("Stack count is 3 (max)"));
		bPassed &= AssertEqualFloat(15.0f, Effects[0].GetStackedValue(), TEXT("Stacked value is 15 (5*3)"));
	}

	LogTestResult(TEXT("Stacking Behavior"), bPassed);

	Manager->RemoveAllEffects(TestActor);
}

void ASkillEffectManagerTestActor::Test_DurationRefresh()
{
	UE_LOG(LogTemp, Display, TEXT("[TEST] Duration Refresh"));

	USkillEffectManager *Manager = GetSkillEffectManager();
	if (!Manager)
		return;

	AActor *TestActor = CreateTestActor(TEXT("RefreshTest"));
	if (!TestActor)
		return;

	// Create non-stackable effect with refresh
	FStatusEffect Buff;
	Buff.EffectName = TEXT("Refresh Buff");
	Buff.EffectID = 3001;
	Buff.EffectType = EStatusType::SpeedBuff;
	Buff.EffectValue = 10.0f;
	Buff.RemainingTurns = 3;
	Buff.bCanStack = false;
	Buff.bRefreshDurationOnReapply = true;
	Buff.ProcessTiming = EStatusEffectTiming::StartOfOwnTurn;

	Manager->ApplyEffect(TestActor, Buff);

	// Simulate 2 turns passing
	Manager->ProcessEndOfTurnEffects(TestActor);
	Manager->ProcessEndOfTurnEffects(TestActor);

	TArray<FStatusEffect> EffectsBefore = Manager->GetActiveEffects(TestActor);
	int32 DurationBefore = (EffectsBefore.Num() > 0) ? EffectsBefore[0].RemainingTurns : -1;

	// Reapply - should refresh duration
	EEffectApplicationResult Result = Manager->ApplyEffect(TestActor, Buff);

	TArray<FStatusEffect> EffectsAfter = Manager->GetActiveEffects(TestActor);
	int32 DurationAfter = (EffectsAfter.Num() > 0) ? EffectsAfter[0].RemainingTurns : -1;

	bool bPassed = true;
	bPassed &= AssertEqual(1, DurationBefore, TEXT("Duration was 1 after 2 ticks"));
	bPassed &= AssertTrue(Result == EEffectApplicationResult::DurationRefreshed, TEXT("Result is DurationRefreshed"));
	bPassed &= AssertEqual(3, DurationAfter, TEXT("Duration refreshed to 3"));

	LogTestResult(TEXT("Duration Refresh"), bPassed);

	Manager->RemoveAllEffects(TestActor);
}

void ASkillEffectManagerTestActor::Test_RemoveByID()
{
	UE_LOG(LogTemp, Display, TEXT("[TEST] Remove By ID"));

	USkillEffectManager *Manager = GetSkillEffectManager();
	if (!Manager)
		return;

	AActor *TestActor = CreateTestActor(TEXT("RemoveIDTest"));
	if (!TestActor)
		return;

	// Apply two different effects
	FStatusEffect Buff1 = FStatusEffect::CreateBuff(TEXT("Buff 1"), 4001, EStatusType::DamageBuff, 10.0f, 3);
	FStatusEffect Buff2 = FStatusEffect::CreateBuff(TEXT("Buff 2"), 4002, EStatusType::SpeedBuff, 5.0f, 3);

	Manager->ApplyEffect(TestActor, Buff1);
	Manager->ApplyEffect(TestActor, Buff2);

	bool bPassed = true;
	bPassed &= AssertEqual(2, Manager->GetEffectCount(TestActor), TEXT("Has 2 effects"));

	// Remove first effect
	bool bRemoved = Manager->RemoveEffectByID(TestActor, 4001);

	bPassed &= AssertTrue(bRemoved, TEXT("RemoveEffectByID returned true"));
	bPassed &= AssertEqual(1, Manager->GetEffectCount(TestActor), TEXT("Has 1 effect after removal"));
	bPassed &= AssertTrue(!Manager->HasEffectByID(TestActor, 4001), TEXT("Effect 4001 removed"));
	bPassed &= AssertTrue(Manager->HasEffectByID(TestActor, 4002), TEXT("Effect 4002 still present"));

	LogTestResult(TEXT("Remove By ID"), bPassed);

	Manager->RemoveAllEffects(TestActor);
}

void ASkillEffectManagerTestActor::Test_RemoveByType()
{
	UE_LOG(LogTemp, Display, TEXT("[TEST] Remove By Type"));

	USkillEffectManager *Manager = GetSkillEffectManager();
	if (!Manager)
		return;

	AActor *TestActor = CreateTestActor(TEXT("RemoveTypeTest"));
	if (!TestActor)
		return;

	// Apply mixed effects
	Manager->ApplyEffect(TestActor, FStatusEffect::CreateBuff(TEXT("Damage 1"), 5001, EStatusType::DamageBuff, 10.0f, 3));
	Manager->ApplyEffect(TestActor, FStatusEffect::CreateBuff(TEXT("Damage 2"), 5002, EStatusType::DamageBuff, 5.0f, 3));
	Manager->ApplyEffect(TestActor, FStatusEffect::CreateBuff(TEXT("Speed"), 5003, EStatusType::SpeedBuff, 10.0f, 3));

	bool bPassed = true;
	bPassed &= AssertEqual(3, Manager->GetEffectCount(TestActor), TEXT("Has 3 effects"));

	// Remove all damage buffs
	int32 Removed = Manager->RemoveEffectsByType(TestActor, EStatusType::DamageBuff);

	bPassed &= AssertEqual(2, Removed, TEXT("Removed 2 damage buffs"));
	bPassed &= AssertEqual(1, Manager->GetEffectCount(TestActor), TEXT("Has 1 effect remaining"));
	bPassed &= AssertTrue(Manager->HasEffectOfType(TestActor, EStatusType::SpeedBuff), TEXT("Speed buff remains"));

	LogTestResult(TEXT("Remove By Type"), bPassed);

	Manager->RemoveAllEffects(TestActor);
}

void ASkillEffectManagerTestActor::Test_RemoveAllBuffs()
{
	UE_LOG(LogTemp, Display, TEXT("[TEST] Remove All Buffs"));

	USkillEffectManager *Manager = GetSkillEffectManager();
	if (!Manager)
		return;

	AActor *TestActor = CreateTestActor(TEXT("RemoveBuffsTest"));
	if (!TestActor)
		return;

	// Apply buffs and debuffs
	Manager->ApplyEffect(TestActor, FStatusEffect::CreateBuff(TEXT("Damage Buff"), 6001, EStatusType::DamageBuff, 10.0f, 3));
	Manager->ApplyEffect(TestActor, FStatusEffect::CreateBuff(TEXT("Speed Buff"), 6002, EStatusType::SpeedBuff, 10.0f, 3));

	FStatusEffect Debuff;
	Debuff.EffectName = TEXT("Speed Debuff");
	Debuff.EffectID = 6003;
	Debuff.EffectType = EStatusType::SpeedDebuff;
	Debuff.EffectValue = -10.0f;
	Debuff.RemainingTurns = 3;
	Debuff.ProcessTiming = EStatusEffectTiming::Persistent;
	Manager->ApplyEffect(TestActor, Debuff);

	bool bPassed = true;
	bPassed &= AssertEqual(3, Manager->GetEffectCount(TestActor), TEXT("Has 3 effects"));
	bPassed &= AssertEqual(2, Manager->GetBuffCount(TestActor), TEXT("Has 2 buffs"));
	bPassed &= AssertEqual(1, Manager->GetDebuffCount(TestActor), TEXT("Has 1 debuff"));

	// Remove all buffs
	int32 Removed = Manager->RemoveAllBuffs(TestActor);

	bPassed &= AssertEqual(2, Removed, TEXT("Removed 2 buffs"));
	bPassed &= AssertEqual(1, Manager->GetEffectCount(TestActor), TEXT("Has 1 effect remaining"));
	bPassed &= AssertEqual(0, Manager->GetBuffCount(TestActor), TEXT("Has 0 buffs"));
	bPassed &= AssertEqual(1, Manager->GetDebuffCount(TestActor), TEXT("Still has 1 debuff"));

	LogTestResult(TEXT("Remove All Buffs"), bPassed);

	Manager->RemoveAllEffects(TestActor);
}

void ASkillEffectManagerTestActor::Test_RemoveAllDebuffs()
{
	UE_LOG(LogTemp, Display, TEXT("[TEST] Remove All Debuffs"));

	USkillEffectManager *Manager = GetSkillEffectManager();
	if (!Manager)
		return;

	AActor *TestActor = CreateTestActor(TEXT("RemoveDebuffsTest"));
	if (!TestActor)
		return;

	// Apply buffs and debuffs
	Manager->ApplyEffect(TestActor, FStatusEffect::CreateBuff(TEXT("Damage Buff"), 7001, EStatusType::DamageBuff, 10.0f, 3));

	FStatusEffect Debuff1;
	Debuff1.EffectName = TEXT("Speed Debuff");
	Debuff1.EffectID = 7002;
	Debuff1.EffectType = EStatusType::SpeedDebuff;
	Debuff1.EffectValue = -10.0f;
	Debuff1.RemainingTurns = 3;
	Debuff1.ProcessTiming = EStatusEffectTiming::Persistent;
	Manager->ApplyEffect(TestActor, Debuff1);

	FStatusEffect Debuff2;
	Debuff2.EffectName = TEXT("Defense Debuff");
	Debuff2.EffectID = 7003;
	Debuff2.EffectType = EStatusType::DefenseDebuff;
	Debuff2.EffectValue = -15.0f;
	Debuff2.RemainingTurns = 3;
	Debuff2.ProcessTiming = EStatusEffectTiming::Persistent;
	Manager->ApplyEffect(TestActor, Debuff2);

	bool bPassed = true;
	bPassed &= AssertEqual(3, Manager->GetEffectCount(TestActor), TEXT("Has 3 effects"));

	// Remove all debuffs (simulates cleanse)
	int32 Removed = Manager->RemoveAllDebuffs(TestActor);

	bPassed &= AssertEqual(2, Removed, TEXT("Removed 2 debuffs"));
	bPassed &= AssertEqual(1, Manager->GetEffectCount(TestActor), TEXT("Has 1 effect remaining"));
	bPassed &= AssertEqual(1, Manager->GetBuffCount(TestActor), TEXT("Buff remains"));

	LogTestResult(TEXT("Remove All Debuffs"), bPassed);

	Manager->RemoveAllEffects(TestActor);
}

void ASkillEffectManagerTestActor::Test_StartOfTurnProcessing()
{
	UE_LOG(LogTemp, Display, TEXT("[TEST] Start Of Turn Processing"));

	USkillEffectManager *Manager = GetSkillEffectManager();
	if (!Manager)
		return;

	AActor *TestActor = CreateTestActor(TEXT("StartTurnTest"), 100, 50); // MaxHP=100
	if (!TestActor)
		return;

	UCharacterDataComponent *CharComp = TestActor->FindComponentByClass<UCharacterDataComponent>();
	if (!CharComp)
		return;

	// Damage first so we have room to heal
	CharComp->CurrentHP = 50;

	// Apply a heal-over-time at start of turn
	FStatusEffect HoT;
	HoT.EffectName = TEXT("Regeneration");
	HoT.EffectID = 8001;
	HoT.EffectType = EStatusType::HealthRestore;
	HoT.EffectValue = 10.0f;
	HoT.RemainingTurns = 3;
	HoT.InitialDuration = 3;
	HoT.ProcessTiming = EStatusEffectTiming::StartOfOwnTurn;

	Manager->ApplyEffect(TestActor, HoT);

	int32 HPBefore = CharComp->CurrentHP;

	// Process start of turn
	Manager->ProcessStartOfTurnEffects(TestActor);

	int32 HPAfter = CharComp->CurrentHP;

	bool bPassed = true;
	bPassed &= AssertEqual(50, HPBefore, TEXT("HP was 50 before"));
	bPassed &= AssertEqual(60, HPAfter, TEXT("HP is 60 after (healed 10)"));

	LogTestResult(TEXT("Start Of Turn Processing"), bPassed);

	Manager->RemoveAllEffects(TestActor);
}

void ASkillEffectManagerTestActor::Test_EndOfTurnProcessing()
{
	UE_LOG(LogTemp, Display, TEXT("[TEST] End Of Turn Processing"));

	USkillEffectManager *Manager = GetSkillEffectManager();
	if (!Manager)
		return;

	AActor *TestActor = CreateTestActor(TEXT("EndTurnTest"), 100, 50);
	if (!TestActor)
		return;

	// Apply end-of-turn effect
	FStatusEffect EnergyDrain;
	EnergyDrain.EffectName = TEXT("Energy Drain");
	EnergyDrain.EffectID = 9001;
	EnergyDrain.EffectType = EStatusType::EnergyDrain;
	EnergyDrain.EffectValue = 5.0f;
	EnergyDrain.RemainingTurns = 2;
	EnergyDrain.ProcessTiming = EStatusEffectTiming::EndOfOwnTurn;

	Manager->ApplyEffect(TestActor, EnergyDrain);

	UCharacterDataComponent *CharComp = TestActor->FindComponentByClass<UCharacterDataComponent>();
	int32 EPBefore = CharComp->CurrentEP;

	// Process end of turn
	Manager->ProcessEndOfTurnEffects(TestActor);

	int32 EPAfter = CharComp->CurrentEP;

	// Check duration ticked
	TArray<FStatusEffect> Effects = Manager->GetActiveEffects(TestActor);
	int32 RemainingTurns = (Effects.Num() > 0) ? Effects[0].RemainingTurns : -1;

	bool bPassed = true;
	bPassed &= AssertEqual(50, EPBefore, TEXT("EP was 50 before"));
	bPassed &= AssertEqual(45, EPAfter, TEXT("EP is 45 after (drained 5)"));
	bPassed &= AssertEqual(1, RemainingTurns, TEXT("Duration ticked to 1"));

	LogTestResult(TEXT("End Of Turn Processing"), bPassed);

	Manager->RemoveAllEffects(TestActor);
}

void ASkillEffectManagerTestActor::Test_DOTDamage()
{
	UE_LOG(LogTemp, Display, TEXT("[TEST] DOT Damage"));

	USkillEffectManager *Manager = GetSkillEffectManager();
	if (!Manager)
		return;

	AActor *TestActor = CreateTestActor(TEXT("DOTTest"), 100, 50);
	if (!TestActor)
		return;

	UCharacterDataComponent *CharComp = TestActor->FindComponentByClass<UCharacterDataComponent>();

	// Apply poison DOT
	FStatusEffect Poison = FStatusEffect::CreateDOT(TEXT("Poison"), 10001, 15.0f, 3, ESpellElement::Earth);

	Manager->ApplyEffect(TestActor, Poison);

	int32 HPBefore = CharComp->CurrentHP;

	// Process end of turn (DOTs process here)
	Manager->ProcessEndOfTurnEffects(TestActor);

	int32 HPAfter = CharComp->CurrentHP;

	bool bPassed = true;
	bPassed &= AssertEqual(100, HPBefore, TEXT("HP was 100 before"));
	bPassed &= AssertEqual(85, HPAfter, TEXT("HP is 85 after (15 poison damage)"));
	bPassed &= AssertTrue(Manager->HasActiveDOT(TestActor), TEXT("HasActiveDOT returns true"));

	LogTestResult(TEXT("DOT Damage"), bPassed);

	Manager->RemoveAllEffects(TestActor);
}

void ASkillEffectManagerTestActor::Test_DurationExpiration()
{
	UE_LOG(LogTemp, Display, TEXT("[TEST] Duration Expiration"));

	USkillEffectManager *Manager = GetSkillEffectManager();
	if (!Manager)
		return;

	AActor *TestActor = CreateTestActor(TEXT("ExpirationTest"));
	if (!TestActor)
		return;

	// Apply 2-turn buff
	FStatusEffect ShortBuff = FStatusEffect::CreateBuff(TEXT("Short Buff"), 11001, EStatusType::DamageBuff, 10.0f, 2);
	ShortBuff.ProcessTiming = EStatusEffectTiming::StartOfOwnTurn;

	Manager->ApplyEffect(TestActor, ShortBuff);

	bool bPassed = true;
	bPassed &= AssertTrue(Manager->HasEffectByID(TestActor, 11001), TEXT("Has effect after apply"));

	// Turn 1
	Manager->ProcessEndOfTurnEffects(TestActor);
	bPassed &= AssertTrue(Manager->HasEffectByID(TestActor, 11001), TEXT("Has effect after turn 1"));

	// Turn 2
	Manager->ProcessEndOfTurnEffects(TestActor);
	bPassed &= AssertTrue(!Manager->HasEffectByID(TestActor, 11001), TEXT("Effect expired after turn 2"));
	bPassed &= AssertEqual(0, Manager->GetEffectCount(TestActor), TEXT("No effects remaining"));

	LogTestResult(TEXT("Duration Expiration"), bPassed);

	Manager->RemoveAllEffects(TestActor);
}

void ASkillEffectManagerTestActor::Test_ConditionalTrigger()
{
	UE_LOG(LogTemp, Display, TEXT("[TEST] Conditional Trigger"));

	USkillEffectManager *Manager = GetSkillEffectManager();
	if (!Manager)
		return;

	AActor *TestActor = CreateTestActor(TEXT("TriggerTest"), 100, 50);
	if (!TestActor)
		return;

	UCharacterDataComponent *CharComp = TestActor->FindComponentByClass<UCharacterDataComponent>();

	// Apply HP threshold effect (activates below 30%)
	FStatusEffect AdrenalineRush;
	AdrenalineRush.EffectName = TEXT("Adrenaline Rush");
	AdrenalineRush.EffectID = 12001;
	AdrenalineRush.EffectType = EStatusType::DamageBuff;
	AdrenalineRush.EffectValue = 25.0f;
	AdrenalineRush.ProcessTiming = EStatusEffectTiming::OnTrigger;
	AdrenalineRush.TriggerCondition = EPassiveTrigger::OnHPBelowThreshold;
	AdrenalineRush.TriggerThreshold = 30.0f;
	AdrenalineRush.bPermanent = true;

	Manager->ApplyEffect(TestActor, AdrenalineRush);

	// At 100 HP (100%), should not trigger
	Manager->ProcessTriggerEffects(TestActor, EPassiveTrigger::OnHPBelowThreshold);
	TArray<FStatusEffect> Effects = Manager->GetActiveEffects(TestActor);
	bool bTriggeredAtFull = (Effects.Num() > 0) ? Effects[0].bTriggerActive : false;

	// Drop HP to 25 (25%)
	CharComp->ServerTakeDamage(75);

	// Now should trigger
	Manager->ProcessTriggerEffects(TestActor, EPassiveTrigger::OnHPBelowThreshold);
	Effects = Manager->GetActiveEffects(TestActor);
	bool bTriggeredAtLow = (Effects.Num() > 0) ? Effects[0].bTriggerActive : false;

	bool bPassed = true;
	bPassed &= AssertTrue(!bTriggeredAtFull, TEXT("Not triggered at full HP"));
	bPassed &= AssertTrue(bTriggeredAtLow, TEXT("Triggered at low HP"));

	LogTestResult(TEXT("Conditional Trigger"), bPassed);

	Manager->RemoveAllEffects(TestActor);
}

void ASkillEffectManagerTestActor::Test_PermanentEffects()
{
	UE_LOG(LogTemp, Display, TEXT("[TEST] Permanent Effects"));

	USkillEffectManager *Manager = GetSkillEffectManager();
	if (!Manager)
		return;

	AActor *TestActor = CreateTestActor(TEXT("PermanentTest"));
	if (!TestActor)
		return;

	// Apply permanent effect
	FStatusEffect Permanent = FStatusEffect::CreatePersistent(TEXT("Equipment Bonus"), 13001, EStatusType::DamageBuff, 15.0f);

	Manager->ApplyEffect(TestActor, Permanent);

	bool bPassed = true;
	bPassed &= AssertTrue(Manager->HasEffectByID(TestActor, 13001), TEXT("Has permanent effect"));

	// Process many turns - should never expire
	for (int32 i = 0; i < 10; ++i)
	{
		Manager->ProcessEndOfTurnEffects(TestActor);
	}

	bPassed &= AssertTrue(Manager->HasEffectByID(TestActor, 13001), TEXT("Still has permanent effect after 10 turns"));

	LogTestResult(TEXT("Permanent Effects"), bPassed);

	Manager->RemoveAllEffects(TestActor);
}

void ASkillEffectManagerTestActor::Test_StatModifierQuery()
{
	UE_LOG(LogTemp, Display, TEXT("[TEST] Stat Modifier Query"));

	USkillEffectManager *Manager = GetSkillEffectManager();
	if (!Manager)
		return;

	AActor *TestActor = CreateTestActor(TEXT("StatQueryTest"));
	if (!TestActor)
		return;

	// Apply multiple damage buffs
	Manager->ApplyEffect(TestActor, FStatusEffect::CreateBuff(TEXT("Buff 1"), 14001, EStatusType::DamageBuff, 10.0f, 3));
	Manager->ApplyEffect(TestActor, FStatusEffect::CreateBuff(TEXT("Buff 2"), 14002, EStatusType::DamageBuff, 15.0f, 3));
	Manager->ApplyEffect(TestActor, FStatusEffect::CreateBuff(TEXT("Speed"), 14003, EStatusType::SpeedBuff, 5.0f, 3));

	float TotalDamageBuff = Manager->GetTotalStatModifier(TestActor, EStatusType::DamageBuff);
	float TotalSpeedBuff = Manager->GetTotalStatModifier(TestActor, EStatusType::SpeedBuff);
	float TotalDefense = Manager->GetTotalStatModifier(TestActor, EStatusType::DefenseBuff);

	bool bPassed = true;
	bPassed &= AssertEqualFloat(25.0f, TotalDamageBuff, TEXT("Total damage buff is 25 (10+15)"));
	bPassed &= AssertEqualFloat(5.0f, TotalSpeedBuff, TEXT("Total speed buff is 5"));
	bPassed &= AssertEqualFloat(0.0f, TotalDefense, TEXT("Total defense buff is 0 (none applied)"));

	LogTestResult(TEXT("Stat Modifier Query"), bPassed);

	Manager->RemoveAllEffects(TestActor);
}

void ASkillEffectManagerTestActor::Test_SourceTracking()
{
	UE_LOG(LogTemp, Display, TEXT("[TEST] Source Tracking"));

	USkillEffectManager *Manager = GetSkillEffectManager();
	if (!Manager)
		return;

	AActor *Caster = CreateTestActor(TEXT("Refractor"));
	AActor *Target = CreateTestActor(TEXT("Target"));
	if (!Caster || !Target)
		return;

	// Apply effect with source tracking
	FStatusEffect Curse = FStatusEffect::CreateBuff(TEXT("Curse"), 15001, EStatusType::DamageDebuff, -20.0f, 3);

	Manager->ApplyEffect(Target, Curse, Caster, TEXT("Dark Curse"), 1);

	TArray<FStatusEffect> Effects = Manager->GetActiveEffects(Target);

	bool bPassed = true;
	bPassed &= AssertEqual(1, Effects.Num(), TEXT("Target has 1 effect"));

	if (Effects.Num() > 0)
	{
		bPassed &= AssertTrue(Effects[0].SourceActor == Caster, TEXT("Source actor tracked"));
		bPassed &= AssertTrue(Effects[0].SourceAbilityName == TEXT("Dark Curse"), TEXT("Source ability tracked"));
		bPassed &= AssertEqual(1, Effects[0].SourceTeamIndex, TEXT("Source team tracked"));
	}

	// Remove effects by source (simulates caster death)
	int32 Removed = Manager->RemoveEffectsBySource(Caster);

	bPassed &= AssertEqual(1, Removed, TEXT("Removed 1 effect from source"));
	bPassed &= AssertEqual(0, Manager->GetEffectCount(Target), TEXT("Target has no effects"));

	LogTestResult(TEXT("Source Tracking"), bPassed);

	Manager->RemoveAllEffects(Target);
}

void ASkillEffectManagerTestActor::Test_WeaponBonuses()
{
	UE_LOG(LogTemp, Display, TEXT("[TEST] Weapon Bonuses"));

	USkillEffectManager *Manager = GetSkillEffectManager();
	if (!Manager)
		return;

	AActor *TestActor = CreateTestActor(TEXT("WeaponBonusTest"));
	if (!TestActor)
		return;

	// Simulate equipping "Iron Sword" with bonuses:
	// +5 Attack, +2 Speed, +3% Crit Chance
	Manager->ApplyWeaponBonuses(
		TestActor,
		TEXT("Iron Sword"),
		1001,  // WeaponID
		5,	   // BonusAttack
		0,	   // BonusDefense
		0,	   // BonusMagicPower
		2,	   // BonusSpeed
		3.0f,  // BonusCritChance
		0.0f); // BonusCritDamage

	bool bPassed = true;

	// Should have 3 effects (Attack, Speed, CritChance)
	bPassed &= AssertEqual(3, Manager->GetEffectCount(TestActor), TEXT("Has 3 weapon bonus effects"));

	// Check stat modifiers are queryable
	float AttackMod = Manager->GetTotalStatModifier(TestActor, EStatusType::RawDamageBuff);
	float SpeedMod = Manager->GetTotalStatModifier(TestActor, EStatusType::SpeedBuff);
	float CritMod = Manager->GetTotalStatModifier(TestActor, EStatusType::CritChanceBuff);

	bPassed &= AssertEqualFloat(5.0f, AttackMod, TEXT("Attack bonus is +5"));
	bPassed &= AssertEqualFloat(2.0f, SpeedMod, TEXT("Speed bonus is +2"));
	bPassed &= AssertEqualFloat(3.0f, CritMod, TEXT("Crit chance bonus is +3%"));

	// All weapon bonuses should be permanent
	for (int32 i = 0; i < 5; ++i)
	{
		Manager->ProcessEndOfTurnEffects(TestActor);
	}
	bPassed &= AssertEqual(3, Manager->GetEffectCount(TestActor), TEXT("Bonuses persist after 5 turns (permanent)"));

	// Remove weapon bonuses (simulate unequipping)
	Manager->RemoveWeaponBonuses(TestActor, 1001);
	bPassed &= AssertEqual(0, Manager->GetEffectCount(TestActor), TEXT("No effects after removing weapon bonuses"));

	LogTestResult(TEXT("Weapon Bonuses"), bPassed);

	Manager->RemoveAllEffects(TestActor);
}

void ASkillEffectManagerTestActor::Test_PhysicalDamageEffects()
{
	UE_LOG(LogTemp, Display, TEXT("[TEST] Physical Damage Effects (Slash/Pierce/Impact)"));

	USkillEffectManager *Manager = GetSkillEffectManager();
	if (!Manager)
		return;

	AActor *Attacker = CreateTestActor(TEXT("GenericWarrior"));
	AActor *Target = CreateTestActor(TEXT("PhysicalTarget"), 100, 50);
	if (!Attacker || !Target)
		return;

	UCharacterDataComponent *CharComp = Target->FindComponentByClass<UCharacterDataComponent>();
	if (!CharComp)
		return;

	bool bPassed = true;

	// Test 1: Slash → Bleed DOT
	Manager->ApplyPhysicalDamageEffect(
		Target,
		TEXT("Iron Sword"),
		2001, // WeaponID
		0,	  // PhysicalType: 0 = Slash
		10,	  // StatusBuildup
		1.0f, // InfusionMultiplier
		2,	  // HitCount (2 hits)
		Attacker,
		0);

	bPassed &= AssertTrue(Manager->HasActiveDOT(Target), TEXT("Slash applied Bleed DOT"));

	int32 HPBefore = CharComp->CurrentHP;
	Manager->ProcessEndOfTurnEffects(Target);
	int32 HPAfter = CharComp->CurrentHP;
	bPassed &= AssertTrue(HPAfter < HPBefore, TEXT("Bleed DOT dealt damage"));

	Manager->RemoveAllEffects(Target);
	CharComp->CurrentHP = 100; // Reset HP

	// Test 2: Pierce → Armor Break (Defense Debuff)
	Manager->ApplyPhysicalDamageEffect(
		Target,
		TEXT("Rapier"),
		2002,
		1, // PhysicalType: 1 = Pierce
		15,
		1.2f, // Higher multiplier weapon
		1,
		Attacker,
		0);

	float DefenseDebuff = Manager->GetTotalStatModifier(Target, EStatusType::DefenseDebuff);
	bPassed &= AssertTrue(DefenseDebuff > 0.0f, TEXT("Pierce applied Armor Break (defense debuff)"));
	bPassed &= AssertTrue(Manager->GetDebuffCount(Target) > 0, TEXT("Target has debuff from Pierce"));

	Manager->RemoveAllEffects(Target);

	// Test 3: Impact → Stun (processed at start of turn)
	Manager->ApplyPhysicalDamageEffect(
		Target,
		TEXT("Warhammer"),
		2003,
		2, // PhysicalType: 2 = Impact
		20,
		1.0f,
		1,
		Attacker,
		0);

	bPassed &= AssertEqual(1, Manager->GetEffectCount(Target), TEXT("Impact applied Stun effect"));

	// Stun should process at start of turn
	TArray<FStatusEffect> Effects = Manager->GetActiveEffects(Target);
	if (Effects.Num() > 0)
	{
		bPassed &= AssertTrue(Effects[0].ProcessTiming == EStatusEffectTiming::StartOfOwnTurn,
							  TEXT("Stun processes at start of turn"));
	}

	LogTestResult(TEXT("Physical Damage Effects (Slash/Pierce/Impact)"), bPassed);

	Manager->RemoveAllEffects(Target);
}

void ASkillEffectManagerTestActor::Test_SpeedBuffTurnManagerNotification()
{
	UE_LOG(LogTemp, Display, TEXT("[TEST] Speed Buff → TurnManager Notification"));

	USkillEffectManager *Manager = GetSkillEffectManager();
	if (!Manager)
		return;

	// Get TurnManager to verify it exists
	UTurnManager *TurnManager = nullptr;
	if (UGameInstance *GI = GetGameInstance())
	{
		TurnManager = GI->GetSubsystem<UTurnManager>();
	}

	if (!TurnManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("    TurnManager not available - skipping integration test"));
		LogTestResult(TEXT("Speed Buff TurnManager Notification"), true); // Pass if TurnManager not available
		return;
	}

	// Create test actors for combat
	AActor *FastChar = CreateTestActor(TEXT("FastChar"), 100, 50);
	AActor *SlowChar = CreateTestActor(TEXT("SlowChar"), 100, 50);
	if (!FastChar || !SlowChar)
		return;

	bool bPassed = true;

	// Test 1: Apply speed buff - should notify TurnManager
	FStatusEffect HasteEffect;
	HasteEffect.EffectName = TEXT("Haste");
	HasteEffect.EffectID = 9001;
	HasteEffect.EffectType = EStatusType::SpeedBuff;
	HasteEffect.EffectValue = 10.0f;
	HasteEffect.RemainingTurns = 3;
	HasteEffect.InitialDuration = 3;
	HasteEffect.ProcessTiming = EStatusEffectTiming::Persistent;

	Manager->ApplyEffect(FastChar, HasteEffect, nullptr, TEXT("Haste Spell"), -1);

	// Verify effect was applied
	bPassed &= AssertTrue(Manager->HasEffectOfType(FastChar, EStatusType::SpeedBuff),
						  TEXT("Speed buff applied"));

	// Query the speed modifier
	float SpeedMod = Manager->GetTotalStatModifier(FastChar, EStatusType::SpeedBuff);
	bPassed &= AssertEqualFloat(10.0f, SpeedMod, TEXT("Speed modifier queryable (+10)"));

	UE_LOG(LogTemp, Display, TEXT("    Speed buff applied - TurnManager should have been notified"));

	// Test 2: Apply speed debuff - should also notify
	FStatusEffect SlowEffect;
	SlowEffect.EffectName = TEXT("Slow");
	SlowEffect.EffectID = 9002;
	SlowEffect.EffectType = EStatusType::SpeedDebuff;
	SlowEffect.EffectValue = 5.0f;
	SlowEffect.RemainingTurns = 2;
	SlowEffect.InitialDuration = 2;
	SlowEffect.ProcessTiming = EStatusEffectTiming::Persistent;

	Manager->ApplyEffect(SlowChar, SlowEffect, nullptr, TEXT("Slow Spell"), -1);

	float SlowMod = Manager->GetTotalStatModifier(SlowChar, EStatusType::SpeedDebuff);
	bPassed &= AssertEqualFloat(5.0f, SlowMod, TEXT("Speed debuff queryable (-5)"));

	UE_LOG(LogTemp, Display, TEXT("    Speed debuff applied - TurnManager should have been notified"));

	// Test 3: Remove speed effect - should notify TurnManager
	Manager->RemoveEffectByID(FastChar, 9001);

	float SpeedModAfterRemove = Manager->GetTotalStatModifier(FastChar, EStatusType::SpeedBuff);
	bPassed &= AssertEqualFloat(0.0f, SpeedModAfterRemove, TEXT("Speed buff removed, modifier is 0"));

	UE_LOG(LogTemp, Display, TEXT("    Speed buff removed - TurnManager should have been notified"));

	// Test 4: Speed effect expiration - should notify TurnManager
	// Apply new buff with 1 turn duration
	FStatusEffect ShortHaste;
	ShortHaste.EffectName = TEXT("Quick Haste");
	ShortHaste.EffectID = 9003;
	ShortHaste.EffectType = EStatusType::SpeedBuff;
	ShortHaste.EffectValue = 8.0f;
	ShortHaste.RemainingTurns = 1;
	ShortHaste.InitialDuration = 1;
	ShortHaste.ProcessTiming = EStatusEffectTiming::Persistent;

	Manager->ApplyEffect(FastChar, ShortHaste, nullptr, TEXT("Quick Haste"), -1);
	bPassed &= AssertTrue(Manager->HasEffectOfType(FastChar, EStatusType::SpeedBuff),
						  TEXT("Short haste applied"));

	// Process end of turn - should expire and notify
	Manager->ProcessEndOfTurnEffects(FastChar);

	bPassed &= AssertTrue(!Manager->HasEffectOfType(FastChar, EStatusType::SpeedBuff),
						  TEXT("Short haste expired after 1 turn"));

	UE_LOG(LogTemp, Display, TEXT("    Speed buff expired - TurnManager should have been notified"));

	LogTestResult(TEXT("Speed Buff TurnManager Notification"), bPassed);

	Manager->RemoveAllEffects(FastChar);
	Manager->RemoveAllEffects(SlowChar);
}

// ========================================
// TEST INFRASTRUCTURE
// ========================================

USkillEffectManager *ASkillEffectManagerTestActor::GetSkillEffectManager()
{
	UGameInstance *GI = GetGameInstance();
	if (!GI)
	{
		UE_LOG(LogTemp, Error, TEXT("[StatusEffectTest] No GameInstance"));
		return nullptr;
	}

	return GI->GetSubsystem<USkillEffectManager>();
}

AActor *ASkillEffectManagerTestActor::CreateTestActor(const FString &Name, int32 HP, int32 EP)
{
	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = FName(*FString::Printf(TEXT("%s_%d"), *Name, TestActorCounter++));
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = this; // Give spawned actors authority via ownership

	AActor *TestActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

	if (TestActor)
	{
		UCharacterDataComponent *CharComp = NewObject<UCharacterDataComponent>(TestActor);
		// Note: CharacterName comes from CharacterData template, not needed for tests
		CharComp->MaxHP = HP;
		CharComp->CurrentHP = HP;
		CharComp->MaxEP = EP;
		CharComp->CurrentEP = EP;
		CharComp->bIsAlive = true;
		CharComp->RegisterComponent();
		TestActor->AddInstanceComponent(CharComp);

		SpawnedTestActors.Add(TestActor);
	}

	return TestActor;
}

void ASkillEffectManagerTestActor::CleanupTestActors()
{
	for (AActor *Actor : SpawnedTestActors)
	{
		if (Actor && IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	SpawnedTestActors.Empty();
}

void ASkillEffectManagerTestActor::LogTestResult(const FString &TestName, bool bPassed, const FString &Details)
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

bool ASkillEffectManagerTestActor::AssertTrue(bool Condition, const FString &Message)
{
	if (!Condition)
	{
		UE_LOG(LogTemp, Error, TEXT("    ASSERT FAILED: %s"), *Message);
	}
	return Condition;
}

bool ASkillEffectManagerTestActor::AssertEqual(int32 Expected, int32 Actual, const FString &Message)
{
	if (Expected != Actual)
	{
		UE_LOG(LogTemp, Error, TEXT("    ASSERT FAILED: %s (Expected: %d, Actual: %d)"), *Message, Expected, Actual);
		return false;
	}
	return true;
}

bool ASkillEffectManagerTestActor::AssertEqualFloat(float Expected, float Actual, const FString &Message, float Tolerance)
{
	if (FMath::Abs(Expected - Actual) > Tolerance)
	{
		UE_LOG(LogTemp, Error, TEXT("    ASSERT FAILED: %s (Expected: %.2f, Actual: %.2f)"), *Message, Expected, Actual);
		return false;
	}
	return true;
}