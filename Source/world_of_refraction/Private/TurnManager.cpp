// Copyright Epic Games, Inc. All Rights Reserved.
// CORRECTED VERSION - Turn debt accumulates per ROUND, not per TURN

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

void UTurnManager::InitializeCombat(const TArray<AActor *> &Team1, const TArray<AActor *> &Team2)
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

	// Calculate speed ratios (but don't add to TurnsOwed yet - that happens in GetNextCombatant)
	CalculateSpeedRatios();

	UE_LOG(LogTemp, Log, TEXT("[TurnManager] Combat initialized with %d combatants"), Combatants.Num());

	// Start first turn
	AdvanceToNextTurn();
}

void UTurnManager::EndCombat()
{
	if (!bCombatActive)
		return;

	UE_LOG(LogTemp, Log, TEXT("[TurnManager] Combat ended at turn %d"), GlobalTurnCount);

	OnCombatEnded.Broadcast(GlobalTurnCount);

	bCombatActive = false;
	Combatants.Empty();
	CurrentActor = nullptr;
	PreviousActor = nullptr;
	GlobalTurnCount = 0;
}

void UTurnManager::AdvanceToNextTurn()
{
	if (!bCombatActive)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TurnManager] AdvanceToNextTurn called but combat not active"));
		return;
	}

	PreviousActor = CurrentActor;

	// // Diagnostic: dump debt state before picking next actor
	// DebugPrintTurnOrder();

	// Find next actor
	FCombatantTurnDebt *NextCombatant = GetNextCombatant();

	if (!NextCombatant)
	{
		UE_LOG(LogTemp, Error, TEXT("[TurnManager] No valid combatant found for next turn"));
		EndCombat();
		return;
	}

	CurrentActor = NextCombatant->Actor;
	NextCombatant->TurnsTaken++;
	GlobalTurnCount++;

	UE_LOG(LogTemp, Log, TEXT("[TurnManager] Turn %d: %s (Team %d)"),
		   GlobalTurnCount,
		   *CurrentActor->GetName(),
		   NextCombatant->TeamIndex);

	OnTurnStarted.Broadcast(CurrentActor, GlobalTurnCount);
}

// ========================================
// CORRECTED: Only calculates ratios, doesn't add to TurnsOwed
// ========================================
void UTurnManager::CalculateSpeedRatios()
{
	if (Combatants.Num() == 0)
		return;

	// Find slowest speed among living combatants
	int32 SlowestSpeed = INT_MAX;
	for (const FCombatantTurnDebt &Combatant : Combatants)
	{
		UCharacterDataComponent *CharComp = Combatant.Actor->FindComponentByClass<UCharacterDataComponent>();
		if (CharComp && CharComp->bIsAlive)
		{
			SlowestSpeed = FMath::Min(SlowestSpeed, Combatant.CachedSpeed);
		}
	}

	if (SlowestSpeed <= 0)
		SlowestSpeed = 1;

	// Calculate speed ratios (slowest = 1.0, others = proportionally higher)
	for (FCombatantTurnDebt &Combatant : Combatants)
	{
		if (Combatant.CachedSpeed <= 0)
			Combatant.SpeedRatio = 1.0f;
		else
			Combatant.SpeedRatio = (float)Combatant.CachedSpeed / (float)SlowestSpeed;
	}
}

// ========================================
// NEW: Adds one round of debt to all combatants
// ========================================
void UTurnManager::AccumulateDebtRound()
{
	for (FCombatantTurnDebt &Combatant : Combatants)
	{
		Combatant.TurnsOwed += Combatant.SpeedRatio;
	}
}

// ========================================
// CORRECTED: Only adds debt when a new round starts
// ========================================
FCombatantTurnDebt *UTurnManager::GetNextCombatant()
{
	// Check if we need a new round (no living combatant has positive net debt)
	float MaxNetDebt = -FLT_MAX;
	for (const FCombatantTurnDebt &Combatant : Combatants)
	{
		UCharacterDataComponent *CharComp = Combatant.Actor->FindComponentByClass<UCharacterDataComponent>();
		if (CharComp && CharComp->bIsAlive)
		{
			float NetDebt = Combatant.TurnsOwed - Combatant.TurnsTaken;
			MaxNetDebt = FMath::Max(MaxNetDebt, NetDebt);
		}
	}

	// If no one has positive debt, start a new round
	if (MaxNetDebt <= KINDA_SMALL_NUMBER)
	{
		AccumulateDebtRound();
	}

	// Find combatant with highest net debt
	FCombatantTurnDebt *BestCombatant = nullptr;
	float HighestDebt = -FLT_MAX;

	for (FCombatantTurnDebt &Combatant : Combatants)
	{
		// Skip dead combatants
		UCharacterDataComponent *CharComp = Combatant.Actor->FindComponentByClass<UCharacterDataComponent>();
		if (!CharComp || !CharComp->bIsAlive)
			continue;

		float NetDebt = Combatant.TurnsOwed - Combatant.TurnsTaken;

		if (NetDebt > HighestDebt + KINDA_SMALL_NUMBER)
		{
			HighestDebt = NetDebt;
			BestCombatant = &Combatant;
		}
		else if (FMath::IsNearlyEqual(NetDebt, HighestDebt, KINDA_SMALL_NUMBER))
		{
			// Tie-breaking
			if (BestCombatant && ShouldBreakTieInFavor(Combatant, *BestCombatant))
			{
				BestCombatant = &Combatant;
			}
		}
	}

	// NOTE: No longer calling CalculateTurnDebts() here - that was the bug!

	return BestCombatant;
}

bool UTurnManager::ShouldBreakTieInFavor(const FCombatantTurnDebt &A, const FCombatantTurnDebt &B) const
{
	// Level 1: Speed (higher wins)
	if (A.CachedSpeed != B.CachedSpeed)
		return A.CachedSpeed > B.CachedSpeed;

	// Level 2: Action speed (higher wins)
	if (A.CachedActionSpeed != B.CachedActionSpeed)
		return A.CachedActionSpeed > B.CachedActionSpeed;

	// Level 3: Underdog (LOWER total stats wins - rewards glass cannon builds)
	int32 TotalA = A.CachedMind + A.CachedBody + A.CachedSpirit;
	int32 TotalB = B.CachedMind + B.CachedBody + B.CachedSpirit;
	if (TotalA != TotalB)
		return TotalA < TotalB;

	// Level 4: Body (higher wins)
	if (A.CachedBody != B.CachedBody)
		return A.CachedBody > B.CachedBody;

	// Level 5: Mind (higher wins)
	if (A.CachedMind != B.CachedMind)
		return A.CachedMind > B.CachedMind;

	// Level 6: Spirit (higher wins)
	if (A.CachedSpirit != B.CachedSpirit)
		return A.CachedSpirit > B.CachedSpirit;

	// Level 7: Team + Position (deterministic fallback)
	if (A.TeamIndex != B.TeamIndex)
		return A.TeamIndex < B.TeamIndex;

	return A.PositionInTeam < B.PositionInTeam;
}

void UTurnManager::CacheActorStats(FCombatantTurnDebt &Combatant)
{
	UCharacterDataComponent *CharComp = Combatant.Actor->FindComponentByClass<UCharacterDataComponent>();

	if (CharComp && CharComp->CharacterData)
	{
		UCharacterData *CharData = CharComp->CharacterData;

		// Speed = Body + TurnSpeed substat
		Combatant.CachedSpeed = CharData->WorldBodyLevel + CharData->TurnSpeed;
		Combatant.CachedActionSpeed = CharData->GetTotalActionSpeed();
		Combatant.CachedMind = CharData->WorldMindLevel;
		Combatant.CachedBody = CharData->WorldBodyLevel;
		Combatant.CachedSpirit = CharData->WorldSpiritLevel;
	}
	else
	{
		// Fallback defaults for testing
		Combatant.CachedSpeed = 5;
		Combatant.CachedActionSpeed = 5;
		Combatant.CachedMind = 3;
		Combatant.CachedBody = 3;
		Combatant.CachedSpirit = 3;
	}
}

void UTurnManager::OnActorSpeedChanged(AActor *Actor)
{
	for (FCombatantTurnDebt &Combatant : Combatants)
	{
		if (Combatant.Actor == Actor)
		{
			CacheActorStats(Combatant);
			// Recalculate all ratios since relative speeds changed
			CalculateSpeedRatios();
			OnSpeedChanged.Broadcast(Actor);
			return;
		}
	}
}

void UTurnManager::OnActorDied(AActor *Actor)
{
	UE_LOG(LogTemp, Log, TEXT("[TurnManager] %s died"), *Actor->GetName());
	// Recalculate ratios since the slowest combatant might have changed
	CalculateSpeedRatios();
}

void UTurnManager::OnActorResurrected(AActor *Actor)
{
	UE_LOG(LogTemp, Log, TEXT("[TurnManager] %s resurrected"), *Actor->GetName());
	// Recalculate ratios since the slowest combatant might have changed
	CalculateSpeedRatios();
}

AActor *UTurnManager::GetCurrentActor() const
{
	return CurrentActor;
}

TArray<AActor *> UTurnManager::PreviewTurnOrder(int32 NumTurns) const
{
	TArray<AActor *> Preview;

	// Create temp copy of state
	TArray<FCombatantTurnDebt> TempCombatants = Combatants;

	for (int32 i = 0; i < NumTurns; i++)
	{
		// Check if we need a new round
		float MaxNetDebt = -FLT_MAX;
		for (const FCombatantTurnDebt &Combatant : TempCombatants)
		{
			UCharacterDataComponent *CharComp = Combatant.Actor->FindComponentByClass<UCharacterDataComponent>();
			if (CharComp && CharComp->bIsAlive)
			{
				float NetDebt = Combatant.TurnsOwed - Combatant.TurnsTaken;
				MaxNetDebt = FMath::Max(MaxNetDebt, NetDebt);
			}
		}

		// If no one has positive debt, add a round
		if (MaxNetDebt <= KINDA_SMALL_NUMBER)
		{
			for (FCombatantTurnDebt &Combatant : TempCombatants)
			{
				Combatant.TurnsOwed += Combatant.SpeedRatio;
			}
		}

		// Find highest debt
		FCombatantTurnDebt *NextCombatant = nullptr;
		float HighestDebt = -FLT_MAX;

		for (FCombatantTurnDebt &Combatant : TempCombatants)
		{
			UCharacterDataComponent *CharComp = Combatant.Actor->FindComponentByClass<UCharacterDataComponent>();
			if (!CharComp || !CharComp->bIsAlive)
				continue;

			float NetDebt = Combatant.TurnsOwed - Combatant.TurnsTaken;

			if (NetDebt > HighestDebt + KINDA_SMALL_NUMBER)
			{
				HighestDebt = NetDebt;
				NextCombatant = &Combatant;
			}
			else if (FMath::IsNearlyEqual(NetDebt, HighestDebt, KINDA_SMALL_NUMBER))
			{
				if (NextCombatant && ShouldBreakTieInFavor(Combatant, *NextCombatant))
				{
					NextCombatant = &Combatant;
				}
			}
		}

		if (NextCombatant)
		{
			Preview.Add(NextCombatant->Actor);
			NextCombatant->TurnsTaken++;
		}
		else
		{
			break;
		}
	}

	return Preview;
}

void UTurnManager::DebugPrintTurnOrder()
{
	UE_LOG(LogTemp, Display, TEXT("=== TURN ORDER DEBUG ==="));
	UE_LOG(LogTemp, Display, TEXT("Current Turn: %d"), GlobalTurnCount);

	if (CurrentActor)
	{
		UE_LOG(LogTemp, Display, TEXT("Current Actor: %s"), *CurrentActor->GetName());
	}

	UE_LOG(LogTemp, Display, TEXT("\nCombatant Debt Status:"));
	for (const FCombatantTurnDebt &Combatant : Combatants)
	{
		float NetDebt = Combatant.TurnsOwed - Combatant.TurnsTaken;
		UE_LOG(LogTemp, Display, TEXT("  %s: Speed=%d, Ratio=%.2f, Owed=%.2f, Taken=%d, Net=%.2f"),
			   *Combatant.Actor->GetName(),
			   Combatant.CachedSpeed,
			   Combatant.SpeedRatio,
			   Combatant.TurnsOwed,
			   Combatant.TurnsTaken,
			   NetDebt);
	}

	TArray<AActor *> Preview = PreviewTurnOrder(10);
	UE_LOG(LogTemp, Display, TEXT("\nNext 10 turns:"));
	for (int32 i = 0; i < Preview.Num(); i++)
	{
		UE_LOG(LogTemp, Display, TEXT("  %d. %s"), i + 1, *Preview[i]->GetName());
	}

	UE_LOG(LogTemp, Display, TEXT("======================"));
}