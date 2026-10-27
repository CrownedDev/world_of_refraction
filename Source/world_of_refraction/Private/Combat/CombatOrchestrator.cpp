// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/CombatOrchestrator.h"
#include "GameFramework/Pawn.h"
#include "Combat/TurnManager.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/ESkillEffectType.h"
#include "Skills/Effects/StatusBuildupManager.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/Defense/DefenseSystem.h"
#include "Infusion/InfusionChargeManager.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/CombatConstants.h"   // WORLDSTAT_KILL_DIVISOR (§7 C3 earn faucet)
#include "EngineUtils.h"              // TActorIterator for the wor.* world-stat debug commands
#include "Kismet/GameplayStatics.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Combat/FleeRules.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/CurrencyWallet.h"
#include "Combat/Grid/CombatGridSubsystem.h"
#include "UI/Combat/CombatHUD.h"
#include "Combat/Camera/CombatCameraDirector.h"
#include "GameFramework/PlayerController.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Definitions/AbilityData.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"
#include "Combat/Mechanics/BattleRandomSubsystem.h"
#include "Character/CharacterData.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Skills/Effects/FSkillEffect.h"
#include "Skills/Effects/FGatheredEffect.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/EAttachedItemKind.h"
#include "Equipment/Durability/DurabilityConstants.h"
#include "Equipment/Durability/CrystalBreak.h"
#include "Inventory/ItemEffectType.h"
#include "Combat/Mechanics/WeatherStateManager.h"
#include "UI/Combat/CombatCommandMenuSubsystem.h"
#include "Equipment/Crystals/CrystalManager.h"
#include "TimerManager.h"
#include "Combat/CombatControl.h"
#include "Party/PartySessionSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace CombatOwnedGearSync
{
	/** B-085 (Q-085): the active loadout led during the fight; its wear goes back to the owned gear. */
	void WriteActiveLoadoutsToOwned(const TArray<AActor *> &Team)
	{
		for (AActor *Actor : Team)
		{
			if (UInventoryComponent *Inv = Actor ? Actor->FindComponentByClass<UInventoryComponent>() : nullptr)
			{
				Inv->WriteLoadoutToOwned(Inv->ActiveLoadoutIndex);
			}
		}
	}
}

ACombatOrchestrator::ACombatOrchestrator()
{
	PrimaryActorTick.bCanEverTick = false;

	CombatState = ECombatState::Idle;
	CurrentActor = nullptr;
	CurrentTurnNumber = 0;

	TurnManagerRef = nullptr;
	SkillEffectManagerRef = nullptr;
	ActionExecutorRef = nullptr;
	bWaitingForAsyncAction = false;

	CameraDirector = CreateDefaultSubobject<UCombatCameraDirector>(TEXT("CameraDirector"));
}

void ACombatOrchestrator::BeginPlay()
{
	Super::BeginPlay();

	// Cache subsystem references FIRST
	UGameInstance *GI = GetGameInstance();
	if (GI)
	{
		TurnManagerRef = GI->GetSubsystem<UTurnManager>();
		SkillEffectManagerRef = GI->GetSubsystem<USkillEffectManager>();
		ActionExecutorRef = GI->GetSubsystem<UActionExecutor>();
	}

	if (!TurnManagerRef)
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatOrchestrator] Failed to get TurnManager subsystem!"));
	}

	if (!SkillEffectManagerRef)
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatOrchestrator] Failed to get SkillEffectManager subsystem!"));
	}

	if (!ActionExecutorRef)
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatOrchestrator] Failed to get ActionExecutor subsystem!"));
	}
	else
	{
		// D8: bound once for the orchestrator's lifetime (unbound in EndPlay) —
		// NOT in BindTurnManagerEvents, which runs per StartCombat.
		ActionExecutorRef->OnActionDeferredArmed.AddDynamic(this, &ACombatOrchestrator::HandleActionDeferredArmed);
	}

	AIDecisionManagerRef = GI ? GI->GetSubsystem<UAIDecisionManager>() : nullptr; // B-067: GI may be null

	if (!AIDecisionManagerRef)
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatOrchestrator] Failed to get AIDecisionManager subsystem!"));
	}

	// Auto-start combat if enabled (AFTER subsystems are cached)
	if (bAutoStartCombat)
	{
		// Delay to allow all actors to finish BeginPlay
		GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
														  { DebugStartCombatWithLevelActors(); });
	}
}

void ACombatOrchestrator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Unregister from systems that hold a back-reference to us
	if (AIDecisionManagerRef)
	{
		AIDecisionManagerRef->ClearCombatOrchestrator();
	}
	if (UGameInstance *GI = GetGameInstance())
	{
		if (UCombatCommandMenuSubsystem *Menu = GI->GetSubsystem<UCombatCommandMenuSubsystem>())
		{
			Menu->ClearCombatOrchestrator();
		}
	}

	UnbindTurnManagerEvents();
	if (ActionExecutorRef)
	{
		ActionExecutorRef->OnActionDeferredArmed.RemoveDynamic(this, &ACombatOrchestrator::HandleActionDeferredArmed);
	}
	GetWorld()->GetTimerManager().ClearTimer(AutoAdvanceTimerHandle);

	TeardownCombatHUD();

	Super::EndPlay(EndPlayReason);
}

void ACombatOrchestrator::OnCombatStartedUI_Implementation(const TArray<AActor *> &LocalParty, const TArray<AActor *> &OpposingParty)
{
	UWorld *World = GetWorld();
	APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] No local player controller - native combat HUD skipped"));
		return;
	}

	if (!CombatHUD)
	{
		CombatHUD = NewObject<UCombatHUD>(this);
	}
	CombatHUD->Build(PC, LocalParty, OpposingParty);
}

void ACombatOrchestrator::TeardownCombatHUD()
{
	if (CombatHUD)
	{
		CombatHUD->Teardown();
	}
}

// ========================================
// COMBAT CONTROL
// ========================================

void ACombatOrchestrator::StartCombat(const TArray<AActor *> &LocalParty, const TArray<AActor *> &OpposingParty, EAIDifficulty Difficulty)
{
	// B-185: the first strike belongs to this start only, even one that returns early.
	const EFirstStrike FightFirstStrike = FirstStrike;
	FirstStrike = EFirstStrike::None;

	// End any previous combat FIRST — ForceEndCombat clears the AI/menu
	// registrations, so doing it after them (as before) left the AI unregistered.
	if (CombatState != ECombatState::Idle)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] StartCombat called while combat already active. Forcing end."));
		ForceEndCombat();
	}

	// Store difficulty
	CombatDifficulty = Difficulty;

	// Register with AI manager
	if (AIDecisionManagerRef)
	{
		AIDecisionManagerRef->SetCombatOrchestrator(this);
	}

	// Register with combat command menu
	if (UGameInstance *GI = GetGameInstance())
	{
		if (UCombatCommandMenuSubsystem *Menu = GI->GetSubsystem<UCombatCommandMenuSubsystem>())
		{
			Menu->SetCombatOrchestrator(this);
		}
	}

	if (!TurnManagerRef)
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatOrchestrator] Cannot start combat - TurnManager not available!"));
	}

	// Validate teams
	if (LocalParty.Num() == 0 || OpposingParty.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatOrchestrator] Cannot start combat - empty team(s)!"));
		return;
	}

	// B-010: seed this battle's random stream before anything rolls (turn 1 can act inside
	// InitializeCombat). The seed is logged so the fight can be replayed.
	if (UBattleRandomSubsystem *BattleRandom = UBattleRandomSubsystem::Get(this))
	{
		BattleRandom->BeginBattle();
	}

	// Store team references
	LocalPartyCombatants = LocalParty;
	OpposingPartyCombatants = OpposingParty;

	// §7 C3 — reset the world-stat earn pool and bind death listeners on BOTH teams (the handler
	// filters to enemy/OpposingParty deaths). Mirrors TurnManager's OnDied bind/unbind lifecycle; AddUnique
	// so a re-init can't double-bind. Unbound in HandleCombatEnded before the team arrays empty.
	PendingWorldStatPool = 0;
	for (const TArray<AActor *> *Team : {&LocalPartyCombatants, &OpposingPartyCombatants})
	{
		for (AActor *Actor : *Team)
		{
			if (UCharacterDataComponent *CharComp = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr)
			{
				CharComp->OnDied.AddUniqueDynamic(this, &ACombatOrchestrator::OnCombatantDied);
			}
		}
	}

	CurrentTurnNumber = 0;
	CurrentActor = nullptr;
	bWaitingForAsyncAction = false;

	SetCombatState(ECombatState::Initializing);

	// D2: clear per-match state (fires-once set) before any combat-start effect applies.
	if (SkillEffectManagerRef)
	{
		SkillEffectManagerRef->ResetForNewCombat();
	}

	// Prepare loadouts for battle
	PrepareAllLoadoutsForBattle();

	// Assign grid positions and place actors
	if (UCombatGridSubsystem *Grid = GetGameInstance()->GetSubsystem<UCombatGridSubsystem>())
	{
		FVector ArenaCenter = GetActorLocation();

		// D-042: members start in their chosen formation cell, enemies in their placeholder row.
		const UPartySessionSubsystem *PartySession = GetGameInstance()->GetSubsystem<UPartySessionSubsystem>();
		TArray<FGridStartPreference> LocalPreferences;
		for (AActor *Member : LocalPartyCombatants)
		{
			FGridStartPreference &Preference = LocalPreferences.AddDefaulted_GetRef();
			const UCharacterDataComponent *Comp = Member ? Member->FindComponentByClass<UCharacterDataComponent>() : nullptr;
			const int32 Slot = PartySession && Comp ? PartySession->GetMemberSlotByData(Comp->CharacterData) : INDEX_NONE;
			ECombatRow Row;
			int32 Column;
			if (Slot != INDEX_NONE && PartySession->GetMemberFormation(Slot, Row, Column))
			{
				Preference = FGridStartPreference::Cell(Row, Column);
			}
		}
		TArray<FGridStartPreference> OpposingPreferences;
		for (AActor *Enemy : OpposingPartyCombatants)
		{
			OpposingPreferences.Add(FGridStartPreference::RowOnly(GetPlaceholderEnemyStartRow(Enemy)));
		}
		Grid->AssignTeamWithPreferences(LocalPartyCombatants, 0, LocalPreferences);
		Grid->AssignTeamWithPreferences(OpposingPartyCombatants, 1, OpposingPreferences);
		Grid->PlaceAllActors(ArenaCenter);
		Grid->UpdateAllActorFacing(ArenaCenter);

		UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Grid positions assigned and actors placed"));
	}

	// Initialise weather leaders
	if (UWeatherStateManager *WeatherManager = GetGameInstance()->GetSubsystem<UWeatherStateManager>())
	{
		WeatherManager->InitialiseLeaders(LocalPartyCombatants, OpposingPartyCombatants);
	}

	// Set arena center for ActionExecutor movement calculations
	if (UActionExecutor *Executor = GetGameInstance()->GetSubsystem<UActionExecutor>())
	{
		Executor->SetArenaCenter(GetActorLocation());
	}

	// Bind to TurnManager events and start turns (if TurnManager available)
	if (!TurnManagerRef)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] TurnManager not available - grid positioned but turns won't advance"));
		return;
	}

	// A combat left active on the GameInstance-scoped TurnManager (e.g. a previous
	// orchestrator destroyed mid-fight) must end BEFORE we bind: InitializeCombat would
	// otherwise EndCombat it with our handlers bound and InProgress set, tearing down
	// this combat as a Draw. No-op when inactive.
	TurnManagerRef->EndCombat();

	BindTurnManagerEvents();

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Starting combat: LocalParty (%d) vs OpposingParty (%d)"),
		   LocalParty.Num(), OpposingParty.Num());

	// InProgress BEFORE InitializeCombat: the first turn is dispatched inside it, and
	// OnActionCompleted bails unless InProgress — a turn-1 death or pass would hang.
	SetCombatState(ECombatState::InProgress);

	// Cameras before InitializeCombat: the first turn is dispatched inside it.
	if (CameraDirector)
	{
		CameraDirector->BeginCombat(LocalParty, OpposingParty, ActionExecutorRef);
	}

	// Initialize TurnManager (this will trigger first OnTurnStarted)
	const int32 OpeningTeam = FightFirstStrike == EFirstStrike::Party   ? TurnManagerTeams::LOCAL_TEAM_INDEX
							: FightFirstStrike == EFirstStrike::Enemy ? TurnManagerTeams::OPPOSING_TEAM_INDEX
																 : INDEX_NONE;
	TurnManagerRef->InitializeCombat(LocalParty, OpposingParty, OpeningTeam);

	// Turn 1 can resolve the whole combat inside InitializeCombat (state back to Idle,
	// teams emptied) — don't build UI for a fight that's already over.
	if (CombatState == ECombatState::InProgress)
	{
		OnCombatStartedUI(LocalPartyCombatants, OpposingPartyCombatants);
	}
}

void ACombatOrchestrator::ForceEndCombat(ECombatState ForcedState)
{
	if (CombatState == ECombatState::Idle)
		return;

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Force ending combat with state %d"), (int32)ForcedState);

	// Unbind first so TurnManager::EndCombat below can't re-enter HandleCombatEnded.
	UnbindTurnManagerEvents();

	// Forced end: drop the combat HUD now. The results screen (BattleGameMode) is
	// its own widget; win paths leave the HUD up and EndPlay removes it.
	TeardownCombatHUD();

	CombatOwnedGearSync::WriteActiveLoadoutsToOwned(LocalPartyCombatants);

	ReleaseCombatResources();

	if (TurnManagerRef)
	{
		TurnManagerRef->EndCombat();
	}

	SetCombatState(ForcedState);

	FCombatResult Result = BuildCombatResult();
	OnCombatResultReady.Broadcast(Result);

	// Reset for next combat
	LocalPartyCombatants.Empty();
	OpposingPartyCombatants.Empty();
	CurrentActor = nullptr;
	CurrentTurnNumber = 0;
	bWaitingForAsyncAction = false;
	DeferredPayloads.Empty(); // W-B: no armed ritual leaks across combats

	SetCombatState(ECombatState::Idle);
}

void ACombatOrchestrator::ReleaseCombatResources()
{
	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoAdvanceTimerHandle);
	}

	UnbindTurnManagerEvents();

	// B-071: the per-fight Player flag (a take-over or a switch) ends with the fight; placed
	// pawns are reused by the next fight, so it must not carry.
	for (AActor *Member : LocalPartyCombatants)
	{
		CombatControl::SetPlayerDriven(Member, false);
	}

	if (CameraDirector)
	{
		CameraDirector->EndCombat();
	}

	UGameInstance *GI = GetGameInstance();

	// B-030: nothing from this fight survives into the next. The executor goes first: it drops the
	// in-flight action's defense windows unresolved, then the defense system forgets them.
	if (ActionExecutorRef)
	{
		ActionExecutorRef->ResetForCombat();
	}
	if (UDefenseSystem *Defense = GI ? GI->GetSubsystem<UDefenseSystem>() : nullptr)
	{
		Defense->ResetForCombat();
	}
	if (UInfusionChargeManager *Infusion = GI ? GI->GetSubsystem<UInfusionChargeManager>() : nullptr)
	{
		Infusion->ResetForCombat();
	}

	if (UWeatherStateManager *WeatherManager = GI ? GI->GetSubsystem<UWeatherStateManager>() : nullptr)
	{
		WeatherManager->EndCombat();
	}

	if (SkillEffectManagerRef)
	{
		SkillEffectManagerRef->ClearAllEffects();
		SkillEffectManagerRef->ResetForNewCombat();
	}

	UStatusBuildupManager *BuildupManager = GI ? GI->GetSubsystem<UStatusBuildupManager>() : nullptr;
	for (const TArray<AActor *> *Team : {&LocalPartyCombatants, &OpposingPartyCombatants})
	{
		for (AActor *Actor : *Team)
		{
			// Defensive IsValid: an actor may have been destroyed mid-combat.
			if (!IsValid(Actor))
			{
				continue;
			}
			if (BuildupManager)
			{
				BuildupManager->ResetStatusBar(Actor);
			}
			// Death listeners bound in StartCombat.
			if (UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>())
			{
				CharComp->OnDied.RemoveDynamic(this, &ACombatOrchestrator::OnCombatantDied);
			}
		}
	}

	if (BuildupManager)
	{
		BuildupManager->ResetForCombat();
	}

	// Loadout battle-ready flags (transfer model: no item debit here).
	ConsumeAllUsedItems();

	if (UCombatGridSubsystem *Grid = GI ? GI->GetSubsystem<UCombatGridSubsystem>() : nullptr)
	{
		Grid->ClearAllPositions();
	}

	if (AIDecisionManagerRef)
	{
		AIDecisionManagerRef->ClearCombatOrchestrator();
	}
	if (UCombatCommandMenuSubsystem *Menu = GI ? GI->GetSubsystem<UCombatCommandMenuSubsystem>() : nullptr)
	{
		Menu->ClearCombatOrchestrator();
	}

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Combat resources released"));
}

// ========================================
// ACTION SUBMISSION
// ========================================

bool ACombatOrchestrator::SubmitAction(const FAction &Action)
{
	if (CombatState != ECombatState::InProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] SubmitAction called but combat not in progress"));
		return false;
	}

	// D-091: fleeing has its own rules (rank, Gold, ending the fight); only the async path has them.
	if (Action.ActionType == EActionType::Flee)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] SubmitAction: Flee goes through SubmitActionAsync - refused"));
		return false;
	}

	AActor *Actor = CurrentActor;
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] SubmitAction called with no current actor"));
		return false;
	}

	if (!ActionExecutorRef)
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatOrchestrator] ActionExecutor not available!"));
		return false;
	}

	if (bWaitingForAsyncAction)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] Already waiting for async action to complete"));
		return false;
	}

	// Clear auto-advance timer since we're submitting a real action
	GetWorld()->GetTimerManager().ClearTimer(AutoAdvanceTimerHandle);

	// Validate action
	FActionValidationResult Validation = ActionExecutorRef->ValidateAction(CurrentActor, Action);
	UE_LOG(LogWoRCombat, Verbose, TEXT("[CombatOrchestrator] Validation: %s - %s"),
		   Validation.bIsValid ? TEXT("VALID") : TEXT("INVALID"),
		   *Validation.ErrorMessage);
	if (!Validation.bIsValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] Action validation failed: %s"), *Validation.ErrorMessage);
		return false;
	}

	// Check if this action requires async execution (projectile spells, attacks with movement)
	bool bRequiresAsync = false;

	if (Action.ActionType == EActionType::Spell && Action.SpellData)
	{
		// Projectile, AOE, and Instant spells need async for the per-impact defense
		// window (Projectile = projectile-arrival moment; AOE and Instant = Cast-notify
		// resolution moment). Instant is now defendable per-impact (Hard difficulty),
		// so it routes async like AOE.
		// D6: Cast entries are authoritative when present (any such entry → async);
		// loose DeliveryType is the empty-CastArray fallback.
		if (Action.SpellData->CastArray.Num() > 0)
		{
			for (const FSkillCastEntry &Entry : Action.SpellData->CastArray)
			{
				if (Entry.DeliveryType == ESpellDeliveryType::Projectile ||
					Entry.DeliveryType == ESpellDeliveryType::AOE ||
					Entry.DeliveryType == ESpellDeliveryType::Instant)
				{
					bRequiresAsync = true;
					break;
				}
			}
		}
		else
		{
			ESpellDeliveryType Delivery = Action.SpellData->DeliveryType;
			bRequiresAsync = (Delivery == ESpellDeliveryType::Projectile ||
							  Delivery == ESpellDeliveryType::AOE ||
							  Delivery == ESpellDeliveryType::Instant);
		}
	}
	else if (Action.ActionType == EActionType::Ability && Action.SkillData)
	{
		// Has a montage → async (W3): the runner owns the montage chain + warp + defense window.
		// Covers attacks + abilities (merged, Cluster 3) — reads the unified SkillData pointer.
		// (Movement no longer decides async; the presence of an animation to play does.)
		bRequiresAsync = (Action.SkillData->SkillMontage != nullptr ||
						  Action.SkillData->RitualCastMontage != nullptr ||
						  Action.SkillData->ReturnMontage != nullptr);
	}

	if (bRequiresAsync)
	{
		// Use async path - turn will end when action fully completes
		bWaitingForAsyncAction = true;

		UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] %s executing %s asynchronously..."),
			   *CurrentActor->GetName(), *Action.GetActionName());

		ActionExecutorRef->ExecuteActionAsync(CurrentActor, Action,
											  FOnActionComplete::CreateUObject(this, &ACombatOrchestrator::HandleAsyncActionCompleted));

		return true; // Action started, will complete later
	}

	// Synchronous execution for instant actions
	FActionResult Result = ActionExecutorRef->ExecuteAction(CurrentActor, Action);

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] %s executed %s: %s (Damage: %d, Healing: %d)"),
		   *CurrentActor->GetName(),
		   *Action.GetActionName(),
		   Result.bSuccess ? TEXT("SUCCESS") : TEXT("FAILED"),
		   Result.TotalDamageDealt,
		   Result.TotalHealingDone);

	// Broadcast result for UI
	OnActionExecuted.Broadcast(CurrentActor, Result);

	// End turn
	OnActionCompleted();

	return Result.bSuccess;
}

void ACombatOrchestrator::SubmitActionAsync(const FAction &Action)
{
	FString InvalidReason;
	if (StartActionAsync(Action, InvalidReason) || InvalidReason.IsEmpty())
	{
		return;
	}

	// Never strand the turn: the auto-advance timer is already cleared and the
	// caller (AI timer / menu click) has no return value to recover with.
	AActor *Actor = CurrentActor;
	if (IsActorAIControlled(Actor))
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] %s AI action invalid (%s) - passing turn"),
			   *Actor->GetName(), *InvalidReason);
		OnActionCompleted();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] %s action invalid (%s) - re-requesting"),
			   *Actor->GetName(), *InvalidReason);
		RequestActionFromActor(Actor);
	}
}

bool ACombatOrchestrator::StartActionAsync(const FAction &Action, FString &OutInvalidReason)
{
	OutInvalidReason.Reset();

	if (CombatState != ECombatState::InProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] Action submitted but combat not in progress"));
		return false;
	}

	if (!CurrentActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] Action submitted with no current actor"));
		return false;
	}

	if (!ActionExecutorRef)
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatOrchestrator] ActionExecutor not available!"));
		return false;
	}

	if (bWaitingForAsyncAction)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] Already waiting for async action to complete"));
		return false;
	}

	GetWorld()->GetTimerManager().ClearTimer(AutoAdvanceTimerHandle);

	const FActionValidationResult Validation = ActionExecutorRef->ValidateAction(CurrentActor, Action);
	if (!Validation.bIsValid)
	{
		OutInvalidReason = Validation.ErrorMessage.IsEmpty() ? FString(TEXT("invalid action")) : Validation.ErrorMessage;
		return false;
	}

	if (Action.ActionType == EActionType::Flee)
	{
		const FText FleeRefused = GetFleeRefusedReason(CurrentActor);
		if (!FleeRefused.IsEmpty())
		{
			OutInvalidReason = FleeRefused.ToString();
			return false;
		}
		bWaitingForAsyncAction = true; // nothing else may start while the fight ends
		BeginFlee();
		return true;
	}

	bWaitingForAsyncAction = true;

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] %s executing %s asynchronously..."),
		   *CurrentActor->GetName(), *Action.GetActionName());

	ActionExecutorRef->ExecuteActionAsync(CurrentActor, Action,
										  FOnActionComplete::CreateUObject(this, &ACombatOrchestrator::HandleAsyncActionCompleted));
	return true;
}

FText ACombatOrchestrator::GetFleeRefusedReason(AActor *Actor) const
{
	if (CombatState != ECombatState::InProgress)
	{
		return NSLOCTEXT("CombatOrchestrator", "FleeNoFight", "There is no fight to flee.");
	}
	if (!Actor || !LocalPartyCombatants.Contains(Actor))
	{
		return NSLOCTEXT("CombatOrchestrator", "FleeNotParty", "Only your party can flee.");
	}
	if (!FleeRules::CanFleeRank(EncounterRank))
	{
		return NSLOCTEXT("CombatOrchestrator", "FleeBoss", "You can't flee this fight.");
	}
	return FText::GetEmpty();
}

namespace
{
	/** A free row step moves exactly one row (D-096). */
	constexpr int32 ORCHESTRATOR_ROW_STEP = 1;

	/** The cell one row forward or back, same column (Q-180: never sideways). */
	FCombatGridPosition OrchestratorStepTarget(const FCombatGridPosition &From, bool bForward)
	{
		FCombatGridPosition Target = From;
		Target.Row = CombatRowHelpers::GetRowFromIndex(From.GetRowIndex() + (bForward ? ORCHESTRATOR_ROW_STEP : -ORCHESTRATOR_ROW_STEP));
		return Target;
	}
}

FText ACombatOrchestrator::GetRowMoveRefusedReason(AActor *Actor, bool bForward) const
{
	if (CombatState != ECombatState::InProgress)
	{
		return NSLOCTEXT("CombatOrchestrator", "RowNoFight", "There is no fight.");
	}
	if (!Actor || Actor != CurrentActor)
	{
		return NSLOCTEXT("CombatOrchestrator", "RowNotTurn", "Only on your turn.");
	}
	if (SkillEffectManagerRef && SkillEffectManagerRef->IsStunned(Actor))
	{
		return NSLOCTEXT("CombatOrchestrator", "RowStunned", "Can't move while stunned.");
	}
	if (bRowMoveUsedThisTurn)
	{
		return NSLOCTEXT("CombatOrchestrator", "RowUsed", "Already moved this turn.");
	}
	const UCombatGridSubsystem *Grid = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCombatGridSubsystem>() : nullptr;
	FCombatGridPosition Position;
	if (!Grid || !Grid->GetActorPosition(Actor, Position))
	{
		return NSLOCTEXT("CombatOrchestrator", "RowNoGrid", "Not on the battle grid.");
	}
	if (bForward && Position.Row == ECombatRow::Front)
	{
		return NSLOCTEXT("CombatOrchestrator", "RowAtFront", "Already in the front row.");
	}
	if (!bForward && Position.Row == ECombatRow::Back)
	{
		return NSLOCTEXT("CombatOrchestrator", "RowAtBack", "Already in the back row.");
	}
	// Q-180: in battle you step only into an empty cell, same column; swapping places with an
	// ally is for formation outside battle (D-042). A fallen ally's cell is taken too.
	if (Grid->GetActorAtPosition(OrchestratorStepTarget(Position, bForward)))
	{
		return NSLOCTEXT("CombatOrchestrator", "RowTaken", "That cell is taken.");
	}
	return FText::GetEmpty();
}

bool ACombatOrchestrator::TryMoveRow(AActor *Actor, bool bForward, FText &OutRefusedReason)
{
	OutRefusedReason = GetRowMoveRefusedReason(Actor, bForward);
	if (!OutRefusedReason.IsEmpty())
	{
		return false;
	}

	UCombatGridSubsystem *Grid = GetGameInstance()->GetSubsystem<UCombatGridSubsystem>();
	FCombatGridPosition Position;
	Grid->GetActorPosition(Actor, Position);
	const FCombatGridPosition Target = OrchestratorStepTarget(Position, bForward);

	// The target cell is empty (GetRowMoveRefusedReason), so this is a plain step (Q-180).
	if (!Grid->MoveActorToRow(Actor, Target.Row, GetActorLocation()))
	{
		OutRefusedReason = NSLOCTEXT("CombatOrchestrator", "RowBlocked", "Can't move there.");
		return false;
	}
	bRowMoveUsedThisTurn = true;
	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] %s stepped to the %s row"), *Actor->GetName(),
		   *CombatRowHelpers::GetRowName(Target.Row));
	return true;
}

void ACombatOrchestrator::BeginFlee()
{
	// D-046 (B-151): the party shares one wallet, so the flee loss is taken once per wallet,
	// not once per member.
	int32 TotalGoldLost = 0;
	TSet<UCurrencyComponent *> ChargedWallets;
	for (AActor *Member : LocalPartyCombatants)
	{
		UCurrencyComponent *Currency = CurrencyWallet::FindFor(Member);
		if (!Currency || ChargedWallets.Contains(Currency))
		{
			continue;
		}
		ChargedWallets.Add(Currency);
		const int32 Lost = FleeRules::GoldLost(Currency->GetBalance(ECurrencyType::Gold));
		if (Lost > 0 && Currency->Spend(ECurrencyType::Gold, Lost))
		{
			TotalGoldLost += Lost;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] %s fled the fight - party lost %d Gold"),
		   *GetNameSafe(CurrentActor), TotalGoldLost);

	FActionResult Result;
	Result.Executor = CurrentActor;
	Result.ActionType = EActionType::Flee;
	Result.bSuccess = true;
	OnActionExecuted.Broadcast(CurrentActor, Result);

	// Party vitals are kept: ForceEndCombat broadcasts the result while the teams are still listed,
	// so ABattleGameMode banks HP/EP before the return to the trial.
	GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		ForceEndCombat(ECombatState::Fled);
	}));
}

FActionValidationResult ACombatOrchestrator::ValidateAction(const FAction &Action) const
{
	if (!ActionExecutorRef)
	{
		return FActionValidationResult(false, TEXT("ActionExecutor not available"));
	}

	AActor *Actor = CurrentActor;
	if (!Actor)
	{
		return FActionValidationResult(false, TEXT("No current actor"));
	}

	return ActionExecutorRef->ValidateAction(CurrentActor, Action);
}

void ACombatOrchestrator::HandleAsyncActionCompleted(const FActionResult &Result)
{
	bWaitingForAsyncAction = false;

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Async action completed: %s (Damage: %d, Healing: %d)"),
		   Result.bSuccess ? TEXT("SUCCESS") : TEXT("FAILED"),
		   Result.TotalDamageDealt,
		   Result.TotalHealingDone);
	// Broadcast result for UI
	if (CurrentActor)
	{
		OnActionExecuted.Broadcast(CurrentActor, Result);
	}

	// End turn
	OnActionCompleted();
}

void ACombatOrchestrator::OnActionCompleted()
{
	if (bWaitingForAsyncAction)
	{
		UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] OnActionCompleted ignored - waiting for async action"));
		return;
	}

	if (CombatState != ECombatState::InProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] OnActionCompleted called but combat not in progress"));
		return;
	}

	// Clear auto-advance timer if it was running
	GetWorld()->GetTimerManager().ClearTimer(AutoAdvanceTimerHandle);

	// Process end-of-turn effects
	ProcessEndOfTurnEffects(CurrentActor);

	// Check win condition
	ECombatState WinState = CheckWinCondition();
	if (WinState != ECombatState::InProgress)
	{
		UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Win condition met: %d"), (int32)WinState);

		// Unbind first: TurnManager::EndCombat below would otherwise re-enter
		// HandleCombatEnded. Everything that handler did now happens here.
		UnbindTurnManagerEvents();

		// Clear all status effects from combat
		if (SkillEffectManagerRef)
		{
			SkillEffectManagerRef->ClearAllEffects();
		}

		// Phase B: destroy any crystals that broke during combat. Runs BEFORE
		// repair so destroyed crystals aren't candidates for repair.
		ApplyBetweenCombatCrystalDestruction();

		// Auto-repair crystals between combats. Only fires on completed battles
		// (ForceEndCombat does NOT repair — abort cases aren't "battles completed").
		ApplyBetweenCombatRepair();

		CombatOwnedGearSync::WriteActiveLoadoutsToOwned(LocalPartyCombatants);

		TurnManagerRef->EndCombat();

		SetCombatState(WinState);

		FCombatResult Result = BuildCombatResult();
		OnCombatResultReady.Broadcast(Result);

		// §7 C3 — win-only world-stat draft. Was in HandleCombatEnded, which never ran
		// on this path (events were unbound above), so it never fired on a real win.
		if (WinState == ECombatState::Victory && PendingWorldStatPool > 0)
		{
			OnWorldStatDraftReady.Broadcast(PendingWorldStatPool);
		}

		ReleaseCombatResources();

		// Reset for next combat
		LocalPartyCombatants.Empty();
		OpposingPartyCombatants.Empty();
		CurrentActor = nullptr;
		DeferredPayloads.Empty(); // W-B: no armed ritual leaks across combats

		SetCombatState(ECombatState::Idle);
		return;
	}

	// Advance to next turn
	TurnManagerRef->AdvanceToNextTurn();
}

// ========================================
// TURN MANAGER EVENT HANDLERS
// ========================================

void ACombatOrchestrator::HandleTurnStarted(AActor *Actor, int32 TurnNumber)
{
	CurrentActor = Actor;
	CurrentTurnNumber = TurnNumber;

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Turn %d started: %s"),
		   TurnNumber, *Actor->GetName());

	// W-B: an Execution turn carries a scheduled ritual fire instead of an action request.
	// Q-044d (B-225): it is still one of the caster's turns, so the turn-start step runs first
	// (spans end, payouts land); FireScheduledExecution skips the fire if that step killed the
	// caster. Every other turn kind (Normal / Bonus) runs the ordinary flow. The countdown that
	// decides WHEN this fires lives in TurnManager (delay burns on Normal turns).
	if (TurnManagerRef && TurnManagerRef->GetCurrentTurnType() == ETurnType::Execution)
	{
		ProcessStartOfTurnEffects(CurrentActor);
		FireScheduledExecution();
		return;
	}

	ProceedWithTurnStart();
}

void ACombatOrchestrator::ProceedWithTurnStart()
{
	// ET-19: each turn brings back the one free weapon / ring switch.
	if (ULoadoutComponent *Loadout = CurrentActor ? CurrentActor->FindComponentByClass<ULoadoutComponent>() : nullptr)
	{
		Loadout->ResetTurnSwitch();
	}
	bRowMoveUsedThisTurn = false; // D-096: and the one free row move

	// Process start-of-turn status effects
	ProcessStartOfTurnEffects(CurrentActor);

	// Process Broken Darkness overflow (aura damage to nearby, self-damage, energy drain)
	ProcessBrokenDarknessOverflow(CurrentActor);

	// Check if actor died from start-of-turn effects (or a ritual that just fired)
	if (!IsActorAlive(CurrentActor))
	{
		UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] %s died at turn start, skipping action"),
			   *CurrentActor->GetName());
		OnActionCompleted();
		return;
	}

	// Broadcast for UI/listeners
	OnActorTurnStarted.Broadcast(CurrentActor, CurrentTurnNumber);

	// Stunned actors are asked too (D-001: Attack/Defend still allowed). ValidateAction
	// enforces the rule; an invalid pick re-asks a player and passes an AI's turn.
	RequestActionFromActor(CurrentActor);
}

void ACombatOrchestrator::HandleActionDeferredArmed(AActor *Caster, const FAction &Action, int32 DelayTurns)
{
	if (!TurnManagerRef)
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatOrchestrator] Ritual armed but no TurnManager — cannot schedule Execution turn"));
		return;
	}

	// Stash the frozen intent under a fresh payload handle; TurnManager carries the
	// handle on the scheduled Execution turn and hands it back at fire time.
	const int32 PayloadId = NextPayloadId++;
	FDeferredActivation Entry;
	Entry.Action = Action;
	Entry.Caster = Caster;
	DeferredPayloads.Add(PayloadId, Entry);

	// Freeze the caster's spell speed NOW — TurnManager stays decoupled (no CharacterData
	// lookup) and uses it as the fire-time SortKey among directly-adjacent ready Execution
	// turns (faster fires first). The orchestrator already reads CharacterData this way.
	float FrozenSpellSpeed = 0.0f;
	if (UCharacterDataComponent *CharComp = Caster ? Caster->FindComponentByClass<UCharacterDataComponent>() : nullptr)
	{
		if (CharComp->CharacterData)
		{
			FrozenSpellSpeed = CharComp->CharacterData->CalculateSpellSpeed();
		}
	}

	TurnManagerRef->ScheduleExecutionTurn(Caster, DelayTurns, PayloadId, FrozenSpellSpeed);

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Ritual armed (payload %d, spellspeed %.2f) — Execution turn in %d normal turn(s)"),
		   PayloadId, FrozenSpellSpeed, DelayTurns);
}

void ACombatOrchestrator::FireScheduledExecution()
{
	if (!ActionExecutorRef)
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatOrchestrator] Execution turn but no ActionExecutor — skipping"));
		TurnManagerRef->AdvanceToNextTurn();
		return;
	}

	const int32 PayloadId = TurnManagerRef->GetCurrentTurnPayloadId();
	FDeferredActivation *Entry = DeferredPayloads.Find(PayloadId);
	if (!Entry)
	{
		// Malformed slot (payload missing) — skip without requesting an action.
		UE_LOG(LogTemp, Error, TEXT("[CombatOrchestrator] Execution turn payload %d not found — skipping"), PayloadId);
		TurnManagerRef->AdvanceToNextTurn();
		return;
	}

	FAction FireAction = Entry->Action;
	FireAction.bIsDeferredFire = true; // cost paid at arm — gates cost/validation
	AActor *Caster = Entry->Caster;
	DeferredPayloads.Remove(PayloadId); // consume

	// Defensive: TurnManager already prunes dead grantees before emitting an Execution
	// turn, so this normally can't trip. Skip the fire if it somehow does.
	if (!IsActorAlive(Caster))
	{
		UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Execution turn caster %s dead/invalid — skipping fire"),
			   IsValid(Caster) ? *Caster->GetName() : TEXT("<invalid>"));
		TurnManagerRef->AdvanceToNextTurn();
		return;
	}

	// Option 2: the Execution turn is a normal turn that skips "ask the character what to
	// do" and runs the pre-decided ritual through the SAME async start a normal action uses
	// (B-031: the old sync router dropped a montage-less ritual ability or spell, because sync
	// ExecuteAction refuses those types). CurrentActor is already the caster (HandleTurnStarted
	// set it before this branch). Validation lets bIsDeferredFire past the stun/silence/
	// dead-target gates and zeroes cost (paid at arm); completion binds
	// HandleAsyncActionCompleted → return → finalize → single AdvanceToNextTurn.
	// bWaitingForAsyncAction is owned by StartActionAsync — do NOT pre-set it here.
	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Execution turn: firing %s (caster %s, payload %d)"),
		   *FireAction.GetActionName(), *Caster->GetName(), PayloadId);

	FString InvalidReason;
	if (!StartActionAsync(FireAction, InvalidReason))
	{
		// No one to re-ask on an Execution turn — don't strand it.
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] Execution fire refused (%s) — advancing"),
			   InvalidReason.IsEmpty() ? TEXT("guard") : *InvalidReason);
		TurnManagerRef->AdvanceToNextTurn();
	}
}

void ACombatOrchestrator::HandleCombatEnded(int32 FinalTurnCount)
{
	// TurnManager ended combat (could be from no valid combatants)
	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] TurnManager ended combat at turn %d"), FinalTurnCount);

	if (CombatState == ECombatState::InProgress)
	{
		// Determine winner if we haven't already
		ECombatState WinState = CheckWinCondition();
		if (WinState == ECombatState::InProgress)
		{
			WinState = ECombatState::Draw; // No one alive = draw
		}

		CombatOwnedGearSync::WriteActiveLoadoutsToOwned(LocalPartyCombatants);

		SetCombatState(WinState);

		FCombatResult Result = BuildCombatResult();
		OnCombatResultReady.Broadcast(Result);

		// §7 C3 — fire the draft-ready hook WIN-ONLY (the deferred 5-pick-3 UI binds it). No reward on
		// a loss/draw. Nothing consumes it yet; the pool persists until the next StartCombat reset.
		if (WinState == ECombatState::Victory && PendingWorldStatPool > 0)
		{
			OnWorldStatDraftReady.Broadcast(PendingWorldStatPool);
		}

		// BEFORE the team arrays empty — it walks them for per-actor unbinds.
		ReleaseCombatResources();

		LocalPartyCombatants.Empty();
		OpposingPartyCombatants.Empty();
		CurrentActor = nullptr;
		DeferredPayloads.Empty(); // W-B: no armed ritual leaks across combats

		SetCombatState(ECombatState::Idle);
	}
}

// ========================================
// WORLD STATS (§7 C3 — earn pool)
// ========================================

void ACombatOrchestrator::OnCombatantDied(AActor *Victim)
{
	if (!Victim)
	{
		return;
	}
	if (LocalPartyCombatants.Contains(Victim))
	{
		TakeOverIfNoPlayerMemberLeft();
	}
	// Only ENEMY (OpposingParty) deaths feed the pool — a player-side (LocalParty) death grants nothing.
	if (!OpposingPartyCombatants.Contains(Victim))
	{
		return;
	}
	const UCharacterDataComponent *VictimComp = Victim->FindComponentByClass<UCharacterDataComponent>();
	const UCharacterData *VictimData = VictimComp ? VictimComp->CharacterData : nullptr;
	if (!VictimData)
	{
		return;
	}
	// Caliber = the dead enemy's total stat budget; map to WSP via the tuning divisor (>= 1 per kill).
	const int32 Caliber = VictimData->GetTotalPool();
	const int32 WSP = FMath::Max(1, Caliber / CombatConstants::WORLDSTAT_KILL_DIVISOR);
	PendingWorldStatPool += WSP;

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Enemy %s down (caliber %d) -> +%d WSP (pool now %d)"),
		   *Victim->GetName(), Caliber, WSP, PendingWorldStatPool);
}

ECombatRow ACombatOrchestrator::GetPlaceholderEnemyStartRow(const AActor *Enemy)
{
	const UCharacterDataComponent *Comp = Enemy ? Enemy->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	const bool bCaster = Comp && Comp->CharacterData && Comp->CharacterData->CharacterClass == ECharacterClass::Caster;
	return bCaster ? ECombatRow::Back : ECombatRow::Front;
}

void ACombatOrchestrator::TakeOverIfNoPlayerMemberLeft()
{
	TArray<AActor *> LivingAIMembers;
	for (AActor *Member : LocalPartyCombatants)
	{
		const UCharacterDataComponent *Comp = IsValid(Member) ? Member->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (!Comp || !Comp->bIsAlive)
		{
			continue;
		}
		if (!IsActorAIControlled(Member))
		{
			return; // a member you play is still standing
		}
		LivingAIMembers.Add(Member);
	}
	if (LivingAIMembers.Num() == 0)
	{
		return;
	}

	const UGameInstance *GI = GetGameInstance();
	const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	if (PartySession && PartySession->GetLetAIFinish())
	{
		UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Your fighters are down - the AI finishes the fight (Let AI finish)"));
		return;
	}
	for (AActor *Member : LivingAIMembers)
	{
		CombatControl::SetPlayerDriven(Member, true);
		UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Your fighters are down - you take over %s (D-045)"), *Member->GetName());
	}
}

FText ACombatOrchestrator::GetControlSwitchRefusedReason(AActor *Actor) const
{
	if (CombatState != ECombatState::InProgress)
	{
		return NSLOCTEXT("CombatOrchestrator", "SwitchNoFight", "There is no fight.");
	}
	if (!Actor || !LocalPartyCombatants.Contains(Actor))
	{
		return NSLOCTEXT("CombatOrchestrator", "SwitchNotParty", "Only your party members can be switched.");
	}
	if (Actor != CurrentActor || bWaitingForAsyncAction)
	{
		return NSLOCTEXT("CombatOrchestrator", "SwitchNotTurn", "Switch a member on its own turn.");
	}
	const APawn *Pawn = Cast<APawn>(Actor);
	if (Pawn && Pawn->IsPlayerControlled())
	{
		return NSLOCTEXT("CombatOrchestrator", "SwitchLeader", "The leader is always yours.");
	}
	if (SkillEffectManagerRef && SkillEffectManagerRef->HasEffectOfType(Actor, ESkillEffectType::RandomSkill))
	{
		return NSLOCTEXT("CombatOrchestrator", "SwitchVoidBroken", "A Void break is playing this turn.");
	}
	return FText::GetEmpty();
}

bool ACombatOrchestrator::SwitchMemberControl(AActor *Actor)
{
	const FText Refused = GetControlSwitchRefusedReason(Actor);
	if (!Refused.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] Control switch refused for %s: %s"), *GetNameSafe(Actor), *Refused.ToString());
		return false;
	}

	const bool bToPlayer = IsActorAIControlled(Actor);
	if (bToPlayer && AIDecisionManagerRef)
	{
		AIDecisionManagerRef->CancelPendingDecision();
	}
	else if (!bToPlayer)
	{
		// The AI takes this turn: the player's open menu must not submit a second action.
		const UGameInstance *GI = GetGameInstance();
		if (UCombatCommandMenuSubsystem *Menu = GI ? GI->GetSubsystem<UCombatCommandMenuSubsystem>() : nullptr)
		{
			if (Menu->IsOpen())
			{
				Menu->Close();
			}
		}
	}
	CombatControl::SetPlayerDriven(Actor, bToPlayer);
	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] %s is now played by %s (D-026)"), *Actor->GetName(),
		   bToPlayer ? TEXT("you") : TEXT("the AI"));
	GetWorld()->GetTimerManager().ClearTimer(AutoAdvanceTimerHandle);
	RequestActionFromActor(Actor);
	return true;
}

// ========================================
// INTERNAL METHODS
// ========================================

void ACombatOrchestrator::SetCombatState(ECombatState NewState)
{
	if (CombatState != NewState)
	{
		ECombatState OldState = CombatState;
		CombatState = NewState;

		UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] State: %d -> %d"), (int32)OldState, (int32)NewState);

		OnCombatStateChanged.Broadcast(NewState);
	}
}

void ACombatOrchestrator::BindTurnManagerEvents()
{
	if (TurnManagerRef)
	{
		TurnManagerRef->OnTurnStarted.AddDynamic(this, &ACombatOrchestrator::HandleTurnStarted);
		TurnManagerRef->OnCombatEnded.AddDynamic(this, &ACombatOrchestrator::HandleCombatEnded);
	}
}

void ACombatOrchestrator::UnbindTurnManagerEvents()
{
	if (TurnManagerRef)
	{
		TurnManagerRef->OnTurnStarted.RemoveDynamic(this, &ACombatOrchestrator::HandleTurnStarted);
		TurnManagerRef->OnCombatEnded.RemoveDynamic(this, &ACombatOrchestrator::HandleCombatEnded);
	}
}

void ACombatOrchestrator::ProcessStartOfTurnEffects(AActor *Actor)
{
	if (!SkillEffectManagerRef)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] SkillEffectManager not available for start-of-turn processing"));
		return;
	}

	// Delegate to SkillEffectManager - processes buffs, regen, etc.
	SkillEffectManagerRef->ProcessStartOfTurnEffects(Actor);

	// Process status bar decay (buildup lives in its own subsystem post-split).
	if (UStatusBuildupManager *BuildupManager = GetGameInstance() ? GetGameInstance()->GetSubsystem<UStatusBuildupManager>() : nullptr)
	{
		BuildupManager->ProcessStatusBarDecay(Actor);
	}

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Processed start-of-turn effects for %s"),
		   *Actor->GetName());
}

void ACombatOrchestrator::ProcessEndOfTurnEffects(AActor *Actor)
{
	if (!SkillEffectManagerRef)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] SkillEffectManager not available for end-of-turn processing"));
		return;
	}

	// Delegate to SkillEffectManager - processes DOTs, ticks durations, expires effects
	SkillEffectManagerRef->ProcessEndOfTurnEffects(Actor);

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Processed end-of-turn effects for %s"),
		   *Actor->GetName());
}

void ACombatOrchestrator::RequestActionFromActor(AActor *Actor)
{
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] RequestActionFromActor called with null actor"));
		return;
	}

	// === AI-CONTROLLED ACTOR, or a Void break playing the turn for you (B-199) ===
	const bool bVoidBroken = SkillEffectManagerRef && SkillEffectManagerRef->HasEffectOfType(Actor, ESkillEffectType::RandomSkill);
	if (IsActorAIControlled(Actor) || bVoidBroken)
	{
		if (!AIDecisionManagerRef)
		{
			UE_LOG(LogTemp, Error, TEXT("[CombatOrchestrator] %s is AI-controlled but no AIDecisionManager - skipping turn"),
				   *Actor->GetName());
			// Don't broadcast player UI events for an AI actor with no AI brain.
			// Don't auto-advance either; this is a configuration error worth seeing.
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Routing %s to AI decision"), *Actor->GetName());
		GetWorld()->GetTimerManager().ClearTimer(AutoAdvanceTimerHandle);
		AIDecisionManagerRef->RequestDecision(Actor);
		return;
	}

	// === PLAYER-CONTROLLED ACTOR ===
	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Broadcasting OnActionRequested for player actor: %s"),
		   *Actor->GetName());
	OnActionRequested.Broadcast(Actor);

	// Auto-advance is a debug fallback for when no UI is wired.
	// Once the menu is bound to OnActionRequested, this should be off.
	if (bAutoAdvanceTurns)
	{
		GetWorld()->GetTimerManager().SetTimer(
			AutoAdvanceTimerHandle,
			this,
			&ACombatOrchestrator::OnActionCompleted,
			AutoAdvanceDelay,
			false);

		UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Auto-advancing in %.1fs..."), AutoAdvanceDelay);
	}
}

ECombatState ACombatOrchestrator::CheckWinCondition()
{
	int32 LocalPartyAlive = CountLivingMembers(LocalPartyCombatants);
	int32 OpposingPartyAlive = CountLivingMembers(OpposingPartyCombatants);

	if (LocalPartyAlive == 0 && OpposingPartyAlive == 0)
	{
		return ECombatState::Draw;
	}
	else if (OpposingPartyAlive == 0)
	{
		return ECombatState::Victory; // LocalParty wins
	}
	else if (LocalPartyAlive == 0)
	{
		return ECombatState::Defeat; // OpposingParty wins
	}

	return ECombatState::InProgress;
}

int32 ACombatOrchestrator::CountLivingMembers(const TArray<AActor *> &Team)
{
	int32 Count = 0;
	for (AActor *Actor : Team)
	{
		if (IsActorAlive(Actor))
		{
			Count++;
		}
	}
	return Count;
}

bool ACombatOrchestrator::IsActorAlive(AActor *Actor) const
{
	if (!Actor)
		return false;

	UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
	if (!CharComp)
		return false;

	return CharComp->bIsAlive;
}

int32 ACombatOrchestrator::GetActorTeamIndex(AActor *Actor) const
{
	if (LocalPartyCombatants.Contains(Actor))
		return 0;
	if (OpposingPartyCombatants.Contains(Actor))
		return 1;
	return -1;
}

TArray<AActor *> ACombatOrchestrator::GetEnemyTeam(AActor *Actor) const
{
	int32 TeamIndex = GetActorTeamIndex(Actor);
	if (TeamIndex == 0)
		return OpposingPartyCombatants;
	if (TeamIndex == 1)
		return LocalPartyCombatants;
	return TArray<AActor *>();
}

FCombatResult ACombatOrchestrator::BuildCombatResult()
{
	FCombatResult Result;
	Result.FinalState = CombatState;
	Result.TotalTurns = CurrentTurnNumber;
	Result.LocalPartySurvivors = CountLivingMembers(LocalPartyCombatants);
	Result.OpposingPartySurvivors = CountLivingMembers(OpposingPartyCombatants);

	// Find last standing actor (for victory screen)
	if (Result.LocalPartySurvivors > 0)
	{
		for (AActor *Actor : LocalPartyCombatants)
		{
			if (IsActorAlive(Actor))
			{
				Result.LastActorStanding = Actor;
				break;
			}
		}
	}
	else if (Result.OpposingPartySurvivors > 0)
	{
		for (AActor *Actor : OpposingPartyCombatants)
		{
			if (IsActorAlive(Actor))
			{
				Result.LastActorStanding = Actor;
				break;
			}
		}
	}

	return Result;
}

void ACombatOrchestrator::PrepareAllLoadoutsForBattle()
{
	auto PrepareActor = [this](AActor *Actor)
	{
		if (!Actor)
			return;

		UInventoryComponent *Inventory = Actor->FindComponentByClass<UInventoryComponent>();
		ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>();

		if (Loadout && Inventory)
		{
			if (Loadout->PrepareForBattle(Inventory))
			{
				UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Prepared loadout for %s"), *Actor->GetName());

				// Apply STARTING effects (non-conditional gear effects, once per combat).
				// GetActiveEffects covers all contributing gear — weapon/ring per class
				// plus the innate (primary-slot) evolution; ATTACHED evolutions
				// deliberately contribute no effects.
				if (SkillEffectManagerRef)
				{
					// Arm gear conditional effects — read by the OnDefenseResolved handler (C3b)
						// to fire on matching impact outcomes; inert until then. Armed first so
						// an actor with only conditional gear (no starting effects) still arms.
						SkillEffectManagerRef->ArmConditionalEffects(Actor, Loadout->GetActiveConditionalEffectsGathered(Actor));

						TArray<FGatheredEffect> StartingEffects = Loadout->GetActiveEffectsGathered(Actor);
					if (StartingEffects.Num() > 0)
					{
						// Def-identity packing: each gathered effect carries its def id + bundle index,
							// so EffectIDs window per-definition (same def from any gear merges).
						SkillEffectManagerRef->ApplyEquipmentEffects(Actor, StartingEffects);
						UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Applied %d starting effects to %s"),
							   StartingEffects.Num(), *Actor->GetName());
					}
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] Failed to prepare loadout for %s"), *Actor->GetName());
			}
		}
	};

	for (AActor *Actor : LocalPartyCombatants)
	{
		PrepareActor(Actor);
	}

	for (AActor *Actor : OpposingPartyCombatants)
	{
		PrepareActor(Actor);
	}
}

// ========================================
// Enemy AI
// ========================================

bool ACombatOrchestrator::IsActorAIControlled(AActor *Actor) const
{
	if (!Actor)
	{
		return false;
	}

	// Runtime control, not asset identity. A Player-origin character can be bot-driven
	// (ghost / AI companion) and an Enemy-origin one player-driven (PvP). Possession or a
	// Player-controlled party member's per-fight flag answers it (B-071, CombatControl.h).
	const APawn *Pawn = Cast<APawn>(Actor);
	return Pawn && !CombatControl::IsPlayerDriven(Pawn);
}

TArray<AActor *> ACombatOrchestrator::GetLivingEnemies(AActor *ForActor) const
{
	TArray<AActor *> Enemies = GetEnemyTeam(ForActor);

	Enemies.RemoveAll([this](AActor *Actor)
					  { return !IsActorAlive(Actor); });

	return Enemies;
}

TArray<AActor *> ACombatOrchestrator::GetLivingAllies(AActor *ForActor) const
{
	TArray<AActor *> Allies;
	int32 TeamIndex = GetActorTeamIndex(ForActor);

	if (TeamIndex == 0)
	{
		Allies = LocalPartyCombatants;
	}
	else if (TeamIndex == 1)
	{
		Allies = OpposingPartyCombatants;
	}

	// Remove self and dead allies
	Allies.RemoveAll([this, ForActor](AActor *Actor)
					 { return Actor == ForActor || !IsActorAlive(Actor); });

	return Allies;
}

// ========================================
// BROKEN DARKNESS HELPERS
// ========================================

UBrokenDarknessManager *ACombatOrchestrator::GetBrokenDarknessManager(AActor *Actor) const
{
	if (!Actor)
	{
		return nullptr;
	}
	return Actor->FindComponentByClass<UBrokenDarknessManager>();
}

TArray<AActor *> ACombatOrchestrator::GetCombatantsInRange(AActor *Origin, float Range)
{
	TArray<AActor *> Result;

	if (!Origin || Range <= 0.0f)
	{
		return Result;
	}

	FVector OriginLocation = Origin->GetActorLocation();

	// Check all combatants from both teams
	for (AActor *Combatant : LocalPartyCombatants)
	{
		if (Combatant && Combatant != Origin && IsActorAlive(Combatant))
		{
			float Distance = FVector::Dist(OriginLocation, Combatant->GetActorLocation());
			if (Distance <= Range)
			{
				Result.Add(Combatant);
			}
		}
	}

	for (AActor *Combatant : OpposingPartyCombatants)
	{
		if (Combatant && Combatant != Origin && IsActorAlive(Combatant))
		{
			float Distance = FVector::Dist(OriginLocation, Combatant->GetActorLocation());
			if (Distance <= Range)
			{
				Result.Add(Combatant);
			}
		}
	}

	return Result;
}

void ACombatOrchestrator::ProcessBrokenDarknessOverflow(AActor *Actor)
{
	UBrokenDarknessManager *BDManager = GetBrokenDarknessManager(Actor);
	if (!BDManager || !BDManager->IsOverloaded())
	{
		return;
	}

	// Get aura range based on MaxEnergy stat
	float AuraRange = BDManager->CalculateAuraRange();

	// Find all combatants in range
	TArray<AActor *> ActorsInRange = GetCombatantsInRange(Actor, AuraRange);

	// Pre-compute the source-side scalars the overload tick needs. StatusMultiplier
	// folds in BOTH the base-stat layer (innate+equip+stone, shared getter) AND the
	// transient StatusMultiplierBuff/Debuff layer — so a transient buff burns BD faster /
	// a debuff slower, matching base-stat. The self-status pass skips BOTH (bSkipBaseStatAmp)
	// to avoid double-counting, since both are baked here.
	float StatusMultiplierBonus = 1.0f;
	float EfficiencyMult = 1.0f;

	UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
	if (CharComp && CharComp->CharacterData)
	{
		// Crystal-aware StatusMultiplier — the single composed getter (T3 consolidation): crystal-aware
		// Spirit stat capped ×1.5, gear (BonusStatusMultiplier + StatusStone) multiplicative beyond,
		// additive transient StatusMultiplierBuff/Debuff. The SAME value AddStatusBuildup and crystal-wear
		// read — lockstep by construction (GetSourceStatusMultiplierFactor was retired). Scales the BD
		// drain AND self-status.
		StatusMultiplierBonus = CharComp->GetEffectiveStatusMultiplier();

		// EfficiencyMultiplier — unified getter (innate crystal-aware Mind + equipment
		// BonusEfficiency + attached EfficiencyStone, one clamp). Byte-neutral vs the
		// canonical getter for characters with no BonusEfficiency and no stone; equipment
		// and the stone now reach the BD drain through it. Fraction-of-cost in
		// [1 - EFFICIENCY_MAX, 1.0]; lower = better efficiency = smaller leak.
		EfficiencyMult = CharComp->GetEffectiveEfficiencyMultiplier();
	}

	// Process the overflow tick: aura HP damage (SpellDamage-scaled), self HP
	// damage (same), and the coupled energy leak (released = BaseEnergyRelease
	// × StatusMultiplier × Efficiency → drains absorption + becomes self-status
	// in alignment element).
	BDManager->ProcessOverloadTick(ActorsInRange, StatusMultiplierBonus, EfficiencyMult);

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] BD Overflow processed for %s - Range: %.1f, Targets: %d"),
		   *Actor->GetName(), AuraRange, ActorsInRange.Num());
}

TArray<AActor *> UTurnManager::GetTeamMembers(int32 TeamIndex) const
{
	TArray<AActor *> TeamMembers;

	for (const FCombatantTurnDebt &Combatant : Combatants)
	{
		if (Combatant.TeamIndex == TeamIndex && Combatant.Actor)
		{
			TeamMembers.Add(Combatant.Actor);
		}
	}

	return TeamMembers;
}

int32 UTurnManager::GetActorTeam(AActor *Actor) const
{
	if (!Actor)
		return -1;

	for (const FCombatantTurnDebt &Combatant : Combatants)
	{
		if (Combatant.Actor == Actor)
		{
			return Combatant.TeamIndex;
		}
	}

	return -1; // Not found
}

// ============================================================
// CRYSTAL DURABILITY — between-combat auto-repair
// ============================================================

// ============================================================
// CRYSTAL DURABILITY — between-combat destruction (Phase B)
// ============================================================

void ACombatOrchestrator::ApplyBetweenCombatCrystalDestruction()
{
	int32 CrystalsDestroyed = 0;
	for (AActor *Actor : LocalPartyCombatants)
	{
		CrystalsDestroyed += CrystalBreak::DestroyBrokenOn(Actor);
	}
	for (AActor *Actor : OpposingPartyCombatants)
	{
		CrystalsDestroyed += CrystalBreak::DestroyBrokenOn(Actor);
	}

	if (CrystalsDestroyed > 0)
	{
		UE_LOG(LogTemp, Log,
			   TEXT("[CombatOrchestrator] Combat end: destroyed %d broken crystal(s)"),
			   CrystalsDestroyed);
	}
}

void ACombatOrchestrator::ApplyBetweenCombatRepair()
{
	const int32 RepairAmount = DurabilityConstants::REPAIR_PER_BATTLE;
	int32 CrystalsRepaired = 0;
	for (const TArray<AActor *> *Team : {&LocalPartyCombatants, &OpposingPartyCombatants})
	{
		for (AActor *Actor : *Team)
		{
			CrystalsRepaired += CrystalBreak::RepairOn(Actor, RepairAmount);
		}
	}

	if (CrystalsRepaired > 0)
	{
		UE_LOG(LogTemp, Log,
			   TEXT("[CombatOrchestrator] Between-combat repair: +%d to %d item(s) (gems and evolutions)"),
			   RepairAmount, CrystalsRepaired);
	}
}

void ACombatOrchestrator::ConsumeAllUsedItems()
{
	auto ConsumeForActor = [](AActor *Actor)
	{
		if (!Actor)
			return;

		ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>();

		if (Loadout)
		{
			// Transfer model: no end-of-combat consume step — the crystal
			// inventory was debited at equip time and item-slot Quantity is the
			// persistent source of truth. Only reset the battle-ready flag.
			Loadout->ResetBattleState();
			UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Reset loadout battle state for %s"), *Actor->GetName());
		}
	};

	for (AActor *Actor : LocalPartyCombatants)
	{
		ConsumeForActor(Actor);
	}

	for (AActor *Actor : OpposingPartyCombatants)
	{
		ConsumeForActor(Actor);
	}
}
