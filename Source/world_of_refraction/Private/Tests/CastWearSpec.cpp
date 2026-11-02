// CastWearSpec.cpp
// B-242 (DRY audit A5): the one crystal wear formula. With character data it is the substat formula over the
// caster's composed stats; without, the base formula. Pins the numbers the three wear paths share.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterDataComponent.h"
#include "Equipment/Crystals/CrystalManager.h"
#include "Equipment/Durability/BreakCalculator.h"

namespace
{
	constexpr int32 CAST_WEAR_TEST_LEVEL = 1;
}

BEGIN_DEFINE_SPEC(FCastWearSpec, "WoR.Crystals.CastWear", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
END_DEFINE_SPEC(FCastWearSpec)

void FCastWearSpec::Define()
{
	AfterEach([this]() { TestWorld.Reset(); });

	It("uses the substat formula over the caster's composed stats", [this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		if (!TestTrue(TEXT("Test world created"), TestWorld->Create())) return;
		AActor *Caster = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		const UCharacterDataComponent *Char = Caster ? Caster->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (!TestNotNull(TEXT("Character data"), Char)) return;
		const FEffectiveStats Stats = Char->GetEffectiveStats();
		for (const bool bIsSpell : {true, false})
		{
			const int32 Expected = UBreakCalculator::CalculateDurabilityWearWithSubstats(
				EItemTier::C_Tier, EItemTier::E_Tier, CAST_WEAR_TEST_LEVEL, bIsSpell, Stats.SpellDamage - 1.0f,
				Stats.StatusMultiplier - 1.0f, 1.0f - Stats.EfficiencyMultiplier, Stats.Resistance);
			TestEqual(bIsSpell ? TEXT("spell") : TEXT("ability"),
					  UCrystalManager::ComputeCastWear(Caster, EItemTier::C_Tier, EItemTier::E_Tier, CAST_WEAR_TEST_LEVEL, bIsSpell), Expected);
		}
	});

	It("falls back to the base formula without a caster", [this]()
	{
		TestEqual(TEXT("base"), UCrystalManager::ComputeCastWear(nullptr, EItemTier::C_Tier, EItemTier::E_Tier, CAST_WEAR_TEST_LEVEL, true),
				  UBreakCalculator::CalculateDurabilityWear(EItemTier::C_Tier, EItemTier::E_Tier, CAST_WEAR_TEST_LEVEL, true));
	});
}

#endif
