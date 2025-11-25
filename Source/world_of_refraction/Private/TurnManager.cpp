// Copyright Epic Games, Inc. All Rights Reserved.

#include "TurnManager.h"
#include "CharacterDataComponent.h"
#include "CharacterData.h"

UTurnManager::UTurnManager()
{
    bCombatActive = false;
    GlobalTurnCount = 0;
    CurrentActor = nullptr;
    PreviousActor = nullptr;
}

void UTurnManager::InitializeCombat(const TArray<AActor*>& Team1, const TArray<AActor*>& Team2)
{
    if (bCombatActive)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TurnManager] Combat already active, ending previous combat"));
        EndCombat();
    }

    Combatants.Empty();
    GlobalTurnCount = 0;
    CurrentActor = nullptr;
    PreviousActor = nullptr;

    // Add Team 1
    for (int32 i = 0; i < Team1.Num(); i++)
    {
        if (Team1[i])
        {
            FCombatantTurnDebt NewCombatant;
            NewCombatant.Actor = Team1[i];
            NewCombatant.TeamIndex = 0;
            NewCombatant.PositionInTeam = i;
            NewCombatant.TurnsOwed = 0.0f;
            NewCombatant.TurnsTaken = 0;
            CacheActorStats(NewCombatant);
            Combatants.Add(NewCombatant);
        }
    }

    // Add Team 2
    for (int32 i = 0; i < Team2.Num(); i++)
    {
        if (Team2[i])
        {
            FCombatantTurnDebt NewCombatant;
            NewCombatant.Actor = Team2[i];
            NewCombatant.TeamIndex = 1;
            NewCombatant.PositionInTeam = i;
            NewCombatant.TurnsOwed = 0.0f;
            NewCombatant.TurnsTaken = 0;
            CacheActorStats(NewCombatant);
            Combatants.Add(NewCombatant);
        }
    }

    if (Combatants.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[TurnManager] No valid combatants provided"));
        return;
    }

    bCombatActive = true;
    CalculateTurnDebts();

    UE_LOG(LogTemp, Log, TEXT("[TurnManager] Combat initialized with %d combatants"), Combatants.Num());

    // Start first turn
    AdvanceToNextTurn();
}

void UTurnManager::EndCombat()
{
    if (!bCombatActive)
        return;

    bCombatActive = false;
    OnCombatEnded.Broadcast();

    UE_LOG(LogTemp, Log, TEXT("[TurnManager] Combat ended after %d turns"), GlobalTurnCount);

    Combatants.Empty();
    CurrentActor = nullptr;
    PreviousActor = nullptr;
    GlobalTurnCount = 0;
}

void UTurnManager::AdvanceToNextTurn()
{
    if (!bCombatActive)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TurnManager] Cannot advance turn - combat not active"));
        return;
    }

    // Broadcast turn end for previous actor
    if (CurrentActor)
    {
        OnTurnEnded.Broadcast(CurrentActor, GlobalTurnCount);
    }

    // Find next combatant
    CalculateTurnDebts();
    FCombatantTurnDebt* NextCombatant = FindNextCombatant();

    if (!NextCombatant)
    {
        UE_LOG(LogTemp, Error, TEXT("[TurnManager] No valid combatant found for next turn"));
        EndCombat();
        return;
    }

    // Update turn state
    PreviousActor = CurrentActor;
    CurrentActor = NextCombatant->Actor;
    NextCombatant->TurnsTaken++;
    GlobalTurnCount++;

    UE_LOG(LogTemp, Log, TEXT("[TurnManager] Turn %d: %s"), GlobalTurnCount, *CurrentActor->GetName());

    // Broadcast turn start
    OnTurnStarted.Broadcast(CurrentActor, GlobalTurnCount);
}

AActor* UTurnManager::GetCurrentActor() const
{
    return CurrentActor;
}

TArray<AActor*> UTurnManager::GetAllCombatants() const
{
    TArray<AActor*> Result;
    for (const FCombatantTurnDebt& Combatant : Combatants)
    {
        Result.Add(Combatant.Actor);
    }
    return Result;
}

TArray<AActor*> UTurnManager::GetTurnOrderPreview(int32 NumTurns)
{
    TArray<AActor*> Result;
    // Implementation would simulate turn order without mutating state
    // Simplified for now
    return Result;
}

void UTurnManager::CalculateTurnDebts()
{
    if (Combatants.Num() == 0)
        return;

    // Find slowest speed
    int32 SlowestSpeed = Combatants[0].Speed;
    for (const FCombatantTurnDebt& Combatant : Combatants)
    {
        if (Combatant.Speed < SlowestSpeed)
            SlowestSpeed = Combatant.Speed;
    }

    if (SlowestSpeed <= 0)
        SlowestSpeed = 1;

    // Calculate speed ratios and accumulate debt
    for (FCombatantTurnDebt& Combatant : Combatants)
    {
        float SpeedRatio = (float)Combatant.Speed / (float)SlowestSpeed;
        Combatant.TurnsOwed += SpeedRatio;
    }
}

FCombatantTurnDebt* UTurnManager::FindNextCombatant()
{
    TArray<FCombatantTurnDebt*> EligibleCombatants;

    // Find alive combatants
    for (FCombatantTurnDebt& Combatant : Combatants)
    {
        UCharacterDataComponent* CharComp = Combatant.Actor->FindComponentByClass<UCharacterDataComponent>();
        if (CharComp && CharComp->bIsAlive)
        {
            EligibleCombatants.Add(&Combatant);
        }
    }

    if (EligibleCombatants.Num() == 0)
        return nullptr;

    // Find highest net debt
    FCombatantTurnDebt* Highest = EligibleCombatants[0];
    float HighestDebt = Highest->TurnsOwed - Highest->TurnsTaken;

    TArray<FCombatantTurnDebt*> TiedCombatants;
    TiedCombatants.Add(Highest);

    for (int32 i = 1; i < EligibleCombatants.Num(); i++)
    {
        float Debt = EligibleCombatants[i]->TurnsOwed - EligibleCombatants[i]->TurnsTaken;

        if (FMath::IsNearlyEqual(Debt, HighestDebt, 0.01f))
        {
            TiedCombatants.Add(EligibleCombatants[i]);
        }
        else if (Debt > HighestDebt)
        {
            HighestDebt = Debt;
            Highest = EligibleCombatants[i];
            TiedCombatants.Empty();
            TiedCombatants.Add(Highest);
        }
    }

    if (TiedCombatants.Num() > 1)
    {
        return ResolveTie(TiedCombatants);
    }

    return Highest;
}

FCombatantTurnDebt* UTurnManager::ResolveTie(TArray<FCombatantTurnDebt*>& TiedCombatants)
{
    // 7-level tie-breaker cascade
    // Level 1: Speed
    // Level 2: AttackSpeed  
    // Level 3: Underdog (lowest total stats)
    // Level 4: WorldBody
    // Level 5: WorldMind
    // Level 6: WorldSpirit
    // Level 7: Team + Position

    // Simplified implementation - just return first for now
    // Full implementation would apply all 7 levels
    return TiedCombatants[0];
}

void UTurnManager::CacheActorStats(FCombatantTurnDebt& Combatant)
{
    UCharacterDataComponent* CharComp = Combatant.Actor->FindComponentByClass<UCharacterDataComponent>();

    if (!CharComp || !CharComp->CharacterData)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TurnManager] Cannot cache stats for %s - missing CharacterDataComponent"),
            *Combatant.Actor->GetName());

        Combatant.Speed = 1;
        Combatant.AttackSpeed = 1;
        Combatant.TotalStats = 3;
        Combatant.WorldBody = 1;
        Combatant.WorldMind = 1;
        Combatant.WorldSpirit = 1;
        return;
    }

    UCharacterData* CharData = CharComp->CharacterData;

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
}

FCombatantTurnDebt* UTurnManager::FindCombatantByActor(AActor* Actor)
{
    for (FCombatantTurnDebt& Combatant : Combatants)
    {
        if (Combatant.Actor == Actor)
        {
            return &Combatant;
        }
    }
    return nullptr;
}

void UTurnManager::OnActorSpeedChanged(AActor* AffectedActor)
{
    FCombatantTurnDebt* Combatant = FindCombatantByActor(AffectedActor);

    if (!Combatant)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TurnManager] OnActorSpeedChanged: Actor not found"));
        return;
    }

    int32 OldSpeed = Combatant->Speed;
    CacheActorStats(*Combatant);

    UE_LOG(LogTemp, Log, TEXT("[TurnManager] Speed changed for %s: %d -> %d"),
        *AffectedActor->GetName(), OldSpeed, Combatant->Speed);

    OnSpeedChanged.Broadcast(AffectedActor, Combatant->Speed);
    CalculateTurnDebts();
}

void UTurnManager::OnActorDied(AActor* DeadActor)
{
    UE_LOG(LogTemp, Log, TEXT("[TurnManager] Actor died: %s"), *DeadActor->GetName());
}

void UTurnManager::OnActorResurrected(AActor* ResurrectedActor)
{
    UE_LOG(LogTemp, Log, TEXT("[TurnManager] Actor resurrected: %s"), *ResurrectedActor->GetName());
}

void UTurnManager::DebugPrintTurnOrder()
{
    UE_LOG(LogTemp, Display, TEXT("========================================"));
    UE_LOG(LogTemp, Display, TEXT("TURN ORDER (Next 5)"));
    UE_LOG(LogTemp, Display, TEXT("========================================"));
    // Implementation
}

void UTurnManager::DebugPrintDebtDetails()
{
    UE_LOG(LogTemp, Display, TEXT("========================================"));
    UE_LOG(LogTemp, Display, TEXT("TURN DEBT DETAILS"));
    UE_LOG(LogTemp, Display, TEXT("========================================"));
    // Implementation
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
}