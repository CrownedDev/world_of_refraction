// ScopeGASSpec.cpp
// B-385: the damage scope (B-383, D-166) survives the GAS side: the effect identity serializes and copies it, the
// mirror tells two scopes apart, and Defense's scope ratio is 1 with no scoped effect and below 1 when a Fire-only
// DefenseBuff meets a Water hit.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Combat/Damage/DamageCalculator.h"
#include "GAS/WoREffectContext.h"
#include "GAS/WoREffectMirrorComponent.h"
#include "GAS/WoREffectSpecBuilder.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 SCOPE_GAS_TEST_EFFECT_ID = 9385;
	constexpr float SCOPE_GAS_TEST_PERCENT = 50.0f;
	constexpr int32 SCOPE_GAS_TEST_TURNS = 2;
	constexpr int32 SCOPE_GAS_TEST_DEFENSE_POINTS = 50;
}

BEGIN_DEFINE_SPEC(FScopeGASSpec, "WoR.Damage.ScopeGAS", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
END_DEFINE_SPEC(FScopeGASSpec)

void FScopeGASSpec::Define()
{
	It("the effect identity serializes the scope and MakeIdentity copies it", [this]()
	{
		FWoREffectIdentity Out;
		Out.EffectType = ESkillEffectType::DamageBuff;
		Out.Element = ESpellElement::Fire;
		Out.DamageScope = EDamageScope::Element;
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes);
		Out.Serialize(Writer);
		FWoREffectIdentity In;
		FMemoryReader Reader(Bytes);
		In.Serialize(Reader);
		TestTrue(TEXT("scope round-trips"), In.DamageScope == EDamageScope::Element);
		TestTrue(TEXT("element round-trips"), In.Element == ESpellElement::Fire);

		FActiveSkillEffect Effect;
		Effect.EffectType = ESkillEffectType::DefenseBuff;
		Effect.DamageScope = EDamageScope::Spell;
		TestTrue(TEXT("MakeIdentity copies it"), FWoREffectSpecBuilder::MakeIdentity(Effect).DamageScope == EDamageScope::Spell);
	});

	It("the mirror tells two scopes apart", [this]()
	{
		FWoRMirroredEffect A;
		A.Identity.EffectType = ESkillEffectType::DamageBuff;
		A.Magnitude = SCOPE_GAS_TEST_PERCENT;
		FWoRMirroredEffect B = A;
		TestTrue(TEXT("same scope matches"), A.Matches(B));
		B.Identity.DamageScope = EDamageScope::Physical;
		TestFalse(TEXT("another scope doesn't"), A.Matches(B));
	});

	Describe("Defense's scope ratio", [this]()
	{
		BeforeEach([this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			TestTrue(TEXT("test world created"), TestWorld->Create());
		});

		AfterEach([this]() { TestWorld.Reset(); });

		It("is 1 with an All-scope buff and below 1 for a Fire-only buff on a Water hit", [this]()
		{
			UDamageCalculator *Calculator = TestWorld->GetSubsystem<UDamageCalculator>();
			USkillEffectManager *Effects = TestWorld->GetSubsystem<USkillEffectManager>();
			if (!TestNotNull(TEXT("calculator"), Calculator) || !TestNotNull(TEXT("effects"), Effects))
			{
				return;
			}
			auto SpawnWithDefenseBuff = [this, Effects](EDamageScope Scope, ESpellElement Element)
			{
				UCharacterData *Data = FWoRTestWorld::NewCharacterData();
				Data->Defense = SCOPE_GAS_TEST_DEFENSE_POINTS;
				AActor *Defender = TestWorld->SpawnCombatant(Data);
				FActiveSkillEffect Buff = FActiveSkillEffect::CreateBuff(TEXT("Guard Up"), SCOPE_GAS_TEST_EFFECT_ID, ESkillEffectType::DefenseBuff,
																		 SCOPE_GAS_TEST_PERCENT, SCOPE_GAS_TEST_TURNS);
				Buff.DamageScope = Scope;
				Buff.Element = Element;
				Effects->ApplyEffect(Defender, Buff);
				return Defender;
			};
			AActor *AllScope = SpawnWithDefenseBuff(EDamageScope::All, ESpellElement::None);
			TestEqual(TEXT("All scope: 1"), Calculator->GetDefenseScopeRatio(AllScope, ESpellElement::Water, true), 1.0f, KINDA_SMALL_NUMBER);
			AActor *FireOnly = SpawnWithDefenseBuff(EDamageScope::Element, ESpellElement::Fire);
			TestTrue(TEXT("Fire-only on a Water hit: below 1"), Calculator->GetDefenseScopeRatio(FireOnly, ESpellElement::Water, true) < 1.0f);
			TestEqual(TEXT("Fire-only on a Fire hit: 1"), Calculator->GetDefenseScopeRatio(FireOnly, ESpellElement::Fire, true), 1.0f,
					  KINDA_SMALL_NUMBER);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
