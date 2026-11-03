// StatusBreakSpec.cpp
// Status breaks (B-063): a full bar's effect lasts STATUS_CAP_TURNS of the holder's own turns,
// counted by the one rule (FTurnSpan, D-051): the landing turn counts if it is the holder's,
// and the break ends as the holder's turn after those starts, before its payouts.
// Also: Burn pays out exactly once per counted turn; Water slows Action + Spell Speed;
// Broken Darkness drains EP; Wind pushes the holder's next turn back one place.
// Break sizes (B-286): SB-19 Burn 8% of max HP, SB-20 Earth defence down 30%, SB-22 Water slows
// both speeds 30%, SB-28 Reality deals 25% of max HP at once and can kill.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/CombatConstants.h"
#include "Combat/TurnManager.h"
#include "Combat/Damage/DamageCalculator.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/TurnSpan.h"
#include "Skills/Effects/BarCapTriggerResolver.h"

namespace
{
	constexpr int32 BREAK_TEST_TWO_TURNS = 2;
	constexpr int32 BREAK_TEST_TURNS_RECORDED = 2;

	// The rules' numbers (docs/Rules/03-status-bars.md), written out so a changed constant fails here.
	constexpr float B286_SB19_BURN_MAX_HP_FRACTION = 0.08f;
	constexpr float B286_SB20_EARTH_DEFENSE_DOWN_PERCENT = 30.0f;
	constexpr float B286_SB22_WATER_SPEED_SLOW_PERCENT = 30.0f;
	constexpr float B286_SB28_REALITY_MAX_HP_FRACTION = 0.25f;
	/** HP left on the holder so the Reality break is a killing blow. */
	constexpr int32 B286_REALITY_LETHAL_HP = 1;
}

BEGIN_DEFINE_SPEC(FStatusBreakSpec, "WoR.Effects.StatusBreaks",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	USkillEffectManager *EffectManager = nullptr;
	UTurnManager *TurnManager = nullptr;

	AActor *Spawn()
	{
		return TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
	}

	static UCharacterDataComponent *CharOf(AActor *Actor)
	{
		return Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	}

	bool Has(AActor *Actor, ESkillEffectType Type) const
	{
		return EffectManager->HasEffectOfType(Actor, Type);
	}

	/** Actors of the next NumTurns turns after the current one. */
	TArray<AActor *> NextTurns(int32 NumTurns)
	{
		TArray<AActor *> Order;
		for (int32 Turn = 0; Turn < NumTurns; ++Turn)
		{
			TurnManager->AdvanceToNextTurn();
			Order.Add(TurnManager->GetCurrentActor());
		}
		return Order;
	}
END_DEFINE_SPEC(FStatusBreakSpec)

void FStatusBreakSpec::Define()
{
	Describe("The holder-turn counter (FTurnSpan)", [this]()
	{
		It("ends a 1-turn span landed off-turn as the holder's second turn starts", [this]()
		{
			FTurnSpan Span = FTurnSpan::OfHolderTurns(1);
			TestFalse(TEXT("Holds through the holder's next turn"), Span.EndsAtHolderTurnStart());
			Span.CountHolderTurn();
			TestTrue(TEXT("Ends as the following turn starts"), Span.EndsAtHolderTurnStart());
		});

		It("counts the landing turn when it lands on the holder's own turn", [this]()
		{
			FTurnSpan Span = FTurnSpan::OfHolderTurns(1);
			Span.CountHolderTurn(); // the landing turn ends
			TestTrue(TEXT("Ends as the holder's next turn starts"), Span.EndsAtHolderTurnStart());
		});

		It("holds a 2-turn span through two holder turns", [this]()
		{
			FTurnSpan Span = FTurnSpan::OfHolderTurns(BREAK_TEST_TWO_TURNS);
			Span.CountHolderTurn();
			TestFalse(TEXT("One turn counted, still held"), Span.EndsAtHolderTurnStart());
			Span.CountHolderTurn();
			TestTrue(TEXT("Two turns counted, over"), Span.EndsAtHolderTurnStart());
		});
	});

	Describe("A full bar", [this]()
	{
		BeforeEach([this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			TestTrue(TEXT("Test world created"), TestWorld->Create());
			EffectManager = TestWorld->GetSubsystem<USkillEffectManager>();
			TurnManager = TestWorld->GetSubsystem<UTurnManager>();
			TestNotNull(TEXT("SkillEffectManager"), EffectManager);
			TestNotNull(TEXT("TurnManager"), TurnManager);
		});

		AfterEach([this]()
		{
			if (TurnManager && TurnManager->IsCombatActive())
			{
				TurnManager->EndCombat();
			}
			TestWorld.Reset();
			EffectManager = nullptr;
			TurnManager = nullptr;
		});

		It("lasts one holder turn and Burns exactly once, at that turn's start", [this]()
		{
			if (!EffectManager) return;
			AActor *Holder = Spawn();
			UCharacterDataComponent *Char = CharOf(Holder);
			if (!TestNotNull(TEXT("Holder"), Char)) return;

			EffectManager->ApplyTriggeredSkillEffect(Spawn(), Holder, ESkillEffectType::DOT, ESpellElement::Fire);
			const int32 HPLanded = Char->CurrentHP;
			TestEqual(TEXT("No damage on landing"), HPLanded, Char->MaxHP);

			EffectManager->ProcessStartOfTurnEffects(Holder);
			const int32 HPAfterTick = Char->CurrentHP;
			TestTrue(FString::Printf(TEXT("Burned at the holder's turn start (%d -> %d)"), HPLanded, HPAfterTick), HPAfterTick < HPLanded);
			EffectManager->ProcessEndOfTurnEffects(Holder);
			TestTrue(TEXT("Still held at that turn's end"), Has(Holder, ESkillEffectType::DOT));

			EffectManager->ProcessStartOfTurnEffects(Holder);
			TestFalse(TEXT("Ends as the next turn starts"), Has(Holder, ESkillEffectType::DOT));
			TestEqual(TEXT("No second Burn"), Char->CurrentHP, HPAfterTick);
		});

		It("landing on the holder's own turn counts that turn (span only)", [this]()
		{
			if (!EffectManager) return;
			AActor *Holder = Spawn();
			EffectManager->ApplyTriggeredSkillEffect(Spawn(), Holder, ESkillEffectType::DefenseDebuff, ESpellElement::Earth);
			EffectManager->ProcessEndOfTurnEffects(Holder);
			TestTrue(TEXT("Held until the next turn starts"), Has(Holder, ESkillEffectType::DefenseDebuff));
			EffectManager->ProcessStartOfTurnEffects(Holder);
			TestFalse(TEXT("Gone as the next turn starts"), Has(Holder, ESkillEffectType::DefenseDebuff));
		});

		It("landing as a Burn on the holder's own turn pays once on landing (Q-063)", [this]()
		{
			if (!EffectManager || !TurnManager) return;
			AActor *Holder = Spawn();
			AActor *Attacker = Spawn();
			UCharacterDataComponent *Char = CharOf(Holder);
			if (!TestNotNull(TEXT("Holder"), Char)) return;
			TurnManager->InitializeCombat({Holder}, {Attacker});
			if (!TestTrue(TEXT("It is the holder's turn"), TurnManager->GetCurrentActor() == Holder)) return;

			EffectManager->ApplyTriggeredSkillEffect(Attacker, Holder, ESkillEffectType::DOT, ESpellElement::Fire);
			const int32 HPAfterLanding = Char->CurrentHP;
			TestTrue(FString::Printf(TEXT("Burned on landing (%d -> %d)"), Char->MaxHP, HPAfterLanding), HPAfterLanding < Char->MaxHP);
			EffectManager->ProcessEndOfTurnEffects(Holder);
			EffectManager->ProcessStartOfTurnEffects(Holder);
			TestFalse(TEXT("Ends as the holder's next turn starts"), Has(Holder, ESkillEffectType::DOT));
			TestEqual(TEXT("No second Burn"), Char->CurrentHP, HPAfterLanding);
		});

		It("of Lightning holds its Stun for exactly one holder turn", [this]()
		{
			if (!EffectManager) return;
			AActor *Holder = Spawn();
			EffectManager->ApplyTriggeredSkillEffect(Spawn(), Holder, ESkillEffectType::Stun, ESpellElement::Lightning);
			EffectManager->ProcessStartOfTurnEffects(Holder);
			TestTrue(TEXT("Stunned on the holder's next turn"), Has(Holder, ESkillEffectType::Stun));
			EffectManager->ProcessEndOfTurnEffects(Holder);
			EffectManager->ProcessStartOfTurnEffects(Holder);
			TestFalse(TEXT("Not stunned on the turn after"), Has(Holder, ESkillEffectType::Stun));
		});

		It("refreshed by the same attacker after its turn counted, holds one more turn", [this]()
		{
			if (!EffectManager) return;
			AActor *Holder = Spawn();
			AActor *Attacker = Spawn();
			EffectManager->ApplyTriggeredSkillEffect(Attacker, Holder, ESkillEffectType::DefenseDebuff, ESpellElement::Earth);
			EffectManager->ProcessStartOfTurnEffects(Holder);
			EffectManager->ProcessEndOfTurnEffects(Holder);
			EffectManager->ApplyTriggeredSkillEffect(Attacker, Holder, ESkillEffectType::DefenseDebuff, ESpellElement::Earth);
			EffectManager->ProcessStartOfTurnEffects(Holder);
			TestTrue(TEXT("Still held through the next turn"), Has(Holder, ESkillEffectType::DefenseDebuff));
			EffectManager->ProcessEndOfTurnEffects(Holder);
			EffectManager->ProcessStartOfTurnEffects(Holder);
			TestFalse(TEXT("Gone the turn after"), Has(Holder, ESkillEffectType::DefenseDebuff));
		});

		It("of Water slows Action Speed and Spell Speed instead of blocking heals", [this]()
		{
			if (!EffectManager) return;
			AActor *Holder = Spawn();
			UCharacterDataComponent *Char = CharOf(Holder);
			if (!TestNotNull(TEXT("Holder"), Char)) return;
			const float ActionSpeedBefore = Char->GetEffectiveActionSpeed();
			const float SpellSpeedBefore = Char->GetEffectiveSpellSpeed();

			const ESkillEffectType WaterBreak = BarCapTriggerResolver::ResolveTrigger(ESpellElement::Water, EPhysicalDamageType::None, false);
			EffectManager->ApplyTriggeredSkillEffect(Spawn(), Holder, WaterBreak, ESpellElement::Water);
			TestFalse(TEXT("No heal block"), Has(Holder, ESkillEffectType::HealBlock));
			TestTrue(FString::Printf(TEXT("Action Speed slowed (%f -> %f)"), ActionSpeedBefore, Char->GetEffectiveActionSpeed()),
					 Char->GetEffectiveActionSpeed() < ActionSpeedBefore);
			TestTrue(FString::Printf(TEXT("Spell Speed slowed (%f -> %f)"), SpellSpeedBefore, Char->GetEffectiveSpellSpeed()),
					 Char->GetEffectiveSpellSpeed() < SpellSpeedBefore);

			EffectManager->ProcessStartOfTurnEffects(Holder);
			EffectManager->ProcessEndOfTurnEffects(Holder);
			EffectManager->ProcessStartOfTurnEffects(Holder);
			TestFalse(TEXT("Action Speed slow gone after one holder turn"), Has(Holder, ESkillEffectType::ActionSpeedDebuff));
			TestFalse(TEXT("Spell Speed slow gone after one holder turn"), Has(Holder, ESkillEffectType::SpellSpeedDebuff));
		});

		It("of Light lowers crit chance for one holder turn (B-112)", [this]()
		{
			if (!EffectManager) return;
			UDamageCalculator *DamageCalc = TestWorld->GetSubsystem<UDamageCalculator>();
			if (!TestNotNull(TEXT("DamageCalculator"), DamageCalc)) return;
			AActor *Holder = Spawn();
			if (!TestTrue(TEXT("Can crit before the break"), DamageCalc->GetCriticalChance(Holder) > 0.0f)) return;

			const ESkillEffectType LightBreak = BarCapTriggerResolver::ResolveTrigger(ESpellElement::Light, EPhysicalDamageType::None, false);
			EffectManager->ApplyTriggeredSkillEffect(Spawn(), Holder, LightBreak, ESpellElement::Light);
			TestEqual(TEXT("Crit chance down by the Light break value"),
					  EffectManager->GetTotalStatModifier(Holder, ESkillEffectType::CritChanceDebuff),
					  CombatConstants::LIGHT_BREAK_CRIT_CHANCE_DOWN_PERCENT);
			TestTrue(FString::Printf(TEXT("No crits while it lasts (%f)"), DamageCalc->GetCriticalChance(Holder)),
					 DamageCalc->GetCriticalChance(Holder) <= KINDA_SMALL_NUMBER);

			EffectManager->ProcessStartOfTurnEffects(Holder);
			TestTrue(TEXT("Held through the holder's next turn"), Has(Holder, ESkillEffectType::CritChanceDebuff));
			EffectManager->ProcessEndOfTurnEffects(Holder);
			EffectManager->ProcessStartOfTurnEffects(Holder);
			TestFalse(TEXT("Gone as the turn after starts"), Has(Holder, ESkillEffectType::CritChanceDebuff));
		});

		It("of Broken Darkness drains EP at the holder's turn start", [this]()
		{
			if (!EffectManager) return;
			AActor *Holder = Spawn();
			UCharacterDataComponent *Char = CharOf(Holder);
			if (!TestNotNull(TEXT("Holder"), Char)) return;
			const int32 EPBefore = Char->CurrentEP;

			EffectManager->ApplyTriggeredSkillEffect(Spawn(), Holder, ESkillEffectType::DrainEnergy, ESpellElement::Darkness);
			TestEqual(TEXT("Nothing drained on landing"), Char->CurrentEP, EPBefore);
			EffectManager->ProcessStartOfTurnEffects(Holder);
			const int32 Expected = EPBefore - FMath::RoundToInt(Char->MaxEP * CombatConstants::BD_BREAK_EP_DRAIN_FRACTION);
			TestEqual(TEXT("A share of max EP drained"), Char->CurrentEP, FMath::Max(0, Expected));
		});

		It("of Wind pushes the holder's next turn back one place, once per attacker", [this]()
		{
			if (!EffectManager || !TurnManager) return;
			AActor *Local = Spawn();
			AActor *Opposing = Spawn();
			TurnManager->InitializeCombat({Local}, {Opposing});
			if (!TestTrue(TEXT("Local acts first"), TurnManager->GetCurrentActor() == Local)) return;

			EffectManager->ApplyTriggeredSkillEffect(Local, Opposing, ESkillEffectType::SkipTurn, ESpellElement::Wind);
			EffectManager->ApplyTriggeredSkillEffect(Local, Opposing, ESkillEffectType::SkipTurn, ESpellElement::Wind);

			const TArray<AActor *> Order = NextTurns(BREAK_TEST_TURNS_RECORDED);
			if (!TestEqual(TEXT("Turns recorded"), Order.Num(), BREAK_TEST_TURNS_RECORDED)) return;
			TestTrue(TEXT("Local goes again before the gusted Opposing"), Order[0] == Local);
			// A second push from the same attacker would hand this turn to Local too.
			TestTrue(TEXT("Opposing acts one place later, pushed only once"), Order[1] == Opposing);
		});

		It("of Fire Burns for 8% of max HP (SB-19, B-286)", [this]()
		{
			if (!EffectManager) return;
			TestEqual(TEXT("Burn share is the rule's 8%"), CombatConstants::BURN_BREAK_MAX_HP_FRACTION, B286_SB19_BURN_MAX_HP_FRACTION);
			AActor *Holder = Spawn();
			UCharacterDataComponent *Char = CharOf(Holder);
			if (!TestNotNull(TEXT("Holder"), Char)) return;

			EffectManager->ApplyTriggeredSkillEffect(Spawn(), Holder, ESkillEffectType::DOT, ESpellElement::Fire);
			EffectManager->ProcessStartOfTurnEffects(Holder);
			const int32 ExpectedBurn = FMath::RoundToInt(Char->MaxHP * B286_SB19_BURN_MAX_HP_FRACTION);
			TestEqual(TEXT("One tick of 8% of max HP"), Char->MaxHP - Char->CurrentHP, ExpectedBurn);
		});

		It("of Earth lowers defence by 30% (SB-20, B-286)", [this]()
		{
			if (!EffectManager) return;
			TestEqual(TEXT("Defence down is the rule's 30%"), CombatConstants::EARTH_BREAK_DEFENSE_DOWN_PERCENT, B286_SB20_EARTH_DEFENSE_DOWN_PERCENT);
			AActor *Holder = Spawn();
			const ESkillEffectType EarthBreak = BarCapTriggerResolver::ResolveTrigger(ESpellElement::Earth, EPhysicalDamageType::None, false);
			EffectManager->ApplyTriggeredSkillEffect(Spawn(), Holder, EarthBreak, ESpellElement::Earth);
			TestEqual(TEXT("Defence down 30%"), EffectManager->GetTotalStatModifier(Holder, ESkillEffectType::DefenseDebuff),
					  B286_SB20_EARTH_DEFENSE_DOWN_PERCENT);
		});

		It("of Water slows Action Speed and Spell Speed by 30% (SB-22, B-286)", [this]()
		{
			if (!EffectManager) return;
			TestEqual(TEXT("Slow is the rule's 30%"), CombatConstants::WATER_BREAK_SPEED_SLOW_PERCENT, B286_SB22_WATER_SPEED_SLOW_PERCENT);
			AActor *Holder = Spawn();
			const ESkillEffectType WaterBreak = BarCapTriggerResolver::ResolveTrigger(ESpellElement::Water, EPhysicalDamageType::None, false);
			EffectManager->ApplyTriggeredSkillEffect(Spawn(), Holder, WaterBreak, ESpellElement::Water);
			TestEqual(TEXT("Action Speed slowed 30%"), EffectManager->GetTotalStatModifier(Holder, ESkillEffectType::ActionSpeedDebuff),
					  B286_SB22_WATER_SPEED_SLOW_PERCENT);
			TestEqual(TEXT("Spell Speed slowed 30%"), EffectManager->GetTotalStatModifier(Holder, ESkillEffectType::SpellSpeedDebuff),
					  B286_SB22_WATER_SPEED_SLOW_PERCENT);
		});

		It("of Reality deals 25% of max HP at once (SB-28, B-286)", [this]()
		{
			if (!EffectManager) return;
			TestEqual(TEXT("Reality share is the rule's 25%"), CombatConstants::REALITY_BREAK_MAX_HP_FRACTION, B286_SB28_REALITY_MAX_HP_FRACTION);
			AActor *Holder = Spawn();
			UCharacterDataComponent *Char = CharOf(Holder);
			if (!TestNotNull(TEXT("Holder"), Char)) return;

			const ESkillEffectType RealityBreak = BarCapTriggerResolver::ResolveTrigger(ESpellElement::Reality, EPhysicalDamageType::None, false);
			EffectManager->ApplyTriggeredSkillEffect(Spawn(), Holder, RealityBreak, ESpellElement::Reality);
			const int32 ExpectedDamage = FMath::RoundToInt(Char->MaxHP * B286_SB28_REALITY_MAX_HP_FRACTION);
			TestEqual(TEXT("25% of max HP taken on landing"), Char->MaxHP - Char->CurrentHP, ExpectedDamage);
			TestFalse(TEXT("No lasting effect"), Has(Holder, ESkillEffectType::BurstDamage));
		});

		It("of Reality can kill (SB-28, B-286)", [this]()
		{
			if (!EffectManager) return;
			AActor *Holder = Spawn();
			UCharacterDataComponent *Char = CharOf(Holder);
			if (!TestNotNull(TEXT("Holder"), Char)) return;
			Char->ServerSetHP(B286_REALITY_LETHAL_HP);

			EffectManager->ApplyTriggeredSkillEffect(Spawn(), Holder, ESkillEffectType::BurstDamage, ESpellElement::Reality);
			TestEqual(TEXT("HP reaches 0"), Char->CurrentHP, 0);
			TestFalse(TEXT("The holder dies"), Char->bIsAlive);
		});
	});
}

#endif
