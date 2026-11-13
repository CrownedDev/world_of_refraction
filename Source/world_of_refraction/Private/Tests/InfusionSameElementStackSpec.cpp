// InfusionSameElementStackSpec.cpp
// B-435 (D-174, ED-51): an infused action uses every one of the character's sources that shares its element.
// Status buildup grows +25% per extra source, capped at +50%; each source pays its own cost once (a crystal
// source pays wear, not EP; a stacked innate pays its EP); a different-element or broken source isn't used. The stack
// is fixed at commit. B-435b (Q-435 B): spells stack too, on their origin's element at any charge level; a BD / Reality
// caster may pick their own element instead.
// Expectations are literals, so a changed number fails here on purpose.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/Rings/RingData.h"
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

	/** B-435b: a Fire spell costing STACK_TEST_BASE_EP. */
	static USpellData *FireSpell()
	{
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		Spell->Element = ESpellElement::Fire;
		Spell->BaseEnergyCost = STACK_TEST_BASE_EP;
		return Spell;
	}

	FAction SpellAction(USpellData *Spell, ESpellSource Origin, EInfusionSourceOption Source, int32 Level) const
	{
		FAction Action;
		Action.ActionType = EActionType::Spell;
		Action.SpellData = Spell;
		Action.SpellSource = Origin;
		Action.SelectedSource = Source;
		Action.SpellInfusionLevel = Level;
		Action.Targets.Add(Caster);
		return Action;
	}

	/** The primary slot holds a ring with Gem and Spell (a Caster's adapter). */
	void SetPrimaryRing(const FCrystalId &Gem, USpellData *Spell)
	{
		Active().PrimarySlotType = EPrimarySlotType::Ring;
		Active().PrimaryRing.RingEntry.Ring = NewObject<URingData>(GetTransientPackage());
		Active().PrimaryRing.RingEntry.AttachedItem = StackTestGem(Gem, STACK_TEST_INTACT);
		Active().PrimaryRing.RingEntry.AssignedSpells = {Spell};
	}

	/** The primary slot holds an evolution of CrystalType (its element). */
	void SeatEvolution(ECrystalType CrystalType)
	{
		UEvolutionItemData *Evolution = NewObject<UEvolutionItemData>(GetTransientPackage());
		Evolution->CrystalType = CrystalType;
		Active().PrimarySlotType = EPrimarySlotType::Evolution;
		Active().PrimaryEvolution = FEvolutionAttachment();
		Active().PrimaryEvolution.Item = Evolution;
		Active().PrimaryEvolution.CurrentDurability = STACK_TEST_INTACT;
	}

	void SetInnateElement(ESpellElement Element) const
	{
		Caster->FindComponentByClass<UCharacterDataComponent>()->CharacterData->InnateElement = Element;
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

	It("stacks an innate spell with a same-element gem at any charge level; the gem adds no EP (B-435b)", [this]()
	{
		if (!Executor) return;
		USpellData *Spell = FireSpell();
		Active().InnateSpells.Add(Spell);
		const FAction Cast = SpellAction(Spell, ESpellSource::Innate, EInfusionSourceOption::Innate, STACK_TEST_L1);
		const int32 InnateEP = Executor->CalculateActionEnergyCost(Caster, Cast);
		TestTrue(TEXT("the innate spell pays EP"), InnateEP > 0);
		TestEqual(TEXT("alone: one source"), Executor->GetSameElementSourceCount(Caster, Cast), 1);
		SetWeaponGem(STACK_TEST_FIRE_GEM, STACK_TEST_INTACT);
		TestEqual(TEXT("with a Fire gem: two"), Executor->GetSameElementSourceCount(Caster, Cast), 2);
		TestEqual(TEXT("uninfused too"), Executor->GetSameElementSourceCount(
			Caster, SpellAction(Spell, ESpellSource::Innate, EInfusionSourceOption::None, 0)), 2);
		TestEqual(TEXT("the gem adds no EP"), Executor->CalculateActionEnergyCost(Caster, Cast), InnateEP);
	});

	It("makes a ring Fire spell with a Fire innate pay the innate's EP for x1.25 buildup; a Water innate doesn't join", [this]()
	{
		if (!Executor) return;
		USpellData *Spell = FireSpell();
		SetPrimaryRing(STACK_TEST_FIRE_GEM, Spell);
		const FAction Cast = SpellAction(Spell, ESpellSource::RingCrystal, EInfusionSourceOption::PrimaryRing, STACK_TEST_L1);
		const int32 Count = Executor->GetSameElementSourceCount(Caster, Cast);
		TestEqual(TEXT("ring + innate"), Count, 2);
		TestEqual(TEXT("x1.25 buildup"), InfusionConstants::SameElementBuildupMultiplier(Count), STACK_TEST_MULT_TWO, STACK_TEST_TOLERANCE);
		TestTrue(TEXT("the free ring spell now pays the innate's EP"), Executor->CalculateActionEnergyCost(Caster, Cast) > 0);
		SetInnateElement(ESpellElement::Water);
		TestEqual(TEXT("a Water innate doesn't join"), Executor->GetSameElementSourceCount(Caster, Cast), 1);
		TestEqual(TEXT("so the ring spell is free again"), Executor->CalculateActionEnergyCost(Caster, Cast), 0);
	});

	It("lets a Reality caster stack on their own element or the spell's source element", [this]()
	{
		if (!Executor) return;
		SetInnateElement(ESpellElement::Reality);
		USpellData *Spell = FireSpell();
		SetPrimaryRing(STACK_TEST_FIRE_GEM, Spell);
		TestTrue(TEXT("Innate is allowed on a ring spell"),
				 Executor->GetAllowedInfusionSourcesForSpell(Caster, Spell).Contains(EInfusionSourceOption::Innate));
		const FAction Own = SpellAction(Spell, ESpellSource::RingCrystal, EInfusionSourceOption::Innate, STACK_TEST_L1);
		const TArray<EInfusionSourceOption> OwnStack = Executor->GetSameElementInfusionSources(Caster, Own);
		TestTrue(TEXT("own element: the innate alone"), OwnStack.Num() == 1 && OwnStack[0] == EInfusionSourceOption::Innate);
		TestTrue(TEXT("own element pays the innate's EP"), Executor->CalculateActionEnergyCost(Caster, Own) > 0);
		const FAction Source = SpellAction(Spell, ESpellSource::RingCrystal, EInfusionSourceOption::PrimaryRing, STACK_TEST_L1);
		const TArray<EInfusionSourceOption> SourceStack = Executor->GetSameElementInfusionSources(Caster, Source);
		TestTrue(TEXT("source element: the ring alone"), SourceStack.Num() == 1 && SourceStack[0] == EInfusionSourceOption::PrimaryRing);
		TestEqual(TEXT("and free"), Executor->CalculateActionEnergyCost(Caster, Source), 0);
	});

	It("sums the L2 HP of each HP-paying source on a spell (innate + a same-element evolution)", [this]()
	{
		if (!Executor) return;
		UCharacterDataComponent *Stats = Caster->FindComponentByClass<UCharacterDataComponent>();
		if (!TestNotNull(TEXT("Stats"), Stats)) return;
		Stats->MaxEP = STACK_TEST_MAX_EP;
		Stats->MaxHP = STACK_TEST_MAX_HP;
		USpellData *Spell = FireSpell();
		Active().InnateSpells.Add(Spell);
		const FAction Cast = SpellAction(Spell, ESpellSource::Innate, EInfusionSourceOption::Innate, STACK_TEST_L2);
		SeatEvolution(ECrystalType::Sapphire); // Water: doesn't join
		const int32 OneSource = CommitHPCost(Cast);
		TestTrue(TEXT("the innate spell pays HP at L2"), OneSource > 0);
		SeatEvolution(ECrystalType::Garnet); // Fire: joins and pays its own HP
		TestEqual(TEXT("two HP-paying sources"), Executor->CountHPPayingInfusionSources(Caster, Cast), 2);
		TestEqual(TEXT("the HP backlash doubles"), CommitHPCost(Cast), 2 * OneSource);
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
