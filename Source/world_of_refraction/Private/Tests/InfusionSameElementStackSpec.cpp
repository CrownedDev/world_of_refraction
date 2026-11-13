// InfusionSameElementStackSpec.cpp
// B-435 (D-174, ED-51): an infused action uses every one of the character's sources that shares its element.
// Status buildup grows +25% per extra source, capped at +50%; each source pays its own cost once (a crystal
// source pays wear, not EP; a stacked innate pays its EP); a different-element or broken source isn't used. Only
// abilities and attacks stack (Q-435: a spell is origin-bound to one source). The stack is fixed at commit.
// Expectations are literals, so a changed number fails here on purpose.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Infusion/InfusionConstants.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"

namespace
{
	const FCrystalId STACK_TEST_FIRE_GEM(ECrystalType::Garnet, EItemTier::F_Tier);
	const FCrystalId STACK_TEST_WATER_GEM(ECrystalType::Sapphire, EItemTier::F_Tier);
	constexpr int32 STACK_TEST_INTACT = 5;
	constexpr int32 STACK_TEST_BROKEN = 0;
	constexpr int32 STACK_TEST_L1 = 1;
	constexpr int32 STACK_TEST_L2 = 2;
	constexpr int32 STACK_TEST_BASE_EP = 100;
	/** D-174: 1 / 2 / 3 / 4 matching sources give buildup x1.0 / x1.25 / x1.5 / x1.5. */
	constexpr float STACK_TEST_MULT_ONE = 1.0f;
	constexpr float STACK_TEST_MULT_TWO = 1.25f;
	constexpr float STACK_TEST_MULT_THREE = 1.5f;
	constexpr float STACK_TEST_MULT_FOUR = 1.5f;
	constexpr float STACK_TEST_TOLERANCE = 0.001f;
	/** Non-zero pools so an HP backlash, if one were charged, would show. */
	constexpr int32 STACK_TEST_MAX_EP = 200;
	constexpr int32 STACK_TEST_MAX_HP = 400;

	FRuntimeAttachedItem StackTestGem(const FCrystalId &Gem, int32 Durability)
	{
		FRuntimeAttachedItem Attached;
		Attached.Kind = EAttachedItemKind::Crystal;
		Attached.Crystal = FCrystalAttachment(Gem, Durability);
		return Attached;
	}
}

BEGIN_DEFINE_SPEC(FInfusionSameElementStackSpec, "WoR.Infusion.SameElementStack",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UActionExecutor *Executor = nullptr;
	ULoadoutComponent *Loadout = nullptr;
	AActor *Caster = nullptr;
	UAbilityData *Ability = nullptr;

	FCombatLoadout &Active() const
	{
		UInventoryComponent *Inventory = Loadout->GetOwner()->FindComponentByClass<UInventoryComponent>();
		return Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
	}

	void SetWeaponGem(const FCrystalId &Gem, int32 Durability)
	{
		Active().PrimaryWeapon.WeaponEntry.AttachedItem = StackTestGem(Gem, Durability);
	}

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

	int32 CommitHPCost(const FAction &Action)
	{
		Executor->CurrentExecutionContext.Emplace();
		Executor->ApplyCommitCosts(Caster, Action);
		const int32 Cost = Executor->CurrentExecutionContext->PendingInfusionHPCost;
		Executor->CurrentExecutionContext.Reset();
		return Cost;
	}
END_DEFINE_SPEC(FInfusionSameElementStackSpec)

void FInfusionSameElementStackSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Executor = TestWorld->GetSubsystem<UActionExecutor>();
		// A Fire Caster holding a weapon: innate Fire is one source, the weapon's gem may be a second.
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Caster;
		Data->InnateElement = ItemIdentity::GetElement(STACK_TEST_FIRE_GEM);
		Loadout = TestWorld->SpawnCombatantWithLoadout(Data, NewObject<UWeaponData>(GetTransientPackage()));
		Caster = Loadout ? Loadout->GetOwner() : nullptr;
		Ability = NewObject<UAbilityData>(GetTransientPackage());
		Ability->BaseEnergyCost = STACK_TEST_BASE_EP;
		if (!TestNotNull(TEXT("ActionExecutor"), Executor) || !TestNotNull(TEXT("Loadout"), Loadout))
		{
			Executor = nullptr; // each case returns early
		}
	});

	AfterEach([this]()
	{
		Executor = nullptr;
		Loadout = nullptr;
		Caster = nullptr;
		Ability = nullptr;
		TestWorld.Reset();
	});

	It("gives buildup x1.0 / x1.25 / x1.5 / x1.5 for 1 / 2 / 3 / 4 matching sources", [this]()
	{
		TestNearlyEqual(TEXT("1 source"), InfusionConstants::SameElementBuildupMultiplier(1), STACK_TEST_MULT_ONE, STACK_TEST_TOLERANCE);
		TestNearlyEqual(TEXT("2 sources"), InfusionConstants::SameElementBuildupMultiplier(2), STACK_TEST_MULT_TWO, STACK_TEST_TOLERANCE);
		TestNearlyEqual(TEXT("3 sources"), InfusionConstants::SameElementBuildupMultiplier(3), STACK_TEST_MULT_THREE, STACK_TEST_TOLERANCE);
		TestNearlyEqual(TEXT("4 sources: capped"), InfusionConstants::SameElementBuildupMultiplier(4), STACK_TEST_MULT_FOUR, STACK_TEST_TOLERANCE);
		TestNearlyEqual(TEXT("No source"), InfusionConstants::SameElementBuildupMultiplier(0), STACK_TEST_MULT_ONE, STACK_TEST_TOLERANCE);
	});

	It("stacks a same-element weapon gem with the innate element, and not a different or broken one", [this]()
	{
		if (!Executor) return;
		const FAction Innate = AbilityAction(EInfusionSourceOption::Innate, STACK_TEST_L1);
		TestEqual(TEXT("Innate alone"), Executor->GetSameElementSourceCount(Caster, Innate), 1);

		SetWeaponGem(STACK_TEST_FIRE_GEM, STACK_TEST_INTACT);
		TestEqual(TEXT("Innate + Fire gem"), Executor->GetSameElementSourceCount(Caster, Innate), 2);
		TestEqual(TEXT("Picked from the gem side, the same pair"),
				  Executor->GetSameElementSourceCount(Caster, AbilityAction(EInfusionSourceOption::WeaponCrystal, STACK_TEST_L1)), 2);

		SetWeaponGem(STACK_TEST_WATER_GEM, STACK_TEST_INTACT);
		TestEqual(TEXT("A Water gem doesn't stack with Fire"), Executor->GetSameElementSourceCount(Caster, Innate), 1);

		SetWeaponGem(STACK_TEST_FIRE_GEM, STACK_TEST_BROKEN);
		TestEqual(TEXT("A broken gem doesn't stack (CL-24)"), Executor->GetSameElementSourceCount(Caster, Innate), 1);
	});

	It("stacks nothing for an uninfused action or a Raw infusion", [this]()
	{
		if (!Executor) return;
		SetWeaponGem(STACK_TEST_FIRE_GEM, STACK_TEST_INTACT);
		TestEqual(TEXT("Uninfused"), Executor->GetSameElementSourceCount(Caster, AbilityAction(EInfusionSourceOption::Innate, 0)), 0);
		TestEqual(TEXT("Raw"), Executor->GetSameElementSourceCount(Caster, AbilityAction(EInfusionSourceOption::Raw, STACK_TEST_L1)), 0);
	});

	It("doesn't stack a spell, which is origin-bound to one source (Q-435)", [this]()
	{
		if (!Executor) return;
		SetWeaponGem(STACK_TEST_FIRE_GEM, STACK_TEST_INTACT);
		FAction Spell;
		Spell.ActionType = EActionType::Spell;
		Spell.SpellData = NewObject<USpellData>(GetTransientPackage());
		Spell.SpellSource = ESpellSource::Innate;
		Spell.SelectedSource = EInfusionSourceOption::Innate;
		Spell.SpellInfusionLevel = STACK_TEST_L1;
		Spell.Targets.Add(Caster);
		TestEqual(TEXT("An innate spell beside a Fire gem"), Executor->GetSameElementSourceCount(Caster, Spell), 0);
	});

	It("keeps the stack resolved at commit when the commit's wear breaks the gem", [this]()
	{
		if (!Executor) return;
		SetWeaponGem(STACK_TEST_FIRE_GEM, STACK_TEST_INTACT);
		const FAction Innate = AbilityAction(EInfusionSourceOption::Innate, STACK_TEST_L1);
		Executor->CurrentExecutionContext.Emplace();
		Executor->CurrentExecutionContext->Executor = Caster;
		Executor->CurrentExecutionContext->Action = Innate;
		Executor->ApplyCommitCosts(Caster, Innate);
		SetWeaponGem(STACK_TEST_FIRE_GEM, STACK_TEST_BROKEN); // as if the commit wore it to 0
		TestEqual(TEXT("Still two sources for this action"), Executor->GetSameElementSourceCount(Caster, Innate), 2);
		Executor->CurrentExecutionContext.Reset();
		TestEqual(TEXT("The next action sees the broken gem"), Executor->GetSameElementSourceCount(Caster, Innate), 1);
	});

	It("charges each source's own cost once: the gem pays wear, not EP; a stacked innate pays its EP", [this]()
	{
		if (!Executor) return;
		const FAction Innate = AbilityAction(EInfusionSourceOption::Innate, STACK_TEST_L1);
		const FAction Gem = AbilityAction(EInfusionSourceOption::WeaponCrystal, STACK_TEST_L1);
		// Relative to the innate's own price (its exact value is WoR.Infusion.Cost), so tier-gap tuning can't break this.
		const int32 InnateEP = Executor->CalculateActionEnergyCost(Caster, Innate);
		TestTrue(TEXT("Innate alone pays EP"), InnateEP > 0);

		SetWeaponGem(STACK_TEST_FIRE_GEM, STACK_TEST_INTACT);
		TestEqual(TEXT("Innate + gem: the innate's EP once, the gem adds none"),
				  Executor->CalculateActionEnergyCost(Caster, Innate), InnateEP);
		TestEqual(TEXT("Gem picked + innate stacked: the innate still pays its EP once"),
				  Executor->CalculateActionEnergyCost(Caster, Gem), InnateEP);

		SetWeaponGem(STACK_TEST_WATER_GEM, STACK_TEST_INTACT);
		TestEqual(TEXT("A different-element gem alone costs no EP"), Executor->CalculateActionEnergyCost(Caster, Gem), 0);
	});

	It("adds no HP backlash for a stacked gem or an innate ability (only Raw / innate spell / evolution pay HP)", [this]()
	{
		if (!Executor) return;
		UCharacterDataComponent *Stats = Caster->FindComponentByClass<UCharacterDataComponent>();
		if (!TestNotNull(TEXT("Stats"), Stats)) return;
		Stats->MaxEP = STACK_TEST_MAX_EP;
		Stats->MaxHP = STACK_TEST_MAX_HP;
		SetWeaponGem(STACK_TEST_FIRE_GEM, STACK_TEST_INTACT);
		TestEqual(TEXT("Innate ability L2 + gem: no HP"), CommitHPCost(AbilityAction(EInfusionSourceOption::Innate, STACK_TEST_L2)), 0);
		TestEqual(TEXT("Gem ability L2 + innate ability: no HP"),
				  CommitHPCost(AbilityAction(EInfusionSourceOption::WeaponCrystal, STACK_TEST_L2)), 0);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
