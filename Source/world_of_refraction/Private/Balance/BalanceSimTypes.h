// BalanceSimTypes.h
// B-292 (docs/Design/BalanceSimulator.md §1, §4): the balance simulator's data. An archetype is one combatant built in
// code; a matchup puts a party against enemies at a rank, AI difficulty and zone tier; a fight record is what one
// seeded fight produced. Plain structs: nothing here is saved or shown to the player.

#pragma once

#include "CoreMinimal.h"
#include "AI/EAIDifficulty.h"
#include "Character/ECharacterClass.h"
#include "Character/EWorldPillar.h"
#include "Combat/Encounter/EEncounterRank.h"
#include "Inventory/ItemTier.h"
#include "Skills/Definitions/EScalingTier.h"
#include "Trial/TrialGate.h"

/** One combatant, built as transient assets (BaselineNumbers.md §3-4). Pillars are world levels 0..7. */
struct FBalanceArchetype
{
	FName Id;
	ECharacterClass Class = ECharacterClass::Generic;
	int32 Mind = 0;
	int32 Body = 0;
	int32 Spirit = 0;
	int32 RawDamage = 0;
	int32 Defense = 0;
	int32 MaxHealth = 0;
	int32 Luck = 0;
	int32 SpellDamage = 0;
	EItemTier WeaponTier = EItemTier::F_Tier;
	int32 WeaponPower = 0;
	/** The pillar that adds to the weapon's Power, how steeply (S .. F), and the level the weapon asks of it. */
	EWorldPillar ScalingPillar = EWorldPillar::Body;
	EScalingTier ScalingTier = EScalingTier::C;
	int32 RequiredLevel = 0;
	/** Skill part of the weapon attack and of the one slotted ability (0 = no ability). */
	int32 AttackBase = 0;
	int32 AbilityBase = 0;
};

/** A party against enemies. Enemy count and rank come from the encounter (Normal 2-3, Mini-boss / Boss 1). */
struct FBalanceMatchup
{
	FName Id;
	TArray<FName> Party;
	FName Enemy;
	int32 EnemyCount = 1;
	EEncounterRank Rank = EEncounterRank::Normal;
	EAIDifficulty Difficulty = EAIDifficulty::Medium;
	int32 ZoneTier = 0;
	ETrialDifficulty TrialDifficulty = ETrialDifficulty::Medium;
};

enum class EBalanceFightOutcome : uint8
{
	PartyWon,
	EnemiesWon,
	Draw,
	/** Hit the tick cap: a bug, never a draw (BalanceSimulator.md §1). */
	Stalled
};

/** What one seeded fight produced. Equal records for one seed = a deterministic fight. */
struct FBalanceFightRecord
{
	FName MatchupId;
	int32 Seed = 0;
	EBalanceFightOutcome Outcome = EBalanceFightOutcome::Stalled;
	int32 Ticks = 0;
	int32 Turns = 0;
	int32 PartyTurns = 0;
	int32 Rounds = 0;
	int32 PartyDamage = 0;
	int32 EnemyDamage = 0;
	int32 Hits = 0;
	int32 Crits = 0;
	int32 StatusBreaks = 0;
	/** Damage events landed on each combatant that died, before it died, in death order. */
	TArray<int32> HitsToKill;
	/** Every damage number in order (the determinism check compares it). */
	TArray<int32> DamageSequence;

	bool operator==(const FBalanceFightRecord &Other) const
	{
		return MatchupId == Other.MatchupId && Seed == Other.Seed && Outcome == Other.Outcome && Turns == Other.Turns &&
			   PartyTurns == Other.PartyTurns && Rounds == Other.Rounds && PartyDamage == Other.PartyDamage &&
			   EnemyDamage == Other.EnemyDamage && Hits == Other.Hits && Crits == Other.Crits &&
			   StatusBreaks == Other.StatusBreaks && HitsToKill == Other.HitsToKill && DamageSequence == Other.DamageSequence;
	}

	/** One line for logs and the debug print. */
	FString ToString() const
	{
		static const TCHAR *OutcomeNames[] = {TEXT("PartyWon"), TEXT("EnemiesWon"), TEXT("Draw"), TEXT("Stalled")};
		return FString::Printf(TEXT("%s seed %d: %s, %d turns (%d party), %d rounds, %d ticks, dmg %d / %d, hits %d (crits %d), breaks %d, hits-to-kill [%s]"),
							   *MatchupId.ToString(), Seed, OutcomeNames[static_cast<int32>(Outcome)], Turns, PartyTurns, Rounds,
							   Ticks, PartyDamage, EnemyDamage, Hits, Crits, StatusBreaks,
							   *FString::JoinBy(HitsToKill, TEXT(","), [](int32 Value) { return FString::FromInt(Value); }));
	}
};

/** Means over a matchup's fights. Stalled fights are counted, not averaged. */
struct FBalanceAggregate
{
	FName MatchupId;
	int32 Fights = 0;
	int32 Stalled = 0;
	float PartyWinRate = 0.0f;
	float MeanTurns = 0.0f;
	float MeanPartyTurns = 0.0f;
	float MeanRounds = 0.0f;
	float MeanHitsToKill = 0.0f;

	static FBalanceAggregate From(const TArray<FBalanceFightRecord> &Records)
	{
		FBalanceAggregate Out;
		int32 Finished = 0;
		int32 Wins = 0;
		int32 Kills = 0;
		for (const FBalanceFightRecord &Record : Records)
		{
			Out.MatchupId = Record.MatchupId;
			++Out.Fights;
			if (Record.Outcome == EBalanceFightOutcome::Stalled)
			{
				++Out.Stalled;
				continue;
			}
			++Finished;
			Wins += Record.Outcome == EBalanceFightOutcome::PartyWon ? 1 : 0;
			Out.MeanTurns += Record.Turns;
			Out.MeanPartyTurns += Record.PartyTurns;
			Out.MeanRounds += Record.Rounds;
			for (const int32 Hits : Record.HitsToKill)
			{
				Out.MeanHitsToKill += Hits;
				++Kills;
			}
		}
		if (Finished > 0)
		{
			Out.PartyWinRate = static_cast<float>(Wins) / Finished;
			Out.MeanTurns /= Finished;
			Out.MeanPartyTurns /= Finished;
			Out.MeanRounds /= Finished;
		}
		Out.MeanHitsToKill = Kills > 0 ? Out.MeanHitsToKill / Kills : 0.0f;
		return Out;
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("%s: %d fights (%d stalled), party wins %.0f%%, turns %.1f (party %.1f), rounds %.1f, hits-to-kill %.1f"),
							   *MatchupId.ToString(), Fights, Stalled, PartyWinRate * 100.0f, MeanTurns, MeanPartyTurns,
							   MeanRounds, MeanHitsToKill);
	}
};
