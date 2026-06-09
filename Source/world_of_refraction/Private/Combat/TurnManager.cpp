// Copyright Epic Games, Inc. All Rights Reserved.
// CORRECTED VERSION - Turn debt accumulates per ROUND, not per TURN

#include "Combat/TurnManager.h"
#include "Combat/CombatConstants.h"
#include "Engine/Engine.h" // GEngine for on-screen debug (DebugPrintPendingTurns)
#include "Character/CharacterDataComponent.h"
#include "Character/CharacterData.h"
#include "Loadout/LoadoutComponent.h"
#include "Loadout/Entries/FWeaponLoadoutEntry.h"
#include "Equipment/FEquipmentStatBonus.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/ESkillEffectType.h"
#include "Engine/GameInstance.h"

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
	PendingTurns.Empty();
	GlobalTurnCount = 0;
	CurrentActor = nullptr;
	PreviousActor = nullptr;
	bCurrentTurnIsBonus = false;

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
	PendingTurns.Empty();
	CurrentActor = nullptr;
	PreviousActor = nullptr;
	GlobalTurnCount = 0;
	bCurrentTurnIsBonus = false;
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

	// If this pick consumes a granted-but-not-yet-taken bonus turn (Emerald), mark it spent —
	// THIS turn is the bonus being taken. The preview's marker read mirrors this, so a flagged
	// upcoming slot clears once the bonus turn is taken. (Single-Emerald: one untaken bonus →
	// the actor's next pick consumes it.)
	// Set the per-turn transient EVERY turn (true OR false) so it describes THIS turn, never a
	// stale prior value. Captured here — before OnTurnStarted.Broadcast — so the widget's
	// refresh reads the result, not the already-zeroed count (the consume-before-observe fix).
	bCurrentTurnIsBonus = (NextCombatant->UntakenBonusTurns > 0);
	if (bCurrentTurnIsBonus)
	{
		NextCombatant->UntakenBonusTurns--;
		// [BONUSDIAG] temp — remove after diagnosis.
		UE_LOG(LogTemp, Warning, TEXT("[BONUSDIAG] take-side: %s took a bonus turn, live marker now=%d, bCurrentTurnIsBonus=1"),
			   *GetNameSafe(NextCombatant->Actor), NextCombatant->UntakenBonusTurns);
	}

	UE_LOG(LogTemp, Log, TEXT("[TurnManager] Turn %d: %s (Team %d)"),
		   GlobalTurnCount,
		   *CurrentActor->GetName(),
		   NextCombatant->TeamIndex);

	OnTurnStarted.Broadcast(CurrentActor, GlobalTurnCount);

	// Tick scheduled bonus turns (Emerald) — exactly once per global turn boundary. Each
	// entry's countdown drops by 1; at 0 the actor is granted an extra turn via the existing
	// RequestExtraTurn debt-credit (honored by the NEXT GetNextCombatant). Dead/invalid actors
	// are dropped silently — an enemy DoT may have killed the target before the bonus fires —
	// using the same living-combatant check as GetNextCombatant. Reverse iteration so RemoveAt
	// is safe.
	for (int32 i = PendingTurns.Num() - 1; i >= 0; --i)
	{
		if (--PendingTurns[i].TurnsRemaining > 0)
		{
			continue;
		}

		AActor *BonusActor = PendingTurns[i].Actor;
		PendingTurns.RemoveAt(i);

		if (IsValid(BonusActor))
		{
			UCharacterDataComponent *CharComp = BonusActor->FindComponentByClass<UCharacterDataComponent>();
			if (CharComp && CharComp->bIsAlive)
			{
				RequestExtraTurn(BonusActor, /*bIsBonusTurn=*/true);
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("[TurnManager] Scheduled bonus turn for %s dropped — not a living combatant"),
					   *BonusActor->GetName());
			}
		}
	}
}

// ========================================
// CORRECTED: Only calculates ratios, doesn't add to TurnsOwed
// ========================================
void UTurnManager::CalculateSpeedRatios()
{
	if (Combatants.Num() == 0)
		return;

	// Effective speed = CachedSpeed (base) * (1 + (ModifyTurnSpeed + TurnSpeedBuff
	// − TurnSpeedDebuff)/100). Computed on-the-fly each call so OnActorDied /
	// OnActorResurrected — which invoke CalculateSpeedRatios without a fresh
	// CacheActorStats — don't compound. CachedSpeed remains the pristine base.
	USkillEffectManager *SEM = GetSkillEffectManager();

	auto GetEffectiveSpeed = [SEM](const FCombatantTurnDebt &Combatant) -> int32
	{
		if (!Combatant.Actor || Combatant.CachedSpeed <= 0)
		{
			return FMath::Max(1, Combatant.CachedSpeed);
		}
		if (!SEM)
		{
			return Combatant.CachedSpeed;
		}
		const float TurnMod = SEM->GetTotalStatModifier(Combatant.Actor, ESkillEffectType::ModifyTurnSpeed);
		const float TurnBuff = SEM->GetTotalStatModifier(Combatant.Actor, ESkillEffectType::TurnSpeedBuff);
		const float TurnDebuff = SEM->GetTotalStatModifier(Combatant.Actor, ESkillEffectType::TurnSpeedDebuff);
		const float Multiplier = 1.0f + (TurnMod + TurnBuff - TurnDebuff) / CombatConstants::STAT_PERCENT_DIVISOR;

		// Attached TurnSpeedStone — a PERMANENT, equipment-derived turn-speed multiplier
		// from the combatant's OWN active weapon attachment (live-resolved). Multiplies
		// EFFECTIVE speed alongside the buff/debuff Multiplier — deliberately NOT folded
		// into CachedSpeed, which stays the pristine tie-break base. Inert (×1) unless a
		// TurnSpeedStone is attached (GetAttachedStonePercent stat-match guard).
		float StoneFactor = 1.0f;
		if (ULoadoutComponent *Loadout = Combatant.Actor->FindComponentByClass<ULoadoutComponent>())
		{
			if (const FRuntimeAttachedItem *AttPtr = Loadout->GetActiveWeaponAttachment())
			{
				const FRuntimeAttachedItem &Att = *AttPtr;
				const float StonePct = CrystalEffectTable::GetAttachedStonePercent(Att, ESubStat::TurnSpeed);
				StoneFactor = 1.0f + StonePct / CombatConstants::STAT_PERCENT_DIVISOR;
			}
		}
		return FMath::Max(1, FMath::RoundToInt(Combatant.CachedSpeed * Multiplier * StoneFactor));
	};

	// Find slowest effective speed among living combatants.
	int32 SlowestSpeed = INT_MAX;
	for (const FCombatantTurnDebt &Combatant : Combatants)
	{
		UCharacterDataComponent *CharComp = Combatant.Actor->FindComponentByClass<UCharacterDataComponent>();
		if (CharComp && CharComp->bIsAlive)
		{
			SlowestSpeed = FMath::Min(SlowestSpeed, GetEffectiveSpeed(Combatant));
		}
	}

	if (SlowestSpeed <= 0 || SlowestSpeed == INT_MAX)
		SlowestSpeed = 1;

	// Calculate speed ratios from effective speed (slowest = 1.0, others higher).
	for (FCombatantTurnDebt &Combatant : Combatants)
	{
		const int32 Effective = GetEffectiveSpeed(Combatant);
		Combatant.SpeedRatio = (Effective > 0) ? (float)Effective / (float)SlowestSpeed : 1.0f;
	}
}

USkillEffectManager *UTurnManager::GetSkillEffectManager() const
{
	if (!SkillEffectManagerRef)
	{
		if (UGameInstance *GI = GetGameInstance())
		{
			const_cast<UTurnManager *>(this)->SkillEffectManagerRef =
				GI->GetSubsystem<USkillEffectManager>();
		}
	}
	return SkillEffectManagerRef;
}

void UTurnManager::RequestExtraTurn(AActor *Actor, bool bIsBonusTurn)
{
	if (!Actor) return;
	for (FCombatantTurnDebt &Combatant : Combatants)
	{
		if (Combatant.Actor == Actor)
		{
			Combatant.TurnsOwed += 1.0f;
			// Emerald bonus grants flag the granted-not-yet-taken turn so the preview keeps
			// showing it after the PendingTurns entry is consumed. Non-bonus callers (e.g.
			// the ExtraAction skill effect) leave this 0 and are never flagged.
			if (bIsBonusTurn)
			{
				Combatant.UntakenBonusTurns++;
			}
			UE_LOG(LogTemp, Log, TEXT("[TurnManager] %s turn granted to %s (TurnsOwed now %.2f, UntakenBonus %d)"),
				   bIsBonusTurn ? TEXT("Bonus") : TEXT("Extra"), *Actor->GetName(), Combatant.TurnsOwed, Combatant.UntakenBonusTurns);
			return;
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("[TurnManager] RequestExtraTurn: %s not in current combat"),
		   *Actor->GetName());
}

void UTurnManager::ScheduleBonusTurn(AActor *Actor, int32 DelayTurns)
{
	if (!Actor)
	{
		return;
	}

	// N==0 (immediate) is handled caller-side (RequestExtraTurn directly); the scheduler
	// only handles a genuine delay (N>=1). A <1 delay here is a misuse — log and drop
	// rather than silently firing at the wrong boundary.
	if (DelayTurns < 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TurnManager] ScheduleBonusTurn: DelayTurns %d < 1 for %s — ignored (N==0 is caller-side immediate)"),
			   DelayTurns, *Actor->GetName());
		return;
	}

	FScheduledTurn Entry;
	Entry.Actor = Actor;
	Entry.TurnsRemaining = DelayTurns;
	PendingTurns.Add(Entry);

	UE_LOG(LogTemp, Log, TEXT("[TurnManager] Scheduled bonus turn for %s in %d turn(s)"),
		   *Actor->GetName(), DelayTurns);
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

		// Pillar-scaled turn speed (TURN_SPEED_BASE + EffectiveSpirit × points × per-point),
		// rounded for int storage. Replaces the earlier raw WorldBodyLevel + TurnSpeed
		// sum which ignored both world scaling and the pillar move from Mind to Spirit.
		Combatant.CachedSpeed = FMath::RoundToInt(CharData->CalculateTurnSpeed());

		// Equipment stat bonus — flat additive to cached turn speed. Read from
		// the actor's active loadout. Hot-swap re-cache is driven by
		// LoadoutComponent calling OnActorSpeedChanged on weapon/ring switch.
		if (ULoadoutComponent *LoadoutComp = Combatant.Actor->FindComponentByClass<ULoadoutComponent>())
		{
			const FEquipmentStatBonus Bonus = LoadoutComp->GetActiveStatBonus(Combatant.Actor);
			Combatant.CachedSpeed += Bonus.BonusTurnSpeed;
		}

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

TArray<FPreviewTurnEntry> UTurnManager::PreviewTurnOrder(int32 NumTurns) const
{
	TArray<FPreviewTurnEntry> Preview;

	// Create temp copies of state — the forward sim mutates these, never the live state.
	TArray<FCombatantTurnDebt> TempCombatants = Combatants;
	TArray<FScheduledTurn> TempPending = PendingTurns;

	// [BONUSDIAG] temp — remove after diagnosis. Logs the bonus-flag INPUT state at the moment
	// this preview runs: how many pending entries + any combatant already carrying a live marker.
	{
		int32 LiveMarkers = 0;
		for (const FCombatantTurnDebt &C : TempCombatants)
		{
			if (C.UntakenBonusTurns > 0)
			{
				LiveMarkers++;
				UE_LOG(LogTemp, Warning, TEXT("[BONUSDIAG] preview-in: %s carries UntakenBonusTurns=%d"),
					   *GetNameSafe(C.Actor), C.UntakenBonusTurns);
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("[BONUSDIAG] PreviewTurnOrder start: TempPending=%d, liveMarkers=%d"),
			   TempPending.Num(), LiveMarkers);
	}

	// Bonus-turn flagging is driven by FCombatantTurnDebt::UntakenBonusTurns (copied into
	// TempCombatants). It covers the FULL lifecycle: a granted-but-not-taken bonus arrives
	// already non-zero (copied live, survives PendingTurns consumption), and a still-pending
	// bonus that FIRES inside this sim increments it below. Either way, the pick step flags +
	// decrements it. No separate awaiting-pick map needed.

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

		if (!NextCombatant)
		{
			break;
		}

		// Record the slot. If this combatant has an untaken bonus turn — either copied live
		// (granted, not yet taken) or credited by a TempPending fire below — THIS pick is the
		// bonus turn: flag it and consume one (mirrors the live take-side decrement).
		FPreviewTurnEntry Entry;
		Entry.Actor = NextCombatant->Actor;
		const int32 MarkerBefore = NextCombatant->UntakenBonusTurns; // [BONUSDIAG] temp
		if (NextCombatant->UntakenBonusTurns > 0)
		{
			Entry.bIsBonusTurn = true;
			NextCombatant->UntakenBonusTurns--;
		}
		// [BONUSDIAG] temp — remove after diagnosis.
		UE_LOG(LogTemp, Warning, TEXT("[BONUSDIAG] sim pick %d: %s marker=%d flagged=%d"),
			   i, *GetNameSafe(Entry.Actor), MarkerBefore, Entry.bIsBonusTurn ? 1 : 0);
		Preview.Add(Entry);
		NextCombatant->TurnsTaken++;

		// Sim-mirror of AdvanceToNextTurn's fire-loop — AFTER the pick, once per boundary.
		// Decrement each pending entry; at 0, credit +1.0 debt on the matching temp combatant
		// (sim copy ONLY) with the same IsValid + alive guard the live fire uses, and bump its
		// UntakenBonusTurns so this actor's NEXT pick is flagged the bonus turn (same marker the
		// live grant sets — so a still-pending bonus flags exactly like an already-granted one).
		if (TempPending.Num() > 0)
		{
			for (int32 p = TempPending.Num() - 1; p >= 0; --p)
			{
				if (--TempPending[p].TurnsRemaining > 0)
				{
					continue;
				}

				AActor *BonusActor = TempPending[p].Actor;
				TempPending.RemoveAt(p);

				if (!IsValid(BonusActor))
				{
					continue;
				}
				UCharacterDataComponent *BonusComp = BonusActor->FindComponentByClass<UCharacterDataComponent>();
				if (!BonusComp || !BonusComp->bIsAlive)
				{
					continue;
				}

				for (FCombatantTurnDebt &TC : TempCombatants)
				{
					if (TC.Actor == BonusActor)
					{
						TC.TurnsOwed += 1.0f;
						TC.UntakenBonusTurns++;
						// [BONUSDIAG] temp — remove after diagnosis.
						UE_LOG(LogTemp, Warning, TEXT("[BONUSDIAG] sim fire: %s pending->granted, marker now=%d"),
							   *GetNameSafe(BonusActor), TC.UntakenBonusTurns);
						break;
					}
				}
			}
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

	TArray<FPreviewTurnEntry> Preview = PreviewTurnOrder(10);
	UE_LOG(LogTemp, Display, TEXT("\nNext 10 turns:"));
	for (int32 i = 0; i < Preview.Num(); i++)
	{
		UE_LOG(LogTemp, Display, TEXT("  %d. %s%s"), i + 1,
			   Preview[i].Actor ? *Preview[i].Actor->GetName() : TEXT("<none>"),
			   Preview[i].bIsBonusTurn ? TEXT(" [BONUS]") : TEXT(""));
	}

	UE_LOG(LogTemp, Display, TEXT("======================"));
}

FString UTurnManager::GetPendingTurnsString() const
{
	if (PendingTurns.Num() == 0)
	{
		return TEXT("PendingTurns: (none). Immediate (N==0) grants bypass the queue via RequestExtraTurn.");
	}

	FString Out = FString::Printf(TEXT("PendingTurns (%d scheduled, N>=1):"), PendingTurns.Num());
	for (const FScheduledTurn &Entry : PendingTurns)
	{
		const FString Name = Entry.Actor ? Entry.Actor->GetName() : TEXT("<invalid>");
		Out += FString::Printf(TEXT("\n  %s - fires in %d turn(s)"), *Name, Entry.TurnsRemaining);
	}
	return Out;
}

void UTurnManager::DebugPrintPendingTurns()
{
	const FString Str = GetPendingTurnsString();
	UE_LOG(LogTemp, Display, TEXT("=== SCHEDULED BONUS TURNS ===\n%s\n============================="), *Str);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, Str);
	}
}