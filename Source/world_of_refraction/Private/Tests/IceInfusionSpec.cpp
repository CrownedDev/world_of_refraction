// IceInfusionSpec.cpp
// B-593 (D-271, Crown 2026-11-24): a Water source also offers an Ice infusion. It deals Ice (the Ice hit kind) and
// costs ICE_INFUSION_COST_MULTIPLIER more, except on a spell marked Deals Ice; the picker lists each Water source's
// Ice entry right after it, and a co-op pick carries the choice.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/Actions/CombatPickRequest.h"
#include "Equipment/Durability/BreakCalculator.h"
#include "Equipment/Durability/DurabilityConstants.h"
#include "Infusion/InfusionConstants.h"
#include "Infusion/InfusionVFXComponent.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"

namespace
{
	constexpr int32 ICE_TEST_BASE_EP = 100;
	constexpr int32 ICE_TEST_L1 = 1;
	constexpr int32 ICE_TEST_L0 = 0;
	/** Rounding of the EP total may move it by one point. */
	constexpr int32 ICE_TEST_ROUNDING = 1;
	constexpr int32 ICE_TEST_DAMAGE = 20;
	constexpr int32 ICE_TEST_BUILDUP = 10;

	FAction IceTestAbility(UAbilityData *Ability, int32 Level, bool bIce)
	{
		FAction Action;
		Action.ActionType = EActionType::Ability;
		Action.SkillData = Ability;
		Action.SelectedSource = EInfusionSourceOption::Innate;
		Action.AbilityInfusionLevel = Level;
		Action.bIceInfusion = bIce;
		return Action;
	}

	FAction IceTestSpell(USpellData *Spell, bool bIce)
	{
		FAction Action;
		Action.ActionType = EActionType::Spell;
		Action.SpellData = Spell;
		Action.SelectedSource = EInfusionSourceOption::Innate;
		Action.SpellInfusionLevel = ICE_TEST_L1;
		Action.bIceInfusion = bIce;
		return Action;
	}
}

BEGIN_DEFINE_SPEC(FIceInfusionSpec, "WoR.Infusion.Ice", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FIceInfusionSpec)

void FIceInfusionSpec::Define()
{
	It("deals Ice only for an infused Ice pick on Water, or a Deals Ice spell cast as Water", [this]()
	{
		UAbilityData *Ability = NewObject<UAbilityData>(GetTransientPackage());
		const FAction Ice = IceTestAbility(Ability, ICE_TEST_L1, true);
		TestTrue(TEXT("an Ice pick on Water is an Ice infusion"), UActionExecutor::IsIceInfusion(Ice, ESpellElement::Water));
		TestEqual(TEXT("its hits deal Ice"), UActionExecutor::GetActionDealtTypeOverride(Ice, ESpellElement::Water), EWoRDamageType::Ice);
		TestEqual(TEXT("not on a source that isn't Water"), UActionExecutor::GetActionDealtTypeOverride(Ice, ESpellElement::Fire), EWoRDamageType::None);
		TestEqual(TEXT("not uninfused"), UActionExecutor::GetActionDealtTypeOverride(IceTestAbility(Ability, ICE_TEST_L0, true), ESpellElement::Water),
				  EWoRDamageType::None);
		TestEqual(TEXT("a plain Water pick deals Water"), UActionExecutor::GetActionDealtTypeOverride(IceTestAbility(Ability, ICE_TEST_L1, false), ESpellElement::Water),
				  EWoRDamageType::None);

		USpellData *IceSpell = NewObject<USpellData>(GetTransientPackage());
		IceSpell->Element = ESpellElement::Water;
		IceSpell->bDealsIce = true;
		TestEqual(TEXT("a Deals Ice spell still deals Ice without the pick"), UActionExecutor::GetActionDealtTypeOverride(IceTestSpell(IceSpell, false), ESpellElement::Water),
				  EWoRDamageType::Ice);
	});

	It("costs an Ice infusion more, but not on a spell marked Deals Ice", [this]()
	{
		UAbilityData *Ability = NewObject<UAbilityData>(GetTransientPackage());
		USpellData *WaterSpell = NewObject<USpellData>(GetTransientPackage());
		WaterSpell->Element = ESpellElement::Water;
		WaterSpell->BaseDamage = ICE_TEST_DAMAGE;
		USpellData *IceSpell = NewObject<USpellData>(GetTransientPackage());
		IceSpell->Element = ESpellElement::Water;
		IceSpell->BaseDamage = ICE_TEST_DAMAGE;
		IceSpell->bDealsIce = true;
		const float Ice = InfusionConstants::ICE_INFUSION_COST_MULTIPLIER;
		TestTrue(TEXT("Ice costs more than Water"), Ice > 1.0f);
		TestEqual(TEXT("an Ice ability"), UActionExecutor::GetIceInfusionCostMultiplier(IceTestAbility(Ability, ICE_TEST_L1, true), ESpellElement::Water), Ice);
		TestEqual(TEXT("a plain Water spell infused Ice"), UActionExecutor::GetIceInfusionCostMultiplier(IceTestSpell(WaterSpell, true), ESpellElement::Water), Ice);
		TestEqual(TEXT("a Deals Ice spell infused Ice: normal"), UActionExecutor::GetIceInfusionCostMultiplier(IceTestSpell(IceSpell, true), ESpellElement::Water), 1.0f);
		TestEqual(TEXT("a Water pick: normal"), UActionExecutor::GetIceInfusionCostMultiplier(IceTestAbility(Ability, ICE_TEST_L1, false), ESpellElement::Water), 1.0f);
	});

	It("charges no Ice price on a Water spell with no damage and no status buildup (B-593b)", [this]()
	{
		const float Ice = InfusionConstants::ICE_INFUSION_COST_MULTIPLIER;
		USpellData *Heal = NewObject<USpellData>(GetTransientPackage());
		Heal->Element = ESpellElement::Water;
		TestEqual(TEXT("no damage, no buildup: normal"), UActionExecutor::GetIceInfusionCostMultiplier(IceTestSpell(Heal, true), ESpellElement::Water), 1.0f);
		USpellData *Chill = NewObject<USpellData>(GetTransientPackage());
		Chill->Element = ESpellElement::Water;
		Chill->StatusBuildup = ICE_TEST_BUILDUP;
		TestEqual(TEXT("buildup only: Ice fills the Ice bar, so it pays"), UActionExecutor::GetIceInfusionCostMultiplier(IceTestSpell(Chill, true), ESpellElement::Water), Ice);
	});

	It("scales only the infusion wear add-on by the Ice factor (B-593b)", [this]()
	{
		const float Ice = InfusionConstants::ICE_INFUSION_COST_MULTIPLIER;
		const EItemTier Matched = EItemTier::C_Tier;
		const float MaxDur = static_cast<float>(DurabilityConstants::GetMaxDurabilityForTier(Matched));
		const float InfusionPct = UBreakCalculator::GetInfusionWearPercent(ICE_TEST_L1, true);
		TestTrue(TEXT("an L1 spell infusion wears"), InfusionPct > 0.0f);
		TestEqual(TEXT("matched tier: all of it is the add-on, x the factor"),
				  UBreakCalculator::CalculateGapDurabilityWear(Matched, Matched, Matched, ICE_TEST_L1, true, Ice), FMath::RoundToInt(InfusionPct * Ice * MaxDur));
		TestEqual(TEXT("the default factor leaves wear as it was"), UBreakCalculator::CalculateGapDurabilityWear(Matched, Matched, Matched, ICE_TEST_L1, true),
				  UBreakCalculator::CalculateDurabilityWear(Matched, Matched, ICE_TEST_L1, true));
		const EItemTier Above = EItemTier::A_Tier;
		TestEqual(TEXT("uninfused tier-gap wear is not scaled"), UBreakCalculator::CalculateGapDurabilityWear(Matched, Matched, Above, ICE_TEST_L0, true, Ice),
				  UBreakCalculator::CalculateDurabilityWear(Matched, Above, ICE_TEST_L0, true));
	});

	It("carries the Ice pick through a co-op pick request", [this]()
	{
		FAction Action = IceTestAbility(NewObject<UAbilityData>(GetTransientPackage()), ICE_TEST_L1, true);
		const FAction RoundTrip = FCombatPickRequest::FromAction(nullptr, Action).ToAction();
		TestTrue(TEXT("Ice kept"), RoundTrip.bIceInfusion);
		TestEqual(TEXT("from the same source"), RoundTrip.SelectedSource, EInfusionSourceOption::Innate);
	});

	It("offers a Water Caster's innate as Water and as Ice, and prices the Ice EP higher", [this]()
	{
		FWoRTestWorld TestWorld;
		if (!TestTrue(TEXT("Test world created"), TestWorld.Create())) return;
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Caster;
		Data->InnateElement = ESpellElement::Water;
		AActor *Caster = TestWorld.SpawnCombatant(Data);
		UActionExecutor *Executor = TestWorld.GetSubsystem<UActionExecutor>();
		if (!TestNotNull(TEXT("Caster"), Caster) || !TestNotNull(TEXT("Executor"), Executor)) return;
		if (!TestEqual(TEXT("the innate is Water"), Executor->GetElementForSourceOption(Caster, EInfusionSourceOption::Innate), ESpellElement::Water)) return;

		UInfusionVFXComponent *VFX = Caster->FindComponentByClass<UInfusionVFXComponent>();
		if (!VFX)
		{
			VFX = FWoRTestWorld::AddComponent<UInfusionVFXComponent>(Caster);
		}
		if (!TestNotNull(TEXT("VFX component"), VFX)) return;
		VFX->CacheAvailableSources();
		const TArray<EInfusionSourceOption> &Sources = VFX->GetCachedSources();
		const int32 Innate = Sources.IndexOfByKey(EInfusionSourceOption::Innate);
		if (!TestTrue(TEXT("Innate offered"), Innate != INDEX_NONE)) return;
		TestFalse(TEXT("first as Water"), VFX->IsSourceIce(Innate));
		TestTrue(TEXT("then as Ice, from the same source"), Sources.IsValidIndex(Innate + 1) && Sources[Innate + 1] == EInfusionSourceOption::Innate &&
															  VFX->IsSourceIce(Innate + 1));
		TestFalse(TEXT("Raw has no Ice entry"), VFX->IsSourceIce(Sources.IndexOfByKey(EInfusionSourceOption::Raw)));
		VFX->RestrictCachedSources({EInfusionSourceOption::Innate});
		TestTrue(TEXT("narrowed to the innate, its Ice entry stays"), VFX->GetCachedSources().Num() == 2 && VFX->IsSourceIce(1));
		VFX->DropIceEntries(); // B-593b: a Deals Ice spell's picker
		TestTrue(TEXT("a Deals Ice spell lists the innate once, with no Ice entry"),
				 VFX->GetCachedSources().Num() == 1 && VFX->GetCachedSources()[0] == EInfusionSourceOption::Innate && !VFX->HasIceEntry());

		UAbilityData *Ability = NewObject<UAbilityData>(GetTransientPackage());
		Ability->BaseEnergyCost = ICE_TEST_BASE_EP;
		FAction Water = IceTestAbility(Ability, ICE_TEST_L1, false);
		Water.Targets.Add(Caster);
		FAction Ice = Water;
		Ice.bIceInfusion = true;
		const int32 WaterEP = Executor->CalculateActionEnergyCost(Caster, Water);
		const int32 IceEP = Executor->CalculateActionEnergyCost(Caster, Ice);
		TestTrue(FString::Printf(TEXT("Ice EP %d is Water's %d x the multiplier"), IceEP, WaterEP),
				 FMath::Abs(IceEP - WaterEP * InfusionConstants::ICE_INFUSION_COST_MULTIPLIER) <= ICE_TEST_ROUNDING && IceEP > WaterEP);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
