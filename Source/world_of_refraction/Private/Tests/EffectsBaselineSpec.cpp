// EffectsBaselineSpec.cpp
// B-134: today's effect lifecycle, pinned so B-042 (effects on the ASC) and later effect work can
// prove an identical before/after: apply, stack, refresh, tick-down, expiry, DOT ticks, clear and
// reset, and the re-entrancy regression (an effect whose logic removes effects while the
// turn-start pass is walking the same list). Each case records what the code does today,
// including the surprising parts (a DOT fires on application; a no-refresh re-apply still reports
// DurationRefreshed), so change a case only on purpose.

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

	static FActiveSkillEffect Buff()
	{
		return FActiveSkillEffect::CreateBuff(TEXT("Baseline buff"), BASELINE_BUFF_ID, ESkillEffectType::DamageBuff,
											  BASELINE_BUFF_VALUE, BASELINE_DURATION_TURNS);
	}

	static FActiveSkillEffect StackingBuff()
	{
		FActiveSkillEffect Effect = Buff();
		Effect.bCanStack = true;
		Effect.MaxStacks = BASELINE_MAX_STACKS;
		return Effect;
	}

	static FActiveSkillEffect Debuff(int32 ID)
	{
		return FActiveSkillEffect::CreateBuff(TEXT("Baseline debuff"), ID, ESkillEffectType::DefenseDebuff,
											  BASELINE_BUFF_VALUE, BASELINE_DURATION_TURNS);
	}

	/** GetActiveEffects returns copies, so a found effect is copied here; valid until the next Find. */
	FActiveSkillEffect FoundEffect;

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
		TestNotNull(TEXT("SkillEffectManager"), EffectManager);
		Holder = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
	});

	AfterEach([this]()
	{
		Holder = nullptr;
		Foe = nullptr;
		EffectManager = nullptr;
		TestWorld.Reset();
	});

	Describe("Apply", [this]()
	{
		It("adds a new effect with its full duration", [this]()
		{
			if (!EffectManager) return;
			TestTrue(TEXT("Applied"), EffectManager->ApplyEffect(Holder, Buff(), Foe) == EEffectApplicationResult::Applied);
			const FActiveSkillEffect *Effect = Find(BASELINE_BUFF_ID);
			if (!TestNotNull(TEXT("On the holder"), Effect)) return;
			TestEqual(TEXT("Full duration"), Effect->RemainingTurns, BASELINE_DURATION_TURNS);
			TestEqual(TEXT("One effect"), EffectManager->GetEffectCount(Holder), 1);
		});

		It("refreshes a non-stacking effect re-applied after it ticked", [this]()
		{
			if (!EffectManager) return;
			EffectManager->ApplyEffect(Holder, Buff(), Foe);
			EndHolderTurn();
			TestEqual(TEXT("Ticked once"), Find(BASELINE_BUFF_ID)->RemainingTurns, BASELINE_DURATION_TURNS - 1);
			TestTrue(TEXT("Refreshed"), EffectManager->ApplyEffect(Holder, Buff(), Foe) == EEffectApplicationResult::DurationRefreshed);
			TestEqual(TEXT("Back to full"), Find(BASELINE_BUFF_ID)->RemainingTurns, BASELINE_DURATION_TURNS);
			TestEqual(TEXT("Still one effect"), EffectManager->GetEffectCount(Holder), 1);
		});

		It("replaces the value but keeps the duration when refresh is off (still reports DurationRefreshed)", [this]()
		{
			if (!EffectManager) return;
			FActiveSkillEffect Effect = Buff();
			Effect.bRefreshDurationOnReapply = false;
			EffectManager->ApplyEffect(Holder, Effect, Foe);
			EndHolderTurn();
			Effect.EffectValue = BASELINE_BUFF_VALUE * 2.0f;
			TestTrue(TEXT("Reported as a refresh"),
					 EffectManager->ApplyEffect(Holder, Effect, Foe) == EEffectApplicationResult::DurationRefreshed);
			TestEqual(TEXT("Duration not reset"), Find(BASELINE_BUFF_ID)->RemainingTurns, BASELINE_DURATION_TURNS - 1);
			TestEqual(TEXT("Value replaced"), Find(BASELINE_BUFF_ID)->EffectValue, BASELINE_BUFF_VALUE * 2.0f);
		});
	});

	Describe("Stack", [this]()
	{
		It("adds stacks up to the cap, then refreshes instead", [this]()
		{
			if (!EffectManager) return;
			EffectManager->ApplyEffect(Holder, StackingBuff(), Foe);
			TestTrue(TEXT("Second application stacks"),
					 EffectManager->ApplyEffect(Holder, StackingBuff(), Foe) == EEffectApplicationResult::StackAdded);
			TestEqual(TEXT("Two stacks"), Find(BASELINE_BUFF_ID)->CurrentStacks, BASELINE_MAX_STACKS);
			TestTrue(TEXT("At the cap it refreshes"),
					 EffectManager->ApplyEffect(Holder, StackingBuff(), Foe) == EEffectApplicationResult::DurationRefreshed);
			TestEqual(TEXT("Still two stacks"), Find(BASELINE_BUFF_ID)->CurrentStacks, BASELINE_MAX_STACKS);
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
				TestEqual(FString::Printf(TEXT("After turn %d"), Turn), Find(BASELINE_BUFF_ID)->RemainingTurns,
						  BASELINE_DURATION_TURNS - Turn);
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

		It("hits once when a DOT lands, skips that turn's end, then hits at the end of each later holder turn", [this]()
		{
			if (!EffectManager) return;
			UCharacterDataComponent *Stats = Holder->FindComponentByClass<UCharacterDataComponent>();
			const int32 Tick = FMath::RoundToInt(BASELINE_DOT_DAMAGE);
			const int32 StartHP = Stats->CurrentHP;
			EffectManager->ApplyEffect(Holder, FActiveSkillEffect::CreateDOT(TEXT("Baseline burn"), BASELINE_DOT_ID, BASELINE_DOT_DAMAGE,
																			BASELINE_DURATION_TURNS, ESpellElement::Fire), Foe);
			TestEqual(TEXT("Fires on application"), Stats->CurrentHP, StartHP - Tick);
			EndHolderTurn();
			TestEqual(TEXT("Same turn's end is skipped"), Stats->CurrentHP, StartHP - Tick);
			EffectManager->ProcessStartOfTurnEffects(Holder);
			TestEqual(TEXT("Nothing at turn start"), Stats->CurrentHP, StartHP - Tick);
			EndHolderTurn();
			TestEqual(TEXT("Next turn's end hits"), Stats->CurrentHP, StartHP - 2 * Tick);
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

		It("clears every holder with ClearAllEffects; ResetForNewCombat only resets the once-per-match record", [this]()
		{
			if (!EffectManager) return;
			FActiveSkillEffect Once = Buff();
			Once.bFiresOncePerMatch = true;
			EffectManager->ApplyEffect(Holder, Once, Foe);
			EffectManager->ApplyEffect(Foe, Buff(), Holder);

			EffectManager->ResetForNewCombat();
			TestEqual(TEXT("Reset leaves the holder's effects"), EffectManager->GetEffectCount(Holder), 1);
			EffectManager->ClearAllEffects();
			TestEqual(TEXT("Holder clear"), EffectManager->GetEffectCount(Holder), 0);
			TestEqual(TEXT("Foe clear"), EffectManager->GetEffectCount(Foe), 0);
			TestTrue(TEXT("Once-per-match works again"),
					 EffectManager->ApplyEffect(Holder, Once, Foe) == EEffectApplicationResult::Applied);
		});
	});

	Describe("Re-entrancy (2405fa82)", [this]()
	{
		It("survives a turn-start cleanse that removes effects from the list being walked", [this]()
		{
			if (!EffectManager) return;
			EffectManager->ApplyEffect(Holder, Debuff(BASELINE_DEBUFF_ID), Foe);
			FActiveSkillEffect Cleanse = FActiveSkillEffect::CreateBuff(TEXT("Baseline cleanse"), BASELINE_CLEANSE_ID,
																		ESkillEffectType::CleanseSelf, 0.0f, BASELINE_DURATION_TURNS);
			EffectManager->ApplyEffect(Holder, Cleanse, Holder);
			EffectManager->ApplyEffect(Holder, Debuff(BASELINE_SECOND_DEBUFF_ID), Foe);

			EffectManager->ProcessStartOfTurnEffects(Holder);
			TestNull(TEXT("First debuff cleansed"), Find(BASELINE_DEBUFF_ID));
			TestNull(TEXT("Second debuff cleansed"), Find(BASELINE_SECOND_DEBUFF_ID));
			TestNotNull(TEXT("The cleanse stays"), Find(BASELINE_CLEANSE_ID));
			TestEqual(TEXT("Only the cleanse left"), EffectManager->GetEffectCount(Holder), 1);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
