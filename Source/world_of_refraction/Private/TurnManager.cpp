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

	// Find next actor
	FCombatantTurnDebt* NextCombatant = GetNextCombatant();

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

void UTurnManager::CalculateTurnDebts()
{
	if (Combatants.Num() == 0)
		return;

	// Find slowest speed
	int32 SlowestSpeed = INT_MAX;
	for (const FCombatantTurnDebt& Combatant : Combatants)
	{
		SlowestSpeed = FMath::Min(SlowestSpeed, Combatant.CachedSpeed);
	}

	if (SlowestSpeed <= 0)
		SlowestSpeed = 1;

	// Calculate ratios
	for (FCombatantTurnDebt& Combatant : Combatants)
	{
		if (Combatant.CachedSpeed <= 0)
			Combatant.SpeedRatio = 1.0f;
		else
			Combatant.SpeedRatio = (float)Combatant.CachedSpeed / (float)SlowestSpeed;

		Combatant.TurnsOwed += Combatant.SpeedRatio;
	}
}

FCombatantTurnDebt* UTurnManager::GetNextCombatant()
{
	FCombatantTurnDebt* BestCombatant = nullptr;
	float HighestDebt = -FLT_MAX;

	for (FCombatantTurnDebt& Combatant : Combatants)
	{
		// Skip dead
		UCharacterDataComponent* CharComp = Combatant.Actor->FindComponentByClass<UCharacterDataComponent>();
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

	// Recalculate debts for next cycle
	if (BestCombatant)
	{
		CalculateTurnDebts();
	}

	return BestCombatant;
}

bool UTurnManager::ShouldBreakTieInFavor(const FCombatantTurnDebt& A, const FCombatantTurnDebt& B) const
{
	// Level 1: Speed
	if (A.CachedSpeed != B.CachedSpeed)
		return A.CachedSpeed > B.CachedSpeed;

	// Level 2: AttackSpeed
	if (A.CachedAttackSpeed != B.CachedAttackSpeed)
		return A.CachedAttackSpeed > B.CachedAttackSpeed;

	// Level 3: Underdog (LOWEST total stats wins)
	int32 TotalA = A.CachedMind + A.CachedBody + A.CachedSpirit;
	int32 TotalB = B.CachedMind + B.CachedBody + B.CachedSpirit;
	if (TotalA != TotalB)
		return TotalA < TotalB;

	// Level 4: Body
	if (A.CachedBody != B.CachedBody)
		return A.CachedBody > B.CachedBody;

	// Level 5: Mind
	if (A.CachedMind != B.CachedMind)
		return A.CachedMind > B.CachedMind;

	// Level 6: Spirit
	if (A.CachedSpirit != B.CachedSpirit)
		return A.CachedSpirit > B.CachedSpirit;

	// Level 7: Team + Position (deterministic)
	if (A.TeamIndex != B.TeamIndex)
		return A.TeamIndex < B.TeamIndex;

	return A.PositionInTeam < B.PositionInTeam;
}

void UTurnManager::CacheActorStats(FCombatantTurnDebt& Combatant)
{
	UCharacterDataComponent* CharComp = Combatant.Actor->FindComponentByClass<UCharacterDataComponent>();

	if (CharComp && CharComp->CharacterData)
	{
		UCharacterData* CharData = CharComp->CharacterData;

		Combatant.CachedSpeed = CharData->WorldBodyLevel + CharData->WorldTurnSpeedPoints;
		Combatant.CachedAttackSpeed = CharData->WorldBodyLevel + CharData->WorldAttackSpeedPoints;
		Combatant.CachedMind = CharData->WorldMindLevel;
		Combatant.CachedBody = CharData->WorldBodyLevel;
		Combatant.CachedSpirit = CharData->WorldSpiritLevel;
	}
	else
	{
		Combatant.CachedSpeed = 5;
		Combatant.CachedAttackSpeed = 5;
		Combatant.CachedMind = 3;
		Combatant.CachedBody = 3;
		Combatant.CachedSpirit = 3;
	}
}

void UTurnManager::OnActorSpeedChanged(AActor* Actor)
{
	for (FCombatantTurnDebt& Combatant : Combatants)
	{
		if (Combatant.Actor == Actor)
		{
			CacheActorStats(Combatant);
			CalculateTurnDebts();
			OnSpeedChanged.Broadcast(Actor);
			return;
		}
	}
}

void UTurnManager::OnActorDied(AActor* Actor)
{
	UE_LOG(LogTemp, Log, TEXT("[TurnManager] %s died"), *Actor->GetName());
}

void UTurnManager::OnActorResurrected(AActor* Actor)
{
	UE_LOG(LogTemp, Log, TEXT("[TurnManager] %s resurrected"), *Actor->GetName());
}

AActor* UTurnManager::GetCurrentActor() const
{
	return CurrentActor;
}

TArray<AActor*> UTurnManager::PreviewTurnOrder(int32 NumTurns) const
{
	TArray<AActor*> Preview;

	// Create temp copy
	TArray<FCombatantTurnDebt> TempCombatants = Combatants;

	for (int32 i = 0; i < NumTurns; i++)
	{
		FCombatantTurnDebt* NextCombatant = nullptr;
		float HighestDebt = -FLT_MAX;

		// Find highest debt
		for (FCombatantTurnDebt& Combatant : TempCombatants)
		{
			UCharacterDataComponent* CharComp = Combatant.Actor->FindComponentByClass<UCharacterDataComponent>();
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

			// Recalculate debts
			int32 SlowestSpeed = INT_MAX;
			for (const FCombatantTurnDebt& Combatant : TempCombatants)
			{
				SlowestSpeed = FMath::Min(SlowestSpeed, Combatant.CachedSpeed);
			}
			if (SlowestSpeed <= 0)
				SlowestSpeed = 1;

			for (FCombatantTurnDebt& Combatant : TempCombatants)
			{
				float SpeedRatio = (float)Combatant.CachedSpeed / (float)SlowestSpeed;
				Combatant.TurnsOwed += SpeedRatio;
			}
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

	TArray<AActor*> Preview = PreviewTurnOrder(10);
	UE_LOG(LogTemp, Display, TEXT("\nNext 10 turns:"));
	for (int32 i = 0; i < Preview.Num(); i++)
	{
		UE_LOG(LogTemp, Display, TEXT("  %d. %s"), i + 1, *Preview[i]->GetName());
	}

	UE_LOG(LogTemp, Display, TEXT("======================"));
}