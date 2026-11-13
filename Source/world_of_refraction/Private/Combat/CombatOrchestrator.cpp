// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/CombatOrchestrator.h"
#include "Combat/Tier/CombatTier.h"
#include "GameFramework/Pawn.h"
#include "Combat/TurnManager.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/ESkillEffectType.h"
#include "Skills/Effects/StatusBuildupManager.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/Actions/ActionPipsRules.h"
#include "Combat/Actions/ActionPipsText.h"
#include "Combat/TurnRotation.h"
#include "GAS/Abilities/WoRSkillAbilities.h"
#include "Combat/Defense/DefenseSystem.h"
#include "Infusion/InfusionChargeManager.h"
#include "Character/CharacterDataComponent.h"
#include "Character/Forms/CharacterFormSubsystem.h"
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
#include "Pool/PoolSubsystem.h"
#include "Trial/TrialGate.h"
#include "Trial/TrialRunSubsystem.h"
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
#include "Player/RunStateComponent.h" // B-232: the seat rotation
#include "Party/PartySessionSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Combat/Mechanics/TurnStartEnergy.h"
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
		TurnManagerRef = WoRFight::Get<UTurnManager>(GI);
		SkillEffectManagerRef = WoRFight::Get<USkillEffectManager>(GI);
		ActionExecutorRef = WoRFight::Get<UActionExecutor>(GI);
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

	AIDecisionManagerRef = WoRFight::Get<UAIDecisionManager>(GI); // B-067: GI may be null

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
		if (UCombatCommandMenuSubsystem *Menu = WoRFight::Get<UCombatCommandMenuSubsystem>(GI))
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
	// B-229: only a controller with a local player can own widgets. A headless world's controller (a spec) and a
	// remote client's controller on the server have none, and building the HUD for them crashed the test editor.
	if (!PC || !PC->GetLocalPlayer())
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

	// B-232 (Q-21-audit): the seat rotation starts over each fight.
	if (URunStateComponent *Run = URunStateComponent::Find(this); Run && HasAuthority())
	{
		Run->ResetRotation();
	}

	// Register with AI manager
	if (AIDecisionManagerRef)
	{
		AIDecisionManagerRef->SetCombatOrchestrator(this);
	}

	// Register with combat command menu
	if (UGameInstance *GI = GetGameInstance())
	{
		if (UCombatCommandMenuSubsystem *Menu = WoRFight::Get<UCombatCommandMenuSubsystem>(GI))
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
	PendingLevelDropSources.Reset();
	for (const TArray<AActor *> *Team : {&LocalPartyCombatants, &OpposingPartyCombatants})
	{
		for (AActor *Actor : *Team)
		{
			if (UCharacterDataComponent *CharComp = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr)
			{
				CharComp->ClearWaitingActionChanges(); // B-170d (D-064): per fight, whatever the switch
				CharComp->OnDied.AddUniqueDynamic(this, &ACombatOrchestrator::OnCombatantDied);
			}
		}
	}

	CurrentTurnNumber = 0;
	CurrentActor = nullptr;
	bWaitingForAsyncAction = false;
	bHoldingForBossOpening = false; // B-406
	bHoldingForBossCinematic = false;
	bTurnContinuesAfterAction = false;
	ActorsPastFirstTurn.Reset();
	TurnSpends.Reset();

	SetCombatState(ECombatState::Initializing);

	// D2: clear per-match state (fires-once set) before any combat-start effect applies.
	if (SkillEffectManagerRef)
	{
		SkillEffectManagerRef->ResetForNewCombat();
	}

	// Prepare loadouts for battle
	PrepareAllLoadoutsForBattle();

	// B-043b: each combatant's loadout skills as granted abilities (no play change; used at wor.Abilities.UseGAS 1).
	for (const TArray<AActor *> *Team : {&LocalPartyCombatants, &OpposingPartyCombatants})
	{
		for (AActor *Actor : *Team)
		{
			WoRSkillAbilities::GrantLoadoutSkills(Actor);
		}
	}

	// Assign grid positions and place actors
	if (UCombatGridSubsystem *Grid = WoRFight::Get<UCombatGridSubsystem>(this))
	{
		FVector ArenaCenter = GetActorLocation();

		// D-042 as changed by D-134 (B-342): members start in their chosen ROW (three rows, no columns),
		// enemies in their placeholder row.
		const UPartySessionSubsystem *PartySession = GetGameInstance()->GetSubsystem<UPartySessionSubsystem>();
		TArray<FGridStartPreference> LocalPreferences;
		for (AActor *Member : LocalPartyCombatants)
		{
			FGridStartPreference &Preference = LocalPreferences.AddDefaulted_GetRef();
			const UCharacterDataComponent *Comp = Member ? Member->FindComponentByClass<UCharacterDataComponent>() : nullptr;
			const int32 Slot = PartySession && Comp ? PartySession->GetMemberSlotOf(Comp) : INDEX_NONE; // B-360b: a copy's own cell
			ECombatRow Row;
			int32 Column;
			if (Slot != INDEX_NONE && PartySession->GetMemberFormation(Slot, Row, Column))
			{
				Preference = FGridStartPreference::RowOnly(Row); // the column is OBSOLETE (B-342)
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

		UE_LOG(LogTemp, Verbose, TEXT("[CombatOrchestrator] Grid positions assigned and actors placed"));
	}

	// Initialise weather leaders
	if (UWeatherStateManager *WeatherManager = WoRFight::Get<UWeatherStateManager>(this))
	{
		WeatherManager->InitialiseLeaders(LocalPartyCombatants, OpposingPartyCombatants);
	}

	// Set arena center for ActionExecutor movement calculations
	if (UActionExecutor *Executor = WoRFight::Get<UActionExecutor>(this))
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
		CameraDirector->OnBossCinematicFinished.AddUniqueDynamic(this, &ACombatOrchestrator::HandleBossCinematicFinished); // B-406
		CameraDirector->OnCinematicHudHidden.AddUniqueDynamic(this, &ACombatOrchestrator::HandleCinematicHudHidden);
		CameraDirector->BeginCombat(LocalParty, OpposingParty, ActionExecutorRef);
	}

	// B-170c (Q-170): the fight-start bank, set before turn 1 is dispatched inside InitializeCombat. The switch is
	// read here as InitializeCombat latches it a moment later (the same call), so the two never disagree.
	if (TurnRotation::IsActionPipsOn())
	{
		for (const TArray<AActor *> *Team : {&LocalPartyCombatants, &OpposingPartyCombatants})
		{
			for (AActor *Actor : *Team)
			{
				if (UCharacterDataComponent *CharComp = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr)
				{
					CharComp->ResetActionsForFight();
				}
			}
		}
	}

	// Initialize TurnManager (this will trigger first OnTurnStarted)
	const int32 OpeningTeam = FightFirstStrike == EFirstStrike::Party   ? TurnManagerTeams::LOCAL_TEAM_INDEX
							: FightFirstStrike == EFirstStrike::Enemy ? TurnManagerTeams::OPPOSING_TEAM_INDEX
																 : INDEX_NONE;
	// B-406 (FeelGuide 9, on trial): a boss's opening cinematic plays first; turn 1 waits for it (nothing is
	// submitted meanwhile: the async-action wait). No boss, no sequence or no viewer: turn 1 starts now.
	// The hold is set before the play: a sequence that finishes inside Play() still finds it (review fix).
	PendingOpeningTeam = OpeningTeam;
	const bool bWasWaitingForAsyncAction = bWaitingForAsyncAction;
	bHoldingForBossOpening = true;
	bWaitingForAsyncAction = true;
	if (CameraDirector && CameraDirector->PlayBossOpening())
	{
		if (bHoldingForBossOpening)
		{
			UE_LOG(LogWoRCombat, Log, TEXT("[CombatOrchestrator] Turn 1 waits for the boss's opening cinematic"));
		}
		return;
	}
	bHoldingForBossOpening = false;
	bWaitingForAsyncAction = bWasWaitingForAsyncAction;
	BeginFirstTurn(OpeningTeam);
}

void ACombatOrchestrator::BeginFirstTurn(int32 OpeningTeam)
{
	TurnManagerRef->InitializeCombat(LocalPartyCombatants, OpposingPartyCombatants, OpeningTeam);

	// Turn 1 can resolve the whole combat inside InitializeCombat (state back to Idle,
	// teams emptied) — don't build UI for a fight that's already over.
	if (CombatState == ECombatState::InProgress)
	{
		OnCombatStartedUI(LocalPartyCombatants, OpposingPartyCombatants);
	}
}

void ACombatOrchestrator::HandleBossCinematicFinished(EBossCinematicKind Kind)
{
	const bool bOpening = bHoldingForBossOpening;
	const bool bHeldRequest = bHoldingForBossCinematic;
	bHoldingForBossOpening = false;
	bHoldingForBossCinematic = false;
	if (CombatState != ECombatState::InProgress || (!bOpening && !bHeldRequest))
	{
		return; // a preview, or a cinematic that ended while an action was still running: nothing was held
	}
	bWaitingForAsyncAction = false;
	const bool bHeldRitualFire = !bOpening && TurnManagerRef && TurnManagerRef->GetCurrentTurnType() == ETurnType::Execution;
	UE_LOG(LogWoRCombat, Log, TEXT("[CombatOrchestrator] %s cinematic ended: %s"), *UEnum::GetValueAsString(Kind),
		   bOpening ? TEXT("turn 1 starts") : bHeldRitualFire ? TEXT("the held ritual fires") : TEXT("the held action request goes out"));
	if (bOpening)
	{
		BeginFirstTurn(PendingOpeningTeam);
	}
	else if (bHeldRitualFire)
	{
		FireScheduledExecution(); // B-406b: the Execution turn held under the cinematic
	}
	else if (CurrentActor)
	{
		RequestActionFromActor(CurrentActor);
	}
}

void ACombatOrchestrator::HandleCinematicHudHidden(bool bHudHidden)
{
	if (CombatHUD)
	{
		CombatHUD->SetHiddenForCinematic(bHudHidden);
	}
}

void ACombatOrchestrator::ForceEndCombat(ECombatState ForcedState)
{
	if (CombatState == ECombatState::Idle)
		return;

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Force ending combat with state %d"), (int32)ForcedState);

	// Forced end: drop the combat HUD now. The results screen (BattleGameMode) is
	// its own widget; win paths leave the HUD up and EndPlay removes it.
	TeardownCombatHUD();

	FinishCombat(ForcedState, /*bTurnManagerEnded=*/false);
}

void ACombatOrchestrator::FinishCombat(ECombatState Result, bool bTurnManagerEnded)
{
	// B-246 (DRY audit A7, Q-246): every way a fight ends runs these steps. Unbind first so
	// TurnManager::EndCombat below can't re-enter HandleCombatEnded.
	UnbindTurnManagerEvents();

	// Clear all status effects from combat. A forced end (flee, abort) keeps them until
	// ReleaseCombatResources, after the result is built, so the banked HP / EP isn't moved by a
	// max-HP buff falling off (as before B-246).
	if (SkillEffectManagerRef && bTurnManagerEnded)
	{
		SkillEffectManagerRef->ClearAllEffects();
	}

	// A finished fight, won or lost (ER-35): destroy the crystals that broke, BEFORE repair so they
	// aren't candidates, then the between-fight repair. A flee or draw keeps its wear (Q-115b); an
	// aborted fight (Idle) is not a battle.
	if (Result == ECombatState::Victory || Result == ECombatState::Defeat)
	{
		ApplyBetweenCombatCrystalDestruction();
		// B-096 (Q-096): the run-end payout reads the passives the party wore in this fight. Before the repair, which
		// lifts a kept broken evolution off 0 durability: one that broke in this fight gives nothing.
		UTrialRunSubsystem *TrialRun = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTrialRunSubsystem>() : nullptr;
		if (TrialRun && HasAuthority())
		{
			TrialRun->RefreshPayoutMultiplier(LocalPartyCombatants);
		}
		ApplyBetweenCombatRepair();
	}

	CombatOwnedGearSync::WriteActiveLoadoutsToOwned(LocalPartyCombatants);

	// B-279: a won fight fills the character-form meters from what the party wore.
	if (Result == ECombatState::Victory)
	{
		if (UCharacterFormSubsystem *Forms = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCharacterFormSubsystem>() : nullptr)
		{
			Forms->RecordFightWon(LocalPartyCombatants);
		}
	}

	if (!bTurnManagerEnded && TurnManagerRef)
	{
		TurnManagerRef->EndCombat();
	}

	SetCombatState(Result);

	FCombatResult CombatResult = BuildCombatResult();
	OnCombatResultReady.Broadcast(CombatResult);

	// §7 C3 — the draft-ready hook WIN-ONLY (the deferred 5-pick-3 UI binds it). No reward on a
	// loss/draw. Nothing consumes it yet; the pool persists until the next StartCombat reset.
	if (Result == ECombatState::Victory && PendingWorldStatPool > 0)
	{
		OnWorldStatDraftReady.Broadcast(PendingWorldStatPool);
	}
	// B-223 / B-223b (D-102; Q-223 changed by Crown 2026-11-03): after a win each party member rolls its own level
	// drops from every defeated enemy, and every mini-boss or boss gives each member its levels. They wait in the run
	// as that character's to place on its own pillars (wor.UI.LevelDrop). A fallen member rolls too (Q-223b). Only the
	// host's party rolls here; a guest's characters get theirs with B-235's personal rewards (co-op, not built).
	int32 LevelDrops = 0;
	UTrialRunSubsystem *LevelDropRun = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	if (Result == ECombatState::Victory && LevelDropRun && LevelDropRun->IsRunActive())
	{
		for (const AActor *Member : LocalPartyCombatants)
		{
			const UCharacterDataComponent *MemberData = Member ? Member->FindComponentByClass<UCharacterDataComponent>() : nullptr;
			// B-360c (Q-360c): by the member's own key, so a later copy of a character banks and places its own levels.
			const FName MemberKey = UPoolSubsystem::GetCharacterKey(MemberData);
			// Only rostered members: a guest or summon has no level-drop screen row to spend them from.
			if (MemberKey.IsNone() || !MemberData->FindCarriedPartyMember())
			{
				continue;
			}
			int32 MemberDrops = 0;
			for (const TPair<EItemTier, EEncounterRank> &Source : PendingLevelDropSources)
			{
				MemberDrops += TrialGate::RollLevelDrops(this, Source.Key, Source.Value);
			}
			LevelDropRun->AddUnspentLevels(MemberKey, MemberDrops);
			LevelDrops += MemberDrops;
		}
	}
	PendingLevelDropSources.Reset();
	if (LevelDrops > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] The win dropped %d pillar level(s) across the party"), LevelDrops);
		OnLevelDropsReady.Broadcast(LevelDrops);
	}

	// B-170d (D-064): no waiting action change outlives its fight, whatever the switch.
	for (const TArray<AActor *> *Team : {&LocalPartyCombatants, &OpposingPartyCombatants})
	{
		for (AActor *Actor : *Team)
		{
			if (UCharacterDataComponent *CharComp = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr)
			{
				CharComp->ClearWaitingActionChanges();
			}
		}
	}

	// BEFORE the team arrays empty — it walks them for per-actor unbinds.
	ReleaseCombatResources();

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
		World->GetTimerManager().ClearTimer(KillShotHoldTimer); // B-405: a forced end during the hold
	}
	LastEnemyDown.Reset();

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

	// B-043b: the skill abilities go before the executor's reset: a running one is cancelled (its action's
	// result is dropped with the fight), then cleared.
	for (const TArray<AActor *> *Team : {&LocalPartyCombatants, &OpposingPartyCombatants})
	{
		for (AActor *Actor : *Team)
		{
			if (IsValid(Actor))
			{
				WoRSkillAbilities::ClearSkills(Actor);
			}
		}
	}

	// B-030: nothing from this fight survives into the next. The executor goes first: it drops the
	// in-flight action's defense windows unresolved, then the defense system forgets them.
	if (ActionExecutorRef)
	{
		ActionExecutorRef->ResetForCombat();
	}
	if (UDefenseSystem *Defense = WoRFight::Get<UDefenseSystem>(GI))
	{
		Defense->ResetForCombat();
	}
	if (UInfusionChargeManager *Infusion = WoRFight::Get<UInfusionChargeManager>(GI))
	{
		Infusion->ResetForCombat();
	}

	if (UWeatherStateManager *WeatherManager = WoRFight::Get<UWeatherStateManager>(GI))
	{
		WeatherManager->EndCombat();
	}

	if (SkillEffectManagerRef)
	{
		SkillEffectManagerRef->ClearAllEffects();
		SkillEffectManagerRef->ResetForNewCombat();
	}

	UStatusBuildupManager *BuildupManager = WoRFight::Get<UStatusBuildupManager>(GI);
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

	if (UCombatGridSubsystem *Grid = WoRFight::Get<UCombatGridSubsystem>(GI))
	{
		Grid->ClearAllPositions();
	}

	if (AIDecisionManagerRef)
	{
		AIDecisionManagerRef->ClearCombatOrchestrator();
	}
	if (UCombatCommandMenuSubsystem *Menu = WoRFight::Get<UCombatCommandMenuSubsystem>(GI))
	{
		Menu->ClearCombatOrchestrator();
	}

	UE_LOG(LogTemp, Verbose, TEXT("[CombatOrchestrator] Combat resources released"));
}

// ========================================
// ACTION SUBMISSION
// ========================================

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

	if (Action.ActionType == EActionType::EndTurn)
	{
		if (!IsActionPipsFight()) // B-170e review: the turn is asked for again
		{
			OutInvalidReason = TEXT("End Turn refused: not an action-pips fight");
			UE_LOG(LogWoRCombat, Warning, TEXT("[CombatOrchestrator] %s"), *OutInvalidReason);
			return false;
		}
		EndTurnByChoice();
		return true;
	}

	GetWorld()->GetTimerManager().ClearTimer(AutoAdvanceTimerHandle);

	const FActionValidationResult Validation = ActionExecutorRef->ValidateAction(CurrentActor, Action);
	if (!Validation.bIsValid)
	{
		OutInvalidReason = Validation.ErrorMessage.IsEmpty() ? FString(TEXT("invalid action")) : Validation.ErrorMessage;
		return false;
	}

	// Every refusal comes before the payment, so a refused action spends and records nothing.
	if (Action.ActionType == EActionType::Flee)
	{
		const FText FleeRefused = GetFleeRefusedReason(CurrentActor);
		if (!FleeRefused.IsEmpty())
		{
			OutInvalidReason = FleeRefused.ToString();
			return false;
		}
	}

	if (!PayActionCost(Action, OutInvalidReason))
	{
		return false;
	}

	if (Action.ActionType == EActionType::Flee)
	{
		bWaitingForAsyncAction = true; // nothing else may start while the fight ends
		BeginFlee();
		return true;
	}

	bWaitingForAsyncAction = true;

	UE_LOG(LogTemp, Verbose, TEXT("[CombatOrchestrator] %s executing %s asynchronously..."),
		   *CurrentActor->GetName(), *Action.GetActionName());

	// B-043b: through the skill's ability at wor.Abilities.UseGAS 1 (the same executor call), else directly.
	const FOnActionComplete OnComplete = FOnActionComplete::CreateUObject(this, &ACombatOrchestrator::HandleAsyncActionCompleted);
	if (!WoRSkillAbilities::TryExecuteThroughAbility(CurrentActor, Action, OnComplete))
	{
		ActionExecutorRef->ExecuteActionAsync(CurrentActor, Action, OnComplete);
	}
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

	/** The row one step forward or back (D-096). B-342 (D-134): no columns, so the slot it lands in is any free one. */
	FCombatGridPosition OrchestratorStepTarget(const FCombatGridPosition &From, bool bForward)
	{
		FCombatGridPosition Target = From;
		Target.Row = CombatRowHelpers::GetRowFromIndex(From.GetRowIndex() + (bForward ? ORCHESTRATOR_ROW_STEP : -ORCHESTRATOR_ROW_STEP));
		return Target;
	}
}

FText ACombatOrchestrator::GetRowEdgeRefusal(ECombatRow Row, bool bForward)
{
	if (bForward && Row == ECombatRow::Front)
	{
		return NSLOCTEXT("CombatOrchestrator", "RowAtFront", "Already in the front row.");
	}
	if (!bForward && Row == ECombatRow::Back)
	{
		return NSLOCTEXT("CombatOrchestrator", "RowAtBack", "Already in the back row.");
	}
	return FText::GetEmpty();
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
	const UCombatGridSubsystem *Grid = WoRFight::Get<UCombatGridSubsystem>(this);
	FCombatGridPosition Position;
	if (!Grid || !Grid->GetActorPosition(Actor, Position))
	{
		return NSLOCTEXT("CombatOrchestrator", "RowNoGrid", "Not on the battle grid.");
	}
	if (const FText Edge = GetRowEdgeRefusal(Position.Row, bForward); !Edge.IsEmpty())
	{
		return Edge;
	}
	// B-342 (D-134): three rows, no columns. A row holds the whole side, so an ally there never blocks the
	// step; only a full row would (it can't be, at MAX_TEAM_SIZE). Replaces Q-180's empty-cell rule.
	FCombatGridPosition FreeSlot;
	if (!Grid->FindFreeSlotInRow(Position.TeamIndex, OrchestratorStepTarget(Position, bForward).Row, FreeSlot))
	{
		return NSLOCTEXT("CombatOrchestrator", "RowFull", "That row is full.");
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

	UCombatGridSubsystem *Grid = WoRFight::Get<UCombatGridSubsystem>(this);
	FCombatGridPosition Position;
	Grid->GetActorPosition(Actor, Position);
	const FCombatGridPosition Target = OrchestratorStepTarget(Position, bForward);

	// The target row has room (GetRowMoveRefusedReason); MoveActorToRow takes a free slot there (B-342).
	if (!Grid->MoveActorToRow(Actor, Target.Row, GetActorLocation()))
	{
		OutRefusedReason = NSLOCTEXT("CombatOrchestrator", "RowBlocked", "Can't move there.");
		return false;
	}
	bRowMoveUsedThisTurn = true;
	UE_LOG(LogTemp, Verbose, TEXT("[CombatOrchestrator] %s stepped to the %s row"), *Actor->GetName(),
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

	FActionValidationResult Result = ActionExecutorRef->ValidateAction(CurrentActor, Action);
	if (Result.bIsValid)
	{
		const FText CostRefusal = GetActionCostRefusedReason(Actor, Action); // B-170c: empty outside an action-pips fight
		if (!CostRefusal.IsEmpty())
		{
			return FActionValidationResult(false, CostRefusal.ToString(), Result.EnergyCost);
		}
	}
	return Result;
}

void ACombatOrchestrator::HandleAsyncActionCompleted(const FActionResult &Result)
{
	bWaitingForAsyncAction = false;

	UE_LOG(LogTemp, Verbose, TEXT("[CombatOrchestrator] Async action completed: %s (Damage: %d, Healing: %d)"),
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
		UE_LOG(LogTemp, Verbose, TEXT("[CombatOrchestrator] OnActionCompleted ignored - waiting for async action"));
		return;
	}

	if (CombatState != ECombatState::InProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] OnActionCompleted called but combat not in progress"));
		return;
	}

	// B-405: the fight is won and its kill shot is holding; the hold's end finishes it.
	if (IsHoldingKillShot())
	{
		UE_LOG(LogTemp, Verbose, TEXT("[CombatOrchestrator] OnActionCompleted ignored - the kill shot is holding"));
		return;
	}

	// Clear auto-advance timer if it was running
	GetWorld()->GetTimerManager().ClearTimer(AutoAdvanceTimerHandle);

	// B-170c (Q-170c): an action-pips turn goes on while actions are left; only 0, End Turn and Defend end it.
	// The turn-start and turn-end steps run once per turn: asking again skips both.
	const bool bActionKeepsTurn = bTurnContinuesAfterAction;
	bTurnContinuesAfterAction = false;
	if (bActionKeepsTurn && CanContinueTurn(CurrentActor))
	{
		const ECombatState MidTurnWinState = CheckWinCondition();
		if (MidTurnWinState != ECombatState::InProgress)
		{
			UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Win condition met mid-turn: %d"), (int32)MidTurnWinState);
			// B-405: a mid-turn win holds on its kill shot too.
			if (MidTurnWinState == ECombatState::Victory && StartKillShotHold())
			{
				return;
			}
			FinishCombat(MidTurnWinState, /*bTurnManagerEnded=*/false);
			return;
		}
		UE_LOG(LogWoRCombat, Verbose, TEXT("[CombatOrchestrator] %s acts again: %s"), *CurrentActor->GetName(), *GetTurnSpendsString());
		RequestActionFromActor(CurrentActor);
		return;
	}

	// Q-170f2 (Crown, 2026-11-12): actions above Max are used this turn or lost now; the rest stays banked. Q-170f3:
	// that includes a bank left above a lowered Max (Max Actions Down). Before the turn-end effects, so a Max Actions
	// Buff whose duration ticks out here doesn't take an action banked under it. An Execution turn (a ritual's fire)
	// is not the holder's own turn and keeps the bank.
	ExpireBonusActionsAtTurnEnd(CurrentActor);

	// Process end-of-turn effects
	ProcessEndOfTurnEffects(CurrentActor);

	// Check win condition
	ECombatState WinState = CheckWinCondition();
	if (WinState != ECombatState::InProgress)
	{
		UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Win condition met: %d"), (int32)WinState);

		// B-405 (R14): a win holds on its kill shot before the fight ends and its rewards show (FeelGuide 6).
		if (WinState == ECombatState::Victory && StartKillShotHold())
		{
			return;
		}
		FinishCombat(WinState, /*bTurnManagerEnded=*/false);
		return;
	}

	// Advance to next turn
	TurnManagerRef->AdvanceToNextTurn();
}

bool ACombatOrchestrator::StartKillShotHold()
{
	AActor *Victim = LastEnemyDown.Get();
	UWorld *World = GetWorld();
	if (!CameraDirector || !Victim || !World)
	{
		return false;
	}
	const float HoldSeconds = CameraDirector->PlayKillShot(Victim, GetEnemyRank(Victim));
	if (HoldSeconds <= 0.0f)
	{
		return false;
	}
	// The fight is won: nothing else may start during the hold (actions, flee, control switches, menu picks all refuse
	// on this flag, as during BeginFlee). FinishCombat clears it.
	bWaitingForAsyncAction = true;
	World->GetTimerManager().SetTimer(KillShotHoldTimer, this, &ACombatOrchestrator::FinishKillShotHold, HoldSeconds, false);
	return true;
}

void ACombatOrchestrator::FinishKillShotHold()
{
	KillShotHoldTimer.Invalidate();
	if (CombatState != ECombatState::InProgress)
	{
		return; // ended another way during the hold (a forced end)
	}
	FinishCombat(ECombatState::Victory, /*bTurnManagerEnded=*/false);
}

bool ACombatOrchestrator::IsHoldingKillShot() const
{
	const UWorld *World = GetWorld();
	return World && World->GetTimerManager().IsTimerActive(KillShotHoldTimer);
}

// ========================================
// TURN MANAGER EVENT HANDLERS
// ========================================

void ACombatOrchestrator::HandleTurnStarted(AActor *Actor, int32 TurnNumber)
{
	CurrentActor = Actor;
	CurrentTurnNumber = TurnNumber;
	bTurnContinuesAfterAction = false; // B-170c: a new turn, Execution turns included
	TurnSpends.Reset();

	UE_LOG(LogTemp, Verbose, TEXT("[CombatOrchestrator] Turn %d started: %s"),
		   TurnNumber, *Actor->GetName());

	// W-B: an Execution turn carries a scheduled ritual fire instead of an action request.
	// Q-044d (B-225): it is still one of the caster's turns, so the turn-start step runs first
	// (spans end, payouts and the Broken Darkness overflow land). A caster that step killed ends
	// the turn as a dead actor's turn does (the fight-over check runs) and its ritual is dropped
	// with its payload. Every way out of the turn goes through OnActionCompleted, so the turn-end
	// step counts it too. Every other turn kind (Normal / Bonus) runs the ordinary flow. The
	// countdown that decides WHEN this fires lives in TurnManager (delay burns on Normal turns).
	if (TurnManagerRef && TurnManagerRef->GetCurrentTurnType() == ETurnType::Execution)
	{
		ProcessStartOfTurnEffects(CurrentActor);
		ProcessBrokenDarknessOverflow(CurrentActor);
		if (!IsActorAlive(CurrentActor))
		{
			UE_LOG(LogTemp, Verbose, TEXT("[CombatOrchestrator] %s died at its Execution turn start, ritual dropped"),
				   *CurrentActor->GetName());
			DeferredPayloads.Remove(TurnManagerRef->GetCurrentTurnPayloadId());
			OnActionCompleted();
			return;
		}
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
		UE_LOG(LogTemp, Verbose, TEXT("[CombatOrchestrator] %s died at turn start, skipping action"),
			   *CurrentActor->GetName());
		OnActionCompleted();
		return;
	}

	// B-170c (D-088, ActionPips.md §1): no actions, no turn. The turn-end step still runs, then the rotation moves on.
	if (IsActionPipsFight() && !BeginTurnActions(CurrentActor))
	{
		UE_LOG(LogWoRCombat, Log, TEXT("[CombatOrchestrator] %s has no actions: turn skipped"), *CurrentActor->GetName());
		// TODO: B-170e: show "No actions" on the HUD (an FText in the shared turn words, D-062).
		OnActionCompleted();
		return;
	}

	// Broadcast for UI/listeners
	OnActorTurnStarted.Broadcast(CurrentActor, CurrentTurnNumber);

	// Stunned actors are asked too (D-001: Attack/Defend still allowed). ValidateAction
	// enforces the rule; an invalid pick re-asks a player and passes an AI's turn.
	RequestActionFromActor(CurrentActor);
}

// ========================================
// ACTION PIPS (B-170c, ActionPips.md §3.3)
// ========================================

bool ACombatOrchestrator::IsActionPipsFight() const
{
	return TurnManagerRef && TurnManagerRef->IsRotationMode();
}

bool ACombatOrchestrator::BeginTurnActions(AActor *Actor)
{
	UCharacterDataComponent *CharComp = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	if (!CharComp)
	{
		return true; // nothing to bank: the turn plays as built
	}
	// (1) The turn's regain. A first turn plays the fight-start bank (Q-170c2).
	if (ActorsPastFirstTurn.Contains(Actor))
	{
		CharComp->RegainActions();
	}
	else
	{
		ActorsPastFirstTurn.Add(Actor);
	}
	CharComp->ResetSkillsUsedThisTurn(); // Q-170j: the per-turn skill limit counts this turn only
	// (2) B-170d: waiting gains and losses (Lose Action next turn, Wind's break, a delayed Emerald gain) land here.
	CharComp->ApplyWaitingActionChanges();
	// (3) Only then the check.
	return CharComp->CurrentActions > 0;
}

FText ACombatOrchestrator::GetActionCostRefusedReason(AActor *Actor, const FAction &Action) const
{
	if (!IsActionPipsFight())
	{
		return FText::GetEmpty();
	}
	const UCharacterDataComponent *CharComp = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	if (!CharComp)
	{
		return FText::GetEmpty();
	}
	// B-170e: the same text the command menu greys a button with (ActionPipsText).
	return ActionPipsText::GetCostRefusedReason(Action.GetActionCost(), CharComp->CurrentActions, CharComp->GetMaxActions());
}

bool ACombatOrchestrator::PayActionCost(const FAction &Action, FString &OutRefusedReason)
{
	bTurnContinuesAfterAction = false;
	if (!IsActionPipsFight())
	{
		return true; // one action per turn, as built
	}
	const FText Refusal = GetActionCostRefusedReason(CurrentActor, Action);
	if (!Refusal.IsEmpty())
	{
		OutRefusedReason = Refusal.ToString();
		return false;
	}
	const int32 Cost = Action.GetActionCost();
	UCharacterDataComponent *CharComp = CurrentActor ? CurrentActor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	if (Cost > 0 && CharComp && !CharComp->SpendActions(Cost))
	{
		OutRefusedReason = TEXT("the actions could not be spent"); // the check above passed, so only a client copy lands here
		return false;
	}
	if (Cost > 0 && CharComp)
	{
		CharComp->NoteSkillUsed(); // Q-170j: End Turn, Defend and switching cost nothing and don't count
	}
	TurnSpends.Add(FString::Printf(TEXT("%s -%d"), *Action.GetActionName(), Cost));
	// Q-170c: Defend ends the turn; Flee ends the fight; a ritual's fire is its own Execution turn.
	const bool bEndsTurn = Action.ActionType == EActionType::Defend || Action.ActionType == EActionType::Flee || Action.bIsDeferredFire;
	bTurnContinuesAfterAction = !bEndsTurn;
	return true;
}

bool ACombatOrchestrator::CanContinueTurn(AActor *Actor) const
{
	if (!Actor || !IsActorAlive(Actor))
	{
		return false;
	}
	if (TurnManagerRef && TurnManagerRef->GetCurrentTurnType() == ETurnType::Execution)
	{
		return false;
	}
	// B-170f (ActionPips.md §3.6): the AI is asked again like a player and spends or banks itself
	// (UAIDecisionManager::DecideSpendOrBank). A Void break's random turn stays one action (Q-170c2, Q-170f2).
	const bool bVoidBroken = SkillEffectManagerRef && SkillEffectManagerRef->HasEffectOfType(Actor, ESkillEffectType::RandomSkill);
	if (bVoidBroken)
	{
		return false;
	}
	if (SkillEffectManagerRef && SkillEffectManagerRef->IsStunned(Actor))
	{
		return false; // a stun leaves one action (Attack or Defend, D-001), not a bank of them (Q-170c2)
	}
	const UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
	if (CharComp && !ActionPipsRules::HasSkillsLeftThisTurn(CharComp->SkillsUsedThisTurn))
	{
		UE_LOG(LogWoRCombat, Verbose, TEXT("[CombatOrchestrator] %s used %d skills: the turn ends, %d action(s) stay banked"),
			   *Actor->GetName(), CharComp->SkillsUsedThisTurn, CharComp->CurrentActions);
		return false; // Q-170j (Crown, 2026-11-12): at most SKILLS_PER_TURN_MAX per turn
	}
	return CharComp && CharComp->CurrentActions > 0;
}

void ACombatOrchestrator::ExpireBonusActionsAtTurnEnd(AActor *Actor)
{
	if (!IsActionPipsFight() || (TurnManagerRef && TurnManagerRef->GetCurrentTurnType() == ETurnType::Execution))
	{
		return;
	}
	if (UCharacterDataComponent *CharComp = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr)
	{
		CharComp->ExpireActionsAboveMax();
		CharComp->ResetSkillsUsedThisTurn(); // Q-170j: the HUD's "Skills n/3" shows only during the holder's turn
	}
}

void ACombatOrchestrator::EndTurnByChoice()
{
	GetWorld()->GetTimerManager().ClearTimer(AutoAdvanceTimerHandle);
	TurnSpends.Add(TEXT("End Turn"));
	UE_LOG(LogWoRCombat, Verbose, TEXT("[CombatOrchestrator] %s ends the turn: %s"), *GetNameSafe(CurrentActor), *GetTurnSpendsString());
	bTurnContinuesAfterAction = false;
	OnActionCompleted();
}

FString ACombatOrchestrator::GetTurnSpendsString() const
{
	if (!IsActionPipsFight())
	{
		return TEXT("one action per turn (wor.Turns.ActionPips 0 at fight start)");
	}
	const UCharacterDataComponent *CharComp = CurrentActor ? CurrentActor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	return FString::Printf(TEXT("%s's turn: %s (%d left, skills %d/%d)"), *GetNameSafe(CurrentActor),
						   TurnSpends.Num() > 0 ? *FString::Join(TurnSpends, TEXT(", ")) : TEXT("nothing spent"),
						   CharComp ? CharComp->CurrentActions : 0, CharComp ? CharComp->SkillsUsedThisTurn : 0,
						   ActionPipsConstants::SKILLS_PER_TURN_MAX);
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

	UE_LOG(LogTemp, Verbose, TEXT("[CombatOrchestrator] Ritual armed (payload %d, spellspeed %.2f) — Execution turn in %d normal turn(s)"),
		   PayloadId, FrozenSpellSpeed, DelayTurns);
}

void ACombatOrchestrator::FireScheduledExecution()
{
	// B-406b: held like an action request under a boss phase cinematic (no fire, no defence timing until the HUD is
	// back); HandleBossCinematicFinished calls this again. The payload stays armed meanwhile.
	if (CameraDirector && CameraDirector->IsBossCinematicPlaying())
	{
		bHoldingForBossCinematic = true;
		bWaitingForAsyncAction = true;
		UE_LOG(LogWoRCombat, Log, TEXT("[CombatOrchestrator] %s's ritual fire waits for the boss cinematic"), *GetNameSafe(CurrentActor));
		return;
	}

	if (!ActionExecutorRef)
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatOrchestrator] Execution turn but no ActionExecutor — skipping"));
		OnActionCompleted();
		return;
	}

	const int32 PayloadId = TurnManagerRef->GetCurrentTurnPayloadId();
	FDeferredActivation *Entry = DeferredPayloads.Find(PayloadId);
	if (!Entry)
	{
		// Malformed slot (payload missing) — skip without requesting an action.
		UE_LOG(LogTemp, Error, TEXT("[CombatOrchestrator] Execution turn payload %d not found — skipping"), PayloadId);
		OnActionCompleted();
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
		UE_LOG(LogTemp, Verbose, TEXT("[CombatOrchestrator] Execution turn caster %s dead/invalid — skipping fire"),
			   IsValid(Caster) ? *Caster->GetName() : TEXT("<invalid>"));
		OnActionCompleted();
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
	UE_LOG(LogTemp, Verbose, TEXT("[CombatOrchestrator] Execution turn: firing %s (caster %s, payload %d)"),
		   *FireAction.GetActionName(), *Caster->GetName(), PayloadId);

	FString InvalidReason;
	if (!StartActionAsync(FireAction, InvalidReason))
	{
		// No one to re-ask on an Execution turn — don't strand it.
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] Execution fire refused (%s) — advancing"),
			   InvalidReason.IsEmpty() ? TEXT("guard") : *InvalidReason);
		OnActionCompleted();
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

		// Q-246: the same end steps as a win through OnActionCompleted (destruction and repair on a
		// finished fight); the TurnManager has already ended.
		FinishCombat(WinState, /*bTurnManagerEnded=*/true);
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
	// OBSOLETE (B-346): no takeover when your fighters fall; AI seats play on (Q-21-audit, PA-12).
	// Only ENEMY (OpposingParty) deaths feed the pool — a player-side (LocalParty) death grants nothing.
	if (!OpposingPartyCombatants.Contains(Victim))
	{
		return;
	}
	LastEnemyDown = Victim; // B-405: the last one down gets the kill shot
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

	// B-223 (D-102): the enemy's own rank (B-224b) and Combat Tier (B-325) set its level drop.
	EEncounterRank Rank = EncounterRank;
	if (const EEncounterRank *OwnRank = EnemyRanks.Find(Victim))
	{
		Rank = *OwnRank;
	}
	// B-187b (Q-187b): a revived mini-boss's team drops as its stepped-up tier (set at spawn, review: not read from
	// the game-instance run, D-064).
	const int32 *TierSteps = EnemyTierSteps.Find(Victim);
	PendingLevelDropSources.Emplace(TrialGate::StepTier(WoRCombatTier::GetCombatTier(Victim), TierSteps ? *TierSteps : 0), Rank); // B-325: its Combat Tier at death (usually the one its HP used at spawn; a loadout change mid-fight can move it)

	UE_LOG(LogTemp, Verbose, TEXT("[CombatOrchestrator] Enemy %s down (caliber %d) -> +%d WSP (pool now %d)"),
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
		if (UCombatCommandMenuSubsystem *Menu = WoRFight::Get<UCombatCommandMenuSubsystem>(GI))
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

		UE_LOG(LogTemp, Verbose, TEXT("[CombatOrchestrator] State: %d -> %d"), (int32)OldState, (int32)NewState);

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
	if (UStatusBuildupManager *BuildupManager = WoRFight::Get<UStatusBuildupManager>(this))
	{
		BuildupManager->ProcessStatusBarDecay(Actor);
	}

	// D-089 (B-173): Energy refills at the start of each of the holder's turns, after the turn-start payouts.
	TurnStartEnergy::Apply(Actor);

	UE_LOG(LogTemp, Verbose, TEXT("[CombatOrchestrator] Processed start-of-turn effects for %s"),
		   *Actor->GetName());
}

void ACombatOrchestrator::ProcessEndOfTurnEffects(AActor *Actor)
{
	// B-232 (Q-21-audit): a turn the side's rotation took moves the rotation to the side's next player.
	if (URunStateComponent *Run = URunStateComponent::Find(this); Run && HasAuthority())
	{
		Run->NoteTurnTaken(Actor);
	}

	if (!SkillEffectManagerRef)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] SkillEffectManager not available for end-of-turn processing"));
		return;
	}

	// Delegate to SkillEffectManager - processes DOTs, ticks durations, expires effects
	SkillEffectManagerRef->ProcessEndOfTurnEffects(Actor);

	UE_LOG(LogTemp, Verbose, TEXT("[CombatOrchestrator] Processed end-of-turn effects for %s"),
		   *Actor->GetName());
}

void ACombatOrchestrator::RequestActionFromActor(AActor *Actor)
{
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] RequestActionFromActor called with null actor"));
		return;
	}

	// B-406: a boss phase cinematic holds the next action (no menu, no AI, no defence timing) until the HUD is back.
	if (CameraDirector && CameraDirector->IsBossCinematicPlaying())
	{
		bHoldingForBossCinematic = true;
		bWaitingForAsyncAction = true;
		UE_LOG(LogWoRCombat, Log, TEXT("[CombatOrchestrator] %s's action waits for the boss cinematic"), *Actor->GetName());
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

		UE_LOG(LogTemp, Verbose, TEXT("[CombatOrchestrator] Routing %s to AI decision"), *Actor->GetName());
		GetWorld()->GetTimerManager().ClearTimer(AutoAdvanceTimerHandle);
		AIDecisionManagerRef->RequestDecision(Actor);
		return;
	}

	// === PLAYER-CONTROLLED ACTOR ===
	UE_LOG(LogTemp, Verbose, TEXT("[CombatOrchestrator] Broadcasting OnActionRequested for player actor: %s"),
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

		UE_LOG(LogTemp, Verbose, TEXT("[CombatOrchestrator] Auto-advancing in %.1fs..."), AutoAdvanceDelay);
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
				UE_LOG(LogTemp, Verbose, TEXT("[CombatOrchestrator] Prepared loadout for %s"), *Actor->GetName());

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
						UE_LOG(LogTemp, Verbose, TEXT("[CombatOrchestrator] Applied %d starting effects to %s"),
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

	UE_LOG(LogTemp, Verbose, TEXT("[CombatOrchestrator] BD Overflow processed for %s - Range: %.1f, Targets: %d"),
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
			UE_LOG(LogTemp, Verbose, TEXT("[CombatOrchestrator] Reset loadout battle state for %s"), *Actor->GetName());
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
