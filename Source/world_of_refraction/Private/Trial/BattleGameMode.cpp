// BattleGameMode.cpp

#include "Trial/BattleGameMode.h"

#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Grid/CombatGridSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "TimerManager.h"
#include "Trial/TrialRunSubsystem.h"

namespace BattleGameModeConstants
{
    // Pre-placement spawn spread. StartCombat's grid PlaceAllActors repositions
    // everyone; these only keep deferred spawns from stacking at one point.
    constexpr float TEAM_X_OFFSET = 300.0f;
    constexpr float COMBATANT_Y_SPACING = 150.0f;
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

    APlayerController *PC = UGameplayStatics::GetPlayerController(this, 0);
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
    if (const UPartySessionSubsystem *PartySession = GetGameInstance()->GetSubsystem<UPartySessionSubsystem>())
    {
        const UParty *TrialParty = PartySession->GetLocalParty();
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

    for (int32 i = 0; i < BattlePartyData.Num(); ++i)
    {
        const FVector Location = Origin + FVector(-BattleGameModeConstants::TEAM_X_OFFSET,
                                                  i * BattleGameModeConstants::COMBATANT_Y_SPACING, 0.0f);
        if (APawn *Spawned = SpawnCombatant(PlayerPawnClass, BattlePartyData[i], FTransform(Location)))
        {
            LocalParty.Add(Spawned);
        }
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
        PC->Possess(CastChecked<APawn>(LocalParty[0]));

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

    UE_LOG(LogTemp, Log, TEXT("[BattleGameMode] Starting combat %d v %d (difficulty %s)"),
           LocalParty.Num(), OpposingParty.Num(), *UEnum::GetValueAsString(TrialRun->GetPendingDifficulty()));
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

    UE_LOG(LogTemp, Log, TEXT("[BattleGameMode] Combat ended (%s, %d turns) - returning to trial"),
           *UEnum::GetValueAsString(Result.FinalState), Result.TotalTurns);

    // ExitEncounter defers its OpenLevel one tick internally - safe to call
    // from inside the result broadcast.
    if (UTrialRunSubsystem *TrialRun = GetGameInstance()->GetSubsystem<UTrialRunSubsystem>())
    {
        TrialRun->ExitEncounter();
    }
}
