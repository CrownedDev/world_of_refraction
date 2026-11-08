// DamageScopeSpec.cpp
// B-383 (Crown's direction 2026-11-08, D-166): every damage and defence boost has a scope (All, Physical, Spell or
// one element). An All buff is unchanged; a Fire-scoped buff raises a Fire hit only; a Physical-scoped one raises
// physical hits only; a Fire-scoped defence buff cuts Fire damage taken only; old data with no scope reads All.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Combat/Damage/DamageCalculator.h"
#include "Debug/Combat/DamageScopeDebug.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/EDamageScope.h"
#include "Skills/Effects/FSkillEffectPayload.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 SCOPE_TEST_BASE_DAMAGE = 100;
	constexpr int32 SCOPE_TEST_EFFECT_ID = 9383;
	constexpr float SCOPE_TEST_BUFF_PERCENT = 50.0f;
	constexpr int32 SCOPE_TEST_TURNS = 2;
	/** Some Defense, so a DefenseBuff has something to multiply. */
	constexpr int32 SCOPE_TEST_DEFENSE_POINTS = 50;
	/** Far past the RawDamage cap, so the stat-cap correction is active. */
	constexpr int32 SCOPE_TEST_OVERCAP_POINTS = 500;

	struct FScopedBoost
	{
		ESkillEffectType Type = ESkillEffectType::None;
		EDamageScope Scope = EDamageScope::All;
		ESpellElement Element = ESpellElement::None;
	};
}

BEGIN_DEFINE_SPEC(FDamageScopeSpec, "WoR.Damage.Scope", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;

	static FActiveSkillEffect MakeBoost(const FScopedBoost &Boost)
	{
		FActiveSkillEffect Effect = FActiveSkillEffect::CreateBuff(TEXT("Scoped boost"), SCOPE_TEST_EFFECT_ID, Boost.Type,
																   SCOPE_TEST_BUFF_PERCENT, SCOPE_TEST_TURNS);
		Effect.DamageScope = Boost.Scope;
		Effect.Element = Boost.Element;
		return Effect;
	}

	/** One hit's final damage, with an optional boost on the attacker and one on the defender (Type None = none). */
	int32 Hit(EActionType ActionType, ESpellElement HitElement, const FScopedBoost &OnAttacker, const FScopedBoost &OnDefender = FScopedBoost(),
			  int32 AttackerRawPoints = 0)
	{
		UDamageCalculator *Calculator = TestWorld->GetSubsystem<UDamageCalculator>();
		USkillEffectManager *Effects = TestWorld->GetSubsystem<USkillEffectManager>();
		if (!Calculator || !Effects)
		{
			AddError(TEXT("no calculator or effect manager"));
			return INDEX_NONE;
		}
		UCharacterData *DefenderData = FWoRTestWorld::NewCharacterData();
		DefenderData->Defense = SCOPE_TEST_DEFENSE_POINTS;
		UCharacterData *AttackerData = FWoRTestWorld::NewCharacterData();
		AttackerData->RawDamage = AttackerRawPoints;
		AActor *Attacker = TestWorld->SpawnCombatant(AttackerData);
		AActor *Defender = TestWorld->SpawnCombatant(DefenderData);
		if (OnAttacker.Type != ESkillEffectType::None)
		{
			Effects->ApplyEffect(Attacker, MakeBoost(OnAttacker));
		}
		if (OnDefender.Type != ESkillEffectType::None)
		{
			Effects->ApplyEffect(Defender, MakeBoost(OnDefender));
		}
		FDamageCalculationInput Input;
		Input.BaseDamage = SCOPE_TEST_BASE_DAMAGE;
		Input.ActionType = ActionType;
		Input.Element = HitElement;
		Input.bIgnoreResistance = true;
		Input.OverrideCritChance = 0.0f;
		return Calculator->CalculateDamage(Attacker, Defender, Input).FinalDamage;
	}

	static FScopedBoost Boost(ESkillEffectType Type, EDamageScope Scope = EDamageScope::All, ESpellElement Element = ESpellElement::None)
	{
		FScopedBoost Out;
		Out.Type = Type;
		Out.Scope = Scope;
		Out.Element = Element;
		return Out;
	}
END_DEFINE_SPEC(FDamageScopeSpec)

void FDamageScopeSpec::Define()
{
	Describe("the scope rule", [this]()
	{
		It("All counts for everything; Physical for non-spells; Spell for spells; Element for its own element", [this]()
		{
			using namespace WoRDamageScope;
			TestTrue(TEXT("All, a Water spell"), Matches(EDamageScope::All, ESpellElement::None, ESpellElement::Water, true));
			TestTrue(TEXT("Physical, an ability"), Matches(EDamageScope::Physical, ESpellElement::None, ESpellElement::Fire, false));
			TestFalse(TEXT("Physical, a spell"), Matches(EDamageScope::Physical, ESpellElement::None, ESpellElement::Fire, true));
			TestTrue(TEXT("Spell, a spell"), Matches(EDamageScope::Spell, ESpellElement::None, ESpellElement::None, true));
			TestTrue(TEXT("Fire only, a Fire hit"), Matches(EDamageScope::Element, ESpellElement::Fire, ESpellElement::Fire, false));
			TestFalse(TEXT("Fire only, a Water hit"), Matches(EDamageScope::Element, ESpellElement::Fire, ESpellElement::Water, true));
			TestFalse(TEXT("Element scope with no element counts for nothing"),
					  Matches(EDamageScope::Element, ESpellElement::None, ESpellElement::None, false));
		});

		It("old data with no scope reads All, even with an Element set; the Element scope keys", [this]()
		{
			FSkillEffectPayload Old;
			Old.EffectType = ESkillEffectType::DamageBuff;
			Old.Element = ESpellElement::Fire;
			const FActiveSkillEffect FromOld = FActiveSkillEffect::BuildRuntimeFromPayload(TEXT("Old"), SCOPE_TEST_EFFECT_ID, Old, {}, false, 1, false);
			TestTrue(TEXT("an unscoped payload is All"), FromOld.DamageScope == EDamageScope::All);
			TestTrue(TEXT("so it counts for a Water hit"),
					 WoRDamageScope::Matches(FromOld.DamageScope, FromOld.Element, ESpellElement::Water, true));

			FSkillEffectPayload Keyed;
			Keyed.EffectType = ESkillEffectType::DamageBuff;
			Keyed.DamageScope = EDamageScope::Element;
			const FActiveSkillEffect FromCast = FActiveSkillEffect::BuildRuntimeFromPayload(TEXT("Keyed"), SCOPE_TEST_EFFECT_ID, Keyed, {}, false, 1,
																							false, ESpellElement::Lightning);
			TestTrue(TEXT("Element scope with no authored element keys to the cast element"), FromCast.Element == ESpellElement::Lightning);
		});
	});

	Describe("in the damage calculator", [this]()
	{
		BeforeEach([this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			TestTrue(TEXT("test world created"), TestWorld->Create());
		});

		AfterEach([this]() { TestWorld.Reset(); });

		It("an All-scope damage buff raises every hit, as before", [this]()
		{
			const int32 Plain = Hit(EActionType::Spell, ESpellElement::Water, FScopedBoost());
			TestTrue(TEXT("Water spell"), Hit(EActionType::Spell, ESpellElement::Water, Boost(ESkillEffectType::DamageBuff)) > Plain);
			TestTrue(TEXT("physical ability"),
					 Hit(EActionType::Ability, ESpellElement::None, Boost(ESkillEffectType::DamageBuff)) >
						 Hit(EActionType::Ability, ESpellElement::None, FScopedBoost()));
		});

		It("a Fire-scoped damage-dealt boost raises a Fire hit only", [this]()
		{
			const FScopedBoost FireOnly = Boost(ESkillEffectType::ModifyDamageDealt, EDamageScope::Element, ESpellElement::Fire);
			TestTrue(TEXT("Fire spell raised"),
					 Hit(EActionType::Spell, ESpellElement::Fire, FireOnly) > Hit(EActionType::Spell, ESpellElement::Fire, FScopedBoost()));
			TestEqual(TEXT("Water spell not"), Hit(EActionType::Spell, ESpellElement::Water, FireOnly),
					  Hit(EActionType::Spell, ESpellElement::Water, FScopedBoost()));
		});

		It("a Physical-scoped damage buff raises physical hits only", [this]()
		{
			const FScopedBoost PhysicalOnly = Boost(ESkillEffectType::DamageBuff, EDamageScope::Physical);
			TestTrue(TEXT("ability raised"),
					 Hit(EActionType::Ability, ESpellElement::None, PhysicalOnly) > Hit(EActionType::Ability, ESpellElement::None, FScopedBoost()));
			TestEqual(TEXT("spell not"), Hit(EActionType::Spell, ESpellElement::Fire, PhysicalOnly),
					  Hit(EActionType::Spell, ESpellElement::Fire, FScopedBoost()));
		});

		It("a Spell-scoped damage buff raises spells only", [this]()
		{
			const FScopedBoost SpellOnly = Boost(ESkillEffectType::DamageBuff, EDamageScope::Spell);
			TestTrue(TEXT("spell raised"),
					 Hit(EActionType::Spell, ESpellElement::Water, SpellOnly) > Hit(EActionType::Spell, ESpellElement::Water, FScopedBoost()));
			TestEqual(TEXT("ability not"), Hit(EActionType::Ability, ESpellElement::None, SpellOnly),
					  Hit(EActionType::Ability, ESpellElement::None, FScopedBoost()));
		});

		It("a Fire-scoped Raw damage buff leaves a Water physical hit alone, below and at the stat cap", [this]()
		{
			const FScopedBoost FireRaw = Boost(ESkillEffectType::RawDamageBuff, EDamageScope::Element, ESpellElement::Fire);
			TestEqual(TEXT("below the cap"), Hit(EActionType::Ability, ESpellElement::Water, FireRaw),
					  Hit(EActionType::Ability, ESpellElement::Water, FScopedBoost()));
			TestEqual(TEXT("at the cap (the cap correction reads the same scoped factor)"),
					  Hit(EActionType::Ability, ESpellElement::Water, FireRaw, FScopedBoost(), SCOPE_TEST_OVERCAP_POINTS),
					  Hit(EActionType::Ability, ESpellElement::Water, FScopedBoost(), FScopedBoost(), SCOPE_TEST_OVERCAP_POINTS));
			TestTrue(TEXT("a Fire physical hit is raised"),
					 Hit(EActionType::Ability, ESpellElement::Fire, FireRaw) > Hit(EActionType::Ability, ESpellElement::Fire, FScopedBoost()));
		});

		It("a Fire-scoped defence buff and damage-taken cut reduce Fire damage taken only", [this]()
		{
			for (const ESkillEffectType Type : {ESkillEffectType::DefenseBuff, ESkillEffectType::ReduceDamageTaken})
			{
				const FScopedBoost FireGuard = Boost(Type, EDamageScope::Element, ESpellElement::Fire);
				const FString Name = UEnum::GetValueAsString(Type);
				TestTrue(*(Name + TEXT(": Fire hit cut")), Hit(EActionType::Spell, ESpellElement::Fire, FScopedBoost(), FireGuard) <
															   Hit(EActionType::Spell, ESpellElement::Fire, FScopedBoost()));
				TestEqual(*(Name + TEXT(": Water hit not")), Hit(EActionType::Spell, ESpellElement::Water, FScopedBoost(), FireGuard),
						  Hit(EActionType::Spell, ESpellElement::Water, FScopedBoost()));
			}
		});

		It("with no scoped effect, the per-hit Defense equals GetDefenderFlatDefense; the debug lists the scope", [this]()
		{
			UDamageCalculator *Calculator = TestWorld->GetSubsystem<UDamageCalculator>();
			USkillEffectManager *Effects = TestWorld->GetSubsystem<USkillEffectManager>();
			if (!TestNotNull(TEXT("calculator"), Calculator) || !TestNotNull(TEXT("effects"), Effects))
			{
				return;
			}
			UCharacterData *Data = FWoRTestWorld::NewCharacterData();
			Data->Defense = SCOPE_TEST_DEFENSE_POINTS;
			AActor *Defender = TestWorld->SpawnCombatant(Data);
			Effects->ApplyEffect(Defender, MakeBoost(Boost(ESkillEffectType::DefenseBuff)));
			TestEqual(TEXT("same Defense"), Calculator->GetDefenderDefenseForHit(Defender, ESpellElement::Water, true),
					  Calculator->GetDefenderFlatDefense(Defender), KINDA_SMALL_NUMBER);
			TestTrue(TEXT("debug names the scope"), WoRDamageScopeDebug::GetScopesString(Defender).Contains(TEXT("[All]")));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
