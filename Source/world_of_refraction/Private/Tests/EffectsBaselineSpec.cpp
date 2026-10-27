// EffectsBaselineSpec.cpp
// B-134: today's effect lifecycle, pinned so B-042 (effects on the ASC) and later effect work can
// prove an identical before/after: apply, stack, refresh, tick-down, expiry, the timing windows
// (fire on application, bDelayFirstExecution, Immediate, Persistent), clear and reset, and the
// re-entrancy regression (2405fa82: an effect whose logic removes effects while the turn-start
// pass is walking the same list). Each case records what the code does today, including the
// surprising parts (a DOT fires on application; a no-refresh re-apply still reports
// DurationRefreshed), so change a case only on purpose. Cleanse's gear exclusion is
// WoR.Effects.CleanseGear; holder-turn spans are WoR.Effects.StatusBreaks.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterDataComponent.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 BASELINE_BUFF_ID = 9401;
	constexpr int32 BASELINE_DEBUFF_ID = 9402;
	constexpr int32 BASELINE_SECOND_DEBUFF_ID = 9403;
	constexpr int32 BASELINE_DOT_ID = 9404;
	constexpr int32 BASELINE_CLEANSE_ID = 9405;
	constexpr float BASELINE_BUFF_VALUE = 20.0f;
	constexpr float BASELINE_STRONGER_BUFF_VALUE = 25.0f;
	constexpr float BASELINE_DOT_DAMAGE = 5.0f;
	constexpr int32 BASELINE_DURATION_TURNS = 3;
	constexpr int32 BASELINE_MAX_STACKS = 2;
}

BEGIN_DEFINE_SPEC(FEffectsBaselineSpec, "WoR.Effects.Baseline",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	USkillEffectManager *EffectManager = nullptr;
	AActor *Holder = nullptr;
	AActor *Foe = nullptr;
	UCharacterDataComponent *HolderStats = nullptr;

	/** GetActiveEffects returns copies, so a found effect is copied here; valid until the next Find. */
	FActiveSkillEffect FoundEffect;

	static FActiveSkillEffect Buff(float Value = BASELINE_BUFF_VALUE)
	{
		return FActiveSkillEffect::CreateBuff(TEXT("Baseline buff"), BASELINE_BUFF_ID, ESkillEffectType::DamageBuff,
											  Value, BASELINE_DURATION_TURNS);
	}

	static FActiveSkillEffect StackingBuff(float Value = BASELINE_BUFF_VALUE)
	{
		FActiveSkillEffect Effect = Buff(Value);
		Effect.bCanStack = true;
		Effect.MaxStacks = BASELINE_MAX_STACKS;
		return Effect;
	}

	static FActiveSkillEffect Debuff(int32 ID)
	{
		return FActiveSkillEffect::CreateBuff(TEXT("Baseline debuff"), ID, ESkillEffectType::DefenseDebuff,
											  BASELINE_BUFF_VALUE, BASELINE_DURATION_TURNS);
	}

	static FActiveSkillEffect Burn(ESkillEffectTiming Timing = ESkillEffectTiming::EndOfOwnTurn)
	{
		FActiveSkillEffect Effect = FActiveSkillEffect::CreateDOT(TEXT("Baseline burn"), BASELINE_DOT_ID, BASELINE_DOT_DAMAGE,
																  BASELINE_DURATION_TURNS, ESpellElement::Fire);
		Effect.ProcessTiming = Timing;
		return Effect;
	}

	static int32 Tick() { return FMath::RoundToInt(BASELINE_DOT_DAMAGE); }

	const FActiveSkillEffect *Find(int32 ID)
	{
		for (const FActiveSkillEffect &Effect : EffectManager->GetActiveEffects(Holder))
		{
			if (Effect.EffectID == ID)
			{
				FoundEffect = Effect;
				return &FoundEffect;
			}
		}
		return nullptr;
	}

	/** The effect's remaining turns, or INDEX_NONE (with a failed check) when it's gone. */
	int32 TurnsLeft(int32 ID)
	{
		const FActiveSkillEffect *Effect = Find(ID);
		TestNotNull(FString::Printf(TEXT("Effect %d present"), ID), Effect);
		return Effect ? Effect->RemainingTurns : INDEX_NONE;
	}

	/** One of the holder's own turns ending: end-of-turn effects run, then durations tick. */
	void EndHolderTurn() { EffectManager->ProcessEndOfTurnEffects(Holder); }
END_DEFINE_SPEC(FEffectsBaselineSpec)

void FEffectsBaselineSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		EffectManager = TestWorld->GetSubsystem<USkillEffectManager>();
		Holder = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
		HolderStats = Holder ? Holder->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (!TestNotNull(TEXT("SkillEffectManager"), EffectManager) || !TestNotNull(TEXT("Holder stats"), HolderStats))
		{
			EffectManager = nullptr; // each case returns early
		}
	});

	AfterEach([this]()
	{
		Holder = nullptr;
		Foe = nullptr;
		HolderStats = nullptr;
		EffectManager = nullptr;
		TestWorld.Reset();
	});

	Describe("Apply", [this]()
	{
		It("adds a new effect with its full duration", [this]()
		{
			if (!EffectManager) return;
			TestTrue(TEXT("Applied"), EffectManager->ApplyEffect(Holder, Buff(), Foe) == EEffectApplicationResult::Applied);
			TestEqual(TEXT("Full duration"), TurnsLeft(BASELINE_BUFF_ID), BASELINE_DURATION_TURNS);
			TestEqual(TEXT("One effect"), EffectManager->GetEffectCount(Holder), 1);
		});

		It("refreshes a non-stacking effect re-applied after it ticked", [this]()
		{
			if (!EffectManager) return;
			EffectManager->ApplyEffect(Holder, Buff(), Foe);
			EndHolderTurn();
			TestEqual(TEXT("Ticked once"), TurnsLeft(BASELINE_BUFF_ID), BASELINE_DURATION_TURNS - 1);
			TestTrue(TEXT("Refreshed"), EffectManager->ApplyEffect(Holder, Buff(), Foe) == EEffectApplicationResult::DurationRefreshed);
			TestEqual(TEXT("Back to full"), TurnsLeft(BASELINE_BUFF_ID), BASELINE_DURATION_TURNS);
			TestEqual(TEXT("Still one effect"), EffectManager->GetEffectCount(Holder), 1);
		});

		It("replaces the value but keeps the duration when refresh is off (still reports DurationRefreshed)", [this]()
		{
			if (!EffectManager) return;
			FActiveSkillEffect Effect = Buff();
			Effect.bRefreshDurationOnReapply = false;
			EffectManager->ApplyEffect(Holder, Effect, Foe);
			EndHolderTurn();
			Effect.EffectValue = BASELINE_STRONGER_BUFF_VALUE;
			TestTrue(TEXT("Reported as a refresh"),
					 EffectManager->ApplyEffect(Holder, Effect, Foe) == EEffectApplicationResult::DurationRefreshed);
			TestEqual(TEXT("Duration not reset"), TurnsLeft(BASELINE_BUFF_ID), BASELINE_DURATION_TURNS - 1);
			const FActiveSkillEffect *Stored = Find(BASELINE_BUFF_ID);
			if (!TestNotNull(TEXT("Stored"), Stored)) return;
			TestEqual(TEXT("Value replaced"), Stored->EffectValue, BASELINE_STRONGER_BUFF_VALUE);
		});
	});

	Describe("Stack", [this]()
	{
		It("adds a stack (refreshing the duration and taking the new per-stack value) up to the cap, then refreshes", [this]()
		{
			if (!EffectManager) return;
			EffectManager->ApplyEffect(Holder, StackingBuff(), Foe);
			EndHolderTurn();
			TestTrue(TEXT("Second application stacks"),
					 EffectManager->ApplyEffect(Holder, StackingBuff(BASELINE_STRONGER_BUFF_VALUE), Foe) == EEffectApplicationResult::StackAdded);
			const FActiveSkillEffect *Stacked = Find(BASELINE_BUFF_ID);
			if (!TestNotNull(TEXT("Stacked"), Stacked)) return;
			TestEqual(TEXT("Two stacks"), Stacked->CurrentStacks, BASELINE_MAX_STACKS);
			TestEqual(TEXT("Duration refreshed"), Stacked->RemainingTurns, BASELINE_DURATION_TURNS);
			TestEqual(TEXT("New per-stack value"), Stacked->EffectValue, BASELINE_STRONGER_BUFF_VALUE);
			TestEqual(TEXT("Stacked total"), EffectManager->GetTotalStatModifier(Holder, ESkillEffectType::DamageBuff),
					  BASELINE_STRONGER_BUFF_VALUE * BASELINE_MAX_STACKS);

			TestTrue(TEXT("At the cap it refreshes"),
					 EffectManager->ApplyEffect(Holder, StackingBuff(BASELINE_STRONGER_BUFF_VALUE), Foe) == EEffectApplicationResult::DurationRefreshed);
			const FActiveSkillEffect *Capped = Find(BASELINE_BUFF_ID);
			if (!TestNotNull(TEXT("Capped"), Capped)) return;
			TestEqual(TEXT("Still two stacks"), Capped->CurrentStacks, BASELINE_MAX_STACKS);
			TestEqual(TEXT("One entry"), EffectManager->GetEffectCount(Holder), 1);
		});
	});

	Describe("Tick-down and expiry", [this]()
	{
		It("loses one turn per holder turn and is removed at zero", [this]()
		{
			if (!EffectManager) return;
			EffectManager->ApplyEffect(Holder, Buff(), Foe);
			for (int32 Turn = 1; Turn < BASELINE_DURATION_TURNS; ++Turn)
			{
				EndHolderTurn();
				TestEqual(FString::Printf(TEXT("After turn %d"), Turn), TurnsLeft(BASELINE_BUFF_ID), BASELINE_DURATION_TURNS - Turn);
			}
			EndHolderTurn();
			TestNull(TEXT("Expired"), Find(BASELINE_BUFF_ID));
		});

		It("never ticks a permanent effect", [this]()
		{
			if (!EffectManager) return;
			EffectManager->ApplyEffect(Holder, FActiveSkillEffect::CreatePersistent(TEXT("Baseline aura"), BASELINE_BUFF_ID,
																					ESkillEffectType::DamageBuff, BASELINE_BUFF_VALUE), Foe);
			for (int32 Turn = 0; Turn <= BASELINE_DURATION_TURNS; ++Turn)
			{
				EndHolderTurn();
			}
			TestNotNull(TEXT("Still there"), Find(BASELINE_BUFF_ID));
		});
	});

	Describe("Timing windows", [this]()
	{
		It("fires an end-of-turn DOT on application, skips that turn's end, then hits at the end of each later turn", [this]()
		{
			if (!EffectManager) return;
			const int32 StartHP = HolderStats->CurrentHP;
			EffectManager->ApplyEffect(Holder, Burn(), Foe);
			TestEqual(TEXT("Fires on application"), HolderStats->CurrentHP, StartHP - Tick());
			EndHolderTurn();
			TestEqual(TEXT("Same turn's end is skipped"), HolderStats->CurrentHP, StartHP - Tick());
			EffectManager->ProcessStartOfTurnEffects(Holder);
			TestEqual(TEXT("Nothing at turn start"), HolderStats->CurrentHP, StartHP - Tick());
			EndHolderTurn();
			TestEqual(TEXT("Next turn's end hits"), HolderStats->CurrentHP, StartHP - 2 * Tick());
		});

		It("waits for its first window with bDelayFirstExecution", [this]()
		{
			if (!EffectManager) return;
			const int32 StartHP = HolderStats->CurrentHP;
			FActiveSkillEffect Delayed = Burn();
			Delayed.bDelayFirstExecution = true;
			EffectManager->ApplyEffect(Holder, Delayed, Foe);
			TestEqual(TEXT("No hit on application"), HolderStats->CurrentHP, StartHP);
			EndHolderTurn();
			TestEqual(TEXT("First end of turn hits"), HolderStats->CurrentHP, StartHP - Tick());
		});

		It("runs an Immediate effect once on application, keeps it stored, and sweeps it at the end of the turn", [this]()
		{
			if (!EffectManager) return;
			const int32 StartHP = HolderStats->CurrentHP;
			EffectManager->ApplyEffect(Holder, Burn(ESkillEffectTiming::Immediate), Foe);
			TestEqual(TEXT("Hit once"), HolderStats->CurrentHP, StartHP - Tick());
			TestEqual(TEXT("Still stored"), EffectManager->GetEffectCount(Holder), 1);
			EffectManager->ProcessStartOfTurnEffects(Holder);
			EndHolderTurn();
			TestEqual(TEXT("No second hit"), HolderStats->CurrentHP, StartHP - Tick());
			TestEqual(TEXT("Swept"), EffectManager->GetEffectCount(Holder), 0);
		});

		It("re-runs a Persistent effect at every turn start", [this]()
		{
			if (!EffectManager) return;
			const int32 StartHP = HolderStats->CurrentHP;
			FActiveSkillEffect Aura = Burn(ESkillEffectTiming::Persistent);
			Aura.bPermanent = true;
			EffectManager->ApplyEffect(Holder, Aura, Foe);
			TestEqual(TEXT("Fires on application"), HolderStats->CurrentHP, StartHP - Tick());
			EffectManager->ProcessStartOfTurnEffects(Holder);
			TestEqual(TEXT("First turn start"), HolderStats->CurrentHP, StartHP - 2 * Tick());
			EndHolderTurn();
			EffectManager->ProcessStartOfTurnEffects(Holder);
			TestEqual(TEXT("Second turn start"), HolderStats->CurrentHP, StartHP - 3 * Tick());
		});
	});

	Describe("Clear", [this]()
	{
		It("removes every effect from one holder", [this]()
		{
			if (!EffectManager) return;
			EffectManager->ApplyEffect(Holder, Buff(), Foe);
			EffectManager->ApplyEffect(Holder, Debuff(BASELINE_DEBUFF_ID), Foe);
			EffectManager->ApplyEffect(Foe, Buff(), Holder);
			EffectManager->RemoveAllEffects(Holder);
			TestEqual(TEXT("Holder clear"), EffectManager->GetEffectCount(Holder), 0);
			TestEqual(TEXT("Foe untouched"), EffectManager->GetEffectCount(Foe), 1);
		});

		It("clears effects with ClearAllEffects, and only ResetForNewCombat resets the once-per-match record", [this]()
		{
			if (!EffectManager) return;
			FActiveSkillEffect Once = Buff();
			Once.bFiresOncePerMatch = true;
			EffectManager->ApplyEffect(Holder, Once, Foe);
			EffectManager->ApplyEffect(Foe, Buff(), Holder);

			EffectManager->ClearAllEffects();
			TestEqual(TEXT("Holder clear"), EffectManager->GetEffectCount(Holder), 0);
			TestEqual(TEXT("Foe clear"), EffectManager->GetEffectCount(Foe), 0);
			TestTrue(TEXT("Clearing keeps the once-per-match record"),
					 EffectManager->ApplyEffect(Holder, Once, Foe) == EEffectApplicationResult::Rejected);

			EffectManager->ApplyEffect(Foe, Buff(), Holder);
			EffectManager->ResetForNewCombat();
			TestEqual(TEXT("Reset leaves effects"), EffectManager->GetEffectCount(Foe), 1);
			TestTrue(TEXT("Once-per-match works again"),
					 EffectManager->ApplyEffect(Holder, Once, Foe) == EEffectApplicationResult::Applied);
		});
	});

	Describe("Re-entrancy (2405fa82)", [this]()
	{
		It("skips an effect a turn-start cleanse removed earlier in the same pass", [this]()
		{
			if (!EffectManager) return;
			FActiveSkillEffect Cleanse = FActiveSkillEffect::CreateBuff(TEXT("Baseline cleanse"), BASELINE_CLEANSE_ID,
																		ESkillEffectType::CleanseSelf, 0.0f, BASELINE_DURATION_TURNS);
			Cleanse.bDelayFirstExecution = true;
			EffectManager->ApplyEffect(Holder, Debuff(BASELINE_DEBUFF_ID), Foe);
			EffectManager->ApplyEffect(Holder, Cleanse, Holder);
			FActiveSkillEffect LaterBurn = Burn(ESkillEffectTiming::StartOfOwnTurn);
			LaterBurn.bDelayFirstExecution = true;
			EffectManager->ApplyEffect(Holder, LaterBurn, Foe);
			TestEqual(TEXT("All three stored before the pass"), EffectManager->GetEffectCount(Holder), 3);
			const int32 StartHP = HolderStats->CurrentHP;

			EffectManager->ProcessStartOfTurnEffects(Holder);
			TestNull(TEXT("Debuff cleansed"), Find(BASELINE_DEBUFF_ID));
			TestNull(TEXT("Burn cleansed"), Find(BASELINE_DOT_ID));
			TestEqual(TEXT("The cleansed burn never ran"), HolderStats->CurrentHP, StartHP);
			TestNotNull(TEXT("The cleanse stays"), Find(BASELINE_CLEANSE_ID));
			TestEqual(TEXT("Only the cleanse left"), EffectManager->GetEffectCount(Holder), 1);
		});

		// TODO: the "add" half of 2405fa82 (an effect appended mid-pass waits for its own window)
		// needs a DOT whose buildup caps a bar and triggers a status; pin it with B-135's buildup spec.
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
