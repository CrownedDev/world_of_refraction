// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/TurnManager.h"
#include "CharacterDataComponent.h"
#include "CharacterData.h"

void UTurnManager::InitializeCombat(const TArray<AActor *> &Team1, const TArray<AActor *> &Team2)
{
    UE_LOG(LogTemp, Log, TEXT("[TurnManager] Initializing combat: Team1=%d, Team2=%d"),
           Team1.Num(), Team2.Num());

    // Clear existing state
    Combatants.Empty();
    CurrentActor = nullptr;
    PreviousActor = nullptr;
    GlobalTurnCount = 0;

    // Add Team 1
    for (int32 i = 0; i < Team1.Num(); i++)
    {
        if (Team1[i])
        {
            FCombatantTurnDebt Combatant;
            Combatant.Actor = Team1[i];
            Combatant.TeamIndex = 0;
            Combatant.ArrayPosition = i;
            CacheActorStats(Combatant);
            Combatants.Add(Combatant);

            UE_LOG(LogTemp, Log, TEXT("[TurnManager]   Team1[%d]: %s (Speed=%d)"),
                   i, *Team1[i]->GetName(), Combatant.Speed);
        }
    }

    // Add Team 2
    for (int32 i = 0; i < Team2.Num(); i++)
    {
        if (Team2[i])
        {
            FCombatantTurnDebt Combatant;
            Combatant.Actor = Team2[i];
            Combatant.TeamIndex = 1;
            Combatant.ArrayPosition = i;
            CacheActorStats(Combatant);
            Combatants.Add(Combatant);

            UE_LOG(LogTemp, Log, TEXT("[TurnManager]   Team2[%d]: %s (Speed=%d)"),
                   i, *Team2[i]->GetName(), Combatant.Speed);
        }
    }

    // Calculate initial turn debts
    CalculateTurnDebts();

    // Start combat
    bCombatActive = true;

    // Advance to first turn
    AdvanceToNextTurn();
}

void UTurnManager::EndCombat()
{
    UE_LOG(LogTemp, Log, TEXT("[TurnManager] Combat ended after %d turns"), GlobalTurnCount);

    bCombatActive = false;
    Combatants.Empty();
    CurrentActor = nullptr;
    PreviousActor = nullptr;
    GlobalTurnCount = 0;

    OnCombatEnded.Broadcast();
}

void UTurnManager::AdvanceToNextTurn()
{
    if (!bCombatActive)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TurnManager] AdvanceToNextTurn called but combat not active"));
        return;
    }

    // Find next combatant
    FCombatantTurnDebt *NextCombatant = FindNextCombatant();

    if (!NextCombatant)
    {
        UE_LOG(LogTemp, Error, TEXT("[TurnManager] No valid combatant found!"));
        return;
    }

    // Increment their turns taken
    NextCombatant->TurnsTaken++;

    // Set as current
    PreviousActor = CurrentActor;
    CurrentActor = NextCombatant->Actor;
    GlobalTurnCount++;

    UE_LOG(LogTemp, Log, TEXT("[TurnManager] Turn %d: %s (Debt=%.2f, Taken=%d, NetDebt=%.2f)"),
           GlobalTurnCount,
           *CurrentActor->GetName(),
           NextCombatant->TurnsOwed,
           NextCombatant->TurnsTaken,
           NextCombatant->TurnsOwed - NextCombatant->TurnsTaken);

    // Broadcast turn started
    OnTurnStarted.Broadcast(CurrentActor, GlobalTurnCount);
}

void UTurnManager::EndCurrentTurn()
{
    if (!CurrentActor)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TurnManager] EndCurrentTurn called but no current actor"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[TurnManager] Ending turn for %s"), *CurrentActor->GetName());

    // Broadcast turn ended
    OnTurnEnded.Broadcast(CurrentActor, GlobalTurnCount);

    // Recalculate debts for next turn
    CalculateTurnDebts();
}

void UTurnManager::CalculateTurnDebts()
{
    // Find slowest speed (baseline for ratios)
    int32 SlowestSpeed = INT_MAX;
    for (const FCombatantTurnDebt &Combatant : Combatants)
    {
        if (Combatant.bIsAlive && Combatant.Speed > 0)
        {
            SlowestSpeed = FMath::Min(SlowestSpeed, Combatant.Speed);
        }
    }

    if (SlowestSpeed == INT_MAX || SlowestSpeed == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TurnManager] No valid speeds found, defaulting to 1"));
        SlowestSpeed = 1;
    }

    UE_LOG(LogTemp, Verbose, TEXT("[TurnManager] Calculating debts (SlowestSpeed=%d)"), SlowestSpeed);

    // Calculate speed ratio for each combatant
    for (FCombatantTurnDebt &Combatant : Combatants)
    {
        if (Combatant.bIsAlive)
        {
            float SpeedRatio = static_cast<float>(Combatant.Speed) / SlowestSpeed;
            Combatant.TurnsOwed += SpeedRatio;

            UE_LOG(LogTemp, Verbose, TEXT("[TurnManager]   %s: Speed=%d, Ratio=%.2f, Owed=%.2f, Taken=%d, Net=%.2f"),
                   *Combatant.Actor->GetName(),
                   Combatant.Speed,
                   SpeedRatio,
                   Combatant.TurnsOwed,
                   Combatant.TurnsTaken,
                   Combatant.TurnsOwed - Combatant.TurnsTaken);
        }
    }
}

FCombatantTurnDebt *UTurnManager::FindNextCombatant()
{
    TArray<FCombatantTurnDebt *> ValidCombatants;
    float HighestNetDebt = -FLT_MAX;

    // Find highest net debt
    for (FCombatantTurnDebt &Combatant : Combatants)
    {
        if (Combatant.bIsAlive)
        {
            float NetDebt = Combatant.TurnsOwed - Combatant.TurnsTaken;

            if (NetDebt > HighestNetDebt)
            {
                HighestNetDebt = NetDebt;
                ValidCombatants.Empty();
                ValidCombatants.Add(&Combatant);
            }
            else if (FMath::IsNearlyEqual(NetDebt, HighestNetDebt, 0.0001f))
            {
                ValidCombatants.Add(&Combatant);
            }
        }
    }

    if (ValidCombatants.Num() == 0)
    {
        return nullptr;
    }

    if (ValidCombatants.Num() == 1)
    {
        return ValidCombatants[0];
    }

    // Tie detected - resolve using cascade
    UE_LOG(LogTemp, Log, TEXT("[TurnManager] Tie detected: %d combatants with debt %.2f"),
           ValidCombatants.Num(), HighestNetDebt);

    return ResolveTie(ValidCombatants);
}

FCombatantTurnDebt *UTurnManager::ResolveTie(TArray<FCombatantTurnDebt *> &TiedCombatants)
{
    // Level 1: Turn Order Speed (primary)
    int32 HighestSpeed = -1;
    for (FCombatantTurnDebt *Combatant : TiedCombatants)
    {
        HighestSpeed = FMath::Max(HighestSpeed, Combatant->Speed);
    }

    TArray<FCombatantTurnDebt *> Remaining;
    for (FCombatantTurnDebt *Combatant : TiedCombatants)
    {
        if (Combatant->Speed == HighestSpeed)
        {
            Remaining.Add(Combatant);
        }
    }

    if (Remaining.Num() == 1)
    {
        UE_LOG(LogTemp, Log, TEXT("[TurnManager]   Tie-break Level 1 (Speed): %s"),
               *Remaining[0]->Actor->GetName());
        return Remaining[0];
    }

    TiedCombatants = Remaining;

    // Level 2: Attack Speed
    int32 HighestAttackSpeed = -1;
    for (FCombatantTurnDebt *Combatant : TiedCombatants)
    {
        HighestAttackSpeed = FMath::Max(HighestAttackSpeed, Combatant->AttackSpeed);
    }

    Remaining.Empty();
    for (FCombatantTurnDebt *Combatant : TiedCombatants)
    {
        if (Combatant->AttackSpeed == HighestAttackSpeed)
        {
            Remaining.Add(Combatant);
        }
    }

    if (Remaining.Num() == 1)
    {
        UE_LOG(LogTemp, Log, TEXT("[TurnManager]   Tie-break Level 2 (AttackSpeed): %s"),
               *Remaining[0]->Actor->GetName());
        return Remaining[0];
    }

    TiedCombatants = Remaining;

    // Level 3: Underdog Advantage (LOWER total stats wins)
    int32 LowestTotalStats = INT_MAX;
    for (FCombatantTurnDebt *Combatant : TiedCombatants)
    {
        LowestTotalStats = FMath::Min(LowestTotalStats, Combatant->TotalStats);
    }

    Remaining.Empty();
    for (FCombatantTurnDebt *Combatant : TiedCombatants)
    {
        if (Combatant->TotalStats == LowestTotalStats)
        {
            Remaining.Add(Combatant);
        }
    }

    if (Remaining.Num() == 1)
    {
        UE_LOG(LogTemp, Log, TEXT("[TurnManager]   Tie-break Level 3 (Underdog): %s"),
               *Remaining[0]->Actor->GetName());
        return Remaining[0];
    }

    TiedCombatants = Remaining;

    // Level 4: World Body (higher wins)
    int32 HighestBody = -1;
    for (FCombatantTurnDebt *Combatant : TiedCombatants)
    {
        HighestBody = FMath::Max(HighestBody, Combatant->WorldBody);
    }

    Remaining.Empty();
    for (FCombatantTurnDebt *Combatant : TiedCombatants)
    {
        if (Combatant->WorldBody == HighestBody)
        {
            Remaining.Add(Combatant);
        }
    }

    if (Remaining.Num() == 1)
    {
        UE_LOG(LogTemp, Log, TEXT("[TurnManager]   Tie-break Level 4 (WorldBody): %s"),
               *Remaining[0]->Actor->GetName());
        return Remaining[0];
    }

    TiedCombatants = Remaining;

    // Level 5: World Mind (higher wins)
    int32 HighestMind = -1;
    for (FCombatantTurnDebt *Combatant : TiedCombatants)
    {
        HighestMind = FMath::Max(HighestMind, Combatant->WorldMind);
    }

    Remaining.Empty();
    for (FCombatantTurnDebt *Combatant : TiedCombatants)
    {
        if (Combatant->WorldMind == HighestMind)
        {
            Remaining.Add(Combatant);
        }
    }

    if (Remaining.Num() == 1)
    {
        UE_LOG(LogTemp, Log, TEXT("[TurnManager]   Tie-break Level 5 (WorldMind): %s"),
               *Remaining[0]->Actor->GetName());
        return Remaining[0];
    }

    TiedCombatants = Remaining;

    // Level 6: World Spirit (higher wins)
    int32 HighestSpirit = -1;
    for (FCombatantTurnDebt *Combatant : TiedCombatants)
    {
        HighestSpirit = FMath::Max(HighestSpirit, Combatant->WorldSpirit);
    }

    Remaining.Empty();
    for (FCombatantTurnDebt *Combatant : TiedCombatants)
    {
        if (Combatant->WorldSpirit == HighestSpirit)
        {
            Remaining.Add(Combatant);
        }
    }

    if (Remaining.Num() == 1)
    {
        UE_LOG(LogTemp, Log, TEXT("[TurnManager]   Tie-break Level 6 (WorldSpirit): %s"),
               *Remaining[0]->Actor->GetName());
        return Remaining[0];
    }

    TiedCombatants = Remaining;

    // Level 7: Team + Array Position (deterministic)
    // Team 1 goes first, then by array position within team
    FCombatantTurnDebt *Winner = TiedCombatants[0];
    for (FCombatantTurnDebt *Combatant : TiedCombatants)
    {
        if (Combatant->TeamIndex < Winner->TeamIndex ||
            (Combatant->TeamIndex == Winner->TeamIndex && Combatant->ArrayPosition < Winner->ArrayPosition))
        {
            Winner = Combatant;
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[TurnManager]   Tie-break Level 7 (Team/Position): %s (Team=%d, Pos=%d)"),
           *Winner->Actor->GetName(), Winner->TeamIndex, Winner->ArrayPosition);

    return Winner;
}

void UTurnManager::CacheActorStats(FCombatantTurnDebt &Combatant)
{
    UCharacterDataComponent *CharComp = Combatant.Actor->FindComponentByClass<UCharacterDataComponent>();

    if (!CharComp || !CharComp->CharacterData)
    {
        UE_LOG(LogTemp, Error, TEXT("[TurnManager] Actor %s missing CharacterDataComponent!"),
               *Combatant.Actor->GetName());

        // Set defaults to prevent crashes
        Combatant.Speed = 1;
        Combatant.AttackSpeed = 1;
        Combatant.TotalStats = 3;
        Combatant.WorldBody = 1;
        Combatant.WorldMind = 1;
        Combatant.WorldSpirit = 1;
        return;
    }

    UCharacterData *CharData = CharComp->CharacterData;

    // Speed = WorldBodyLevel + TurnSpeed sub-stat points
    Combatant.WorldBody = CharData->WorldBodyLevel;
    Combatant.Speed = Combatant.WorldBody + CharData->GetTotalTurnSpeed();

    // Attack Speed = WorldBodyLevel + AttackSpeed sub-stat points
    Combatant.AttackSpeed = Combatant.WorldBody + CharData->GetTotalAttackSpeed();

    // World stats for tie-breaking
    Combatant.WorldMind = CharData->WorldMindLevel;
    Combatant.WorldSpirit = CharData->WorldSpiritLevel;

    // Total stats (for underdog advantage)
    Combatant.TotalStats = CharData->WorldMindLevel + CharData->WorldBodyLevel + CharData->WorldSpiritLevel;

    UE_LOG(LogTemp, Verbose, TEXT("[TurnManager] Cached stats for %s: Speed=%d, AttackSpeed=%d, Total=%d"),
           *Combatant.Actor->GetName(), Combatant.Speed, Combatant.AttackSpeed, Combatant.TotalStats);
}

FCombatantTurnDebt *UTurnManager::FindCombatantByActor(AActor *Actor)
{
    for (FCombatantTurnDebt &Combatant : Combatants)
    {
        if (Combatant.Actor == Actor)
        {
            return &Combatant;
        }
    }
    return nullptr;
}

void UTurnManager::OnActorSpeedChanged(AActor *AffectedActor)
{
    FCombatantTurnDebt *Combatant = FindCombatantByActor(AffectedActor);

    if (!Combatant)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TurnManager] OnActorSpeedChanged: Actor not found"));
        return;
    }

    int32 OldSpeed = Combatant->Speed;

    // Recache stats (picks up new speed value)
    CacheActorStats(*Combatant);

    UE_LOG(LogTemp, Log, TEXT("[TurnManager] Speed changed: %s (%d -> %d)"),
           *AffectedActor->GetName(), OldSpeed, Combatant->Speed);

    // Recalculate all debts (speed change affects ratios)
    CalculateTurnDebts();

    // Broadcast event
    OnSpeedChanged.Broadcast(AffectedActor);
}

void UTurnManager::OnActorDied(AActor *DeadActor)
{
    FCombatantTurnDebt *Combatant = FindCombatantByActor(DeadActor);

    if (!Combatant)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TurnManager] OnActorDied: Actor not found"));
        return;
    }

    Combatant->bIsAlive = false;

    UE_LOG(LogTemp, Log, TEXT("[TurnManager] Actor died: %s (will skip turns)"),
           *DeadActor->GetName());

    // If current actor died mid-turn, advance immediately
    if (CurrentActor == DeadActor)
    {
        UE_LOG(LogTemp, Log, TEXT("[TurnManager]   Advancing turn (current actor died)"));
        EndCurrentTurn();
        AdvanceToNextTurn();
    }
}

void UTurnManager::OnActorResurrected(AActor *ResurrectedActor)
{
    FCombatantTurnDebt *Combatant = FindCombatantByActor(ResurrectedActor);

    if (!Combatant)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TurnManager] OnActorResurrected: Actor not found"));
        return;
    }

    Combatant->bIsAlive = true;

    UE_LOG(LogTemp, Log, TEXT("[TurnManager] Actor resurrected: %s (will resume turns)"),
           *ResurrectedActor->GetName());

    // Recalculate debts (they're back in rotation)
    CalculateTurnDebts();
}

TArray<AActor *> UTurnManager::GetAllCombatants() const
{
    TArray<AActor *> Result;
    for (const FCombatantTurnDebt &Combatant : Combatants)
    {
        Result.Add(Combatant.Actor);
    }
    return Result;
}

TArray<AActor *> UTurnManager::GetTurnOrderPreview(int32 NumTurns)
{
    // Create a copy to simulate without affecting actual state
    TArray<FCombatantTurnDebt> SimulatedCombatants = Combatants;
    TArray<AActor *> Preview;

    for (int32 i = 0; i < NumTurns; i++)
    {
        // Find highest net debt (same logic as FindNextCombatant)
        FCombatantTurnDebt *NextCombatant = nullptr;
        float HighestNetDebt = -FLT_MAX;

        for (FCombatantTurnDebt &Combatant : SimulatedCombatants)
        {
            if (Combatant.bIsAlive)
            {
                float NetDebt = Combatant.TurnsOwed - Combatant.TurnsTaken;
                if (NetDebt > HighestNetDebt)
                {
                    HighestNetDebt = NetDebt;
                    NextCombatant = &Combatant;
                }
            }
        }

        if (!NextCombatant)
        {
            break;
        }

        // Add to preview
        Preview.Add(NextCombatant->Actor);

        // Simulate turn taken
        NextCombatant->TurnsTaken++;

        // Simulate debt recalculation
        int32 SlowestSpeed = INT_MAX;
        for (const FCombatantTurnDebt &Combatant : SimulatedCombatants)
        {
            if (Combatant.bIsAlive && Combatant.Speed > 0)
            {
                SlowestSpeed = FMath::Min(SlowestSpeed, Combatant.Speed);
            }
        }

        if (SlowestSpeed > 0)
        {
            for (FCombatantTurnDebt &Combatant : SimulatedCombatants)
            {
                if (Combatant.bIsAlive)
                {
                    float SpeedRatio = static_cast<float>(Combatant.Speed) / SlowestSpeed;
                    Combatant.TurnsOwed += SpeedRatio;
                }
            }
        }
    }

    return Preview;
}

void UTurnManager::DebugPrintTurnOrder()
{
    UE_LOG(LogTemp, Display, TEXT("========================================"));
    UE_LOG(LogTemp, Display, TEXT("TURN ORDER (Next 10 Turns)"));
    UE_LOG(LogTemp, Display, TEXT("========================================"));

    TArray<AActor *> Preview = GetTurnOrderPreview(10);

    for (int32 i = 0; i < Preview.Num(); i++)
    {
        UE_LOG(LogTemp, Display, TEXT("  %d. %s"), i + 1, *Preview[i]->GetName());
    }

    UE_LOG(LogTemp, Display, TEXT("========================================"));
}

void UTurnManager::DebugPrintDebtDetails()
{
    UE_LOG(LogTemp, Display, TEXT("========================================"));
    UE_LOG(LogTemp, Display, TEXT("TURN DEBT DETAILS (Turn %d)"), GlobalTurnCount);
    UE_LOG(LogTemp, Display, TEXT("========================================"));

    for (const FCombatantTurnDebt &Combatant : Combatants)
    {
        float NetDebt = Combatant.TurnsOwed - Combatant.TurnsTaken;

        UE_LOG(LogTemp, Display, TEXT("%s:"), *Combatant.Actor->GetName());
        UE_LOG(LogTemp, Display, TEXT("  Speed: %d"), Combatant.Speed);
        UE_LOG(LogTemp, Display, TEXT("  Owed: %.2f"), Combatant.TurnsOwed);
        UE_LOG(LogTemp, Display, TEXT("  Taken: %d"), Combatant.TurnsTaken);
        UE_LOG(LogTemp, Display, TEXT("  Net Debt: %.2f"), NetDebt);
        UE_LOG(LogTemp, Display, TEXT("  Alive: %s"), Combatant.bIsAlive ? TEXT("Yes") : TEXT("No"));
    }

    UE_LOG(LogTemp, Display, TEXT("========================================"));
}

void UTurnManager::Debug_ShowState()
{
    if (!bCombatActive)
    {
        UE_LOG(LogTemp, Display, TEXT("[TurnManager] Combat not active"));
        return;
    }

    UE_LOG(LogTemp, Display, TEXT("========================================"));
    UE_LOG(LogTemp, Display, TEXT("TURN MANAGER STATE"));
    UE_LOG(LogTemp, Display, TEXT("========================================"));
    UE_LOG(LogTemp, Display, TEXT("Current Turn: %d"), GlobalTurnCount);
    UE_LOG(LogTemp, Display, TEXT("Current Actor: %s"), CurrentActor ? *CurrentActor->GetName() : TEXT("None"));
    UE_LOG(LogTemp, Display, TEXT("Combatants: %d"), Combatants.Num());
    UE_LOG(LogTemp, Display, TEXT("========================================"));

    DebugPrintDebtDetails();
    DebugPrintTurnOrder();
}