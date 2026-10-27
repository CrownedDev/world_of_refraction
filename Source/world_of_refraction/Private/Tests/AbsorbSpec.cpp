// AbsorbSpec.cpp
// GAS stage 3 (B-042j, D-092, EffectsOverhaul.md §2.5): Barrier / Shield / Ward. One case per rule:
// scope, Uses (per instance, one charge), unlimited, Pool, a charge that catches nothing isn't spent,
// used up goes / timed refills at the holder's turn start, duration ends it, spend order, the verb's tag.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AbilitySystemComponent.h"
#include "GAS/WoRAbsorb.h"
#include "GAS/WoRTurnEffectTracker.h"
#include "Tags/WoRGameplayTags.h"

namespace
{
	constexpr int32 ABSORB_MAGNITUDE = 5;
	constexpr int32 ABSORB_TWO_USES = 2;
	constexpr int32 ABSORB_ONE_USE = 1;
	constexpr int32 ABSORB_UNLIMITED = 0;
	constexpr int32 ABSORB_UNTIL_SPENT = 0;
	constexpr int32 ABSORB_TWO_TURNS = 2;
	constexpr int32 HIT_SMALL = 3;
	constexpr int32 HIT_BIG = 8;
	constexpr int32 WARD_COUNT = 2;

	FWoRAbsorbCharge MakeCharge(EWoRAbsorbVerb Verb, EWoRChargeMode Mode, int32 MaxUses, ESpellElement Element = ESpellElement::None)
	{
		FWoRAbsorbCharge Charge;
		Charge.Verb = Verb;
		Charge.Mode = Mode;
		Charge.MaxUses = MaxUses;
		Charge.Magnitude = ABSORB_MAGNITUDE;
		Charge.Element = Element;
		return Charge;
	}
}

BEGIN_DEFINE_SPEC(FAbsorbSpec, "WoR.GAS.Absorb", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	AActor *Hero = nullptr;
	UAbilitySystemComponent *ASC = nullptr;
	UWoRTurnEffectTracker *Tracker = nullptr;

	FActiveGameplayEffectHandle Give(const FWoRAbsorbCharge &Charge, int32 Turns = ABSORB_UNTIL_SPENT)
	{
		return Tracker->ApplyAbsorb(Charge, Turns, Hero);
	}

	int32 HitPhysical(int32 Damage) { return Tracker->AbsorbHit(Damage, ESpellElement::None, EPhysicalDamageType::Slash); }
	int32 HitFire(int32 Damage) { return Tracker->AbsorbHit(Damage, ESpellElement::Fire, EPhysicalDamageType::None); }
	int32 HitWater(int32 Damage) { return Tracker->AbsorbHit(Damage, ESpellElement::Water, EPhysicalDamageType::None); }

	void PassHolderTurn()
	{
		Tracker->OnHolderTurnEnd();
		Tracker->OnHolderTurnStart();
	}
END_DEFINE_SPEC(FAbsorbSpec)

void FAbsorbSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (Hero)
		{
			ASC = FWoRTestWorld::AddComponent<UAbilitySystemComponent>(Hero);
			ASC->InitAbilityActorInfo(Hero, Hero);
			Tracker = FWoRTestWorld::AddComponent<UWoRTurnEffectTracker>(Hero);
		}
		if (!TestNotNull(TEXT("AbilitySystemComponent"), ASC) || !TestNotNull(TEXT("Tracker"), Tracker))
		{
			Tracker = nullptr; // each case returns early
		}
	});

	AfterEach([this]()
	{
		ASC = nullptr;
		Tracker = nullptr;
		Hero = nullptr;
		TestWorld.Reset();
	});

	It("scopes: Barrier catches anything, Shield physical only, Ward its element only", [this]()
	{
		if (!Tracker) return;
		FWoRAbsorbCharge Barrier = MakeCharge(EWoRAbsorbVerb::Barrier, EWoRChargeMode::Uses, ABSORB_UNLIMITED);
		FWoRAbsorbCharge Shield = MakeCharge(EWoRAbsorbVerb::Shield, EWoRChargeMode::Uses, ABSORB_UNLIMITED);
		FWoRAbsorbCharge Ward = MakeCharge(EWoRAbsorbVerb::Ward, EWoRChargeMode::Uses, ABSORB_UNLIMITED, ESpellElement::Fire);
		TestTrue(TEXT("Barrier: physical"), Barrier.Catches(ESpellElement::None, EPhysicalDamageType::Slash));
		TestTrue(TEXT("Barrier: Fire"), Barrier.Catches(ESpellElement::Fire, EPhysicalDamageType::None));
		TestTrue(TEXT("Shield: physical"), Shield.Catches(ESpellElement::None, EPhysicalDamageType::Impact));
		TestFalse(TEXT("Shield: not Fire"), Shield.Catches(ESpellElement::Fire, EPhysicalDamageType::None));
		TestTrue(TEXT("Ward Fire: Fire"), Ward.Catches(ESpellElement::Fire, EPhysicalDamageType::None));
		TestFalse(TEXT("Ward Fire: not Water"), Ward.Catches(ESpellElement::Water, EPhysicalDamageType::None));
		TestFalse(TEXT("Ward Fire: not physical"), Ward.Catches(ESpellElement::None, EPhysicalDamageType::Slash));
	});

	It("refuses a Ward with no element (it would just be a Barrier)", [this]()
	{
		if (!Tracker) return;
		AddExpectedError(TEXT("absorber"), EAutomationExpectedErrorFlags::Contains, 0);
		TestFalse(TEXT("No handle"), Give(MakeCharge(EWoRAbsorbVerb::Ward, EWoRChargeMode::Uses, ABSORB_ONE_USE)).IsValid());
		TestEqual(TEXT("No absorber"), Tracker->GetAbsorbs().Num(), 0);
	});

	It("Uses: negates up to the magnitude per instance and spends one charge; used up, it goes", [this]()
	{
		if (!Tracker) return;
		Give(MakeCharge(EWoRAbsorbVerb::Barrier, EWoRChargeMode::Uses, ABSORB_TWO_USES));
		TestTrue(TEXT("The verb's tag is granted"), ASC->HasMatchingGameplayTag(WoRTags::Status_Absorb_Barrier));
		TestEqual(TEXT("A big hit: up to the magnitude"), HitPhysical(HIT_BIG), ABSORB_MAGNITUDE);
		TestEqual(TEXT("A small hit: all of it"), HitPhysical(HIT_SMALL), HIT_SMALL);
		TestEqual(TEXT("Two uses spent: gone"), Tracker->GetAbsorbs().Num(), 0);
		TestFalse(TEXT("Its tag goes with it"), ASC->HasMatchingGameplayTag(WoRTags::Status_Absorb_Barrier));
		TestEqual(TEXT("Nothing left to negate"), HitPhysical(HIT_SMALL), 0);
	});

	It("Uses, unlimited: catches every instance and spends nothing", [this]()
	{
		if (!Tracker) return;
		Give(MakeCharge(EWoRAbsorbVerb::Ward, EWoRChargeMode::Uses, ABSORB_UNLIMITED, ESpellElement::Fire));
		for (int32 Hit = 0; Hit < ABSORB_TWO_USES + 1; ++Hit)
		{
			TestEqual(TEXT("Each Fire hit: up to the magnitude"), HitFire(HIT_BIG), ABSORB_MAGNITUDE);
		}
		TestEqual(TEXT("Still there"), Tracker->GetAbsorbs().Num(), 1);
	});

	It("Pool: one budget drawn down across hits, then gone", [this]()
	{
		if (!Tracker) return;
		Give(MakeCharge(EWoRAbsorbVerb::Barrier, EWoRChargeMode::Pool, ABSORB_UNLIMITED));
		TestEqual(TEXT("First hit takes 3 of 5"), HitPhysical(HIT_SMALL), HIT_SMALL);
		TestEqual(TEXT("Second hit takes the last 2"), HitPhysical(HIT_SMALL), ABSORB_MAGNITUDE - HIT_SMALL);
		TestEqual(TEXT("Spent: gone"), Tracker->GetAbsorbs().Num(), 0);
	});

	It("doesn't spend a charge that catches nothing", [this]()
	{
		if (!Tracker) return;
		Give(MakeCharge(EWoRAbsorbVerb::Ward, EWoRChargeMode::Uses, ABSORB_ONE_USE, ESpellElement::Fire));
		TestEqual(TEXT("A Water hit: nothing negated"), HitWater(HIT_SMALL), 0);
		TestEqual(TEXT("The Fire Ward keeps its charge"), Tracker->GetAbsorbs().Num(), 1);
		TestEqual(TEXT("The Fire hit spends it"), HitFire(HIT_SMALL), HIT_SMALL);
		TestEqual(TEXT("Then it goes"), Tracker->GetAbsorbs().Num(), 0);
	});

	It("timed and counted: waits when spent, refills at the holder's turn start, ends with its span", [this]()
	{
		if (!Tracker) return;
		Give(MakeCharge(EWoRAbsorbVerb::Barrier, EWoRChargeMode::Uses, ABSORB_ONE_USE), ABSORB_TWO_TURNS);
		TestEqual(TEXT("Turn 1: the first hit is negated"), HitPhysical(HIT_SMALL), HIT_SMALL);
		TestEqual(TEXT("Turn 1: the second gets through"), HitPhysical(HIT_SMALL), 0);
		TestEqual(TEXT("Spent but kept for its refill"), Tracker->GetAbsorbs().Num(), 1);
		PassHolderTurn();
		TestEqual(TEXT("Turn 2: refilled"), HitPhysical(HIT_SMALL), HIT_SMALL);
		PassHolderTurn();
		TestEqual(TEXT("After two turns: gone"), Tracker->GetAbsorbs().Num(), 0);
		TestEqual(TEXT("Nothing negated"), HitPhysical(HIT_SMALL), 0);
	});

	It("timed Pool: doesn't refill, and goes when spent before its duration", [this]()
	{
		if (!Tracker) return;
		Give(MakeCharge(EWoRAbsorbVerb::Barrier, EWoRChargeMode::Pool, ABSORB_UNLIMITED), ABSORB_TWO_TURNS);
		TestEqual(TEXT("Takes 3 of 5"), HitPhysical(HIT_SMALL), HIT_SMALL);
		PassHolderTurn();
		TestEqual(TEXT("Turn 2: only the 2 left, no refill"), HitPhysical(HIT_SMALL), ABSORB_MAGNITUDE - HIT_SMALL);
		TestEqual(TEXT("Spent before its duration: gone"), Tracker->GetAbsorbs().Num(), 0);
	});

	It("grants each verb's own tag", [this]()
	{
		if (!Tracker) return;
		Give(MakeCharge(EWoRAbsorbVerb::Shield, EWoRChargeMode::Uses, ABSORB_UNLIMITED));
		Give(MakeCharge(EWoRAbsorbVerb::Ward, EWoRChargeMode::Uses, ABSORB_UNLIMITED, ESpellElement::Fire));
		TestTrue(TEXT("Shield"), ASC->HasMatchingGameplayTag(WoRTags::Status_Absorb_Shield));
		TestTrue(TEXT("Ward"), ASC->HasMatchingGameplayTag(WoRTags::Status_Absorb_Ward));
		TestFalse(TEXT("No Barrier"), ASC->HasMatchingGameplayTag(WoRTags::Status_Absorb_Barrier));
	});

	It("refuses a Generic Ward (a resolved hit is never Generic)", [this]()
	{
		if (!Tracker) return;
		AddExpectedError(TEXT("absorber"), EAutomationExpectedErrorFlags::Contains, 0);
		TestFalse(TEXT("No handle"),
				  Give(MakeCharge(EWoRAbsorbVerb::Ward, EWoRChargeMode::Uses, ABSORB_ONE_USE, ESpellElement::Generic)).IsValid());
	});

	It("spends Ward, then Shield, then Barrier, oldest first within a verb", [this]()
	{
		if (!Tracker) return;
		Give(MakeCharge(EWoRAbsorbVerb::Barrier, EWoRChargeMode::Pool, ABSORB_UNLIMITED));
		Give(MakeCharge(EWoRAbsorbVerb::Ward, EWoRChargeMode::Pool, ABSORB_UNLIMITED, ESpellElement::Fire));
		Give(MakeCharge(EWoRAbsorbVerb::Ward, EWoRChargeMode::Pool, ABSORB_UNLIMITED, ESpellElement::Fire));
		// 8 Fire damage: the older Ward takes 5, the newer Ward 3; the Barrier is untouched.
		TestEqual(TEXT("All of it negated"), HitFire(HIT_BIG), HIT_BIG);
		int32 BarrierPool = -1;
		int32 WardPoolLeft = 0;
		for (const FWoRAbsorbCharge &Charge : Tracker->GetAbsorbs())
		{
			if (Charge.Verb == EWoRAbsorbVerb::Barrier)
			{
				BarrierPool = Charge.PoolLeft;
			}
			else
			{
				WardPoolLeft += Charge.PoolLeft;
			}
		}
		TestEqual(TEXT("The Barrier is untouched"), BarrierPool, ABSORB_MAGNITUDE);
		TestEqual(TEXT("The Wards keep 2 between them (the older one went)"), WardPoolLeft, WARD_COUNT * ABSORB_MAGNITUDE - HIT_BIG);
		TestEqual(TEXT("Two absorbers left"), Tracker->GetAbsorbs().Num(), 2);
	});

	It("orders by gain within a verb in the pure spend too", [this]()
	{
		FWoRAbsorbCharge Newer = MakeCharge(EWoRAbsorbVerb::Barrier, EWoRChargeMode::Pool, ABSORB_UNLIMITED);
		FWoRAbsorbCharge Older = Newer;
		Newer.Fill();
		Older.Fill();
		Newer.Arrival = 1;
		Older.Arrival = 0;
		TArray<FWoRAbsorbCharge *> Charges{&Newer, &Older};
		TestEqual(TEXT("Negates 3"), WoRAbsorb::Spend(Charges, HIT_SMALL, ESpellElement::None, EPhysicalDamageType::Slash), HIT_SMALL);
		TestEqual(TEXT("The older one paid"), Older.PoolLeft, ABSORB_MAGNITUDE - HIT_SMALL);
		TestEqual(TEXT("The newer one is whole"), Newer.PoolLeft, ABSORB_MAGNITUDE);
	});
}

#endif
