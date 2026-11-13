// FieldEvolutionSpec.cpp
// B-214h (FieldAbilities choice 2; Kingdom Hearts 2's forms): the evolution on the character's own slot gives an
// always-on sprint bonus and augments the field skill (cheaper EP, flight limits it lifts); a broken one gives nothing.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Field/FieldMovementComponent.h"
#include "Field/FieldSkillTypes.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"

namespace FieldEvolutionSpecValues
{
	constexpr float SPRINT_BONUS_PERCENT = 20.0f;
	constexpr float EP_DISCOUNT_PERCENT = 50.0f;
	constexpr int32 DURABILITY = 10;
	constexpr int32 BASE_COST = 4;
	constexpr float TOLERANCE = 0.001f;
	constexpr float EXTRA_CEILING = 200.0f;
}

BEGIN_DEFINE_SPEC(FFieldEvolutionSpec, "WoR.Field.Evolution",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UFieldMovementComponent *Field = nullptr;
	FCombatLoadout *Active = nullptr;

	/** A character with a loadout and field movement; Active is its active loadout. */
	bool SpawnWithLoadout()
	{
		ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(FWoRTestWorld::NewCharacterData());
		UInventoryComponent *Inventory = Loadout ? Loadout->GetOwner()->FindComponentByClass<UInventoryComponent>() : nullptr;
		if (!Inventory || !Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex))
		{
			return false;
		}
		Active = &Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		Field = FWoRTestWorld::AddComponent<UFieldMovementComponent>(Loadout->GetOwner());
		return Field != nullptr;
	}

	/** Seats an evolution with these field values on the character's own slot. */
	UEvolutionItemData *SeatEvolution(float SprintBonus, float EPDiscount, int32 Durability)
	{
		UEvolutionItemData *Evolution = NewObject<UEvolutionItemData>(GetTransientPackage());
		Evolution->FieldSprintBonusPercent = SprintBonus;
		Evolution->FieldSkillEPDiscountPercent = EPDiscount;
		Active->PrimarySlotType = EPrimarySlotType::Evolution;
		Active->PrimaryEvolution = FEvolutionAttachment();
		Active->PrimaryEvolution.Item = Evolution;
		Active->PrimaryEvolution.CurrentDurability = Durability;
		return Evolution;
	}
END_DEFINE_SPEC(FFieldEvolutionSpec)

void FFieldEvolutionSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
	});

	AfterEach([this]()
	{
		Field = nullptr;
		Active = nullptr;
		TestWorld.Reset();
	});

	Describe("the rules", [this]()
	{
		It("turn a bonus into a multiplier, discount EP to at least 1, and combine flight limits", [this]()
		{
			using namespace FieldEvolutionSpecValues;
			TestEqual(TEXT("+20%"), FieldSkillRules::GetEvolutionSprintMultiplier(SPRINT_BONUS_PERCENT), 1.2f, TOLERANCE);
			TestEqual(TEXT("none"), FieldSkillRules::GetEvolutionSprintMultiplier(0.0f), FieldSkillRules::NO_SPRINT_BOOST, TOLERANCE);
			TestEqual(TEXT("half of 4"), FieldSkillRules::ApplyFieldEPDiscount(BASE_COST, EP_DISCOUNT_PERCENT), BASE_COST / 2);
			TestEqual(TEXT("never below 1"), FieldSkillRules::ApplyFieldEPDiscount(1, EP_DISCOUNT_PERCENT), FieldSkillRules::MIN_FIELD_EP_COST);
			TestEqual(TEXT("free stays free"), FieldSkillRules::ApplyFieldEPDiscount(0, EP_DISCOUNT_PERCENT), 0);
			FFieldFlightLimits Effect;
			Effect.bNoCeiling = true;
			Effect.ExtraCeiling = EXTRA_CEILING;
			FFieldFlightLimits Evolution;
			Evolution.bUnlimitedEP = true;
			Evolution.ExtraCeiling = EXTRA_CEILING;
			const FFieldFlightLimits Both = FieldSkillRules::CombineFlightLimits(Effect, Evolution);
			TestTrue(TEXT("either lifts"), Both.bNoCeiling && Both.bUnlimitedEP && !Both.bIgnoreNoFly);
			TestEqual(TEXT("ceilings add"), Both.ExtraCeiling, 2.0f * EXTRA_CEILING, TOLERANCE);
		});
	});

	Describe("the component", [this]()
	{
		It("reads the seated evolution's bonus and augment, and nothing from a broken one", [this]()
		{
			using namespace FieldEvolutionSpecValues;
			if (!TestTrue(TEXT("spawned"), SpawnWithLoadout()))
			{
				return;
			}
			TestNull(TEXT("no evolution yet"), Field->GetSeatedEvolution());
			TestEqual(TEXT("normal sprint"), Field->GetSprintSpeedMultiplier(), FieldSkillRules::NO_SPRINT_BOOST, TOLERANCE);
			TestEqual(TEXT("full cost"), Field->GetFieldEPCost(BASE_COST), BASE_COST);

			UEvolutionItemData *Evolution = SeatEvolution(SPRINT_BONUS_PERCENT, EP_DISCOUNT_PERCENT, DURABILITY);
			Evolution->FieldFlightLimits.bUnlimitedEP = true;
			TestTrue(TEXT("seated"), Field->GetSeatedEvolution() == Evolution);
			TestEqual(TEXT("an evolved character sprints faster"), Field->GetSprintSpeedMultiplier(),
					  FieldSkillRules::GetEvolutionSprintMultiplier(SPRINT_BONUS_PERCENT), TOLERANCE);
			TestEqual(TEXT("the augment: cheaper field skills"), Field->GetFieldEPCost(BASE_COST), BASE_COST / 2);
			TestTrue(TEXT("the augment: unlimited flight"), Field->GetEffectiveFlightLimits().bUnlimitedEP);

			Active->PrimaryEvolution.CurrentDurability = 0; // breakable by default: worn to 0 is broken
			TestNull(TEXT("broken gives nothing"), Field->GetSeatedEvolution());
			TestEqual(TEXT("normal sprint again"), Field->GetSprintSpeedMultiplier(), FieldSkillRules::NO_SPRINT_BOOST, TOLERANCE);
		});

		It("gives every evolution the starting bonus and augment by default", [this]()
		{
			const UEvolutionItemData *Default = GetDefault<UEvolutionItemData>();
			TestEqual(TEXT("+15% sprint"), Default->FieldSprintBonusPercent, FieldSkillRules::EVOLUTION_SPRINT_BONUS_PERCENT, FieldEvolutionSpecValues::TOLERANCE);
			TestEqual(TEXT("-25% field EP"), Default->FieldSkillEPDiscountPercent, FieldSkillRules::EVOLUTION_FIELD_EP_DISCOUNT_PERCENT, FieldEvolutionSpecValues::TOLERANCE);
		});
	});
}

#endif
