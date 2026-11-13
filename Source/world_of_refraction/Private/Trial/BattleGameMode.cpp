// BattleGameMode.cpp

#include "Trial/BattleGameMode.h"
#include "Combat/WoRFightSubsystem.h"
#include "Combat/Damage/WoRHitKind.h"
#include "Skills/Effects/StatusBuildupManager.h"

#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CombatCharacter.h"
#include "Character/Forms/CharacterFormSubsystem.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/Boss/BossPhaseComponent.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"
#include "Combat/CombatControl.h"
#include "Combat/Grid/CombatGridSubsystem.h"
#include "Engine/GameInstance.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/EquipmentDataBase.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Loadout/LoadoutComponent.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "TimerManager.h"
#include "Combat/Encounter/EnemyZone.h"
#include "Combat/Boss/BossTriggers.h"
#include "Memory/MemoryConditions.h"
#include "Memory/MemoryUnlock.h"
#include "Memory/HiddenCounterSubsystem.h"
#include "Combat/Defense/DefenseSystem.h"
#include "Combat/Defense/GuardRules.h"
#include "Reaction/FightTriggers.h"
#include "Reaction/ReactionTrigger.h"
#include "Pool/PoolSubsystem.h"
#include "Trial/DefeatRules.h"
#include "Trial/TrialGate.h"
#include "Trial/TrialRunSubsystem.h"
#include "Save/SaveSubsystem.h"
#include "Currency/MentorTokens.h"
#include "UI/Combat/CombatResultsWidget.h"
#include "GameFramework/PlayerController.h"
#include "Player/WoRGameState.h"
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
    // B-227: a listen server's level change keeps its clients (seamless travel; CopyProperties).
    GameStateClass = AWoRGameState::StaticClass();
    bUseSeamlessTravel = true;
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
    TArray<EEncounterRank> OpposingRanks; // B-224b: each spawned enemy's own rank for its HP, kept paired with OpposingParty
    TArray<int32> OpposingTierSteps; // B-187b: each spawned enemy's tier steps (a revived mini-boss's team), paired the same way

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
        // B-360b: two copies of one character come as two equal entries; the n-th entry is the n-th member with that asset.
        int32 Occurrence = 0;
        for (int32 Earlier = 0; Earlier < i; ++Earlier)
        {
            Occurrence += BattlePartyData[Earlier] == BattlePartyData[i] ? 1 : 0;
        }
        const int32 Slot = PartySession ? PartySession->GetNthMemberSlotByData(BattlePartyData[i], Occurrence, /*bLivingOnly=*/true) : INDEX_NONE;
        const FPartyMember *Member = TrialParty && TrialParty->Members.IsValidIndex(Slot) ? &TrialParty->Members[Slot] : nullptr;
        UClass *MemberPawnClass = Member ? Member->PawnClass.LoadSynchronous() : nullptr;

        const FVector Location = Origin + FVector(-BattleGameModeConstants::TEAM_X_OFFSET,
                                                  i * BattleGameModeConstants::COMBATANT_Y_SPACING, 0.0f);
        APawn *Spawned = SpawnCombatant(MemberPawnClass ? MemberPawnClass : PlayerPawnClass, BattlePartyData[i], FTransform(Location),
                                        Member ? Member->MemberId : FGuid());
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
        // B-346 (Q-21-audit): you command every member but the AI seats the leader set before the fight.
        const bool bPlayerControlled = !Member || !Member->bAISeat;
        CombatControl::SetPlayerDriven(Spawned, bPlayerControlled);
        if (Member && Member->bAISeat)
        {
            CombatControl::SetAISeatDifficulty(Spawned, Member->AISeatDifficulty);
        }
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
    // B-279d: an authored character fought as a mini-boss wears its form (any enemy whose character has one, §1b).
    const UCharacterFormSubsystem *Forms = GetGameInstance()->GetSubsystem<UCharacterFormSubsystem>();
    for (int32 i = 0; i < OpposingPartyData.Num(); ++i)
    {
        const FVector Location = Origin + FVector(BattleGameModeConstants::TEAM_X_OFFSET,
                                                  i * BattleGameModeConstants::COMBATANT_Y_SPACING, 0.0f);
        if (APawn *Spawned = SpawnCombatant(EnemyPawnClass, OpposingPartyData[i], FTransform(Location)))
        {
            OpposingParty.Add(Spawned);
            OpposingRanks.Add(TrialRun->GetPendingEnemyRank(i));
            OpposingTierSteps.Add(TrialRun->GetPendingEnemyTierSteps(i)); // B-187b review: its own team's revivals
            const UCharacterFormData *Form = Forms ? Forms->FindFormForCharacter(OpposingPartyData[i]) : nullptr;
            const ACharacter *FormPawn = Cast<ACharacter>(Spawned);
            if (Form && FormPawn && FormPawn->GetMesh() && Form->DressMesh(*FormPawn->GetMesh()))
            {
                UE_LOG(LogTemp, Log, TEXT("[BattleGameMode] %s fights as form %s"), *GetNameSafe(OpposingPartyData[i]), *Form->GetName());
            }
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
    if (UCombatGridSubsystem *Grid = WoRFight::Get<UCombatGridSubsystem>(this))
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
    // B-293c (Q-293c): the door's difficulty scales enemy damage only. D-120 (B-327): the zone scales nothing; it
    // only decides which enemies are placed there, and their strength is their own Combat Tier (rank x tier below).
    const float EnemyStrength = TrialRun->GetEnemyStrengthMultiplier();
    const float EnemyHealth = TrialGate::GetEnemyHealthMultiplier(TrialRun->GetTrialDifficulty());
    for (int32 EnemyIndex = 0; EnemyIndex < OpposingParty.Num(); ++EnemyIndex)
    {
        AActor *Enemy = OpposingParty[EnemyIndex];
        // B-224 / B-224b: the enemy's own rank (its team's; Q-224c) and tier multiply max HP on top.
        const EEncounterRank EnemyRank = OpposingRanks.IsValidIndex(EnemyIndex) ? OpposingRanks[EnemyIndex] : TrialRun->GetPendingRank();
        SpawnedOrchestrator->SetEnemyRank(Enemy, EnemyRank); // B-223: its level drop
        const int32 EnemyTierSteps = OpposingTierSteps.IsValidIndex(EnemyIndex) ? OpposingTierSteps[EnemyIndex] : 0;
        SpawnedOrchestrator->SetEnemyTierSteps(Enemy, EnemyTierSteps); // B-187b review: its drops at the stepped-up tier
        if (!TrialGate::ApplyEnemyStrength(Enemy, EnemyHealth, EnemyStrength))
        {
            UE_LOG(LogTemp, Warning, TEXT("[BattleGameMode] %s: trial difficulty not applied (no character data or effect manager)"),
                   *GetNameSafe(Enemy));
        }
        else if (!TrialGate::ApplyEnemyRankHealth(Enemy, EnemyHealth, EnemyRank, EnemyTierSteps)) // B-187b: a revived mini-boss
        {
            UE_LOG(LogTemp, Warning, TEXT("[BattleGameMode] %s: rank health (%s) not applied"),
                   *GetNameSafe(Enemy), *UEnum::GetValueAsString(EnemyRank));
        }
    }
    UE_LOG(LogTemp, Log, TEXT("[BattleGameMode] Trial difficulty %s, zone tier %d: enemy HP x%.2f, damage x%.2f"),
           *TrialGate::GetDifficultyName(TrialRun->GetTrialDifficulty()).ToString(), Zone.Tier, EnemyHealth, EnemyStrength);

    // B-191: reactions (worn items facing a character) and the bosses' own triggers change the
    // fight before it starts; B-212: those with a mid-fight When wait in FightTriggers.
    const TArray<FReactionTrigger> Reactions = ReactionTriggers::GatherTriggers();
    FightTriggers = NewObject<UFightTriggers>(this);
    const TArray<FFiredReaction> Fired = FightTriggers->StartReactions(Reactions, LocalParty, OpposingParty);
    const int32 ReactionEffects = ReactionTriggers::ApplyFired(Fired);
    // B-209: fired reactions and worn bound gear whose conditions hold unlock their memories (since
    // B-211 memories no longer change bosses).
    MemoryUnlock::UnlockAtFightStart(GetGameInstance(), Fired, LocalParty, OpposingParty);
    // B-193: any opposing combatant with boss data gets its phases: after reactions change its
    // stats (so they can't reach a phase early), before boss triggers skip or add phases.
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

    // B-211 (D-100 part 1): each boss's own triggers change it; memories no longer do.
    const int32 BossTriggersFired = FightTriggers->StartBosses(LocalParty, OpposingParty);
    UE_LOG(LogTemp, Log, TEXT("[BattleGameMode] Reactions: %d of %d fired (%d effects); boss triggers fired: %d"),
           Fired.Num(), Reactions.Num(), ReactionEffects, BossTriggersFired);

    SpawnedOrchestrator->SetEncounterRank(TrialRun->GetPendingRank());
    // B-185: the side that struck first on the map takes the opening round.
    SpawnedOrchestrator->SetFirstStrike(TrialRun->GetPendingFirstStrike());
    UE_LOG(LogTemp, Log, TEXT("[BattleGameMode] First strike: %s"), *UEnum::GetValueAsString(TrialRun->GetPendingFirstStrike()));
    // B-209: a NoDamageTaken memory condition compares against this at the win.
    PartyStartHP.Reset();
    for (const AActor *Member : LocalParty)
    {
        if (const UCharacterDataComponent *MemberData = Member ? Member->FindComponentByClass<UCharacterDataComponent>() : nullptr)
        {
            PartyStartHP.Add(Member, MemberData->CurrentHP);
        }
    }
    FightUsage.Reset();
    WatchFightUsage(true);
    FightTriggers->Watch(true);
    SpawnedOrchestrator->StartCombat(LocalParty, OpposingParty, TrialRun->GetPendingDifficulty());
    ApplyFieldStrike(*TrialRun, LocalParty, OpposingParty); // B-214f
}

void ABattleGameMode::ApplyFieldStrike(UTrialRunSubsystem &TrialRun, const TArray<AActor *> &LocalParty,
                                       const TArray<AActor *> &OpposingParty)
{
    // B-214f (Q-214f): a field hit started this fight. The party already strikes first (the pending first strike);
    // here every enemy of the struck team takes the hit's buildup, as if hit by the party's first member.
    const FFieldStrike Strike = TrialRun.GetPendingFieldStrike();
    TrialRun.SetPendingFieldStrike(FFieldStrike());
    UStatusBuildupManager *Buildup = WoRFight::Get<UStatusBuildupManager>(this);
    const bool bCarriesSomething = Strike.Element != ESpellElement::None || Strike.Physical != EPhysicalDamageType::None;
    if (!Strike.IsSet() || !bCarriesSomething || !Buildup || LocalParty.Num() == 0)
    {
        return;
    }
    const FWoRHitKind Hit = FWoRHitKind::FromPair(Strike.Element, Strike.Physical, Strike.bIce ? EWoRDamageType::Ice : EWoRDamageType::None);
    int32 Struck = 0;
    for (AActor *Enemy : OpposingParty)
    {
        if (Enemy && Buildup->AddStatusBuildupForHit(LocalParty[0], Enemy, Strike.Buildup, Hit))
        {
            ++Struck;
        }
    }
    UE_LOG(LogTemp, Log, TEXT("[BattleGameMode] Field strike: %s%s / %s buildup %.0f on %d of %d enemies"),
           *UEnum::GetValueAsString(Strike.Element), Strike.bIce ? TEXT(" (Ice)") : TEXT(""),
           *UEnum::GetValueAsString(Strike.Physical), Strike.Buildup, Struck, OpposingParty.Num());
}

APawn *ABattleGameMode::SpawnCombatant(UClass *PawnClass, UCharacterData *Data, const FTransform &SpawnTransform, const FGuid &MemberId)
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
        CharComp->PartyMemberId = MemberId; // B-360b: before BeginPlay, which reads the member's slot
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
    WatchFightUsage(false);
    if (FightTriggers)
    {
        FightTriggers->Watch(false);
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

                // B-360b: by the member id stamped at spawn, so two copies of one character each bank their own.
                const int32 SlotIndex = PartySession->GetMemberSlotOf(CharComp);
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
                    // B-348c (Q-348b): Broken Darkness stays on the member until reverted; strain carries through the run.
                    PartySession->SetMemberBrokenDarkness(SlotIndex, CharComp->IsBrokenDarkness(), BDManager ? BDManager->GetAccruedStrain() : 0.0f);
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
            // B-073 (D-006): a win pays run Prisms now and adds to the run's tally (by the fight's rank).
            TrialRun->RecordFightReward(GetWorld(), TrialRun->GetPendingRank());
            TrialRun->MarkPendingEncounterBeaten();
            // B-362c (D-160 changed): a win no longer recovers the dropped Prisms; the pickup on the map does
            // (APrismDropPickup). WoRDefeat::RecoverPrisms is OBSOLETE.
            // B-361b (D-154, Q-361b): a boss beaten with another player pays every player a Mentor Token.
            if (TrialRun->GetPendingRank() == EEncounterRank::Boss)
            {
                TrialRun->MarkRunBossBeaten();
                WoRMentorTokens::PayPlayersTogether(GetWorld(), MentorTokenRules::PER_BOSS_TOGETHER, TEXT("a boss beaten together"));
            }
            // B-209: a beaten boss or mini-boss unlocks its memory, and memories whose unlock
            // conditions the win meets.
            CheckFightWonMemories();
        }
    }

    // B-362 (D-155, D-160): a wipe drops the run's Prisms here and costs a second chance; the run may be over.
    if (Result.FinalState == ECombatState::Defeat)
    {
        ApplyWipe();
    }

    if (!ShowResults(Result))
    {
        ReturnToTrial();
    }
}

void ABattleGameMode::ApplyWipe()
{
    const UTrialRunSubsystem *TrialRun = GetGameInstance()->GetSubsystem<UTrialRunSubsystem>();
    if (!TrialRun || !Orchestrator.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleGameMode] Wipe: no trial run or orchestrator - nothing taken"));
        return;
    }
    TArray<FName> MemberKeys;
    for (const AActor *Member : Orchestrator->GetLocalParty())
    {
        const UCharacterDataComponent *MemberData = Member ? Member->FindComponentByClass<UCharacterDataComponent>() : nullptr;
        // Rostered members only (a guest or summon is not the party), keyed as their levels are (B-360c); logged by the wipe.
        if (MemberData && MemberData->FindCarriedPartyMember())
        {
            MemberKeys.Add(UPoolSubsystem::GetCharacterKey(MemberData));
        }
    }
    // TODO(D-103 co-op): GetLocalParty is the server's side; every player's members should pay once co-op fights land.
    bWipeEndsRun = WoRDefeat::ApplyWipe(GetWorld(), TrialRun->GetPendingRank(), MemberKeys).bRunEnds;
    // Save the loss now: the next save point is leaving the results screen, and quitting there would undo the wipe.
    if (USaveSubsystem *Save = GetGameInstance()->GetSubsystem<USaveSubsystem>())
    {
        Save->SaveNow(GetWorld(), true);
    }
}

void ABattleGameMode::CheckFightWonMemories()
{
    const UTrialRunSubsystem *TrialRun = GetGameInstance()->GetSubsystem<UTrialRunSubsystem>();
    if (!TrialRun || !Orchestrator.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleGameMode] Fight won: no trial run or orchestrator - memory conditions not checked"));
        return;
    }
    FMemoryEvent Event = MemoryConditions::MakeEvent(GetGameInstance(), EMemorySource::FightWon);
    Event.Party.Reset();
    for (const AActor *Member : Orchestrator->GetLocalParty())
    {
        FMemoryEventMember &Added = Event.Party.Add_GetRef(MemoryConditions::MakeMember(Member));
        if (const FMemoryFightUsage *Used = FightUsage.Find(Member))
        {
            Added.Used = *Used;
            UE_LOG(LogTemp, Log, TEXT("[BattleGameMode] %s used: %s"), *Member->GetName(), *GetFightUsageString(*Used));
        }
    }
    for (UCharacterData *Beaten : TrialRun->GetPendingOpposingParty())
    {
        Event.Beaten.Add(Beaten);
        const UBossData *BossData = UBossData::FindFor(Beaten);
        if (BossData && BossData->MemoryOnDefeat)
        {
            Event.Named.AddUnique(BossData->MemoryOnDefeat);
        }
    }
    Event.bNoDamageTaken = WasPartyUnhurt();
    // The bosses' own memories first, journalled as from a boss; then every conditional memory
    // the win meets, as from the fight.
    FMemoryEvent BossEvent = Event;
    BossEvent.Source = EMemorySource::Boss;
    MemoryConditions::CheckEvent(GetGameInstance(), BossEvent, {});
    Event.Named.Reset();
    MemoryConditions::CheckEvent(GetGameInstance(), Event);
}

void ABattleGameMode::HandleActionStarted(AActor *Executor, const FAction &Action, int32 EnergyCost)
{
    // The party's own use only: PartyStartHP holds the player side for this fight.
    if (PartyStartHP.Contains(Executor))
    {
        MemoryConditions::RecordAction(FightUsage.FindOrAdd(Executor), Executor, Action);
    }
}

void ABattleGameMode::HandleTargetKilled(AActor *Killer, AActor *Victim)
{
    // A finishing blow is on an enemy, not a party member.
    if (PartyStartHP.Contains(Killer) && !PartyStartHP.Contains(Victim))
    {
        MemoryConditions::RecordKill(FightUsage.FindOrAdd(Killer), Killer);
        AddHiddenCount(Killer, HiddenCounters::KILLS); // B-379
    }
}

void ABattleGameMode::HandleDefenseResolved(AActor *Defender, AActor * /*Attacker*/, EDefenseType DefenseType, bool bPerfect,
                                            int32 /*ImpactIndex*/, float /*AttackEnergyCost*/)
{
    // D-084: a Guard on Perfect timing is a parry. The party's own only.
    if (PartyStartHP.Contains(Defender) && GuardRules::IsPerfectGuard(DefenseType, bPerfect))
    {
        AddHiddenCount(Defender, HiddenCounters::PARRIES);
    }
}

void ABattleGameMode::AddHiddenCount(const AActor *Member, const TCHAR *Counter)
{
    const UCharacterDataComponent *Data = Member ? Member->FindComponentByClass<UCharacterDataComponent>() : nullptr;
    const UCharacterData *Character = Data ? Data->CharacterData : nullptr;
    if (!Character)
    {
        return;
    }
    // The count is the player's own (their save): a co-op friend's character's goes to their machine.
    if (AWoRPlayerState *Friend = MemoryUnlock::FindRemotePlayerOf(GetGameInstance(), Character))
    {
        Friend->Client_AddCounter(FName(Counter), const_cast<UCharacterData *>(Character));
        return;
    }
    const UGameInstance *GI = GetGameInstance(); // B-379 review: a bare test world has none
    if (UHiddenCounterSubsystem *Counters = GI ? GI->GetSubsystem<UHiddenCounterSubsystem>() : nullptr)
    {
        Counters->Add(FName(Counter), Character);
    }
}

void ABattleGameMode::WatchFightUsage(bool bWatch)
{
    UActionExecutor *Executor = WoRFight::Get<UActionExecutor>(this);
    if (!Executor)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleGameMode] No action executor - skill and weapon use won't count for memories"));
        return;
    }
    UDefenseSystem *Defense = WoRFight::Get<UDefenseSystem>(this); // B-379: parries for the hidden counters
    if (bWatch)
    {
        Executor->OnActionStarted.AddUniqueDynamic(this, &ABattleGameMode::HandleActionStarted);
        Executor->OnTargetKilled.AddUniqueDynamic(this, &ABattleGameMode::HandleTargetKilled);
        if (Defense)
        {
            Defense->OnDefenseResolved.AddUniqueDynamic(this, &ABattleGameMode::HandleDefenseResolved);
        }
    }
    else
    {
        Executor->OnActionStarted.RemoveDynamic(this, &ABattleGameMode::HandleActionStarted);
        Executor->OnTargetKilled.RemoveDynamic(this, &ABattleGameMode::HandleTargetKilled);
        if (Defense)
        {
            Defense->OnDefenseResolved.RemoveDynamic(this, &ABattleGameMode::HandleDefenseResolved);
        }
    }
}

void ABattleGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    WatchFightUsage(false);
    if (FightTriggers)
    {
        FightTriggers->Watch(false);
    }
    Super::EndPlay(EndPlayReason);
}

FString ABattleGameMode::GetFightUsageString(const FMemoryFightUsage &Usage)
{
    const auto Names = [](const auto &Objects)
    {
        return Objects.IsEmpty() ? FString(TEXT("none")) : FString::JoinBy(Objects, TEXT(", "), [](const UObject *Object) { return GetNameSafe(Object); });
    };
    return FString::Printf(TEXT("skills: %s | Breakthroughs: %s | finishing weapons: %s"), *Names(Usage.Skills),
                           *Names(Usage.Breakthroughs), *Names(Usage.FinishingWeapons));
}

bool ABattleGameMode::WasPartyUnhurt() const
{
    for (const TPair<TWeakObjectPtr<const AActor>, int32> &Start : PartyStartHP)
    {
        const AActor *Member = Start.Key.Get();
        const UCharacterDataComponent *MemberData = Member ? Member->FindComponentByClass<UCharacterDataComponent>() : nullptr;
        if (!MemberData || MemberData->CurrentHP < Start.Value)
        {
            return false;
        }
    }
    return !PartyStartHP.IsEmpty();
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
        if (bWipeEndsRun)
        {
            UE_LOG(LogTemp, Log, TEXT("[BattleGameMode] The run is over (D-155): back to the hub"));
            TrialRun->ExitTrialNextTick(); // B-362: 3 main-boss losses or 5 wipes; deferred like ExitEncounter (mid-teardown)
        }
        else
        {
            TrialRun->ExitEncounter();
        }
    }
}
