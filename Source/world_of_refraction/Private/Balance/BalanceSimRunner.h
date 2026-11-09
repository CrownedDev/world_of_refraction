// BalanceSimRunner.h
// B-292 (docs/Design/BalanceSimulator.md §1): runs seeded all-AI fights headless in one reused test world. Each fight
// spawns its combatants from archetypes, seeds the battle stream (wor.Battle.Seed), applies the trial difficulty x zone
// strength (as BattleGameMode does) and the rank HP stand-in, then ticks to the end or the tick cap (Stalled).
// The B-291 seams (instant impacts, no AI think delay) and quiet logging are on between Begin and End.

#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"
#include "Balance/BalanceSimTypes.h"

class ACombatOrchestrator;
class FWoRTestWorld;
class UBalanceSimRecorder;

class FBalanceSimRunner
{
public:
	/** About 3 ticks a turn with the seams on; a fight this long is stuck. */
	static constexpr int32 FIGHT_TICK_CAP = 6000;

	FBalanceSimRunner();
	~FBalanceSimRunner();

	/** Creates the world and turns the seams on. False when the world can't be made. */
	bool Begin();

	/** Restores the console variables and log levels and releases the world. Safe to call twice. */
	void End();

	/** One fight of Matchup on Seed (non-zero: 0 asks the battle stream for a fresh seed). */
	FBalanceFightRecord RunFight(const FBalanceMatchup &Matchup, int32 Seed);

	/** Fights fights of Matchup on seeds BaseSeed + i. */
	TArray<FBalanceFightRecord> RunMatchup(const FBalanceMatchup &Matchup, int32 Fights, int32 BaseSeed);

private:
	AActor *SpawnArchetype(const FBalanceArchetype &Archetype);
	void ApplyEnemyStrength(AActor *Enemy, const FBalanceMatchup &Matchup);
	void ClearFight();

	TUniquePtr<FWoRTestWorld> TestWorld;
	ACombatOrchestrator *Orchestrator = nullptr;
	TStrongObjectPtr<UBalanceSimRecorder> Recorder;
	/** The transient assets of the current fight, kept from garbage collection until it ends. */
	TArray<TStrongObjectPtr<UObject>> FightAssets;
	TArray<AActor *> FightActors;

	int32 SavedInstantImpacts = 0;
	float SavedThinkDelayScale = 1.0f;
	int32 SavedBattleSeed = 0;
	ELogVerbosity::Type SavedLogTemp = ELogVerbosity::Log;
	ELogVerbosity::Type SavedLogCombat = ELogVerbosity::Log;
	ELogVerbosity::Type SavedLogEffects = ELogVerbosity::Log;
	ELogVerbosity::Type SavedLogAI = ELogVerbosity::Log;
	bool bRunning = false;
};

#endif // WITH_DEV_AUTOMATION_TESTS
