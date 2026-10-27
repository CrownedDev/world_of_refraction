// InfusionCostSpec.cpp
// B-136: what an infused action costs today (UActionExecutor::CalculateActionEnergyCost, the preview
// that is also the EP spend, and UInfusionCostHelper's HP cost). A spell pays its charge multiplier
// (L1 x1.5, L2 x2.0); an infused ability pays INFUSION_ENERGY_MULTIPLIER on top (D-035: x2.25 / x3.0);
// both also pay an upside-only stat surcharge (RawDamage / SpellDamage above 1.0). Crystal-powered
// casts and a deferred fire cost no EP. The HP cost is the infused EP as a share of max EP, applied to
// max HP, less Resistance. Expectations are literals, so a changed number fails here on purpose.
// Not covered: ApplyCommitCosts' source routing (which sources pay HP) needs a running action; and the
// ability's x1.5 is rounded before the charge level, so x2.25 is exact only for bases like 100.
// Damage and status charge bands are WoR.Status.InfusedBuildup; wear is WoR.Durability.CrystalBreak.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/CombatConstants.h"
#include "Infusion/InfusionConstants.h"
#include "Infusion/InfusionCostHelper.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 COST_TEST_BASE_EP = 100;
	constexpr int32 COST_TEST_UNINFUSED = 0;
	constexpr int32 COST_TEST_L1 = 1;
	constexpr int32 COST_TEST_L2 = 2;
	/** D-035's numbers: the ability surcharge times each charge level. */
	constexpr float COST_TEST_D035_L1 = 2.25f;
	constexpr float COST_TEST_D035_L2 = 3.0f;
	/** Today's costs of a 100-EP action on a neutral caster. */
	constexpr int32 COST_TEST_ABILITY_L1_EP = 225;
	constexpr int32 COST_TEST_ABILITY_L2_EP = 300;
	constexpr int32 COST_TEST_ABILITY_SOURCE_ONLY_EP = 150;
	constexpr int32 COST_TEST_SPELL_L1_EP = 150;
	constexpr int32 COST_TEST_SPELL_L2_EP = 200;
	/** A +20% stat buff: surcharge x1.2 (ability L1 270, spell L1 180); a −20% debuff adds nothing. */
	constexpr float COST_TEST_STAT_SWING_PERCENT = 20.0f;
	constexpr int32 COST_TEST_ABILITY_L1_BUFFED_EP = 270;
	constexpr int32 COST_TEST_SPELL_L1_BUFFED_EP = 180;
	constexpr int32 COST_TEST_STAT_EFFECT_ID = 9601;
	constexpr int32 COST_TEST_DURATION_TURNS = 3;
	/** 100 EP of 200 is half: half of 400 HP is 200, less 25% Resistance is 150. */
	constexpr int32 COST_TEST_MAX_EP = 200;
	constexpr int32 COST_TEST_MAX_HP = 400;
	constexpr int32 COST_TEST_HP_COST = 200;
	constexpr float COST_TEST_RESIST_PERCENT = 25.0f;
	constexpr int32 COST_TEST_HP_COST_RESISTED = 150;
	constexpr float COST_TEST_TOLERANCE = 0.001f;
}

BEGIN_DEFINE_SPEC(FInfusionCostSpec, "WoR.Infusion.Cost",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UActionExecutor *Executor = nullptr;
	USkillEffectManager *Effects = nullptr;
	AActor *Caster = nullptr;
	UCharacterDataComponent *Stats = nullptr;
	UAbilityData *Ability = nullptr;
	USpellData *Spell = nullptr;

	FAction AbilityAction(EInfusionSourceOption Source, int32 Level) const
	{
		FAction Action;
		Action.ActionType = EActionType::Ability;
		Action.SkillData = Ability;
		Action.SelectedSource = Source;
		Action.AbilityInfusionLevel = Level;
		Action.Targets.Add(Caster);
		return Action;
	}

	int32 AbilityCost(EInfusionSourceOption Source, int32 Level)
	{
		return Executor->CalculateActionEnergyCost(Caster, AbilityAction(Source, Level));
	}

	int32 SpellCost(ESpellSource Source, int32 Level)
	{
		FAction Action;
		Action.ActionType = EActionType::Spell;
		Action.SpellData = Spell;
		Action.SpellSource = Source;
		Action.SelectedSource = (Level > COST_TEST_UNINFUSED) ? EInfusionSourceOption::Innate : EInfusionSourceOption::None;
		Action.SpellInfusionLevel = Level;
		Action.Targets.Add(Caster);
		return Executor->CalculateActionEnergyCost(Caster, Action);
	}

	void GiveStatEffect(ESkillEffectType Type, float Percent)
	{
		FActiveSkillEffect Effect = FActiveSkillEffect::CreateBuff(TEXT("Cost test stat"), COST_TEST_STAT_EFFECT_ID, Type, Percent,
																   COST_TEST_DURATION_TURNS);
		Effect.bDelayFirstExecution = true;
		Effects->ApplyEffect(Caster, Effect, Caster);
	}
END_DEFINE_SPEC(FInfusionCostSpec)

void FInfusionCostSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Executor = TestWorld->GetSubsystem<UActionExecutor>();
		Effects = TestWorld->GetSubsystem<USkillEffectManager>();
		Caster = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Stats = Caster ? Caster->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		Ability = NewObject<UAbilityData>(GetTransientPackage());
		Ability->BaseEnergyCost = COST_TEST_BASE_EP;
		Spell = NewObject<USpellData>(GetTransientPackage());
		Spell->BaseEnergyCost = COST_TEST_BASE_EP;
		if (!TestNotNull(TEXT("ActionExecutor"), Executor) || !TestNotNull(TEXT("SkillEffectManager"), Effects) ||
			!TestNotNull(TEXT("Caster stats"), Stats))
		{
			Executor = nullptr; // each case returns early
		}
	});

	AfterEach([this]()
	{
		Executor = nullptr;
		Effects = nullptr;
		Caster = nullptr;
		Stats = nullptr;
		Ability = nullptr;
		Spell = nullptr;
		TestWorld.Reset();
	});

	It("pins D-035's numbers: an infused ability pays x2.25 at L1 and x3.0 at L2", [this]()
	{
		TestNearlyEqual(TEXT("L1"), CombatConstants::INFUSION_ENERGY_MULTIPLIER * InfusionConstants::L1_ENERGY_MULT,
						COST_TEST_D035_L1, COST_TEST_TOLERANCE);
		TestNearlyEqual(TEXT("L2"), CombatConstants::INFUSION_ENERGY_MULTIPLIER * InfusionConstants::L2_ENERGY_MULT,
						COST_TEST_D035_L2, COST_TEST_TOLERANCE);
	});

	It("charges an ability its base cost uninfused, then the surcharge and charge level", [this]()
	{
		if (!Executor) return;
		TestEqual(TEXT("Uninfused"), AbilityCost(EInfusionSourceOption::None, COST_TEST_UNINFUSED), COST_TEST_BASE_EP);
		TestEqual(TEXT("Raw L1"), AbilityCost(EInfusionSourceOption::Raw, COST_TEST_L1), COST_TEST_ABILITY_L1_EP);
		TestEqual(TEXT("Raw L2"), AbilityCost(EInfusionSourceOption::Raw, COST_TEST_L2), COST_TEST_ABILITY_L2_EP);
		TestEqual(TEXT("Innate L1 pays the same EP"), AbilityCost(EInfusionSourceOption::Innate, COST_TEST_L1), COST_TEST_ABILITY_L1_EP);
		TestEqual(TEXT("A chosen source at L0 still pays the surcharge (recorded)"),
				  AbilityCost(EInfusionSourceOption::Raw, COST_TEST_UNINFUSED), COST_TEST_ABILITY_SOURCE_ONLY_EP);
	});

	It("charges an innate spell only its charge level", [this]()
	{
		if (!Executor) return;
		TestEqual(TEXT("Uninfused"), SpellCost(ESpellSource::Innate, COST_TEST_UNINFUSED), COST_TEST_BASE_EP);
		TestEqual(TEXT("L1"), SpellCost(ESpellSource::Innate, COST_TEST_L1), COST_TEST_SPELL_L1_EP);
		TestEqual(TEXT("L2"), SpellCost(ESpellSource::Innate, COST_TEST_L2), COST_TEST_SPELL_L2_EP);
	});

	It("adds a surcharge for a stat above 1.0, and nothing for one below", [this]()
	{
		if (!Executor) return;
		GiveStatEffect(ESkillEffectType::RawDamageBuff, COST_TEST_STAT_SWING_PERCENT);
		TestEqual(TEXT("Ability L1 with RawDamage x1.2"), AbilityCost(EInfusionSourceOption::Raw, COST_TEST_L1), COST_TEST_ABILITY_L1_BUFFED_EP);
		TestEqual(TEXT("Uninfused: no surcharge"), AbilityCost(EInfusionSourceOption::None, COST_TEST_UNINFUSED), COST_TEST_BASE_EP);
		Effects->RemoveAllEffects(Caster);

		GiveStatEffect(ESkillEffectType::SpellDamageBuff, COST_TEST_STAT_SWING_PERCENT);
		TestEqual(TEXT("Spell L1 with SpellDamage x1.2"), SpellCost(ESpellSource::Innate, COST_TEST_L1), COST_TEST_SPELL_L1_BUFFED_EP);
		Effects->RemoveAllEffects(Caster);

		GiveStatEffect(ESkillEffectType::RawDamageDebuff, COST_TEST_STAT_SWING_PERCENT);
		TestEqual(TEXT("A low stat never makes it cheaper"), AbilityCost(EInfusionSourceOption::Raw, COST_TEST_L1), COST_TEST_ABILITY_L1_EP);
	});

	It("charges no EP when a crystal powers the cast, or for a deferred fire", [this]()
	{
		if (!Executor) return;
		TestEqual(TEXT("Ability through the active ring"), AbilityCost(EInfusionSourceOption::ActiveRing, COST_TEST_L2), 0);
		TestEqual(TEXT("Ability through the primary ring"), AbilityCost(EInfusionSourceOption::PrimaryRing, COST_TEST_L2), 0);
		TestEqual(TEXT("Ability through the weapon crystal"), AbilityCost(EInfusionSourceOption::WeaponCrystal, COST_TEST_L2), 0);
		TestEqual(TEXT("Ring spell"), SpellCost(ESpellSource::RingCrystal, COST_TEST_L2), 0);
		TestEqual(TEXT("Weapon-crystal spell"), SpellCost(ESpellSource::WeaponCrystal, COST_TEST_L2), 0);
		FAction Deferred = AbilityAction(EInfusionSourceOption::Raw, COST_TEST_L2);
		Deferred.bIsDeferredFire = true;
		TestEqual(TEXT("Deferred fire already paid at arm"), Executor->CalculateActionEnergyCost(Caster, Deferred), 0);
	});

	It("turns infused EP into HP by share of max EP, less Resistance", [this]()
	{
		if (!Executor) return;
		Stats->MaxEP = COST_TEST_MAX_EP;
		Stats->MaxHP = COST_TEST_MAX_HP;
		if (!TestNearlyEqual(TEXT("No Resistance to start"), Stats->GetEffectiveStats().Resistance, 0.0f, COST_TEST_TOLERANCE)) return;
		TestEqual(TEXT("HP cost"), UInfusionCostHelper::CalculateHPCost(Caster, COST_TEST_BASE_EP), COST_TEST_HP_COST);
		TestEqual(TEXT("No EP, no HP"), UInfusionCostHelper::CalculateHPCost(Caster, 0), 0);

		Stats->CurrentHP = COST_TEST_HP_COST;
		TestTrue(TEXT("A cost equal to current HP would kill"), UInfusionCostHelper::WouldKill(Caster, COST_TEST_BASE_EP));
		Stats->CurrentHP = COST_TEST_HP_COST + 1;
		TestFalse(TEXT("One HP more survives"), UInfusionCostHelper::WouldKill(Caster, COST_TEST_BASE_EP));

		GiveStatEffect(ESkillEffectType::ModifyStatusResist, COST_TEST_RESIST_PERCENT);
		TestEqual(TEXT("25% Resistance takes a quarter off"), UInfusionCostHelper::CalculateHPCost(Caster, COST_TEST_BASE_EP),
				  COST_TEST_HP_COST_RESISTED);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
