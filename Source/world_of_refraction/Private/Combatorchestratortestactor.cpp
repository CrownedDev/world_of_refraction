// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatOrchestratorTestActor.h"
#include "TurnManager.h"
#include "CharacterDataComponent.h"
#include "CharacterData.h"
#include "ActionStructs.h"
#include "BaseAttackData.h"
#include "SpellData.h"
#include "AbilityData.h"
#include "StatusEffectManager.h"
#include "Kismet/GameplayStatics.h"

ACombatOrchestratorTestActor::ACombatOrchestratorTestActor()
{
	PrimaryActorTick.bCanEverTick = false;
	TestOrchestrator = nullptr;
	TestsPassed = 0;
	TestsFailed = 0;
	TestActorCounter = 0;
	bWaitingForResult = false;
	bActionExecuted = false;
}

void ACombatOrchestratorTestActor::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoRunTests)
	{
		RunAllTests();
	}
}

// ========================================
// TEST EXECUTION
// ========================================

void ACombatOrchestratorTestActor::RunAllTests()
{
	UE_LOG(LogTemp, Display, TEXT("========================================"));
	UE_LOG(LogTemp, Display, TEXT("COMBAT ORCHESTRATOR TEST SUITE"));
	UE_LOG(LogTemp, Display, TEXT("========================================"));

	TestsPassed = 0;
	TestsFailed = 0;

	Test_BasicCombatFlow();
	Test_StateTransitions();
	Test_VictoryCondition();
	Test_DefeatCondition();
	Test_ForceEndCombat();
	Test_ActionExecutorIntegration();
	Test_RealAttackExecution();
	Test_SpellExecution();
	Test_AbilityExecution();
	Test_MultiTargetAction();
	Test_EnergyCostDeduction();

	UE_LOG(LogTemp, Display, TEXT("========================================"));
	UE_LOG(LogTemp, Display, TEXT("RESULTS: %d passed, %d failed"), TestsPassed, TestsFailed);
	UE_LOG(LogTemp, Display, TEXT("========================================"));
}

void ACombatOrchestratorTestActor::Test_BasicCombatFlow()
{
	UE_LOG(LogTemp, Display, TEXT("\n[TEST] Basic Combat Flow"));

	ACombatOrchestrator* Orchestrator = GetOrCreateOrchestrator();
	if (!Orchestrator)
	{
		PrintTestResult("Basic Combat Flow", false);
		return;
	}

	// Configure for fast testing
	Orchestrator->bAutoAdvanceTurns = true;
	Orchestrator->AutoAdvanceDelay = 0.1f;

	// Create teams
	TArray<AActor*> Team0;
	Team0.Add(CreateTestCharacter("Player1", 5, 5, 5, 0, 0));
	Team0.Add(CreateTestCharacter("Player2", 5, 5, 5, 0, 0));

	TArray<AActor*> Team1;
	Team1.Add(CreateTestCharacter("Enemy1", 5, 5, 5, 0, 1));
	Team1.Add(CreateTestCharacter("Enemy2", 5, 5, 5, 0, 1));

	// Start combat
	Orchestrator->StartCombat(Team0, Team1);

	// Verify state
	bool bCorrectState = (Orchestrator->GetCombatState() == ECombatState::InProgress);
	bool bHasCurrentActor = (Orchestrator->GetCurrentActor() != nullptr);
	bool bCorrectTeamCounts = (Team0.Num() == 2 && Team1.Num() == 2);

	UE_LOG(LogTemp, Display, TEXT("    Combat started successfully"));
	UE_LOG(LogTemp, Display, TEXT("    Current actor: %s"),
		Orchestrator->GetCurrentActor() ? *Orchestrator->GetCurrentActor()->GetName() : TEXT("None"));
	UE_LOG(LogTemp, Display, TEXT("    Teams: %d vs %d"), Team0.Num(), Team1.Num());

	bool bPassed = bCorrectState && bHasCurrentActor && bCorrectTeamCounts;
	PrintTestResult("Basic Combat Flow", bPassed);

	// Cleanup
	Orchestrator->ForceEndCombat();
	CleanupTestActors(Team0);
	CleanupTestActors(Team1);
}

void ACombatOrchestratorTestActor::Test_StateTransitions()
{
	UE_LOG(LogTemp, Display, TEXT("\n[TEST] State Transitions"));

	ACombatOrchestrator* Orchestrator = GetOrCreateOrchestrator();
	if (!Orchestrator)
	{
		PrintTestResult("State Transitions", false);
		return;
	}

	// Track state changes
	RecordedStateTransitions.Empty();
	Orchestrator->OnCombatStateChanged.AddDynamic(this, &ACombatOrchestratorTestActor::OnTestCombatStateChanged);

	// Create minimal teams
	TArray<AActor*> Team0;
	Team0.Add(CreateTestCharacter("P1", 5, 5, 5, 0, 0));

	TArray<AActor*> Team1;
	Team1.Add(CreateTestCharacter("E1", 5, 5, 5, 0, 1));

	// Start and immediately end combat
	Orchestrator->StartCombat(Team0, Team1);
	Orchestrator->ForceEndCombat(ECombatState::Victory);

	// Log transitions
	UE_LOG(LogTemp, Display, TEXT("    Recorded %d state transitions"), RecordedStateTransitions.Num());
	for (int32 i = 0; i < RecordedStateTransitions.Num(); i++)
	{
		UE_LOG(LogTemp, Display, TEXT("    [%d] -> %d"), i, (int32)RecordedStateTransitions[i]);
	}

	// Should have: Initializing(1) -> InProgress(2) -> Victory(3) -> Idle(0)
	bool bPassed = RecordedStateTransitions.Num() >= 3;
	PrintTestResult("State Transitions", bPassed);

	// Cleanup
	Orchestrator->OnCombatStateChanged.RemoveDynamic(this, &ACombatOrchestratorTestActor::OnTestCombatStateChanged);
	CleanupTestActors(Team0);
	CleanupTestActors(Team1);
}

void ACombatOrchestratorTestActor::Test_VictoryCondition()
{
	UE_LOG(LogTemp, Display, TEXT("\n[TEST] Victory Condition"));

	ACombatOrchestrator* Orchestrator = GetOrCreateOrchestrator();
	if (!Orchestrator)
	{
		PrintTestResult("Victory Condition", false);
		return;
	}

	Orchestrator->bAutoAdvanceTurns = false;

	// Create teams
	TArray<AActor*> Team0;
	AActor* Player = CreateTestCharacter("Player", 5, 5, 5, 0, 0);
	Team0.Add(Player);

	TArray<AActor*> Team1;
	AActor* Enemy = CreateTestCharacter("Enemy", 5, 5, 5, 0, 1);
	Team1.Add(Enemy);

	// Track result
	LastCombatResult = FCombatResult();
	Orchestrator->OnCombatResultReady.AddDynamic(this, &ACombatOrchestratorTestActor::OnTestCombatResultReady);

	// Start combat
	Orchestrator->StartCombat(Team0, Team1);

	// Kill all enemies using debug tool
	Orchestrator->DebugKillActor(Enemy);

	// Complete current action to trigger win check
	Orchestrator->OnActionCompleted();

	// Check result
	bool bVictory = (LastCombatResult.FinalState == ECombatState::Victory);
	bool bCorrectSurvivors = (LastCombatResult.Team0Survivors == 1 && LastCombatResult.Team1Survivors == 0);

	UE_LOG(LogTemp, Display, TEXT("    Result: State=%d, Team0=%d alive, Team1=%d alive"),
		(int32)LastCombatResult.FinalState,
		LastCombatResult.Team0Survivors,
		LastCombatResult.Team1Survivors);

	bool bPassed = bVictory && bCorrectSurvivors;
	PrintTestResult("Victory Condition", bPassed);

	// Cleanup
	Orchestrator->OnCombatResultReady.RemoveDynamic(this, &ACombatOrchestratorTestActor::OnTestCombatResultReady);
	CleanupTestActors(Team0);
	CleanupTestActors(Team1);
}

void ACombatOrchestratorTestActor::Test_DefeatCondition()
{
	UE_LOG(LogTemp, Display, TEXT("\n[TEST] Defeat Condition"));

	ACombatOrchestrator* Orchestrator = GetOrCreateOrchestrator();
	if (!Orchestrator)
	{
		PrintTestResult("Defeat Condition", false);
		return;
	}

	Orchestrator->bAutoAdvanceTurns = false;

	// Create teams
	TArray<AActor*> Team0;
	AActor* Player = CreateTestCharacter("Player", 5, 5, 5, 0, 0);
	Team0.Add(Player);

	TArray<AActor*> Team1;
	Team1.Add(CreateTestCharacter("Enemy", 5, 5, 5, 0, 1));

	// Track result
	LastCombatResult = FCombatResult();
	Orchestrator->OnCombatResultReady.AddDynamic(this, &ACombatOrchestratorTestActor::OnTestCombatResultReady);

	// Start combat
	Orchestrator->StartCombat(Team0, Team1);

	// Kill all players
	Orchestrator->DebugKillActor(Player);

	// Complete action to trigger defeat check
	Orchestrator->OnActionCompleted();

	// Check result
	bool bDefeat = (LastCombatResult.FinalState == ECombatState::Defeat);
	bool bCorrectSurvivors = (LastCombatResult.Team0Survivors == 0 && LastCombatResult.Team1Survivors == 1);

	UE_LOG(LogTemp, Display, TEXT("    Result: State=%d, Team0=%d alive, Team1=%d alive"),
		(int32)LastCombatResult.FinalState,
		LastCombatResult.Team0Survivors,
		LastCombatResult.Team1Survivors);

	bool bPassed = bDefeat && bCorrectSurvivors;
	PrintTestResult("Defeat Condition", bPassed);

	// Cleanup
	Orchestrator->OnCombatResultReady.RemoveDynamic(this, &ACombatOrchestratorTestActor::OnTestCombatResultReady);
	CleanupTestActors(Team0);
	CleanupTestActors(Team1);
}

void ACombatOrchestratorTestActor::Test_ForceEndCombat()
{
	UE_LOG(LogTemp, Display, TEXT("\n[TEST] Force End Combat"));

	ACombatOrchestrator* Orchestrator = GetOrCreateOrchestrator();
	if (!Orchestrator)
	{
		PrintTestResult("Force End Combat", false);
		return;
	}

	// Create teams
	TArray<AActor*> Team0;
	Team0.Add(CreateTestCharacter("P1", 5, 5, 5, 0, 0));

	TArray<AActor*> Team1;
	Team1.Add(CreateTestCharacter("E1", 5, 5, 5, 0, 1));

	// Start combat
	Orchestrator->StartCombat(Team0, Team1);

	bool bWasInProgress = (Orchestrator->GetCombatState() == ECombatState::InProgress);

	// Force end
	Orchestrator->ForceEndCombat();

	bool bIsIdle = (Orchestrator->GetCombatState() == ECombatState::Idle);

	UE_LOG(LogTemp, Display, TEXT("    Was in progress: %s"), bWasInProgress ? TEXT("Yes") : TEXT("No"));
	UE_LOG(LogTemp, Display, TEXT("    Is now idle: %s"), bIsIdle ? TEXT("Yes") : TEXT("No"));

	bool bPassed = bWasInProgress && bIsIdle;
	PrintTestResult("Force End Combat", bPassed);

	// Cleanup
	CleanupTestActors(Team0);
	CleanupTestActors(Team1);
}

void ACombatOrchestratorTestActor::Test_ActionExecutorIntegration()
{
	UE_LOG(LogTemp, Display, TEXT("\n[TEST] ActionExecutor Integration"));

	ACombatOrchestrator* Orchestrator = GetOrCreateOrchestrator();
	if (!Orchestrator)
	{
		PrintTestResult("ActionExecutor Integration", false);
		return;
	}

	// Disable auto-advance - we'll submit actions manually
	Orchestrator->bAutoAdvanceTurns = false;

	// Track action execution
	bActionExecuted = false;
	LastActionResult = FActionResult();
	Orchestrator->OnActionExecuted.AddDynamic(this, &ACombatOrchestratorTestActor::OnTestActionExecuted);

	// Create teams
	TArray<AActor*> Team0;
	AActor* Player = CreateTestCharacter("ActionTestPlayer", 5, 5, 5, 0, 0);
	Team0.Add(Player);

	TArray<AActor*> Team1;
	AActor* Enemy = CreateTestCharacter("ActionTestEnemy", 5, 5, 5, 0, 1);
	Team1.Add(Enemy);

	// Start combat
	Orchestrator->StartCombat(Team0, Team1);

	// Verify we're in progress and have a current actor
	bool bInProgress = (Orchestrator->GetCombatState() == ECombatState::InProgress);
	bool bHasActor = (Orchestrator->GetCurrentActor() != nullptr);

	if (!bInProgress || !bHasActor)
	{
		UE_LOG(LogTemp, Error, TEXT("    Failed to start combat for action test"));
		PrintTestResult("ActionExecutor Integration", false);
		Orchestrator->ForceEndCombat();
		Orchestrator->OnActionExecuted.RemoveDynamic(this, &ACombatOrchestratorTestActor::OnTestActionExecuted);
		CleanupTestActors(Team0);
		CleanupTestActors(Team1);
		return;
	}

	// Test 1: Validate action (should pass for Defend)
	FAction DefendAction;
	DefendAction.ActionType = EActionType::Defend;

	FActionValidationResult Validation = Orchestrator->ValidateAction(DefendAction);
	bool bValidationPassed = Validation.bIsValid;

	UE_LOG(LogTemp, Display, TEXT("    Defend action validation: %s"),
		bValidationPassed ? TEXT("VALID") : *Validation.ErrorMessage);

	// Test 2: Submit action
	bool bSubmitSuccess = Orchestrator->SubmitAction(DefendAction);

	UE_LOG(LogTemp, Display, TEXT("    Submit action result: %s"),
		bSubmitSuccess ? TEXT("SUCCESS") : TEXT("FAILED"));

	// Test 3: Check that OnActionExecuted was broadcast
	UE_LOG(LogTemp, Display, TEXT("    OnActionExecuted fired: %s"),
		bActionExecuted ? TEXT("YES") : TEXT("NO"));

	// Test 4: Check action result
	bool bResultValid = LastActionResult.bSuccess && LastActionResult.ActionType == EActionType::Defend;
	UE_LOG(LogTemp, Display, TEXT("    Action result valid: %s (Type=%d, Success=%d)"),
		bResultValid ? TEXT("YES") : TEXT("NO"),
		(int32)LastActionResult.ActionType,
		LastActionResult.bSuccess);

	// Test 5: Check that turn advanced (or combat ended if someone died)
	bool bTurnAdvanced = (Orchestrator->GetCurrentTurnNumber() > 1) ||
		(Orchestrator->GetCombatState() != ECombatState::InProgress);

	UE_LOG(LogTemp, Display, TEXT("    Turn advanced: %s (Turn=%d, State=%d)"),
		bTurnAdvanced ? TEXT("YES") : TEXT("NO"),
		Orchestrator->GetCurrentTurnNumber(),
		(int32)Orchestrator->GetCombatState());

	bool bPassed = bValidationPassed && bSubmitSuccess && bActionExecuted && bResultValid;
	PrintTestResult("ActionExecutor Integration", bPassed);

	// Cleanup
	Orchestrator->ForceEndCombat();
	Orchestrator->OnActionExecuted.RemoveDynamic(this, &ACombatOrchestratorTestActor::OnTestActionExecuted);
	CleanupTestActors(Team0);
	CleanupTestActors(Team1);
}

void ACombatOrchestratorTestActor::Test_RealAttackExecution()
{
	UE_LOG(LogTemp, Display, TEXT("\n[TEST] Real Attack Execution (DA_Test_Attack)"));

	// Load the attack data asset
	UBaseAttackData* TestAttack = LoadObject<UBaseAttackData>(nullptr,
		TEXT("/Game/Testing/Attacks/DA_Test_Attack.DA_Test_Attack"));

	if (!TestAttack)
	{
		UE_LOG(LogTemp, Error, TEXT("    Failed to load DA_Test_Attack - check asset path"));
		PrintTestResult("Real Attack Execution", false);
		return;
	}

	UE_LOG(LogTemp, Display, TEXT("    Loaded: %s (Hits: %d)"), *TestAttack->AttackName, TestAttack->HitCount);

	ACombatOrchestrator* Orchestrator = GetOrCreateOrchestrator();
	if (!Orchestrator)
	{
		PrintTestResult("Real Attack Execution", false);
		return;
	}

	// Disable auto-advance
	Orchestrator->bAutoAdvanceTurns = false;

	// Track action execution
	bActionExecuted = false;
	LastActionResult = FActionResult();
	Orchestrator->OnActionExecuted.AddDynamic(this, &ACombatOrchestratorTestActor::OnTestActionExecuted);

	// Create attacker with good Body stat for damage
	TArray<AActor*> Team0;
	AActor* Attacker = CreateTestCharacter("TestAttacker", 3, 6, 3, 0, 0);
	Team0.Add(Attacker);

	// Create target with moderate defense
	TArray<AActor*> Team1;
	AActor* Target = CreateTestCharacter("TestTarget", 3, 4, 3, 0, 1);
	Team1.Add(Target);

	// Get target's initial HP
	UCharacterDataComponent* TargetComp = Target->FindComponentByClass<UCharacterDataComponent>();
	int32 InitialHP = TargetComp ? TargetComp->CurrentHP : 0;

	UE_LOG(LogTemp, Display, TEXT("    Target initial HP: %d"), InitialHP);

	// Start combat
	Orchestrator->StartCombat(Team0, Team1);

	// Verify combat started
	if (Orchestrator->GetCombatState() != ECombatState::InProgress)
	{
		UE_LOG(LogTemp, Error, TEXT("    Combat failed to start"));
		PrintTestResult("Real Attack Execution", false);
		Orchestrator->ForceEndCombat();
		Orchestrator->OnActionExecuted.RemoveDynamic(this, &ACombatOrchestratorTestActor::OnTestActionExecuted);
		CleanupTestActors(Team0);
		CleanupTestActors(Team1);
		return;
	}

	// Create attack action
	FAction AttackAction;
	AttackAction.ActionType = EActionType::Attack;
	AttackAction.AttackData = TestAttack;
	AttackAction.Targets.Add(Target);

	// Validate action
	FActionValidationResult Validation = Orchestrator->ValidateAction(AttackAction);
	UE_LOG(LogTemp, Display, TEXT("    Attack validation: %s"),
		Validation.bIsValid ? TEXT("VALID") : *Validation.ErrorMessage);

	// Submit attack
	bool bSubmitSuccess = Orchestrator->SubmitAction(AttackAction);
	UE_LOG(LogTemp, Display, TEXT("    Submit result: %s"), bSubmitSuccess ? TEXT("SUCCESS") : TEXT("FAILED"));

	// Check damage was dealt
	int32 FinalHP = TargetComp ? TargetComp->CurrentHP : 0;
	int32 DamageDealt = InitialHP - FinalHP;

	UE_LOG(LogTemp, Display, TEXT("    Target final HP: %d (took %d damage)"), FinalHP, DamageDealt);
	UE_LOG(LogTemp, Display, TEXT("    ActionResult.TotalDamageDealt: %d"), LastActionResult.TotalDamageDealt);
	UE_LOG(LogTemp, Display, TEXT("    ActionResult.bWasCritical: %s"), LastActionResult.bWasCritical ? TEXT("YES") : TEXT("NO"));

	// Verify results
	bool bDamageDealt = (DamageDealt > 0);
	bool bResultMatchesHP = (LastActionResult.TotalDamageDealt == DamageDealt);
	bool bActionFired = bActionExecuted;
	bool bTargetInAffected = LastActionResult.AffectedTargets.Contains(Target);

	UE_LOG(LogTemp, Display, TEXT("    Damage dealt: %s"), bDamageDealt ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogTemp, Display, TEXT("    Result matches HP change: %s"), bResultMatchesHP ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogTemp, Display, TEXT("    Target in affected list: %s"), bTargetInAffected ? TEXT("YES") : TEXT("NO"));

	bool bPassed = bSubmitSuccess && bDamageDealt && bResultMatchesHP && bActionFired && bTargetInAffected;
	PrintTestResult("Real Attack Execution", bPassed);

	// Cleanup
	Orchestrator->ForceEndCombat();
	Orchestrator->OnActionExecuted.RemoveDynamic(this, &ACombatOrchestratorTestActor::OnTestActionExecuted);
	CleanupTestActors(Team0);
	CleanupTestActors(Team1);
}

void ACombatOrchestratorTestActor::Test_SpellExecution()
{
	UE_LOG(LogTemp, Display, TEXT("\n[TEST] Spell Execution (DA_Test_Spell)"));

	// Load the spell data asset
	USpellData* TestSpell = LoadObject<USpellData>(nullptr,
		TEXT("/Game/Testing/Spells/DA_Test_Spell.DA_Test_Spell"));

	if (!TestSpell)
	{
		UE_LOG(LogTemp, Error, TEXT("    Failed to load DA_Test_Spell - check asset path"));
		PrintTestResult("Spell Execution", false);
		return;
	}

	UE_LOG(LogTemp, Display, TEXT("    Loaded: %s (Element: %d)"), *TestSpell->SpellName, (int32)TestSpell->Element);

	ACombatOrchestrator* Orchestrator = GetOrCreateOrchestrator();
	if (!Orchestrator)
	{
		PrintTestResult("Spell Execution", false);
		return;
	}

	Orchestrator->bAutoAdvanceTurns = false;

	// Track action execution
	bActionExecuted = false;
	LastActionResult = FActionResult();
	Orchestrator->OnActionExecuted.AddDynamic(this, &ACombatOrchestratorTestActor::OnTestActionExecuted);

	// Caster on Team1 so they go first
	TArray<AActor*> Team0;
	AActor* Target = CreateTestCharacter("SpellTarget", 3, 3, 3, 0, 0);
	Team0.Add(Target);

	TArray<AActor*> Team1;
	AActor* Caster = CreateTestCharacter("FireCaster", 5, 3, 5, 0, 1);
	Team1.Add(Caster);

	// Get target's initial HP
	UCharacterDataComponent* TargetComp = Target->FindComponentByClass<UCharacterDataComponent>();
	int32 InitialHP = TargetComp ? TargetComp->CurrentHP : 0;

	UE_LOG(LogTemp, Display, TEXT("    Target initial HP: %d"), InitialHP);

	// Start combat
	Orchestrator->StartCombat(Team0, Team1);

	// Create spell action
	FAction SpellAction;
	SpellAction.ActionType = EActionType::Spell;
	SpellAction.SpellData = TestSpell;
	SpellAction.Targets.Add(Target);
	SpellAction.bUseElementalMode = true;

	// Validate
	FActionValidationResult Validation = Orchestrator->ValidateAction(SpellAction);
	UE_LOG(LogTemp, Display, TEXT("    Spell validation: %s"),
		Validation.bIsValid ? TEXT("VALID") : *Validation.ErrorMessage);

	// Submit spell
	bool bSubmitSuccess = Orchestrator->SubmitAction(SpellAction);
	UE_LOG(LogTemp, Display, TEXT("    Submit result: %s"), bSubmitSuccess ? TEXT("SUCCESS") : TEXT("FAILED"));

	// Check results
	int32 FinalHP = TargetComp ? TargetComp->CurrentHP : 0;
	int32 DamageDealt = InitialHP - FinalHP;

	UE_LOG(LogTemp, Display, TEXT("    Target final HP: %d (took %d damage)"), FinalHP, DamageDealt);
	UE_LOG(LogTemp, Display, TEXT("    ActionResult.TotalDamageDealt: %d"), LastActionResult.TotalDamageDealt);
	UE_LOG(LogTemp, Display, TEXT("    ActionResult.EnergySpent: %d"), LastActionResult.EnergySpent);

	bool bDamageDealt = (DamageDealt > 0);
	bool bEnergySpent = (LastActionResult.EnergySpent > 0);
	bool bResultMatchesHP = (LastActionResult.TotalDamageDealt == DamageDealt);

	UE_LOG(LogTemp, Display, TEXT("    Damage dealt: %s"), bDamageDealt ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogTemp, Display, TEXT("    Energy spent: %s"), bEnergySpent ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogTemp, Display, TEXT("    Result matches HP: %s"), bResultMatchesHP ? TEXT("YES") : TEXT("NO"));

	bool bPassed = bSubmitSuccess && bDamageDealt && bEnergySpent && bResultMatchesHP;
	PrintTestResult("Spell Execution", bPassed);

	// Cleanup
	Orchestrator->ForceEndCombat();
	Orchestrator->OnActionExecuted.RemoveDynamic(this, &ACombatOrchestratorTestActor::OnTestActionExecuted);
	CleanupTestActors(Team0);
	CleanupTestActors(Team1);
}

void ACombatOrchestratorTestActor::Test_AbilityExecution()
{
	UE_LOG(LogTemp, Display, TEXT("\n[TEST] Ability Execution (DA_Test_Ability)"));

	// Load the ability data asset
	UAbilityData* TestAbility = LoadObject<UAbilityData>(nullptr,
		TEXT("/Game/Testing/Abilities/DA_Test_Ability.DA_Test_Ability"));

	if (!TestAbility)
	{
		UE_LOG(LogTemp, Error, TEXT("    Failed to load DA_Test_Ability - check asset path"));
		PrintTestResult("Ability Execution", false);
		return;
	}

	UE_LOG(LogTemp, Display, TEXT("    Loaded: %s (Damage: %d)"), *TestAbility->AbilityName, TestAbility->BaseDamage);

	ACombatOrchestrator* Orchestrator = GetOrCreateOrchestrator();
	if (!Orchestrator)
	{
		PrintTestResult("Ability Execution", false);
		return;
	}

	Orchestrator->bAutoAdvanceTurns = false;

	// Track action execution
	bActionExecuted = false;
	LastActionResult = FActionResult();
	Orchestrator->OnActionExecuted.AddDynamic(this, &ACombatOrchestratorTestActor::OnTestActionExecuted);

	// Create teams
	TArray<AActor*> Team0;
	AActor* User = CreateTestCharacter("AbilityUser", 5, 5, 5, 0, 0);
	Team0.Add(User);

	TArray<AActor*> Team1;
	AActor* Target = CreateTestCharacter("AbilityTarget", 3, 3, 3, 0, 1);
	Team1.Add(Target);

	// Get initial values
	UCharacterDataComponent* TargetComp = Target->FindComponentByClass<UCharacterDataComponent>();
	UCharacterDataComponent* UserComp = User->FindComponentByClass<UCharacterDataComponent>();
	int32 InitialHP = TargetComp ? TargetComp->CurrentHP : 0;
	int32 InitialEP = UserComp ? UserComp->CurrentEP : 0;

	UE_LOG(LogTemp, Display, TEXT("    Target initial HP: %d"), InitialHP);
	UE_LOG(LogTemp, Display, TEXT("    Attacker initial EP: %d"), InitialEP);

	// Start combat
	Orchestrator->StartCombat(Team0, Team1);

	// Create ability action
	FAction AbilityAction;
	AbilityAction.ActionType = EActionType::Ability;
	AbilityAction.AbilityData = TestAbility;
	AbilityAction.Targets.Add(Target);

	// Validate
	FActionValidationResult Validation = Orchestrator->ValidateAction(AbilityAction);
	UE_LOG(LogTemp, Display, TEXT("    Ability validation: %s"),
		Validation.bIsValid ? TEXT("VALID") : *Validation.ErrorMessage);

	// Submit ability
	bool bSubmitSuccess = Orchestrator->SubmitAction(AbilityAction);
	UE_LOG(LogTemp, Display, TEXT("    Submit result: %s"), bSubmitSuccess ? TEXT("SUCCESS") : TEXT("FAILED"));

	// Check results
	int32 FinalHP = TargetComp ? TargetComp->CurrentHP : 0;
	int32 FinalEP = UserComp ? UserComp->CurrentEP : 0;
	int32 DamageDealt = InitialHP - FinalHP;
	int32 EnergySpent = InitialEP - FinalEP;

	UE_LOG(LogTemp, Display, TEXT("    Target final HP: %d (took %d damage)"), FinalHP, DamageDealt);
	UE_LOG(LogTemp, Display, TEXT("    Attacker EP: %d -> %d (spent %d)"), InitialEP, FinalEP, EnergySpent);
	UE_LOG(LogTemp, Display, TEXT("    ActionResult.TotalDamageDealt: %d"), LastActionResult.TotalDamageDealt);

	bool bDamageDealt = (DamageDealt > 0);
	bool bActionFired = bActionExecuted;

	UE_LOG(LogTemp, Display, TEXT("    Damage dealt: %s"), bDamageDealt ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogTemp, Display, TEXT("    Action fired: %s"), bActionFired ? TEXT("YES") : TEXT("NO"));

	bool bPassed = bSubmitSuccess && bDamageDealt && bActionFired;
	PrintTestResult("Ability Execution", bPassed);

	// Cleanup
	Orchestrator->ForceEndCombat();
	Orchestrator->OnActionExecuted.RemoveDynamic(this, &ACombatOrchestratorTestActor::OnTestActionExecuted);
	CleanupTestActors(Team0);
	CleanupTestActors(Team1);
}

void ACombatOrchestratorTestActor::Test_MultiTargetAction()
{
	UE_LOG(LogTemp, Display, TEXT("\n[TEST] Multi-Target Action (Attack 3 targets)"));

	// Load the attack data asset
	UBaseAttackData* TestAttack = LoadObject<UBaseAttackData>(nullptr,
		TEXT("/Game/Testing/Attacks/DA_Test_Attack.DA_Test_Attack"));

	if (!TestAttack)
	{
		UE_LOG(LogTemp, Error, TEXT("    Failed to load DA_Test_Attack - check asset path"));
		PrintTestResult("Multi-Target Action", false);
		return;
	}

	ACombatOrchestrator* Orchestrator = GetOrCreateOrchestrator();
	if (!Orchestrator)
	{
		PrintTestResult("Multi-Target Action", false);
		return;
	}

	Orchestrator->bAutoAdvanceTurns = false;

	// Track action execution
	bActionExecuted = false;
	LastActionResult = FActionResult();
	Orchestrator->OnActionExecuted.AddDynamic(this, &ACombatOrchestratorTestActor::OnTestActionExecuted);

	// Create attacker
	TArray<AActor*> Team0;
	AActor* Attacker = CreateTestCharacter("MultiAttacker", 5, 6, 5, 0, 0);
	Team0.Add(Attacker);

	// Create 3 targets
	TArray<AActor*> Team1;
	AActor* Target1 = CreateTestCharacter("Target1", 3, 3, 3, 0, 1);
	AActor* Target2 = CreateTestCharacter("Target2", 3, 3, 3, 0, 1);
	AActor* Target3 = CreateTestCharacter("Target3", 3, 3, 3, 0, 1);
	Team1.Add(Target1);
	Team1.Add(Target2);
	Team1.Add(Target3);

	// Get initial HPs
	UCharacterDataComponent* Comp1 = Target1->FindComponentByClass<UCharacterDataComponent>();
	UCharacterDataComponent* Comp2 = Target2->FindComponentByClass<UCharacterDataComponent>();
	UCharacterDataComponent* Comp3 = Target3->FindComponentByClass<UCharacterDataComponent>();

	int32 InitHP1 = Comp1 ? Comp1->CurrentHP : 0;
	int32 InitHP2 = Comp2 ? Comp2->CurrentHP : 0;
	int32 InitHP3 = Comp3 ? Comp3->CurrentHP : 0;

	UE_LOG(LogTemp, Display, TEXT("    Initial HPs: %d, %d, %d"), InitHP1, InitHP2, InitHP3);

	// Start combat
	Orchestrator->StartCombat(Team0, Team1);

	// Create multi-target attack action
	FAction AttackAction;
	AttackAction.ActionType = EActionType::Attack;
	AttackAction.AttackData = TestAttack;
	AttackAction.Targets.Add(Target1);
	AttackAction.Targets.Add(Target2);
	AttackAction.Targets.Add(Target3);

	// Submit attack
	bool bSubmitSuccess = Orchestrator->SubmitAction(AttackAction);
	UE_LOG(LogTemp, Display, TEXT("    Submit result: %s"), bSubmitSuccess ? TEXT("SUCCESS") : TEXT("FAILED"));

	// Check damage to each target
	int32 Damage1 = InitHP1 - (Comp1 ? Comp1->CurrentHP : 0);
	int32 Damage2 = InitHP2 - (Comp2 ? Comp2->CurrentHP : 0);
	int32 Damage3 = InitHP3 - (Comp3 ? Comp3->CurrentHP : 0);
	int32 TotalDamage = Damage1 + Damage2 + Damage3;

	UE_LOG(LogTemp, Display, TEXT("    Damage dealt: %d, %d, %d"), Damage1, Damage2, Damage3);
	UE_LOG(LogTemp, Display, TEXT("    Total in result: %d"), LastActionResult.TotalDamageDealt);
	UE_LOG(LogTemp, Display, TEXT("    Affected targets count: %d"), LastActionResult.AffectedTargets.Num());

	bool bAllHit = (Damage1 > 0 && Damage2 > 0 && Damage3 > 0);
	bool bCorrectCount = (LastActionResult.AffectedTargets.Num() == 3);
	bool bTotalMatches = (LastActionResult.TotalDamageDealt == TotalDamage);

	UE_LOG(LogTemp, Display, TEXT("    All hit: %s"), bAllHit ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogTemp, Display, TEXT("    Correct count: %s"), bCorrectCount ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogTemp, Display, TEXT("    Total matches: %s"), bTotalMatches ? TEXT("YES") : TEXT("NO"));

	bool bPassed = bSubmitSuccess && bAllHit && bCorrectCount && bTotalMatches;
	PrintTestResult("Multi-Target Action", bPassed);

	// Cleanup
	Orchestrator->ForceEndCombat();
	Orchestrator->OnActionExecuted.RemoveDynamic(this, &ACombatOrchestratorTestActor::OnTestActionExecuted);
	CleanupTestActors(Team0);
	CleanupTestActors(Team1);
}

void ACombatOrchestratorTestActor::Test_EnergyCostDeduction()
{
	UE_LOG(LogTemp, Display, TEXT("\n[TEST] Energy Cost Deduction"));

	// Load the spell data asset
	USpellData* TestSpell = LoadObject<USpellData>(nullptr,
		TEXT("/Game/Testing/Spells/DA_Test_Spell.DA_Test_Spell"));

	if (!TestSpell)
	{
		UE_LOG(LogTemp, Error, TEXT("    Failed to load DA_Test_Spell - check asset path"));
		PrintTestResult("Energy Cost Deduction", false);
		return;
	}

	ACombatOrchestrator* Orchestrator = GetOrCreateOrchestrator();
	if (!Orchestrator)
	{
		PrintTestResult("Energy Cost Deduction", false);
		return;
	}

	Orchestrator->bAutoAdvanceTurns = false;

	// Track action execution
	bActionExecuted = false;
	LastActionResult = FActionResult();
	Orchestrator->OnActionExecuted.AddDynamic(this, &ACombatOrchestratorTestActor::OnTestActionExecuted);

	// FIXED: Caster on Team1 so they go first (Team1 acts first in turn order)
	TArray<AActor*> Team0;
	AActor* Target = CreateTestCharacter("EnergyTarget", 3, 3, 3, 0, 0);
	Team0.Add(Target);

	TArray<AActor*> Team1;
	AActor* Caster = CreateTestCharacter("EnergyCaster", 5, 3, 5, 0, 1);
	Team1.Add(Caster);

	// Get caster's initial EP
	UCharacterDataComponent* CasterComp = Caster->FindComponentByClass<UCharacterDataComponent>();
	int32 InitialEP = CasterComp ? CasterComp->CurrentEP : 0;

	UE_LOG(LogTemp, Display, TEXT("    Caster initial EP: %d"), InitialEP);

	// Start combat - Caster (Team1) should go first
	Orchestrator->StartCombat(Team0, Team1);

	// Get expected energy cost
	UCharacterData* CasterData = CasterComp ? CasterComp->CharacterData : nullptr;
	int32 ExpectedCost = TestSpell->CalculateEnergyCost(CasterData, true);
	UE_LOG(LogTemp, Display, TEXT("    Expected energy cost: %d"), ExpectedCost);

	// Create spell action
	FAction SpellAction;
	SpellAction.ActionType = EActionType::Spell;
	SpellAction.SpellData = TestSpell;
	SpellAction.Targets.Add(Target);
	SpellAction.bUseElementalMode = true;

	// Submit spell
	bool bSubmitSuccess = Orchestrator->SubmitAction(SpellAction);
	UE_LOG(LogTemp, Display, TEXT("    Submit result: %s"), bSubmitSuccess ? TEXT("SUCCESS") : TEXT("FAILED"));

	// Check energy deduction
	int32 FinalEP = CasterComp ? CasterComp->CurrentEP : 0;
	int32 ActualEnergySpent = InitialEP - FinalEP;

	UE_LOG(LogTemp, Display, TEXT("    Caster final EP: %d (spent %d)"), FinalEP, ActualEnergySpent);
	UE_LOG(LogTemp, Display, TEXT("    ActionResult.EnergySpent: %d"), LastActionResult.EnergySpent);

	bool bEnergyDeducted = (ActualEnergySpent > 0);
	bool bResultMatchesActual = (LastActionResult.EnergySpent == ActualEnergySpent);

	UE_LOG(LogTemp, Display, TEXT("    Energy deducted: %s"), bEnergyDeducted ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogTemp, Display, TEXT("    Result matches actual: %s"), bResultMatchesActual ? TEXT("YES") : TEXT("NO"));

	bool bPassed = bSubmitSuccess && bEnergyDeducted && bResultMatchesActual;
	PrintTestResult("Energy Cost Deduction", bPassed);

	// Cleanup
	Orchestrator->ForceEndCombat();
	Orchestrator->OnActionExecuted.RemoveDynamic(this, &ACombatOrchestratorTestActor::OnTestActionExecuted);
	CleanupTestActors(Team0);
	CleanupTestActors(Team1);
}

// ========================================
// TEST HELPERS
// ========================================

ACombatOrchestrator* ACombatOrchestratorTestActor::GetOrCreateOrchestrator()
{
	// Check if we have GameInstance (need to be in PIE)
	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatTest] Not in PIE! Press ALT+P to start Play-In-Editor, then run tests."));
		return nullptr;
	}

	// Reuse existing or spawn new
	if (!TestOrchestrator || !IsValid(TestOrchestrator))
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		TestOrchestrator = GetWorld()->SpawnActor<ACombatOrchestrator>(ACombatOrchestrator::StaticClass(), SpawnParams);

		if (TestOrchestrator)
		{
			UE_LOG(LogTemp, Log, TEXT("[CombatTest] Spawned test CombatOrchestrator"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[CombatTest] Failed to spawn CombatOrchestrator!"));
		}
	}

	return TestOrchestrator;
}

AActor* ACombatOrchestratorTestActor::CreateTestCharacter(const FString& Name, int32 Mind, int32 Body, int32 Spirit, int32 TurnSpeed, int32 TeamIndex)
{
	// Generate unique name to prevent collision with actors from previous tests
	FString UniqueName = FString::Printf(TEXT("%s_%d"), *Name, TestActorCounter++);

	// Spawn empty actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = FName(*UniqueName);
	AActor* TestActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

	if (!TestActor)
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatTest] Failed to spawn test actor: %s"), *UniqueName);
		return nullptr;
	}

	// Create CharacterData asset - use display name for logs
	UCharacterData* CharData = NewObject<UCharacterData>(TestActor);
	CharData->CharacterName = Name;  // Display name stays readable
	CharData->WorldMindLevel = Mind;
	CharData->WorldBodyLevel = Body;
	CharData->WorldSpiritLevel = Spirit;
	CharData->WorldTurnSpeedPoints = TurnSpeed;
	CharData->WorldAttackSpeedPoints = 0;

	// Create and setup component (CRITICAL: assign data BEFORE register)
	UCharacterDataComponent* CharComp = NewObject<UCharacterDataComponent>(TestActor);
	CharComp->CharacterData = CharData;
	CharComp->RegisterComponent();
	TestActor->AddOwnedComponent(CharComp);

	return TestActor;
}

void ACombatOrchestratorTestActor::CleanupTestActors(TArray<AActor*>& Actors)
{
	for (AActor* Actor : Actors)
	{
		if (Actor && IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	Actors.Empty();
}

void ACombatOrchestratorTestActor::PrintTestResult(const FString& TestName, bool bPassed)
{
	if (bPassed)
	{
		UE_LOG(LogTemp, Display, TEXT("  [PASS] %s"), *TestName);
		TestsPassed++;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("  [FAIL] %s"), *TestName);
		TestsFailed++;
	}
}

// ========================================
// EVENT HANDLERS
// ========================================

void ACombatOrchestratorTestActor::OnTestCombatStateChanged(ECombatState NewState)
{
	RecordedStateTransitions.Add(NewState);
}

void ACombatOrchestratorTestActor::OnTestCombatResultReady(const FCombatResult& Result)
{
	LastCombatResult = Result;
	bWaitingForResult = false;
}

void ACombatOrchestratorTestActor::OnTestActionExecuted(AActor* Actor, const FActionResult& Result)
{
	LastActionResult = Result;
	bActionExecuted = true;
	UE_LOG(LogTemp, Log, TEXT("[CombatTest] OnActionExecuted: Actor=%s, Success=%d, Type=%d"),
		Actor ? *Actor->GetName() : TEXT("None"),
		Result.bSuccess ? 1 : 0,
		(int32)Result.ActionType);
}