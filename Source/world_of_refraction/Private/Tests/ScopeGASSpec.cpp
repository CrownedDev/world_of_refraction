// ScopeGASSpec.cpp
// B-385: the damage scope (B-383, D-166) survives the GAS side: the effect identity serializes and copies it, the
// mirror tells two scopes apart, and Defense's scope correction is 0 with no scoped effect, negative when a Fire-only
// DefenseBuff meets a Water hit, and keeps a Water hit's Defense when a Fire-only debuff floors every layer.

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
	/** A debuff of 100% or more floors the Defense layers at x0. */
	constexpr float SCOPE_GAS_TEST_FLOORING_PERCENT = 150.0f;
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

	Describe("Defense's scope correction", [this]()
	{
		BeforeEach([this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			TestTrue(TEXT("test world created"), TestWorld->Create());
		});

		AfterEach([this]() { TestWorld.Reset(); });

		It("is 0 with an All-scope buff, takes Defense away for a Fire-only buff on a Water hit, and survives a floor", [this]()
		{
			UDamageCalculator *Calculator = TestWorld->GetSubsystem<UDamageCalculator>();
			USkillEffectManager *Effects = TestWorld->GetSubsystem<USkillEffectManager>();
			if (!TestNotNull(TEXT("calculator"), Calculator) || !TestNotNull(TEXT("effects"), Effects))
			{
				return;
			}
			auto SpawnWithDefenseBuff = [this, Effects](ESkillEffectType Type, EDamageScope Scope, ESpellElement Element, float Percent)
			{
				UCharacterData *Data = FWoRTestWorld::NewCharacterData();
				Data->Defense = SCOPE_GAS_TEST_DEFENSE_POINTS;
				AActor *Defender = TestWorld->SpawnCombatant(Data);
				FActiveSkillEffect Buff = FActiveSkillEffect::CreateBuff(TEXT("Guard change"), SCOPE_GAS_TEST_EFFECT_ID, Type, Percent, SCOPE_GAS_TEST_TURNS);
				Buff.DamageScope = Scope;
				Buff.Element = Element;
				Effects->ApplyEffect(Defender, Buff);
				return Defender;
			};
			auto DeltaFor = [Calculator](AActor *Defender, ESpellElement HitElement)
			{
				float Delta = -1.0f;
				Calculator->GetDefenderDefenseForHit(Defender, HitElement, true, &Delta);
				return Delta;
			};
			AActor *AllScope = SpawnWithDefenseBuff(ESkillEffectType::DefenseBuff, EDamageScope::All, ESpellElement::None, SCOPE_GAS_TEST_PERCENT);
			TestEqual(TEXT("All scope: no correction"), DeltaFor(AllScope, ESpellElement::Water), 0.0f, KINDA_SMALL_NUMBER);
			AActor *FireOnly = SpawnWithDefenseBuff(ESkillEffectType::DefenseBuff, EDamageScope::Element, ESpellElement::Fire, SCOPE_GAS_TEST_PERCENT);
			TestTrue(TEXT("Fire-only buff on a Water hit: takes Defense away"), DeltaFor(FireOnly, ESpellElement::Water) < 0.0f);
			TestEqual(TEXT("Fire-only buff on a Fire hit: none"), DeltaFor(FireOnly, ESpellElement::Fire), 0.0f, KINDA_SMALL_NUMBER);
			// Review: a Fire-only debuff that floors every Defense layer to 0 must still leave a Water hit its Defense.
			AActor *Floored = SpawnWithDefenseBuff(ESkillEffectType::DefenseDebuff, EDamageScope::Element, ESpellElement::Fire, SCOPE_GAS_TEST_FLOORING_PERCENT);
			TestTrue(TEXT("floored for Fire: the Water hit keeps its Defense"),
					 Calculator->GetDefenderDefenseForHit(Floored, ESpellElement::Water, true) > 0.0f &&
						 DeltaFor(Floored, ESpellElement::Water) > 0.0f);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
