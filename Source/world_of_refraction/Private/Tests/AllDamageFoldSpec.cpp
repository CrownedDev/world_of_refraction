// AllDamageFoldSpec.cpp
// B-384 (Crown 2026-11-08, D-167): the four "all damage %" channels add into one total. Two +20% sources give +40%,
// not +44%; the difficulty's enemy-strength buff is one of them; scopes (B-383) still apply; the old multiplied path
// stays only behind wor.Damage.FoldAllDamage 0.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Combat/Damage/DamageCalculator.h"
#include "Debug/Combat/DamageScopeDebug.h"
#include "HAL/IConsoleManager.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/EDamageScope.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 FOLD_TEST_BASE_DAMAGE = 100;
	constexpr int32 FOLD_TEST_EFFECT_ID = 9384;
	constexpr float FOLD_TEST_PERCENT = 20.0f;
	constexpr int32 FOLD_TEST_TURNS = 2;

	struct FFoldBoost
	{
		ESkillEffectType Type = ESkillEffectType::None;
		float Percent = FOLD_TEST_PERCENT;
		EDamageScope Scope = EDamageScope::All;
		ESpellElement Element = ESpellElement::None;
	};
}

BEGIN_DEFINE_SPEC(FAllDamageFoldSpec, "WoR.Damage.AllDamageFold", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	AActor *LastAttacker = nullptr;

	static FFoldBoost Of(ESkillEffectType Type, float Percent = FOLD_TEST_PERCENT, EDamageScope Scope = EDamageScope::All,
						 ESpellElement Element = ESpellElement::None)
	{
		FFoldBoost Boost;
		Boost.Type = Type;
		Boost.Percent = Percent;
		Boost.Scope = Scope;
		Boost.Element = Element;
		return Boost;
	}

	/** One hit's final damage with Boosts on the attacker (each its own effect id, as from different sources). */
	int32 Hit(EActionType ActionType, ESpellElement HitElement, const TArray<FFoldBoost> &Boosts)
	{
		UDamageCalculator *Calculator = TestWorld->GetSubsystem<UDamageCalculator>();
		USkillEffectManager *Effects = TestWorld->GetSubsystem<USkillEffectManager>();
		if (!Calculator || !Effects)
		{
			AddError(TEXT("no calculator or effect manager"));
			return INDEX_NONE;
		}
		AActor *Attacker = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		AActor *Defender = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		int32 Id = FOLD_TEST_EFFECT_ID;
		for (const FFoldBoost &Boost : Boosts)
		{
			FActiveSkillEffect Effect = FActiveSkillEffect::CreateBuff(TEXT("Fold boost"), Id++, Boost.Type, Boost.Percent, FOLD_TEST_TURNS);
			Effect.DamageScope = Boost.Scope;
			Effect.Element = Boost.Element;
			Effects->ApplyEffect(Attacker, Effect);
		}
		LastAttacker = Attacker;
		FDamageCalculationInput Input;
		Input.BaseDamage = FOLD_TEST_BASE_DAMAGE;
		Input.ActionType = ActionType;
		Input.Element = HitElement;
		Input.bIgnoreResistance = true;
		Input.bIgnoreDefense = true;
		Input.OverrideCritChance = 0.0f;
		return Calculator->CalculateDamage(Attacker, Defender, Input).FinalDamage;
	}

	float PercentOfLast(EActionType ActionType, ESpellElement HitElement)
	{
		UDamageCalculator *Calculator = TestWorld->GetSubsystem<UDamageCalculator>();
		return Calculator && LastAttacker ? Calculator->GetAllDamagePercent(LastAttacker, ActionType, HitElement) : -1.0f;
	}
END_DEFINE_SPEC(FAllDamageFoldSpec)

void FAllDamageFoldSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());
		LastAttacker = nullptr;
	});

	AfterEach([this]()
	{
		LastAttacker = nullptr;
		TestWorld.Reset();
	});

	It("two +20% sources add to +40%, the same as one +40% buff (not +44%)", [this]()
	{
		const int32 TwoSources = Hit(EActionType::Spell, ESpellElement::Fire,
									 {Of(ESkillEffectType::DamageBuff), Of(ESkillEffectType::ModifyDamageDealt)});
		TestEqual(TEXT("one total"), PercentOfLast(EActionType::Spell, ESpellElement::Fire), 2.0f * FOLD_TEST_PERCENT, KINDA_SMALL_NUMBER);
		const int32 OneBuff = Hit(EActionType::Spell, ESpellElement::Fire, {Of(ESkillEffectType::DamageBuff, 2.0f * FOLD_TEST_PERCENT)});
		TestEqual(TEXT("same damage as one +40% buff"), TwoSources, OneBuff);
	});

	It("the Raw axis joins the sum for physical hits, the Spell axis for spells", [this]()
	{
		Hit(EActionType::Ability, ESpellElement::None, {Of(ESkillEffectType::RawDamageBuff), Of(ESkillEffectType::DamageBuff)});
		TestEqual(TEXT("physical: Raw + generic"), PercentOfLast(EActionType::Ability, ESpellElement::None), 2.0f * FOLD_TEST_PERCENT,
				  KINDA_SMALL_NUMBER);
		TestEqual(TEXT("a spell: generic only"), PercentOfLast(EActionType::Spell, ESpellElement::Fire), FOLD_TEST_PERCENT, KINDA_SMALL_NUMBER);
	});

	It("the difficulty's enemy-strength buff (a persistent gear DamageBuff) adds into the same total", [this]()
	{
		Hit(EActionType::Spell, ESpellElement::Fire, {Of(ESkillEffectType::ModifyDamageDealt)});
		USkillEffectManager *Effects = TestWorld->GetSubsystem<USkillEffectManager>();
		if (!TestNotNull(TEXT("effects"), Effects) || !TestNotNull(TEXT("attacker"), LastAttacker))
		{
			return;
		}
		FActiveSkillEffect Strength = FActiveSkillEffect::CreatePersistent(TEXT("Enemy strength (damage)"), FOLD_TEST_EFFECT_ID + 100,
																		   ESkillEffectType::DamageBuff, FOLD_TEST_PERCENT);
		Strength.bFromGear = true;
		Effects->ApplyEffect(LastAttacker, Strength);
		TestEqual(TEXT("+20 dealt + 20 strength"), PercentOfLast(EActionType::Spell, ESpellElement::Fire), 2.0f * FOLD_TEST_PERCENT,
				  KINDA_SMALL_NUMBER);
	});

	It("scopes still apply: a Fire-only buff stays out of a Water hit's total", [this]()
	{
		Hit(EActionType::Spell, ESpellElement::Water,
			{Of(ESkillEffectType::DamageBuff, FOLD_TEST_PERCENT, EDamageScope::Element, ESpellElement::Fire), Of(ESkillEffectType::ModifyDamageDealt)});
		TestEqual(TEXT("Water: the All one only"), PercentOfLast(EActionType::Spell, ESpellElement::Water), FOLD_TEST_PERCENT, KINDA_SMALL_NUMBER);
		TestEqual(TEXT("Fire: both"), PercentOfLast(EActionType::Spell, ESpellElement::Fire), 2.0f * FOLD_TEST_PERCENT, KINDA_SMALL_NUMBER);
		TestTrue(TEXT("the debug shows the one line"), WoRDamageScopeDebug::GetAllDamageString(LastAttacker).Contains(TEXT("All-damage")));
	});

	It("wor.Damage.FoldAllDamage 0 brings back the old multiplied channels (before / after check)", [this]()
	{
		IConsoleVariable *Fold = IConsoleManager::Get().FindConsoleVariable(TEXT("wor.Damage.FoldAllDamage"));
		if (!TestNotNull(TEXT("the switch exists"), Fold))
		{
			return;
		}
		const TArray<FFoldBoost> Stack = {Of(ESkillEffectType::DamageBuff), Of(ESkillEffectType::ModifyDamageDealt)};
		const int32 Folded = Hit(EActionType::Spell, ESpellElement::Fire, Stack);
		Fold->Set(0, ECVF_SetByCode);
		const int32 Multiplied = Hit(EActionType::Spell, ESpellElement::Fire, Stack);
		Fold->Set(1, ECVF_SetByCode);
		TestTrue(TEXT("the old path stacks higher (x1.44 against x1.40)"), Multiplied > Folded);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
