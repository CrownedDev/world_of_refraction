// BrokenDarknessSpec.cpp
// Broken Darkness numbers (B-287), from docs/Rules/02-elements-and-damage.md and 03-status-bars.md:
// - ED-39: its resistance row is -5 to every element, -15 to Reality, no physical columns.
// - ED-40: a Guard absorbs EP at 5% of the hit's cost, a Perfect Guard at 10% x2 (20%), with no
//   Efficiency invested; EP may overfill MaxEP by 30% (overload); None, Generic and Reality hits
//   absorb nothing.
// - ED-41: a Light or Void cast costs 25% of the spell's base damage x Spell Damage as HP, waived
//   when the caster's world levels exactly match the spell's requirements.
// - SB-06: absorption stacks multiply matching-alignment buildup x1 / x1 / x2 / x4 at 0-3 stacks;
//   a forbidden cast puts 25% of the spell's base damage as buildup on its own bar.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Defense/DefenseSystem.h"
#include "Combat/Defense/EDefenseType.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"
#include "Combat/Resistance/ClassInnateResistanceTable.h"
#include "Skills/Definitions/EScalingTier.h"
#include "Skills/Definitions/WorldStatRequirements.h"
#include "Skills/Effects/BuildupMath.h"
#include "Skills/Effects/StatusBuildupManager.h"

namespace
{
	// The rules' numbers, written out so a changed value fails here.
	constexpr float B287_ED39_ELEMENT_RESISTANCE = -5.0f;
	constexpr float B287_ED39_REALITY_RESISTANCE = -15.0f;
	constexpr float B287_ED39_PHYSICAL_RESISTANCE = 0.0f;
	constexpr float B287_ED40_GUARD_RATE = 0.05f;
	constexpr float B287_ED40_PERFECT_GUARD_RATE = 0.10f;
	constexpr float B287_ED40_PERFECT_MULTIPLIER = 2.0f;
	constexpr float B287_ED40_OVERLOAD_FRACTION = 0.30f;
	constexpr float B287_ED41_SELF_DAMAGE_FRACTION = 0.25f;
	constexpr float B287_SB06_SELF_BUILDUP_FRACTION = 0.25f;
	constexpr float B287_SB06_STACK_MULTIPLIERS[] = {1.0f, 1.0f, 2.0f, 4.0f};
	constexpr float B287_NO_STACK_MULTIPLIER = 1.0f;

	/** A hit's EP cost large enough that 5% of it is a whole number of EP. */
	constexpr float B287_HIT_EP_COST = 100.0f;
	/** A hit's EP cost so large that one Guard fills the overload window. */
	constexpr float B287_FLOODING_HIT_EP_COST = 100000.0f;
	/** A forbidden spell's base damage: small enough that its cost neither kills nor caps the bar. */
	constexpr float B287_FORBIDDEN_SPELL_BASE_DAMAGE = 40.0f;
	/** Efficiency's absorption scaling fraction for a character with nothing invested. */
	constexpr float B287_NO_EFFICIENCY = 0.0f;
	constexpr int32 B287_REQUIRED_MIND = 2;
	constexpr int32 B287_REQUIRED_BODY = 1;
	constexpr int32 B287_REQUIRED_SPIRIT = 3;
}

BEGIN_DEFINE_SPEC(FBrokenDarknessSpec, "WoR.Combat.BrokenDarkness",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	AActor *BDActor = nullptr;
	UCharacterDataComponent *BDChar = nullptr;
	UBrokenDarknessManager *BD = nullptr;

	/** A born Broken Darkness caster with nothing invested, flipped and on its base Darkness pool. */
	void SpawnBrokenDarkness()
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Caster;
		Data->InnateElement = ESpellElement::Darkness;
		Data->bBrokenDarknessInnate = true;
		BDActor = TestWorld->SpawnCombatant(Data);
		BDChar = BDActor ? BDActor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (!BDChar)
		{
			return;
		}
		BD = FWoRTestWorld::AddComponent<UBrokenDarknessManager>(BDActor);
		BD->InitializeBornBrokenDarkness();
	}

	/** The holder guards a successful hit of Element costing EPCost. */
	void Guard(ESpellElement Element, bool bPerfect, float EPCost)
	{
		FDefenseResult Result;
		Result.bSuccess = true;
		Result.bWasInWindow = true;
		BD->OnDefenseResolved(EDefenseType::Block, Result, Element, EPCost, bPerfect);
	}

	/** EP gained by one Guard of a Fire hit costing B287_HIT_EP_COST, from empty. */
	int32 EPFromOneGuard(bool bPerfect)
	{
		BDChar->CurrentEP = 0;
		Guard(ESpellElement::Fire, bPerfect, B287_HIT_EP_COST);
		return BDChar->CurrentEP;
	}
END_DEFINE_SPEC(FBrokenDarknessSpec)

void FBrokenDarknessSpec::Define()
{
	Describe("The resistance row (ED-39)", [this]()
	{
		It("is -5 to every element, -15 to Reality and 0 on the physical columns", [this]()
		{
			const ClassInnateResistanceTable::FResistanceRow Row =
				ClassInnateResistanceTable::ResolveRow(ECharacterClass::Caster, ESpellElement::Darkness, true);
			const ESpellElement Elements[] = {ESpellElement::Fire, ESpellElement::Water, ESpellElement::Earth,
											  ESpellElement::Wind, ESpellElement::Light, ESpellElement::Darkness,
											  ESpellElement::Lightning, ESpellElement::Void};
			for (const ESpellElement Element : Elements)
			{
				TestEqual(*FString::Printf(TEXT("%s column"), *UEnum::GetValueAsString(Element)),
						  ClassInnateResistanceTable::GetElementColumn(Row, Element), B287_ED39_ELEMENT_RESISTANCE);
			}
			TestEqual(TEXT("Reality column"), ClassInnateResistanceTable::GetElementColumn(Row, ESpellElement::Reality),
					  B287_ED39_REALITY_RESISTANCE);
			const EPhysicalDamageType PhysicalTypes[] = {EPhysicalDamageType::Slash, EPhysicalDamageType::Pierce,
														 EPhysicalDamageType::Impact};
			for (const EPhysicalDamageType PhysicalType : PhysicalTypes)
			{
				TestEqual(*FString::Printf(TEXT("%s column"), *UEnum::GetValueAsString(PhysicalType)),
						  ClassInnateResistanceTable::GetPhysicalColumn(Row, PhysicalType), B287_ED39_PHYSICAL_RESISTANCE);
			}
		});
	});

	Describe("A Broken Darkness character", [this]()
	{
		BeforeEach([this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			TestTrue(TEXT("Test world created"), TestWorld->Create());
			SpawnBrokenDarkness();
			TestTrue(TEXT("Born Broken Darkness"), BD && BDChar && BDChar->IsBrokenDarkness() && BD->IsTransformed());
			TestTrue(TEXT("Has an EP pool"), BDChar && BDChar->MaxEP > 0);
		});

		AfterEach([this]()
		{
			TestWorld.Reset();
			BDActor = nullptr;
			BDChar = nullptr;
			BD = nullptr;
		});

		It("absorbs 5% of a guarded hit's cost, 20% on a Perfect Guard (ED-40)", [this]()
		{
			if (!BD || !BDChar) return;
			const float EfficiencyFactor = GetScalingFraction(ESubStat::Efficiency, BDChar->GetEffectiveEfficiencyMultiplier());
			if (!TestEqual(TEXT("No Efficiency invested"), EfficiencyFactor, B287_NO_EFFICIENCY)) return;

			TestEqual(TEXT("Guard: 5% of the cost"), EPFromOneGuard(false),
					  FMath::RoundToInt(B287_HIT_EP_COST * B287_ED40_GUARD_RATE));
			TestEqual(TEXT("Perfect Guard: 10% x2 of the cost"), EPFromOneGuard(true),
					  FMath::RoundToInt(B287_HIT_EP_COST * B287_ED40_PERFECT_GUARD_RATE * B287_ED40_PERFECT_MULTIPLIER));
		});

		It("absorbs nothing from None, Generic or Reality hits (ED-40)", [this]()
		{
			if (!BD || !BDChar) return;
			const int32 StartEP = BDChar->MaxEP / 2;
			const ESpellElement Unabsorbable[] = {ESpellElement::None, ESpellElement::Generic};
			for (const ESpellElement Element : Unabsorbable)
			{
				BDChar->CurrentEP = StartEP;
				Guard(Element, true, B287_HIT_EP_COST);
				TestEqual(*FString::Printf(TEXT("%s: no EP gained"), *UEnum::GetValueAsString(Element)), BDChar->CurrentEP, StartEP);
			}
			// A guarded Reality hit drains instead (the cleanse), so it never gains.
			BDChar->CurrentEP = StartEP;
			Guard(ESpellElement::Reality, true, B287_HIT_EP_COST);
			TestTrue(*FString::Printf(TEXT("Reality: no EP gained (%d -> %d)"), StartEP, BDChar->CurrentEP), BDChar->CurrentEP <= StartEP);
		});

		It("overfills EP by at most 30% of MaxEP and is then overloaded (ED-40)", [this]()
		{
			if (!BD || !BDChar) return;
			TestEqual(TEXT("Overload window is 30% of MaxEP"), BD->GetOverloadCapacity(), BDChar->MaxEP * B287_ED40_OVERLOAD_FRACTION);

			BDChar->CurrentEP = BDChar->MaxEP;
			Guard(ESpellElement::Fire, false, B287_FLOODING_HIT_EP_COST);
			TestEqual(TEXT("EP stops at MaxEP + 30%"), BDChar->CurrentEP,
					  BDChar->MaxEP + FMath::RoundToInt(BDChar->MaxEP * B287_ED40_OVERLOAD_FRACTION));
			TestTrue(TEXT("Overloaded"), BD->IsOverloaded());
		});

		It("pays 25% of a Light spell's base damage x Spell Damage as HP (ED-41)", [this]()
		{
			if (!BD || !BDChar) return;
			const int32 ExpectedDamage =
				FMath::RoundToInt(B287_FORBIDDEN_SPELL_BASE_DAMAGE * B287_ED41_SELF_DAMAGE_FRACTION * BDChar->GetEffectiveSpellDamage());
			if (!TestTrue(TEXT("The cost does not kill"), ExpectedDamage < BDChar->CurrentHP)) return;
			const int32 HPBefore = BDChar->CurrentHP;

			TestTrue(TEXT("Light is forbidden"), BD->ProcessForbiddenCast(ESpellElement::Light, B287_FORBIDDEN_SPELL_BASE_DAMAGE));
			TestEqual(TEXT("HP cost"), HPBefore - BDChar->CurrentHP, ExpectedDamage);
		});

		It("pays for Void too, and nothing for an allowed element (ED-41)", [this]()
		{
			if (!BD || !BDChar) return;
			const int32 HPBefore = BDChar->CurrentHP;
			TestFalse(TEXT("Fire is not forbidden"), BD->ProcessForbiddenCast(ESpellElement::Fire, B287_FORBIDDEN_SPELL_BASE_DAMAGE));
			TestEqual(TEXT("No HP cost for Fire"), BDChar->CurrentHP, HPBefore);

			TestTrue(TEXT("Void is forbidden"), BD->ProcessForbiddenCast(ESpellElement::Void, B287_FORBIDDEN_SPELL_BASE_DAMAGE));
			TestTrue(TEXT("Void costs HP"), BDChar->CurrentHP < HPBefore);
		});

		It("puts 25% of a forbidden spell's base damage on its own bar (SB-06)", [this]()
		{
			if (!BD || !BDChar) return;
			UStatusBuildupManager *Buildup = TestWorld->GetSubsystem<UStatusBuildupManager>();
			if (!TestNotNull(TEXT("StatusBuildupManager"), Buildup)) return;
			const float ExpectedBuildup = WoRBuildupMath::Compute(Buildup->GatherBuildupMathInputs(
				BDActor, BDActor, B287_FORBIDDEN_SPELL_BASE_DAMAGE * B287_SB06_SELF_BUILDUP_FRACTION, ESpellElement::Light,
				EPhysicalDamageType::None, false));
			const float BarBefore = Buildup->GetStatusBarBuildup(BDActor);

			BD->ProcessForbiddenCast(ESpellElement::Light, B287_FORBIDDEN_SPELL_BASE_DAMAGE);
			TestEqual(TEXT("Self-buildup in Light"), Buildup->GetStatusBarBuildup(BDActor) - BarBefore, ExpectedBuildup);
			TestTrue(TEXT("Pending element is Light"), Buildup->GetPendingElement(BDActor) == ESpellElement::Light);
		});

		It("multiplies matching buildup x1 / x1 / x2 / x4 at 0-3 stacks (SB-06)", [this]()
		{
			if (!BD || !BDChar) return;
			UStatusBuildupManager *Buildup = TestWorld->GetSubsystem<UStatusBuildupManager>();
			if (!TestNotNull(TEXT("StatusBuildupManager"), Buildup)) return;
			AActor *Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());

			for (int32 Stacks = 0; Stacks < static_cast<int32>(UE_ARRAY_COUNT(B287_SB06_STACK_MULTIPLIERS)); ++Stacks)
			{
				Guard(ESpellElement::Fire, false, B287_HIT_EP_COST);
				if (!TestEqual(TEXT("Stacks after this absorption"), BD->GetCurrentStackCount(), Stacks)) return;
				TestEqual(*FString::Printf(TEXT("Stack multiplier at %d stacks"), Stacks), BD->GetStackStatusMultiplier(),
						  B287_SB06_STACK_MULTIPLIERS[Stacks]);
				const FBuildupMathInputs Matching =
					Buildup->GatherBuildupMathInputs(BDActor, Foe, B287_HIT_EP_COST, ESpellElement::Fire, EPhysicalDamageType::None, false);
				TestEqual(*FString::Printf(TEXT("A Fire hit's buildup at %d stacks"), Stacks), Matching.StackMultiplier,
						  B287_SB06_STACK_MULTIPLIERS[Stacks]);
				const FBuildupMathInputs Other =
					Buildup->GatherBuildupMathInputs(BDActor, Foe, B287_HIT_EP_COST, ESpellElement::Water, EPhysicalDamageType::None, false);
				TestEqual(*FString::Printf(TEXT("A Water hit's buildup at %d stacks"), Stacks), Other.StackMultiplier,
						  B287_NO_STACK_MULTIPLIER);
			}
		});
	});

	Describe("The forbidden-cast waiver (ED-41)", [this]()
	{
		It("applies only when the world levels exactly match the spell's requirements", [this]()
		{
			FWorldStatRequirements Requirements;
			Requirements.RequiredWorldMind = B287_REQUIRED_MIND;
			Requirements.RequiredWorldBody = B287_REQUIRED_BODY;
			Requirements.RequiredWorldSpirit = B287_REQUIRED_SPIRIT;

			FWorldPillarLevels Levels;
			Levels.Mind = B287_REQUIRED_MIND;
			Levels.Body = B287_REQUIRED_BODY;
			Levels.Spirit = B287_REQUIRED_SPIRIT;
			TestTrue(TEXT("Exact match waives"), Requirements.ExactlyMatchedBy(Levels));

			FWorldPillarLevels Above = Levels;
			++Above.Mind;
			TestFalse(TEXT("Above on one pillar pays"), Requirements.ExactlyMatchedBy(Above));

			FWorldPillarLevels Below = Levels;
			--Below.Spirit;
			TestFalse(TEXT("Below on one pillar pays"), Requirements.ExactlyMatchedBy(Below));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
