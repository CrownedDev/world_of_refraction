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

	// Calculate speed ratios (but don't add to TurnsOwed yet - that happens in AdvanceSimState)
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

	// The single shared scheduler step runs on REAL state. AdvanceSimState owns pinned
	// bonus fire, round-check+accumulate, select+tiebreak, TurnsTaken++ and the normal-turn
	// delay countdown — none of those may be repeated here or they would double-apply.
	FPreviewTurnEntry Picked;
	if (!AdvanceSimState(Combatants, PendingTurns, Picked))
	{
		UE_LOG(LogTemp, Error, TEXT("[TurnManager] No valid combatant found for next turn"));
		EndCombat();
		return;
	}

	CurrentActor = Picked.Actor;
	// Per-turn transient, set EVERY turn (true OR false) so it describes THIS turn. The
	// consume happened inside AdvanceSimState; captured here — before OnTurnStarted.Broadcast
	// — so the widget's refresh reads the result (the consume-before-observe fix).
	bCurrentTurnIsBonus = Picked.bIsBonusTurn;
	GlobalTurnCount++;

	UE_LOG(LogTemp, Log, TEXT("[TurnManager] Turn %d: %s (Team %d)"),
		   GlobalTurnCount,
		   *CurrentActor->GetName(),
		   GetActorTeam(CurrentActor));

	// Belt reflects post-advance state. Rebuilt BEFORE the broadcast: the turn-order strip
	// refreshes synchronously inside OnTurnStarted and reads PreviewTurnOrder — now a belt
	// slice — so a post-broadcast rebuild would hand it last turn's belt, one slot stale.
	RebuildBelt();

	OnTurnStarted.Broadcast(CurrentActor, GlobalTurnCount);
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

	// Cluster 2b: bonus turns are pinned belt entries (ScheduleBonusTurn), not debt
	// credits. The bonus path here is retired; the param survives only so existing
	// call sites compile. True is a no-op so a stray call can't silently re-create
	// the old debt-soft bonus behaviour.
	if (bIsBonusTurn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TurnManager] RequestExtraTurn(bIsBonusTurn=true) for %s ignored — use ScheduleBonusTurn for bonuses"),
			   *Actor->GetName());
		return;
	}

	for (FCombatantTurnDebt &Combatant : Combatants)
	{
		if (Combatant.Actor == Actor)
		{
			Combatant.TurnsOwed += 1.0f;
			UE_LOG(LogTemp, Log, TEXT("[TurnManager] Extra turn granted to %s (TurnsOwed now %.2f)"),
				   *Actor->GetName(), Combatant.TurnsOwed);
			return;
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("[TurnManager] RequestExtraTurn: %s not in current combat"),
		   *Actor->GetName());
}

void UTurnManager::ScheduleBonusTurn(AActor *Actor, int32 DelayTurns, int32 Count)
{
	if (!Actor)
	{
		return;
	}

	FScheduledTurn Entry;
	Entry.Actor = Actor;
	Entry.TurnsRemaining = DelayTurns; // 0 = ready: fires on the very next scheduler step
	Entry.Count = FMath::Max(1, Count);
	PendingTurns.Add(Entry);

	UE_LOG(LogTemp, Log, TEXT("[TurnManager] Scheduled %d pinned bonus turn(s) for %s after %d normal turn(s)"),
		   Entry.Count, *Actor->GetName(), DelayTurns);
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
	// Cluster 2a: a slice of the materialized belt — no forward sim. Callers ask <= 10 and
	// the belt horizon is 16 (TURN_BELT_HORIZON), so the slice always covers them; asking
	// beyond the belt returns what's available.
	TArray<FPreviewTurnEntry> Out;
	const int32 Count = FMath::Min(NumTurns, TurnBelt.Num());
	for (int32 i = 0; i < Count; ++i)
	{
		Out.Add(TurnBelt[i]);
	}
	return Out;
}

bool UTurnManager::AdvanceSimState(TArray<FCombatantTurnDebt> &State,
								   TArray<FScheduledTurn> &Pending,
								   FPreviewTurnEntry &OutEntry) const
{
	// THE scheduler step: the live path (AdvanceToNextTurn, real state) and the belt fill
	// (RebuildBelt, scratch state) both run through here — the only place a turn is
	// selected. Deliberately log-free: RebuildBelt replays it 16x per rebuild.

	// Pinned-fire (Cluster 2b) — BEFORE the debt machinery. A ready entry
	// (TurnsRemaining <= 0) preempts the debt pick: the bonus lands at its guaranteed
	// slot regardless of debt. FIFO by Pending order. A fired bonus does NOT burn other
	// entries' delay — only normal turns do (countdown after the debt pick below).
	for (int32 p = 0; p < Pending.Num();)
	{
		if (Pending[p].TurnsRemaining > 0)
		{
			++p;
			continue;
		}

		AActor *BonusActor = Pending[p].Actor;

		bool bGranteeAlive = false;
		if (IsValid(BonusActor))
		{
			UCharacterDataComponent *BonusComp = BonusActor->FindComponentByClass<UCharacterDataComponent>();
			bGranteeAlive = BonusComp && BonusComp->bIsAlive;
		}

		if (!bGranteeAlive)
		{
			// Dead/invalid grantee: drop the entry WITHOUT emitting a turn, keep scanning
			// for the next ready entry (or fall through to the debt pick).
			Pending.RemoveAt(p);
			continue;
		}

		OutEntry.Actor = BonusActor;
		OutEntry.bIsBonusTurn = true;
		OutEntry.bIsForced = false;

		if (--Pending[p].Count <= 0)
		{
			Pending.RemoveAt(p);
		}

		// Bonus turns still count as taken (matching pre-2b). NOTE: no TurnsOwed credit —
		// the pinned turn deliberately costs one slot of future normal priority.
		for (FCombatantTurnDebt &Combatant : State)
		{
			if (Combatant.Actor == BonusActor)
			{
				Combatant.TurnsTaken++;
				break;
			}
		}
		return true;
	}

	// Check if we need a new round
	float MaxNetDebt = -FLT_MAX;
	for (const FCombatantTurnDebt &Combatant : State)
	{
		UCharacterDataComponent *CharComp = Combatant.Actor->FindComponentByClass<UCharacterDataComponent>();
		if (CharComp && CharComp->bIsAlive)
		{
			float NetDebt = Combatant.TurnsOwed - Combatant.TurnsTaken;
			MaxNetDebt = FMath::Max(MaxNetDebt, NetDebt);
		}
	}

	// If no one has positive debt, add a round — on ALL entries including dead ones,
	// matching AccumulateDebtRound exactly (dead combatants accrue catch-up debt).
	if (MaxNetDebt <= KINDA_SMALL_NUMBER)
	{
		for (FCombatantTurnDebt &Combatant : State)
		{
			Combatant.TurnsOwed += Combatant.SpeedRatio;
		}
	}

	// Find highest debt
	FCombatantTurnDebt *NextCombatant = nullptr;
	float HighestDebt = -FLT_MAX;

	for (FCombatantTurnDebt &Combatant : State)
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
		return false;
	}

	// Record the slot — a NORMAL turn. Bonus identity now comes solely from the
	// pinned-fire step above; the debt pick never flags.
	OutEntry.Actor = NextCombatant->Actor;
	OutEntry.bIsBonusTurn = false;
	OutEntry.bIsForced = false;
	NextCombatant->TurnsTaken++;

	// Normal-turn delay countdown (Cluster 2b): a NORMAL turn was picked, so every pinned
	// entry burns one turn of delay. Deliberately NOT run on the pinned-fire path above —
	// bonus turns don't decrement other bonuses' delay.
	for (FScheduledTurn &Entry : Pending)
	{
		Entry.TurnsRemaining--;
	}

	return true;
}

void UTurnManager::RebuildBelt()
{
	TArray<FCombatantTurnDebt> Scratch = Combatants;
	TArray<FScheduledTurn> ScratchPending = PendingTurns;

	TurnBelt.Reset();
	for (int32 i = 0; i < CombatConstants::TURN_BELT_HORIZON; ++i)
	{
		FPreviewTurnEntry Entry;
		if (!AdvanceSimState(Scratch, ScratchPending, Entry))
		{
			break;
		}
		TurnBelt.Add(Entry);
	}
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
		return TEXT("PendingTurns: (none).");
	}

	FString Out = FString::Printf(TEXT("PendingTurns (%d pinned):"), PendingTurns.Num());
	for (const FScheduledTurn &Entry : PendingTurns)
	{
		const FString Name = Entry.Actor ? Entry.Actor->GetName() : TEXT("<invalid>");
		Out += FString::Printf(TEXT("\n  %s - fires after %d normal turn(s), count %d"),
							   *Name, Entry.TurnsRemaining, Entry.Count);
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

void UTurnManager::DebugPrintBelt()
{
	UE_LOG(LogTemp, Display, TEXT("=== TURN BELT (%d entries) ==="), TurnBelt.Num());
	for (int32 i = 0; i < TurnBelt.Num(); i++)
	{
		UE_LOG(LogTemp, Display, TEXT("  %d. %s%s"), i + 1,
			   TurnBelt[i].Actor ? *TurnBelt[i].Actor->GetName() : TEXT("<none>"),
			   TurnBelt[i].bIsBonusTurn ? TEXT(" [BONUS]") : TEXT(""));
	}
	UE_LOG(LogTemp, Display, TEXT("======================"));
}