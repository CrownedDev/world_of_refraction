// BattleRandomSpec.cpp
// B-010: the per-battle random stream. Same seed, same draws and same crits; the seed
// is set when combat starts, and wor.Battle.Seed forces it for a replay.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "HAL/IConsoleManager.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/Damage/DamageCalculator.h"
#include "Combat/Mechanics/BattleRandomSubsystem.h"

namespace
{
	constexpr int32 BATTLE_RANDOM_SEED = 424242;
	constexpr int32 BATTLE_RANDOM_OTHER_SEED = 1337;
	constexpr int32 BATTLE_RANDOM_FORCED_SEED = 777001;
	constexpr int32 BATTLE_RANDOM_DRAWS = 32;
	constexpr int32 BATTLE_RANDOM_DRAW_KINDS = 4; // FRand, RandRange, FRandRange, RandBool per round
	constexpr int32 BATTLE_RANDOM_INT_MAX = 9;
	constexpr float BATTLE_RANDOM_FLOAT_MIN = -5.0f;
	constexpr float BATTLE_RANDOM_FLOAT_MAX = 5.0f;
	constexpr int32 BATTLE_RANDOM_HITS = 24;
	constexpr float BATTLE_RANDOM_COIN_FLIP_CRIT = 0.5f;
	constexpr int32 BATTLE_RANDOM_BASE_DAMAGE = 10;
	constexpr int32 BATTLE_RANDOM_CVAR_OFF = 0;
	constexpr const TCHAR *BATTLE_SEED_CVAR_NAME = TEXT("wor.Battle.Seed");
}

BEGIN_DEFINE_SPEC(FBattleRandomSpec, "WoR.Combat.BattleRandom",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UBattleRandomSubsystem *BattleRandom = nullptr;

	/** One round of every draw kind, flattened to floats so two runs compare in one array. */
	TArray<float> DrawSequence()
	{
		TArray<float> Draws;
		UWorld *World = TestWorld->GetWorld();
		for (int32 Round = 0; Round < BATTLE_RANDOM_DRAWS; ++Round)
		{
			Draws.Add(UBattleRandomSubsystem::FRand(World));
			Draws.Add(static_cast<float>(UBattleRandomSubsystem::RandRange(World, 0, BATTLE_RANDOM_INT_MAX)));
			Draws.Add(UBattleRandomSubsystem::FRandRange(World, BATTLE_RANDOM_FLOAT_MIN, BATTLE_RANDOM_FLOAT_MAX));
			Draws.Add(UBattleRandomSubsystem::RandBool(World) ? 1.0f : 0.0f);
		}
		return Draws;
	}

	/** A run of coin-flip-crit hits between two fresh combatants: which ones crit, and the damage.
	 *  Reseeds with Seed first unless Seed is 0 (then the stream is used as it stands). */
	TArray<int32> CritRun(int32 Seed)
	{
		TArray<int32> Damage;
		UDamageCalculator *Calculator = TestWorld->GetSubsystem<UDamageCalculator>();
		AActor *Attacker = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		AActor *Defender = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestNotNull(TEXT("DamageCalculator"), Calculator) || !TestTrue(TEXT("Combatants spawned"), Attacker && Defender))
		{
			return Damage;
		}

		FDamageCalculationInput Input;
		Input.BaseDamage = BATTLE_RANDOM_BASE_DAMAGE;
		Input.ActionType = EActionType::Ability;
		Input.OverrideCritChance = BATTLE_RANDOM_COIN_FLIP_CRIT;

		if (Seed != 0)
		{
			BattleRandom->SeedStream(Seed);
		}
		for (int32 Hit = 0; Hit < BATTLE_RANDOM_HITS; ++Hit)
		{
			const FDamageCalculationResult Result = Calculator->CalculateDamage(Attacker, Defender, Input);
			// Negative marks a crit, so one array carries both the crit pattern and the numbers.
			Damage.Add(Result.bWasCritical ? -Result.FinalDamage : Result.FinalDamage);
		}
		return Damage;
	}

	static void SetSeedCVar(int32 Value)
	{
		if (IConsoleVariable *SeedVar = IConsoleManager::Get().FindConsoleVariable(BATTLE_SEED_CVAR_NAME))
		{
			SeedVar->Set(Value, ECVF_SetByCode);
		}
	}
END_DEFINE_SPEC(FBattleRandomSpec)

void FBattleRandomSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		BattleRandom = UBattleRandomSubsystem::Get(TestWorld->GetWorld());
		TestNotNull(TEXT("BattleRandomSubsystem"), BattleRandom);
	});

	AfterEach([this]()
	{
		SetSeedCVar(BATTLE_RANDOM_CVAR_OFF);
		BattleRandom = nullptr;
		TestWorld.Reset();
	});

	It("repeats the same draws for the same seed", [this]()
	{
		if (!BattleRandom)
		{
			return;
		}
		BattleRandom->SeedStream(BATTLE_RANDOM_SEED);
		const TArray<float> First = DrawSequence();
		TestEqual(TEXT("Every draw is counted"), BattleRandom->GetDrawCount(), BATTLE_RANDOM_DRAWS * BATTLE_RANDOM_DRAW_KINDS);

		BattleRandom->SeedStream(BATTLE_RANDOM_SEED);
		TestEqual(TEXT("Reseeding resets the draw count"), BattleRandom->GetDrawCount(), 0);
		const TArray<float> Second = DrawSequence();
		TestTrue(TEXT("Same seed, same draws"), First == Second);

		BattleRandom->SeedStream(BATTLE_RANDOM_OTHER_SEED);
		TestFalse(TEXT("Another seed, other draws"), First == DrawSequence());
	});

	It("rolls the same crits for the same seed", [this]()
	{
		if (!BattleRandom)
		{
			return;
		}
		const TArray<int32> First = CritRun(BATTLE_RANDOM_SEED);
		const TArray<int32> Second = CritRun(BATTLE_RANDOM_SEED);
		TestEqual(TEXT("Every hit resolved"), First.Num(), BATTLE_RANDOM_HITS);
		TestTrue(TEXT("Same seed, same crits and damage"), First == Second);
		TestTrue(TEXT("The crit roll is live (some hits crit, some don't)"),
				 First.ContainsByPredicate([](int32 D) { return D < 0; }) &&
				 First.ContainsByPredicate([](int32 D) { return D > 0; }));
	});

	It("takes the seed from wor.Battle.Seed when it is set", [this]()
	{
		if (!BattleRandom)
		{
			return;
		}
		SetSeedCVar(BATTLE_RANDOM_FORCED_SEED);
		TestEqual(TEXT("BeginBattle returns the forced seed"), BattleRandom->BeginBattle(), BATTLE_RANDOM_FORCED_SEED);
		TestEqual(TEXT("And the stream uses it"), BattleRandom->GetSeed(), BATTLE_RANDOM_FORCED_SEED);
	});

	It("picks a fresh, non-zero seed when wor.Battle.Seed is 0", [this]()
	{
		if (!BattleRandom)
		{
			return;
		}
		const int32 Seed = BattleRandom->BeginBattle();
		TestNotEqual(TEXT("Seed is never 0 (0 means 'fresh' on the replay command)"), Seed, 0);
		TestEqual(TEXT("GetSeed reports it"), BattleRandom->GetSeed(), Seed);
	});

	It("is seeded when combat starts", [this]()
	{
		UWorld *World = TestWorld->GetWorld();
		ACombatOrchestrator *Orchestrator = World ? World->SpawnActor<ACombatOrchestrator>() : nullptr;
		AActor *Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		AActor *Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!BattleRandom || !TestNotNull(TEXT("Orchestrator"), Orchestrator) || !TestTrue(TEXT("Combatants spawned"), Hero && Foe))
		{
			return;
		}
		BattleRandom->SeedStream(BATTLE_RANDOM_OTHER_SEED);
		SetSeedCVar(BATTLE_RANDOM_FORCED_SEED);

		Orchestrator->StartCombat({Hero}, {Foe});
		TestEqual(TEXT("StartCombat seeded the stream"), BattleRandom->GetSeed(), BATTLE_RANDOM_FORCED_SEED);

		if (Orchestrator->GetCombatState() != ECombatState::Idle)
		{
			Orchestrator->ForceEndCombat();
		}
	});

	It("gives two fights started on the same seed the same rolls", [this]()
	{
		UWorld *World = TestWorld->GetWorld();
		ACombatOrchestrator *Orchestrator = World ? World->SpawnActor<ACombatOrchestrator>() : nullptr;
		if (!BattleRandom || !TestNotNull(TEXT("Orchestrator"), Orchestrator))
		{
			return;
		}
		SetSeedCVar(BATTLE_RANDOM_FORCED_SEED);

		// Each fight: StartCombat seeds, then the fight's crit rolls run on that stream (no reseed).
		TArray<int32> Fights[2];
		for (TArray<int32> &FightRolls : Fights)
		{
			AActor *Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			AActor *Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			if (!TestTrue(TEXT("Combatants spawned"), Hero && Foe))
			{
				return;
			}
			Orchestrator->StartCombat({Hero}, {Foe});
			TestEqual(TEXT("Forced seed applied"), BattleRandom->GetSeed(), BATTLE_RANDOM_FORCED_SEED);
			FightRolls = CritRun(0);
			if (Orchestrator->GetCombatState() != ECombatState::Idle)
			{
				Orchestrator->ForceEndCombat();
			}
		}
		TestEqual(TEXT("Every hit resolved"), Fights[0].Num(), BATTLE_RANDOM_HITS);
		TestTrue(TEXT("Same seed at combat start, same crits and damage"), Fights[0] == Fights[1]);
	});

	It("falls back to the global RNG with no world", [this]()
	{
		TestNull(TEXT("No world, no stream"), UBattleRandomSubsystem::Get(nullptr));
		const float Draw = UBattleRandomSubsystem::FRand(nullptr);
		TestTrue(TEXT("Still a valid draw in [0, 1)"), Draw >= 0.0f && Draw < 1.0f);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
