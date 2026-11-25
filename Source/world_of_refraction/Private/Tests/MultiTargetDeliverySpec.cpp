// MultiTargetDeliverySpec.cpp
// B-607 (D-275): a Projectile cast at three targets. OnePerTarget (Slash Volley) sends a copy at each, so each is hit
// after its own straight distance; OneBigWave (Fan Slash) sends one wave toward their middle, so two side by side are
// hit together and the one behind them later, and no projectile actor flies. The timing (WoRMultiTargetDelivery) is
// checked on its own, then through a real cast: a hero and three foes, the hit times read off the world clock. Cast
// headless as in WoR.Combat.OrchestratorActions (montage-less, wor.Combat.InstantImpacts 1); the foes are
// player-driven, so no defense is rolled for them.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "HAL/IConsoleManager.h"
#include "EngineUtils.h"
#include "Components/SceneComponent.h"
#include "WoRTestWorld.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/Actions/ActionExecutorInternal.h"
#include "Combat/Projectile/SkillProjectile.h"
#include "Skills/Definitions/MultiTargetDelivery.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr float MULTI_TARGET_SPEED = 1500.0f;
	/** Two foes side by side at this depth, this far to each side, and one straight behind them at the far depth. */
	constexpr float MULTI_TARGET_NEAR_DEPTH = 300.0f;
	constexpr float MULTI_TARGET_SIDE_OFFSET = 300.0f;
	constexpr float MULTI_TARGET_FAR_DEPTH = 900.0f;
	constexpr int32 MULTI_TARGET_THREE_FOES = 3;
	/** The timer and the projectile land within a frame or two of the computed time (0.01 s frames). */
	constexpr float MULTI_TARGET_TIME_TOLERANCE = 0.03f;
	constexpr float MULTI_TARGET_EXACT_TOLERANCE = 0.001f;
	constexpr int32 MULTI_TARGET_SPELL_DAMAGE = 40;
	constexpr int32 MULTI_TARGET_SPELL_EP = 5;
	constexpr int32 MULTI_TARGET_START_EP = 40;
	constexpr int32 MULTI_TARGET_TOUGH_HP = 10000;
	constexpr float MULTI_TARGET_FULL_DEBUFF_PERCENT = 100.0f;
	constexpr int32 MULTI_TARGET_EFFECT_TURNS = 3;
	constexpr int32 MULTI_TARGET_NO_CRIT_ID = 0;
	constexpr int32 MULTI_TARGET_NO_LUCK_ID = 1;
	constexpr int32 MULTI_TARGET_BATTLE_SEED = 6070;
	/** Spans the executor's 10 s action timeout at 0.01 s a frame; a run stops once the turn moves on. */
	constexpr int32 MULTI_TARGET_SETTLE_FRAMES = 1100;
	constexpr float MULTI_TARGET_NOT_HIT = -1.0f;
	/** A skill window expecting two impacts (the window-rule case). */
	constexpr int32 MULTI_TARGET_SKILL_HITS = 2;

	void SetMultiTargetCVar(const TCHAR *Name, int32 Value)
	{
		if (IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			CVar->Set(Value, ECVF_SetByCode);
		}
	}

	/** Near left, near right, far middle: the middle of the three is straight ahead (+X). */
	TArray<FVector> FoeSpots()
	{
		return {FVector(MULTI_TARGET_NEAR_DEPTH, -MULTI_TARGET_SIDE_OFFSET, 0.0f),
				FVector(MULTI_TARGET_NEAR_DEPTH, MULTI_TARGET_SIDE_OFFSET, 0.0f),
				FVector(MULTI_TARGET_FAR_DEPTH, 0.0f, 0.0f)};
	}
}

BEGIN_DEFINE_SPEC(FMultiTargetDeliverySpec, "WoR.Skills.MultiTargetDelivery",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ACombatOrchestrator *Orchestrator = nullptr;
	USpellData *Spell = nullptr;

	/** What one cast at the three foes did. */
	struct FCastOutcome
	{
		bool bRan = false;
		/** Seconds from the submit to each foe's first lost HP, in FoeSpots' order; MULTI_TARGET_NOT_HIT if never. */
		TArray<float> HitSeconds;
		int32 MostProjectilesInFlight = 0;
		bool bTurnMovedOn = false;
	};

	static UCharacterDataComponent *StatsOf(const AActor *Actor)
	{
		return Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	}

	static void PlaceAt(AActor *Actor, const FVector &Location)
	{
		if (!Actor->GetRootComponent())
		{
			USceneComponent *Root = NewObject<USceneComponent>(Actor);
			Actor->SetRootComponent(Root);
			Root->RegisterComponent();
		}
		Actor->SetActorLocation(Location);
	}

	/** A rooted, montage-less Generic Projectile spell at every enemy (Generic: castable by the test character). */
	USpellData *MakeSpell(EMultiTargetDelivery Mode)
	{
		FSkillCastEntry Entry;
		Entry.DeliveryType = ESpellDeliveryType::Projectile;
		Entry.ProjectileSpeed = MULTI_TARGET_SPEED;
		USpellData *NewSpell = NewObject<USpellData>(GetTransientPackage());
		NewSpell->Name = TEXT("Montage-less Multi-Target Spell");
		NewSpell->BaseDamage = MULTI_TARGET_SPELL_DAMAGE;
		NewSpell->BaseEnergyCost = MULTI_TARGET_SPELL_EP;
		NewSpell->ExecutionType = EAbilityExecutionType::Ranged;
		NewSpell->TargetType = ETargetType::Enemy;
		NewSpell->TargetCount = ETargetCount::All;
		NewSpell->Element = ESpellElement::Generic;
		NewSpell->MultiTargetDelivery = Mode;
		NewSpell->CastArray = {Entry};
		NewSpell->DeliveryType = Entry.DeliveryType;
		NewSpell->AddToRoot();
		return NewSpell;
	}

	/** The hero at the origin casts Spell at three tough foes at FoeSpots, until the turn moves on. */
	FCastOutcome RunCast()
	{
		FCastOutcome Outcome;
		TestWorld = MakeUnique<FWoRTestWorld>();
		if (!Spell || !TestWorld->Create() || !TestWorld->GetWorld())
		{
			return Outcome;
		}
		UWorld *World = TestWorld->GetWorld();
		Orchestrator = World->SpawnActor<ACombatOrchestrator>();
		USkillEffectManager *Effects = TestWorld->GetSubsystem<USkillEffectManager>();
		AActor *Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		TArray<AActor *> Foes;
		for (int32 Index = 0; Index < MULTI_TARGET_THREE_FOES; ++Index)
		{
			Foes.Add(TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData()));
		}
		if (!Orchestrator || !Effects || !Hero || Foes.Contains(nullptr))
		{
			return Outcome;
		}
		Orchestrator->StartCombat({Hero}, Foes);
		if (Orchestrator->GetCurrentActor() != Hero)
		{
			return Outcome;
		}
		UCharacterDataComponent *HeroStats = StatsOf(Hero);
		HeroStats->MaxEP = MULTI_TARGET_START_EP;
		HeroStats->CurrentEP = MULTI_TARGET_START_EP;
		Effects->ApplyEffect(Hero, FActiveSkillEffect::CreateBuff(TEXT("No crit"), MULTI_TARGET_NO_CRIT_ID, ESkillEffectType::CritChanceDebuff,
																  MULTI_TARGET_FULL_DEBUFF_PERCENT, MULTI_TARGET_EFFECT_TURNS), Hero);
		PlaceAt(Hero, FVector::ZeroVector);
		const TArray<FVector> Spots = FoeSpots();
		for (int32 Index = 0; Index < Foes.Num(); ++Index)
		{
			UCharacterDataComponent *FoeStats = StatsOf(Foes[Index]);
			FoeStats->MaxHP = MULTI_TARGET_TOUGH_HP;
			FoeStats->CurrentHP = MULTI_TARGET_TOUGH_HP;
			Effects->ApplyEffect(Foes[Index], FActiveSkillEffect::CreateBuff(TEXT("No luck"), MULTI_TARGET_NO_LUCK_ID, ESkillEffectType::LuckDebuff,
																			 MULTI_TARGET_FULL_DEBUFF_PERCENT, MULTI_TARGET_EFFECT_TURNS), Foes[Index]);
			PlaceAt(Foes[Index], Spots[Index]);
		}

		FAction Action;
		Action.ActionType = EActionType::Spell;
		Action.SpellData = Spell;
		Action.Targets = Foes;
		Outcome.HitSeconds.Init(MULTI_TARGET_NOT_HIT, Foes.Num());
		const double SubmitTime = World->GetTimeSeconds();
		Orchestrator->SubmitActionAsync(Action);
		for (int32 Frame = 0; Frame < MULTI_TARGET_SETTLE_FRAMES && Orchestrator->GetCurrentActor() == Hero; ++Frame)
		{
			TestWorld->Tick();
			int32 InFlight = 0;
			for (TActorIterator<ASkillProjectile> It(World); It; ++It)
			{
				++InFlight;
			}
			Outcome.MostProjectilesInFlight = FMath::Max(Outcome.MostProjectilesInFlight, InFlight);
			for (int32 Index = 0; Index < Foes.Num(); ++Index)
			{
				if (Outcome.HitSeconds[Index] == MULTI_TARGET_NOT_HIT && StatsOf(Foes[Index])->CurrentHP < MULTI_TARGET_TOUGH_HP)
				{
					Outcome.HitSeconds[Index] = static_cast<float>(World->GetTimeSeconds() - SubmitTime);
				}
			}
		}
		Outcome.bRan = true;
		Outcome.bTurnMovedOn = Orchestrator->GetCurrentActor() != Hero;
		return Outcome;
	}

	/** Every foe was hit, and the turn moved on. */
	bool TestAllHit(const FCastOutcome &Outcome)
	{
		bool bAll = TestTrue(TEXT("The cast ran"), Outcome.bRan) && TestEqual(TEXT("Three foes"), Outcome.HitSeconds.Num(), MULTI_TARGET_THREE_FOES);
		for (int32 Index = 0; bAll && Index < Outcome.HitSeconds.Num(); ++Index)
		{
			bAll = TestTrue(FString::Printf(TEXT("Foe %d was hit"), Index), Outcome.HitSeconds[Index] > 0.0f);
		}
		TestTrue(TEXT("The turn moved on"), Outcome.bTurnMovedOn);
		return bAll;
	}
END_DEFINE_SPEC(FMultiTargetDeliverySpec)

void FMultiTargetDeliverySpec::Define()
{
	Describe("the timing", [this]()
	{
		It("OnePerTarget: each of three is hit after its own straight distance", [this]()
		{
			const TArray<FVector> Spots = FoeSpots();
			const TArray<float> Arrivals = WoRMultiTargetDelivery::GetArrivalSeconds(EMultiTargetDelivery::OnePerTarget, FVector::ZeroVector, Spots,
																					   MULTI_TARGET_SPEED, FVector::ForwardVector);
			if (!TestEqual(TEXT("One time a target"), Arrivals.Num(), Spots.Num()))
			{
				return;
			}
			for (int32 Index = 0; Index < Spots.Num(); ++Index)
			{
				TestEqual(FString::Printf(TEXT("Foe %d: distance / speed"), Index), Arrivals[Index], static_cast<float>(Spots[Index].Size() / MULTI_TARGET_SPEED),
						  MULTI_TARGET_EXACT_TOLERANCE);
			}
		});

		It("OneBigWave: two side by side are hit together, the one behind them later, each by its depth", [this]()
		{
			const TArray<float> Arrivals = WoRMultiTargetDelivery::GetArrivalSeconds(EMultiTargetDelivery::OneBigWave, FVector::ZeroVector, FoeSpots(),
																					   MULTI_TARGET_SPEED, FVector::ForwardVector);
			if (!TestEqual(TEXT("One time a target"), Arrivals.Num(), MULTI_TARGET_THREE_FOES))
			{
				return;
			}
			TestEqual(TEXT("Near left: its depth / speed"), Arrivals[0], MULTI_TARGET_NEAR_DEPTH / MULTI_TARGET_SPEED, MULTI_TARGET_EXACT_TOLERANCE);
			TestEqual(TEXT("Near right: hit with the near left"), Arrivals[1], Arrivals[0], MULTI_TARGET_EXACT_TOLERANCE);
			TestEqual(TEXT("Far: its depth / speed"), Arrivals[2], MULTI_TARGET_FAR_DEPTH / MULTI_TARGET_SPEED, MULTI_TARGET_EXACT_TOLERANCE);
			TestTrue(TEXT("The wave reaches a side foe sooner than a copy flying at it would"),
					 Arrivals[0] < FoeSpots()[0].Size() / MULTI_TARGET_SPEED);
		});

		It("OneBigWave: faces the targets' middle; the caster's forward when they stand on it; a target behind is hit at once", [this]()
		{
			TestEqual(TEXT("Toward the middle, flat"),
					  WoRMultiTargetDelivery::GetWaveDirection(FVector::ZeroVector, {FVector(0.0f, 100.0f, 50.0f), FVector(0.0f, 300.0f, -50.0f)},
															   FVector::ForwardVector),
					  FVector(0.0f, 1.0f, 0.0f));
			TestEqual(TEXT("On the caster: its forward"),
					  WoRMultiTargetDelivery::GetWaveDirection(FVector::ZeroVector, {FVector::ZeroVector}, FVector(0.0f, -1.0f, 0.0f)),
					  FVector(0.0f, -1.0f, 0.0f));
			TestEqual(TEXT("No targets and no forward: +X"),
					  WoRMultiTargetDelivery::GetWaveDirection(FVector::ZeroVector, {}, FVector::ZeroVector), FVector::ForwardVector);
			const TArray<float> Behind = WoRMultiTargetDelivery::GetArrivalSeconds(
				EMultiTargetDelivery::OneBigWave, FVector::ZeroVector,
				{FVector(MULTI_TARGET_FAR_DEPTH, 0.0f, 0.0f), FVector(MULTI_TARGET_FAR_DEPTH, 0.0f, 0.0f), FVector(-MULTI_TARGET_FAR_DEPTH, 0.0f, 0.0f)},
				MULTI_TARGET_SPEED, FVector::ForwardVector);
			TestEqual(TEXT("A target behind the wave's start: at once"), Behind.Last(), 0.0f);
			TestTrue(TEXT("No speed is read as the floor, not a divide by zero"),
					 FMath::IsFinite(WoRMultiTargetDelivery::GetArrivalSeconds(EMultiTargetDelivery::OneBigWave, FVector::ZeroVector,
																			  {FVector(MULTI_TARGET_NEAR_DEPTH, 0.0f, 0.0f)}, 0.0f, FVector::ForwardVector)[0]));
		});
	});

	It("a skill's window waits for its shots when they cover its impacts, not for the swing's end; a spell's always waits", [this]()
	{
		FActionExecutionContext Exec;
		FPendingDefenseContext SkillWindow;
		SkillWindow.ActionType = EActionType::Ability;
		SkillWindow.ExpectedImpacts = MULTI_TARGET_SKILL_HITS;
		FPendingDefenseContext SpellWindow;
		SpellWindow.ActionType = EActionType::Spell;
		TestFalse(TEXT("A swing with nothing sent: closes at the animation's end"), WaitsForArrivals(Exec, SkillWindow));
		TestTrue(TEXT("A spell: waits"), WaitsForArrivals(Exec, SpellWindow));
		Exec.ArrivalsSentPerTarget = MULTI_TARGET_SKILL_HITS - 1;
		TestFalse(TEXT("Fewer shots than its hits: closes at the animation's end (lands the rest there)"), WaitsForArrivals(Exec, SkillWindow));
		Exec.ArrivalsSentPerTarget = MULTI_TARGET_SKILL_HITS;
		TestTrue(TEXT("Shots covering its hits: waits for them"), WaitsForArrivals(Exec, SkillWindow));
		FPendingDefenseContext TimerWindow;
		TimerWindow.ActionType = EActionType::Ability;
		TestFalse(TEXT("A timer-close window (no impacts expected) is not held"), WaitsForArrivals(Exec, TimerWindow));
	});

	Describe("a cast at three foes", [this]()
	{
		BeforeEach([this]()
		{
			SetMultiTargetCVar(TEXT("wor.Combat.InstantImpacts"), 1);
			// One battle seed for every run, so no roll left by an earlier spec decides a damage check.
			SetMultiTargetCVar(TEXT("wor.Battle.Seed"), MULTI_TARGET_BATTLE_SEED);
		});

		AfterEach([this]()
		{
			SetMultiTargetCVar(TEXT("wor.Combat.InstantImpacts"), 0);
			SetMultiTargetCVar(TEXT("wor.Battle.Seed"), 0);
			if (Orchestrator && Orchestrator->GetCombatState() != ECombatState::Idle)
			{
				Orchestrator->ForceEndCombat();
			}
			Orchestrator = nullptr;
			TestWorld.Reset();
			if (Spell)
			{
				Spell->RemoveFromRoot();
				Spell = nullptr;
			}
		});

		It("OnePerTarget: a projectile flies at each, and each lands after its own flight", [this]()
		{
			Spell = MakeSpell(EMultiTargetDelivery::OnePerTarget);
			const FCastOutcome Outcome = RunCast();
			if (!TestAllHit(Outcome))
			{
				return;
			}
			TestEqual(TEXT("Three projectiles in flight at once"), Outcome.MostProjectilesInFlight, MULTI_TARGET_THREE_FOES);
			const TArray<FVector> Spots = FoeSpots();
			TestEqual(TEXT("The two near foes are hit together"), Outcome.HitSeconds[1], Outcome.HitSeconds[0], MULTI_TARGET_TIME_TOLERANCE);
			// The gap, not the times: both carry the same cast-to-launch delay.
			TestEqual(TEXT("The far foe after the flight-time gap"), Outcome.HitSeconds[2] - Outcome.HitSeconds[0],
					  static_cast<float>((Spots[2].Size() - Spots[0].Size()) / MULTI_TARGET_SPEED), MULTI_TARGET_TIME_TOLERANCE);
		});

		It("OneBigWave: no projectile flies, and each is hit when the wave reaches it", [this]()
		{
			Spell = MakeSpell(EMultiTargetDelivery::OneBigWave);
			const FCastOutcome Outcome = RunCast();
			if (!TestAllHit(Outcome))
			{
				return;
			}
			TestEqual(TEXT("No projectile actor"), Outcome.MostProjectilesInFlight, 0);
			TestEqual(TEXT("The two near foes are hit together"), Outcome.HitSeconds[1], Outcome.HitSeconds[0], MULTI_TARGET_TIME_TOLERANCE);
			TestEqual(TEXT("The far foe after the depth gap (later than a copy's flight-time gap)"),
					  Outcome.HitSeconds[2] - Outcome.HitSeconds[0], (MULTI_TARGET_FAR_DEPTH - MULTI_TARGET_NEAR_DEPTH) / MULTI_TARGET_SPEED,
					  MULTI_TARGET_TIME_TOLERANCE);
			TestTrue(TEXT("The near foes are hit no later than their depth / speed after the cast"),
					 Outcome.HitSeconds[0] <= MULTI_TARGET_NEAR_DEPTH / MULTI_TARGET_SPEED + MULTI_TARGET_TIME_TOLERANCE);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
