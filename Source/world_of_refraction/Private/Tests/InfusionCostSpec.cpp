// InfusionCostSpec.cpp
// B-136: what an infused action costs (UActionExecutor::CalculateActionEnergyCost, the preview that is
// also the EP spend, and UInfusionCostHelper's HP cost). B-334 (D-126): the EP cost is the base x the
// charge level (L0 / L1 x1, L2 x2), for spells and abilities alike; an infused ability's base carries
// INFUSION_ENERGY_MULTIPLIER (D-035 stands: x1.5 at L1, x3.0 at L2); the stat surcharge is gone (Q-334).
// Crystal-powered casts and a deferred fire cost no EP. The HP cost is the infused EP as a share of max
// EP, applied to max HP, less Resistance, and only at L2 (ApplyCommitCosts, checked through the friend
// access). Expectations are literals, so a changed number fails here on purpose.
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
	constexpr int32 COST_TEST_PAST_MAX_LEVEL = 3;
	/** D-126: the charge level is the multiplier (x1 with no charge). */
	constexpr float COST_TEST_LEVEL_MULT_L0 = 1.0f;
	constexpr float COST_TEST_LEVEL_MULT_L1 = 1.0f;
	constexpr float COST_TEST_LEVEL_MULT_L2 = 2.0f;
	/** D-035 stands: an infused ability's base is x1.5. */
	constexpr float COST_TEST_D035_SURCHARGE = 1.5f;
	/** Costs of a 100-EP action on a neutral caster (D-126). */
	constexpr int32 COST_TEST_ABILITY_L1_EP = 150;
	constexpr int32 COST_TEST_ABILITY_L2_EP = 300;
	constexpr int32 COST_TEST_ABILITY_SOURCE_ONLY_EP = 150;
	constexpr int32 COST_TEST_SPELL_L1_EP = 100;
	constexpr int32 COST_TEST_SPELL_L2_EP = 200;
	/** A +20% stat buff no longer adds a surcharge (Q-334). */
	constexpr float COST_TEST_STAT_SWING_PERCENT = 20.0f;
	constexpr int32 COST_TEST_STAT_EFFECT_ID = 9601;
	constexpr int32 COST_TEST_DURATION_TURNS = 3;
	/** 100 EP of 200 is half: half of 400 HP is 200, less 25% Resistance is 150. */
	constexpr int32 COST_TEST_MAX_EP = 200;
	constexpr int32 COST_TEST_MAX_HP = 400;
	constexpr int32 COST_TEST_HP_COST = 200;
	constexpr float COST_TEST_RESIST_PERCENT = 25.0f;
	constexpr int32 COST_TEST_HP_COST_RESISTED = 150;
	/** An L2 innate spell's 200 EP is all of 200 max EP: all of 400 max HP. */
	constexpr int32 COST_TEST_SPELL_L2_HP_COST = 400;
	/** An L2 Raw ability's 300 EP is 1.5x of 200 max EP: 1.5x of 400 max HP. */
	constexpr int32 COST_TEST_ABILITY_L2_HP_COST = 600;
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

	FAction SpellAction(ESpellSource Source, int32 Level) const
	{
		FAction Action;
		Action.ActionType = EActionType::Spell;
		Action.SpellData = Spell;
		Action.SpellSource = Source;
		Action.SelectedSource = (Level > COST_TEST_UNINFUSED) ? EInfusionSourceOption::Innate : EInfusionSourceOption::None;
		Action.SpellInfusionLevel = Level;
		Action.Targets.Add(Caster);
		return Action;
	}

	int32 SpellCost(ESpellSource Source, int32 Level)
	{
		return Executor->CalculateActionEnergyCost(Caster, SpellAction(Source, Level));
	}

	/** The HP cost ApplyCommitCosts stashes for Action (paid at finalize), read through the friend access. */
	int32 CommitHPCost(const FAction &Action)
	{
		Executor->CurrentExecutionContext.Emplace();
		Executor->ApplyCommitCosts(Caster, Action);
		const int32 Cost = Executor->CurrentExecutionContext->PendingInfusionHPCost;
		Executor->CurrentExecutionContext.Reset();
		return Cost;
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

	It("pins D-126: the EP multiplier is the charge level, and only L2 pays HP", [this]()
	{
		TestNearlyEqual(TEXT("L0"), InfusionConstants::ChargeLevelCostMultiplier(COST_TEST_UNINFUSED), COST_TEST_LEVEL_MULT_L0, COST_TEST_TOLERANCE);
		TestNearlyEqual(TEXT("L1"), InfusionConstants::ChargeLevelCostMultiplier(COST_TEST_L1), COST_TEST_LEVEL_MULT_L1, COST_TEST_TOLERANCE);
		TestNearlyEqual(TEXT("L2"), InfusionConstants::ChargeLevelCostMultiplier(COST_TEST_L2), COST_TEST_LEVEL_MULT_L2, COST_TEST_TOLERANCE);
		TestNearlyEqual(TEXT("Past the max clamps to L2"), InfusionConstants::ChargeLevelCostMultiplier(COST_TEST_PAST_MAX_LEVEL),
						COST_TEST_LEVEL_MULT_L2, COST_TEST_TOLERANCE);
		TestNearlyEqual(TEXT("D-035 stands"), CombatConstants::INFUSION_ENERGY_MULTIPLIER, COST_TEST_D035_SURCHARGE, COST_TEST_TOLERANCE);
		TestFalse(TEXT("L1 pays no HP"), InfusionConstants::LevelPaysHP(COST_TEST_L1));
		TestTrue(TEXT("L2 pays HP"), InfusionConstants::LevelPaysHP(COST_TEST_L2));
	});

	It("charges an ability its base cost uninfused, then D-035's surcharge times the charge level", [this]()
	{
		if (!Executor) return;
		TestEqual(TEXT("Uninfused"), AbilityCost(EInfusionSourceOption::None, COST_TEST_UNINFUSED), COST_TEST_BASE_EP);
		TestEqual(TEXT("Raw L1"), AbilityCost(EInfusionSourceOption::Raw, COST_TEST_L1), COST_TEST_ABILITY_L1_EP);
		TestEqual(TEXT("Raw L2"), AbilityCost(EInfusionSourceOption::Raw, COST_TEST_L2), COST_TEST_ABILITY_L2_EP);
		TestEqual(TEXT("Innate L1 pays the same EP"), AbilityCost(EInfusionSourceOption::Innate, COST_TEST_L1), COST_TEST_ABILITY_L1_EP);
		TestEqual(TEXT("A chosen source at L0 still pays the surcharge (recorded)"),
				  AbilityCost(EInfusionSourceOption::Raw, COST_TEST_UNINFUSED), COST_TEST_ABILITY_SOURCE_ONLY_EP);
	});

	It("charges an innate spell its base times the charge level", [this]()
	{
		if (!Executor) return;
		TestEqual(TEXT("Uninfused"), SpellCost(ESpellSource::Innate, COST_TEST_UNINFUSED), COST_TEST_BASE_EP);
		TestEqual(TEXT("L1"), SpellCost(ESpellSource::Innate, COST_TEST_L1), COST_TEST_SPELL_L1_EP);
		TestEqual(TEXT("L2"), SpellCost(ESpellSource::Innate, COST_TEST_L2), COST_TEST_SPELL_L2_EP);
	});

	It("adds no stat surcharge any more (Q-334)", [this]()
	{
		if (!Executor) return;
		GiveStatEffect(ESkillEffectType::RawDamageBuff, COST_TEST_STAT_SWING_PERCENT);
		TestEqual(TEXT("Ability L1 with RawDamage x1.2"), AbilityCost(EInfusionSourceOption::Raw, COST_TEST_L1), COST_TEST_ABILITY_L1_EP);
		Effects->RemoveAllEffects(Caster);

		GiveStatEffect(ESkillEffectType::SpellDamageBuff, COST_TEST_STAT_SWING_PERCENT);
		TestEqual(TEXT("Spell L2 with SpellDamage x1.2"), SpellCost(ESpellSource::Innate, COST_TEST_L2), COST_TEST_SPELL_L2_EP);
		Effects->RemoveAllEffects(Caster);
	});

	It("charges the HP cost only at L2", [this]()
	{
		if (!Executor) return;
		Stats->MaxEP = COST_TEST_MAX_EP;
		Stats->MaxHP = COST_TEST_MAX_HP;
		if (!TestNearlyEqual(TEXT("No Resistance to start"), Stats->GetEffectiveStats().Resistance, 0.0f, COST_TEST_TOLERANCE)) return;
		TestEqual(TEXT("Innate spell L1: no HP"), CommitHPCost(SpellAction(ESpellSource::Innate, COST_TEST_L1)), 0);
		TestEqual(TEXT("Innate spell L2: HP"), CommitHPCost(SpellAction(ESpellSource::Innate, COST_TEST_L2)), COST_TEST_SPELL_L2_HP_COST);
		TestEqual(TEXT("Raw ability L1: no HP"), CommitHPCost(AbilityAction(EInfusionSourceOption::Raw, COST_TEST_L1)), 0);
		TestEqual(TEXT("Raw ability L2: HP"), CommitHPCost(AbilityAction(EInfusionSourceOption::Raw, COST_TEST_L2)), COST_TEST_ABILITY_L2_HP_COST);
		TestEqual(TEXT("Innate ability L2: still no HP"), CommitHPCost(AbilityAction(EInfusionSourceOption::Innate, COST_TEST_L2)), 0);
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
