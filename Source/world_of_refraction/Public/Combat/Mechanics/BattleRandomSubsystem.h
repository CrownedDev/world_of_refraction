// BattleRandomSubsystem.h
// One seeded random stream per battle (B-010). Combat rolls (crit, Broken Darkness, AI
// choices, random targets and effects, in-fight items) draw from it, so a fight replays
// exactly from the seed it logs at combat start. World-scoped: it lives and dies with the
// battle map, so it holds no per-fight state on the GameInstance (see D-064).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Math/RandomStream.h"
#include "BattleRandomSubsystem.generated.h"

UCLASS()
class WORLD_OF_REFRACTION_API UBattleRandomSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase &Collection) override;

	/** Seeds the stream for a new battle: `wor.Battle.Seed` when non-zero, else a fresh seed.
	 *  Logs the seed (so the fight can be replayed) and returns it. */
	int32 BeginBattle();

	/** Restarts the stream from NewSeed (tests, replays). */
	void SeedStream(int32 NewSeed);

	int32 GetSeed() const { return Seed; }

	/** Draws taken since the last seed — tells two runs apart when the debug print is compared. */
	int32 GetDrawCount() const { return DrawCount; }

	/** The battle stream of WorldContext's world, or null with no world. */
	static UBattleRandomSubsystem *Get(const UObject *WorldContext);

	// Draws from WorldContext's battle stream; the global FMath RNG when there is no world.
	// Same ranges as their FMath namesakes (RandRange is inclusive of Max).
	static float FRand(const UObject *WorldContext);
	static float FRandRange(const UObject *WorldContext, float Min, float Max);
	static int32 RandRange(const UObject *WorldContext, int32 Min, int32 Max);
	static bool RandBool(const UObject *WorldContext);

private:
	FRandomStream Stream;
	int32 Seed = 0;
	int32 DrawCount = 0;
};
