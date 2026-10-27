// InfusionCostSpec.cpp
// B-136: what an infused action costs today (UActionExecutor::CalculateActionEnergyCost, the preview
// that is also the spend, and UInfusionCostHelper's HP cost). A spell pays its charge multiplier
// (L1 x1.5, L2 x2.0); an infused ability pays INFUSION_ENERGY_MULTIPLIER on top (D-035: x2.25 / x3.0);
// both also pay the upside-only stat surcharge. Crystal-powered casts cost no EP. The HP cost is the
// infused EP as a share of max EP, applied to max HP, less Resistance. Damage and status charge bands
// are WoR.Status.InfusedBuildup; crystal wear and breaks are WoR.Durability.CrystalBreak.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/CombatConstants.h"
#include "Infusion/InfusionConstants.h"
#include "Infusion/InfusionCostHelper.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/EScalingTier.h"
#include "Skills/Definitions/SpellData.h"

namespace
{
	/** Large enough that every multiplier lands on a whole number before rounding. */
	constexpr int32 COST_TEST_BASE_EP = 100;
	constexpr int32 COST_TEST_UNINFUSED = 0;
	constexpr int32 COST_TEST_L1 = 1;
	constexpr int32 COST_TEST_L2 = 2;
	/** D-035's numbers: the ability surcharge times each charge level. */
	constexpr float COST_TEST_D035_L1 = 2.25f;
	constexpr float COST_TEST_D035_L2 = 3.0f;
	constexpr int32 COST_TEST_MAX_EP = 200;
	constexpr int32 COST_TEST_MAX_HP = 400;
	constexpr float COST_TEST_TOLERANCE = 0.001f;
}

BEGIN_DEFINE_SPEC(FInfusionCostSpec, "WoR.Infusion.Cost",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UActionExecutor *Executor = nullptr;
	AActor *Caster = nullptr;
	UCharacterDataComponent *Stats = nullptr;
	UAbilityData *Ability = nullptr;
	USpellData *Spell = nullptr;

	int32 AbilityCost(EInfusionSourceOption Source, int32 Level)
	{
		FAction Action;
		Action.ActionType = EActionType::Ability;
		Action.SkillData = Ability;
		Action.SelectedSource = Source;
		Action.AbilityInfusionLevel = Level;
		Action.Targets.Add(Caster);
		return Executor->CalculateActionEnergyCost(Caster, Action);
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

	/** The base cost times Multiplier times the caster's upside-only surcharge on Stat. */
	static int32 Expected(float Multiplier, float Stat)
	{
		return FMath::RoundToInt(COST_TEST_BASE_EP * Multiplier * (1.0f + StatFraction(Stat)));
	}
END_DEFINE_SPEC(FInfusionCostSpec)

void FInfusionCostSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Executor = TestWorld->GetSubsystem<UActionExecutor>();
		Caster = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Stats = Caster ? Caster->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		Ability = NewObject<UAbilityData>(GetTransientPackage());
		Ability->BaseEnergyCost = COST_TEST_BASE_EP;
		Spell = NewObject<USpellData>(GetTransientPackage());
		Spell->BaseEnergyCost = COST_TEST_BASE_EP;
		if (!TestNotNull(TEXT("ActionExecutor"), Executor) || !TestNotNull(TEXT("Caster stats"), Stats))
		{
			Executor = nullptr; // each case returns early
		}
	});

	AfterEach([this]()
	{
		Executor = nullptr;
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
		if (!TestEqual(TEXT("Uninfused: the base cost (no efficiency, tier or gap change on this caster)"),
					   AbilityCost(EInfusionSourceOption::None, COST_TEST_UNINFUSED), COST_TEST_BASE_EP)) return;
		const float RawDamage = Stats->GetEffectiveRawDamage();
		TestEqual(TEXT("L1"), AbilityCost(EInfusionSourceOption::Raw, COST_TEST_L1), Expected(COST_TEST_D035_L1, RawDamage));
		TestEqual(TEXT("L2"), AbilityCost(EInfusionSourceOption::Raw, COST_TEST_L2), Expected(COST_TEST_D035_L2, RawDamage));
	});

	It("charges an innate spell only its charge level", [this]()
	{
		if (!Executor) return;
		if (!TestEqual(TEXT("Uninfused: the base cost"), SpellCost(ESpellSource::Innate, COST_TEST_UNINFUSED), COST_TEST_BASE_EP)) return;
		const float SpellDamage = Stats->GetEffectiveStats().SpellDamage;
		TestEqual(TEXT("L1"), SpellCost(ESpellSource::Innate, COST_TEST_L1), Expected(InfusionConstants::SPELL_L1_ENERGY_MULT, SpellDamage));
		TestEqual(TEXT("L2"), SpellCost(ESpellSource::Innate, COST_TEST_L2), Expected(InfusionConstants::SPELL_L2_ENERGY_MULT, SpellDamage));
	});

	It("charges no EP when a crystal powers the cast", [this]()
	{
		if (!Executor) return;
		TestEqual(TEXT("Ability through the active ring"), AbilityCost(EInfusionSourceOption::ActiveRing, COST_TEST_L2), 0);
		TestEqual(TEXT("Ability through the weapon crystal"), AbilityCost(EInfusionSourceOption::WeaponCrystal, COST_TEST_L2), 0);
		TestEqual(TEXT("Ring spell"), SpellCost(ESpellSource::RingCrystal, COST_TEST_L2), 0);
		TestEqual(TEXT("Weapon-crystal spell"), SpellCost(ESpellSource::WeaponCrystal, COST_TEST_L2), 0);
	});

	It("turns infused EP into HP by share of max EP, less Resistance", [this]()
	{
		if (!Executor) return;
		Stats->MaxEP = COST_TEST_MAX_EP;
		Stats->MaxHP = COST_TEST_MAX_HP;
		const float Resistance = Stats->GetEffectiveStats().Resistance;
		const int32 ExpectedHP = FMath::RoundToInt(static_cast<float>(COST_TEST_BASE_EP) / COST_TEST_MAX_EP * COST_TEST_MAX_HP *
												   FMath::Max(0.0f, 1.0f - Resistance));
		TestEqual(TEXT("HP cost"), UInfusionCostHelper::CalculateHPCost(Caster, COST_TEST_BASE_EP), ExpectedHP);
		TestEqual(TEXT("No EP, no HP"), UInfusionCostHelper::CalculateHPCost(Caster, 0), 0);

		Stats->CurrentHP = ExpectedHP;
		TestTrue(TEXT("A cost equal to current HP would kill"), UInfusionCostHelper::WouldKill(Caster, COST_TEST_BASE_EP));
		Stats->CurrentHP = ExpectedHP + 1;
		TestFalse(TEXT("One HP more survives"), UInfusionCostHelper::WouldKill(Caster, COST_TEST_BASE_EP));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
