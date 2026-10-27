// BattleGameMode.cpp

#include "Trial/BattleGameMode.h"

#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CombatCharacter.h"
#include "Combat/Boss/BossPhaseComponent.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"
#include "Combat/CombatControl.h"
#include "Combat/Grid/CombatGridSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Loadout/LoadoutComponent.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "TimerManager.h"
#include "Combat/Encounter/EnemyZone.h"
#include "Memory/MemoryUnlock.h"
#include "Reaction/ReactionTrigger.h"
#include "Trial/TrialGate.h"
#include "Trial/TrialRunSubsystem.h"
#include "UI/Combat/CombatResultsWidget.h"
#include "GameFramework/PlayerController.h"
#include "Player/WoRPlayerState.h"

namespace BattleGameModeConstants
{
    // Pre-placement spawn spread. StartCombat's grid PlaceAllActors repositions
    // everyone; these only keep deferred spawns from stacking at one point.
    constexpr float TEAM_X_OFFSET = 300.0f;
    constexpr float COMBATANT_Y_SPACING = 150.0f;
}

ABattleGameMode::ABattleGameMode()
{
    PlayerStateClass = AWoRPlayerState::StaticClass();
}

void ABattleGameMode::BeginPlay()
{
    Super::BeginPlay();

    // Deferred one tick: GameMode BeginPlay vs initial player login ordering
    // is not reliable, and BootstrapCombat needs PC0 for possession.
    GetWorldTimerManager().SetTimerForNextTick(this, &ABattleGameMode::BootstrapCombat);
}

void ABattleGameMode::BootstrapCombat()
{
    UTrialRunSubsystem *TrialRun = GetGameInstance()->GetSubsystem<UTrialRunSubsystem>();
    if (!TrialRun || !TrialRun->HasPendingEncounter())
    {
        // Direct PIE on the battle level (no stash) is a legitimate dev path —
        // log and idle rather than bouncing to a trial that was never entered.
        UE_LOG(LogTemp, Warning, TEXT("[BattleGameMode] No pending encounter stashed - battle level idles"));
        return;
    }

    UClass *PlayerPawnClass = DefaultPlayerPawnClass.LoadSynchronous();
    UClass *EnemyPawnClass = DefaultEnemyPawnClass.LoadSynchronous();
    UClass *LoadedOrchestratorClass = OrchestratorClass.LoadSynchronous();

    // Named per field. The old message listed all three whichever one failed, and
    // could not distinguish "never authored" from "authored but the path no longer
    // resolves" — the second is what a moved asset with a stale soft path looks
    // like, and it cost a full diagnosis pass to identify once already.
    auto LogUnresolved = [](const TCHAR *FieldName, const FSoftObjectPath &Path)
    {
        UE_LOG(LogTemp, Error, TEXT("[BattleGameMode] %s unset or failed to load: %s"),
               FieldName, Path.IsNull() ? TEXT("<unset>") : *Path.ToString());
    };

    if (!PlayerPawnClass)
    {
        LogUnresolved(TEXT("DefaultPlayerPawnClass"), DefaultPlayerPawnClass.ToSoftObjectPath());
    }
    if (!EnemyPawnClass)
    {
        LogUnresolved(TEXT("DefaultEnemyPawnClass"), DefaultEnemyPawnClass.ToSoftObjectPath());
    }
    if (!LoadedOrchestratorClass)
    {
        LogUnresolved(TEXT("OrchestratorClass"), OrchestratorClass.ToSoftObjectPath());
    }

    if (!PlayerPawnClass || !EnemyPawnClass || !LoadedOrchestratorClass)
    {
        AbortToTrial(TEXT("battle class refs unresolved - see the per-field errors above"));
        return;
    }

    APlayerController *PC = ResolvePartyLeader();
    const AActor *StartSpot = FindPlayerStart(PC);
    const FVector Origin = StartSpot ? StartSpot->GetActorLocation() : FVector::ZeroVector;

    // Spawn fresh pawns per stashed CharacterData. Deferred-spawn is MANDATORY:
    // UCharacterDataComponent::BeginPlay runs the full init cascade off the
    // assigned asset (see SpawnCombatant).
    TArray<AActor *> LocalParty;
    TArray<AActor *> OpposingParty;

    // Battle Party — the Trial Party members who were present when the join window
    // closed. Enemy roster still comes from the encounter, not the party.
    const TArray<UCharacterData *> BattlePartyData = TrialRun->GetPendingBattleParty();
    const TArray<UCharacterData *> OpposingPartyData = TrialRun->GetPendingOpposingParty();

    // Fail loudly rather than falling back to some other roster source: the point
    // of this arc is that the party layer is authoritative, and a silent fallback
    // would hide a regression in exactly the path we are trying to prove.
    if (BattlePartyData.Num() == 0)
    {
        AbortToTrial(TEXT("Battle Party empty at battle start - the encounter stashed no members. "
                          "Check the encounter trigger path or PartySession lazy-create"));
        return;
    }

    // Cross-check against the session party. Disagreement is not fatal — a Battle
    // Party is legitimately a SUBSET of the Trial Party — but a Battle Party member
    // absent from the Trial Party means the membership gate leaked, so say so.
    const UPartySessionSubsystem *PartySession = GetGameInstance()->GetSubsystem<UPartySessionSubsystem>();
    const UParty *TrialParty = PartySession ? PartySession->GetLocalParty() : nullptr;
    if (PartySession)
    {
        const int32 TrialCount = TrialParty ? TrialParty->GetMemberCount() : 0;
        if (!TrialParty)
        {
            UE_LOG(LogTemp, Warning,
                   TEXT("[BattleGameMode] No Trial Party exists - the encounter path did not create one. "
                        "Battle proceeds on the stashed roster (%d)"),
                   BattlePartyData.Num());
        }
        else if (BattlePartyData.Num() > TrialCount)
        {
            UE_LOG(LogTemp, Warning,
                   TEXT("[BattleGameMode] Battle Party (%d) exceeds Trial Party (%d) - membership gate leaked"),
                   BattlePartyData.Num(), TrialCount);
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("[BattleGameMode] Battle Party %d of Trial Party %d"),
                   BattlePartyData.Num(), TrialCount);
        }
    }

    // D-003: each member spawns as its own FPartyMember.PawnClass (DefaultPlayerPawnClass
    // when unset or unresolved) and plays as its D-004 control setting.
    APawn *LeaderPawn = nullptr;
    APawn *FirstPlayerMemberPawn = nullptr;
    for (int32 i = 0; i < BattlePartyData.Num(); ++i)
    {
        const int32 Slot = PartySession ? PartySession->GetMemberSlotByData(BattlePartyData[i]) : INDEX_NONE;
        const FPartyMember *Member = TrialParty && TrialParty->Members.IsValidIndex(Slot) ? &TrialParty->Members[Slot] : nullptr;
        UClass *MemberPawnClass = Member ? Member->PawnClass.LoadSynchronous() : nullptr;

        const FVector Location = Origin + FVector(-BattleGameModeConstants::TEAM_X_OFFSET,
                                                  i * BattleGameModeConstants::COMBATANT_Y_SPACING, 0.0f);
        APawn *Spawned = SpawnCombatant(MemberPawnClass ? MemberPawnClass : PlayerPawnClass, BattlePartyData[i], FTransform(Location));
        if (!Spawned)
        {
            continue;
        }
        if (!CanEnterCombat(Spawned))
        {
            UE_LOG(LogTemp, Warning, TEXT("[BattleGameMode] %s has no gear - left out of the fight (D-016)"),
                   *BattlePartyData[i]->GetName());
            Spawned->Destroy();
            continue;
        }
        const bool bPlayerControlled = !Member || Member->bPlayerControlled;
        CombatControl::SetPlayerDriven(Spawned, bPlayerControlled);
        if (Slot == PartyConstants::LEADER_SLOT)
        {
            LeaderPawn = Spawned;
        }
        if (bPlayerControlled && !FirstPlayerMemberPawn)
        {
            FirstPlayerMemberPawn = Spawned;
        }
        LocalParty.Add(Spawned);
    }
    for (int32 i = 0; i < OpposingPartyData.Num(); ++i)
    {
        const FVector Location = Origin + FVector(BattleGameModeConstants::TEAM_X_OFFSET,
                                                  i * BattleGameModeConstants::COMBATANT_Y_SPACING, 0.0f);
        if (APawn *Spawned = SpawnCombatant(EnemyPawnClass, OpposingPartyData[i], FTransform(Location)))
        {
            OpposingParty.Add(Spawned);
        }
    }
    if (LocalParty.Num() == 0 || OpposingParty.Num() == 0)
    {
        AbortToTrial(FString::Printf(TEXT("combatant spawn failed (LocalParty %d/%d, OpposingParty %d/%d)"),
                                     LocalParty.Num(), BattlePartyData.Num(), OpposingParty.Num(), OpposingPartyData.Num()));
        return;
    }

    // Possess the first player-side pawn: view target + (with a
    // CombatPlayerController-derived PC class) realtime defense input.
    //
    // ⚠️ CONTROL READS BEFORE THIS POINT ARE UNRELIABLE. ACombatCharacter sets
    // AutoPossessAI, so every pawn — including this one — auto-spawned an AI
    // controller back at FinishSpawningActor. LocalParty[0] therefore reports
    // IsBotControlled() until the Possess below. Combat-time reads are safe (they
    // all run after StartCombat, which is after this), but anything that asks
    // "is this AI?" during a pawn's BeginPlay would get the wrong answer.
    if (PC)
    {
        APawn *PreviousPawn = PC->GetPawn();

        // The AI controller this displaces reaps itself — see
        // ACombatAIController::OnUnPossess. Handling it here too would be a second
        // mechanism for one job, and would still miss the login-time orphan.
        // The leader when it fights; if it sat out (no gear, Q-071b), the first member you play.
        APawn *PossessTarget = LeaderPawn ? LeaderPawn : FirstPlayerMemberPawn ? FirstPlayerMemberPawn : CastChecked<APawn>(LocalParty[0]);
        PC->Possess(PossessTarget);

        if (PreviousPawn && PreviousPawn != PC->GetPawn())
        {
            PreviousPawn->Destroy();
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleGameMode] No PC0 at bootstrap - combat starts unpossessed"));
    }

    // Stale grid entries from the trial level survive the swap on the
    // GameInstance-scoped subsystem and can block slot assignment.
    if (UCombatGridSubsystem *Grid = GetGameInstance()->GetSubsystem<UCombatGridSubsystem>())
    {
        Grid->ClearAllPositions();
    }

    ACombatOrchestrator *SpawnedOrchestrator =
        GetWorld()->SpawnActor<ACombatOrchestrator>(LoadedOrchestratorClass, Origin, FRotator::ZeroRotator);
    if (!SpawnedOrchestrator)
    {
        AbortToTrial(TEXT("orchestrator spawn failed"));
        return;
    }
    Orchestrator = SpawnedOrchestrator;
    SpawnedOrchestrator->OnCombatResultReady.AddDynamic(this, &ABattleGameMode::HandleCombatEnd);

    UE_LOG(LogTemp, Log, TEXT("[BattleGameMode] Starting combat %d v %d (difficulty %s, rank %s)"),
           LocalParty.Num(), OpposingParty.Num(), *UEnum::GetValueAsString(TrialRun->GetPendingDifficulty()),
           *UEnum::GetValueAsString(TrialRun->GetPendingRank()));
    // B-183 / B-186: the gate's difficulty and the zone the fight started in scale every enemy
    // before the fight (and its HUD) starts.
    const FEncounterZoneInfo Zone = TrialRun->GetPendingZone();
    const float EnemyStrength = TrialRun->GetEnemyStrengthMultiplier() * AEnemyZone::GetTierStrengthMultiplier(Zone.Tier);
    for (AActor *Enemy : OpposingParty)
    {
        if (!TrialGate::ApplyEnemyStrength(Enemy, EnemyStrength))
        {
            UE_LOG(LogTemp, Warning, TEXT("[BattleGameMode] %s: trial difficulty not applied (no character data or effect manager)"),
                   *GetNameSafe(Enemy));
        }
    }
    UE_LOG(LogTemp, Log, TEXT("[BattleGameMode] Trial difficulty %s, zone tier %d: enemy strength x%.2f"),
           *TrialGate::GetDifficultyName(TrialRun->GetTrialDifficulty()).ToString(), Zone.Tier, EnemyStrength);

    // B-191: reactions (worn items facing a character) and this run's memories change the fight
    // before it starts.
    const TArray<FReactionTrigger> Reactions = ReactionTriggers::GatherTriggers();
    const TArray<FFiredReaction> Fired = ReactionTriggers::FindFired(Reactions, LocalParty, OpposingParty);
    const int32 ReactionEffects = ReactionTriggers::ApplyFired(Fired);
    // B-209: fired reactions and bound gear worn by its owner unlock their memories (before the
    // memory boss changes below, so a memory unlocked now counts this fight).
    MemoryUnlock::UnlockAtFightStart(GetGameInstance(), Fired, LocalParty);
    // B-193: any opposing combatant with boss data gets its phases: after reactions change its
    // stats (so they can't reach a phase early), before memories skip or add phases.
    for (AActor *Enemy : OpposingParty)
    {
        const UCharacterDataComponent *EnemyData = Enemy ? Enemy->FindComponentByClass<UCharacterDataComponent>() : nullptr;
        if (UBossData *BossData = EnemyData ? UBossData::FindFor(EnemyData->CharacterData) : nullptr)
        {
            UBossPhaseComponent *Phases = NewObject<UBossPhaseComponent>(Enemy);
            Enemy->AddInstanceComponent(Phases);
            Phases->RegisterComponent();
            Phases->Setup(BossData);
        }
    }

    const int32 MemoryChanges = ReactionTriggers::ApplyMemoryBossModifiers(TrialRun->GetRunMemories(), OpposingParty);
    UE_LOG(LogTemp, Log, TEXT("[BattleGameMode] Reactions: %d of %d fired (%d effects); memory boss changes: %d"),
           Fired.Num(), Reactions.Num(), ReactionEffects, MemoryChanges);

    SpawnedOrchestrator->SetEncounterRank(TrialRun->GetPendingRank());
    // B-185: the side that struck first on the map takes the opening round.
    SpawnedOrchestrator->SetFirstStrike(TrialRun->GetPendingFirstStrike());
    UE_LOG(LogTemp, Log, TEXT("[BattleGameMode] First strike: %s"), *UEnum::GetValueAsString(TrialRun->GetPendingFirstStrike()));
    SpawnedOrchestrator->StartCombat(LocalParty, OpposingParty, TrialRun->GetPendingDifficulty());
}

APawn *ABattleGameMode::SpawnCombatant(UClass *PawnClass, UCharacterData *Data, const FTransform &SpawnTransform)
{
    if (!Data)
    {
        return nullptr;
    }

    // Deferred spawn: CharacterData must be assigned BEFORE FinishSpawning so
    // UCharacterDataComponent::BeginPlay (inventory/loadout/world-stat init,
    // crystal-aware pool recompute, HP/EP seed, born-BD) reads this asset, not
    // the BP default. InitializeFromTemplate() is NOT a recovery path - it
    // skips the cascade.
    APawn *Pawn = GetWorld()->SpawnActorDeferred<APawn>(PawnClass, SpawnTransform, nullptr, nullptr,
                                                        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Pawn)
    {
        UE_LOG(LogTemp, Error, TEXT("[BattleGameMode] Deferred spawn failed for %s"), *Data->GetName());
        return nullptr;
    }

    if (UCharacterDataComponent *CharComp = Pawn->FindComponentByClass<UCharacterDataComponent>())
    {
        CharComp->CharacterData = Data;
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[BattleGameMode] %s has no UCharacterDataComponent - %s cannot fight"),
               *PawnClass->GetName(), *Data->GetName());
        Pawn->Destroy();
        return nullptr;
    }

    UGameplayStatics::FinishSpawningActor(Pawn, SpawnTransform);
    UE_LOG(LogTemp, Log, TEXT("[BattleGameMode] Spawned %s as %s"), *Data->GetName(), *Pawn->GetName());
    return Pawn;
}

bool ABattleGameMode::CanEnterCombat(const AActor *Combatant)
{
    const UCharacterDataComponent *CharComp = Combatant ? Combatant->FindComponentByClass<UCharacterDataComponent>() : nullptr;
    if (!CharComp || !CharComp->CharacterData)
    {
        return false;
    }
    if (CharComp->CharacterData->CharacterClass == ECharacterClass::Caster)
    {
        return true;
    }
    const ULoadoutComponent *Loadout = Combatant->FindComponentByClass<ULoadoutComponent>();
    // GetActiveRing: a Resonator's rings sit in its ring slots, not the primary slot.
    return Loadout && (Loadout->GetPrimaryWeapon() || Loadout->GetPrimaryRing() || Loadout->GetPrimaryEvolution() ||
                       Loadout->HasSecondaryEquipment() || Loadout->GetActiveRing());
}

APlayerController *ABattleGameMode::ResolvePartyLeader()
{
    UPartySessionSubsystem *PartySession = GetGameInstance()->GetSubsystem<UPartySessionSubsystem>();
    const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
    APlayerController *Leader = Party ? Party->ResolveLeader(GetWorld()) : nullptr;
    if (!Leader)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleGameMode] %s - using the first local player"),
               Party ? TEXT("The party's leader isn't in this level") : TEXT("No party (battle map opened directly?)"));
        Leader = GetWorld()->GetFirstPlayerController();
    }
    if (Party && Leader)
    {
        // Rebind in both cases so the wallet and later reads agree with the start spot.
        PartySession->RebindLeader(Leader);
    }
    return Leader;
}

void ABattleGameMode::AbortToTrial(const FString &Reason)
{
    UE_LOG(LogTemp, Error, TEXT("[BattleGameMode] Battle aborted: %s - returning to trial"), *Reason);
    if (UTrialRunSubsystem *TrialRun = GetGameInstance()->GetSubsystem<UTrialRunSubsystem>())
    {
        TrialRun->ExitEncounter();
    }
}

void ABattleGameMode::HandleCombatEnd(const FCombatResult &Result)
{
    if (Orchestrator.IsValid())
    {
        Orchestrator->OnCombatResultReady.RemoveDynamic(this, &ABattleGameMode::HandleCombatEnd);
    }

    UE_LOG(LogTemp, Log, TEXT("[BattleGameMode] Combat ended (%s, %d turns)"),
           *UEnum::GetValueAsString(Result.FinalState), Result.TotalTurns);

    // Bank player-side vitals into the party slots BEFORE the level swap - the
    // pawns are still alive here, and ExitEncounter's OpenLevel destroys them.
    // Must stay here, not in ReturnToTrial: the orchestrator empties its team
    // arrays right after this broadcast, before the results screen is dismissed.
    // Player side only: GetLocalParty() is TeamIndex 0. Enemies are unrostered
    // and must not be written back.
    //
    // A dead combatant banks CurrentHP == 0 (ServerTakeDamage floors at 0 and
    // CheckDeath leaves HP untouched) - that zero IS the dead marker the trial
    // side filters on, so deaths must NOT be skipped here.
    if (Orchestrator.IsValid())
    {
        if (UPartySessionSubsystem *PartySession = GetGameInstance()->GetSubsystem<UPartySessionSubsystem>())
        {
            int32 BankedCount = 0;
            for (AActor *Combatant : Orchestrator->GetLocalParty())
            {
                if (!Combatant)
                {
                    continue;
                }

                const UCharacterDataComponent *CharComp = Combatant->FindComponentByClass<UCharacterDataComponent>();
                if (!CharComp || !CharComp->CharacterData)
                {
                    continue;
                }

                // Pointer-identity match, same limit as elsewhere: two pawns
                // sharing one CharacterData asset resolve to the same slot and
                // the later walked one wins. Not reachable with distinct lords.
                const int32 SlotIndex = PartySession->GetMemberSlotByData(CharComp->CharacterData);
                if (SlotIndex != INDEX_NONE)
                {
                    PartySession->SetMemberVitals(SlotIndex, CharComp->CurrentHP, CharComp->CurrentEP);
                    // B-115: the orchestrator ran the between-fight destruction and +10 repair
                    // before this broadcast, so the snapshot is what the next fight starts with.
                    const ULoadoutComponent *Loadout = Combatant->FindComponentByClass<ULoadoutComponent>();
                    const UBrokenDarknessManager *BDManager = Combatant->FindComponentByClass<UBrokenDarknessManager>();
                    PartySession->SetMemberCarriedState(SlotIndex,
                                                        Loadout ? Loadout->SnapshotCarriedGear() : FCarriedGearState(),
                                                        BDManager ? BDManager->SnapshotCarriedAbsorption() : FCarriedAbsorptionState());
                    ++BankedCount;
                }
            }
            UE_LOG(LogTemp, Log, TEXT("[BattleGameMode] Banked vitals for %d/%d player-side combatant(s)"),
                   BankedCount, Orchestrator->GetLocalParty().Num());
        }
    }

    // B-184: a win beats every map team the fight came from, for the rest of the run.
    if (Result.FinalState == ECombatState::Victory)
    {
        if (UTrialRunSubsystem *TrialRun = GetGameInstance()->GetSubsystem<UTrialRunSubsystem>())
        {
            TrialRun->MarkPendingEncounterBeaten();
            // B-209: a beaten boss or mini-boss unlocks its memory.
            MemoryUnlock::UnlockBossDrops(GetGameInstance(), TrialRun->GetPendingOpposingParty());
        }
    }

    if (!ShowResults(Result))
    {
        ReturnToTrial();
    }
}

bool ABattleGameMode::ShowResults(const FCombatResult &Result)
{
    APlayerController *PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC)
    {
        return false;
    }

    UClass *WidgetClass = ResultsWidgetClass.IsNull() ? UCombatResultsWidget::StaticClass()
                                                      : ResultsWidgetClass.LoadSynchronous();
    if (!WidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleGameMode] ResultsWidgetClass unresolved (%s) - skipping results"),
               *ResultsWidgetClass.ToString());
        return false;
    }

    ResultsWidget = CreateWidget<UCombatResultsWidget>(PC, WidgetClass);
    if (!ResultsWidget)
    {
        return false;
    }

    constexpr int32 ResultsZOrder = 100;
    ResultsWidget->AddToViewport(ResultsZOrder);
    ResultsWidget->OnContinue.AddDynamic(this, &ABattleGameMode::HandleResultsContinue);
    ResultsWidget->ShowResult(Result);

    // GameOnly input keeps keys/clicks in the viewport; route them to the results widget.
    // The next level's controller resets input mode on the swap.
    FInputModeUIOnly InputMode;
    InputMode.SetWidgetToFocus(ResultsWidget->TakeWidget());
    PC->SetInputMode(InputMode);
    PC->SetShowMouseCursor(true);
    return true;
}

void ABattleGameMode::HandleResultsContinue()
{
    if (ResultsWidget)
    {
        ResultsWidget->OnContinue.RemoveDynamic(this, &ABattleGameMode::HandleResultsContinue);
        ResultsWidget->RemoveFromParent();
        ResultsWidget = nullptr;
    }
    ReturnToTrial();
}

void ABattleGameMode::ReturnToTrial()
{
    UE_LOG(LogTemp, Log, TEXT("[BattleGameMode] Returning to trial"));

    // ExitEncounter defers its OpenLevel one tick internally - safe to call
    // from inside the result broadcast.
    if (UTrialRunSubsystem *TrialRun = GetGameInstance()->GetSubsystem<UTrialRunSubsystem>())
    {
        TrialRun->ExitEncounter();
    }
}
