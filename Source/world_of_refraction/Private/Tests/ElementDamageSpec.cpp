// ElementDamageSpec.cpp
// B-064 (D-005 C, D-036, Q-145, Q-146b): the defender's per-element resistance scales elemental damage
// 1:1, capped at +-50%; non-elemental hits are unchanged; Ignore Defense drops positive resistance.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "ElementDamageRecorder.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/Damage/DamageCalculator.h"
#include "Combat/Damage/DamageMath.h"
#include "Combat/Resistance/EWoRDamageType.h"
#include "Combat/Resistance/ResistanceQuery.h"

namespace
{
	constexpr int32 ELEMENT_TEST_BASE = 100;
	constexpr float ELEMENT_TEST_RESIST = 0.2f;
	constexpr float ELEMENT_TEST_WEAK = -0.15f;
	constexpr float ELEMENT_TEST_OVER_CAP = 0.7f;
	constexpr float ELEMENT_TEST_UNDER_CAP = -0.9f;
}

BEGIN_DEFINE_SPEC(FElementDamageSpec, "WoR.Damage.Element",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
TUniquePtr<FWoRTestWorld> TestWorld;
UDamageCalculator *Calculator = nullptr;
AActor *Attacker = nullptr;
AActor *FireCaster = nullptr;

int32 HitFireCaster(ESpellElement Element) const
{
	FDamageCalculationInput Input;
	Input.BaseDamage = ELEMENT_TEST_BASE;
	Input.ActionType = EActionType::Spell;
	Input.Element = Element;
	Input.bCanCrit = false;
	return Calculator->CalculateDamage(Attacker, FireCaster, Input).FinalDamage;
}

/** What a hit of Element should deal to the Fire Caster: the no-resistance damage x (1 - capped resistance). */
int32 ExpectedAgainstFireCaster(ESpellElement Element, int32 Neutral) const
{
	float Resistance = WoRResistance::GetTypeResistance(FireCaster, WoRDamageType::ElementToDamageType(Element));
	if (Element == ESpellElement::Water)
	{
		Resistance += WoRResistance::GetTypeResistance(FireCaster, EWoRDamageType::Ice);
	}
	return FMath::Max(DamageConstants::MIN_DAMAGE,
					  FMath::RoundToInt(Neutral * WoRDamageMath::GetElementMultiplier(Resistance, false)));
}
END_DEFINE_SPEC(FElementDamageSpec)

void FElementDamageSpec::Define()
{
	Describe("the element multiplier", [this]()
	{
		It("is 1 - resistance, 1:1", [this]()
		{
			TestEqual(TEXT("resistant"), WoRDamageMath::GetElementMultiplier(ELEMENT_TEST_RESIST, false), 1.0f - ELEMENT_TEST_RESIST);
			TestEqual(TEXT("weak"), WoRDamageMath::GetElementMultiplier(ELEMENT_TEST_WEAK, false), 1.0f - ELEMENT_TEST_WEAK);
			TestEqual(TEXT("neutral"), WoRDamageMath::GetElementMultiplier(0.0f, false), 1.0f);
		});

		It("caps resistance at +-50%", [this]()
		{
			TestEqual(TEXT("over the cap"), WoRDamageMath::GetElementMultiplier(ELEMENT_TEST_OVER_CAP, false),
					  1.0f - WoRDamageMath::ELEMENT_DAMAGE_RESIST_CAP);
			TestEqual(TEXT("under the cap"), WoRDamageMath::GetElementMultiplier(ELEMENT_TEST_UNDER_CAP, false),
					  1.0f + WoRDamageMath::ELEMENT_DAMAGE_RESIST_CAP);
		});

		It("Ignore Defense drops positive resistance only (Q-146b)", [this]()
		{
			TestEqual(TEXT("resistance ignored"), WoRDamageMath::GetElementMultiplier(ELEMENT_TEST_RESIST, true), 1.0f);
			TestEqual(TEXT("weakness kept"), WoRDamageMath::GetElementMultiplier(ELEMENT_TEST_WEAK, true), 1.0f - ELEMENT_TEST_WEAK);
		});
	});

	Describe("against a Fire Caster", [this]()
	{
		BeforeEach([this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			TestTrue(TEXT("test world created"), TestWorld->Create());
			Calculator = TestWorld->GetSubsystem<UDamageCalculator>();
			UCharacterData *FireData = FWoRTestWorld::NewCharacterData();
			FireData->CharacterClass = ECharacterClass::Caster;
			FireData->InnateElement = ESpellElement::Fire;
			Attacker = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			FireCaster = TestWorld->SpawnCombatant(FireData);
		});

		AfterEach([this]()
		{
			TestWorld.Reset();
		});

		It("resists Fire, is weak to Water, and takes non-elemental damage unchanged", [this]()
		{
			if (!TestTrue(TEXT("setup"), Calculator && Attacker && FireCaster))
			{
				return;
			}
			TestTrue(TEXT("the class row resists Fire"),
					 WoRResistance::GetTypeResistance(FireCaster, EWoRDamageType::Fire) > 0.0f);
			TestTrue(TEXT("the class row is weak to Water"),
					 WoRResistance::GetTypeResistance(FireCaster, EWoRDamageType::Water) < 0.0f);

			const int32 Neutral = HitFireCaster(ESpellElement::None);
			const int32 Fire = HitFireCaster(ESpellElement::Fire);
			const int32 Water = HitFireCaster(ESpellElement::Water);
			TestTrue(TEXT("resistant: less"), Fire < Neutral);
			TestTrue(TEXT("weak: more"), Water > Neutral);
			// Rounded before vs after the step: within 1.
			TestTrue(TEXT("Fire = neutral x (1 - resistance)"),
					 FMath::Abs(Fire - ExpectedAgainstFireCaster(ESpellElement::Fire, Neutral)) <= 1);
			TestTrue(TEXT("Water = neutral x (1 - resistance)"),
					 FMath::Abs(Water - ExpectedAgainstFireCaster(ESpellElement::Water, Neutral)) <= 1);
			TestEqual(TEXT("a leftover Generic counts as non-elemental"), HitFireCaster(ESpellElement::Generic), Neutral);
		});

		It("OnElementDamageDealt fires once per hit with the hit's element", [this]()
		{
			UActionExecutor *Executor = TestWorld->GetSubsystem<UActionExecutor>();
			if (!TestTrue(TEXT("setup"), Executor && Attacker && FireCaster))
			{
				return;
			}
			UElementDamageRecorder *Recorder = NewObject<UElementDamageRecorder>();
			Recorder->Listen(Executor);
			const FCombatHitResult Hit = Executor->ApplyDamage(Attacker, FireCaster, ELEMENT_TEST_BASE, ESpellElement::Water, false);
			if (TestEqual(TEXT("one event"), Recorder->Entries.Num(), 1))
			{
				TestEqual(TEXT("element"), Recorder->Entries[0].Element, ESpellElement::Water);
				TestEqual(TEXT("damage"), Recorder->Entries[0].Damage, Hit.DamageDealt);
			}
		});

		It("skips element resistance when the input says so", [this]()
		{
			if (!TestTrue(TEXT("setup"), Calculator && Attacker && FireCaster))
			{
				return;
			}
			FDamageCalculationInput Input;
			Input.BaseDamage = ELEMENT_TEST_BASE;
			Input.ActionType = EActionType::Spell;
			Input.Element = ESpellElement::Fire;
			Input.bCanCrit = false;
			Input.bIgnoreResistance = true;
			TestEqual(TEXT("as non-elemental"), Calculator->CalculateDamage(Attacker, FireCaster, Input).FinalDamage,
					  HitFireCaster(ESpellElement::None));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
