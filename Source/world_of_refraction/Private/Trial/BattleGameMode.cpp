// BattleGameMode.cpp

#include "Trial/BattleGameMode.h"

#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Grid/CombatGridSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
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
    if (!PlayerPawnClass || !EnemyPawnClass || !LoadedOrchestratorClass)
    {
        AbortToTrial(TEXT("DefaultPlayerPawnClass / DefaultEnemyPawnClass / OrchestratorClass not all set"));
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
    const TArray<UCharacterData *> LocalPartyData = TrialRun->GetPendingLocalParty();
    const TArray<UCharacterData *> OpposingPartyData = TrialRun->GetPendingOpposingParty();
    for (int32 i = 0; i < LocalPartyData.Num(); ++i)
    {
        const FVector Location = Origin + FVector(-BattleGameModeConstants::TEAM_X_OFFSET,
                                                  i * BattleGameModeConstants::COMBATANT_Y_SPACING, 0.0f);
        if (APawn *Spawned = SpawnCombatant(PlayerPawnClass, LocalPartyData[i], FTransform(Location)))
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
                                     LocalParty.Num(), LocalPartyData.Num(), OpposingParty.Num(), OpposingPartyData.Num()));
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
        APawn *PlayerPawn = CastChecked<APawn>(LocalParty[0]);
        APawn *PreviousPawn = PC->GetPawn();

        // Capture the auto-spawned AI controller before Possess detaches it —
        // AController::OnPossess unpossesses the incumbent but does not destroy
        // it, leaving a pawnless controller behind.
        AController *DisplacedAI = PlayerPawn->GetController();

        PC->Possess(PlayerPawn);

        if (DisplacedAI && DisplacedAI != PC)
        {
            DisplacedAI->Destroy();
        }
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
