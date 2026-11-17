// TrialPoolSpec.cpp
// B-566 (D-225, TrialPoolDesign stage 2): the pool's rules, headless. Unproven here: the authored tags on the real
// items and trials (PIE sheet B-566, wor.Trial.Pool).

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/Encounter/EnemyZone.h"
#include "Equipment/Crystals/CrystalTypeHelpers.h"
#include "Equipment/Apparel/ApparelData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Misc/AutomationTest.h"
#include "Trial/TrialData.h"
#include "Trial/TrialLoot.h"
#include "Trial/TrialPool.h"

namespace TrialPoolSpecValues
{
	constexpr int32 ROLLS = 3000;
	constexpr int32 SEED = 566;
}

BEGIN_DEFINE_SPEC(FTrialPoolSpec, "WoR.Trial.Pool", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FTrialPoolSpec)

void FTrialPoolSpec::Define()
{
	It("opens one more tier per zone, F to E up to F to A", [this]()
	{
		const FTrialTierRange First = TrialPool::GetZoneTierRange(0);
		TestTrue(TEXT("zone 0: F to E"), First.Min == EItemTier::F_Tier && First.Max == EItemTier::E_Tier);
		TestTrue(TEXT("zone 2: F to C"), TrialPool::GetZoneTierRange(2).Max == EItemTier::C_Tier);
		TestTrue(TEXT("zone 4: F to A"), TrialPool::GetZoneTierRange(4).Max == EItemTier::A_Tier);
		TestTrue(TEXT("past the last zone: as zone 4"), TrialPool::GetZoneTierRange(9).Max == EItemTier::A_Tier);
		TestFalse(TEXT("zone 0 holds no D"), First.Contains(EItemTier::D_Tier));
	});

	It("takes the zone's own pool over the trial's", [this]()
	{
		UTrialData *Trial = NewObject<UTrialData>();
		Trial->Pool = FGameplayTag::RequestGameplayTag(TEXT("Source.Zone.Tier1"), /*ErrorIfNotFound=*/false);
		FEncounterZoneInfo Zone;
		TestTrue(TEXT("no zone pool: the trial's"), TrialPool::GetPoolTag(Trial, Zone) == Trial->Pool);
		Zone.LootPool = FGameplayTag::RequestGameplayTag(TEXT("Source.Zone.Tier2"), /*ErrorIfNotFound=*/false);
		TestTrue(TEXT("a zone pool wins"), TrialPool::GetPoolTag(Trial, Zone) == Zone.LootPool);
		TestFalse(TEXT("neither: none"), TrialPool::GetPoolTag(nullptr, FEncounterZoneInfo()).IsValid());
	});

	It("holds every element's gem, no Quartz and no stones (Q-566)", [this]()
	{
		const TArray<ECrystalType> &Crystals = TrialPool::GetPoolCrystals();
		for (const ECrystalType Gem : {ECrystalType::Garnet, ECrystalType::Sapphire, ECrystalType::Citrine, ECrystalType::Emerald, ECrystalType::Amber,
									   ECrystalType::Opal, ECrystalType::Onyx, ECrystalType::Amethyst, ECrystalType::Iolite})
		{
			TestTrue(FString::Printf(TEXT("has %d"), static_cast<int32>(Gem)), Crystals.Contains(Gem));
		}
		TestEqual(TEXT("nine"), Crystals.Num(), 9);
		TestFalse(TEXT("no Quartz"), Crystals.Contains(ECrystalType::Quartz));
		TestFalse(TEXT("no Damage Stone"), Crystals.Contains(ECrystalType::DamageStone));
	});

	It("leans to the trial's element: more often and at higher tiers", [this]()
	{
		TestEqual(TEXT("Fire trial, Garnet"), TrialPool::GetCrystalWeight(ECrystalType::Garnet, ESpellElement::Fire), TrialPool::OWN_ELEMENT_WEIGHT);
		TestEqual(TEXT("Fire trial, Sapphire"), TrialPool::GetCrystalWeight(ECrystalType::Sapphire, ESpellElement::Fire), TrialPool::OTHER_ELEMENT_WEIGHT);
		TestEqual(TEXT("a Generic trial leans nowhere"), TrialPool::GetCrystalWeight(ECrystalType::Garnet, ESpellElement::Generic), TrialPool::OTHER_ELEMENT_WEIGHT);
		const FTrialTierRange Range = TrialPool::GetZoneTierRange(4);
		TestEqual(TEXT("outside the range: never"), TrialPool::GetCrystalTierWeight(EItemTier::S_Tier, Range, true), 0.0f);

		FRandomStream Roll(TrialPoolSpecValues::SEED);
		TMap<ECrystalType, int32> Counts;
		int64 OwnTierSum = 0;
		int64 OtherTierSum = 0;
		int32 OtherCount = 0;
		bool bInRange = true;
		for (int32 Index = 0; Index < TrialPoolSpecValues::ROLLS; ++Index)
		{
			const FCrystalId Crystal = TrialPool::PickCrystal(ESpellElement::Fire, Range, Roll);
			++Counts.FindOrAdd(Crystal.Type);
			bInRange &= Range.Contains(Crystal.Tier);
			if (Crystal.Type == ECrystalType::Garnet)
			{
				OwnTierSum += static_cast<int32>(Crystal.Tier);
			}
			else
			{
				OtherTierSum += static_cast<int32>(Crystal.Tier);
				++OtherCount;
			}
		}
		TestTrue(TEXT("every tier inside the zone's range"), bInRange);
		const int32 Garnets = Counts.FindRef(ECrystalType::Garnet);
		TestTrue(TEXT("Garnet beats Sapphire"), Garnets > Counts.FindRef(ECrystalType::Sapphire));
		TestTrue(TEXT("Garnet beats Amber"), Garnets > Counts.FindRef(ECrystalType::Amber));
		TestTrue(TEXT("Garnets come higher on average"), Garnets > 0 && OtherCount > 0 &&
					 static_cast<double>(OwnTierSum) / Garnets > static_cast<double>(OtherTierSum) / OtherCount);
	});

	It("reads an evolution as a pool item", [this]()
	{
		UEvolutionItemData *Evolution = NewObject<UEvolutionItemData>();
		Evolution->ItemName = TEXT("Gale Force");
		TestNotNull(TEXT("its tags"), TrialLoot::GetSourceTags(Evolution));
		TestEqual(TEXT("its name"), TrialLoot::GetItemName(Evolution).ToString(), FString(TEXT("Gale Force")));
		TestTrue(TEXT("a chest can give it"), TrialLoot::CanGrant(Evolution));
		TestFalse(TEXT("an apparel piece is not given yet (stage 5)"), TrialLoot::CanGrant(NewObject<UApparelData>()));
	});
}

#endif
